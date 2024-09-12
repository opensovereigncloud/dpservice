// SPDX-FileCopyrightText: 2023 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include <rte_graph.h>
#include <rte_mbuf.h>
#include "nodes/common_node.h"
#include "rte_flow/dp_rte_async_flow_isolation.h"
#include "rte_flow/dp_rte_async_flow.h"
#include "rte_flow/dp_rte_async_flow_template.h"
#include "rte_flow/dp_rte_flow_helpers.h"

DP_NODE_REGISTER(PF1_PROXY, pf1_proxy, DP_NODE_DEFAULT_NEXT_ONLY);

static uint16_t pf1_port_id;
static uint16_t pf1_tap_port_id;

//#define DP_PROXY_TEMPLATE_MAX_RULES 8
//
//struct pf1_proxy_async_data {
//	struct dp_port_async_template *default_templates[DP_PROXY_TEMPLATE_MAX_RULES];
//	struct rte_flow *default_flows[DP_PROXY_TEMPLATE_MAX_RULES];
//};
//
//enum dp_proxy_pattern_type {
//	DP_PROXY_PATTERN_ETH,
//	DP_PROXY_PATTERN_IPV6,
//	DP_PROXY_PATTERN_COUNT,
//};
//
//enum dp_proxy_actions_type {
//	DP_PROXY_ACTIONS_PORT,
//	DP_PROXY_ACTIONS_COUNT,
//};
//
//static const struct rte_flow_pattern_template_attr proxy_pattern_template_attr = {
//	.ingress = 1
//};
//
//static const struct rte_flow_actions_template_attr proxy_actions_template_attr = {
//	.ingress = 1
//};
//
//static const struct rte_flow_template_table_attr proxy_template_table_attr = {
//	.flow_attr = {
//		.group = 0,
//		.ingress = 1,
//	},
//	.nb_flows = DP_PROXY_TEMPLATE_MAX_RULES,
//};
//
//// Function to create async templates for pf1 proxy node (only handling IPv6)
//static int pf1_proxy_create_async_templates(struct pf1_proxy_async_data *async_data, const struct dp_port *port)
//{
//	struct dp_port_async_template *tmpl;
//
//	// Allocate async template
//	tmpl = dp_alloc_async_template(DP_PROXY_PATTERN_COUNT, DP_PROXY_ACTIONS_COUNT);
//	if (!tmpl)
//		return DP_ERROR;
//
//	// Store the template in the async_data structure
//	async_data->default_templates[DP_PORT_ASYNC_TEMPLATE_PF_PROXY] = tmpl;
//
//	// Define the pattern to match Ethernet and IPv6 traffic
//	static const struct rte_flow_item pattern[] = {
//		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
//			.mask = &dp_flow_item_eth_mask,
//		},
//		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
//			.mask = &dp_flow_item_ipv6_mask,
//		},
//		{	.type = RTE_FLOW_ITEM_TYPE_END,
//		},
//	};
//	tmpl->pattern_templates[DP_PROXY_PATTERN_IPV6]
//		= dp_create_async_pattern_template(port->port_id, &proxy_pattern_template_attr, pattern);
//
//	// Define the action to forward packets to another port
//	static const struct rte_flow_action actions[] = {
//		{	.type = RTE_FLOW_ACTION_TYPE_PORT_ID, },
//		{	.type = RTE_FLOW_ACTION_TYPE_END, },
//	};
//	tmpl->actions_templates[DP_PROXY_ACTIONS_PORT]
//		= dp_create_async_actions_template(port->port_id, &proxy_actions_template_attr, actions, actions);
//
//	tmpl->table_attr = &proxy_template_table_attr;
//
//	return dp_init_async_template(port->port_id, tmpl);
//}
//
//// Function to create async rules for the pf1 proxy (only handling IPv6)
//static struct rte_flow *pf1_proxy_create_async_rule(uint16_t port_id, uint16_t target_port_id,
//													struct rte_flow_template_table *template_table)
//{
//	// Define the flow pattern for Ethernet and IPv6 traffic
//	struct rte_flow_item pattern[] = {
//		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
//			.spec = &dp_flow_item_eth_mask,
//		},
//		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
//			.mask = &dp_flow_item_ipv6_mask,
//		},
//		{	.type = RTE_FLOW_ITEM_TYPE_END },
//	};
//
//	// Define the action (forward to target port)
//	struct rte_flow_action_port_id port_action = {
//		.id = target_port_id,
//	};
//	struct rte_flow_action actions[] = {
//		{	.type = RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR,
//			.conf = &port_action,
//		},
//		{	.type = RTE_FLOW_ACTION_TYPE_END },
//	};
//
//	// Create the asynchronous rule
//	return dp_create_async_rule(port_id, template_table,
//								pattern, DP_PROXY_PATTERN_IPV6,
//								actions, DP_PROXY_ACTIONS_PORT);
//}
//
//// Function to create the necessary async rules for pf1_proxy (only handling IPv6)
//static int pf1_proxy_create_async_rules(struct pf1_proxy_async_data *async_data, const struct dp_port *port)
//{
//	struct rte_flow *flow;
//	uint16_t rule_count = 0;
//
//	// Fetch the async templates
//	struct dp_port_async_template **templates = async_data->default_templates;
//
//	// Create rule to forward from pf1_port_id to pf1_tap_port_id
//	flow = pf1_proxy_create_async_rule(port->port_id, pf1_tap_port_id,
//									   templates[DP_PORT_ASYNC_TEMPLATE_PF_PROXY]->template_table);
//	if (!flow) {
//		DPS_LOG_ERR("Failed to create PF1 -> TAP async rule", DP_LOG_PORTID(port->port_id));
//		return DP_ERROR;
//	} else {
//		async_data->default_flows[DP_PORT_ASYNC_FLOW_PF1_TO_TAP] = flow;
//		rule_count++;
//	}
//
//	// Create rule to forward from pf1_tap_port_id to pf1_port_id
//	flow = pf1_proxy_create_async_rule(pf1_tap_port_id, port->port_id,
//									   templates[DP_PORT_ASYNC_TEMPLATE_PF_PROXY]->template_table);
//	if (!flow) {
//		DPS_LOG_ERR("Failed to create TAP -> PF1 async rule", DP_LOG_PORTID(port->port_id));
//		// Handle partial success and proceed
//	} else {
//		async_data->default_flows[DP_PORT_ASYNC_FLOW_TAP_TO_PF1] = flow;
//		rule_count++;
//	}
//
//	// Commit async rules in a blocking manner
//	if (dp_blocking_commit_async_rules(port->port_id, rule_count)) {
//		DPS_LOG_ERR("Failed to commit PF1 Proxy async rules", DP_LOG_PORTID(port->port_id));
//		return DP_ERROR;
//	}
//
//	return DP_OK;
//}
//
static int pf1_proxy_node_init(__rte_unused const struct rte_graph *graph, __rte_unused struct rte_node *node)
{
//	struct pf1_proxy_async_data *async_data = rte_zmalloc("pf1_proxy_async_data", sizeof(*async_data), 0);
//	if (!async_data) {
//		DPS_LOG_ERR("Failed to allocate async data for PF1 proxy");
//		return DP_ERROR;
//	}

	pf1_port_id = dp_get_pf1()->port_id;
	pf1_tap_port_id = dp_get_pf_proxy_tap_port()->port_id;

//	// Create async templates for the proxy
//	if (pf1_proxy_create_async_templates(async_data, dp_get_pf1()) != DP_OK) {
//		DPS_LOG_ERR("Failed to create async templates for PF1 proxy", DP_LOG_PORTID(pf1_port_id));
//		return DP_ERROR;
//	}
//
//	// Create async rules for PF1 proxy
//	if (pf1_proxy_create_async_rules(async_data, dp_get_pf1()) != DP_OK) {
//		DPS_LOG_ERR("Failed to create async rules for PF1 proxy", DP_LOG_PORTID(pf1_port_id));
//		return DP_ERROR;
//	}

	return DP_OK;
}

