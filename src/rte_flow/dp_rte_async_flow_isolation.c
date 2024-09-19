// SPDX-FileCopyrightText: 2023 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "rte_flow/dp_rte_async_flow_isolation.h"

#include "dp_error.h"
#ifdef ENABLE_VIRTSVC
#include "dp_virtsvc.h"
#endif
#include "rte_flow/dp_rte_async_flow.h"
#include "rte_flow/dp_rte_async_flow_template.h"
#include "rte_flow/dp_rte_flow_helpers.h"

#define DP_ISOLATION_DEFAULT_TABLE_MAX_RULES 8

enum dp_isolation_pattern_type {
	DP_ISOLATION_PATTERN_IPV6_PROTO,
	DP_ISOLATION_PATTERN_COUNT,
};

enum dp_isolation_actions_type {
	DP_ISOLATION_ACTIONS_QUEUE,
	DP_ISOLATION_ACTIONS_COUNT,
};

// TODO #ifdef ENABLE_PF1_PROXY
enum dp_pf1_proxy_pattern_type {
	DP_PF1_PROXY_PATTERN_REPR_PORT,
	DP_PF1_PROXY_PATTERN_COUNT,
};

enum dp_pf1_proxy_actions_type {
	DP_PF1_PROXY_ACTIONS_REPR_PORT,
	DP_PF1_PROXY_ACTIONS_COUNT,
};
// TODO #endif

static const struct rte_flow_pattern_template_attr ingress_pattern_template_attr = {
	.ingress = 1
};

// TODO #ifdef ENABLE_PF1_PROXY
static const struct rte_flow_pattern_template_attr transfer_pattern_template_attr = {
	.transfer = 1
};
// TODO #endif

static const struct rte_flow_actions_template_attr ingress_actions_template_attr = {
	.ingress = 1
};

// TODO #ifdef ENABLE_PF1_PROXY
static const struct rte_flow_actions_template_attr transfer_actions_template_attr = {
	.transfer = 1
};
// TODO #endif

static const struct rte_flow_template_table_attr pf_ingress_template_table_attr = {
	.flow_attr = {
		.group = 0,
		.ingress = 1,
	},
	.nb_flows = DP_ISOLATION_DEFAULT_TABLE_MAX_RULES,
};

static const struct rte_flow_template_table_attr group_ingress_template_table_attr = {
	.flow_attr = {
		.group = 10,
		.ingress = 1,
	},
	.nb_flows = DP_ISOLATION_DEFAULT_TABLE_MAX_RULES,
};

// TODO #ifdef ENABLE_PF1_PROXY
static const struct rte_flow_template_table_attr pf_transfer_template_table_attr = {
	.flow_attr = {
		.group = 0,
		.transfer = 1,
	},
	.nb_flows = DP_ISOLATION_DEFAULT_TABLE_MAX_RULES,
};
// TODO #endif

int dp_create_pf_async_group_templates(struct dp_port *port)
{
	printf("dp_create_pf_async_group_templates %u\n", port->port_id);

	struct dp_port_async_template *tmpl;

	tmpl = dp_alloc_async_template(DP_ISOLATION_PATTERN_COUNT, DP_ISOLATION_ACTIONS_COUNT);
	if (!tmpl)
		return DP_ERROR;

	port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_GROUP_ISOLATION] = tmpl;

	// no need to check returned values here, dp_create_async_template() takes care of everything

	static const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &dp_flow_item_eth_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};
	tmpl->pattern_templates[DP_ISOLATION_PATTERN_IPV6_PROTO]
		= dp_create_async_pattern_template(port->port_id, &ingress_pattern_template_attr, pattern);

	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_QUEUE, },
		{	.type = RTE_FLOW_ACTION_TYPE_END, },
	};
	tmpl->actions_templates[DP_ISOLATION_ACTIONS_QUEUE]
		= dp_create_async_actions_template(port->port_id, &ingress_actions_template_attr, actions, actions);

	tmpl->table_attr = &group_ingress_template_table_attr;

	return dp_init_async_template(port->port_id, tmpl);
}

