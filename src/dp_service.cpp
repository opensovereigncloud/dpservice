#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "dp_alias.h"
#include "dp_conf.h"
#include "dp_error.h"
#include "dp_flow.h"
#include "dp_lb.h"
#include "dp_log.h"
#include "dp_lpm.h"
#include "dp_multi_path.h"
#include "dp_nat.h"
#include "dp_port.h"
#include "dp_version.h"
#include "dpdk_layer.h"
#include "grpc/dp_grpc_service.h"

static char **dp_argv;
static int dp_argc;
static char *dp_mlx_args[4];

static int dp_args_add_mellanox(int *orig_argc, char ***orig_argv)
{
	int curarg;
	int argend = -1;
	int argc = *orig_argc;
	char **argv = *orig_argv;

	// will be adding two devices (4 args) + terminator
	dp_argv = (char **)calloc(argc + 5, sizeof(*dp_argv));
	if (!dp_argv) {
		DP_EARLY_ERR("Cannot allocate argument array");
		return DP_ERROR;
	}

	// copy EAL args
	for (curarg = 0; curarg < argc; curarg++) {
		if (strcmp(argv[curarg], "--") == 0) {
			argend = curarg;
			break;
		} else {
			dp_argv[curarg] = argv[curarg];
		}
	}
	// add mellanox args (remember that they can be written to, so strdup())
	dp_mlx_args[0] = dp_argv[curarg++] = strdup("-a");
	dp_mlx_args[1] = dp_argv[curarg++] = strdup(dp_conf_get_eal_a_pf0());
	dp_mlx_args[2] = dp_argv[curarg++] = strdup("-a");
	dp_mlx_args[3] = dp_argv[curarg++] = strdup(dp_conf_get_eal_a_pf1());
	if (!dp_mlx_args[0] || !dp_mlx_args[1] || !dp_mlx_args[2] || !dp_mlx_args[3]) {
		DP_EARLY_ERR("Cannot allocate Mellanox arguments");
		return DP_ERROR;
	}

	// add original dp_service args
	if (argend >= 0) {
		for (int j = argend; j < argc; ++j)
			dp_argv[curarg++] = argv[j];
	}
	dp_argv[curarg] = NULL;
	dp_argc = curarg;

	*orig_argc = dp_argc;
	*orig_argv = dp_argv;
	return DP_OK;
}

static void dp_args_free_mellanox()
{
	for (int i = 0; i < 4; ++i)
		free(dp_mlx_args[i]);
	free(dp_argv);
}

static int dp_eal_init(int *argc_ptr, char ***argv_ptr)
{
	if (dp_is_mellanox_opt_set())
		if (DP_FAILED(dp_args_add_mellanox(argc_ptr, argv_ptr)))
			return DP_ERROR;
	return rte_eal_init(*argc_ptr, *argv_ptr);
}

static void dp_eal_cleanup()
{
	rte_eal_cleanup();
	if (dp_is_mellanox_opt_set())
		dp_args_free_mellanox();
}


static int init_interfaces()
{
	int pf0_socket;

	if (DP_FAILED(dp_ports_init())
		|| DP_FAILED(dp_graph_init()))
		return DP_ERROR;

	// VFs are started by GRPC later
	if (DP_FAILED(dp_port_start(dp_port_get_pf0_id()))
		|| DP_FAILED(dp_port_start(dp_port_get_pf1_id())))
		return DP_ERROR;

	pf0_socket = rte_eth_dev_socket_id(dp_port_get_pf0_id());
	if (DP_FAILED(pf0_socket)) {
		DPS_LOG_ERR("Cannot get numa socket for pf0 port %d %s", dp_port_get_pf0_id(), dp_strerror(pf0_socket));
		return DP_ERROR;
	}

	if (DP_FAILED(dp_flow_init(pf0_socket))
		|| DP_FAILED(dp_nat_init(pf0_socket))
		|| DP_FAILED(dp_lb_init(pf0_socket))
		|| DP_FAILED(dp_lpm_init(pf0_socket))
		|| DP_FAILED(dp_alias_init(pf0_socket)))
		return DP_ERROR;

	if (dp_conf_is_wcmp_enabled())
		fill_port_select_table(dp_conf_get_wcmp_frac());

	return DP_OK;
}

static void *dp_handle_grpc(__rte_unused void *arg)
{
	GRPCService *grpc_svc;
	char addr[12];  // '[::]:65535\0'

	dp_log_set_thread_name("grpc");

	grpc_svc = new GRPCService();

	snprintf(addr, sizeof(addr), "[::]:%d", dp_conf_get_grpc_port());

	// we are in a thread, proper teardown would be complicated here, so exit instead
	if (!grpc_svc->run(addr))
		rte_exit(EXIT_FAILURE, "Cannot run without working GRPC server\n");

	delete grpc_svc;
	return NULL;
}

static inline int run_dpdk_service()
{
	int ret, result;

	if (DP_FAILED(init_interfaces()))
		return DP_ERROR;

	ret = rte_ctrl_thread_create(dp_get_ctrl_thread_id(), "grpc-thread", NULL, dp_handle_grpc, NULL);
	if (DP_FAILED(ret)) {
		DPS_LOG_ERR("Cannot create grpc thread %s", dp_strerror(ret));
		return ret;
	}

	result = dp_dpdk_main_loop();

	ret = pthread_join(*dp_get_ctrl_thread_id(), NULL);  // returns errno on failure
	if (ret) {
		DPS_LOG_ERR("Cannot join grpc thread %s", dp_strerror(ret));
		return DP_ERROR;
	}

	return result;
}

static int run_service()
{
	int result;

	if (!dp_conf_is_conntrack_enabled() && dp_conf_is_offload_enabled()) {
		DP_EARLY_ERR("Disabled conntrack requires disabled offloading!");
		return DP_ERROR;
	}

	if (DP_FAILED(dp_log_init()))
		return DP_ERROR;

	dp_log_set_thread_name("control");
	DPS_LOG_INFO("Starting DP Service version %s", DP_SERVICE_VERSION);
	// from this point on, only DPS_LOG should be used

	if (DP_FAILED(dp_dpdk_init()))
		return DP_ERROR;

	result = run_dpdk_service();

	dp_dpdk_exit();

	return result;
}

int main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	int eal_argcount;
	enum dp_conf_runmode runmode;

	// Read the config file first because it can contain EAL arguments
	// (those need to be injected *before* rte_eal_init())
	if (DP_FAILED(dp_conf_parse_file(getenv("DP_CONF"))))
		return EXIT_FAILURE;

	eal_argcount = dp_eal_init(&argc, &argv);
	if (DP_FAILED(eal_argcount)) {
		DP_EARLY_ERR("Failed to initialize EAL");
		return EXIT_FAILURE;
	}

	runmode = dp_conf_parse_args(argc - eal_argcount, argv + eal_argcount);
	switch (runmode) {
	case DP_CONF_RUNMODE_ERROR:
		retval = EXIT_FAILURE;
		break;
	case DP_CONF_RUNMODE_EXIT:
		retval = EXIT_SUCCESS;
		break;
	case DP_CONF_RUNMODE_NORMAL:
		retval = DP_FAILED(run_service()) ? EXIT_FAILURE : EXIT_SUCCESS;
		break;
	}

	dp_eal_cleanup();
	dp_conf_free();

	return retval;
}
