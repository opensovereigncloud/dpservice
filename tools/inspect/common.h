// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __COMMON_H__
#define __COMMON_H__

#include <rte_hash.h>

struct rte_hash *dp_inspect_get_table(const char *name, int numa_socket);

#endif
