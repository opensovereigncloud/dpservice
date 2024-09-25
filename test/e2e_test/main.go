// SPDX-FileCopyrightText: 2022 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"log"
	"sync"
	"time"

	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/fatih/color"
	flag "github.com/spf13/pflag"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/client-go/tools/remotecommand"
)

const (
	dsName = "overlaytest"
	// port of nginx-container for curl tests
	curlPort = 80
)

var (
	ping, curl, verbose, list bool
	fromAZ, toAZ, namespace   string
	rerun                     []int
)

type testCase struct {
	Id     uint16 `json:"id"`
	From   string `json:"from"`
	FromIP string `json:"from-ip"`
	FromAZ string `json:"from-az"`
	To     string `json:"to"`
	ToIP   string `json:"to-ip"`
	ToAZ   string `json:"to-az"`
	Kind   string `json:"kind"`
	Result bool   `json:"result"`
	Error  string `json:"error"`
}

type testDst struct {
	AZ   string
	name string
}

func main() {
	var kubeConfigPath string

	flag.StringVarP(&namespace, "namespace", "n", "dpservice-test", "Namespace of the Daemonset")
	flag.BoolVarP(&verbose, "verbose", "v", false, "Show verbose output")
	flag.BoolVarP(&ping, "ping", "p", false, "If ping should be tested")
	flag.BoolVarP(&curl, "curl", "c", false, "If curl should be tested")
	flag.BoolVarP(&list, "list", "l", false, "List previously run tests")
	flag.IntSliceVarP(&rerun, "rerun", "r", []int{}, "Test ID to rerun")
	flag.StringVarP(&fromAZ, "from-az", "f", "all", "Tests from specified AZ")
	flag.StringVarP(&toAZ, "to-az", "t", "all", "Tests to specified AZ")
	flag.StringVarP(&kubeConfigPath, "kubeconfig", "k", "", "location of your kubeconfig file")

	flag.Parse()

	// only list saved test results and exit
	if list {
		verbose = true
		testsFromFile := getTestsFromFile()
		analyseResults(testsFromFile)
		os.Exit(0)
	}

	// to prevent running tests on wrong cluster, kubeconfig has to be implicitly specified
	if kubeConfigPath == "" {
		fmt.Println("Kubeconfig path has to be specified via --kubeconfig/-k flag")
		os.Exit(1)
	} else {
		fmt.Printf("Using kubeconfig: %s\n", kubeConfigPath)
	}

	kubeConfig, err := clientcmd.BuildConfigFromFlags("", kubeConfigPath)
	if err != nil {
		fmt.Printf("Error getting kubernetes config: %v\n", err.Error())
		os.Exit(1)
	}

	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		fmt.Printf("Error creating clientset: %v\n", err.Error())
		os.Exit(1)
	}

	// check if DaemonSet is ready
	for i := 0; i < 3; i++ {
		ds, err := clientset.AppsV1().DaemonSets(namespace).Get(context.Background(), dsName, metav1.GetOptions{})
		if err != nil {
			fmt.Printf("Error getting daemonset: %v\n", err.Error())
			os.Exit(1)
		}
		if ds.Status.DesiredNumberScheduled != ds.Status.NumberReady {
			duration := time.Duration(i+1) * time.Second * 5
			fmt.Printf("Daemonset \"%s\" is not running on all nodes (%d/%d). Retry in %v seconds...\n", dsName, ds.Status.NumberReady, ds.Status.DesiredNumberScheduled, duration.Seconds())
			time.Sleep(duration)
			continue
		} else {
			fmt.Printf("Daemonset \"%s\" is ready.\n", dsName)
			break
		}
	}

	// list pods of DaemonSet that are in Running phase
	pods, err := listPods(namespace, clientset, fmt.Sprintf("name=%s", dsName))
	if err != nil {
		fmt.Printf("Could not get pods: %s", err.Error())
		os.Exit(1)
	}

	// prepare test cases
	testsToRun := prerpareTestCases(pods)
	if len(testsToRun) == 0 {
		fmt.Printf("Nothing to do; no test case matched the filter.\n")
		os.Exit(0)
	}

	// run tests
	if len(rerun) > 0 {
		fmt.Printf("Pods running: %d. Re-running selected tests.\n----------\n",
			len(pods.Items))
	} else {
		fmt.Printf("Pods running: %d. Starting tests from %s to %s.\n----------\n",
			len(pods.Items), fromAZ, toAZ)
	}
	testResults := runTests(kubeConfig, testsToRun)

	// analyse test results and write to CLI
	analyseResults(testResults)

	// save test results to file
	saveResultsToFile(testResults)
}

