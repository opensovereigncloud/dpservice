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
#include "inspect_iface.h"
#include "inspect_lb.h"
#include "inspect_nat.h"
#include "inspect_vnf.h"
#include "inspect_vni.h"

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


static void list_tables(void)
{
	printf("Supported tables (-t argument):\n");
	// table_choices is from conf.c
	// 1 - skip the "list" option
	for (size_t i = 1; i < RTE_DIM(table_choices); ++i)
		printf("  %s\n", table_choices[i]);
}

static const struct dp_inspect_spec *get_spec(enum dp_conf_table selected_table)
{
	switch (selected_table) {
	case DP_CONF_TABLE_LIST:
		return NULL;
	case DP_CONF_TABLE_CONNTRACK:
		return &dp_inspect_conntrack_spec;
	case DP_CONF_TABLE_DNAT:
		return &dp_inspect_dnat_spec;
	case DP_CONF_TABLE_IFACE:
		return &dp_inspect_iface_spec;
	case DP_CONF_TABLE_LB:
		return &dp_inspect_lb_spec;
	case DP_CONF_TABLE_LB_ID:
		return &dp_inspect_lb_id_spec;
	case DP_CONF_TABLE_PORTMAP:
		return &dp_inspect_portmap_spec;
	case DP_CONF_TABLE_PORTOVERLOAD:
		return &dp_inspect_portoverload_spec;
	case DP_CONF_TABLE_SNAT:
		return &dp_inspect_snat_spec;
	case DP_CONF_TABLE_VNF:
		return &dp_inspect_vnf_spec;
	case DP_CONF_TABLE_VNF_REV:
		return &dp_inspect_vnf_rev_spec;
	case DP_CONF_TABLE_VNI:
		return &dp_inspect_vni_spec;
	}
	return NULL;
}


static void dp_argparse_version(void)
{
	printf("DP Service version %s\n", DP_SERVICE_VERSION);
}

int main(int argc, char **argv)
{
	const struct dp_inspect_spec *spec;
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

	spec = get_spec(dp_conf_get_table());
	if (!spec) {
		list_tables();
		ret = DP_OK;
	} else
		ret = dp_inspect(spec, dp_conf_get_numa_socket(), dp_conf_is_dump() ? DP_INSPECT_DUMP : DP_INSPECT_COUNT);

	eal_cleanup();

	return DP_FAILED(ret) ? EXIT_FAILURE : EXIT_SUCCESS;
}
