// SPDX-FileCopyrightText: 2023 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_eal.h>

#include "dp_error.h"
#include "dp_version.h"

#include "inspect.h"
#include "inspect_conntrack.h"
#include "inspect_lb.h"

// generated definitions for getopt(),
// generated storage variables and
// generated getters for such variables
#include "opts.h"
#include "opts.c"

// EAL needs writable arguments (both the string and the array!)
// therefore convert them from literals and remember them for freeing later
static const char *eal_arg_strings[] = {
	"dpservice-inspect",			// this binary (not used, can actually be any string)
	"--proc-type=secondary",		// connect to the primary process (dpservice-bin) instead
	"--no-pci",						// do not try to use any hardware
	"--log-level=6",				// hide DPDK's informational messages (level 7)
};

static char *eal_args_mem[RTE_DIM(eal_arg_strings)];
static char *eal_args[RTE_DIM(eal_args_mem)];

static int eal_init(void)
{
	for (size_t i = 0; i < RTE_DIM(eal_arg_strings); ++i) {
		eal_args[i] = eal_args_mem[i] = strdup(eal_arg_strings[i]);
		if (!eal_args[i]) {
			fprintf(stderr, "Cannot allocate EAL arguments\n");
			for (size_t j = 0; j < RTE_DIM(eal_args_mem); ++j)
				free(eal_args_mem[j]);
			return DP_ERROR;
		}
	}
	return rte_eal_init(RTE_DIM(eal_args), eal_args);
}

static void eal_cleanup(void)
{
	rte_eal_cleanup();
	for (size_t i = 0; i < RTE_DIM(eal_args_mem); ++i)
		free(eal_args_mem[i]);
}


static void dp_argparse_version(void)
{
	printf("DP Service version %s\n", DP_SERVICE_VERSION);
}

int main(int argc, char **argv)
{
	int ret;

	switch (dp_conf_parse_args(argc, argv)) {
	case DP_CONF_RUNMODE_ERROR:
		return EXIT_FAILURE;
	case DP_CONF_RUNMODE_EXIT:
		return EXIT_SUCCESS;
	case DP_CONF_RUNMODE_NORMAL:
		break;
	}

	ret = eal_init();
	if (DP_FAILED(ret)) {
		fprintf(stderr, "Cannot init EAL %s\n", dp_strerror_verbose(ret));
		return EXIT_FAILURE;
	}

	switch (dp_conf_get_table()) {
	case DP_CONF_TABLE_LB:
		ret = dp_inspect_table(LB_TABLE_NAME, dp_conf_get_numa_socket(), dp_conf_is_dump() ? dp_inspect_lb : NULL);
		break;
	case DP_CONF_TABLE_CONNTRACK:
		ret = dp_inspect_table(CONNTRACK_TABLE_NAME, dp_conf_get_numa_socket(), dp_conf_is_dump() ? dp_inspect_conntrack : NULL);
		break;
	}

	eal_cleanup();

	return DP_FAILED(ret) ? EXIT_FAILURE : EXIT_SUCCESS;
}
