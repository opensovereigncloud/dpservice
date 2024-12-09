// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_H__
#define __INSPECT_H__

int dp_inspect_table(const char *name, int numa_socket, int (*dumpfunc)(const void *key, const void *val));

#endif
