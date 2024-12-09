// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect.h"

#include <rte_hash.h>

#include "dp_error.h"
#include "dp_ipaddr.h"

// HACK HACK HACK to make including dp_ipaddr.h work
const union dp_ipv6 *dp_conf_get_underlay_ip(void);
const union dp_ipv6 *dp_conf_get_underlay_ip(void) {
	return &dp_empty_ipv6;
}


static int dp_dump_table(const struct rte_hash *htable, int (*dumpfunc)(const void *key, const void *val))
{
	uint32_t iter = 0;
	void *val = NULL;
	const void *key;
	int ret;

	while ((ret = rte_hash_iterate(htable, (const void **)&key, (void **)&val, &iter)) != -ENOENT) {
		if (DP_FAILED(ret)) {
			fprintf(stderr, "Iterating table failed with %d\n", ret);
			return ret;
		}
		if (DP_FAILED(dumpfunc(key, val))) {
			fprintf(stderr, "Dumping table failed with %d\n", ret);
			return ret;
		}
	}
	return DP_OK;
}

int dp_inspect_table(const char *name, int numa_socket, int (*dumpfunc)(const void *key, const void *val))
{
	struct rte_hash *htable;
	char full_name[RTE_HASH_NAMESIZE];

	// TODO shouldn't this be taken from dpservice?
	if ((unsigned int)snprintf(full_name, sizeof(full_name), "%s_%d", name, numa_socket) >= RTE_HASH_NAMESIZE) {
		fprintf(stderr, "jhash table name '%s' is too long\n", name);
		return DP_ERROR;
	}

	htable = rte_hash_find_existing(full_name);
	if (!htable) {
		fprintf(stderr, "Table '%s' not found\n", full_name);
		return DP_ERROR;
	}

	// Unless in dump-mode, just print the table size
	if (!dumpfunc) {
		printf("Table '%s' has %u entries\n", full_name, rte_hash_count(htable));
		return DP_OK;
	}

	return dp_dump_table(htable, dumpfunc);
}
