/* SPDX-License-Identifier: GPL-3.0-only
 * Source-only experimental integration. No production query or context creation. */
#include <windows.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "eng.h"
#include "eng_gl.h"
#include "tex_bake.h"
#include "native_adapter.h"
#include "native_adapter_support.h"
#include "native_display.h"
#include "native_fixture.h"
#include "native_target_state.h"
#include "native_batch.h"
#include "paired_proof_support.h"
#include "n22_api_table.h"
#if !ACN22_NATIVE_ADAPTER || !ACN22_NATIVE_GPU_PROOF
#error Native adapter requires its separate explicit opt-in target
#endif
#ifndef ACVR_BACKEND_API_V1_SIZE
#error Native adapter requires reviewed libacvr prepare-display contract
#endif
#define GLX_FUNCTIONS(X) \
 X(active,void,(GLenum),"glActiveTexture") \
 X(client,void,(GLenum),"glClientActiveTexture") \
 X(program,void,(GLuint),"glUseProgram") \
 X(vao,void,(GLuint),"glBindVertexArray") \
 X(buffer,void,(GLenum,GLuint),"glBindBuffer") \
 X(sampler,void,(GLuint,GLuint),"glBindSampler") \
 X(fbo,void,(GLenum,GLuint),"glBindFramebuffer") \
 X(draw_buffers,void,(GLsizei,const GLenum *),"glDrawBuffers") \
 X(status,GLenum,(GLenum),"glCheckFramebufferStatus") \
 X(attachment,void,(GLenum,GLenum,GLenum,GLint *),"glGetFramebufferAttachmentParameteriv")
