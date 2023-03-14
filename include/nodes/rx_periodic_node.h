#ifndef __INCLUDE_RX_PERIODIC_NODE_PRIV_H__
#define __INCLUDE_RX_PERIODIC_NODE_PRIV_H__

#include "dpdk_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	RX_PERIODIC_NEXT_CLS,
	RX_PERIODIC_NEXT_MAX
};

struct rx_periodic_node_ctx
{
	// uint16_t queue_id; // it is not needed and was not initialized actually
	uint16_t next;
};

struct rx_periodic_node_receive_queues {
	struct rte_ring *periodic_msg_queue;
	struct rte_ring *grpc_tx;
	struct rte_ring *grpc_rx; 
	struct rte_ring *monitoring_rx;
};

struct rx_periodic_node_main {
 	uint16_t next_index[DP_MAX_PORTS];
 };

int rx_periodic_set_next(uint16_t port_id, uint16_t next_index);
struct rte_node_register *rx_periodic_node_get(void);
#ifdef __cplusplus
}
#endif
#endif
