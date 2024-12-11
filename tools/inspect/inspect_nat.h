// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_NAT_H__
#define __INSPECT_NAT_H__

// TODO (global problem) define in dpservice
#define DNAT_TABLE_NAME "dnat_table"
#define SNAT_TABLE_NAME "snat_table"
#define PORTMAP_TABLE_NAME "nat_portmap_table"
#define PORTOVERLOAD_TABLE_NAME "nat_portoverload_table"

int dp_inspect_dnat(const void *key, const void *val);

int dp_inspect_snat(const void *key, const void *val);

int dp_inspect_portmap(const void *key, const void *val);

int dp_inspect_portoverload(const void *key, const void *val);

#endif
