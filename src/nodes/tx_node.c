#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_malloc.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include "node_api.h"
#include "nodes/tx_node_priv.h"
#include "dp_lpm.h"
#include "dp_mbuf_dyn.h"

#define DP_MAX_PATT_ACT	6

static struct ethdev_tx_node_main ethdev_tx_main;
static 	uint8_t	dst_addr[16] = "\xff\xff\xff\xff\xff\xff\xff\xff"
						   "\xff\xff\xff\xff\xff\xff\xff\xff";

static int tx_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	struct tx_node_ctx *ctx = (struct tx_node_ctx *)node->ctx;
	uint64_t port_id = DP_MAX_PORTS;
	uint16_t i;

	/* Find our port id */
	for (i = 0; i < DP_MAX_PORTS; i++) {
		if (ethdev_tx_main.nodes[i] == node->id) {
			port_id = i;
			break;
		}
	}

	RTE_VERIFY(port_id < DP_MAX_PORTS);

	/* Update port and queue */
	ctx->port_id = port_id;
	ctx->queue_id = graph->id;

	return 0;
}

static __rte_always_inline int handle_offload(struct rte_mbuf *m, struct dp_flow *df)
{
	struct underlay_conf *u_conf = get_underlay_conf();
	int res = 0, route_direct = DP_ROUTE_TO_VM;
	struct rte_flow_action_set_mac flow_mac;
	struct rte_flow_item	pattern[DP_MAX_PATT_ACT];
	struct rte_flow_action	action[DP_MAX_PATT_ACT];
	uint8_t					pattern_cnt = 0;
	uint8_t					action_cnt = 0;
	struct rte_flow_attr	attr;
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;
	struct rte_flow_item_geneve gen_spec;
	struct rte_flow_item_geneve gen_mask;
	struct rte_flow_item_ipv4 ipv4_spec;
	struct rte_flow_item_ipv4 ipv4_mask;
	struct rte_flow_item_ipv6 ipv6_spec;
	struct rte_flow_item_ipv6 ipv6_mask;
	struct rte_flow_item_icmp icmp_spec;
	struct rte_flow_item_icmp icmp_mask;
	struct rte_flow_item_tcp tcp_spec;
	struct rte_flow_item_tcp tcp_mask;
	struct rte_flow_item_udp udp_spec;
	struct rte_flow_item_udp udp_mask;
	struct rte_flow *flow;
	int encap_size = sizeof(struct rte_ether_hdr) +
					 sizeof(struct rte_ipv6_hdr) +
					 sizeof(struct rte_udp_hdr) +
					 sizeof(struct rte_flow_item_geneve);
	uint8_t encap_hdr[encap_size];
	uint8_t decap_hdr[sizeof(struct rte_ether_hdr)];

	memset(&pattern, 0, sizeof(pattern));
	memset(&action, 0, sizeof(action));
	memset(&attr, 0, sizeof(attr));

	attr.ingress = 1;
	attr.priority = 0;
	attr.transfer = 1;
	/* First find out the packet direction */
	if (df->nxt_hop == DP_PF_PORT)
		route_direct = DP_ROUTE_TO_PF_ENCAPPED;
	else if ((df->nxt_hop > DP_PF_PORT) && (df->geneve_hdr))
		route_direct = DP_ROUTE_TO_VM_DECAPPED;

	memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
	memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
	if (route_direct == DP_ROUTE_TO_VM_DECAPPED)
		eth_spec.type = htons(RTE_ETHER_TYPE_IPV6);
	else
		eth_spec.type = htons(RTE_ETHER_TYPE_IPV4);
	eth_mask.type = htons(0xffff);
	pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[pattern_cnt].spec = &eth_spec;
	pattern[pattern_cnt].mask = &eth_mask;
	pattern_cnt++;

	if (route_direct == DP_ROUTE_TO_VM_DECAPPED) {
		memset(&ipv6_spec, 0, sizeof(struct rte_flow_item_ipv6));
		memset(&ipv6_mask, 0, sizeof(struct rte_flow_item_ipv6));
		ipv6_spec.hdr.proto =  DP_IP_PROTO_UDP;
		rte_memcpy(ipv6_spec.hdr.dst_addr, u_conf->src_ip6, sizeof(ipv6_spec.hdr.dst_addr));
		ipv6_mask.hdr.proto = 0xff;
		rte_memcpy(ipv6_mask.hdr.dst_addr, dst_addr, sizeof(ipv6_spec.hdr.dst_addr));
		pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_IPV6;
		pattern[pattern_cnt].spec = &ipv6_spec;
		pattern[pattern_cnt].mask = &ipv6_mask;
		pattern_cnt++;

		memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
		memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
		udp_spec.hdr.dst_port = htons(u_conf->dst_port);
		udp_mask.hdr.dst_port = 0xffff;
		pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_UDP;
		pattern[pattern_cnt].spec = &udp_spec;
		pattern[pattern_cnt].mask = &udp_mask;
		pattern_cnt++;

		memset(&gen_spec, 0, sizeof(struct rte_flow_item_geneve));
		memset(&gen_mask, 0, sizeof(struct rte_flow_item_geneve));
		pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_GENEVE;
		gen_spec.protocol = htons(RTE_ETHER_TYPE_IPV4);
		rte_memcpy(gen_spec.vni, &df->dst_vni, sizeof(gen_spec.vni));
		gen_mask.vni[0] = 0xFF;
		gen_mask.vni[1] = 0xFF;
		gen_mask.vni[2] = 0xFF;
		gen_mask.protocol = 0xFFFF;
		pattern[pattern_cnt].spec = &gen_spec;
		pattern[pattern_cnt].mask = &gen_mask;
		pattern_cnt++;

		memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));
		memset(&ipv4_mask, 0, sizeof(struct rte_flow_item_ipv4));
		ipv4_spec.hdr.next_proto_id = df->l4_type;
		ipv4_spec.hdr.dst_addr = df->dst_addr;
		ipv4_mask.hdr.next_proto_id = 0xff;
		ipv4_mask.hdr.dst_addr = 0xffffffff;
		pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_IPV4;
		pattern[pattern_cnt].spec = &ipv4_spec;
		pattern[pattern_cnt].mask = &ipv4_mask;
		pattern_cnt++;
	} else {
		memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));
		memset(&ipv4_mask, 0, sizeof(struct rte_flow_item_ipv4));
		ipv4_spec.hdr.next_proto_id = df->l4_type;
		ipv4_spec.hdr.dst_addr = df->dst_addr;
		ipv4_mask.hdr.next_proto_id = 0xff;
		ipv4_mask.hdr.dst_addr = 0xffffffff;
		pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_IPV4;
		pattern[pattern_cnt].spec = &ipv4_spec;
		pattern[pattern_cnt].mask = &ipv4_mask;
		pattern_cnt++;
	}

	if (route_direct != DP_ROUTE_TO_VM_DECAPPED) {
		if (df->l4_type == DP_IP_PROTO_TCP) {
			memset(&tcp_spec, 0, sizeof(struct rte_flow_item_tcp));
			memset(&tcp_mask, 0, sizeof(struct rte_flow_item_tcp));
			tcp_spec.hdr.dst_port = df->dst_port;
			tcp_spec.hdr.src_port = df->src_port;
			tcp_mask.hdr.dst_port = 0xffff;
			tcp_mask.hdr.src_port = 0xffff;
			pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_TCP;
			pattern[pattern_cnt].spec = &tcp_spec;
			pattern[pattern_cnt].mask = &tcp_mask;
			pattern_cnt++;
		}
		if (df->l4_type == DP_IP_PROTO_UDP) {
			memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
			memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
			udp_spec.hdr.dst_port = df->dst_port;
			udp_spec.hdr.src_port = df->src_port;
			udp_mask.hdr.dst_port = 0xffff;
			udp_mask.hdr.src_port = 0xffff;
			pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_UDP;
			pattern[pattern_cnt].spec = &udp_spec;
			pattern[pattern_cnt].mask = &udp_mask;
			pattern_cnt++;
		}
		if (df->l4_type == DP_IP_PROTO_ICMP) {
			memset(&icmp_spec, 0, sizeof(struct rte_flow_item_icmp));
			memset(&icmp_mask, 0, sizeof(struct rte_flow_item_icmp));
			icmp_spec.hdr.icmp_type = df->icmp_type;
			icmp_mask.hdr.icmp_type = 0xff;
			pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_ICMP;
			pattern[pattern_cnt].spec = &icmp_spec;
			pattern[pattern_cnt].mask = &icmp_mask;
			pattern_cnt++;
		}
	}

	pattern[pattern_cnt].type = RTE_FLOW_ITEM_TYPE_END;
	pattern_cnt++;

	if (route_direct == DP_ROUTE_TO_PF_ENCAPPED) {
		memset(encap_hdr, 0, encap_size);
		memset(decap_hdr, 0, sizeof(struct rte_ether_hdr));
		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_RAW_DECAP;
		struct rte_flow_action_raw_decap raw_decap = {.data = decap_hdr, .size = sizeof(struct rte_ether_hdr)};
		action[action_cnt].conf = &raw_decap;
		action_cnt++;

		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_RAW_ENCAP;
		struct rte_ether_hdr *new_eth_hdr = (struct rte_ether_hdr *) encap_hdr;
		rte_ether_addr_copy(dp_get_neigh_mac(df->nxt_hop), &new_eth_hdr->d_addr);
		rte_ether_addr_copy(dp_get_mac(df->nxt_hop), &new_eth_hdr->s_addr);
		new_eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV6);

		struct rte_ipv6_hdr *new_ipv6_hdr = (struct rte_ipv6_hdr*)(&encap_hdr[sizeof(struct rte_ether_hdr)]);
		new_ipv6_hdr->vtc_flow = htonl(DP_IP6_VTC_FLOW);
		new_ipv6_hdr->hop_limits = DP_IP6_HOP_LIMIT;
		new_ipv6_hdr->proto = DP_IP_PROTO_UDP;
		rte_memcpy(new_ipv6_hdr->src_addr, u_conf->src_ip6, sizeof(new_ipv6_hdr->src_addr));
		rte_memcpy(new_ipv6_hdr->dst_addr, df->dst_addr6, sizeof(new_ipv6_hdr->dst_addr));

		struct rte_udp_hdr *new_udp_hdr = (struct rte_udp_hdr*)(new_ipv6_hdr + 1);
		new_udp_hdr->src_port = htons(u_conf->src_port); /* TODO this should come via df pointer */
		new_udp_hdr->dst_port = htons(u_conf->dst_port);

		struct rte_flow_item_geneve *new_geneve_hdr = (struct rte_flow_item_geneve*)(new_udp_hdr + 1);
		rte_memcpy(new_geneve_hdr->vni, &df->dst_vni, sizeof(new_geneve_hdr->vni));
		new_geneve_hdr->ver_opt_len_o_c_rsvd0 = 0;
		new_geneve_hdr->protocol = htons(RTE_ETHER_TYPE_IPV4);

		struct rte_flow_action_raw_encap raw_encap = {.data = encap_hdr, .size = encap_size};
		action[action_cnt].conf = &raw_encap;
		action_cnt++;
	} else if (route_direct == DP_ROUTE_TO_VM) {
		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_SET_MAC_DST;
		rte_ether_addr_copy(dp_get_neigh_mac(df->nxt_hop), (struct rte_ether_addr *)flow_mac.mac_addr);
		action[action_cnt].conf = &flow_mac;
		action_cnt++;

		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_SET_MAC_SRC;
		rte_ether_addr_copy(dp_get_mac(df->nxt_hop), (struct rte_ether_addr *)flow_mac.mac_addr);
		action[action_cnt].conf = &flow_mac;
		action_cnt++;
	} else if (route_direct == DP_ROUTE_TO_VM_DECAPPED) {
		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_RAW_DECAP;
		struct rte_flow_action_raw_decap raw_decap = {.data = NULL, .size = encap_size};
		action[action_cnt].conf = &raw_decap;
		action_cnt++;

		action[action_cnt].type = RTE_FLOW_ACTION_TYPE_RAW_ENCAP;
		struct rte_ether_hdr *new_eth_hdr = (struct rte_ether_hdr *) decap_hdr;
		rte_ether_addr_copy(dp_get_neigh_mac(df->nxt_hop), &new_eth_hdr->d_addr);
		rte_ether_addr_copy(dp_get_mac(df->nxt_hop), &new_eth_hdr->s_addr);
		new_eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV4);
		struct rte_flow_action_raw_encap raw_encap = {.data = decap_hdr, .size = sizeof(struct rte_ether_hdr)};
		action[action_cnt].conf = &raw_encap;
		action_cnt++;
	}
	action[action_cnt].type = RTE_FLOW_ACTION_TYPE_PORT_ID;
	struct rte_flow_action_port_id nport_id = {.original = 0, .reserved= 0, .id = df->nxt_hop};
	action[action_cnt].conf = &nport_id;
	action_cnt++;
	action[action_cnt].type = RTE_FLOW_ACTION_TYPE_END;
	action_cnt++;

	struct rte_flow_error error;
	res = rte_flow_validate(m->port, &attr, pattern, action, &error);

	if (res) { 
		printf("Flow can't be validated message: %s\n", error.message ? error.message : "(no stated reason)");
		return 0;
	} else {
		printf("Flow validated on port %d targeting port %d \n ", m->port, df->nxt_hop);
		flow = rte_flow_create(m->port, &attr, pattern, action, &error);
		if (!flow)
			printf("Flow can't be created message: %s\n", error.message ? error.message : "(no stated reason)");
	}
	return 1;

	return 0;
}


