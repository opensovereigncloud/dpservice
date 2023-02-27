#ifndef __INCLUDE_DP_RTE_FLOW_INIT_H
#define __INCLUDE_DP_RTE_FLOW_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int dp_install_isolated_mode_ipip(int port_id, uint8_t proto_id);
int dp_install_isolated_mode_geneve(int port_id);
#ifdef ENABLE_VIRTSVC
int dp_install_isolated_mode_virtsvc(int port_id, uint8_t proto_id, uint8_t svc_ipv6[16], uint16_t svc_port);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_DP_RTE_FLOW_INIT_H */
