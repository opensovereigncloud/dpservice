#ifndef __INCLUDE_DP_RTE_FLOW_H
#define __INCLUDE_DP_RTE_FLOW_H


#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_flow.h>
#include "dp_mbuf_dyn.h"

#include "dp_lpm.h"
#include "nodes/srv6_common.h"

#include "node_api.h"


#define DP_FLOW_TYPE_LOCAL 1
#define DP_FLOW_TYPE_OUTGOING 2
#define DP_FLOW_TYPE_INCOMING 3


#define DP_FLOW_OVERLAY_TYPE_GENEVE 1
#define DP_FLOW_OVERLAY_TYPE_IPIP 2
#define DP_FLOW_OVERLAY_TYPE_SRV6 3


uint16_t extract_inner_ethernet_header(struct rte_mbuf* pkt); 
uint16_t extract_outter_ethernet_header(struct rte_mbuf* pkt);
int extract_inner_l3_header(struct rte_mbuf* pkt,void* hdr,uint16_t offset); //offset, ipv4/ipv6 header
int extract_inner_l4_header(struct rte_mbuf* pkt,void* hdr,uint16_t offset); //offset,  tcp/udp/icmp header
// int extract_inner_l3_l4_header(struct rte_mbuf* pkt,uint16_t offset); //call the above two functions
int extract_outer_ipv6_header(struct rte_mbuf* pkt,void* hdr,uint16_t offset);
int extract_outer_srv6_header(struct rte_mbuf* pkt,void* hdr,uint16_t offset);
// int extract_outer_ipv6seg_header(struct rte_mbuf* pkt, struct segment_routing_hdr *seg6_hdr);

// functions to craft actions/patterns are added later

int craft_srv6_pattern_combined_hdr(struct rte_flow_item *pattern, uint16_t port_id, struct rte_ipv6_hdr *ipv6_hdr, struct segment_routing_hdr *srv6_hdr);

void create_rte_flow_rule_attr (struct rte_flow_attr *attr, uint32_t group, uint32_t priority,uint32_t ingress, uint32_t egress, uint32_t transfer);

int insert_ethernet_match_pattern(struct rte_flow_item *pattern,int pattern_cnt, 
                                struct rte_flow_item_eth *eth_spec,
	                            struct rte_flow_item_eth *eth_mask,
                                struct rte_ether_addr *src, size_t nr_src_mask_len,
                                struct rte_ether_addr *dst, size_t nr_dst_mask_len,
                                rte_be16_t type);

int insert_ipv6_match_pattern(struct rte_flow_item *pattern,int pattern_cnt, 
                            struct rte_flow_item_ipv6 *ipv6_spec,
	                        struct rte_flow_item_ipv6 *ipv6_mask,
                            uint8_t *src, size_t nr_src_mask_len,
                            uint8_t *dst, size_t nr_dst_mask_len,
                            uint8_t proto);


#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_DP_RTE_FLOW_H */
