#ifndef ACN22_NATIVE_BATCH_H
#define ACN22_NATIVE_BATCH_H
#include "pose_stage.h"
#include "quad_gl.h"
#define ACN22_CZ_ENTRIES 8192u
#define ACN22_CZ_DIVISOR 2097152.0
typedef struct {
    float position[4], colour[4], primary[4], board[4];
} acn22_native_vertex;

/* Internal owner-thread API: all input/output/count storage is distinct and
 * nonoverlapping. No arbitrary external caller or frame lease is admitted. */
acvr_result acn22_native_prepare(const acn22_pose_fragment *fragment, geo_quad *copy);
acvr_result acn22_native_pack(const acn22_pose_fragment *fragment, uint32_t vertex,
                            const float xy[2], const float rgba[4], const float st[4],
                            acn22_native_vertex *out);
acvr_result acn22_native_uv(const acn22_pose_fragment *fragment,uint32_t vertex,
                          int base_u,int base_v,int range_u,int range_v,
                          float scale_u,float scale_v,float centre_u,float centre_v,
                          float atlas_u,float atlas_v,float out[4]);
acvr_result acn22_native_cz_bytes(const eng_fog *fog,uint8_t out[ACN22_CZ_ENTRIES]);

/* CPU seam in actual patched quad_gl bb_emit: no GL call/context is created. */
acvr_result acn22_upstream_batch_capture(const geo_quad *native,
                           const acn22_pose_fragment *fragment,
                           const float (*rgba)[4],const float (*st)[4],
                           acn22_native_vertex *out,uint32_t capacity,uint32_t *written);
/* Separate proof-only entry. Default compile gate OFF; legacy fixture and
 * begin_eye never call it. Caller must establish the native GL pass/ortho,
 * stage the whole queue and abandon the target on any failure. */
acvr_result acn22_native_draw_fragment(const acn22_pose_fragment *fragment,const eng_draw_cfg *cfg);

/* Renderer-private serial binding/capture helpers. No emulation changes. */
const acn22_pose_fragment *acn22_native_active(void);
acvr_result acn22_native_bind(const acn22_pose_fragment *fragment,
                            acn22_native_vertex *capture,uint32_t capacity,uint32_t *written);
int acn22_native_capture_active(void);
void acn22_native_capture_fan(uint32_t count,const float *xy,const float *rgba,const float *st);
int acn22_native_fan_index(int triangle,int corner);
void acn22_native_fail(void);
acvr_result acn22_native_unbind(void);
#endif
