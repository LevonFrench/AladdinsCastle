"""Narrow attributed proof-only hooks in already-approved MIT native units."""


def apply(edit, contents):
    edit('engine/tex_bake.c', '#include "tex_bake.h"',
         '#include "tex_bake.h"\n#include "paired_proof_support.h"')
    edit('engine/tex_bake.c', '    if (!budget_env_done) budget_env();',
         '''#if ACN22_NATIVE_GPU_PROOF
    if (acn22_proof_cache_locked()) {
        if (!acn22_proof_cache_ready()) return 0;
        return bake_impl(min_u,min_v,range_u,range_v,texbank,pal_group,cmode,
                         out_su,out_sv,out_ou,out_ov,0,1); /* lookup ONLY */
    }
#endif
    if (!budget_env_done) budget_env();''')
    contents['engine/tex_bake.c'] += CACHE
    edit('engine/quad_gl.c', '#include "native_batch.h"',
         '#include "native_batch.h"\n#include "paired_proof_support.h"')
    contents['engine/quad_gl.c'] += PRIME


CACHE = r'''
#if ACN22_NATIVE_GPU_PROOF
/* ACN22 private own-context proof: fixed small atlas, no deferred/placeholder path. */
static int acn22_cache_lock;
int acn22_proof_cache_locked(void) { return acn22_cache_lock; }
void acn22_proof_cache_lock(int locked) { acn22_cache_lock=locked; }
int acn22_proof_cache_configure(void)
{
    if (ap_n || tex_cache_total || gen_n || tex_free_n) return 0;
    if (getenv("ENG_ATLAS_PREALLOC")) return 0; /* do not inherit hidden page policy */
    atlas_dim=1024; atlas_maxp=1; g_tex_atlas=1;
    g_tex_opaque=1; g_tex_fixedcap=1; g_tex_pow2sample=0; g_tex_pow2alloc=0;
    g_tex_fifo=0; g_tex_orphan=0;
    tex_cache_budget=8u*1024u*1024u;
    budget_env_done=1; g_budget_draw=g_budget_pump=0; pend_n=0; in_pump=0;
    renderer_texture_init();
    return ap_n==1 && apages[0].tex && apages[0].shadow && glGetError()==GL_NO_ERROR;
}
int acn22_proof_cache_ready(void)
{
    if (atlas_dim!=1024 || atlas_maxp!=1 || g_tex_atlas!=1 || ap_n!=1 ||
        !apages[0].tex || !apages[0].shadow || apages[0].ndr ||
        g_budget_draw || g_budget_pump || pend_n || in_pump || !budget_env_done ||
        g_tex_opaque!=1 || g_tex_fixedcap!=1 || g_tex_pow2sample || g_tex_pow2alloc ||
        g_tex_fifo || g_tex_orphan || tex_cache_evictions || g_tex_page_resets ||
        !direct_palette_loaded || tex_cache_total<1 || tex_cache_total>3) return 0;
    int occupied=0;
    for (int i=0;i<TEX_CACHE_SIZE;i++) if (tex_cache[i].occupied) {
        TexCacheEntry *e=&tex_cache[i]; occupied++;
        if (!e->gl_texture || e->pal_group>=128 || e->pal_gen!=g_eng_pal_gen[e->pal_group] ||
            e->cap!=TEX_BAKE_MAX || e->a_page!=0 || e->gl_texture!=apages[0].tex ||
            !e->alloc_w || !e->alloc_h || !e->used_w || !e->used_h) return 0;
    }
    return occupied==tex_cache_total;
}
int acn22_proof_cache_dump(FILE *out)
{
    if (!out || !acn22_proof_cache_ready()) return 0;
    fprintf(out,"{\"locked\":%d,\"atlas_dim\":%d,\"pages\":%d,\"page_texture\":%u,"
        "\"budget_draw\":%ld,\"budget_pump\":%ld,\"pending\":%d,\"in_pump\":%d,"
        "\"misses\":%d,\"evictions\":%d,\"page_resets\":%ld,\"reallocs\":%d,"
        "\"subimages\":%d,\"cache_bytes\":%zu,\"palette_generation\":[",
        acn22_cache_lock,atlas_dim,ap_n,(unsigned)apages[0].tex,g_budget_draw,g_budget_pump,
        pend_n,in_pump,tex_frame_misses,tex_cache_evictions,g_tex_page_resets,tex_reallocs,
        tex_subimages,tex_cache_bytes);
    for (int i=0;i<128;i++) fprintf(out,"%s%u",i?",":"",(unsigned)g_eng_pal_gen[i]);
    fprintf(out,"],\"entries\":["); int n=0;
    for (int i=0;i<TEX_CACHE_SIZE;i++) if (tex_cache[i].occupied) {
        TexCacheEntry *e=&tex_cache[i];
        fprintf(out,"%s{\"slot\":%d,\"texture\":%u,\"pal_group\":%u,\"pal_gen\":%u,"
          "\"min_u\":%u,\"min_v\":%u,\"range_u\":%u,\"range_v\":%u,\"texbank\":%u,"
          "\"cmode\":%u,\"cap\":%u,\"alloc_w\":%u,\"alloc_h\":%u,\"used_w\":%u,"
          "\"used_h\":%u,\"a_page\":%u,\"a_x\":%u,\"a_y\":%u,\"su_f32\":\"%08x\","
          "\"sv_f32\":\"%08x\",\"ou_f32\":\"%08x\",\"ov_f32\":\"%08x\"}",
          n++?",":"",i,(unsigned)e->gl_texture,(unsigned)e->pal_group,(unsigned)e->pal_gen,
          (unsigned)e->min_u,(unsigned)e->min_v,(unsigned)e->range_u,(unsigned)e->range_v,
          (unsigned)e->texbank,(unsigned)e->cmode,(unsigned)e->cap,(unsigned)e->alloc_w,
          (unsigned)e->alloc_h,(unsigned)e->used_w,(unsigned)e->used_h,(unsigned)e->a_page,
          (unsigned)e->a_x,(unsigned)e->a_y,acn22_fbits(e->su),acn22_fbits(e->sv),
          acn22_fbits(e->ou),acn22_fbits(e->ov));
    }
    fprintf(out,"]}\n"); return !ferror(out);
}
#endif
'''