// list Running pods in specified namespace with filtered LabelSelector
func listPods(namespace string, client kubernetes.Interface, filter string) (*v1.PodList, error) {
	pods, err := client.CoreV1().Pods(namespace).List(
		context.Background(),
		metav1.ListOptions{LabelSelector: filter, FieldSelector: "status.phase=Running"})
	if err != nil {
		err = fmt.Errorf("error getting pods: %v", err)
		return nil, err
	}
	return pods, nil
}

// prepare ping command
func testPing(kubeConfig *rest.Config, pod *v1.Pod, ip string) (string, string, error) {
	command := fmt.Sprintf("ping -c5 %s", ip)
	return podExec(kubeConfig, pod, command)
}

// prepare curl command
func testCurl(kubeConfig *rest.Config, pod *v1.Pod, ip string) (string, string, error) {
	command := fmt.Sprintf("curl -sIG %s", ip)
	return podExec(kubeConfig, pod, command)
}

// execute command in pod and return Stdout, Stderr
func podExec(kubeConfig *rest.Config, pod *v1.Pod, command string) (string, string, error) {
	coreClient, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return "", "", err
	}

	stdout := &bytes.Buffer{}
	stderr := &bytes.Buffer{}
	request := coreClient.CoreV1().RESTClient().Post().
		Namespace(pod.Namespace).Resource("pods").Name(pod.Name).
		SubResource("exec").VersionedParams(&v1.PodExecOptions{
		Command:   []string{"/bin/sh", "-c", command},
		Container: dsName,
		Stdin:     true,
		Stdout:    true,
		Stderr:    true,
		TTY:       false,
	}, scheme.ParameterCodec)
	exec, err := remotecommand.NewSPDYExecutor(kubeConfig, "POST", request.URL())
	if err != nil {
		return "", "", fmt.Errorf("%w Failed to create SPDY Executor for %v/%v", err, pod.Namespace, pod.Name)
	}

	err = exec.StreamWithContext(context.Background(), remotecommand.StreamOptions{
		Stdin:  os.Stdin,
		Stdout: stdout,
		Stderr: stderr,
		Tty:    false,
	})
	if err != nil {
		return "", "", fmt.Errorf("%w Failed executing command %s on %v/%v", err, command, pod.Namespace, pod.Name)
	}

	return stdout.String(), stderr.String(), nil
}

// get the AZ name from node name
func getAZ(name string) string {
	if strings.Contains(name, "-z1-") {
		return "AZ1"
	}
	if strings.Contains(name, "-z2-") {
		return "AZ2"
	}
	if strings.Contains(name, "-z3-") {
		return "AZ3"
	}
	return ""
}

// get HTTP code from output of curl
func getHttpCode(input string) int {
	lines := strings.Split(input, "\n")
	statusLine := lines[0]
	parts := strings.Split(statusLine, " ")
	code := parts[1]

	httpCode, err := strconv.Atoi(code)
	if err != nil {
		return 0
	}
	return httpCode
}

// prepare test cases to be run based on running pods and filters
func prerpareTestCases(pods *v1.PodList) []testCase {
	testsToRun := make([]testCase, 0)

	if len(rerun) > 0 {
		// pick tests from file if re-runing them
		testsFromFile := getTestsFromFile()

		rerunMap := make(map[uint16]struct{}, len(rerun))
		for _, id := range rerun {
			rerunMap[uint16(id)] = struct{}{}
		}

		for id, test := range testsFromFile {
			if _, exists := rerunMap[test.Id]; exists {
				testsToRun = append(testsToRun, testsFromFile[id])
			}
		}
	} else {
		// else prepare the tests based on filters
		testIps := map[string]testDst{}
		for _, pod := range pods.Items {
			testIps[pod.Status.PodIP] = testDst{AZ: getAZ(pod.Spec.NodeName), name: pod.Name}
			testIps[pod.Status.HostIP] = testDst{AZ: getAZ(pod.Spec.NodeName), name: pod.Spec.NodeName}
			testIps["1.1.1.1"] = testDst{AZ: "Internet", name: "cloudflare"}
		}
		id := uint16(1)
		for _, pod := range pods.Items {
			if getAZ(pod.Spec.NodeName) == fromAZ || fromAZ == "all" {
				if ping {
					for ip, dst := range testIps {
						if dst.AZ == toAZ || toAZ == "all" {
							currentTest := testCase{
								Id:     id,
								From:   pod.Name,
								FromIP: pod.Status.PodIP,
								FromAZ: getAZ(pod.Spec.NodeName),
								To:     dst.name,
								ToAZ:   dst.AZ,
								ToIP:   ip,
								Kind:   "ping",
							}
							id++
							testsToRun = append(testsToRun, currentTest)
						}
					}
				}
				if curl {
					for ip, dst := range testIps {
						if dst.AZ == toAZ || toAZ == "all" {
							currentTest := testCase{
								Id:     id,
								From:   pod.Name,
								FromIP: pod.Status.PodIP,
								FromAZ: getAZ(pod.Spec.NodeName),
								To:     dst.name,
								ToIP:   ip,
								Kind:   "curl",
							}
							if strings.Contains(dst.AZ, "AZ") {
								currentTest.ToAZ = dst.AZ
								currentTest.ToIP = fmt.Sprintf("%s:%d", currentTest.ToIP, curlPort)
							} else if dst.AZ == "Internet" {
								currentTest.ToAZ = "Internet"
							} else {
								continue
							}
							id++
							testsToRun = append(testsToRun, currentTest)
						}
					}
				}
			}
		}
	}
	return testsToRun
}

