"""Pinned quad_gl varying/batch edits; runtime entry remains proof-gated OFF.

Attribution: native anchors are from MIT namco22-decompile a9f1a0a1f9de.
Texture bake, colour/fade, painter order and legacy batch arrays stay upstream.
"""
def apply(edit, contents):
    name = 'engine/quad_gl.c'
    edit(name, '#include "quad_gl.h"', '#include "quad_gl.h"\n#include "native_batch.h"')
    edit(name, 'static void (APIENTRY *p_active_texture)(GLenum);',
         'static void (APIENTRY *p_active_texture)(GLenum);\nstatic void (APIENTRY *p_client_texture)(GLenum);')
    edit(name, '    p_active_texture = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glActiveTexture");',
         '    p_active_texture = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glActiveTexture");\n    p_client_texture = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glClientActiveTexture");')
    edit(name, 'static float *bb_xyz;',
         'static float bb_pose[BB_MAXV * 4], bb_board[BB_MAXV * 4];\nstatic GLuint pose_current_fog;\nstatic float *bb_xyz;')
    edit(name, 'float frgb[3]; int depth; } bb_run;',
         'float frgb[3]; int depth, pose; GLuint pose_fog; } bb_run;')
    edit(name, 'static void bb_flush(void)\n{', POSE_APPLY+'\nstatic void bb_flush(void)\n{')
    edit(name, '    if (bb_run.depth) {\n        glVertexPointer(3, GL_FLOAT, 0, bb_xyz);',
         '''    if (bb_run.pose) {
        if (bb_run.pose_fog) {
            p_client_texture(GL_TEXTURE1); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(4, GL_FLOAT, 0, bb_board); p_client_texture(GL_TEXTURE0);
        }
        glVertexPointer(4, GL_FLOAT, 0, bb_pose);
        glDrawArrays(GL_TRIANGLES, 0, bb_n);
        glVertexPointer(2, GL_FLOAT, 0, bb_xy);
        if (bb_run.pose_fog) {
            p_client_texture(GL_TEXTURE1); glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            p_client_texture(GL_TEXTURE0);
        }
    } else if (bb_run.depth) {
        glVertexPointer(3, GL_FLOAT, 0, bb_xyz);''')
    edit(name, '    qs_tex_on(st->tex_on);',
         '    if (!st->pose) pose_fog_apply(0, st->frgb);\n    qs_tex_on(st->tex_on);')
    edit(name, '        if (mt_state == 1) qs_fog(st->fog, st->frgb, st->env);\n        else qs_env(st->env);',
         '''        if (st->pose) {
            qs_fog(0, st->frgb, st->env); pose_fog_apply(st->pose_fog, st->frgb);
        } else if (mt_state == 1) qs_fog(st->fog, st->frgb, st->env);
        else qs_env(st->env);''')
    edit(name, '    struct bbstate st_buf = *st_;',
         '''    if (acn22_native_capture_active()) {
        acn22_native_capture_fan((uint32_t)n, qa_xy, qa_rgba, qa_st); return;
    }
    if (acn22_native_active()) {
        if (n != (int)acn22_native_active()->count) { acn22_native_fail(); return; }
        for (int i=0;i<n;i++) {
            acn22_native_vertex v;
            if (acn22_native_pack(acn22_native_active(), (uint32_t)i, qa_xy+i*2,
                    qa_rgba+i*4, qa_st+i*4, &v)!=ACVR_OK) { acn22_native_fail(); return; }
        }
    }
    struct bbstate st_buf = *st_;
    st_buf.pose = acn22_native_active()!=NULL;
    st_buf.pose_fog = st_buf.pose ? pose_current_fog : 0;''')
    edit(name, '    if (st_buf.tex_on && !st_buf.fog && bb_valid && bb_run.tex_on && bb_run.fog && st_buf.tex == bb_run.tex) {',
         '    if (!st_buf.pose && !bb_run.pose && st_buf.tex_on && !st_buf.fog && bb_valid && bb_run.tex_on && bb_run.fog && st_buf.tex == bb_run.tex) {')
    edit(name, '            int s = k == 0 ? 0 : i + k - 1;',
         '            int s = acn22_native_fan_index(i, k);')
    edit(name, '            if (st->depth) { bb_xyz[bb_n*3+0]',
         '''            if (st->pose) {
                acn22_native_vertex v;
                if (acn22_native_pack(acn22_native_active(), (uint32_t)s, qa_xy+s*2,
                        qa_rgba+s*4, qa_st+s*4, &v)!=ACVR_OK) { acn22_native_fail(); return; }
                memcpy(bb_pose+bb_n*4, v.position, sizeof v.position);
                memcpy(bb_board+bb_n*4, v.board, sizeof v.board);
            } else if (st->depth) { bb_xyz[bb_n*3+0]''')
    edit(name, '    glDisableClientState(GL_VERTEX_ARRAY);',
         '    pose_fog_apply(0, qs.frgb);\n    glDisableClientState(GL_VERTEX_ARRAY);')
    edit(name, 'static void draw_quad_one(const geo_quad *q, const eng_draw_cfg *cfg)\n{',
         POSE_TEXTURE+'\nstatic void draw_quad_one(const geo_quad *q, const eng_draw_cfg *cfg)\n{\n    pose_current_fog = 0;')
    edit(name, '    g_perf_bake += eng_now() - _tb;',
         '    g_perf_bake += eng_now() - _tb;\n    if (acn22_native_active() && !tex) { acn22_native_fail(); return; }')
    edit(name, '    geo_sv sv[8], cv[32];', '    geo_sv sv[10], cv[32];')
    edit(name, '    for (int i = 0; i < q->nrv && nsv < 8; i++) {',
         '    for (int i = 0; i < q->nrv && nsv < (acn22_native_active() ? 10 : 8); i++) {')
    edit(name, '        float iw = (q->rv[i].z > 0) ? 1.0f / (float)q->rv[i].z : 1.0f;',
         '''        if (acn22_native_active()) {
            float t[4];
            if (acn22_native_uv(acn22_native_active(), (uint32_t)i,
                    bmin_u,bmin_v,brange_u,brange_v,bsu,bsv,cu,cv_,0,0,t)!=ACVR_OK) {
                acn22_native_fail(); return;
            }
            tu=t[0]; tv=t[1];
        }
        float iw = acn22_native_active() ? 1.0f : ((q->rv[i].z > 0) ? 1.0f / (float)q->rv[i].z : 1.0f);''')
    edit(name, '            sh = (float)q->rv[i].bri / 64.0f;',
         '            sh = acn22_native_active() ? (float)acn22_native_active()->v[i].brightness / 64.0f : (float)q->rv[i].bri / 64.0f;')
    edit(name, '    int ncv = clip_to_screen(sv, nsv, cv);',
         '    int ncv;\n    if (acn22_native_active()) { ncv=nsv; memcpy(cv,sv,sizeof *cv*nsv); }\n    else ncv = clip_to_screen(sv, nsv, cv);')
    edit(name, '    if (!solid && base_alpha && mt_state == 1 && cfg->fog && cfg->fog_quad &&',
         '    if (!acn22_native_active() && !solid && base_alpha && mt_state == 1 && cfg->fog && cfg->fog_quad &&')
    edit(name, '    if (solid) {\n        /* SOLID through',
         '''    if (acn22_native_active() && cfg->fog && cfg->fog_quad && !(q->cz_adjust & 0x800000)) {
        eng_fog f; memset(&f,0,sizeof f); f.alpha_const=-1;
        if (cfg->fog_quad(q,&f)) {
            pose_current_fog=pose_fog_texture(&f);
            if (!pose_current_fog) { acn22_native_fail(); return; }
            for (int c=0;c<3;c++) fogrgb[c]=f.rgb[c]/255.0f;
            if (cfg->fade_rgb) cfg->fade_rgb(fogrgb,fogrgb+1,fogrgb+2);
        }
    }
    if (solid) {
        /* SOLID through''')
    edit(name, '    if (fogm == 0 && cfg->fog && cfg->fog_quad) {',
         '    if (!acn22_native_active() && fogm == 0 && cfg->fog && cfg->fog_quad) {')
    contents[name] += '\n'+CPU_CAPTURE

