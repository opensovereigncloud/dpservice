pf0_tap = "dtap0"
pf1_tap = "dtap1"
pf0_mac = "90:3c:b3:33:72:fb"
pf1_mac = "90:3c:b3:33:72:fc"
vf0_mac = "66:73:20:a9:a7:00"
vf0_tap = "dtapvf_0"
vf1_mac = "66:73:20:a9:a7:12"
vf1_tap = "dtapvf_1"
vf2_mac = "66:73:20:a9:a7:14"
vf2_tap = "dtapvf_2"
vf_patt = "dtapvf_"
vm1_name = "vm1"
vm2_name = "vm2"
vm3_name = "vm3"
vni = "100"
vni2 = "101"
# t_vni needs to be set, when we use geneve
t_vni = "0"
ul_ipv6 = "2a10:afc0:e01f:f403::1"
# vni 100 (0x64) is encoded in these addresses
ul_actual_dst="2a10:afc0:e01f:f408:0:64::"
ul_actual_src="2a10:afc0:e01f:f403:0:64::"
ul_short_src="2a10:afc0:e01f:f403:"
tun_type_geneve="geneve"
dhcp_mtu = 1337
dhcp_dns1 = "8.8.4.4"
dhcp_dns2 = "8.8.8.8"

bcast_mac = "ff:ff:ff:ff:ff:ff"
null_ip = "0.0.0.0"
gw_ip4 = "169.254.0.1"
bc_ip = "255.255.255.255"
vf0_ip ="172.32.10.5"
vf0_ipv6 = "2001::10"
vf1_ip ="176.44.33.12"
vf1_ipv6 = "2001::20"
vf2_ip ="176.44.45.20"
vf2_ipv6 = "2001::30"
gw_ip6 = "fe80::1"
mc_ip6 = "ff02::1"
mc_mac = "33:33:00:00:00:01"
back_ip1 = "2a10:abc0:d015:4027:0:c8::"
back_ip2 = "2a10:abc0:d015:4027:0:7b::"
public_ip = "45.86.6.6"
public_server_ip = public_ip
public_server_port = 443

ov_target_ip = "192.168.129.5"
ov_target_pfx = "192.168.129.0"
virtual_ip = "174.23.4.5"
nat_vip = "176.22.22.22"
nat_local_min_port = 100
nat_local_max_port = 102
nat_neigh_min_port = 500
nat_neigh_max_port = 520

nat_local_single_min_port = 100
nat_local_single_max_port = 101


nat_neigh_ul_dst="2a10:afc0:e01f:f406:0:64::"
pfx_ip = "174.23.4.0"
lb_name = "my_lb"
lb_ip = "174.12.3.4"

MAX_LINES_ROUTE_REPLY = 36

virtsvc_udp_svc_ipv6 = "2a00:da8:fff6::1"
virtsvc_udp_svc_port = 53
virtsvc_udp_virtual_ip = "1.2.3.4"
virtsvc_udp_virtual_port = 5353

virtsvc_tcp_svc_ipv6 = "2a00:da8:fff6::2"
virtsvc_tcp_svc_port = 443
virtsvc_tcp_virtual_ip = "5.6.7.8"
virtsvc_tcp_virtual_port = 4443

sniff_timeout = 2
