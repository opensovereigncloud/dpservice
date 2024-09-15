// SPDX-FileCopyrightText: 2023 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include <rte_graph.h>
#include <rte_mbuf.h>
#include "nodes/common_node.h"

DP_NODE_REGISTER(PF1, pf1, DP_NODE_DEFAULT_NEXT_ONLY);

static uint16_t pf1_port_id;

static int pf1_node_init(__rte_unused const struct rte_graph *graph, __rte_unused struct rte_node *node)
{
	pf1_port_id = dp_get_pf1()->port_id;
	return DP_OK;
}

static uint16_t pf1_node_process(struct rte_graph *graph,
									   struct rte_node *node,
									   void **objs,
									   uint16_t nb_objs)
{
	dp_graphtrace_node_burst(node, objs, nb_objs);

	// since this node is emitting packets, dp_forward_* wrapper functions cannot be used
	// this code should closely resemble the one inside those functions

	uint16_t sent_count = rte_eth_tx_burst(pf1_port_id, 0, (struct rte_mbuf **)objs, nb_objs);

	// TODO clean this up
	dp_graphtrace_tx_burst(node, objs, sent_count, pf1_port_id);

	if (sent_count != nb_objs) {
		DPNODE_LOG_WARNING(node, "Unable to send packets through PF1 node");  // TODO max/value log
		dp_graphtrace_next_burst(node, objs+sent_count, nb_objs-sent_count, PF1_NEXT_DROP);
		rte_node_enqueue(graph, node, PF1_NEXT_DROP, objs+sent_count, nb_objs-sent_count);
	}

	return nb_objs;
}
