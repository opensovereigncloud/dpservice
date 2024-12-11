// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#include "inspect_vni.h"

#include <stdio.h>

#include "dp_error.h"
#include "dp_vni.h"

#include "inspect.h"


int dp_inspect_vni(const void *key, const void *val)
{
	const struct dp_vni_key *vni_key = key;
	const struct dp_vni_data *vni_data = val;

	printf(" vni: %3d, vni: %3d, socket: %d, rib: %p, rib6: %p, ref_count: %u\n",
		vni_key->vni,
		vni_data->vni,
		vni_data->socket_id,
		vni_data->ipv4[DP_SOCKETID(vni_data->socket_id)],
		vni_data->ipv6[DP_SOCKETID(vni_data->socket_id)],
		rte_atomic32_read(&vni_data->ref_count.refcount)
	);
	return DP_OK;
}
