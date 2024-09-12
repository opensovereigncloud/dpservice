# TESTPMD

## Init
```shell
dpdk-testpmd -l 3,5 -a 0000:98:00.0,class=rxq_cqe_comp_en=0,dv_flow_en=2,dv_esw_en=1,fdb_def_rule_en=1,representor=pf[0-1]vf[124-125] -- -i --flow-isolate-all
```

## Init flow config
```shell
port stop all
flow configure 0 queues_number 4 queues_size 64 counters_number 0 aging_counters_number 0 meters_number 0 flags 0
flow configure 1 queues_number 4 queues_size 64 counters_number 0 aging_counters_number 0 meters_number 0 flags 0
flow configure 2 queues_number 4 queues_size 64 counters_number 0 aging_counters_number 0 meters_number 0 flags 0
flow configure 3 queues_number 4 queues_size 64 counters_number 0 aging_counters_number 0 meters_number 0 flags 0
port start all
```

## PF1 to VF Flow
```shell
flow pattern_template 0 create transfer relaxed no pattern_template_id 10 template represented_port ethdev_port_id is 1 / eth / end

flow actions_template 0 create transfer actions_template_id 10 template represented_port / end mask represented_port / end
flow template_table 0 create group 0 priority 0 transfer table_id 5 rules_number 8 pattern_template 10 actions_template 10

flow queue 0 create 0 template_table 5 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 1 / eth / end actions represented_port ethdev_port_id 3 / end
flow queue 0 create 0 template_table 5 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 3 / eth / end actions represented_port ethdev_port_id 1 / end
flow push 0 queue 0
```

## PF1 to VF Flow with jump
```shell
flow pattern_template 0 create transfer relaxed no pattern_template_id 10 template eth / end
flow actions_template 0 create transfer actions_template_id 10 template jump / end mask jump / end
flow template_table 0 create group 0 priority 0 transfer table_id 5 rules_number 8 pattern_template 10 actions_template 10
flow queue 0 create 0 template_table 5 pattern_template 0 actions_template 0 postpone no pattern eth / end actions jump group 1 / end
flow push 0 queue 0

flow pattern_template 0 create transfer relaxed no pattern_template_id 11 template represented_port ethdev_port_id is 1 / eth / end
flow actions_template 0 create transfer actions_template_id 11 template represented_port / end mask represented_port / end
flow template_table 0 create group 1 priority 0 transfer table_id 6 rules_number 8 pattern_template 11 actions_template 11
flow queue 0 create 0 template_table 6 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 1 / eth / end actions represented_port ethdev_port_id 3 / end
flow queue 0 create 0 template_table 6 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 3 / eth / end actions represented_port ethdev_port_id 1 / end
flow push 0 queue 0
```

flow pattern_template 0 create transfer relaxed no pattern_template_id 11 template represented_port ethdev_port_id is 1 / eth / ipv6 / end
flow actions_template 0 create transfer actions_template_id 11 template drop / end mask drop / end
flow template_table 0 create group 0 priority 0 transfer table_id 6 rules_number 8 pattern_template 11 actions_template 11
flow queue 0 create 0 template_table 6 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 1 / eth type is 0x86dd / ipv6  proto is 0x003a  / end actions drop / end
flow push 0 queue 0


flow pattern_template 0 create transfer relaxed no pattern_template_id 10 template represented_port ethdev_port_id is 1 / eth / end
flow actions_template 0 create transfer actions_template_id 10 template represented_port / end mask represented_port / end
flow template_table 0 create group 0 priority 0 transfer table_id 5 rules_number 8 pattern_template 10 actions_template 10
flow queue 0 create 0 template_table 5 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 1 / eth / end actions represented_port ethdev_port_id 3 / end
flow queue 0 create 0 template_table 5 pattern_template 0 actions_template 0 postpone no pattern represented_port ethdev_port_id is 3 / eth / end actions represented_port ethdev_port_id 1 / end
flow push 0 queue 0
