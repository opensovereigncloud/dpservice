// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_nat.h"

#include <stdio.h>

#include "dp_error.h"
#include "dp_nat.h"

#include "common_ip.h"


static int dp_inspect_dnat(const void *key, const void *val)
{
	const struct nat_key *nat_key = key;
	const struct dnat_data *dnat_data = val;

	char ip[INET_ADDRSTRLEN];
	char vip[INET_ADDRSTRLEN];

	DP_IPV4_TO_STR(nat_key->ip, vip);
	DP_IPV4_TO_STR(dnat_data->dnat_ip, ip);
	printf(" vip: %15s, vni: %3u, ip: %15s\n",
		vip,
		nat_key->vni,
		ip
	);
	return DP_OK;
}

static int dp_inspect_snat(const void *key, const void *val)
{
	const struct nat_key *nat_key = key;
	const struct snat_data *snat_data = val;

	char ip[INET_ADDRSTRLEN];
	char vip_ip[INET_ADDRSTRLEN];
	char nat_ip[INET_ADDRSTRLEN];
	char ul_vip[INET6_ADDRSTRLEN];
	char ul_nat[INET6_ADDRSTRLEN];

	DP_IPV4_TO_STR(nat_key->ip, ip);
	DP_IPV4_TO_STR(snat_data->vip_ip, vip_ip);
	DP_IPV4_TO_STR(snat_data->nat_ip, nat_ip);
	DP_IPV6_TO_STR(&snat_data->ul_vip_ip6, ul_vip);
	DP_IPV6_TO_STR(&snat_data->ul_nat_ip6, ul_nat);
	printf(" vni: %3u, ip: %15s, vip_ip: %15s, nat_ip: %15s, min_port: %5u, max_port: %5u, ul_vip: %s, ul_nat: %s\n",
		nat_key->vni,
		ip,
		vip_ip,
		nat_ip,
		snat_data->nat_port_range[0],
		snat_data->nat_port_range[1],
		ul_vip,
		ul_nat
	);
	return DP_OK;
}

static int dp_inspect_portmap(const void *key, const void *val)
{
	const struct netnat_portmap_key *portmap_key = key;
	const struct netnat_portmap_data *portmap_data = val;

	char src_ip[INET6_ADDRSTRLEN];
	char nat_ip[INET_ADDRSTRLEN];

	DP_IPADDR_TO_STR(&portmap_key->src_ip, src_ip);
	DP_IPV4_TO_STR(portmap_data->nat_ip, nat_ip);
	printf(" vni: %3u, src_ip: %15s, src_port: %5u, nat_ip: %15s, nat_port: %5u, flows: %u\n",
		portmap_key->vni,
		src_ip,
		portmap_key->iface_src_port,
		nat_ip,
		portmap_data->nat_port,
		portmap_data->flow_cnt
	);
	return DP_OK;
}

static int dp_inspect_portoverload(const void *key, const void *val)
{
	const struct netnat_portoverload_tbl_key *pkey = key;

	char nat_ip[INET6_ADDRSTRLEN];
	char dst_ip[INET6_ADDRSTRLEN];

	(void)val;  // apparently no data here

	DP_IPV4_TO_STR(pkey->nat_ip, nat_ip);
	DP_IPV4_TO_STR(pkey->dst_ip, dst_ip);
	printf(" nat_ip: %15s, nat_port: %5u, dst_ip: %15s, dst_port: %5u, proto: %4s\n",
		nat_ip, pkey->nat_port,
		dst_ip, pkey->dst_port,
		get_str_ipproto(pkey->l4_type)
	);
	return DP_OK;
}


// TODO global problem: define names in dpservice
const struct dp_inspect_spec dp_inspect_dnat_spec = {
	.table_name = "dnat_table",
	.dump_func = dp_inspect_dnat,
};

const struct dp_inspect_spec dp_inspect_snat_spec = {
	.table_name = "snat_table",
	.dump_func = dp_inspect_snat,
};

const struct dp_inspect_spec dp_inspect_portmap_spec = {
	.table_name = "nat_portmap_table",
	.dump_func = dp_inspect_portmap,
};

const struct dp_inspect_spec dp_inspect_portoverload_spec = {
	.table_name = "nat_portoverload_table",
	.dump_func = dp_inspect_portoverload,
};
