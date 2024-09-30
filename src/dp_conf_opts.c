// SPDX-FileCopyrightText: 2023 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

/***********************************************************************/
/*                        DO NOT EDIT THIS FILE                        */
/*                                                                     */
/* This file has been generated by dp_conf_generate.py                 */
/* Please edit dp_conf.json and re-run the script to update this file. */
/***********************************************************************/

#include "dp_argparse.h"

#ifndef ARRAY_SIZE
#	define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#endif

enum {
	OPT_HELP = 'h',
	OPT_VERSION = 'v',
_OPT_SHOPT_MAX = 255,
	OPT_PF0,
	OPT_PF1,
#ifdef ENABLE_PF1_PROXY
	OPT_PF1_PROXY,
#endif
	OPT_IPV6,
	OPT_VF_PATTERN,
	OPT_DHCP_MTU,
	OPT_DHCP_DNS,
	OPT_DHCPV6_DNS,
#ifdef ENABLE_VIRTSVC
	OPT_UDP_VIRTSVC,
#endif
#ifdef ENABLE_VIRTSVC
	OPT_TCP_VIRTSVC,
#endif
	OPT_WCMP,
	OPT_NIC_TYPE,
	OPT_NO_STATS,
	OPT_NO_CONNTRACK,
	OPT_ENABLE_IPV6_OVERLAY,
	OPT_NO_OFFLOAD,
#ifdef ENABLE_PYTEST
	OPT_GRAPHTRACE_LOGLEVEL,
#endif
	OPT_COLOR,
	OPT_LOG_FORMAT,
	OPT_GRPC_PORT,
#ifdef ENABLE_PYTEST
	OPT_FLOW_TIMEOUT,
#endif
	OPT_MULTIPORT_ESWITCH,
};

#define OPTSTRING ":hv" \

static const struct option dp_conf_longopts[] = {
	{ "help", 0, 0, OPT_HELP },
	{ "version", 0, 0, OPT_VERSION },
	{ "pf0", 1, 0, OPT_PF0 },
	{ "pf1", 1, 0, OPT_PF1 },
#ifdef ENABLE_PF1_PROXY
	{ "pf1-proxy", 1, 0, OPT_PF1_PROXY },
#endif
	{ "ipv6", 1, 0, OPT_IPV6 },
	{ "vf-pattern", 1, 0, OPT_VF_PATTERN },
	{ "dhcp-mtu", 1, 0, OPT_DHCP_MTU },
	{ "dhcp-dns", 1, 0, OPT_DHCP_DNS },
	{ "dhcpv6-dns", 1, 0, OPT_DHCPV6_DNS },
#ifdef ENABLE_VIRTSVC
	{ "udp-virtsvc", 1, 0, OPT_UDP_VIRTSVC },
#endif
#ifdef ENABLE_VIRTSVC
	{ "tcp-virtsvc", 1, 0, OPT_TCP_VIRTSVC },
#endif
	{ "wcmp", 1, 0, OPT_WCMP },
	{ "nic-type", 1, 0, OPT_NIC_TYPE },
	{ "no-stats", 0, 0, OPT_NO_STATS },
	{ "no-conntrack", 0, 0, OPT_NO_CONNTRACK },
	{ "enable-ipv6-overlay", 0, 0, OPT_ENABLE_IPV6_OVERLAY },
	{ "no-offload", 0, 0, OPT_NO_OFFLOAD },
#ifdef ENABLE_PYTEST
	{ "graphtrace-loglevel", 1, 0, OPT_GRAPHTRACE_LOGLEVEL },
#endif
	{ "color", 1, 0, OPT_COLOR },
	{ "log-format", 1, 0, OPT_LOG_FORMAT },
	{ "grpc-port", 1, 0, OPT_GRPC_PORT },
#ifdef ENABLE_PYTEST
	{ "flow-timeout", 1, 0, OPT_FLOW_TIMEOUT },
#endif
	{ "multiport-eswitch", 0, 0, OPT_MULTIPORT_ESWITCH },
	{ NULL, 0, 0, 0 }
};

static const char *nic_type_choices[] = {
	"mellanox",
	"tap",
	"bluefield2",
};

static const char *color_choices[] = {
	"never",
	"always",
	"auto",
};

static const char *log_format_choices[] = {
	"text",
	"json",
};

static char pf0_name[IF_NAMESIZE];
static char pf1_name[IF_NAMESIZE];
#ifdef ENABLE_PF1_PROXY
static char pf1_proxy[IF_NAMESIZE];
#endif
static char vf_pattern[IF_NAMESIZE];
static int dhcp_mtu = 1500;
static int wcmp_perc = 100;
static enum dp_conf_nic_type nic_type = DP_CONF_NIC_TYPE_MELLANOX;
static bool stats_enabled = true;
static bool conntrack_enabled = true;
static bool ipv6_overlay_enabled = false;
static bool offload_enabled = true;
#ifdef ENABLE_PYTEST
static int graphtrace_loglevel = 0;
#endif
static enum dp_conf_color color = DP_CONF_COLOR_NEVER;
static enum dp_conf_log_format log_format = DP_CONF_LOG_FORMAT_TEXT;
static int grpc_port = 1337;
#ifdef ENABLE_PYTEST
static int flow_timeout = DP_FLOW_DEFAULT_TIMEOUT;
#endif
static bool multiport_eswitch = false;

