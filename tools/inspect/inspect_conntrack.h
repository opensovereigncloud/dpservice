// SPDX-FileCopyrightText: 2024 SAP SE or an SAP affiliate company and IronCore contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef __INSPECT_CONNTRACK_H__
#define __INSPECT_CONNTRACK_H__

#include "inspect.h"

int dp_inspect_init_conntrack(struct dp_inspect_spec *out_spec, enum dp_inspect_output_format format);

#endif