POSE_APPLY = r'''
/* ACN22 proof-only CZ sampler. Unit1 texture matrix is scoped and restored. */
static int pose_fog_on;
static void pose_fog_apply(GLuint texture,const float rgb[3])
{
    if (!texture && !pose_fog_on) return;
    GLint mode; glGetIntegerv(GL_MATRIX_MODE,&mode);
    p_active_texture(GL_TEXTURE1);
    if (!texture) {
        glDisable(GL_TEXTURE_1D); glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(mode);
        pose_fog_on=0; p_active_texture(GL_TEXTURE0); return;
    }
    if (!pose_fog_on) { glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(mode); }
    glDisable(GL_TEXTURE_2D); glEnable(GL_TEXTURE_1D); glBindTexture(GL_TEXTURE_1D,texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_INTERPOLATE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_PREVIOUS); glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_CONSTANT); glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_RGB,GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND2_RGB,GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE); glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_PREVIOUS);
    glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE,1);
    float colour[4]={rgb[0],rgb[1],rgb[2],1}; glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,colour);
    pose_fog_on=1; p_active_texture(GL_TEXTURE0);
}
'''
POSE_TEXTURE = r'''
static GLuint pose_fog_texture(const eng_fog *fog)
{
    static GLuint texture; static int have; static uint8_t previous[ACN22_CZ_ENTRIES];
    uint8_t bytes[ACN22_CZ_ENTRIES];
    if (!p_active_texture || !p_client_texture || mt_state!=1) return 0;
    GLint size=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size);
    if (size<(GLint)ACN22_CZ_ENTRIES || acn22_native_cz_bytes(fog,bytes)!=ACVR_OK) return 0;
    if (!have || memcmp(previous,bytes,sizeof bytes)) {
        bb_flush(); /* queued fragments may still reference this table */
        if (!texture) glGenTextures(1,&texture);
        if (!texture) return 0;
        p_active_texture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_1D,texture);
        glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        GLint alignment,row,skip_pixel,skip_row;
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixel); glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_row);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        glTexImage1D(GL_TEXTURE_1D,0,GL_ALPHA8,ACN22_CZ_ENTRIES,0,GL_ALPHA,GL_UNSIGNED_BYTE,bytes);
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,row);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixel); glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_row);
        p_active_texture(GL_TEXTURE0);
        if (glGetError()!=GL_NO_ERROR) { have=0; return 0; }
        memcpy(previous,bytes,sizeof bytes); have=1;
    }
    return texture;
}
'''
CPU_CAPTURE = r'''
acvr_result acn22_upstream_batch_capture(const geo_quad *q,const acn22_pose_fragment *fragment,
        const float (*rgba)[4],const float (*st)[4],acn22_native_vertex *out,
        uint32_t capacity,uint32_t *written)
{
    if (!written) return ACVR_BAD_ARGUMENT;
    *written=0;
    if (!q || !rgba || !st || !out || bb_n || bb_valid) return ACVR_BAD_ARGUMENT;
    geo_quad copy; int n=q->nrv;
    if (fragment) {
        acvr_result r=acn22_native_prepare(fragment,&copy); if (r!=ACVR_OK) return r;
        if (fragment->native!=q) return ACVR_BAD_ARGUMENT;
        n=copy.nrv;
    }
    if (n<3 || n>9 || (uint32_t)(n-2)*3>capacity) return ACVR_BAD_ARGUMENT;
    /* Preflight packet contents before touching output or renderer binding. */
    for (int i=0;i<n;i++) {
        const geo_quad *src=fragment?&copy:q;
        float xy[2]={src->rv[i].sx16/16.0f,src->rv[i].sy16/16.0f}; acn22_native_vertex v;
        acvr_result r=acn22_native_pack(fragment,(uint32_t)i,xy,rgba[i],st[i],&v);
        if (r!=ACVR_OK) return r;
    }
    acvr_result r=acn22_native_bind(fragment,out,capacity,written); if (r!=ACVR_OK) return r;
    for (int i=0;i<n;i++) {
        const geo_quad *src=fragment?&copy:q;
        qa_xy[i*2]=src->rv[i].sx16/16.0f; qa_xy[i*2+1]=src->rv[i].sy16/16.0f;
        memcpy(qa_rgba+i*4,rgba[i],sizeof *rgba); memcpy(qa_st+i*4,st[i],sizeof *st);
    }
    bb_emit(n,&(struct bbstate){0}); /* CPU capture branch returns before any GL */
    return acn22_native_unbind();
}
'''