static struct rte_flow *dp_create_pf_async_group_rule(uint16_t port_id, struct rte_flow_template_table *template_table)
{
	const struct rte_flow_item_eth eth_spec = {
		.hdr.ether_type = htons(RTE_ETHER_TYPE_IPV6),
	};
	const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};

	static const struct rte_flow_action_queue queue_action = {
		.index = 0,
	};
	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &queue_action,
		},
		{	.type = RTE_FLOW_ACTION_TYPE_END },
	};

	return dp_create_async_rule(port_id, template_table,
								pattern, DP_ISOLATION_PATTERN_IPV6_PROTO,
								actions, DP_ISOLATION_ACTIONS_QUEUE);
}

int dp_create_pf_async_isolation_templates(struct dp_port *port)
{
	struct dp_port_async_template *tmpl;

	tmpl = dp_alloc_async_template(DP_ISOLATION_PATTERN_COUNT, DP_ISOLATION_ACTIONS_COUNT);
	if (!tmpl)
		return DP_ERROR;

	port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_PF_ISOLATION] = tmpl;

	// no need to check returned values here, dp_create_async_template() takes care of everything

	static const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &dp_flow_item_eth_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.mask = &dp_flow_item_ipv6_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};
	tmpl->pattern_templates[DP_ISOLATION_PATTERN_IPV6_PROTO]
		= dp_create_async_pattern_template(port->port_id, &ingress_pattern_template_attr, pattern);

	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_JUMP, },
		{	.type = RTE_FLOW_ACTION_TYPE_END, },
	};
	tmpl->actions_templates[DP_ISOLATION_ACTIONS_QUEUE]
		= dp_create_async_actions_template(port->port_id, &ingress_actions_template_attr, actions, actions);

	tmpl->table_attr = &pf_ingress_template_table_attr;

	return dp_init_async_template(port->port_id, tmpl);
}

// TODO #ifdef ENABLE_PF1_PROXY
int dp_create_pf_async_proxy_templates_dst(struct dp_port *port)
{
	struct dp_port_async_template *tmpl;

	tmpl = dp_alloc_async_template(DP_PF1_PROXY_PATTERN_COUNT, DP_PF1_PROXY_ACTIONS_COUNT);
	if (!tmpl)
		return DP_ERROR;

	port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY_DST] = tmpl;

	static const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.mask = &dp_flow_item_ethdev_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &dp_flow_item_eth_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.mask = &dp_flow_item_ipv6_dst_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};
	tmpl->pattern_templates[DP_PF1_PROXY_PATTERN_REPR_PORT]
		= dp_create_async_pattern_template(port->port_id, &transfer_pattern_template_attr, pattern);

	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT, },
		{	.type = RTE_FLOW_ACTION_TYPE_END, },
	};
	tmpl->actions_templates[DP_PF1_PROXY_ACTIONS_REPR_PORT]
		= dp_create_async_actions_template(port->port_id, &transfer_actions_template_attr, actions, actions);

	tmpl->table_attr = &pf_transfer_template_table_attr;

	return dp_init_async_template(port->port_id, tmpl);
}

int dp_create_pf_async_proxy_templates(struct dp_port *port)
{
	struct dp_port_async_template *tmpl;

	tmpl = dp_alloc_async_template(DP_PF1_PROXY_PATTERN_COUNT, DP_PF1_PROXY_ACTIONS_COUNT);
	if (!tmpl)
		return DP_ERROR;

	port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY] = tmpl;

	static const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.mask = &dp_flow_item_ethdev_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &dp_flow_item_eth_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.mask = &dp_flow_item_ipv6_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};
	tmpl->pattern_templates[DP_PF1_PROXY_PATTERN_REPR_PORT]
		= dp_create_async_pattern_template(port->port_id, &transfer_pattern_template_attr, pattern);

	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT, },
		{	.type = RTE_FLOW_ACTION_TYPE_END, },
	};
	tmpl->actions_templates[DP_PF1_PROXY_ACTIONS_REPR_PORT]
		= dp_create_async_actions_template(port->port_id, &transfer_actions_template_attr, actions, actions);

	tmpl->table_attr = &pf_transfer_template_table_attr;

	return dp_init_async_template(port->port_id, tmpl);
}
// TODO #endif