static __rte_always_inline int pf1_proxy_packet(struct rte_node *node,
												struct rte_mbuf *pkt)
{
	uint16_t port_id;
	uint16_t sent_count;

	if (pkt->port == pf1_tap_port_id) {
		port_id = pf1_port_id;
	} else if (pkt->port == pf1_port_id) {
		port_id = pf1_tap_port_id;
	} else {
		DPNODE_LOG_WARNING(node, "Unexpected packet in PF1 Proxy node", DP_LOG_PORTID(pkt->port));
		return DP_ERROR;
	}

	sent_count = rte_eth_tx_burst(port_id, 0, &pkt, 1);
	if (sent_count != 1) {
		DPNODE_LOG_WARNING(node, "Unable to send packet through PF1 Proxy node", DP_LOG_PORTID(pkt->port));
		return DP_ERROR;
	}

	dp_graphtrace_tx_burst(node, (void **)&pkt, 1, port_id);
	return DP_OK;
}

static uint16_t pf1_proxy_node_process(__rte_unused struct rte_graph *graph,
									   __rte_unused struct rte_node *node,
									   __rte_unused void **objs,
									   uint16_t nb_objs)
{
	dp_graphtrace_node_burst(node, objs, nb_objs);

	// since this node is emitting packets, dp_forward_* wrapper functions cannot be used
	// this code should closely resemble the one inside those functions

//	for (uint16_t i = 0; i < nb_objs; ++i) {
//		if (DP_FAILED(pf1_proxy_packet(node, objs[i]))) {
//			dp_graphtrace_next_burst(node, &objs[i], 1, PF1_PROXY_NEXT_DROP);
//			rte_node_enqueue(graph, node, PF1_PROXY_NEXT_DROP, &objs[i], 1);
//		}
//	}

	return nb_objs;
}
