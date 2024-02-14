#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_hash.h>

#include "dp_error.h"
#include "dp_log.h"
#include "dp_util.h"

// EAL needs writable arguments (both the string and the array!)
// therefore convert them from literals and remember them for freeing later
static const char *eal_arg_strings[] = {
	"dp_lbtable",					// this binary (not used, can actually be any string)
	"--proc-type=secondary",		// connect to the primary process (dp_service) instead
	"--no-pci",						// do not try to use any hardware
	"--log-level=6",				// hide DPDK's informational messages (level 7)
};
static char *eal_args_mem[RTE_DIM(eal_arg_strings)];
static char *eal_args[RTE_DIM(eal_args_mem)];

static int eal_init(void)
{
	for (unsigned i = 0; i < RTE_DIM(eal_arg_strings); ++i) {
		eal_args[i] = eal_args_mem[i] = strdup(eal_arg_strings[i]);
		if (!eal_args[i]) {
			fprintf(stderr, "Cannot allocate EAL arguments\n");
			for (unsigned j = 0; j < RTE_DIM(eal_args_mem); ++j)
				free(eal_args_mem[j]);
			return DP_ERROR;
		}
	}
	return rte_eal_init(RTE_DIM(eal_args), eal_args);
}

static void eal_cleanup(void)
{
	rte_eal_cleanup();
	for (unsigned i = 0; i < RTE_DIM(eal_args_mem); ++i)
		free(eal_args_mem[i]);
}

// TODO .h
struct lb_key {
	uint32_t	ip;
	uint32_t	vni;
};

struct lb_port {
	uint8_t			protocol;
	rte_be16_t		port;
};

#define DP_LB_MAX_IPS_PER_VIP	64
#define DP_VNF_IPV6_ADDR_SIZE	16
struct lb_value {
	uint8_t				lb_id[DP_LB_ID_MAX_LEN];
	struct lb_port		ports[DP_LB_MAX_PORTS];
	uint32_t			back_end_ips[DP_LB_MAX_IPS_PER_VIP][4];
	uint16_t			last_sel_pos;
	uint16_t			back_end_cnt;
	uint8_t				lb_ul_addr[DP_VNF_IPV6_ADDR_SIZE];
};

int main(int argc, char *argv[])
{
	int retcode = EXIT_SUCCESS;
	int ret;
	char table_name[64];
	struct rte_hash *h;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <socket_id>\n", argv[0]);
		return EXIT_FAILURE;
	}

	ret = eal_init();
	if (DP_FAILED(ret)) {
		fprintf(stderr, "Cannot init EAL %s\n", dp_strerror_verbose(ret));
		return EXIT_FAILURE;
	}

	snprintf(table_name, sizeof(table_name), "ipv4_lb_table_%u", atoi(argv[1]));
	h = rte_hash_find_existing(table_name);
	if (!h) {
		fprintf(stderr, "Table '%s' not found\n", table_name);
		retcode = EXIT_FAILURE;
	} else {
		uint32_t iter = 0;
		struct lb_value *lb_val = NULL;
		const struct lb_key *lb_key;
		char ip[32];

		printf("Found '%s':\n", table_name);
		while ((ret = rte_hash_iterate(h, (const void **)&lb_key, (void **)&lb_val, &iter)) != -ENOENT) {
			if (DP_FAILED(ret)) {
				fprintf(stderr, "Iterating table failed with %d\n", ret);
				retcode = EXIT_FAILURE;
				break;
			}
			snprintf(ip, sizeof(ip), DP_IPV4_PRINT_FMT, DP_IPV4_PRINT_BYTES(htonl(lb_key->ip)));
			printf(" ip: %15s  vni: %03u  lb_id: '%s'\n", ip, lb_key->vni, lb_val->lb_id);
		}
	}

	eal_cleanup();

	return retcode;
}
