import pytest
import threading

from helpers import *

def send_bounce_pkt_to_pf(ipv6_nat):
	bouce_pkt = (Ether(dst=ipv6_multicast_mac, src=PF0.mac, type=0x86DD) /
				 IPv6(dst=ipv6_nat, src=router_ul_ipv6, nh=4) /
				 IP(dst=nat_vip, src=public_ip) /
				 TCP(sport=8989, dport=2050))
	delayed_sendp(bouce_pkt, PF0.tap)

def test_network_nat_pkt_relay(prepare_ifaces, grpc_client):

	nat_ul_ipv6 = grpc_client.addnat(VM1.name, nat_vip, nat_local_min_port, nat_local_max_port)

	grpc_client.addneighnat(nat_vip, vni1, 1000, 1100, "2a00:da8:fff6:2404:0:4d:0:28")
	grpc_client.addneighnat(nat_vip, vni1, 2000, 2100, "2a00:da8:fff6:1404:0:38:0:31")
	grpc_client.delneighnat(nat_vip, vni1, 1000, 1100)

	threading.Thread(target=send_bounce_pkt_to_pf,  args=(nat_ul_ipv6,)).start()

	# it seems that we also receive the injected packet, skip it
	pkt = sniff_packet(PF0.tap, is_tcp_pkt, skip=1)
	dst_ip = pkt[IPv6].dst
	dport = pkt[TCP].dport
	assert dst_ip == "2a00:da8:fff6:1404:0:38:0:31" and dport == 2050, \
		f"Wrong network-nat relayed packet (outer dst ipv6: {dst_ip}, dport: {dport})"
