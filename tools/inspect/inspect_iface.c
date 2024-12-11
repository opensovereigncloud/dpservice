// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_iface.h"

#include <stdio.h>

#include "dp_error.h"
#include "dp_port.h"


static int dp_inspect_iface(const void *key, const void *val)
{
	const char *iface_id = key;
	const struct dp_port *iface_port = val;

	printf(" id: %.*s, %p (private memory)\n", DP_IFACE_ID_MAX_LEN, iface_id, iface_port);
	return DP_OK;
}


const struct dp_inspect_spec dp_inspect_iface_spec = {
	.table_name = "interface_table",
	.dump_func = dp_inspect_iface,
};