#ifdef ENABLE_VIRTSVC
// TODO rename template here too!
int dp_create_virtsvc_async_isolation_templates(struct dp_port *port, uint8_t proto_id)
{
	struct dp_port_async_template *template;

	template = dp_alloc_async_template(DP_ISOLATION_PATTERN_COUNT, DP_ISOLATION_ACTIONS_COUNT);
	if (!template)
		return DP_ERROR;

	if (proto_id == IPPROTO_TCP)
		port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_VIRTSVC_TCP_ISOLATION] = template;
	else
		port->default_async_rules.default_templates[DP_PORT_ASYNC_TEMPLATE_VIRTSVC_UDP_ISOLATION] = template;

	const struct rte_flow_item tcp_src_pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &dp_flow_item_eth_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.mask = &dp_flow_item_ipv6_src_dst_mask,
		},
		{	.type = proto_id == IPPROTO_TCP ? RTE_FLOW_ITEM_TYPE_TCP : RTE_FLOW_ITEM_TYPE_UDP,
			.mask = proto_id == IPPROTO_TCP ? (const void *)&dp_flow_item_tcp_src_mask : (const void *)&dp_flow_item_udp_src_mask,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};
	template->pattern_templates[DP_ISOLATION_PATTERN_IPV6_PROTO]
		= dp_create_async_pattern_template(port->port_id, &ingress_pattern_template_attr, tcp_src_pattern);

	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_JUMP, },
		{	.type = RTE_FLOW_ACTION_TYPE_END, },
	};
	template->actions_templates[DP_ISOLATION_ACTIONS_QUEUE]
		= dp_create_async_actions_template(port->port_id, &ingress_actions_template_attr, actions, actions);

	template->table_attr = &pf_ingress_template_table_attr;

	return dp_init_async_template(port->port_id, template);
}
#endif


static struct rte_flow *dp_create_pf_async_isolation_rule(uint16_t port_id, uint8_t proto, struct rte_flow_template_table *template_table)
{
	const struct rte_flow_item_eth eth_spec = {
		.hdr.ether_type = htons(RTE_ETHER_TYPE_IPV6),
	};
	const struct rte_flow_item_ipv6 ipv6_spec = {
		.hdr.proto = proto,
	};
	const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &ipv6_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};

	static const struct rte_flow_action_jump jump_action = {
		.group = 10,
	};
	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_action,
		},
		{	.type = RTE_FLOW_ACTION_TYPE_END },
	};

	return dp_create_async_rule(port_id, template_table,
								pattern, DP_ISOLATION_PATTERN_IPV6_PROTO,
								actions, DP_ISOLATION_ACTIONS_QUEUE);
}

static struct rte_flow *dp_create_pf_async_proxy_rule_dst(uint16_t port_id, uint16_t src_port_id, uint16_t dst_port_id, uint8_t proto_id, const union dp_ipv6 *dst_ipv6, struct rte_flow_template_table *template_table)
{
	const struct rte_flow_item_ethdev src_port_pattern = {
		.port_id = src_port_id,
	};
	const struct rte_flow_item_eth eth_spec = {
		.hdr.ether_type = htons(RTE_ETHER_TYPE_IPV6),
	};

	DPS_LOG_INFO("dp_create_pf_async_proxy_rule dst_addr:", DP_LOG_IPV6(*dst_ipv6));