PRIME = r'''
#if ACN22_NATIVE_GPU_PROOF
/* No geometry emission: preflight native texture cache and CZ upload, then lock. */
int acn22_proof_prime(const geo_quad *q,int count,const eng_fog *fog)
{
    if (!q || count<1 || count>2 || bb_n || bb_valid) return 0;
    acn22_proof_cache_lock(0); eng_draw_begin();
    int ok=1;
    for (int i=0;i<count && ok;i++) {
        float su,sv,ou,ov;
        GLuint tex=bake_quad_texture(q[i].uvbox[0],q[i].uvbox[2],
            q[i].uvbox[1]-q[i].uvbox[0]+1,q[i].uvbox[3]-q[i].uvbox[2]+1,
            q[i].texbank,(q[i].color>>8)&127,q[i].cmode,&su,&sv,&ou,&ov);
        if (!tex || !isfinite(su) || !isfinite(sv) || su<=0 || sv<=0) ok=0;
        else tex_bake_commit(tex);
    }
    if (ok && fog) {
        GLuint texture=pose_fog_texture(fog);
        uint8_t expected[ACN22_CZ_ENTRIES],actual[ACN22_CZ_ENTRIES]={0};
        if (!texture || acn22_native_cz_bytes(fog,expected)!=ACVR_OK) ok=0;
        else {
            GLint pack=4,width=0; glGetIntegerv(GL_PACK_ALIGNMENT,&pack);
            p_active_texture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_1D,texture);
            glGetTexLevelParameteriv(GL_TEXTURE_1D,0,GL_TEXTURE_WIDTH,&width);
            glPixelStorei(GL_PACK_ALIGNMENT,1);
            glGetTexImage(GL_TEXTURE_1D,0,GL_ALPHA,GL_UNSIGNED_BYTE,actual);
            glPixelStorei(GL_PACK_ALIGNMENT,pack); p_active_texture(GL_TEXTURE0);
            if (width!=(GLint)ACN22_CZ_ENTRIES || memcmp(actual,expected,sizeof actual)) ok=0;
        }
    }
    eng_draw_end();
    if (glGetError()!=GL_NO_ERROR || !acn22_proof_cache_ready()) ok=0;
    acn22_proof_cache_lock(ok); return ok;
}
#endif
'''
