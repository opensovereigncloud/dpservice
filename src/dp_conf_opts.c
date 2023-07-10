/***********************************************************************/
/*                        DO NOT EDIT THIS FILE                        */
/*                                                                     */
/* This file has been generated by dp_conf_generate.py                 */
/* Please edit dp_conf.json and re-run the script to update this file. */
/***********************************************************************/

enum {
	OPT_HELP = 'h',
	OPT_VERSION = 'v',
_OPT_SHOPT_MAX = 255,
	OPT_PF0,
	OPT_PF1,
	OPT_IPV6,
	OPT_VF_PATTERN,
	OPT_DHCP_MTU,
	OPT_DHCP_DNS,
#ifdef ENABLE_VIRTSVC
	OPT_UDP_VIRTSVC,
#endif
#ifdef ENABLE_VIRTSVC
	OPT_TCP_VIRTSVC,
#endif
	OPT_WCMP_FRACTION,
	OPT_NIC_TYPE,
	OPT_NO_STATS,
	OPT_NO_CONNTRACK,
	OPT_ENABLE_IPV6_OVERLAY,
	OPT_NO_OFFLOAD,
#ifdef ENABLE_GRAPHTRACE
#ifdef ENABLE_PYTEST
	OPT_GRAPHTRACE_LOGLEVEL,
#endif
#endif
	OPT_COLOR,
	OPT_LOG_FORMAT,
	OPT_GRPC_PORT,
#ifdef ENABLE_PYTEST
	OPT_FLOW_TIMEOUT,
#endif
};

#define OPTSTRING \
	"h" \
	"v" \

static const struct option longopts[] = {
	{ "help", 0, 0, OPT_HELP },
	{ "version", 0, 0, OPT_VERSION },
	{ "pf0", 1, 0, OPT_PF0 },
	{ "pf1", 1, 0, OPT_PF1 },
	{ "ipv6", 1, 0, OPT_IPV6 },
	{ "vf-pattern", 1, 0, OPT_VF_PATTERN },
	{ "dhcp-mtu", 1, 0, OPT_DHCP_MTU },
	{ "dhcp-dns", 1, 0, OPT_DHCP_DNS },
#ifdef ENABLE_VIRTSVC
	{ "udp-virtsvc", 1, 0, OPT_UDP_VIRTSVC },
#endif
#ifdef ENABLE_VIRTSVC
	{ "tcp-virtsvc", 1, 0, OPT_TCP_VIRTSVC },
#endif
	{ "wcmp-fraction", 1, 0, OPT_WCMP_FRACTION },
	{ "nic-type", 1, 0, OPT_NIC_TYPE },
	{ "no-stats", 0, 0, OPT_NO_STATS },
	{ "no-conntrack", 0, 0, OPT_NO_CONNTRACK },
	{ "enable-ipv6-overlay", 0, 0, OPT_ENABLE_IPV6_OVERLAY },
	{ "no-offload", 0, 0, OPT_NO_OFFLOAD },
#ifdef ENABLE_GRAPHTRACE
#ifdef ENABLE_PYTEST
	{ "graphtrace-loglevel", 1, 0, OPT_GRAPHTRACE_LOGLEVEL },
#endif
#endif
	{ "color", 1, 0, OPT_COLOR },
	{ "log-format", 1, 0, OPT_LOG_FORMAT },
	{ "grpc-port", 1, 0, OPT_GRPC_PORT },
#ifdef ENABLE_PYTEST
	{ "flow-timeout", 1, 0, OPT_FLOW_TIMEOUT },
#endif
	{ NULL, }
};

