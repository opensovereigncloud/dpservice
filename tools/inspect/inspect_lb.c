#include "inspect_lb.h"

#include <stdint.h>
#include <stdio.h>
#include <rte_byteorder.h>

#include "dp_error.h"
#include "dp_ipaddr.h"
#include "dp_lb.h"

#include "common.h"

// TODO HACK HACK HACK
const union dp_ipv6 *dp_conf_get_underlay_ip(void) {
	return NULL;
}

int dp_inspect_lb_ipv4(int numa_socket)
{
	struct rte_hash *htable;

	htable = dp_inspect_get_table("loadbalancer_table", numa_socket);
	if (!htable)
		return DP_ERROR;

	uint32_t iter = 0;
	struct lb_value *lb_val = NULL;
	const struct lb_key *lb_key;
	char ip[INET6_ADDRSTRLEN];
	int ret;

	while ((ret = rte_hash_iterate(htable, (const void **)&lb_key, (void **)&lb_val, &iter)) != -ENOENT) {
		if (DP_FAILED(ret)) {
			fprintf(stderr, "Iterating table failed with %d\n", ret);
			return ret;
		}
		int dp_ipaddr_to_str(const struct dp_ip_address *addr, char *dest, int dest_len);
		DP_IPADDR_TO_STR(&lb_key->ip, ip);
		printf(" ip: %15s  vni: %03u  lb_id: '%s'\n", ip, lb_key->vni, lb_val->lb_id);
	}

	return DP_OK;
}