const char *dp_conf_get_pf0_name(void)
{
	return pf0_name;
}

const char *dp_conf_get_pf1_name(void)
{
	return pf1_name;
}

#ifdef ENABLE_PF1_PROXY
const char *dp_conf_get_pf1_proxy(void)
{
	return pf1_proxy;
}

#endif
const char *dp_conf_get_vf_pattern(void)
{
	return vf_pattern;
}

int dp_conf_get_dhcp_mtu(void)
{
	return dhcp_mtu;
}

int dp_conf_get_wcmp_perc(void)
{
	return wcmp_perc;
}

enum dp_conf_nic_type dp_conf_get_nic_type(void)
{
	return nic_type;
}

bool dp_conf_is_stats_enabled(void)
{
	return stats_enabled;
}

bool dp_conf_is_conntrack_enabled(void)
{
	return conntrack_enabled;
}

bool dp_conf_is_ipv6_overlay_enabled(void)
{
	return ipv6_overlay_enabled;
}

bool dp_conf_is_offload_enabled(void)
{
	return offload_enabled;
}

#ifdef ENABLE_PYTEST
int dp_conf_get_graphtrace_loglevel(void)
{
	return graphtrace_loglevel;
}

#endif
enum dp_conf_color dp_conf_get_color(void)
{
	return color;
}

enum dp_conf_log_format dp_conf_get_log_format(void)
{
	return log_format;
}

int dp_conf_get_grpc_port(void)
{
	return grpc_port;
}

#ifdef ENABLE_PYTEST
int dp_conf_get_flow_timeout(void)
{
	return flow_timeout;
}

#endif
bool dp_conf_is_multiport_eswitch(void)
{
	return multiport_eswitch;
}



/* These functions need to be implemented by the user of this generated code */
static void dp_argparse_version(void);
static int dp_argparse_opt_ipv6(const char *arg);
static int dp_argparse_opt_dhcp_dns(const char *arg);
static int dp_argparse_opt_dhcpv6_dns(const char *arg);
#ifdef ENABLE_VIRTSVC
static int dp_argparse_opt_udp_virtsvc(const char *arg);
#endif
#ifdef ENABLE_VIRTSVC
static int dp_argparse_opt_tcp_virtsvc(const char *arg);
#endif


static inline void dp_argparse_help(const char *progname, FILE *outfile)
{
	fprintf(outfile, "Usage: %s [options]\n"
		" -h, --help                             display this help and exit\n"
		" -v, --version                          display version and exit\n"
		"     --pf0=IFNAME                       first physical interface (e.g. eth0)\n"
		"     --pf1=IFNAME                       second physical interface (e.g. eth1)\n"
#ifdef ENABLE_PF1_PROXY
		"     --pf1-proxy=IFNAME                 VF representor to use as a proxy for pf1 packets\n"
#endif
		"     --ipv6=ADDR6                       IPv6 underlay address\n"
		"     --vf-pattern=PATTERN               virtual interface name pattern (e.g. 'eth1vf')\n"
		"     --dhcp-mtu=SIZE                    set the mtu field in DHCP responses (68 - 1500)\n"
		"     --dhcp-dns=IPv4                    set the domain name server field in DHCP responses (can be used multiple times)\n"
		"     --dhcpv6-dns=ADDR6                 set the domain name server field in DHCPv6 responses (can be used multiple times)\n"
#ifdef ENABLE_VIRTSVC
		"     --udp-virtsvc=IPv4,port,IPv6,port  map a VM-accessible IPv4 endpoint to an outside IPv6 UDP service\n"
#endif
#ifdef ENABLE_VIRTSVC
		"     --tcp-virtsvc=IPv4,port,IPv6,port  map a VM-accessible IPv4 endpoint to an outside IPv6 TCP service\n"
#endif
		"     --wcmp=PERCENTAGE                  weighted-cost-multipath percentage for pf0 (0 - 100)\n"
		"     --nic-type=NICTYPE                 NIC type to use: 'mellanox' (default), 'tap' or 'bluefield2'\n"
		"     --no-stats                         do not print periodic statistics to stdout\n"
		"     --no-conntrack                     disable connection tracking\n"
		"     --enable-ipv6-overlay              enable IPv6 overlay addresses\n"
		"     --no-offload                       disable traffic offloading\n"
#ifdef ENABLE_PYTEST
		"     --graphtrace-loglevel=LEVEL        verbosity level of packet traversing the graph framework\n"
#endif
		"     --color=MODE                       output colorization mode: 'never' (default), 'always' or 'auto'\n"
		"     --log-format=FORMAT                set the format of individual log lines (on standard output): 'text' (default) or 'json'\n"
		"     --grpc-port=PORT                   listen for gRPC clients on this port\n"
#ifdef ENABLE_PYTEST
		"     --flow-timeout=SECONDS             inactive flow timeout (except TCP established flows)\n"
#endif
		"     --multiport-eswitch                run on NIC configured in multiport e-switch mode\n"
	, progname);
}