typedef struct {
#define FIELD(n,r,a,s) r (APIENTRY *n)a;
 GLX_FUNCTIONS(FIELD)
#undef FIELD
} dispatch;
typedef struct {
    GLint read_fbo,draw_fbo,program,vao,array,element,active,client,mode,pack,unpack;
    GLint sampler[2],units,clips;
    GLboolean modern[5],clip[32];
    int attrib,client_attrib,projection,model,texture[2],sampler_saved[2];
} saved;
struct acvr_frame {
    uint64_t id;
    geo_quad quads[2],frozen[2];
    uint32_t count;
    acvr_game_camera camera;
};
struct acvr_backend {
    acvr_graphics_device device;
    dispatch gl;
    DWORD thread;
    uint64_t tick,input_tick,cache_fingerprint;
    struct acvr_frame *lease;
    acn22_display_pair pair;
    uint8_t *texture,*tilemap,*texture_copy,*tilemap_copy;
    uint8_t palette[128][256][3],fog_table[8192];
    eng_fog fog;
    eng_draw_cfg config;
    int paused,cz,owns_cache;
    acvr_result diagnostic;
};
static struct acvr_backend *current;
static const GLenum modern_caps[]={0x8db9,0x8c89,0x864f,0x8e51,0x8458};
static int extension(const char *list,const char *name)
{
    if(!list)return 0;
    size_t n=strlen(name);
    for(const char *p=list;(p=strstr(p,name));p+=n)
        if((p==list || p[-1]==' ') && (!p[n] || p[n]==' '))return 1;
    return 0;
}
static int owner(const struct acvr_backend *b)
{
    return b && current==b && GetCurrentThreadId()==b->thread &&
        (uint64_t)(uintptr_t)wglGetCurrentContext()==b->device.context;
}
void *acn22_adapter_proc(const char *name)
{
    if(!owner(current) || !name)return NULL;
    void *p=current->device.get_proc(current->device.proc_user,name);
    uintptr_t value=(uintptr_t)p;
    return value>3 && value!=UINTPTR_MAX?p:NULL;
}
static int scope_end(struct acvr_backend *b,saved *s)
{
    b->gl.fbo(0x8ca9,(GLuint)s->draw_fbo);b->gl.fbo(0x8ca8,(GLuint)s->read_fbo);
    for(int i=1;i>=0;--i)if(s->texture[i]){
        b->gl.active(0x84c0+i);glMatrixMode(GL_TEXTURE);glPopMatrix();
    }
    if(s->model){glMatrixMode(GL_MODELVIEW);glPopMatrix();}
    if(s->projection){glMatrixMode(GL_PROJECTION);glPopMatrix();}
    if(s->client_attrib){
        b->gl.vao(0);glPopClientAttrib(); /* restore the scratch/default VAO too */
    }
    b->gl.vao((GLuint)s->vao);b->gl.buffer(0x8892,(GLuint)s->array);
    b->gl.buffer(0x8893,(GLuint)s->element);
    if(s->attrib)glPopAttrib();
    for(int i=0;i<5;++i)(s->modern[i]?glEnable:glDisable)(modern_caps[i]);
    for(int i=0;i<s->clips;++i)(s->clip[i]?glEnable:glDisable)(0x3000+i);
    for(int i=0;i<2;++i)if(s->sampler_saved[i])b->gl.sampler((GLuint)i,(GLuint)s->sampler[i]);
    b->gl.buffer(0x88eb,(GLuint)s->pack);b->gl.buffer(0x88ec,(GLuint)s->unpack);
    b->gl.active((GLenum)s->active);b->gl.client((GLenum)s->client);
    b->gl.program((GLuint)s->program);glMatrixMode((GLenum)s->mode);
    return glGetError()==GL_NO_ERROR;
}
static acvr_result scope_begin(struct acvr_backend *b,saved *s)
{
    memset(s,0,sizeof *s);
    if(glGetError()!=GL_NO_ERROR)return ACVR_ERROR;
    glGetIntegerv(0x8ca6,&s->draw_fbo);glGetIntegerv(0x8caa,&s->read_fbo);
    glGetIntegerv(0x8b8d,&s->program);glGetIntegerv(0x85b5,&s->vao);
    glGetIntegerv(0x8894,&s->array);glGetIntegerv(0x8895,&s->element);
    glGetIntegerv(0x84e0,&s->active);glGetIntegerv(0x84e1,&s->client);
    glGetIntegerv(GL_MATRIX_MODE,&s->mode);
    glGetIntegerv(0x88ed,&s->pack);glGetIntegerv(0x88ef,&s->unpack);
    glGetIntegerv(0x84e2,&s->units);glGetIntegerv(0x0d32,&s->clips);
    if(s->units<2 || s->units>32 || s->clips<0 || s->clips>32 || glGetError()!=GL_NO_ERROR)
        return ACVR_UNSUPPORTED;
    for(int i=0;i<5;++i)s->modern[i]=glIsEnabled(modern_caps[i]);
    for(int i=0;i<s->clips;++i)s->clip[i]=glIsEnabled(0x3000+i);
    if(glGetError()!=GL_NO_ERROR)return ACVR_UNSUPPORTED;
    /* Reject unproved imaging transforms before mutations, rather than silently
     * allowing colour-table/convolution/colour-matrix texture conversion. */
    if(extension((const char *)glGetString(GL_EXTENSIONS),"GL_ARB_imaging")){
        const GLenum imaging[]={0x80d0,0x80d1,0x80d2,0x8010,0x8011,0x8012,0x8024,0x802e};
        for(size_t i=0;i<sizeof imaging/sizeof imaging[0];++i)
            if(glIsEnabled(imaging[i]))return ACVR_UNSUPPORTED;
        GLfloat matrix[16];glGetFloatv(0x80b1,matrix);
        for(int i=0;i<16;++i)if(matrix[i]!=(i%5==0?1.f:0.f))return ACVR_UNSUPPORTED;
        for(int i=0;i<8;++i){
            GLfloat value=0;glGetFloatv(0x801c+i,&value);
            if(value!=(i<4?1.f:0.f))return ACVR_UNSUPPORTED;
            glGetFloatv(0x80b4+i,&value);
            if(value!=(i<4?1.f:0.f))return ACVR_UNSUPPORTED;
        }
    }
    if(glGetError()!=GL_NO_ERROR)return ACVR_UNSUPPORTED;
    glPushAttrib(GL_ALL_ATTRIB_BITS|0x20000000u); /* include multisample state */
    if(glGetError()!=GL_NO_ERROR)return ACVR_ERROR;
    s->attrib=1;
    /* Capture both sampler names before any mutation, for partial unwinding. */
    for(int i=0;i<2;++i){
        b->gl.active(0x84c0+i);glGetIntegerv(0x8919,s->sampler+i);
        if(glGetError()!=GL_NO_ERROR)goto fail;
        s->sampler_saved[i]=1;
    }
    b->gl.active((GLenum)s->active);
    b->gl.vao(0); /* Save the VAO actually used by native client arrays. */
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    if(glGetError()!=GL_NO_ERROR)goto fail;
    s->client_attrib=1;
    glMatrixMode(GL_PROJECTION);glPushMatrix();
    if(glGetError()!=GL_NO_ERROR)goto fail;
    s->projection=1;glLoadIdentity();glOrtho(0,640,480,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();
    if(glGetError()!=GL_NO_ERROR)goto fail;
    s->model=1;glLoadIdentity();
    for(int i=0;i<s->units;++i){
        b->gl.active(0x84c0+i);b->gl.client(0x84c0+i);
        glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_2D);glDisable(0x806f);
        glDisable(0x8513);glDisable(0x84f5);
        glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        if(i<2){
            b->gl.sampler((GLuint)i,0);glMatrixMode(GL_TEXTURE);glPushMatrix();
            if(glGetError()!=GL_NO_ERROR)goto fail;
            s->texture[i]=1;glLoadIdentity();
        }
    }
    b->gl.active(0x84c0);b->gl.client(0x84c0);glMatrixMode(GL_MODELVIEW);
    b->gl.program(0);b->gl.vao(0);b->gl.buffer(0x8892,0);b->gl.buffer(0x8893,0);
    b->gl.buffer(0x88eb,0);b->gl.buffer(0x88ec,0);
    for(int i=0;i<5;++i)glDisable(modern_caps[i]);
    for(int i=0;i<s->clips;++i)glDisable(0x3000+i);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);
    glDisable(GL_FOG);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_DITHER);
    glDisable(GL_STENCIL_TEST);glDisable(0x809d);glDisable(0x809e);
    glDisable(0x809f);glDisable(0x80a0);glDisable(GL_POLYGON_STIPPLE);glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_SCISSOR_TEST);glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    const GLenum packs[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,
        GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,
        0x806c,0x806b,0x806e,0x806d,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST,
        GL_UNPACK_SWAP_BYTES,GL_UNPACK_LSB_FIRST};
    for(size_t i=0;i<sizeof packs/sizeof packs[0];++i)
        glPixelStorei(packs[i],packs[i]==GL_PACK_ALIGNMENT || packs[i]==GL_UNPACK_ALIGNMENT?1:0);
    glPixelTransferf(GL_RED_SCALE,1);glPixelTransferf(GL_GREEN_SCALE,1);
    glPixelTransferf(GL_BLUE_SCALE,1);glPixelTransferf(GL_ALPHA_SCALE,1);
    glPixelTransferf(GL_RED_BIAS,0);glPixelTransferf(GL_GREEN_BIAS,0);
    glPixelTransferf(GL_BLUE_BIAS,0);glPixelTransferf(GL_ALPHA_BIAS,0);
    glPixelTransferi(GL_MAP_COLOR,0);
    if(glGetError()==GL_NO_ERROR)return ACVR_OK;
