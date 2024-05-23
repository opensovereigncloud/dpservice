## dpservice-cli get vni

Get vni usage information

```
dpservice-cli get vni <--vni> <--vni-type> [flags]
```

### Examples

```
dpservice-cli get vni --vni=vm1 --vni-type=0
```

### Options

```
  -h, --help             help for vni
      --vni uint32       VNI to check.
      --vni-type uint8   VNI Type: VniIpv4 = 0/VniIpv6 = 1.
```

### Options inherited from parent commands

```
      --address string             dpservice address. (default "localhost:1337")
      --connect-timeout duration   Timeout to connect to the dpservice. (default 4s)
  -o, --output string              Output format. [json|yaml|table|name] (default "table")
      --pretty                     Whether to render pretty output.
  -w, --wide                       Whether to render more info in table output.
```

### SEE ALSO

* [dpservice-cli get](dpservice-cli_get.md)	 - Gets one of [interface virtualip loadbalancer nat firewallrule vni version init]

###### Auto generated by spf13/cobra on 25-Sep-2023