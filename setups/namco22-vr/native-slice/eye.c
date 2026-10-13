#include <math.h>
#include <string.h>
#include "eye.h"
#include "quad_gl.h"
#include "ss22_gl.h"
#include "fog_hw.h"

const acvr_eye *acn22_eye;
static int fixture_layers;
void acn22_invalidate_frame(void) { fixture_layers=0; acn22_eye=NULL; }

void acn22_latch_regs(const ss22_regs *r)
{
    fixture_layers = r && !r->spriteram && !r->vics && !r->spot_enabled && r->cgram && r->textram;
    if (!fixture_layers) return;
    for (int i = 0; i < 0x1e000; ++i) if (r->cgram[i]) { fixture_layers = 0; return; }
    for (int i = 0; i < 0x2000; ++i) if (r->textram[i]) { fixture_layers = 0; return; }
}

static int eye_valid(const acvr_eye *e)
{
    if (!e || e->size < sizeof *e || e->version != 1 || e->array_layer ||
        !e->rect_width || !e->rect_height || e->rect_width > 16384 ||
        e->rect_height > 16384 || e->rect_width*3 != e->rect_height*4 || e->rect_x < 0 || e->rect_y < 0 ||
        e->rect_x > 16384 || e->rect_y > 16384) return 0;
    for (int i = 0; i < 16; ++i) {
        if (!isfinite(e->view_from_scene[i]) || !isfinite(e->projection_from_view[i])) return 0;
        /* Only camera-local X/Y translation in the first native witness. */
        float v = (i % 5 == 0) ? 1.0f : 0.0f;
        if (i != 12 && i != 13 && e->view_from_scene[i] != v) return 0;
    }
    const float *p = e->projection_from_view;
    if (p[0] <= 0 || p[5] <= 0 || p[11] != -1 || p[15] != 0 ||
        p[1] || p[2] || p[3] || p[4] || p[6] || p[7] || p[12] || p[13] ||
        p[10] >= -1 || p[14] >= 0) return 0;
    return 1;
}

int acn22_project(const geo_quad *q, const acvr_eye *e, geo_quad *out)
{
    if (!q || !out || !eye_valid(e) || q->direct || q->behind ||
        q->nrv != 4 || q->ndv || q->clip[0] > 0 || q->clip[1] < 639 ||
        q->clip[2] > 0 || q->clip[3] < 479) return 0;
    geo_quad copy = *q;
    const float *p = e->projection_from_view;
    for (int i = 0; i < 4; ++i) {
        /* The captured board view is +Z forward; acvr is RH -Z forward.
         * Scale is already composed into view by the runtime. This slice
         * uses native units (provisional 1000 units/metre in the fixture). */
        double x = q->rv[i].acn_view[0] + e->view_from_scene[12];
        double y = q->rv[i].acn_view[1] + e->view_from_scene[13];
        double z = -q->rv[i].acn_view[2];
        double w = -z;
        double nx = (p[0]*x + p[8]*z) / w;
        double ny = (p[5]*y + p[9]*z) / w;
        double nz = (p[10]*z + p[14]) / w;
        /* Refuse clipping rather than drop polygons or reuse native clipping
         * for a new head pose. W must equal native depth for texcoords/fog. */
        if (!isfinite(nx) || !isfinite(ny) || !isfinite(nz) || w <= 0 ||
            w != q->rv[i].z || fabs(nx) >= 1 || fabs(ny) >= 1 || fabs(nz) >= 1) return 0;
        copy.rv[i].sx16 = (int32_t)llround((nx + 1) * 320 * 16);
        copy.rv[i].sy16 = (int32_t)llround((1 - ny) * 240 * 16);
    }
    *out = copy;
    return 1;
}

acvr_result acn22_begin_eye(const acvr_eye *e)
{
    acn22_eye = NULL;
    if (!eye_valid(e)) return ACVR_UNSUPPORTED;
    if (!fixture_layers || ss22_sprites()) return ACVR_UNSUPPORTED;
    /* Identity gamma is skipped so its full-frame scratch target cannot
     * touch the other SBS half. Nonidentity scanout needs a per-eye FBO. */
    if (g_fog.have_gamma) for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 256; ++i) if (g_fog.gamma[c][i] != i) return ACVR_UNSUPPORTED;
    int n; const geo_quad *q = acn22_prepared(&n);
    if (!n) return ACVR_BAD_STATE;
    for (int i = 0; i < n; ++i) { geo_quad copy; if (!acn22_project(q+i, e, &copy)) return ACVR_UNSUPPORTED; }
    acn22_eye = e;
    return ACVR_OK;
}
void acn22_end_eye(void) { acn22_eye = NULL; }
acvr_result acn22_stage_prepared(const acvr_eye *e, acn22_pose_fragment *out,
                               uint32_t capacity, uint32_t *written)
{
    if (!written) return ACVR_BAD_ARGUMENT;
    *written=0;
    if (!fixture_layers || ss22_sprites()) return ACVR_UNSUPPORTED;
    if (g_fog.have_gamma) for (int c=0;c<3;c++) for(int i=0;i<256;i++)
        if(g_fog.gamma[c][i]!=i)return ACVR_UNSUPPORTED;
    int n; const geo_quad *q=acn22_prepared(&n);
    if (!n) return ACVR_BAD_STATE;
    return acn22_pose_stage(q,(uint32_t)n,e,out,capacity,written);
}
void acn22_draw_quad(const geo_quad *q, const void *cfg)
{
    if (!acn22_eye) { eng_draw_quad(q, cfg); return; }
    geo_quad copy;
    if (acn22_project(q, acn22_eye, &copy)) eng_draw_quad(&copy, cfg);
}
