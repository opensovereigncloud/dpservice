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
	port   = 16909
)

var (
	retries                 int
	ping, curl, verbose     bool
	fromAZ, toAZ, namespace string
)

// TODO: add id for rerun possibility
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

type testObject struct {
	AZ   string
	name string
}

func main() {
	var kubeConfigPath string
	var rerun int
	var failedTests uint8

	// TODO:
	//		 rerun specified tests -- currently only 1 test allowed
	//		 add docs
	//		 add makefile -- done
	flag.StringVarP(&namespace, "namespace", "n", "dpservice-test", "Namespace of the Daemonset")
	flag.BoolVarP(&verbose, "verbose", "v", false, "Show verbose output")
	flag.BoolVarP(&ping, "ping", "p", false, "If ping should be tested")
	flag.BoolVarP(&curl, "curl", "c", false, "If curl should be tested")
	// TODO: change rerun to slice
	flag.IntVarP(&rerun, "rerun", "r", 0, "Test ID to rerun")
	// TODO: remove retries and only keep rerun
	flag.IntVarP(&retries, "retries", "i", 0, "Maximum retries for each test")
	flag.StringVarP(&fromAZ, "from-az", "f", "all", "Tests from specified AZ")
	flag.StringVarP(&toAZ, "to-az", "t", "all", "Tests to specified AZ")
	flag.StringVarP(&kubeConfigPath, "kubeconfig", "k", "", "location of your kubeconfig file")

	flag.Parse()
	fmt.Printf("Using kubeconfig: %s\n", kubeConfigPath)

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

	// prepare test cases based on filter
	var tests []testCase
	if rerun != 0 {
		// Read the JSON file
		file, err := os.Open("test_results.json")
		if err != nil {
			log.Fatalf("Error opening file: %v", err)
		}
		defer file.Close()

		// Read the file contents into a byte slice
		jsonData, err := io.ReadAll(file)
		if err != nil {
			log.Fatalf("Error reading file: %v", err)
		}

		var rerunTests []testCase
		// Unmarshal the JSON data into a slice of Test structs
		err = json.Unmarshal(jsonData, &rerunTests)
		if err != nil {
			log.Fatalf("Error unmarshalling data: %v", err)
		}

		// TODO: allow to rerun more tests
		for id, testCase := range rerunTests {
			if testCase.Id == uint16(rerun) {
				tests = append(tests, rerunTests[id])
			}
		}
	} else {
		tests = prerpareTestCases(pods)
	}
	if len(tests) == 0 {
		fmt.Printf("Nothing to do; no test case matched the filter.\n")
		os.Exit(0)
	}

	// run tests
	fmt.Printf("Pods running: %d. Starting tests with max %d retries per test from %s to %s.\n----------\n",
		len(pods.Items), retries, fromAZ, toAZ)
	tests = runTests(kubeConfig, tests)

	// analyse test results
	// TODO: clean up the output
	for _, test := range tests {
		if !test.Result {
			failedTests++
			if verbose {
				fmt.Printf("%s ID - %d: %s from %s/%s to %s/%s: %s\n",
					color.RedString("FAIL!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
					color.YellowString(test.ToAZ), test.To, test.Error)
			} else {
				fmt.Printf("%s ID - %d: %s from %s/%s to %s/%s\n",
					color.RedString("FAIL!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
					color.YellowString(test.ToAZ), test.To)
			}
		} else if verbose {
			fmt.Printf("%s ID - %d: %s from %s/%s to %s/%s\n",
				color.GreenString("PASS!"), test.Id, test.Kind, color.YellowString(test.FromAZ), test.From,
				color.YellowString(test.ToAZ), test.To)
		}
	}
	result := fmt.Sprintf("%s | %s | %s\n",
		color.YellowString(fmt.Sprintf("RUN: %d", len(tests))),
		color.GreenString(fmt.Sprintf("PASSED: %d", len(tests)-int(failedTests))),
		color.RedString(fmt.Sprintf("FAILED: %d", failedTests)))
	if failedTests > 0 {
		fmt.Printf("----------\n%s -- %s", color.RedString("FAIL!"), result)
	} else {
		fmt.Printf("----------\n%s -- %s", color.GreenString("SUCCESS!"), result)
	}

	// write the results to JSON file
	jsonData, err := json.MarshalIndent(tests, "", "  ")
	if err != nil {
		log.Fatalf("Error marshalling data: %v", err)
	}

	err = os.WriteFile("test_results.json", jsonData, 0644)
	if err != nil {
		log.Fatalf("Error writing to file: %v", err)
	}
}

// list pods in specified namespace with filtered LabelSelector
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
		Command: []string{"/bin/sh", "-c", command},
		Stdin:   true,
		Stdout:  true,
		Stderr:  true,
		TTY:     false,
	}, scheme.ParameterCodec)
	exec, err := remotecommand.NewSPDYExecutor(kubeConfig, "POST", request.URL())
	if err != nil {
		return "", "", fmt.Errorf("%w Failed to create SPDY Executor for %v/%v", err, pod.Namespace, pod.Name)
	}

	// retry for defined times
	for i := 0; i < retries+1; i++ {
		err = exec.StreamWithContext(context.Background(), remotecommand.StreamOptions{
			Stdin:  os.Stdin,
			Stdout: stdout,
			Stderr: stderr,
			Tty:    false,
		})
		if err == nil {
			break
		} else if i == 2 {
			return "", "", fmt.Errorf("%w Failed executing command %s on %v/%v", err, command, pod.Namespace, pod.Name)
		}
		retries++
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

func prerpareTestCases(pods *v1.PodList) []testCase {
	tests := make([]testCase, 0)
	testIps := map[string]testObject{}
	for _, pod := range pods.Items {
		testIps[pod.Status.PodIP] = testObject{AZ: getAZ(pod.Spec.NodeName), name: pod.Name}
		testIps[pod.Status.HostIP] = testObject{AZ: getAZ(pod.Spec.NodeName), name: pod.Spec.NodeName}
		testIps["1.1.1.1"] = testObject{AZ: "Internet", name: "cloudflare"}
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
						tests = append(tests, currentTest)
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
						if strings.Contains(dst.name, "shoot") {
							currentTest.ToAZ = dst.AZ
							currentTest.ToIP = fmt.Sprintf("%s:%d", currentTest.ToIP, port)
						} else if dst.AZ == "Internet" {
							currentTest.ToAZ = "Internet"
						} else {
							continue
						}
						id++
						tests = append(tests, currentTest)
					}
				}
			}
		}
	}
	return tests
}

func runTests(kubeConfig *rest.Config, tests []testCase) []testCase {
	wg := new(sync.WaitGroup)
	for i, test := range tests {
		if verbose {
			fmt.Printf("Test started: %s from %s/%s to %s/%s\n",
				test.Kind, color.YellowString(test.FromAZ), test.From, color.YellowString(test.ToAZ), test.To)
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