fail:
    scope_end(b,s);return ACVR_ERROR;
}
static acvr_result target(struct acvr_backend *b,const acvr_render_target *t)
{
    b->gl.fbo(0x8d40,(GLuint)t->framebuffer);
    if(b->gl.status(0x8d40)!=0x8cd5)return ACVR_UNSUPPORTED;
    GLint type=0,name=0;
    b->gl.attachment(0x8d40,0x8ce0,0x8cd0,&type);
    b->gl.attachment(0x8d40,0x8ce0,0x8cd1,&name);
    if(type!=GL_TEXTURE || (GLuint)name!=t->colour_image)return ACVR_BAD_ARGUMENT;
    const acn22_target_ops ops={glGetIntegerv,b->gl.draw_buffers,b->gl.attachment,glGetError};
    acvr_result level=acn22_target_level_zero(&ops);
    if(level!=ACVR_OK)return level; /* no unsupported mip may reach level0 queries */
    glBindTexture(GL_TEXTURE_2D,(GLuint)t->colour_image);
    GLint width=0,height=0,format=0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    if(width!=(GLint)t->width || height!=(GLint)t->height || format!=0x8058)
        return ACVR_UNSUPPORTED;
    if(t->depth_image){
        b->gl.attachment(0x8d40,0x8d00,0x8cd0,&type);
        b->gl.attachment(0x8d40,0x8d00,0x8cd1,&name);
        if((type!=GL_TEXTURE && type!=0x8d41) || (GLuint)name!=t->depth_image)
            return ACVR_BAD_ARGUMENT;
    }
    return glGetError()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR;
}
static int fog(const geo_quad *q,eng_fog *out);
static int resources(const struct acvr_backend *b)
{
    static const uint16_t generations[ENG_PAL_GROUPS]={0};
    for(int i=0;i<8192;++i)if(b->fog_table[i]!=(i==4?130:(i==5?250:0)))return 0;
    return g_texture_data==b->texture && g_texture_tilemap==b->tilemap &&
        direct_palette_loaded==1 && !memcmp(direct_palette,b->palette,sizeof b->palette) &&
        !memcmp(g_eng_pal_gen,generations,sizeof generations) &&
        !memcmp(b->texture,b->texture_copy,TEXTURE_TOTAL_SIZE) &&
        !memcmp(b->tilemap,b->tilemap_copy,TEXTUREMAP_SIZE) &&
        g_scene_x0==0 && g_scene_x1==640 && !g_eng_quad_dx && !g_eng_tex_tile16 &&
        !g_tex_clipbox && !g_eng_degen_uv_legacy &&
        b->fog.tab==b->fog_table && !b->fog.sdelta && b->fog.alpha_const==-1 &&
        b->fog.rgb[0]==10 && b->fog.rgb[1]==20 && b->fog.rgb[2]==30 &&
        b->config.shade==1 && b->config.texel_centre==1 && b->config.fog==b->cz &&
        b->config.fog_quad==(b->cz?fog:NULL) && !b->config.fade_rgb &&
        !b->config.flat_white && !b->config.flat_quad && !b->config.depth_test &&
        !b->config.quad_depth && !b->config.fog_before_shade &&
        !b->config.write_prio_alpha && !b->config.wide_backdrop;
}
static int lease(const struct acvr_backend *b,const struct acvr_frame *f)
{return owner(b) && f && b->lease==f && f->id==g_eng_frame && f->count==(b->cz?1u:2u);}
static int fog(const geo_quad *q,eng_fog *out)
{
    if(!current || !current->lease || !current->cz || !q || !out)return 0;
    *out=current->fog;return 1;
}
static void release(struct acvr_backend *b,struct acvr_frame *f)
{
    if(!owner(b) || b->lease!=f)return;
    memset(&b->pair,0,sizeof b->pair);b->lease=NULL;free(f);
}
static void ACVR_CALL close_backend(struct acvr_backend *b)
{
    if(!owner(b))return;
    release(b,b->lease);
    saved s;acvr_result scoped=scope_begin(b,&s);
    if(b->owns_cache){
        int quad=acn22_adapter_quad_shutdown();int cache=acn22_adapter_cache_shutdown();
        if(!quad || !cache)b->diagnostic=ACVR_ERROR;
    }
    if(scoped==ACVR_OK && !scope_end(b,&s))b->diagnostic=ACVR_ERROR;
    g_texture_data=NULL;g_texture_tilemap=NULL;direct_palette_loaded=0;
    memset(direct_palette,0,sizeof direct_palette);memset(g_eng_pal_gen,0,sizeof g_eng_pal_gen);
    g_eng_frame=0;
    free(b->texture);free(b->tilemap);free(b->texture_copy);free(b->tilemap_copy);
    current=NULL;free(b);
}
static acvr_result ACVR_CALL open_backend(const acvr_open_info *in,acvr_backend **out,acvr_backend_info *info)
{
    if(!out)return ACVR_BAD_ARGUMENT;
    *out=NULL;
    if(!in || !info)return ACVR_BAD_ARGUMENT;
    if(in->size<sizeof *in || info->size<sizeof *info || in->version!=ACVR_STRUCT_VERSION ||
       info->version!=ACVR_STRUCT_VERSION || in->graphics.size<sizeof in->graphics ||
       in->graphics.version!=ACVR_STRUCT_VERSION)return ACVR_BAD_VERSION;
    if(!in->game_id_utf8 || strcmp(in->game_id_utf8,"synthetic-system22-native") ||
       (in->content_root_utf8 && *in->content_root_utf8) ||
       (in->storage_root_utf8 && *in->storage_root_utf8))return ACVR_UNSUPPORTED;
    int cz=0;
    if(in->backend_options_utf8 && *in->backend_options_utf8){
        if(strcmp(in->backend_options_utf8,"clipped-cz"))return ACVR_UNSUPPORTED;
        cz=1;
    }
    if(current || g_texture_data || g_texture_tilemap || !acn22_adapter_cache_fresh())return ACVR_BAD_STATE;
    if(in->graphics.api!=ACVR_GRAPHICS_GL || in->graphics.flags&ACVR_DEVICE_GLES ||
       !(in->graphics.flags&ACVR_DEVICE_GL_COMPATIBILITY))return ACVR_UNSUPPORTED;
    if(!in->graphics.get_proc || !in->graphics.context ||
       (uint64_t)(uintptr_t)wglGetCurrentContext()!=in->graphics.context)return ACVR_BAD_STATE;
    GLint profile=0,major=0,minor=0;
    glGetIntegerv(0x9126,&profile);glGetIntegerv(0x821b,&major);glGetIntegerv(0x821c,&minor);
    if(!(profile&2) || major<3 || (major==3 && minor<3) || glGetError()!=GL_NO_ERROR)
        return ACVR_UNSUPPORTED;
    struct acvr_backend *b=calloc(1,sizeof *b);if(!b)return ACVR_ERROR;
    b->device=in->graphics;b->thread=GetCurrentThreadId();b->cz=cz;current=b;
#define LOAD(n,r,a,name) do { void *p=acn22_adapter_proc(name); \
    _Static_assert(sizeof p==sizeof b->gl.n,"native function pointer width"); \
    if(!p){current=NULL;free(b);return ACVR_UNSUPPORTED;}memcpy(&b->gl.n,&p,sizeof p); } while(0);
    GLX_FUNCTIONS(LOAD)
