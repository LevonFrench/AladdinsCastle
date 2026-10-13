/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ACN22_NATIVE_ADAPTER_SUPPORT_H
#define ACN22_NATIVE_ADAPTER_SUPPORT_H
void *acn22_adapter_proc(const char *name);
int acn22_adapter_quad_idle(void);
int acn22_adapter_quad_shutdown(void);
int acn22_adapter_cache_shutdown(void);
int acn22_adapter_cache_fresh(void);
unsigned long long acn22_adapter_cache_fingerprint(void);
#endif