static int dp_conf_parse_arg(int opt, const char *arg)
{
	(void)arg;  // if no option uses an argument, this would be unused
	switch (opt) {
	case OPT_PF0:
		return dp_argparse_string(arg, pf0_name, ARRAY_SIZE(pf0_name));
	case OPT_PF1:
		return dp_argparse_string(arg, pf1_name, ARRAY_SIZE(pf1_name));
#ifdef ENABLE_PF1_PROXY
	case OPT_PF1_PROXY:
		return dp_argparse_string(arg, pf1_proxy, ARRAY_SIZE(pf1_proxy));
#endif
	case OPT_IPV6:
		return dp_argparse_opt_ipv6(arg);
	case OPT_VF_PATTERN:
		return dp_argparse_string(arg, vf_pattern, ARRAY_SIZE(vf_pattern));
	case OPT_DHCP_MTU:
		return dp_argparse_int(arg, &dhcp_mtu, 68, 1500);
	case OPT_DHCP_DNS:
		return dp_argparse_opt_dhcp_dns(arg);
	case OPT_DHCPV6_DNS:
		return dp_argparse_opt_dhcpv6_dns(arg);
#ifdef ENABLE_VIRTSVC
	case OPT_UDP_VIRTSVC:
		return dp_argparse_opt_udp_virtsvc(arg);
#endif
#ifdef ENABLE_VIRTSVC
	case OPT_TCP_VIRTSVC:
		return dp_argparse_opt_tcp_virtsvc(arg);
#endif
	case OPT_WCMP:
		return dp_argparse_int(arg, &wcmp_perc, 0, 100);
	case OPT_NIC_TYPE:
		return dp_argparse_enum(arg, (int *)&nic_type, nic_type_choices, ARRAY_SIZE(nic_type_choices));
	case OPT_NO_STATS:
		return dp_argparse_store_false(&stats_enabled);
	case OPT_NO_CONNTRACK:
		return dp_argparse_store_false(&conntrack_enabled);
	case OPT_ENABLE_IPV6_OVERLAY:
		return dp_argparse_store_true(&ipv6_overlay_enabled);
	case OPT_NO_OFFLOAD:
		return dp_argparse_store_false(&offload_enabled);
#ifdef ENABLE_PYTEST
	case OPT_GRAPHTRACE_LOGLEVEL:
		return dp_argparse_int(arg, &graphtrace_loglevel, 0, DP_GRAPHTRACE_LOGLEVEL_MAX);
#endif
	case OPT_COLOR:
		return dp_argparse_enum(arg, (int *)&color, color_choices, ARRAY_SIZE(color_choices));
	case OPT_LOG_FORMAT:
		return dp_argparse_enum(arg, (int *)&log_format, log_format_choices, ARRAY_SIZE(log_format_choices));
	case OPT_GRPC_PORT:
		return dp_argparse_int(arg, &grpc_port, 1024, 65535);
#ifdef ENABLE_PYTEST
	case OPT_FLOW_TIMEOUT:
		return dp_argparse_int(arg, &flow_timeout, 1, 300);
#endif
	case OPT_MULTIPORT_ESWITCH:
		return dp_argparse_store_true(&multiport_eswitch);
	default:
		fprintf(stderr, "Unimplemented option %d\n", opt);
		return DP_ERROR;
	}
}

enum dp_conf_runmode dp_conf_parse_args(int argc, char **argv)
{
	const char *progname = argv[0];
	int option_index = -1;
	int opt;

	while ((opt = getopt_long(argc, argv, OPTSTRING, dp_conf_longopts, &option_index)) != -1) {
		switch (opt) {
		case OPT_HELP:
			dp_argparse_help(progname, stdout);
			return DP_CONF_RUNMODE_EXIT;
		case OPT_VERSION:
			dp_argparse_version();
			return DP_CONF_RUNMODE_EXIT;
		case ':':
			fprintf(stderr, "Missing argument for '%s'\n", argv[optind-1]);
			return DP_CONF_RUNMODE_ERROR;
		case '?':
			if (optopt > 0)
				fprintf(stderr, "Unknown option '-%c'\n", optopt);
			else
				fprintf(stderr, "Unknown option '%s'\n", argv[optind-1]);
			return DP_CONF_RUNMODE_ERROR;
		default:
			if (DP_FAILED(dp_conf_parse_arg(opt, optarg))) {
				if (option_index >= 0)
					fprintf(stderr, "Invalid argument for '--%s'\n", dp_conf_longopts[option_index].name);
				else
					fprintf(stderr, "Invalid argument for '-%c'\n", opt);
				return DP_CONF_RUNMODE_ERROR;
			}
		}
		option_index = -1;
	}
	return DP_CONF_RUNMODE_NORMAL;
}

