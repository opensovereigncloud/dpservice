# Command-line tool for debugging over gRPC
There is a command-line tool to directly connect to a running dp-service and communicate with it (orchestrate it).

This tool is not production ready and will be replaced with a proper alternative. Please use it with caution.

## Types or argument values
 - `int` - a number; beware the use of unchecked `atoi(str[29])` here
 - `int` - a number; beware the use of unchecked `(uint32_t)atoi(str[29])` here
 - `ipv4` - an IP, expected to be valid; no checks, max length 29
 - `ipv6` - an IPv6, expected to be valid; no checks, max length 39
 - `portspec` - a string of length 29, expected to be a list of comma-separated `int` values; beware the use of `strtok()` match protospec!)
 - `protospec` - a string of length 29, expected to be a list of comma-separated `tcp|udp|icmp` string values; beware the use of `strtok()`

Note that when used, `portspec` and `protospec` definitions must match each other in order and length!

## Available commands:
Initialization/check for initialized service:
```
--init
--is_initialized
```

Add/delete/list network interfaces:
```
--addmachine <machine_name[63]> --vni <int> --ipv4 <ipv4> --ipv6 <ipv6> (--vm_pci <vm_pci[29]>) (--pxe_ip <ipv4> --pxe_str <path[29]>)
--delmachine <machine_name[63]>
--getmachine <machine_name[63]>
--getmachines
```

Add/delete/list routes (ip route equivalents):
```
--addroute --vni <int> [--ipv4 <ipv4>|--ipv6 <ipv6>] --length <int> --t_vni <int> --t_ipv6 <ipv6>
--delroute --vni <int> [--ipv4 <ipv4>|--ipv6 <ipv6>] --length <int> --t_vni <int> --t_ipv6 <ipv6>
--listroutes --vni <int>
```

Add/delete/list prefixes (to route other IP ranges to a given interface):
```
--addpfx <machine_name[63]> --ipv4 <ipv4> --length <int>
--delpfx <machine_name[63]> --ipv4 <ipv4> --length <int>
--listpfx <machine_name[63]>
```

Create/delete/list loadbalancers:
```
--createlb <lb_name[63]> --vni <int> --ipv4 <ipv4> --port <portspec> --protocol <protospec>
--dellb <lb_name[63]>
--getlb <lb_name[63]>
```

Add/delete/list loadbalancer backing IPs:
```
--addlbvip <lb_name[63]> --t_ipv6 <ipv6>
--dellbvip <lb_name[63]> --t_ipv6 <ipv6>
--listbackips <lb_name[63]>
```

Add/delete/list loadbalancer prefixes (call on loadbalancer targets so the public IP packets can reach them):
```
--addlbpfx <machine_name[63]> --ipv4 <ipv4> --length <int>
--dellbpfx <machine_name[63]> --ipv4 <ipv4> --length <int>
--listlbpfx <machine_name[63]>
```

Add/delete/list a virtual IP for the interface (SNAT):
```
--addvip <machine_name[63]> --ipv4 <ipv4>
--delvip <machine_name[63]>
--getvip <machine_name[63]>
```

Add/delete/list NAT IP (with port range) for the interface:
```
--addnat <machine_name[63]> --ipv4 <ipv4> --min_port <uint> --max_port <uint>
--delnat <machine_name[63]>
--getnat <machine_name[63]>
```

Add/delete/list neighbors (dp-services) with the same NAT IP:
```
--addneighnat --vni <int> [--ipv4 <ipv4>|--ipv6 <ipv6_broken>] --min_port <uint> --max_port <uint> --t_ipv6 <ipv6>
--delneighnat --vni <int> [--ipv4 <ipv4>|--ipv6 <ipv6_broken>] --min_port <uint> --max_port <uint>
--getnatinfo [local|neigh|<anything_that_kills_dp_service[9]>] [--ipv4 <ipv4>|--ipv6 <ipv6_undefined_behavior>]
```

Add/delete/list firewall rules:
```
--addfwrule <machine_name[63]> --fw_ruleid <id[63]> --priority <int> --direction [ingress|<anything[29]>] --action [accept|<anything[29]> --src_ip <ipv4> --src_length <int> --dst_ip <ipv4> --dst_length <int> (--proto [tcp|udp|icmp] --src_port_min <int> --dst_port_min <int> --scr_port_max <int> --dst_port_max <int> --icmp_code <int> --icmp_type <int>)
--delfwrule <machine_name[63]> --fw_ruleid <id[63]>
--getfwrule <machine_name[63]> --fw_ruleid <id[63]>
--listfwrules <machine_name[63]>
```
