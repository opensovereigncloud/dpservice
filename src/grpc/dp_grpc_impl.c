#include "grpc/dp_grpc_impl.h"
#include <time.h>
#include "dp_error.h"
#include "dp_lb.h"
#include "dp_log.h"
#include "dp_lpm.h"
#include "dp_nat.h"
#ifdef ENABLE_VIRTSVC
#	include "dp_virtsvc.h"
#endif
#include "dp_vnf.h"
#include "dp_vni.h"
#include "dpdk_layer.h"
#include "grpc/dp_grpc_api.h"
#include "grpc/dp_grpc_responder.h"

#define DP_SHOW_EXT_ROUTES true
#define DP_SHOW_INT_ROUTES false

static uint32_t pfx_counter = 1;

static __rte_always_inline void dp_generate_underlay_ipv6(uint8_t route[DP_VNF_IPV6_ADDR_SIZE])
{
	uint32_t local = htonl(pfx_counter);
	uint8_t random_byte;

	srand(time(NULL));
	random_byte = rand() % 256;

	/* First 8 bytes for host */
	rte_memcpy(route, get_underlay_conf()->src_ip6, DP_VNF_IPV6_ADDR_SIZE);
	/* Following 2 bytes for kernel routing and 1 byte reserved */
	memset(route + 8, 0, 3);

#ifdef ENABLE_STATIC_UNDERLAY_IP
	/* 1 byte static value */
	uint8_t static_byte = 0x01;

	rte_memcpy(route + 11, &static_byte, 1);
	RTE_SET_USED(random_byte);
#else
	/* 1 byte random value */
	rte_memcpy(route + 11, &random_byte, 1);
#endif

	/* 4 byte counter */
	rte_memcpy(route + 12, &local, 4);

	pfx_counter++;
}

static int dp_insert_vnf_entry(struct dp_vnf_value *val, enum vnf_type v_type,
							   int vni, uint16_t portid, uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE])
{
	dp_generate_underlay_ipv6(ul_addr6);
	val->v_type = v_type;
	val->portid = portid;
	val->vni = vni;
	return dp_set_vnf_value((void *)ul_addr6, val);
}

static __rte_always_inline int dp_get_vnf_entry(struct dp_vnf_value *val, enum vnf_type v_type, uint16_t portid)
{
	val->v_type = v_type;
	val->portid = portid;
	val->vni = dp_get_vm_vni(portid);
	return dp_find_vnf_with_value(val);
}

static __rte_always_inline int dp_remove_vnf_entry(struct dp_vnf_value *val, enum vnf_type v_type, uint16_t portid)
{
	val->v_type = v_type;
	val->portid = portid;
	val->vni = dp_get_vm_vni(portid);
	return dp_del_vnf_with_value(val);
}

static int dp_process_add_lb(struct dp_grpc_responder *responder)
{
	struct dp_lb *request = &responder->request.add_lb;
	struct dp_ul_addr *reply = dp_grpc_single_reply(responder);

	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];
	struct dp_vnf_value vnf_val = {0};
	int ret = DP_GRPC_OK;
	int vni;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		vni = request->vni;
		if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_LB, vni, 0, ul_addr6))) {
			ret = DP_GRPC_ERR_VNF_INSERT;
			goto err;
		}
		ret = dp_create_lb(request, ul_addr6);
		if (DP_FAILED(ret))
			goto err_vnf;
		if (DP_FAILED(dp_create_vni_route_table(vni, DP_IP_PROTO_IPV4,
					  rte_eth_dev_socket_id(dp_port_get_pf0_id())))
		) {
			ret = DP_GRPC_ERR_VNI_INIT4;
			goto err_lb;
		}
	} else {
		ret = DP_GRPC_ERR_BAD_IPVER;
		goto err;
	}
	rte_memcpy(reply->addr6, ul_addr6, sizeof(reply->addr6));
	return DP_GRPC_OK;

err_lb:
	dp_delete_lb((void *)request->lb_id);
err_vnf:
	dp_del_vnf_with_vnf_key(ul_addr6);
err:
	return ret;
}

static int dp_process_del_lb(struct dp_grpc_responder *responder)
{
	struct dp_lb_id *request = &responder->request.del_lb;
	struct dp_lb lb;
	int ret;

	ret = dp_get_lb(request->lb_id, &lb);
	if (DP_FAILED(ret))
		return ret;

	dp_del_vnf_with_vnf_key(lb.ul_addr6);

	ret = dp_delete_lb(request->lb_id);
	if (DP_FAILED(ret))
		return ret;

	if (DP_FAILED(dp_delete_vni_route_table(lb.vni, DP_IP_PROTO_IPV4)))
		return DP_GRPC_ERR_VNI_FREE4;

	return DP_GRPC_OK;
}