static const char *nic_type_choices[] = {
	"hw",
	"tap",
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

static void print_help_args(FILE *outfile)
{
	fprintf(outfile, "%s",
		" -h, --help                             display this help and exit\n"
		" -v, --version                          display version and exit\n"
		"     --pf0=IFNAME                       first physical interface (e.g. eth0)\n"
		"     --pf1=IFNAME                       second physical interface (e.g. eth1)\n"
		"     --ipv6=ADDR6                       IPv6 underlay address\n"
		"     --vf-pattern=PATTERN               virtual interface name pattern (e.g. 'eth1vf')\n"
		"     --dhcp-mtu=SIZE                    set the mtu field in DHCP responses (68 - 1500)\n"
		"     --dhcp-dns=IPv4                    set the domain name server field in DHCP responses (can be used multiple times)\n"
#ifdef ENABLE_VIRTSVC
		"     --udp-virtsvc=IPv4,port,IPv6,port  map a VM-accessible IPv4 endpoint to an outside IPv6 UDP service\n"
#endif
#ifdef ENABLE_VIRTSVC
		"     --tcp-virtsvc=IPv4,port,IPv6,port  map a VM-accessible IPv4 endpoint to an outside IPv6 TCP service\n"
#endif
		"     --wcmp-fraction=FRACTION           weighted-cost-multipath coefficient for pf0 (0.0 - 1.0)\n"
		"     --nic-type=NICTYPE                 NIC type to use: 'hw' (default) or 'tap'\n"
		"     --no-stats                         do not print periodic statistics to stdout\n"
		"     --no-conntrack                     disable connection tracking\n"
		"     --enable-ipv6-overlay              enable IPv6 overlay addresses\n"
		"     --no-offload                       disable traffic offloading\n"
#ifdef ENABLE_GRAPHTRACE
#ifdef ENABLE_PYTEST
		"     --graphtrace-loglevel=LEVEL        verbosity level of packet traversing the graph framework\n"
#endif
#endif
		"     --color=MODE                       output colorization mode: 'never' (default), 'always' or 'auto'\n"
		"     --log-format=FORMAT                set the format of individual log lines (on standard output): 'text' (default) or 'json'\n"
		"     --grpc-port=PORT                   listen for gRPC clients on this port\n"
#ifdef ENABLE_PYTEST
		"     --flow-timeout=SECONDS             inactive flow timeout (except TCP established flows)\n"
#endif
	);
}

static char pf0_name[IFNAMSIZ];
static char pf1_name[IFNAMSIZ];
static char vf_pattern[IFNAMSIZ];
static int dhcp_mtu = 1500;
static double wcmp_frac = 1.0;
static enum dp_conf_nic_type nic_type = DP_CONF_NIC_TYPE_HW;
static bool stats_enabled = true;
static bool conntrack_enabled = true;
static bool ipv6_overlay_enabled = false;
static bool offload_enabled = true;
#ifdef ENABLE_GRAPHTRACE
#ifdef ENABLE_PYTEST
static int graphtrace_loglevel = 0;
#endif
#endif
static enum dp_conf_color color = DP_CONF_COLOR_NEVER;
static enum dp_conf_log_format log_format = DP_CONF_LOG_FORMAT_TEXT;
static int grpc_port = 1337;
#ifdef ENABLE_PYTEST
static int flow_timeout = DP_FLOW_DEFAULT_TIMEOUT;
#endif

const char *dp_conf_get_pf0_name()
{
	return pf0_name;
}

const char *dp_conf_get_pf1_name()
{
	return pf1_name;
}

const char *dp_conf_get_vf_pattern()
{
	return vf_pattern;
}

int dp_conf_get_dhcp_mtu()
{
	return dhcp_mtu;
}

double dp_conf_get_wcmp_frac()
{
	return wcmp_frac;
}

enum dp_conf_nic_type dp_conf_get_nic_type()
{
	return nic_type;
}

bool dp_conf_is_stats_enabled()
{
	return stats_enabled;
}

bool dp_conf_is_conntrack_enabled()
{
	return conntrack_enabled;
}

bool dp_conf_is_ipv6_overlay_enabled()
{
	return ipv6_overlay_enabled;
}

bool dp_conf_is_offload_enabled()
{
	return offload_enabled;
}

#ifdef ENABLE_GRAPHTRACE
#ifdef ENABLE_PYTEST
int dp_conf_get_graphtrace_loglevel()
{
	return graphtrace_loglevel;
}

#endif
#endif
enum dp_conf_color dp_conf_get_color()
{
	return color;
}

enum dp_conf_log_format dp_conf_get_log_format()
{
	return log_format;
}

int dp_conf_get_grpc_port()
{
	return grpc_port;
}

#ifdef ENABLE_PYTEST
int dp_conf_get_flow_timeout()
{
	return flow_timeout;
}

#endif
