"""Adapter-only hooks in approved units; default/proof paths retain original code."""
import re
from native_patch import POSE_TEXTURE

def apply(edit, contents):
    name='engine/quad_gl.c'
    edit(name,'#include "paired_proof_support.h"',
        '#include "paired_proof_support.h"\n#if ACN22_NATIVE_ADAPTER\n#include "native_adapter_support.h"\n#define ACN22_NATIVE_PROC(n) acn22_adapter_proc(n)\n#else\n#define ACN22_NATIVE_PROC(n) SDL_GL_GetProcAddress(n)\n#endif')
    edit(name,'SDL_GL_GetProcAddress("glActiveTexture")','ACN22_NATIVE_PROC("glActiveTexture")')
    edit(name,'SDL_GL_GetProcAddress("glClientActiveTexture")','ACN22_NATIVE_PROC("glClientActiveTexture")')
    adapted=POSE_TEXTURE.replace('    static GLuint texture; static int have; static uint8_t previous[ACN22_CZ_ENTRIES];\n','')
    for old,new in (('texture','acn22_context_fog_tex'),('have','acn22_context_fog_have'),('previous','acn22_context_fog_previous')):
        adapted=re.sub(r'\b'+old+r'\b',new,adapted)
    edit(name,POSE_TEXTURE,'\n#if ACN22_NATIVE_ADAPTER\nstatic GLuint acn22_context_fog_tex;\nstatic int acn22_context_fog_have;\nstatic uint8_t acn22_context_fog_previous[ACN22_CZ_ENTRIES];\n'+adapted+'\n#else\n'+POSE_TEXTURE+'\n#endif\n')
    edit(name,'void eng_draw_begin(void)\n{\n    static int hook_once;',
        '#if ACN22_NATIVE_ADAPTER\nstatic int acn22_context_hook_once;\n#endif\nvoid eng_draw_begin(void)\n{\n#if ACN22_NATIVE_ADAPTER\n#define hook_once acn22_context_hook_once\n#else\n    static int hook_once;\n#endif')
    edit(name,'    eng_draw_resume();\n}\nvoid eng_draw_resume(void)',
        '    eng_draw_resume();\n#if ACN22_NATIVE_ADAPTER\n#undef hook_once\n#endif\n}\nvoid eng_draw_resume(void)')
    contents[name]+=QUAD_CLOSE
    edit('engine/tex_bake.c','#include "paired_proof_support.h"',
        '#include "paired_proof_support.h"\n#if ACN22_NATIVE_ADAPTER\n#include "native_adapter_support.h"\n#endif')
    contents['engine/tex_bake.c']+=CACHE_CLOSE

QUAD_CLOSE=r'''
#if ACN22_NATIVE_ADAPTER
int acn22_adapter_quad_idle(void) { return !bb_n && !bb_valid && !acn22_native_active(); }
int acn22_adapter_quad_shutdown(void)
{
    if (!acn22_adapter_quad_idle()) return 0;
    if (acn22_context_fog_tex) glDeleteTextures(1,&acn22_context_fog_tex);
    acn22_context_fog_tex=0; acn22_context_fog_have=0;
    memset(acn22_context_fog_previous,0,sizeof acn22_context_fog_previous);
    free(bb_xyz); bb_xyz=NULL;
    pose_current_fog=0; pose_fog_on=0; depth_applied=0;
    qs.valid=0; mt_state=-1; p_active_texture=NULL; p_client_texture=NULL;
    acn22_context_hook_once=0; tex_bake_set_flush_hook(NULL);
    return glGetError()==GL_NO_ERROR;
}
#endif
'''
CACHE_CLOSE=r'''
#if ACN22_NATIVE_ADAPTER
int acn22_adapter_cache_fresh(void) { return !ap_n && !tex_cache_total && !gen_n && !tex_free_n; }
unsigned long long acn22_adapter_cache_fingerprint(void)
{
    uint64_t h=UINT64_C(14695981039346656037);
    for(int slot=0;slot<TEX_CACHE_SIZE;slot++) {
        const TexCacheEntry *e=&tex_cache[slot];
        if(!e->occupied)continue;
        const uint64_t words[]={slot,e->bytes,e->alloc_w,e->alloc_h,e->used_w,e->used_h,
            acn22_fbits(e->su),acn22_fbits(e->sv),e->min_u,e->min_v,e->range_u,e->range_v,
            e->texbank,e->pal_group,e->pal_gen,e->cmode,e->cap,e->gl_texture,
            e->a_page,e->a_x,e->a_y,acn22_fbits(e->ou),acn22_fbits(e->ov)};
        /* Exclude legitimate hit ref bit and all C padding. */
        for(size_t i=0;i<sizeof words/sizeof words[0];i++)for(int b=0;b<8;b++) {
            h^=(words[i]>>(b*8))&255;h*=UINT64_C(1099511628211);
        }
    }
    const unsigned char *pal=(const unsigned char *)g_eng_pal_gen;
    for(size_t i=0;i<sizeof g_eng_pal_gen;i++){h^=pal[i];h*=UINT64_C(1099511628211);}
    h^=(uint64_t)tex_frame_misses; h*=UINT64_C(1099511628211);
    h^=(uint64_t)tex_reallocs; h*=UINT64_C(1099511628211);
    h^=(uint64_t)tex_subimages; h*=UINT64_C(1099511628211);
    return (unsigned long long)h;
}
int acn22_adapter_cache_shutdown(void)
{
    if (gen_n) glDeleteTextures(gen_n,gen_pool);
    for (int i=0;i<TEX_CACHE_SIZE;i++) if(tex_cache[i].occupied) {
        int page_texture=0;
        for(int p=0;p<ap_n;p++) if(tex_cache[i].gl_texture==apages[p].tex)page_texture=1;
        if(!page_texture)glDeleteTextures(1,&tex_cache[i].gl_texture);
    }
    for(int p=0;p<ap_n;p++){glDeleteTextures(1,&apages[p].tex);free(apages[p].shadow);}
    for(int i=0;i<tex_free_n;i++)glDeleteTextures(1,&tex_free[i].id);
    memset(apages,0,sizeof apages);memset(tex_cache,0,sizeof tex_cache);
    memset(tex_free,0,sizeof tex_free);gen_n=ap_n=tex_free_n=tex_cache_total=0;
    ap_reset_hand=tex_evict_hand=0;tex_cache_bytes=0;tex_frame_hits=tex_frame_misses=0;
    tex_cache_evictions=tex_reallocs=tex_subimages=0;g_tex_page_resets=0;
    pend_n=in_pump=0;g_budget_draw=g_budget_pump=0;acn22_proof_cache_lock(0);
    return glGetError()==GL_NO_ERROR;
}
#endif
'''
