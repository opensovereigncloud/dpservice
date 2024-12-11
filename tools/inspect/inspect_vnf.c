// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_vnf.h"

#include <stdio.h>

#include "dp_error.h"
#include "dp_ipaddr.h"

#include "common_vnf.h"


static void print_vnf(const union dp_ipv6 *ul_addr6, const struct dp_vnf *vnf)
{
	char ul[INET6_ADDRSTRLEN];
	char ol[INET6_ADDRSTRLEN];

	DP_IPV6_TO_STR(ul_addr6, ul);
	DP_IPADDR_TO_STR(&vnf->alias_pfx.ol, ol);

	printf(" ul: %s, type: %6s, vni: %3u, port_id: %3u, prefix: %15s, length: %u\n",
		ul,
		get_str_vnftype(vnf->type),
		vnf->vni,
		vnf->port_id,
		ol, vnf->alias_pfx.length
	);
}


static int dp_inspect_vnf(const void *key, const void *val)
{
	print_vnf(key, val);
	return DP_OK;
}

static int dp_inspect_vnf_rev(const void *key, const void *val)
{
	print_vnf(val, key);
	return DP_OK;
}


const struct dp_inspect_spec dp_inspect_vnf_spec = {
	.table_name = "vnf_table",
	.dump_func = dp_inspect_vnf,
};

const struct dp_inspect_spec dp_inspect_vnf_rev_spec = {
	.table_name = "reverse_vnf_table",
	.dump_func = dp_inspect_vnf_rev,
};
