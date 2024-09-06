##@ Development

.PHONY: fmt
fmt: ## Run go fmt against code.
	go fmt ./...

.PHONY: lint
lint:
	golangci-lint run ./...

.PHONY: vet
vet:
	go vet ./...

.PHONY: addlicense
addlicense: ## Add license headers to all go files.
	find . -name '*.go' -exec go run github.com/google/addlicense -c 'IronCore authors' {} +

.PHONY: checklicense
checklicense: ## Check that every file has a license header present.
	find . -name '*.go' -exec go run github.com/google/addlicense  -check -c 'IronCore authors' {} +

.PHONY: check
check: addlicense lint test # Generate manifests, code, lint, add licenses, test

##@ Build

.PHONY: build
build: fmt vet ## Build binary.
	go build -o bin/e2e_test main.go

.PHONY: test
test: ##fmt lint ## Run all tests.
	@go run ./... --kubeconfig=$(kubeconfig) -pc

.PHONY: test-ping
test-ping: ##fmt lint ## Run ping tests.
	@go run ./... --kubeconfig=$(kubeconfig) -p

.PHONY: test-curl
test-curl: ##fmt lint ## Run curl tests.
	@go run ./... --kubeconfig=$(kubeconfig) -c

.PHONY: create-env
create-env: ## Create testing environment via Helm chart.
	helm install testing-env ./testing-env/ --kubeconfig=$(kubeconfig)

.PHONY: delete-env
delete-env: ## Create testing environment via Helm chart.
	helm delete testing-env --kubeconfig=$(kubeconfig)