static int dp_process_get_lb(struct dp_grpc_responder *responder)
{
	struct dp_lb_id *request = &responder->request.del_lb;
	struct dp_lb *reply = dp_grpc_single_reply(responder);

	return dp_get_lb(request->lb_id, reply);
}

static int dp_process_add_lb_target(struct dp_grpc_responder *responder)
{
	struct dp_lb_target *request = &responder->request.add_lbtrgt;

	if (request->ip_type == RTE_ETHER_TYPE_IPV6)
		return dp_add_lb_back_ip(request->lb_id, request->addr6, sizeof(request->addr6));
	else
		return DP_GRPC_ERR_BAD_IPVER;
}

static int dp_process_del_lb_target(struct dp_grpc_responder *responder)
{
	struct dp_lb_target *request = &responder->request.del_lbtrgt;

	if (request->ip_type == RTE_ETHER_TYPE_IPV6)
		return dp_del_lb_back_ip(request->lb_id, request->addr6);
	else
		return DP_GRPC_ERR_BAD_IPVER;
}

static int dp_process_init(struct dp_grpc_responder *responder)
{
	dp_del_all_neigh_nat_entries_in_vni(DP_NETWORK_NAT_ALL_VNI);
	return dp_lpm_reset_all_route_tables(rte_eth_dev_socket_id(dp_port_get_pf0_id()));
}

static int dp_process_vni_in_use(struct dp_grpc_responder *responder)
{
	struct dp_vni *request = &responder->request.vni_in_use;
	struct dp_vni_in_use *reply = dp_grpc_single_reply(responder);

	if (request->type == DP_VNI_IPV4) {
		reply->in_use = dp_is_vni_route_tbl_available(request->vni,
													  DP_IP_PROTO_IPV4,
													  rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	} else
		return DP_GRPC_ERR_WRONG_TYPE;

	return DP_GRPC_OK;
}

static int dp_process_add_fwall_rule(struct dp_grpc_responder *responder)
{
	struct dp_fwrule *request = &responder->request.add_fwrule;
	int port_id;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	if (dp_get_firewall_rule(request->rule.rule_id, port_id))
		return DP_GRPC_ERR_ALREADY_EXISTS;

	if (request->rule.action == DP_FWALL_DROP)
		return DP_GRPC_ERR_NO_DROP_SUPPORT;

	if (DP_FAILED(dp_add_firewall_rule(&request->rule, port_id)))
		return DP_GRPC_ERR_OUT_OF_MEMORY;

	return DP_GRPC_OK;
}

static int dp_process_get_fwall_rule(struct dp_grpc_responder *responder)
{
	struct dp_fwrule_id *request = &responder->request.get_fwrule;
	struct dp_fwall_rule *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct dp_fwall_rule *rule;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	rule = dp_get_firewall_rule(request->rule_id, port_id);
	if (!rule)
		return DP_GRPC_ERR_NOT_FOUND;

	*reply = *rule;
	return DP_GRPC_OK;
}

static int dp_process_del_fwall_rule(struct dp_grpc_responder *responder)
{
	struct dp_fwrule_id *request = &responder->request.del_fwrule;
	int port_id;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	if (DP_FAILED(dp_delete_firewall_rule(request->rule_id, port_id)))
		return DP_GRPC_ERR_NOT_FOUND;

	return DP_GRPC_OK;
}

static int dp_process_vni_reset(struct dp_grpc_responder *responder)
{
	struct dp_vni *request = &responder->request.vni_reset;

	if (request->type == DP_VNI_BOTH)
		return dp_lpm_reset_route_tables(request->vni, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	else
		return DP_GRPC_ERR_WRONG_TYPE;
}

static int dp_process_add_vip(struct dp_grpc_responder *responder)
{
	struct dp_vip *request = &responder->request.add_vip;
	struct dp_ul_addr *reply = dp_grpc_single_reply(responder);

	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];
	struct dp_vnf_value vnf_val = {0};
	int port_id;
	uint32_t vm_ip, vm_vni;
	uint32_t vip;
	int ret;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id)) {
		ret = DP_GRPC_ERR_NO_VM;
		goto err;
	}

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		vm_ip = dp_get_dhcp_range_ip4(port_id);
		vm_vni = dp_get_vm_vni(port_id);
		if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_VIP, vm_vni, port_id, ul_addr6))) {
			ret = DP_GRPC_ERR_VNF_INSERT;
			goto err;
		}
		vip = ntohl(request->addr);
		ret = dp_set_vm_snat_ip(vm_ip, vip, vm_vni, ul_addr6);
		if (DP_FAILED(ret))
			goto err_vnf;

		ret = dp_set_dnat_ip(vip, vm_ip, vm_vni);
		if (DP_FAILED(ret))
			goto err_snat;

		rte_memcpy(reply->addr6, ul_addr6, sizeof(reply->addr6));
	} else {
		ret = DP_GRPC_ERR_BAD_IPVER;
		goto err;
	}
	return DP_GRPC_OK;

