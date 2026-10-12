// SPDX-License-Identifier: GPL-3.0-only
#ifndef N22_API_TABLE_H
#define N22_API_TABLE_H
#include "acvr.h"
#include <string.h>
#ifdef ACVR_BACKEND_API_V1_SIZE
#define N22_API_TABLE_OLD_SIZE ACVR_BACKEND_API_V1_SIZE
#else
#define N22_API_TABLE_OLD_SIZE ((uint32_t)sizeof(acvr_backend_api))
#endif
static inline acvr_result n22_api_table_valid(const acvr_backend_api *out)
{
    if (!out) return ACVR_BAD_ARGUMENT;
    if (out->version != ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if (out->size != N22_API_TABLE_OLD_SIZE && out->size < sizeof(*out))
        return ACVR_BAD_ARGUMENT;
    return ACVR_OK;
}
static inline void n22_api_table_payload(acvr_backend_api *out,
                                        const acvr_backend_api *value)
{
    size_t covered = out->size < sizeof(*out) ? out->size : sizeof(*out);
    /* Validation precedes this call. Keep caller prefix and unknown tail. */
    memcpy((unsigned char *)out + 8, (const unsigned char *)value + 8, covered - 8);
}
#endif
