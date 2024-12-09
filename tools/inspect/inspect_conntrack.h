// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_CONNTRACK_H__
#define __INSPECT_CONNTRACK_H__

#define CONNTRACK_TABLE_NAME "conntrack_table"

int dp_inspect_conntrack(const void *key, const void *val);

#endif