	const struct rte_flow_item_ipv6 ipv6_spec = {
		.hdr.proto = proto_id,
		.hdr.dst_addr = DP_INIT_FROM_IPV6(dst_ipv6),
	};
	const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &src_port_pattern,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &ipv6_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};

	const struct rte_flow_item_ethdev dst_port_action = {
		.port_id = dst_port_id,
	};
	const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
			.conf = &dst_port_action,
		},
		{	.type = RTE_FLOW_ACTION_TYPE_END },
	};

	return dp_create_async_rule(port_id, template_table,
								pattern, DP_PF1_PROXY_PATTERN_REPR_PORT,
								actions, DP_PF1_PROXY_ACTIONS_REPR_PORT);
}

static struct rte_flow *dp_create_pf_async_proxy_rule(uint16_t port_id, uint16_t src_port_id, uint16_t dst_port_id, uint8_t proto_id, struct rte_flow_template_table *template_table)
{
	const struct rte_flow_item_ethdev src_port_pattern = {
		.port_id = src_port_id,
	};
	const struct rte_flow_item_eth eth_spec = {
		.hdr.ether_type = htons(RTE_ETHER_TYPE_IPV6),
	};
	const struct rte_flow_item_ipv6 ipv6_spec = {
		.hdr.proto = proto_id,
	};
	const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &src_port_pattern,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &ipv6_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};

	const struct rte_flow_item_ethdev dst_port_action = {
		.port_id = dst_port_id,
	};
	const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
			.conf = &dst_port_action,
		},
		{	.type = RTE_FLOW_ACTION_TYPE_END },
	};

	return dp_create_async_rule(port_id, template_table,
								pattern, DP_PF1_PROXY_PATTERN_REPR_PORT,
								actions, DP_PF1_PROXY_ACTIONS_REPR_PORT);
}
#ifdef ENABLE_VIRTSVC
struct rte_flow *dp_create_virtsvc_async_isolation_rule(uint16_t port_id, uint8_t proto_id,
														const union dp_ipv6 *svc_ipv6, rte_be16_t svc_port,
														struct rte_flow_template_table *template_table,
														const union dp_ipv6 *ul_addr)
{
	const struct rte_flow_item_eth eth_spec = {
		.hdr.ether_type = htons(RTE_ETHER_TYPE_IPV6),
	};

	const struct rte_flow_item_ipv6 ipv6_spec = {
		.hdr.proto = proto_id,
		.hdr.src_addr = DP_INIT_FROM_IPV6(svc_ipv6),
		.hdr.dst_addr = DP_INIT_FROM_IPV6(ul_addr),
	};
	const struct rte_flow_item_tcp tcp_spec = {
		.hdr.src_port = svc_port,
	};
	const struct rte_flow_item_udp udp_spec = {
		.hdr.src_port = svc_port,
	};
	const struct rte_flow_item pattern[] = {
		{	.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &ipv6_spec,
		},
		{	.type = proto_id == IPPROTO_TCP ? RTE_FLOW_ITEM_TYPE_TCP : RTE_FLOW_ITEM_TYPE_UDP,
			.spec = proto_id == IPPROTO_TCP ? (const void *)&tcp_spec : (const void *)&udp_spec,
		},
		{	.type = RTE_FLOW_ITEM_TYPE_END },
	};

	static const struct rte_flow_action_jump jump_action = {
		.group = 10,
	};
	static const struct rte_flow_action actions[] = {
		{	.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_action,
		},
		{	.type = RTE_FLOW_ACTION_TYPE_END },
	};

	return dp_create_async_rule(port_id, template_table,
								pattern, DP_ISOLATION_PATTERN_IPV6_PROTO,
								actions, DP_ISOLATION_ACTIONS_QUEUE);
}
#endif

