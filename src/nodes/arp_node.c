#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include "node_api.h"
#include "nodes/arp_node_priv.h"
#include "dp_mbuf_dyn.h"
#include "dp_lpm.h"

struct arp_node_main arp_node;

static int arp_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	struct arp_node_ctx *ctx = (struct arp_node_ctx *)node->ctx;

	ctx->next = ARP_NEXT_DROP;


	RTE_SET_USED(graph);

	return 0;
}

static __rte_always_inline int handle_arp(struct rte_mbuf *m)
{
	struct rte_ether_hdr *incoming_eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	struct rte_arp_hdr *incoming_arp_hdr = (struct rte_arp_hdr*) (incoming_eth_hdr + 1);
	uint32_t  requested_ip = ntohl(incoming_arp_hdr->arp_data.arp_tip);
	struct rte_ether_addr  tmp_addr; 
	uint32_t temp_ip;

	if (requested_ip == dp_get_gw_ip4()) {
		if(incoming_arp_hdr->arp_data.arp_sip != incoming_arp_hdr->arp_data.arp_tip) {
			rte_ether_addr_copy(&incoming_arp_hdr->arp_data.arp_sha, &incoming_eth_hdr->d_addr);

			rte_memcpy(incoming_eth_hdr->s_addr.addr_bytes, dp_get_mac(m->port), 6);


			incoming_arp_hdr->arp_opcode = htons(2);
			rte_memcpy(tmp_addr.addr_bytes, incoming_arp_hdr->arp_data.arp_sha.addr_bytes, 
				   RTE_ETHER_ADDR_LEN);
			rte_memcpy(incoming_arp_hdr->arp_data.arp_sha.addr_bytes, dp_get_mac(m->port), RTE_ETHER_ADDR_LEN);
			temp_ip = incoming_arp_hdr->arp_data.arp_sip;
			incoming_arp_hdr->arp_data.arp_sip = incoming_arp_hdr->arp_data.arp_tip;
			incoming_arp_hdr->arp_data.arp_tip = temp_ip;
			rte_ether_addr_copy(&tmp_addr, &incoming_arp_hdr->arp_data.arp_tha);
		} else {
			printf("sats:in handle_arp\n");
			rte_memcpy(incoming_eth_hdr->s_addr.addr_bytes, dp_get_mac(m->port), RTE_ETHER_ADDR_LEN);
			memset(incoming_eth_hdr->d_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);
			rte_memcpy(incoming_arp_hdr->arp_data.arp_sha.addr_bytes, dp_get_mac(m->port), RTE_ETHER_ADDR_LEN);
			memset(incoming_arp_hdr->arp_data.arp_tha.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);

		}
		return 1;
	}
	return 0;
} 

static __rte_always_inline uint16_t arp_node_process(struct rte_graph *graph,
													 struct rte_node *node,
													 void **objs,
													 uint16_t cnt)
{
	struct rte_mbuf *mbuf0, **pkts;
	int i;

	pkts = (struct rte_mbuf **)objs;


	for (i = 0; i < cnt; i++) {
		mbuf0 = pkts[i];
		if (handle_arp(mbuf0))
			rte_node_enqueue_x1(graph, node, arp_node.next_index[mbuf0->port], mbuf0);
		else
			rte_node_enqueue_x1(graph, node, ARP_NEXT_DROP, mbuf0);
	}	

    return cnt;
}

int arp_set_next(uint16_t port_id, uint16_t next_index)
{

	arp_node.next_index[port_id] = next_index;
	return 0;
}

static struct rte_node_register arp_node_base = {
	.name = "arp",
	.init = arp_node_init,
	.process = arp_node_process,

	.nb_edges = ARP_NEXT_MAX,
	.next_nodes =
		{
			[ARP_NEXT_DROP] = "drop",
		},
};

struct rte_node_register *arp_node_get(void)
{
	return &arp_node_base;
}

RTE_NODE_REGISTER(arp_node_base);
