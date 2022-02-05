#ifndef __INCLUDE_DP_GRPC_IMPL_H__
#define __INCLUDE_DP_GRPC_IMPL_H__

#include <stdint.h>
#include <rte_mbuf.h>
#include "dp_lpm.h"
#include "dp_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DP_REQ_TYPE_NONE,
	DP_REQ_TYPE_HELLO,
	DP_REQ_TYPE_ADDVIP,
	DP_REQ_TYPE_DELVIP,
	DP_REQ_TYPE_GETVIP,
	DP_REQ_TYPE_ADDMACHINE,
	DP_REQ_TYPE_DELMACHINE,
} dp_req_type;

typedef struct dp_com_head {
	uint16_t com_type;
	uint8_t is_chained;
	uint8_t buf_count;
} dp_com_head;

typedef struct dp_vip {
	uint32_t ip_type;
	union {
		uint32_t	vip_addr;
		uint8_t		vip_addr6[16];
	} vip;
	char machine_id[VM_MACHINE_ID_STR_LEN];
} dp_vip;

typedef struct dp_addmachine {
	uint32_t	ip4_addr;
	uint8_t		ip6_addr6[16];
	char		machine_id[VM_MACHINE_ID_STR_LEN];
	uint32_t	vni;
} dp_addmachine;

typedef struct dp_delmachine {
	char		machine_id[VM_MACHINE_ID_STR_LEN];
} dp_delmachine;

typedef struct dp_delvip {
	char		machine_id[VM_MACHINE_ID_STR_LEN];
} dp_delvip;

typedef struct dp_request {
	dp_com_head com_head;
	union {
		uint32_t		hello;
		dp_vip			add_vip;
		dp_addmachine	add_machine;
		dp_delmachine	del_machine;
		dp_delvip		del_vip;
	};
} dp_request;

typedef struct dp_vf_pci {
	char		name[IFNAMSIZ];
	uint32_t	domain;
	uint32_t	bus;
	uint32_t	slot;
	uint32_t	function;
} dp_vf_pci;

typedef struct dp_reply {
	dp_com_head com_head;
	union {
		uint32_t	hello;
		dp_vip		get_vip;
		dp_vf_pci	vf_pci;
	};
} dp_reply;

int dp_send_to_worker(dp_request *req);
int dp_recv_from_worker(dp_reply *rep);
int dp_process_request(struct rte_mbuf *m);
void dp_fill_head(dp_com_head* head, uint16_t type,
				  uint8_t is_chained, uint8_t count);

#ifdef __cplusplus
}
#endif
#endif