// TODO think about renaming now that there is the PF proxy...
int dp_create_pf_async_isolation_rules(struct dp_port *port)
{
	struct rte_flow *flow;
	uint16_t rule_count = 0;
	size_t rules_required = 3;
	struct dp_port_async_template **templates = port->default_async_rules.default_templates;

	flow = dp_create_pf_async_group_rule(port->port_id,
											 templates[DP_PORT_ASYNC_TEMPLATE_GROUP_ISOLATION]->template_table);
	if (!flow) {
		DPS_LOG_ERR("Failed to install PF async GROUP isolation rule", DP_LOG_PORT(port));
		return DP_ERROR;
	} else {
		port->default_async_rules.default_flows[DP_PORT_ASYNC_FLOW_ISOLATE_GROUP] = flow;
		rule_count++;
	}

	flow = dp_create_pf_async_isolation_rule(port->port_id, IPPROTO_IPIP,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF_ISOLATION]->template_table);
	if (!flow) {
		DPS_LOG_ERR("Failed to install PF async IPIP isolation rule", DP_LOG_PORT(port));
		return DP_ERROR;
	} else {
		port->default_async_rules.default_flows[DP_PORT_ASYNC_FLOW_ISOLATE_IPIP] = flow;
		rule_count++;
	}

	flow = dp_create_pf_async_isolation_rule(port->port_id, IPPROTO_IPV6,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF_ISOLATION]->template_table);
	if (!flow) {
		DPS_LOG_ERR("Failed to install PF async IPV6 isolation rule", DP_LOG_PORT(port));
		// cannot return, need to commit all previous rules
	} else {
		port->default_async_rules.default_flows[DP_PORT_ASYNC_FLOW_ISOLATE_IPV6] = flow;
		rule_count++;
	}

