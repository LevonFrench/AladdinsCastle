#ifndef ACN22_POSE_STAGE_H
#define ACN22_POSE_STAGE_H
#include "acvr.h"
#include "geo_hw.h"
#define ACN22_POSE_MAX_QUADS 1024u
#define ACN22_POSE_MAX_VERTS 9u

typedef struct {
    double clip[4];
    double uv[2], brightness, board_z;
} acn22_pose_vertex;
typedef struct {
    const geo_quad *native; /* borrowed immutable queue; same lifetime as input */
    uint32_t source_index, fan_index, count;
    int32_t painter_key;
    int emission_order;
    acn22_pose_vertex v[ACN22_POSE_MAX_VERTS];
} acn22_pose_fragment;
typedef struct {
    double uv[2], brightness, board_z, ndc_z;
} acn22_pose_sample;

/* No GL, allocation, emulation step or sort. Count resets to0 on failure;
 * output bytes stay untouched. Input, output and count must not overlap.
 * Two validation passes require the source and eye to remain immutable. */
acvr_result acn22_pose_stage(const geo_quad *source, uint32_t count,
                           const acvr_eye *eye, acn22_pose_fragment *output,
                           uint32_t capacity, uint32_t *written);
/* Pack native-ortho homogeneous position and pre-bake texture coordinate.
 * This is transport, not a new GPU path or texture/fog rasterizer. */
acvr_result acn22_pose_pack(const acn22_pose_vertex *v, double position[4], double texture[4]);
/* Barycentric raster sample of a fan triangle, for CPU attribute checks. */
acvr_result acn22_pose_interpolate(const acn22_pose_vertex triangle[3],
                                 const double lambda[3], acn22_pose_sample *sample);
#endif
