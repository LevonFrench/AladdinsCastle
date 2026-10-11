#ifndef ACN22_PAIRED_PROOF_SUPPORT_H
#define ACN22_PAIRED_PROOF_SUPPORT_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "geo_hw.h"
#include "quad_gl.h"
static inline unsigned acn22_fbits(float x) { uint32_t u; memcpy(&u,&x,4); return (unsigned)u; }
/* Only defined in the separate explicit proof library, never production. */
int acn22_proof_cache_configure(void);
int acn22_proof_cache_ready(void);
void acn22_proof_cache_lock(int locked);
int acn22_proof_cache_locked(void);
int acn22_proof_cache_dump(FILE *out);
int acn22_proof_prime(const geo_quad *q,int count,const eng_fog *fog);
#endif
