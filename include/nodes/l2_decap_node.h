#ifndef __INCLUDE_L2_DECAP_NODE_H__
#define __INCLUDE_L2_DECAP_NODE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int l2_decap_node_append_vf_tx(uint16_t port_id, const char *tx_node_name);

#ifdef __cplusplus
}
#endif
#endif