#undef LOAD
    b->texture=calloc(TEXTURE_TOTAL_SIZE,1);b->tilemap=calloc(TEXTUREMAP_SIZE,1);
    b->texture_copy=calloc(TEXTURE_TOTAL_SIZE,1);b->tilemap_copy=calloc(TEXTUREMAP_SIZE,1);
    if(!b->texture || !b->tilemap || !b->texture_copy || !b->tilemap_copy){close_backend(b);return ACVR_ERROR;}
    memset(b->texture,2,256);memcpy(b->texture_copy,b->texture,TEXTURE_TOTAL_SIZE);
    const int groups[]={1,2,6};const uint8_t colors[][3]={{30,155,245},{240,160,35},{80,100,120}};
    for(int i=0;i<3;++i)memcpy(b->palette[groups[i]][2],colors[i],3);
    b->fog_table[4]=130;b->fog_table[5]=250;b->fog.tab=b->fog_table;b->fog.alpha_const=-1;
    b->fog.rgb[0]=10;b->fog.rgb[1]=20;b->fog.rgb[2]=30;
    g_texture_data=b->texture;g_texture_tilemap=b->tilemap;
    memcpy(direct_palette,b->palette,sizeof b->palette);direct_palette_loaded=1;
    memset(g_eng_pal_gen,0,sizeof g_eng_pal_gen);g_eng_frame=0;
    g_scene_x0=0;g_scene_x1=640;g_eng_quad_dx=NULL;g_eng_tex_tile16=0;
    g_tex_clipbox=g_eng_degen_uv_legacy=g_perf_enabled=0;
    b->config.shade=1;b->config.texel_centre=1;b->config.fog=cz;b->config.fog_quad=cz?fog:NULL;
    saved s;acvr_result r=scope_begin(b,&s);
    if(r==ACVR_OK){
        b->owns_cache=1;
        if(!acn22_proof_cache_configure())r=ACVR_ERROR;
        if(!scope_end(b,&s))r=ACVR_ERROR;
    }
    if(r!=ACVR_OK){close_backend(b);return r;}
    acvr_backend_info value={0};value.native_rate_num=59906;value.native_rate_den=1000;
    value.player_count=1;value.scene_units_per_metre=1000;value.control_stride=sizeof(acvr_control_desc);
    memcpy((char *)info+8,(const char *)&value+8,sizeof value-8);*out=b;return ACVR_OK;
}
static acvr_result ACVR_CALL pause_backend(acvr_backend *b,uint32_t paused)
{
    if(!owner(b))return ACVR_BAD_STATE;
    if(paused>1)return ACVR_BAD_ARGUMENT;
    b->paused=(int)paused;b->input_tick=0;memset(&b->pair,0,sizeof b->pair);return ACVR_OK;
}
static acvr_result ACVR_CALL inputs(acvr_backend *b,const acvr_inputs *in)
{
    if(!owner(b) || b->paused || b->lease)return ACVR_BAD_STATE;
    if(!in || in->size<sizeof *in || in->version!=ACVR_STRUCT_VERSION)return ACVR_BAD_VERSION;
    if(in->tick_id!=b->tick+1)return ACVR_BAD_ARGUMENT;
    if(in->gun_count || in->axis_count || in->button_count)return ACVR_UNSUPPORTED;
    b->input_tick=in->tick_id;return ACVR_OK;
}
static acvr_result ACVR_CALL step(acvr_backend *b,const acvr_step_info *in,acvr_frame **out,acvr_frame_info *info)
{
    if(!out)return ACVR_BAD_ARGUMENT;
    *out=NULL;
    if(!owner(b) || b->paused || b->lease)return ACVR_BAD_STATE;
    if(!in || !info || in->size<sizeof *in || info->size<sizeof *info ||
       in->version!=ACVR_STRUCT_VERSION || info->version!=ACVR_STRUCT_VERSION)return ACVR_BAD_VERSION;
    if(in->tick_id!=b->tick+1 || in->tick_id!=b->input_tick || in->tick_id>UINT_MAX ||
       in->tick_id-1>UINT64_MAX/UINT64_C(1000000000000) ||
       in->simulation_time_ns!=(int64_t)((in->tick_id-1)*UINT64_C(1000000000000)/59906))
        return ACVR_BAD_ARGUMENT;
    if(!resources(b))return ACVR_BAD_STATE;
    struct acvr_frame *f=calloc(1,sizeof *f);if(!f)return ACVR_ERROR;
    f->count=(uint32_t)acn22_fixture_scene(b->cz,f->quads);memcpy(f->frozen,f->quads,sizeof f->quads);
    f->id=in->tick_id;ACVR_INIT(&f->camera);f->camera.flags=ACVR_CAMERA_VIEW_LOCAL;
    f->camera.raster_width=640;f->camera.raster_height=480;
    f->camera.focal_x_px=f->camera.focal_y_px=400;f->camera.centre_x_px=320;f->camera.centre_y_px=240;
    f->camera.viewport_px[2]=640;f->camera.viewport_px[3]=480;
    for(int i=0;i<4;++i)f->camera.view_from_scene[i*5]=1;
    f->camera.near_scene=100;f->camera.far_scene=12000;
    acvr_frame_info value={0};value.frame_id=f->id;value.camera_count=1;
    memcpy((char *)info+8,(const char *)&value+8,sizeof value-8);
    g_eng_frame=(unsigned)f->id;b->tick=f->id;b->lease=f;*out=f;return ACVR_OK;
}
static void ACVR_CALL release_backend(acvr_backend *b,acvr_frame *f){release(b,f);}
static acvr_result ACVR_CALL camera(acvr_backend *b,const acvr_frame *f,uint32_t id,acvr_game_camera *out)
{
    if(!lease(b,f))return ACVR_BAD_STATE;
    if(!out || out->size<sizeof *out || out->version!=ACVR_STRUCT_VERSION)return ACVR_BAD_VERSION;
    if(id)return ACVR_BAD_ARGUMENT;
    memcpy((char *)out+8,(const char *)&f->camera+8,sizeof *out-8);return ACVR_OK;
}
static acvr_result ACVR_CALL prepare(acvr_backend *b,const acvr_frame *f,uint32_t count,uint32_t stride,const acvr_draw_info *d)
{
    if(!lease(b,f))return ACVR_BAD_STATE;
    memset(&b->pair,0,sizeof b->pair);
    if(!resources(b) || memcmp(f->quads,f->frozen,sizeof f->quads) || !acn22_adapter_quad_idle())return ACVR_BAD_STATE;
    acn22_display_pair candidate;
    acvr_result r=acn22_display_stage(f->quads,f->count,f->id,count,stride,d,&candidate);
    if(r!=ACVR_OK)return r;
    for(int eye=0;eye<2;++eye)for(uint32_t i=0;i<candidate.count[eye];++i){
        geo_quad copy;r=acn22_native_prepare(candidate.packets[eye]+i,&copy);if(r!=ACVR_OK)return r;
    }
    saved s;r=scope_begin(b,&s);if(r!=ACVR_OK)return r;
    for(uint32_t i=0;i<2 && r==ACVR_OK;++i){
        const acvr_draw_info *record=(const acvr_draw_info *)((const char *)d+i*stride);
        r=target(b,&record->target);
    }
    if(r==ACVR_OK && !acn22_proof_prime(f->quads,(int)f->count,b->cz?&b->fog:NULL))r=ACVR_ERROR;
    if(!scope_end(b,&s))r=ACVR_ERROR;
    if(r!=ACVR_OK)return r;
    b->cache_fingerprint=acn22_adapter_cache_fingerprint();b->pair=candidate;return ACVR_OK;
}
static acvr_result ACVR_CALL draw(acvr_backend *b,const acvr_frame *f,const acvr_draw_info *in)
{
    if(!lease(b,f))return ACVR_BAD_STATE;
    acvr_result r=acn22_display_match(&b->pair,in);
    if(r!=ACVR_OK){memset(&b->pair,0,sizeof b->pair);return r;}
    if(!resources(b) || memcmp(f->quads,f->frozen,sizeof f->quads) ||
       !acn22_proof_cache_locked() || !acn22_proof_cache_ready() ||
       acn22_adapter_cache_fingerprint()!=b->cache_fingerprint){memset(&b->pair,0,sizeof b->pair);return ACVR_BAD_STATE;}
    saved s;r=scope_begin(b,&s);
    if(r!=ACVR_OK){memset(&b->pair,0,sizeof b->pair);return r;}
    const acn22_target_ops ops={glGetIntegerv,b->gl.draw_buffers,b->gl.attachment,glGetError};
    acn22_target_selection selection={0};
    r=target(b,&in->target);
    if(r==ACVR_OK)r=acn22_target_colour_begin(&ops,&selection);
    if(r==ACVR_OK){
        const acvr_eye *e=in->views;
        glViewport(e->rect_x,e->rect_y,e->rect_width,e->rect_height);
        glEnable(GL_SCISSOR_TEST);glScissor(e->rect_x,e->rect_y,e->rect_width,e->rect_height);
        glClearColor(12.f/255,18.f/255,28.f/255,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
        eng_draw_begin();
        int eye=b->pair.next_eye;
        for(uint32_t i=0;i<b->pair.count[eye] && r==ACVR_OK;++i)
            r=acn22_native_draw_fragment_borrowed(b->pair.packets[eye]+i,&b->config,b->device.context,b->thread);
        eng_draw_end(); /* always flush native batch BEFORE target/state restoration */
        if(glGetError()!=GL_NO_ERROR || !resources(b) || !acn22_proof_cache_ready() ||
           acn22_adapter_cache_fingerprint()!=b->cache_fingerprint)r=ACVR_ERROR;
    }
    if(selection.restore_pending){
        /* Restore ALL target selections while this same borrowed FBO is bound,
         * including failed selection, draw and post-draw/resource exits. */
        b->gl.fbo(0x8ca9,(GLuint)in->target.framebuffer);
        if(acn22_target_colour_restore(&ops,&selection)!=ACVR_OK)r=ACVR_ERROR;
    }
    if(!scope_end(b,&s))r=ACVR_ERROR;
    if(r==ACVR_OK && ++b->pair.next_eye<2)return r;
    memset(&b->pair,0,sizeof b->pair);return r;
}
static acvr_result ACVR_CALL outputs(acvr_backend *b,acvr_outputs *out)
{
    if(!owner(b))return ACVR_BAD_STATE;
    if(!out || out->size<sizeof *out || out->version!=ACVR_STRUCT_VERSION)return ACVR_BAD_VERSION;
    if(!out->capacity || !out->events || out->stride<sizeof(acvr_output_event) ||
       out->stride%_Alignof(acvr_output_event))return ACVR_BAD_ARGUMENT;
    out->count=out->dropped=0;return ACVR_OK;
}
acvr_result ACVR_CALL acn22_native_adapter_query(uint32_t abi,acvr_backend_api *out)
{
    if(abi!=ACVR_ABI_VERSION)return ACVR_BAD_VERSION;
    acvr_result r=n22_api_table_valid(out);if(r!=ACVR_OK)return r;
    acvr_backend_api api={0};api.abi_version=ACVR_ABI_VERSION;
    /* Old complete tables are supported, but cannot negotiate drawing without
     * the mandatory preparation tail. An old graphics runtime rejects mask0. */
    api.supported_graphics=out->size>=sizeof *out?ACVR_GRAPHICS_BIT(ACVR_GRAPHICS_GL):0;
    api.game_open=open_backend;api.game_close=close_backend;api.game_pause=pause_backend;
    api.game_set_inputs=inputs;api.game_step=step;api.game_release_frame=release_backend;
    api.game_camera=camera;api.game_draw_eye=draw;api.game_poll_outputs=outputs;
    api.game_prepare_display=prepare;n22_api_table_payload(out,&api);return ACVR_OK;
}
