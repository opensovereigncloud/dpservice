#include <rte_arp.h>
#include "dp_periodic_msg.h"
#include "dp_mbuf_dyn.h"
#include "dp_lpm.h"
#include "nodes/arp_node.h"
#include "nodes/ipv6_nd_node.h"

static uint8_t dp_mc_ipv6[16] = {0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x01};
static uint8_t dp_mc_mac[6] = {0x33,0x33,0x00,0x00,0x00,0x01};


void send_to_all_vfs(struct rte_mbuf *pkt, enum dp_periodic_type per_type, uint16_t eth_type)
{
	struct dp_flow *df_ptr;
	struct rte_ether_hdr *eth_hdr;
	struct dp_dpdk_layer *dp_layer = get_dpdk_layer();
	struct dp_ports *ports = get_dp_ports();

	// send pkt to all allocated VFs
	DP_FOREACH_PORT(ports, port) {
		if (port->port_type == DP_PORT_VF && port->allocated) {
				struct rte_mbuf *clone_buf = rte_pktmbuf_copy(pkt,dp_layer->rte_mempool, 0, UINT32_MAX);
				clone_buf->port = port->port_id;
				eth_hdr = rte_pktmbuf_mtod(clone_buf, struct rte_ether_hdr *);
				rte_ether_addr_copy(dp_get_mac(clone_buf->port), &eth_hdr->src_addr);
				if(eth_type == RTE_ETHER_TYPE_ARP) {
					struct rte_arp_hdr *arp_hdr;
					arp_hdr = (struct rte_arp_hdr*) (eth_hdr + 1);
					rte_memcpy(arp_hdr->arp_data.arp_sha.addr_bytes, 
							   dp_get_mac(clone_buf->port), RTE_ETHER_ADDR_LEN);
					if (dp_arp_cycle_needed(clone_buf->port))
						arp_hdr->arp_data.arp_tip = htonl(dp_get_dhcp_range_ip4(clone_buf->port));
				}
				df_ptr = alloc_dp_flow_ptr(clone_buf);
				if (!df_ptr) {
					printf("Can not get private pointer in periodic mbuf\n");
					return;
				}
				memset(df_ptr, 0, sizeof(struct dp_flow));
				df_ptr->periodic_type = per_type;
				df_ptr->l3_type = eth_type;
				
				rte_ring_sp_enqueue(dp_layer->periodic_msg_queue, clone_buf);
			}
	}
	return;
 }

void trigger_garp()
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_arp_hdr *arp_hdr;
	struct rte_mbuf *pkt;
	struct dp_dpdk_layer *dp_layer = get_dpdk_layer();

	pkt = rte_pktmbuf_alloc(dp_layer->rte_mempool);
	if(pkt == NULL)
		printf("rte_mbuf allocation failed\n");

	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	eth_hdr->ether_type = htons(0x0806);
	memset(eth_hdr->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);

	arp_hdr = (struct rte_arp_hdr*) (eth_hdr + 1);
	arp_hdr->arp_opcode = htons(DP_ARP_REQUEST);
	arp_hdr->arp_hardware = htons(DP_ARP_HW_ETH);
	arp_hdr->arp_protocol = htons(RTE_ETHER_TYPE_IPV4);
	arp_hdr->arp_hlen = 6;
	arp_hdr->arp_plen = 4;
	arp_hdr->arp_data.arp_sip = htonl(dp_get_gw_ip4());
	arp_hdr->arp_data.arp_tip = htonl(dp_get_gw_ip4());
	memset(arp_hdr->arp_data.arp_tha.addr_bytes, 0, RTE_ETHER_ADDR_LEN);

	pkt->data_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr);
	pkt->pkt_len = pkt->data_len;
	pkt->packet_type = RTE_PTYPE_L2_ETHER_ARP;

	send_to_all_vfs(pkt, DP_PER_TYPE_DIRECT_TX, RTE_ETHER_TYPE_ARP);
	rte_pktmbuf_free(pkt);
	return;

}

