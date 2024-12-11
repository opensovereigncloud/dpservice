// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_LB_H__
#define __INSPECT_LB_H__

#define LB_TABLE_NAME "loadbalancer_table"
#define LB_ID_TABLE_NAME "loadbalancer_id_table"

int dp_inspect_lb(const void *key, const void *val);

int dp_inspect_lb_id(const void *key, const void *val);

#endif