err_snat:
	dp_del_vm_snat_ip(vm_ip, vm_vni);
err_vnf:
	dp_del_vnf_with_vnf_key(ul_addr6);
err:
	return ret;
}

static int dp_process_del_vip(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.del_vip;
	struct dp_vip *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct snat_data *s_data;
	uint32_t vm_ip, vm_vni;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	vm_ip = dp_get_dhcp_range_ip4(port_id);
	vm_vni = dp_get_vm_vni(port_id);

	s_data = dp_get_vm_snat_data(vm_ip, vm_vni);
	if (!s_data || !s_data->vip_ip)
		return DP_GRPC_ERR_SNAT_NO_DATA;

	dp_del_vnf_with_vnf_key(s_data->ul_ip6);

	reply->addr = s_data->vip_ip;

	// always delete, i.e. do not use dp_del_vip_from_dnat(),
	// because 1:1 VIP is not shared with anything
	dp_del_dnat_ip(s_data->vip_ip, vm_vni);
	dp_del_vm_snat_ip(vm_ip, vm_vni);

	return DP_GRPC_OK;
}

static int dp_process_get_vip(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.get_vip;
	struct dp_vip *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct snat_data *s_data;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	s_data = dp_get_vm_snat_data(dp_get_dhcp_range_ip4(port_id), dp_get_vm_vni(port_id));
	if (!s_data || !s_data->vip_ip)
		return DP_GRPC_ERR_SNAT_NO_DATA;

	reply->addr = htonl(s_data->vip_ip);
	rte_memcpy(reply->ul_addr6, s_data->ul_ip6, sizeof(reply->ul_addr6));
	return DP_GRPC_OK;
}

static int dp_process_add_lb_prefix(struct dp_grpc_responder *responder)
{
	struct dp_prefix *request = &responder->request.add_lbpfx;
	struct dp_route *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct dp_vnf_value vnf_val = {
		.alias_pfx.ip = ntohl(request->addr),
		.alias_pfx.length = request->length,
	};
	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	if (!DP_FAILED(dp_get_vnf_entry(&vnf_val, DP_VNF_TYPE_LB_ALIAS_PFX, port_id)))
		return DP_GRPC_ERR_ALREADY_EXISTS;

	if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_LB_ALIAS_PFX, dp_get_vm_vni(port_id), port_id, ul_addr6)))
		return DP_GRPC_ERR_VNF_INSERT;

	rte_memcpy(reply->trgt_addr6, ul_addr6, sizeof(reply->trgt_addr6));
	return DP_GRPC_OK;
}

static int dp_process_del_lb_prefix(struct dp_grpc_responder *responder)
{
	struct dp_prefix *request = &responder->request.del_lbpfx;

	int port_id;
	struct dp_vnf_value vnf_val = {
		.alias_pfx.ip = ntohl(request->addr),
		.alias_pfx.length = request->length,
	};

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	return dp_remove_vnf_entry(&vnf_val, DP_VNF_TYPE_LB_ALIAS_PFX, port_id);
}

