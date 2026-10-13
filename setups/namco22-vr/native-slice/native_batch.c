#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "native_batch.h"
#include "tex_bake.h"
#include "eng_gl.h"
#if ACN22_NATIVE_ADAPTER && defined(_WIN32)
#include <windows.h>
#endif
#ifndef ACN22_NATIVE_GPU_PROOF
#define ACN22_NATIVE_GPU_PROOF 0
#endif
static const acn22_pose_fragment *active;
static acn22_native_vertex *sink;
static uint32_t sink_capacity,*sink_count;
static int bound,failed;

acvr_result acn22_native_prepare(const acn22_pose_fragment *f,geo_quad *copy)
{
    if(!f || !copy || !f->native || f->count<3 || f->count>ACN22_POSE_MAX_VERTS ||
       f->fan_index>1 || f->source_index>=ACN22_POSE_MAX_QUADS)return ACVR_BAD_ARGUMENT;
    const geo_quad *q=f->native;
    if(q->direct || q->behind || q->ndv || q->nrv!=4 || q->objectflags ||
       q->clip[0]>0 || q->clip[1]<639 || q->clip[2]>0 || q->clip[3]<479 ||
       f->painter_key!=q->zsort || f->emission_order!=q->order)return ACVR_UNSUPPORTED;
    geo_quad out=*q;out.nrv=(int)f->count;
    for(uint32_t i=0;i<f->count;i++) {
        const acn22_pose_vertex *v=f->v+i;double p[4],t[4];
        if(acn22_pose_pack(v,p,t)!=ACVR_OK)return ACVR_BAD_ARGUMENT;
        if(v->clip[3]<1e-30 || fabs(v->clip[0])>v->clip[3]*(1+1e-8) ||
           fabs(v->clip[1])>v->clip[3]*(1+1e-8) || fabs(v->clip[2])>v->clip[3]*(1+1e-8))return ACVR_UNSUPPORTED;
        for(int k=0;k<4;k++)if(!isfinite((float)p[k]))return ACVR_UNSUPPORTED;
        geo_vert *r=&out.rv[i];memset(r,0,sizeof *r);r->valid=1;
        r->sx16=(int32_t)llround(p[0]/p[3]*16);r->sy16=(int32_t)llround(p[1]/p[3]*16);
        r->z=(int32_t)v->board_z;r->uf=(int32_t)llround(v->uv[0]*65536);
        r->vf=(int32_t)llround(v->uv[1]*65536);r->bf=(int32_t)fmin(2147483647.0,v->brightness*65536);
        r->u=(uint32_t)v->uv[0];r->v=(uint32_t)v->uv[1];r->bri=(int)v->brightness;
    }
    *copy=out;return ACVR_OK;
}
acvr_result acn22_native_pack(const acn22_pose_fragment *f,uint32_t i,
                            const float xy[2],const float rgba[4],const float st[4],acn22_native_vertex *out)
{
    if(!rgba || !st || !out || (!f && !xy))return ACVR_BAD_ARGUMENT;
    acn22_native_vertex v={0};
    for(int k=0;k<4;k++) {
        if(!isfinite(rgba[k]) || !isfinite(st[k]))return ACVR_BAD_ARGUMENT;
        v.colour[k]=rgba[k];v.primary[k]=st[k];
    }
    if(f) {
        if(f->count<3 || f->count>ACN22_POSE_MAX_VERTS || i>=f->count || st[2]!=0 || st[3]!=1)return ACVR_BAD_ARGUMENT;
        double p[4],t[4];if(acn22_pose_pack(f->v+i,p,t)!=ACVR_OK)return ACVR_BAD_ARGUMENT;
        for(int k=0;k<4;k++){v.position[k]=(float)p[k];if(!isfinite(v.position[k]))return ACVR_UNSUPPORTED;}
        if(v.position[3]<=0)return ACVR_UNSUPPORTED;
        v.board[0]=(float)(f->v[i].board_z/ACN22_CZ_DIVISOR);
    } else {
        if(!isfinite(xy[0]) || !isfinite(xy[1]))return ACVR_BAD_ARGUMENT;
        v.position[0]=xy[0];v.position[1]=xy[1];v.position[3]=1;
    }
    v.board[3]=1;*out=v;return ACVR_OK;
}
acvr_result acn22_native_uv(const acn22_pose_fragment *f,uint32_t i,
                          int bu,int bv,int ru,int rv,float su,float sv,float cu,float cv,
                          float au,float av,float out[4])
{
    if(!f || !out || f->count<3 || f->count>ACN22_POSE_MAX_VERTS || i>=f->count || ru<=0 || rv<=0 ||
       !isfinite(su) || !isfinite(sv) || !isfinite(cu) || !isfinite(cv) ||
       !isfinite(au) || !isfinite(av))return ACVR_BAD_ARGUMENT;
    float t[4]={(float)((f->v[i].uv[0]-bu+cu)/ru*su+au),
                (float)((f->v[i].uv[1]-bv+cv)/rv*sv+av),0,1};
    if(!isfinite(t[0]) || !isfinite(t[1]))return ACVR_BAD_ARGUMENT;
    memcpy(out,t,sizeof t);return ACVR_OK;
}
acvr_result acn22_native_cz_bytes(const eng_fog *fog,uint8_t out[ACN22_CZ_ENTRIES])
{
    if(!fog || !out || (!fog->tab && (fog->alpha_const<-1 || fog->alpha_const>255)) ||
       fog->sdelta<-256 || fog->sdelta>255)return ACVR_BAD_ARGUMENT;
    for(uint32_t i=0;i<ACN22_CZ_ENTRIES;i++)out[i]=(uint8_t)eng_fog_alpha(fog,(int32_t)(i*256));
    return ACVR_OK;
}
const acn22_pose_fragment *acn22_native_active(void){return active;}
acvr_result acn22_native_bind(const acn22_pose_fragment *f,acn22_native_vertex *capture,
                            uint32_t capacity,uint32_t *written)
{
    if(bound)return ACVR_BAD_STATE;
    if(!capture && !ACN22_NATIVE_GPU_PROOF)return ACVR_UNSUPPORTED;
    if(capture && (!written || !capacity))return ACVR_BAD_ARGUMENT;
    active=f;sink=capture;sink_capacity=capacity;sink_count=written;bound=1;failed=0;
    if(written)*written=0;
    return ACVR_OK;
}
int acn22_native_capture_active(void){return sink!=NULL;}
int acn22_native_fan_index(int triangle,int corner){return corner==0?0:triangle+corner-1;}
void acn22_native_fail(void){failed=1;}
void acn22_native_capture_fan(uint32_t n,const float *xy,const float *rgba,const float *st)
{
    acn22_native_vertex scratch[21];uint32_t count=0;
    if(!sink || !sink_count || n<3 || n>9 || (n-2)*3>sink_capacity){failed=1;return;}
    for(uint32_t i=1;i+1<n;i++)for(int k=0;k<3;k++) {
        uint32_t s=(uint32_t)acn22_native_fan_index((int)i,k);
        if(acn22_native_pack(active,s,xy+s*2,rgba+s*4,st+s*4,scratch+count)!=ACVR_OK){failed=1;return;}
        count++;
    }
    memcpy(sink,scratch,count*sizeof *sink);*sink_count=count;
}
acvr_result acn22_native_unbind(void)
{
    acvr_result result=failed?ACVR_ERROR:ACVR_OK;
    active=NULL;sink=NULL;sink_capacity=0;sink_count=NULL;bound=0;failed=0;return result;
}
acvr_result acn22_native_draw_fragment(const acn22_pose_fragment *f,const eng_draw_cfg *cfg)
{
    if(!ACN22_NATIVE_GPU_PROOF){(void)f;(void)cfg;return ACVR_UNSUPPORTED;}
    geo_quad q;acvr_result result=acn22_native_prepare(f,&q);if(result!=ACVR_OK)return result;
    if(!cfg || cfg->flat_white || cfg->flat_quad || cfg->depth_test || cfg->write_prio_alpha ||
       cfg->fog_before_shade || cfg->wide_backdrop || g_tex_opaque!=1)return ACVR_UNSUPPORTED;
    if(!SDL_GL_GetCurrentContext())return ACVR_UNSUPPORTED;
    int profile=0;if(SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,&profile)!=0 ||
        profile!=SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)return ACVR_UNSUPPORTED;
    int major=0,minor=0;
    if(SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major)!=0 ||
       SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor)!=0 ||
       major<3 || (major==3 && minor<3))return ACVR_UNSUPPORTED;
    GLint program=0,buffer=0,vao=0;
    glGetIntegerv(0x8B8D,&program); /* GL_CURRENT_PROGRAM */
    glGetIntegerv(0x8894,&buffer);  /* GL_ARRAY_BUFFER_BINDING */
    glGetIntegerv(0x85B5,&vao);     /* GL_VERTEX_ARRAY_BINDING */
    if(program || buffer || vao || glIsEnabled(GL_DEPTH_TEST) || glIsEnabled(GL_LIGHTING) ||
       glIsEnabled(GL_FOG) || glIsEnabled(GL_CULL_FACE))return ACVR_UNSUPPORTED;
    GLfloat projection[16],model[16];glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,model);
    for(int i=0;i<16;i++) {
        double expected=i==0?2.0/640:(i==5?-2.0/480:(i==10?-1:(i==12?-1:(i==13||i==15?1:0))));
        if(!isfinite(projection[i]) || fabs(projection[i]-expected)>1e-7 ||
           !isfinite(model[i]) || fabs(model[i]-(i%5==0?1:0))>1e-7)return ACVR_UNSUPPORTED;
    }
    if(g_scene_x0!=0 || g_scene_x1!=640 || (g_eng_quad_dx && g_eng_quad_dx(&q)))return ACVR_UNSUPPORTED;
    result=acn22_native_bind(f,NULL,0,NULL);if(result!=ACVR_OK)return result;
    eng_draw_quad(&q,cfg);return acn22_native_unbind();
}
acvr_result acn22_native_draw_fragment_borrowed(const acn22_pose_fragment *f,
    const eng_draw_cfg *cfg,uint64_t context,uint32_t owner_thread)
{
#if ACN22_NATIVE_ADAPTER && ACN22_NATIVE_GPU_PROOF && defined(_WIN32)
    if(GetCurrentThreadId()!=owner_thread || !context ||
       (uint64_t)(uintptr_t)wglGetCurrentContext()!=context)return ACVR_BAD_STATE;
    geo_quad q;acvr_result r=acn22_native_prepare(f,&q);if(r!=ACVR_OK)return r;
    if(!cfg || cfg->flat_white || cfg->flat_quad || cfg->depth_test || cfg->write_prio_alpha ||
       cfg->fog_before_shade || cfg->wide_backdrop || g_tex_opaque!=1)return ACVR_UNSUPPORTED;
    GLint program=0,buffer=0,vao=0;
    glGetIntegerv(0x8b8d,&program);glGetIntegerv(0x8894,&buffer);glGetIntegerv(0x85b5,&vao);
    if(program || buffer || vao || glIsEnabled(GL_DEPTH_TEST) || glIsEnabled(GL_LIGHTING) ||
       glIsEnabled(GL_FOG) || glIsEnabled(GL_CULL_FACE))return ACVR_UNSUPPORTED;
    GLfloat projection[16],model[16];
    glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,model);
    for(int i=0;i<16;i++){
        double expected=i==0?2.0/640:(i==5?-2.0/480:(i==10?-1:(i==12?-1:(i==13||i==15?1:0))));
        if(!isfinite(projection[i]) || fabs(projection[i]-expected)>1e-7 ||
           !isfinite(model[i]) || fabs(model[i]-(i%5==0?1:0))>1e-7)return ACVR_UNSUPPORTED;
    }
    if(g_scene_x0!=0 || g_scene_x1!=640 || (g_eng_quad_dx && g_eng_quad_dx(&q)))return ACVR_UNSUPPORTED;
    r=acn22_native_bind(f,NULL,0,NULL);if(r!=ACVR_OK)return r;
    eng_draw_quad(&q,cfg);return acn22_native_unbind();
#else
    (void)f;(void)cfg;(void)context;(void)owner_thread;return ACVR_UNSUPPORTED;
#endif
}