static __rte_always_inline int handle_flow(struct rte_mbuf *m)
{
	struct dp_flow *df;

	df = get_dp_flow_ptr(m);
	if (df && df->valid)
		handle_offload(m, df);
	rte_free(df);
	return 1;
}

static __rte_always_inline void rewrite_eth_hdr(struct rte_mbuf *m, uint16_t port_id, uint16_t eth_type)
{
	struct rte_ether_hdr *eth_hdr;
	eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(m, sizeof(struct rte_ether_hdr));
	eth_hdr->ether_type = htons(eth_type);
	rte_ether_addr_copy(dp_get_neigh_mac(port_id), &eth_hdr->d_addr);
	rte_ether_addr_copy(dp_get_mac(port_id), &eth_hdr->s_addr);
}

static __rte_always_inline uint16_t tx_node_process(struct rte_graph *graph,
													struct rte_node *node,
													void **objs,
													uint16_t cnt)
{
	struct tx_node_ctx *ctx = (struct tx_node_ctx *)node->ctx;
	struct rte_mbuf *mbuf0, **pkts;
	uint16_t port, queue;
	uint16_t sent_count, i;

	RTE_SET_USED(objs);
	RTE_SET_USED(cnt);

	/* Get Tx port id */
	port = ctx->port_id;
	queue = ctx->queue_id;

	pkts = (struct rte_mbuf **)objs;


	for (i = 0; i < cnt; i++) {
		mbuf0 = pkts[i];
		if (mbuf0->port != port) {
			if (port == DP_PF_PORT) {
				rewrite_eth_hdr(mbuf0, port, RTE_ETHER_TYPE_IPV6);
			} else
				rewrite_eth_hdr(mbuf0, port, RTE_ETHER_TYPE_IPV4);
		}	
		handle_flow(mbuf0);
	}	

	sent_count = rte_eth_tx_burst(port, queue, (struct rte_mbuf **)objs,
				 cnt);

	/* Redirect unsent pkts to drop node */
	if (sent_count != cnt) {
		rte_node_enqueue(graph, node, TX_NEXT_DROP,
				 &objs[sent_count], cnt - sent_count);
	}

	return sent_count;
}

struct ethdev_tx_node_main * tx_node_data_get(void)
{
	return &ethdev_tx_main;
}

static struct rte_node_register tx_node_base = {
	.name = "tx",
	.init = tx_node_init,
	.process = tx_node_process,

	.nb_edges = TX_NEXT_MAX,
	.next_nodes =
		{
			[TX_NEXT_DROP] = "drop",
		},
};

struct rte_node_register *tx_node_get(void)
{
	return &tx_node_base;
}

RTE_NODE_REGISTER(tx_node_base);