// TODO #ifdef ENABLE_PF1_PROXY
	if (port == dp_get_pf0()) {
		rules_required += 4;
		// TODO maybe another function like virstvc uses (that returns the number above)

		uint16_t pf1_port_id = dp_get_pf1()->port_id;
		uint16_t proxy_port_id = dp_get_pf1_proxy_port()->port_id;

		DPS_LOG_INFO("Selecting a port for PF1 proxy", DP_LOG_PORTID(proxy_port_id));

		// TODO there is now too much code replication, think on some wrapper/better API
		printf("FLOW ICMP6 %u -> %u\n", pf1_port_id, proxy_port_id);
		flow = dp_create_pf_async_proxy_rule(port->port_id, pf1_port_id, proxy_port_id, IPPROTO_ICMPV6,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY]->template_table);
		if (!flow) {
			DPS_LOG_ERR("Failed to install PF async pf1 to proxy rule", DP_LOG_PORT(port));
			return DP_ERROR;
		} else {
			//port->default_async_rules.default_flows[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY] = flow;
			rule_count++;
		}

		printf("FLOW ICMP6 %u -> %u\n", proxy_port_id, pf1_port_id);
		flow = dp_create_pf_async_proxy_rule(port->port_id, proxy_port_id, pf1_port_id,IPPROTO_ICMPV6,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY]->template_table);
		if (!flow) {
			DPS_LOG_ERR("Failed to install PF async pf1 from proxy rule", DP_LOG_PORT(port));
			return DP_ERROR;
		} else {
			//port->default_async_rules.default_flows[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY] = flow;
			rule_count++;
		}

		printf("FLOW TCP %u -> %u\n", proxy_port_id, pf1_port_id);
		flow = dp_create_pf_async_proxy_rule(port->port_id, proxy_port_id, pf1_port_id, IPPROTO_TCP,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY]->template_table);
		if (!flow) {
			DPS_LOG_ERR("Failed to install PF async pf1 to proxy rule", DP_LOG_PORT(port));
			return DP_ERROR;
		} else {
			//port->default_async_rules.default_flows[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY] = flow;
			rule_count++;
		}

		printf("FLOW UDP %u -> %u\n", proxy_port_id, pf1_port_id);
		flow = dp_create_pf_async_proxy_rule(port->port_id, proxy_port_id, pf1_port_id, IPPROTO_UDP,
											 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY]->template_table);
		if (!flow) {
			DPS_LOG_ERR("Failed to install PF async pf1 to proxy rule", DP_LOG_PORT(port));
			return DP_ERROR;
		} else {
			//port->default_async_rules.default_flows[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY_TEST] = flow;
			rule_count++;
		}

		static const char *pf1_proxy_ipv6_str;
		pf1_proxy_ipv6_str = dp_conf_get_pf1_proxy_ipv6();

		static union dp_ipv6 pf1_proxy_ipv6 = {0};
		if (DP_FAILED(dp_str_to_ipv6(pf1_proxy_ipv6_str, &pf1_proxy_ipv6))) {
			DP_EARLY_ERR("Invalid IPv6 address format: '%s'", pf1_proxy_ipv6_str);
			return DP_ERROR;
		}

		// Get the underlay IP
		static const union dp_ipv6 *ul_ip;
		ul_ip = dp_conf_get_underlay_ip();

		// Create an array of pointers to the IPv6 addresses
		const union dp_ipv6 *ipv6_array[] = { &pf1_proxy_ipv6, ul_ip };

		// Calculate the length of the array
		size_t array_len = sizeof(ipv6_array) / sizeof(ipv6_array[0]);

		// Loop over the array and print the IPv6 addresses
		for (size_t i = 0; i < array_len; i++) {
			rules_required += 2;
			DPS_LOG_INFO("Rule for IPv6 address", DP_LOG_IPV6(*ipv6_array[i]));
			static const union dp_ipv6 *ipv6;
			ipv6 = ipv6_array[i];

			printf("FLOW TCP %u -> %u\n", pf1_port_id, proxy_port_id);
			flow = dp_create_pf_async_proxy_rule_dst(port->port_id, pf1_port_id, proxy_port_id, IPPROTO_TCP, ipv6,
												 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY_DST]->template_table);
			if (!flow) {
				DPS_LOG_ERR("Failed to install PF async pf1 to proxy rule", DP_LOG_PORT(port));
				return DP_ERROR;
			} else {
				//port->default_async_rules.default_flows[DP_PORT_ASYNC_FLOW_PF1_TO_PROXY] = flow;
				rule_count++;
			}

			printf("FLOW UDP %u -> %u\n", pf1_port_id, proxy_port_id);
			flow = dp_create_pf_async_proxy_rule_dst(port->port_id, pf1_port_id, proxy_port_id, IPPROTO_UDP, ipv6,
												 templates[DP_PORT_ASYNC_TEMPLATE_PF1_PROXY_DST]->template_table);
			if (!flow) {
				DPS_LOG_ERR("Failed to install PF async pf1 to proxy rule", DP_LOG_PORT(port));
				return DP_ERROR;
			} else {
				//port->default_async_rules.default_flows[DP_PORT_ASYNC_FLOW_PF1_TO_PROXY] = flow;
				rule_count++;
			}

		}
	}
// TODO #endif

#ifdef ENABLE_VIRTSVC
	rule_count += dp_create_virtsvc_async_isolation_rules(port->port_id,
											templates[DP_PORT_ASYNC_TEMPLATE_VIRTSVC_TCP_ISOLATION]->template_table,
											templates[DP_PORT_ASYNC_TEMPLATE_VIRTSVC_UDP_ISOLATION]->template_table);
	// cannot end on error, need to commit partial success
#endif

	if (dp_blocking_commit_async_rules(port->port_id, rule_count)) {
		DPS_LOG_ERR("Failed to commit PF async isolation rules", DP_LOG_PORT(port));
		return DP_ERROR;
	}


#ifdef ENABLE_VIRTSVC
	rules_required += dp_virtsvc_get_count();
#endif
	if (rule_count != rules_required) {
		DPS_LOG_ERR("Not all PF async isolation rules were installed", DP_LOG_VALUE(rule_count), DP_LOG_MAX(rules_required), DP_LOG_PORT(port));
		return DP_ERROR;
	}

	return DP_OK;
}
