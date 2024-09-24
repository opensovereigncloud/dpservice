# Interconnectivity test

This test can be used to check networking related issues in Kubernetes cluster.

It creates testing environment via Helm chart that deploys DaemonSet and then runs connectivity tests between it's pods.

Pods have 2 containers: `rancherlabs/swiss-army-knife` that is used to execute tests and `nginx` used as an endpoint for curl tests.

## Prerequisities

To run this interconnectivity test, you need to have running Kubernetes cluster and it's kubeconfig file.

You also need to have [Helm](https://helm.sh/) installed.

## Preparing test environment

Testing environment is created via Helm chart (in folder [testing-env](./testing-env/)).
This can be done either running Helm command:

```bash
helm install <name> <helm-chart/path> --kubeconfig=<kubeconfig/path>

e.g.:
helm install testing-env ./testing-env/ --kubeconfig=<kubeconfig/path>
```

or using Makefile with command:

```bash
make create-env kubeconfig="<kubeconfig/path>"
```

## Testing

### Basic tests

Makefile provides commands for basic tests without any filters.
Connectivity is currently tested via ping and curl. You can select which one or both to test.

```bash
make test kubeconfig="<kubeconfig/path>"
make test-ping kubeconfig="<kubeconfig/path>"
make test-curl kubeconfig="<kubeconfig/path>"
```

### Specific tests

To run specific tests you can build binary via `make build` and run tests with filters.

Flag `-k, --kubeconfig <string>` needs to be allways specified to prevent deploying on wrong (possibly production) cluster.

If testing environment is running in different namespace, this can be specified by flag `-n, --namespace <string>`.

Filters can be set via flags when running binary.
These filters are currently available:

|  |  |
|----------|----------|
| `-p, --ping`   | If ping should be tested   |
| `-c, --curl` | If curl should be tested |
| `-f, --from-az <string>` | Tests from specified AZ (default "all") |
| `-t, --to-az <string>` | Tests to specified AZ (default "all") |

By default only failed tests are shown.
To get more verbose output, you can add `-v, --verbose` flag to see also successfull tests.

Few examples of filters:

Test ping and curl from pods running on nodes in all AZs to pods running on nodes only in AZ1:

```bash
./e2e_test --kubeconfig=<kubeconfig/path> -pc -to-az=AZ1
```

Test only curl from pods running on nodes in AZ3:

```bash
./e2e_test --kubeconfig=<kubeconfig/path> -c -v -f=AZ3
```

Test summary is shown in console in format `result -- total tests | passed | failed` e.g.: `SUCCESS! -- RUN: 22 | PASSED: 22 | FAILED: 0`

### Re-running tests

After running tests for the first time, test results are saved to json file whith each test having its ID.
This allows to re-run only specific tests via `-r, --rerun <ints>` flag.
No filters are needed to be specified when re-running the tests only list of IDs.

```bash
./e2e_test --kubeconfig=<kubeconfig/path> -r=5,7,12
```

## Deleting test environment

To clean up testing environment after testing run either Helm command:

```bash
helm delete <name> --kubeconfig=<kubeconfig/path>

e.g.:
helm delete testing-env --kubeconfig=<kubeconfig/path>
```

or use Makefile with command:

```bash
make delete-env kubeconfig="<kubeconfig/path>"
```