static int dp_process_add_prefix(struct dp_grpc_responder *responder)
{
	struct dp_prefix *request = &responder->request.add_pfx;
	struct dp_ul_addr *reply = dp_grpc_single_reply(responder);

	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];
	int port_id;
	uint32_t vm_vni;
	int socket_id;
	struct dp_vnf_value vnf_val = {
		.alias_pfx.ip = ntohl(request->addr),
		.alias_pfx.length = request->length,
	};
	int ret;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		vm_vni = dp_get_vm_vni(port_id);
		socket_id = rte_eth_dev_socket_id(port_id);
		ret = dp_add_route(port_id, vm_vni, 0, vnf_val.alias_pfx.ip, NULL, vnf_val.alias_pfx.length, socket_id);
		if (DP_FAILED(ret))
			return ret;

		if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_ALIAS_PFX, vm_vni, port_id, ul_addr6))) {
			dp_del_route(port_id, vm_vni, 0, vnf_val.alias_pfx.ip, NULL, vnf_val.alias_pfx.length, socket_id);
			return DP_GRPC_ERR_VNF_INSERT;
		}
		rte_memcpy(reply->addr6, ul_addr6, sizeof(reply->addr6));
	} else
		return DP_GRPC_ERR_BAD_IPVER;

	return DP_GRPC_OK;
}

static int dp_process_del_prefix(struct dp_grpc_responder *responder)
{
	struct dp_prefix *request = &responder->request.del_pfx;

	int port_id;
	struct dp_vnf_value vnf_val = {
		.alias_pfx.ip = ntohl(request->addr),
		.alias_pfx.length = request->length,
	};
	int ret, ret2;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		ret = dp_del_route(port_id, dp_get_vm_vni(port_id), 0,
						   vnf_val.alias_pfx.ip, 0,
						   vnf_val.alias_pfx.length, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
		// ignore the error and try to delete the vnf entry anyway
	} else
		return DP_GRPC_ERR_BAD_IPVER;

	ret2 = dp_remove_vnf_entry(&vnf_val, DP_VNF_TYPE_ALIAS_PFX, port_id);
	return DP_FAILED(ret) ? ret : ret2;
}

static int dp_process_add_interface(struct dp_grpc_responder *responder)
{
	struct dp_iface *request = &responder->request.add_iface;
	struct dp_vf_pci *reply = dp_grpc_single_reply(responder);

	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];
	uint16_t port_id = DP_INVALID_PORT_ID;
	struct dp_vnf_value vnf_val;
	int ret = DP_GRPC_OK;
	int socket_id;

	if (request->pci_name[0] == '\0'
		|| DP_FAILED(rte_eth_dev_get_port_by_name(request->pci_name, &port_id))
	) {
		ret = DP_GRPC_ERR_NOT_FOUND;
		goto err;
	}

	if (port_id == DP_INVALID_PORT_ID) {
		ret = DP_GRPC_ERR_LIMIT_REACHED;
		goto err;
	}

	if (!dp_port_is_vf_free(port_id)) {
		ret = DP_GRPC_ERR_ALREADY_EXISTS;
		// fill the device details anyway so the caller knows which one is already allocated
		// TODO as below, fill in properly
		reply->bus = 2;
		reply->domain = 2;
		reply->function = 2;
		rte_eth_dev_get_name_by_port(port_id, reply->name);
		goto err;
	}

	// can only fail if the port_id is invalid
	socket_id = rte_eth_dev_socket_id(port_id);

	if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_INTERFACE_IP, request->vni, port_id, ul_addr6))) {
		ret = DP_GRPC_ERR_VNF_INSERT;
		goto err;
	}
	if (DP_FAILED(dp_map_vm_handle(request->iface_id, port_id))) {
		ret = DP_GRPC_ERR_VM_HANDLE;
		goto err_vnf;
	}
	if (DP_FAILED(setup_vm(port_id, request->vni, socket_id))) {
		ret = DP_GRPC_ERR_VNI_INIT4;
		goto handle_err;
	}
	if (DP_FAILED(setup_vm6(port_id, request->vni, socket_id))) {
		ret = DP_GRPC_ERR_VNI_INIT6;
		goto vm_err;
	}
	dp_set_dhcp_range_ip4(port_id, ntohl(request->ip4_addr), DP_LPM_DHCP_IP_DEPTH, socket_id);
	dp_set_vm_pxe_ip4(port_id, ntohl(request->ip4_pxe_addr), socket_id);
	dp_set_vm_pxe_str(port_id, request->pxe_str);
	dp_set_dhcp_range_ip6(port_id, request->ip6_addr, DP_LPM_DHCP_IP6_DEPTH, socket_id);
	ret = dp_add_route(port_id, request->vni, 0, ntohl(request->ip4_addr), NULL, 32, socket_id);
	if (DP_FAILED(ret))
		goto vm_err;
	ret = dp_add_route6(port_id, request->vni, 0, request->ip6_addr, NULL, 128, socket_id);
	if (DP_FAILED(ret))
		goto route_err;
	if (DP_FAILED(dp_port_start(port_id))) {
		ret = DP_GRPC_ERR_PORT_START;
		goto route6_err;
	}
	/* TODO get the pci info of this port and fill it accordingly */
	// NOTE: this should be part of dp_port structure so no rte_ call should be needed at this point
	reply->bus = 2;
	reply->domain = 2;
	reply->function = 2;
	rte_eth_dev_get_name_by_port(port_id, reply->name);

	rte_memcpy(dp_get_vm_ul_ip6(port_id), ul_addr6, sizeof(ul_addr6));
	rte_memcpy(reply->ul_addr6, dp_get_vm_ul_ip6(port_id), sizeof(reply->ul_addr6));
	return DP_GRPC_OK;

