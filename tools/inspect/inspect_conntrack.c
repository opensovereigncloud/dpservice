// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_conntrack.h"

#include <stdio.h>
#include "dp_error.h"
#include "dp_flow.h"

#include "inspect.h"

char str_proto[16];

static const char *get_strproto(uint8_t proto)
{
	switch (proto) {
		case IPPROTO_IP: return "ip";
		case IPPROTO_ICMP: return "icmp";
		case IPPROTO_IPIP: return "ipip";
		case IPPROTO_TCP: return "tcp";
		case IPPROTO_UDP: return "udp";
		case IPPROTO_IPV6: return "ipv6";
		default: break;
	}
	snprintf(str_proto, sizeof(str_proto), "%u", proto);
	return str_proto;
}

static const char *get_strtype(enum dp_vnf_type type)
{
	switch (type) {
	case DP_VNF_TYPE_UNDEFINED:
		return "none";
	case DP_VNF_TYPE_LB_ALIAS_PFX:
		return "lb_pfx";
	case DP_VNF_TYPE_ALIAS_PFX:
		return "pfx";
	case DP_VNF_TYPE_LB:
		return "lb";
	case DP_VNF_TYPE_VIP:
		return "vip";
	case DP_VNF_TYPE_NAT:
		return "nat";
	case DP_VNF_TYPE_INTERFACE_IP:
		return "iface";
	}
	return "?";
}

static const char *get_strstate(enum dp_flow_tcp_state state)
{
	switch (state) {
	case DP_FLOW_TCP_STATE_NONE:
		return "none";
	case DP_FLOW_TCP_STATE_NEW_SYN:
		return "syn";
	case DP_FLOW_TCP_STATE_NEW_SYNACK:
		return "synack";
	case DP_FLOW_TCP_STATE_ESTABLISHED:
		return "est";
	case DP_FLOW_TCP_STATE_FINWAIT:
		return "finwai";
	case DP_FLOW_TCP_STATE_RST_FIN:
		return "rstfin";
	}
	return "?";
};

int dp_inspect_conntrack(const void *key, const void *val)
{
	const struct flow_key *flow_key = key;
	const struct flow_value *flow_val = val;

	char src[INET6_ADDRSTRLEN];
	char dst[INET6_ADDRSTRLEN];

	uint64_t hz = rte_get_tsc_hz();
	uint64_t age = (rte_rdtsc() - flow_val->timestamp) / hz;

	DP_IPADDR_TO_STR(&flow_key->l3_src, src);
	DP_IPADDR_TO_STR(&flow_key->l3_dst, dst);

	printf(" type: %6s, proto: %4s, vni: %3u, src: %15s:%5u, dst: %15s:%5u, port_id: %3u, state: %6s, flags: 0x%02x, aged: %d, age: %02lu:%02lu:%02lu, timeout: %u\n",
		get_strtype(flow_key->vnf_type),
		get_strproto(flow_key->proto),
		flow_key->vni,
		src, flow_key->src.port_src,
		dst, flow_key->port_dst,
		flow_val->created_port_id,
		get_strstate(flow_val->l4_state.tcp.state),
		flow_val->flow_flags,
		flow_val->aged,
		age / 3600, (age % 60) / 60, age % 60,
		flow_val->timeout_value
	);

	return DP_OK;
}