// run test cases and update their results
func runTests(kubeConfig *rest.Config, tests []testCase) []testCase {
	wg := new(sync.WaitGroup)
	for i, test := range tests {
		if verbose {
			fmt.Printf("Test ID - %3d started: %s from %s/%s to %s/%s\n",
				test.Id, test.Kind, color.YellowString(test.FromAZ), test.From, color.YellowString(test.ToAZ), test.To)
		}
		wg.Add(1)
		go func(test testCase) {
			defer wg.Done()
			pod := v1.Pod{
				ObjectMeta: metav1.ObjectMeta{
					Name:      test.From,
					Namespace: namespace,
				},
			}
			if test.Kind == "ping" {
				_, _, err := testPing(kubeConfig, &pod, test.ToIP)
				if err != nil {
					tests[i].Result = false
					tests[i].Error = err.Error()
				} else {
					tests[i].Result = true
				}
			}
			if test.Kind == "curl" {
				stdout, _, err := testCurl(kubeConfig, &pod, test.ToIP)
				if err == nil && getHttpCode(stdout) > 199 && getHttpCode(stdout) < 400 {
					tests[i].Result = true
				} else {
					tests[i].Result = false
					tests[i].Error = err.Error()
				}
			}

		}(test)
	}
	wg.Wait()

	return tests
}

func analyseResults(testResults []testCase) {
	var failedTests uint8

	for _, test := range testResults {
		if !test.Result {
			failedTests++
			if verbose {
				fmt.Printf("%s ID - %3d: %s from %s/%s to %s/%s: %s\n",
					color.RedString("FAIL!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
					color.YellowString(test.ToAZ), test.To, test.Error)
			} else {
				fmt.Printf("%s ID - %3d: %s from %s/%s to %s/%s\n",
					color.RedString("FAIL!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
					color.YellowString(test.ToAZ), test.To)
			}
		} else if verbose {
			fmt.Printf("%s ID - %3d: %s from %s/%s to %s/%s\n",
				color.GreenString("PASS!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
				color.YellowString(test.ToAZ), test.To)
		}
	}
	result := fmt.Sprintf("%s | %s | %s\n",
		color.YellowString(fmt.Sprintf("RUN: %d", len(testResults))),
		color.GreenString(fmt.Sprintf("PASSED: %d", len(testResults)-int(failedTests))),
		color.RedString(fmt.Sprintf("FAILED: %d", failedTests)))
	if failedTests > 0 {
		fmt.Printf("----------\n%s -- %s", color.RedString("FAIL!"), result)
	} else {
		fmt.Printf("----------\n%s -- %s", color.GreenString("SUCCESS!"), result)
	}
}

func getTestsFromFile() []testCase {
	file, err := os.Open("test_results.json")
	if err != nil {
		log.Fatalf("Error opening file: %v", err)
	}
	defer file.Close()

	jsonData, err := io.ReadAll(file)
	if err != nil {
		log.Fatalf("Error reading file: %v", err)
	}

	var testsFromFile []testCase
	err = json.Unmarshal(jsonData, &testsFromFile)
	if err != nil {
		log.Fatalf("Error unmarshalling data: %v", err)
	}
	return testsFromFile
}

// save the test results to file and update re-run tests
func saveResultsToFile(testResults []testCase) {
	// if re-running tests, update their results
	if len(rerun) > 0 {
		testsFromFile := getTestsFromFile()

		testResultsMap := make(map[uint16]testCase, len(testResults))
		for _, test := range testResults {
			testResultsMap[uint16(test.Id)] = test
		}

		for id, test := range testsFromFile {
			if test, exists := testResultsMap[test.Id]; exists {
				testsFromFile[id].Result = test.Result
			}
		}

		testResults = testsFromFile
	}

	jsonData, err := json.MarshalIndent(testResults, "", "  ")
	if err != nil {
		log.Fatalf("Error marshalling data: %v", err)
	}

	err = os.WriteFile("test_results.json", jsonData, 0644)
	if err != nil {
		log.Fatalf("Error writing to file: %v", err)
	}
}
