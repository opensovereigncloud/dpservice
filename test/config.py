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
# t_vni needs to be set, when we use geneve
t_vni = "0"
ul_ipv6 = "2a10:afc0:e01f:f403::1"
# vni 100 (0x64) is encoded in these addresses
ul_actual_dst="2a10:afc0:e01f:f408:0:64::"
ul_actual_src="2a10:afc0:e01f:f403:0:64::"
start_str = "DPDK main loop started"
tun_type_geneve="geneve"
port_redundancy=True

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
back_ip1 = "195.168.129.1"
back_ip2 = "195.168.129.2"
public_ip = "45.86.6.6"

ov_target_ip = "192.168.129.5"
ov_target_pfx = "192.168.129.0"
virtual_ip = "174.23.4.5"
pfx_ip = "174.23.4.0"

MAX_LINES_ROUTE_REPLY = 36