route6_err:
	dp_del_route6(port_id, request->vni, 0, request->ip6_addr, NULL, 128, socket_id);
route_err:
	dp_del_route(port_id, request->vni, 0, ntohl(request->ip4_addr), NULL, 32, socket_id);
vm_err:
	dp_del_vm(port_id, socket_id, DP_LPM_ROLLBACK);
handle_err:
	dp_del_portid_with_vm_handle(request->iface_id);
err_vnf:
	dp_del_vnf_with_vnf_key(ul_addr6);
err:
	return ret;
}

static int dp_process_del_interface(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.del_iface;

	int port_id;
	int ret = DP_GRPC_OK;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NOT_FOUND;

	dp_del_vnf_with_vnf_key(dp_get_vm_ul_ip6(port_id));
	if (DP_FAILED(dp_port_stop(port_id)))
		ret = DP_GRPC_ERR_PORT_STOP;
	// carry on with cleanup though
	dp_del_portid_with_vm_handle(request->iface_id);
	dp_del_vm(port_id, rte_eth_dev_socket_id(port_id), !DP_LPM_ROLLBACK);
#ifdef ENABLE_VIRTSVC
	dp_virtsvc_del_vm(port_id);
#endif
	return ret;
}

static int dp_process_get_interface(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.get_iface;
	struct dp_iface *reply = dp_grpc_single_reply(responder);
	int port_id;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NOT_FOUND;

	reply->ip4_addr = dp_get_dhcp_range_ip4(port_id);
	rte_memcpy(reply->ip6_addr, dp_get_dhcp_range_ip6(port_id), sizeof(reply->ip6_addr));
	reply->vni = dp_get_vm_vni(port_id);
	rte_memcpy(reply->iface_id, dp_get_vm_machineid(port_id), sizeof(reply->iface_id));
	rte_eth_dev_get_name_by_port(port_id, reply->pci_name);
	rte_memcpy(reply->ul_addr6, dp_get_vm_ul_ip6(port_id), sizeof(reply->ul_addr6));
	return DP_GRPC_OK;
}

static int dp_process_add_route(struct dp_grpc_responder *responder)
{
	struct dp_route *request = &responder->request.add_route;

	if (request->pfx_ip_type == RTE_ETHER_TYPE_IPV4) {
		return dp_add_route(dp_port_get_pf0_id(), request->vni, request->trgt_vni,
							ntohl(request->pfx_addr), request->trgt_addr6,
							request->pfx_length, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	} else if (request->pfx_ip_type == RTE_ETHER_TYPE_IPV6) {
		return dp_add_route6(dp_port_get_pf0_id(), request->vni, request->trgt_vni,
							 request->pfx_addr6, request->trgt_addr6,
							 request->pfx_length, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	} else
		return DP_GRPC_ERR_BAD_IPVER;
}

static int dp_process_del_route(struct dp_grpc_responder *responder)
{
	struct dp_route *request = &responder->request.del_route;

	if (request->pfx_ip_type == RTE_ETHER_TYPE_IPV4) {
		return dp_del_route(dp_port_get_pf0_id(), request->vni, request->trgt_vni,
							ntohl(request->pfx_addr), request->trgt_addr6,
							request->pfx_length, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	} else if (request->pfx_ip_type == RTE_ETHER_TYPE_IPV6) {
		return dp_del_route6(dp_port_get_pf0_id(), request->vni, request->trgt_vni,
							 request->pfx_addr6, request->trgt_addr6,
							 request->pfx_length, rte_eth_dev_socket_id(dp_port_get_pf0_id()));
	} else
		return DP_GRPC_ERR_BAD_IPVER;
}

static int dp_process_add_nat(struct dp_grpc_responder *responder)
{
	struct dp_nat *request = &responder->request.add_nat;
	struct dp_ul_addr *reply = dp_grpc_single_reply(responder);

	uint8_t ul_addr6[DP_VNF_IPV6_ADDR_SIZE];
	struct dp_vnf_value vnf_val = {0};
	int port_id;
	uint32_t vm_ip, vm_vni;
	int ret;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id)) {
		ret = DP_GRPC_ERR_NO_VM;
		goto err;
	}

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		vm_ip = dp_get_dhcp_range_ip4(port_id);
		vm_vni = dp_get_vm_vni(port_id);
		if (DP_FAILED(dp_insert_vnf_entry(&vnf_val, DP_VNF_TYPE_NAT, vm_vni, port_id, ul_addr6))) {
			ret = DP_GRPC_ERR_VNF_INSERT;
			goto err;
		}
		ret = dp_set_vm_network_snat_ip(vm_ip, ntohl(request->addr), vm_vni,
										request->min_port, request->max_port,
										ul_addr6);
		if (DP_FAILED(ret))
			goto err_vnf;

		ret = dp_set_dnat_ip(ntohl(request->addr), 0, vm_vni);
		if (DP_FAILED(ret) && ret != DP_GRPC_ERR_DNAT_EXISTS)
			goto err_dnat;
		rte_memcpy(reply->addr6, ul_addr6, sizeof(reply->addr6));
	} else {
		ret = DP_GRPC_ERR_BAD_IPVER;
		goto err;
	}
	return DP_GRPC_OK;

err_dnat:
	dp_del_vm_network_snat_ip(vm_ip, vm_vni);
err_vnf:
	dp_del_vnf_with_vnf_key(ul_addr6);
err:
	return ret;

}

