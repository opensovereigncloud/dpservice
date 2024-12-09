#include "common.h"

struct rte_hash *dp_inspect_get_table(const char *name, int numa_socket)
{
	struct rte_hash *htable;
	char full_name[RTE_HASH_NAMESIZE];

	// TODO shouldn't this be taken from dpservice?
	if ((unsigned int)snprintf(full_name, sizeof(full_name), "%s_%d", name, numa_socket) >= RTE_HASH_NAMESIZE) {
		fprintf(stderr, "jhash table name '%s' is too long\n", name);
		return NULL;
	}

	htable = rte_hash_find_existing(full_name);
	if (!htable)
		fprintf(stderr, "Table '%s' not found\n", full_name);

	return htable;
}
