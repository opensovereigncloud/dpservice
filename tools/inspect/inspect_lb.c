// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_lb.h"

#include <stdio.h>

#include "dp_error.h"
#include "dp_ipaddr.h"
#include "dp_lb.h"

#include "inspect.h"


int dp_inspect_lb(const void *key, const void *val)
{
	const struct lb_key *lb_key = key;
	const struct lb_value *lb_val = val;

	char ip[INET6_ADDRSTRLEN];

	DP_IPADDR_TO_STR(&lb_key->ip, ip);
	printf(" ip: %15s, vni: %3u, lb_id: '%.*s'\n", ip, lb_key->vni, DP_LB_ID_MAX_LEN, lb_val->lb_id);
	return DP_OK;
}


int dp_inspect_lb_id(const void *key, const void *val)
{
	const char *lb_id = key;
	const struct lb_key *lb_key = val;

	char ip[INET6_ADDRSTRLEN];

	DP_IPADDR_TO_STR(&lb_key->ip, ip);
	printf(" ip: %15s, vni: %3u, lb_id: '%.*s'\n", ip, lb_key->vni, DP_LB_ID_MAX_LEN, lb_id);
	return DP_OK;
}