static int dp_process_del_nat(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.del_nat;
	struct dp_vip *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct snat_data *s_data;
	uint32_t vm_ip, vm_vni;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	vm_ip = dp_get_dhcp_range_ip4(port_id);
	vm_vni = dp_get_vm_vni(port_id);

	s_data = dp_get_vm_snat_data(vm_ip, vm_vni);
	if (!s_data || !s_data->network_nat_ip)
		return DP_GRPC_ERR_SNAT_NO_DATA;

	dp_del_vnf_with_vnf_key(s_data->ul_nat_ip6);

	reply->addr = s_data->network_nat_ip;
	dp_del_vip_from_dnat(s_data->network_nat_ip, vm_vni);
	return dp_del_vm_network_snat_ip(vm_ip, vm_vni);
}

static int dp_process_get_nat(struct dp_grpc_responder *responder)
{
	struct dp_iface_id *request = &responder->request.get_nat;
	struct dp_nat *reply = dp_grpc_single_reply(responder);

	int port_id;
	struct snat_data *s_data;

	port_id = dp_get_portid_with_vm_handle(request->iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	s_data = dp_get_vm_snat_data(dp_get_dhcp_range_ip4(port_id), dp_get_vm_vni(port_id));
	if (!s_data || !s_data->network_nat_ip)
		return DP_GRPC_ERR_SNAT_NO_DATA;

	reply->addr = htonl(s_data->network_nat_ip);
	reply->min_port = s_data->network_nat_port_range[0];
	reply->max_port = s_data->network_nat_port_range[1];
	rte_memcpy(reply->ul_addr6, s_data->ul_nat_ip6, sizeof(reply->ul_addr6));
	return DP_GRPC_OK;
}

static int dp_process_add_neigh_nat(struct dp_grpc_responder *responder)
{
	struct dp_nat *request = &responder->request.add_nat;
	int ret;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		ret = dp_add_network_nat_entry(ntohl(request->addr), NULL,
									   request->neigh_vni,
									   request->min_port,
									   request->max_port,
									   request->neigh_addr6);
		if (DP_FAILED(ret))
			return ret;

		ret = dp_set_dnat_ip(ntohl(request->addr), 0, request->neigh_vni);
		if (DP_FAILED(ret) && ret != DP_GRPC_ERR_DNAT_EXISTS)
			return ret;
	} else
		return DP_GRPC_ERR_BAD_IPVER;

	return DP_GRPC_OK;
}

static int dp_process_del_neigh_nat(struct dp_grpc_responder *responder)
{
	struct dp_nat *request = &responder->request.del_neighnat;
	int ret;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		ret = dp_del_network_nat_entry(ntohl(request->addr), NULL,
									   request->neigh_vni,
									   request->min_port,
									   request->max_port);
		if (DP_FAILED(ret))
			return ret;

		dp_del_vip_from_dnat(ntohl(request->addr), request->neigh_vni);
	} else
		return DP_GRPC_ERR_BAD_IPVER;

	return DP_GRPC_OK;

}

