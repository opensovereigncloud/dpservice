// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_VNF_H__
#define __INSPECT_VNF_H__

#define VNF_TABLE_NAME "vnf_table"
#define VNF_REV_TABLE_NAME "reverse_vnf_table"

int dp_inspect_vnf(const void *key, const void *val);

int dp_inspect_vnf_rev(const void *key, const void *val);

#endif
