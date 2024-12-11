// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_H__
#define __INSPECT_H__

struct dp_inspect_spec {
	const char *table_name;
	int (*dump_func)(const void *key, const void *val);
};

enum dp_inspect_mode {
	DP_INSPECT_COUNT,
	DP_INSPECT_DUMP,
};

int dp_inspect(const struct dp_inspect_spec *spec, int numa_socket, enum dp_inspect_mode mode);

#endif