static int dp_process_list_interfaces(struct dp_grpc_responder *responder)
{
	struct dp_iface *reply;
	int act_ports[DP_MAX_PORTS];
	int count;

	count = dp_get_active_vm_ports(act_ports);
	if (!count)
		return DP_GRPC_OK;

	dp_grpc_set_multireply(responder, sizeof(*reply));

	for (int i = 0; i < count; ++i) {
		reply = dp_grpc_add_reply(responder);
		if (!reply)
			return DP_GRPC_OK;  // just truncate the list
		reply->ip4_addr = dp_get_dhcp_range_ip4(act_ports[i]);
		rte_memcpy(reply->ip6_addr, dp_get_dhcp_range_ip6(act_ports[i]),
				   sizeof(reply->ip6_addr));
		reply->vni = dp_get_vm_vni(act_ports[i]);
		rte_memcpy(reply->iface_id, dp_get_vm_machineid(act_ports[i]),
			sizeof(reply->iface_id));
		rte_eth_dev_get_name_by_port(act_ports[i], reply->pci_name);
		rte_memcpy(reply->ul_addr6, dp_get_vm_ul_ip6(act_ports[i]), sizeof(reply->ul_addr6));
	}

	return DP_GRPC_OK;
}

static int dp_process_list_routes(struct dp_grpc_responder *responder)
{
	// ignore errors (already logged) and return at least partial list
	dp_list_routes(responder->request.list_route.vni, rte_eth_dev_socket_id(dp_port_get_pf0_id()), 0, DP_SHOW_EXT_ROUTES, responder);
	return DP_GRPC_OK;
}

static int dp_process_list_lb_targets(struct dp_grpc_responder *responder)
{
	return dp_get_lb_back_ips(responder->request.list_lbtrgt.lb_id, responder);
}

