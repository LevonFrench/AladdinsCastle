/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ACN22_NATIVE_ADAPTER_H
#define ACN22_NATIVE_ADAPTER_H
#include "acvr.h"
/* Separate opt-in synthetic factory; never the published production query. */
acvr_result ACVR_CALL acn22_native_adapter_query(uint32_t,acvr_backend_api *);
#endif
