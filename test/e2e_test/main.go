// SPDX-FileCopyrightText: 2022 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bytes"
	"context"
	"sync"
	"time"

	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	flag "github.com/spf13/pflag"
	appsv1 "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/client-go/tools/remotecommand"
	"k8s.io/client-go/util/homedir"
)

const dsName = "overlaytest"

var retries int

type test struct {
	from   string
	fromIP string
	fromAZ string
	to     string
	toIP   string
	toAZ   string
	kind   string
	result bool
	err    error
}

type testObject struct {
	AZ   string
	name string
}

func main() {
	var kubeConfigPath *string
	var verbose, ping, curl bool
	var namespace, port, fromAZ, toAZ string
	var createDS, deleteDS, rerun bool
	var failedTests uint8

	// TODO: add flag for parallel tests -- by default running in parallel without flag
	//		 test only specified AZ-AZ combination -- done, flags from-az and to-az
	//		 add verbose mode -- added as flag
	//		 rerun only failed tests -- added as flag, needs more work
	//		 add docs
	//		 add makefile
	flag.StringVarP(&namespace, "namespace", "n", "dpservice-test", "Namespace of the Daemonset")
	flag.BoolVarP(&verbose, "verbose", "v", false, "Show verbose output")
	// TODO: by default create and only if specified don't delete ds -- done
	flag.BoolVarP(&createDS, "create-ds", "c", true, "If Daemonset should be created before tests")
	flag.BoolVarP(&deleteDS, "delete-ds", "d", true, "If Daemonset should be deleted after tests")
	flag.BoolVarP(&ping, "ping", "p", false, "If ping should be tested")
	flag.BoolVarP(&curl, "curl", "u", false, "If curl should be tested")
	flag.BoolVarP(&rerun, "rerun", "r", false, "If curl should be tested")
	// TODO: remove retries and only keep rerun
	flag.IntVarP(&retries, "retries", "i", 0, "Maximum retries for each test")
	flag.StringVarP(&fromAZ, "from-az", "f", "all", "Tests from specified AZ")
	flag.StringVarP(&toAZ, "to-az", "t", "all", "Tests to specified AZ")

	if home := homedir.HomeDir(); home != "" {
		kubeConfigPath = flag.String("kubeconfig", filepath.Join(home, ".kube", "config"), "location of your kubeconfig file")
	} else {
		kubeConfigPath = flag.String("kubeconfig", "", "location of your kubeconfig file")
	}
	flag.Parse()
	fmt.Printf("Using kubeconfig: %s\n", *kubeConfigPath)

	kubeConfig, err := clientcmd.BuildConfigFromFlags("", *kubeConfigPath)
	if err != nil {
		fmt.Printf("Error getting kubernetes config: %v\n", err.Error())
		os.Exit(1)
	}

	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		fmt.Printf("Error creating clientset: %v\n", err.Error())
		os.Exit(1)
	}

	if createDS {
		// deploy daemonset
		// TODO: add retry if old daemonset is still terminating
		deployDaemonSet(namespace, clientset)
		time.Sleep(2 * time.Second)
		// wait for it to be ready
		for i := 0; i < 3; i++ {
			ds, err := clientset.AppsV1().DaemonSets(namespace).Get(context.Background(), dsName, metav1.GetOptions{})
			if err != nil {
				fmt.Printf("Error getting daemonset: %v\n", err.Error())
				os.Exit(1)
			}
			if ds.Status.DesiredNumberScheduled != ds.Status.NumberReady {
				sleep := time.Duration(i+1) * time.Second * 5
				fmt.Printf("Daemonset \"%s\" is not running on all nodes (%d/%d). Retry in %v seconds...\n", dsName, ds.Status.NumberReady, ds.Status.DesiredNumberScheduled, sleep.Seconds())
				time.Sleep(sleep)
				continue
			} else {
				fmt.Printf("Daemonset \"%s\" is ready.\n", dsName)
				break
			}
		}
	}

	// list pods of daemonset that are in Running phase
	pods, err := listPods(namespace, clientset, fmt.Sprintf("name=%s", dsName))
	if err != nil {
		fmt.Printf("Could not get pods: %s", err.Error())
		os.Exit(1)
	}
	fmt.Println("Pods count:", len(pods.Items))

	tests := make([]test, 0)
	testIps := map[string]testObject{}
	for _, pod := range pods.Items {
		testIps[pod.Status.PodIP] = testObject{AZ: getAZ(pod.Spec.NodeName), name: pod.Name}
		testIps[pod.Status.HostIP] = testObject{AZ: getAZ(pod.Spec.NodeName), name: pod.Spec.NodeName}
		testIps["1.1.1.1"] = testObject{AZ: "Internet", name: "cloudflare"}
	}

	fmt.Printf("Starting tests with max %d retries per test from %s to %s.\n----------\n", retries, fromAZ, toAZ)
	wg := new(sync.WaitGroup)
	for _, pod := range pods.Items {
		if getAZ(pod.Spec.NodeName) == fromAZ || fromAZ == "all" {
			wg.Add(1)
			go func(pod v1.Pod) {
				defer wg.Done()
				fmt.Printf("Pod: %s (%s) on node %s (%s) test started.\n", pod.Name, pod.Status.PodIP, pod.Spec.NodeName, getAZ(pod.Spec.NodeName))

				if ping {
					for ip, dst := range testIps {
						currentTest := test{
							from:   pod.Name,
							fromIP: pod.Status.PodIP,
							fromAZ: getAZ(pod.Spec.NodeName),
							to:     dst.name,
							toAZ:   dst.AZ,
							toIP:   ip,
							kind:   "ping",
						}
						if dst.AZ == toAZ || toAZ == "all" {
							_, _, err := testPing(kubeConfig, &pod, ip)
							if err != nil {
								currentTest.result = false
								currentTest.err = err
							} else {
								currentTest.result = true
							}
							tests = append(tests, currentTest)
						}
					}
				}

				if curl {
					for ip, dst := range testIps {
						currentTest := test{}
						if dst.AZ == toAZ || toAZ == "all" {
							if strings.Contains(dst.name, "shoot") {
								currentTest = test{
									from:   pod.Name,
									fromIP: pod.Status.PodIP,
									fromAZ: getAZ(pod.Spec.NodeName),
									to:     dst.name,
									toAZ:   dst.AZ,
									toIP:   ip,
									kind:   "curl",
								}
								port = "16909"
							} else if dst.AZ == "Internet" {
								currentTest = test{
									from:   pod.Name,
									fromIP: pod.Status.PodIP,
									fromAZ: getAZ(pod.Spec.NodeName),
									to:     dst.name,
									toIP:   ip,
									toAZ:   "Internet",
									kind:   "curl",
								}
								port = ""
							} else {
								continue
							}
							stdout, _, err := testCurl(kubeConfig, &pod, ip, port)
							if err == nil && getHttpCode(stdout) > 199 && getHttpCode(stdout) < 400 {
								//msg += fmt.Sprintf("failed: %s\n", err.Error())
								currentTest.result = true
							} else {
								//msg += fmt.Sprintf("OK! HTTP code: %d\n", getHttpCode(stdout))
								currentTest.result = false
								currentTest.err = err
							}
							tests = append(tests, currentTest)
						}
					}
				}
				fmt.Printf("Pod: %s (%s) on node %s (%s) test ended.\n", pod.Name, pod.Status.PodIP, pod.Spec.NodeName, getAZ(pod.Spec.NodeName))
			}(pod)
		}
	}
	wg.Wait()

	// analyse test results
	for _, test := range tests {
		if !test.result {
			failedTests++
			if verbose {
				fmt.Println(test.kind, "test from:", test.from, "to:", test.to, "failed:", test.err.Error())
			} else {
				fmt.Println(test.kind, "test from:", test.from, "to:", test.to, "failed.")
			}
		} else if verbose {
			fmt.Println(test.kind, "test from:", test.from, "to:", test.to, "succeded")
		}
	}

	if failedTests > 0 {
		fmt.Printf("----------\n%d out of %d tests failed!\n", failedTests, len(tests))

		// rerun failed tests
		if rerun {
			for _, test := range tests {
				if !test.result {
					fmt.Printf("Testing %s from %s to %s again: ", test.kind, test.from, test.to)
					pod := v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name:      test.from,
							Namespace: namespace,
						},
					}
					if test.kind == "ping" {
						_, _, err := testPing(kubeConfig, &pod, test.toIP)
						if err != nil {
							fmt.Println("test failed again")
						} else {
							fmt.Println("test successful now")
							test.result = true
						}
					} else if test.kind == "curl" {
						if test.toAZ == "Internet" {
							_, _, err := testCurl(kubeConfig, &pod, test.toIP, "")
							if err != nil {
								fmt.Println("test failed again")
							} else {
								fmt.Println("test successful now")
								test.result = true
							}
						} else {
							_, _, err := testCurl(kubeConfig, &pod, test.toIP, "16909")
							if err != nil {
								fmt.Println("test failed again")
							} else {
								fmt.Println("test successful now")
								test.result = true
							}
						}
					}
				}
			}
		}
	} else {
		fmt.Printf("----------\nAll %d tests were successful.\n", len(tests))
	}

	if deleteDS {
		deleteDaemonSet(namespace, dsName, clientset)
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
func testCurl(kubeConfig *rest.Config, pod *v1.Pod, ip string, port string) (string, string, error) {
	var command string
	if port == "" {
		command = fmt.Sprintf("curl -sIG %s", ip)
	} else {
		command = fmt.Sprintf("curl -sIG %s:%s", ip, port)
	}
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

// deploy daemonset in specified namespace
func deployDaemonSet(namespace string, client kubernetes.Interface) {
	// skip if DS already exists
	_, err := client.AppsV1().DaemonSets(namespace).Get(context.Background(), dsName, metav1.GetOptions{})
	if err == nil {
		return
	}

	// create namespace if doesn't exist
	_, err = client.CoreV1().Namespaces().Get(context.Background(), namespace, metav1.GetOptions{})
	if err != nil {
		if err.Error() == fmt.Sprintf("namespaces \"%s\" not found", namespace) {
			fmt.Printf("Namespace %s not found, creating it.\n", namespace)
			ns := v1.Namespace{
				ObjectMeta: metav1.ObjectMeta{
					Name: namespace,
				},
			}
			_, err := client.CoreV1().Namespaces().Create(context.Background(), &ns, metav1.CreateOptions{})
			if err != nil {
				fmt.Printf("Failed to create namespace %s: %s\n", namespace, err)
				os.Exit(1)
			}
			fmt.Printf("Namespace %s created.\n", namespace)
		}
	}

	ds := appsv1.DaemonSet{
		ObjectMeta: metav1.ObjectMeta{
			Name: dsName,
		},
		Spec: appsv1.DaemonSetSpec{
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					"name": dsName,
				},
			},
			Template: v1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						"name": dsName,
					},
				},
				Spec: v1.PodSpec{
					Tolerations: []v1.Toleration{
						{
							Operator: "Exists",
						},
					},
					Containers: []v1.Container{
						{
							Image:                  "rancherlabs/swiss-army-knife",
							ImagePullPolicy:        "Always",
							Name:                   dsName,
							Command:                []string{"sh", "-c", "tail -f /dev/null"},
							TerminationMessagePath: "/dev/termination-log",
						},
					},
				},
			},
		},
	}
	fmt.Printf("Creating daemonset \"%s\"\n", ds.Name)
	_, err = client.AppsV1().DaemonSets(namespace).Create(context.Background(), &ds, metav1.CreateOptions{})
	if err != nil {
		fmt.Printf("Failed to create daemonset \"%s\": %s\n", ds.Name, err.Error())
		os.Exit(1)
	}
	fmt.Printf("Successfully created daemonset \"%s\"\n", ds.Name)
}