static int dp_process_list_fwall_rules(struct dp_grpc_responder *responder)
{
	int port_id;

	port_id = dp_get_portid_with_vm_handle(responder->request.list_fwrule.iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	dp_list_firewall_rules(port_id, responder);
	return DP_GRPC_OK;
}

static int dp_process_list_lb_prefixes(struct dp_grpc_responder *responder)
{
	int port_id;

	port_id = dp_get_portid_with_vm_handle(responder->request.list_lbpfx.iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	dp_list_vnf_alias_routes(port_id, DP_VNF_TYPE_LB_ALIAS_PFX, responder);
	return DP_GRPC_OK;
}

static int dp_process_list_prefixes(struct dp_grpc_responder *responder)
{
	int port_id;

	port_id = dp_get_portid_with_vm_handle(responder->request.list_pfx.iface_id);
	if (DP_FAILED(port_id))
		return DP_GRPC_ERR_NO_VM;

	dp_list_vnf_alias_routes(port_id, DP_VNF_TYPE_ALIAS_PFX, responder);
	return DP_GRPC_OK;
}

static int dp_process_list_nat_entries(struct dp_grpc_responder *responder)
{
	struct dp_nat_id *request = &responder->request.list_nat;
	int ret;

	if (request->ip_type == RTE_ETHER_TYPE_IPV4) {
		if (request->type == DP_NETNAT_INFO_TYPE_LOCAL)
			ret = dp_list_nat_local_entry(ntohl(request->addr), responder);
		else if (request->type == DP_NETNAT_INFO_TYPE_NEIGHBOR)
			ret = dp_list_nat_neigh_entry(ntohl(request->addr), responder);
		else
			return DP_GRPC_ERR_WRONG_TYPE;
		return DP_FAILED(ret) ? DP_GRPC_ERR_ITERATOR : DP_GRPC_OK;
	} else {
		return DP_GRPC_ERR_BAD_IPVER;
	}
}

void dp_process_request(struct rte_mbuf *m)
{
	struct dp_grpc_responder responder;
	uint8_t request_type;
	int ret;

	request_type = dp_grpc_init_responder(&responder, m);

	switch (request_type) {
	case DP_REQ_TYPE_INIT:
		ret = dp_process_init(&responder);
		break;
	case DP_REQ_TYPE_IS_VNI_IN_USE:
		ret = dp_process_vni_in_use(&responder);
		break;
	case DP_REQ_TYPE_VNI_RESET:
		ret = dp_process_vni_reset(&responder);
		break;
	case DP_REQ_TYPE_CREATELB:
		ret = dp_process_add_lb(&responder);
		break;
	case DP_REQ_TYPE_GETLB:
		ret = dp_process_get_lb(&responder);
		break;
	case DP_REQ_TYPE_DELLB:
		ret = dp_process_del_lb(&responder);
		break;
	case DP_REQ_TYPE_ADDLBVIP:
		ret = dp_process_add_lb_target(&responder);
		break;
	case DP_REQ_TYPE_DELLBVIP:
		ret = dp_process_del_lb_target(&responder);
		break;
	case DP_REQ_TYPE_ADDVIP:
		ret = dp_process_add_vip(&responder);
		break;
	case DP_REQ_TYPE_DELVIP:
		ret = dp_process_del_vip(&responder);
		break;
	case DP_REQ_TYPE_GETVIP:
		ret = dp_process_get_vip(&responder);
		break;
	case DP_REQ_TYPE_ADDPREFIX:
		ret = dp_process_add_prefix(&responder);
		break;
	case DP_REQ_TYPE_DELPREFIX:
		ret = dp_process_del_prefix(&responder);
		break;
	case DP_REQ_TYPE_ADDLBPREFIX:
		ret = dp_process_add_lb_prefix(&responder);
		break;
	case DP_REQ_TYPE_DELLBPREFIX:
		ret = dp_process_del_lb_prefix(&responder);
		break;
	case DP_REQ_TYPE_ADDMACHINE:
		ret = dp_process_add_interface(&responder);
		break;
	case DP_REQ_TYPE_DELMACHINE:
		ret = dp_process_del_interface(&responder);
		break;
	case DP_REQ_TYPE_GETMACHINE:
		ret = dp_process_get_interface(&responder);
		break;
	case DP_REQ_TYPE_ADDROUTE:
		ret = dp_process_add_route(&responder);
		break;
	case DP_REQ_TYPE_DELROUTE:
		ret = dp_process_del_route(&responder);
		break;
	case DP_REQ_TYPE_LISTROUTE:
		ret = dp_process_list_routes(&responder);
		break;
	case DP_REQ_TYPE_ADD_NATVIP:
		ret = dp_process_add_nat(&responder);
		break;
	case DP_REQ_TYPE_GET_NATENTRY:
		ret = dp_process_list_nat_entries(&responder);  // TODO talk about this name in all layers, it's always different
		break;
	case DP_REQ_TYPE_DEL_NATVIP:
		ret = dp_process_del_nat(&responder);
		break;
	case DP_REQ_TYPE_GET_NATVIP:
		ret = dp_process_get_nat(&responder);
		break;
	case DP_REQ_TYPE_ADD_NEIGH_NAT:
		ret = dp_process_add_neigh_nat(&responder);
		break;
	case DP_REQ_TYPE_DEL_NEIGH_NAT:
		ret = dp_process_del_neigh_nat(&responder);
		break;
	case DP_REQ_TYPE_LISTPREFIX:
		ret = dp_process_list_prefixes(&responder);
		break;
	case DP_REQ_TYPE_LISTLBPREFIX:
		ret = dp_process_list_lb_prefixes(&responder);
		break;
	case DP_REQ_TYPE_LIST_FWALL_RULES:
		ret = dp_process_list_fwall_rules(&responder);
		break;
	case DP_REQ_TYPE_LISTLBBACKENDS:
		ret = dp_process_list_lb_targets(&responder);
		break;
	case DP_REQ_TYPE_LISTMACHINE:
		ret = dp_process_list_interfaces(&responder);
		break;
	case DP_REQ_TYPE_ADD_FWALL_RULE:
		ret = dp_process_add_fwall_rule(&responder);
		break;
	case DP_REQ_TYPE_DEL_FWALL_RULE:
		ret = dp_process_del_fwall_rule(&responder);
		break;
	case DP_REQ_TYPE_GET_FWALL_RULE:
		ret = dp_process_get_fwall_rule(&responder);
		break;
	default:
		ret = DP_GRPC_ERR_BAD_REQUEST;
		break;
	}

	if (DP_FAILED(ret)) {
		// as gRPC errors are explicitely defined due to API reasons
		// extract the proper value from the standard (negative) retvals
		ret = dp_errcode_to_grpc_errcode(ret);
		DPGRPC_LOG_WARNING("Failed request", DP_LOG_GRPCRET(ret), DP_LOG_GRPCERR(ret));
	}

	dp_grpc_send_response(&responder, ret);
}