void trigger_nd_unsol_adv()
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct nd_msg *ns_msg;
	struct icmp6hdr *icmp6_hdr;
	uint16_t pkt_size;
	struct rte_mbuf *pkt;
	const uint8_t *rt_ip = dp_get_gw_ip6();
	struct dp_dpdk_layer *dp_layer = get_dpdk_layer();

	pkt = rte_pktmbuf_alloc(dp_layer->rte_mempool);
	if(pkt == NULL)
		printf("rte_mbuf allocation failed\n");

	pkt->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_ICMP;
	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	ipv6_hdr = (struct rte_ipv6_hdr*)(eth_hdr+1);
	ns_msg = (struct nd_msg*) (ipv6_hdr + 1);

	rte_memcpy(eth_hdr->dst_addr.addr_bytes, dp_mc_mac, sizeof(eth_hdr->dst_addr.addr_bytes));
	eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV6);

	ipv6_hdr->proto = 0x3a; //ICMP6
	ipv6_hdr->vtc_flow = htonl(0x60000000);
	ipv6_hdr->hop_limits = 255;
	rte_memcpy(ipv6_hdr->src_addr, rt_ip,sizeof(ipv6_hdr->src_addr));
	rte_memcpy(ipv6_hdr->dst_addr, dp_mc_ipv6,sizeof(ipv6_hdr->dst_addr));
	ipv6_hdr->payload_len = htons(sizeof(struct icmp6hdr) + sizeof(struct in6_addr));		
	
	icmp6_hdr = &(ns_msg->icmph);
	memset(icmp6_hdr, 0, sizeof(struct icmp6hdr));
	icmp6_hdr->icmp6_type = 136;
	icmp6_hdr->icmp6_solicited	= 0;
	icmp6_hdr->icmp6_override	= 1;
	icmp6_hdr->icmp6_router = 1;
	icmp6_hdr->icmp6_hop_limit = 255;
	

	rte_memcpy(&ns_msg->target, dp_get_gw_ip6(), sizeof(ns_msg->target));
	pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr) + 
			   sizeof(struct icmp6hdr) + sizeof(struct in6_addr);
	pkt->data_len = pkt_size;
	pkt->pkt_len = pkt_size;
	//L4 cksum calculation 
	icmp6_hdr->icmp6_cksum	= 0;
	icmp6_hdr->icmp6_cksum = rte_ipv6_udptcp_cksum(ipv6_hdr, icmp6_hdr);

	send_to_all_vfs(pkt, DP_PER_TYPE_DIRECT_TX,RTE_ETHER_TYPE_IPV6);
	rte_pktmbuf_free(pkt);
	return;
}

void trigger_nd_ra()
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct ra_msg *ra_msg;
	struct icmp6hdr *icmp6_hdr;
	uint16_t pkt_size;
	struct rte_mbuf *pkt_buf;
	struct dp_dpdk_layer *dp_layer = get_dpdk_layer();
	const uint8_t *rt_ip = dp_get_gw_ip6();

	pkt_buf = rte_pktmbuf_alloc(dp_layer->rte_mempool);
	if (!pkt_buf)
		printf("rte_mbuf allocation failed\n");

	pkt_buf->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_ICMP;
	eth_hdr = rte_pktmbuf_mtod(pkt_buf, struct rte_ether_hdr *);
	ipv6_hdr = (struct rte_ipv6_hdr*)(eth_hdr+1);
	ra_msg = (struct ra_msg*) (ipv6_hdr + 1);

	memset(&eth_hdr->src_addr, 0xff, RTE_ETHER_ADDR_LEN);
	rte_memcpy(eth_hdr->dst_addr.addr_bytes, dp_mc_mac, sizeof(eth_hdr->dst_addr.addr_bytes));
	eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV6);

	ipv6_hdr->proto = 0x3a; //ICMP6
	ipv6_hdr->vtc_flow = htonl(0x60000000);
	ipv6_hdr->hop_limits = 255;
	rte_memcpy(ipv6_hdr->src_addr, rt_ip,sizeof(ipv6_hdr->src_addr));
	rte_memcpy(ipv6_hdr->dst_addr, dp_mc_ipv6,sizeof(ipv6_hdr->dst_addr));

	icmp6_hdr = &(ra_msg->icmph);
	memset(icmp6_hdr,0,sizeof(struct icmp6hdr));
	icmp6_hdr->icmp6_type = NDISC_ROUTER_ADVERTISEMENT;
	icmp6_hdr->icmp6_managed	= 1;
	icmp6_hdr->icmp6_other	= 1;
	icmp6_hdr->icmp6_rt_lifetime = 0xffff;
	ra_msg->reachable_time = 0;
	ra_msg->retrans_timer = 0;
	ipv6_hdr->payload_len = htons(sizeof(struct ra_msg));
	icmp6_hdr->icmp6_hop_limit = 255;

	pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr) + sizeof(struct ra_msg);
	pkt_buf->data_len = pkt_size;
	pkt_buf->pkt_len = pkt_size;

	//L4 cksum calculation 
	icmp6_hdr->icmp6_cksum	= 0;
	icmp6_hdr->icmp6_cksum = rte_ipv6_udptcp_cksum(ipv6_hdr,icmp6_hdr);

	// send pkt to all allocated VFs
	send_to_all_vfs(pkt_buf, DP_PER_TYPE_DIRECT_TX, RTE_ETHER_TYPE_IPV6);
	rte_pktmbuf_free(pkt_buf);
}