// delete Daemonset in specified namespace
func deleteDaemonSet(namespace string, daemonset string, client kubernetes.Interface) {
	_, err := client.AppsV1().DaemonSets(namespace).Get(context.Background(), daemonset, metav1.GetOptions{})
	if err != nil {
		fmt.Printf("Daemonset \"%s\" not found: %s\n", daemonset, err.Error())
	} else {
		err = client.AppsV1().DaemonSets(namespace).Delete(context.Background(), daemonset, metav1.DeleteOptions{})
		if err != nil {
			fmt.Printf("Could not delete daemonset \"%s\": %s\n", daemonset, err.Error())
			os.Exit(1)
		} else {
			fmt.Printf("Successfully deleted daemonset \"%s\"\n", daemonset)
		}
	}

	_, err = client.CoreV1().Namespaces().Get(context.Background(), namespace, metav1.GetOptions{})
	if err != nil {
		fmt.Printf("Namespace \"%s\" not found: %s\n", namespace, err.Error())
	} else {
		err = client.CoreV1().Namespaces().Delete(context.Background(), namespace, metav1.DeleteOptions{})
		if err != nil {
			fmt.Printf("Could not delete namespace \"%s\": %s\n", namespace, err.Error())
			os.Exit(1)
		} else {
			fmt.Printf("Successfully deleted namespace \"%s\"\n", namespace)
		}
	}
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
