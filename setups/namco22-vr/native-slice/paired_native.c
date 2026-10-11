/* Own formula fixture, actual native batch and private transactional wire. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#include <SDL2/SDL.h>
#include "eng.h"
#include "eng_gl.h"
#include "tex_bake.h"
#include "native_batch.h"
#include "paired_proof_support.h"
#include "paired_oracle.h"
#include "product_title.h"
#ifndef ACN22_NATIVE_GPU_PROOF
#define ACN22_NATIVE_GPU_PROOF 0
#endif

static char root[_MAX_PATH];
static geo_quad scene[2];
static int count, current_case;
static eng_draw_cfg config;
static uint8_t table[8192], *rgb, *export_rgb;
static eng_fog material;
static uint64_t lease,scene_frame,sequence,deadline;
static int lease_active;
static char bound_snapshot[65],bound_image[65],bound_composition[65];
static uint64_t target_generation;
static LARGE_INTEGER frequency;
static acvr_eye eyes[2];
static acn22_pose_fragment packets[2][4];
static uint32_t packet_count[2];
static float head[16],anchor[16],projection[16],eye_offset[2][16];
static uint64_t entry_frame[2],exit_frame[2],readback_qpc;
typedef struct {uint64_t lease_id,frame,counter,seq,target;
    float view[16],projection[16];
    } proof_stamp;
static proof_stamp entries[2],exits[2];

static uint64_t qpc(void) { LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
    }
static void floats(float *dst,const uint32_t *src) { for(int i=0;i<16;i++)memcpy(dst+i,src+i,4);
    }
static int alive(void) { return GetTickCount64()<deadline;
    }
static FILE *fresh(const char *name)
{
    char path[_MAX_PATH];
    if(snprintf(path,sizeof path,"%s/%s",root,name)>=(int)sizeof path)return NULL;
    int fd=_open(path,_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(fd<0)return NULL;
    FILE *f=_fdopen(fd,"wb");
    if(!f)_close(fd);
    return f;
}
static int blob(const char *name,const void *data,size_t size)
{
    FILE *f=fresh(name);
    if(!f)return 0;
    int ok=fwrite(data,1,size,f)==size;
    return fclose(f)==0 && ok;
}
static int same(const char *name,const void *data,size_t size)
{
    char path[_MAX_PATH];
    if(snprintf(path,sizeof path,"%s/%s",root,name)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    uint8_t buf[4096];
    const uint8_t *p=data;
    int ok=1;
    while(size && ok){size_t n=size>sizeof buf?sizeof buf:size;
        ok=fread(buf,1,n,f)==n && !memcmp(buf,p,n);
        p+=n;
        size-=n;
        }
    if(fgetc(f)!=EOF)ok=0;
    if(ferror(f))ok=0;
    return fclose(f)==0 && ok;
}
static void integer(FILE *f,uint64_t value,int n) { for(int i=0;i<n;i++){fputc((int)(value&255),f);
        value>>=8;
        } }
static void real32(FILE *f,float x) { uint32_t b;
    memcpy(&b,&x,4);
    integer(f,b,4);
    }
static void real64(FILE *f,double x) { uint64_t b;
    memcpy(&b,&x,8);
    integer(f,b,8);
    }
static void zeros(FILE *f,int n) { while(n--)fputc(0,f);
    }
static void vertex(FILE *f,const geo_vert *v)
{
    integer(f,(uint32_t)v->sx16,4);
    integer(f,(uint32_t)v->sy16,4);
    integer(f,(uint32_t)v->z,4);
    integer(f,v->u,4);
    integer(f,v->v,4);
    integer(f,(uint32_t)v->bri,4);
    integer(f,(uint32_t)v->valid,4);
    integer(f,(uint32_t)v->uf,4);
    integer(f,(uint32_t)v->vf,4);
    integer(f,(uint32_t)v->bf,4);
    for(int i=0;i<3;i++)real64(f,v->acn_view[i]);
}
static int fog(const geo_quad *q,eng_fog *out) { (void)q;
    *out=material;
    return current_case==1;
    }
static int snapshot(const char *name)
{
    FILE *f=fresh(name);
    if(!f)return 0;
    fwrite("ACN22-SNAPSHOT-V1",1,18,f);
    integer(f,g_eng_frame,8);
    integer(f,640,4);
    integer(f,480,4);
    real64(f,400);
    real64(f,400);
    real64(f,320);
    real64(f,240);
    fputc(12,f);
    fputc(18,f);
    fputc(28,f);
    int flags[]={config.shade,config.fog,config.fog_before_shade,config.flat_white,config.write_prio_alpha,
      config.texel_centre,config.wide_backdrop,config.depth_test,g_tex_opaque,g_eng_tex_tile16,g_tex_clipbox,
      g_eng_degen_uv_legacy,g_tex_pow2sample,g_tex_pow2alloc,tex_bake_atlas_active()};
    for(int i=0;i<15;i++)fputc(flags[i],f);
    integer(f,(uint32_t)g_tex_fixedcap,4);
    fputc(0,f);
    real32(f,g_scene_x0);
    real32(f,g_scene_x1);
    fputc(3,f);
    const size_t sizes[]={sizeof direct_palette,TEXTURE_TOTAL_SIZE,TEXTUREMAP_SIZE};
    for(int i=0;i<3;i++){fputc(i+1,f);
        integer(f,sizes[i],8);
        zeros(f,32);
        } /* UNHASHED diagnostic holes */
    integer(f,(uint32_t)count,4);
    for(int i=0;i<count;i++){
        const geo_quad *q=scene+i;
        integer(f,(uint32_t)q->nrv,4);
        integer(f,(uint32_t)q->ndv,4);
        integer(f,q->color,4);
        int meta[]={q->texbank,q->cmode,q->cz_type,q->zsort,q->cz_adjust,q->flags_raw,q->cz_value,q->behind,q->order,q->pick_code};
        for(int j=0;j<10;j++)integer(f,(uint32_t)meta[j],4);
        for(int j=0;j<4;j++)integer(f,(uint32_t)q->clip[j],4);
        for(int j=0;j<4;j++)integer(f,q->uvbox[j],2);
        integer(f,(uint32_t)q->objectflags,4);
        integer(f,(uint32_t)q->direct,4);
        for(int j=0;j<4;j++)vertex(f,q->v+j);
        for(int j=0;j<4;j++)vertex(f,q->rv+j);
        eng_fog resolved={0};
        resolved.alpha_const=255;
        int applies=config.fog && config.fog_quad && config.fog_quad(q,&resolved);
        fputc(applies,f);
        fputc(resolved.tab!=NULL,f);
        fwrite(resolved.rgb,1,3,f);
        integer(f,(uint32_t)resolved.sdelta,4);
        integer(f,(uint32_t)resolved.alpha_const,4);
        zeros(f,32);
    }
    int ok=!ferror(f) && ftell(f)==225+641*count;
    return fclose(f)==0 && ok;
}
static void matrix(FILE *f,const float *m)
{ fputc('[',f);
    for(int i=0;i<16;i++)fprintf(f,"%s\"%08x\"",i?",":"",acn22_fbits(m[i]));
    fputc(']',f);
    }
static int stamp(int i,proof_stamp *s)
{
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT,vp);
    if(!lease_active || vp[0]!=eyes[i].rect_x || vp[1]!=eyes[i].rect_y || vp[2]!=640 || vp[3]!=480)return 0;
    s->lease_id=lease;
    s->frame=scene_frame;
    s->counter=g_eng_frame;
    s->seq=sequence;
    s->target=target_generation;
    memcpy(s->view,eyes[i].view_from_scene,sizeof s->view);
    memcpy(s->projection,eyes[i].projection_from_view,sizeof s->projection);
    return 1;
}
static void stamp_json(FILE *f,const proof_stamp *s)
{
    fprintf(f,"{\"lease\":%llu,\"frame\":%llu,\"native_frame_counter\":%llu,\"sequence\":%llu,\"target_generation\":%llu,\"view_f32\":",
      (unsigned long long)s->lease_id,(unsigned long long)s->frame,(unsigned long long)s->counter,
      (unsigned long long)s->seq,(unsigned long long)s->target);
    matrix(f,s->view);
    fprintf(f,",\"projection_f32\":");
    matrix(f,s->projection);
    fputc('}',f);
}
static int metadata(const char *name,int phase,int after)
{
    FILE *f=fresh(name);
    if(!f)return 0;
    fprintf(f,"{\"case\":%d,\"phase\":%d,\"lease\":%llu,\"frame\":%llu,\"sequence\":%llu,"
      "\"native_frame_counter\":%u,\"target_generation\":%llu,\"pid\":%lu,\"qpc_frequency\":%llu,"
      "\"readback_qpc\":%llu,\"bound_snapshot_sha256\":\"%s\",\"bound_composition_sha256\":\"%s\",\"scale_f32\":\"447a0000\",\"head_f32\":",current_case,phase,
      (unsigned long long)lease,(unsigned long long)scene_frame,(unsigned long long)sequence,g_eng_frame,(unsigned long long)target_generation,
      (unsigned long)GetCurrentProcessId(),(unsigned long long)frequency.QuadPart,(unsigned long long)readback_qpc,bound_snapshot,bound_composition);
    matrix(f,head);
    fprintf(f,",\"anchor_f32\":");
    matrix(f,anchor);
    fprintf(f,",\"projection_f32\":");
    matrix(f,projection);
    fprintf(f,",\"eyes\":[");
    for(int i=0;i<2;i++){
        fprintf(f,"%s{\"eye_index\":%u,\"rect\":[%d,%d,%u,%u],\"view_f32\":",i?",":"",
          eyes[i].eye_index,eyes[i].rect_x,eyes[i].rect_y,eyes[i].rect_width,eyes[i].rect_height);
        matrix(f,eyes[i].view_from_scene);
        fprintf(f,",\"eye_from_head_f32\":");
        matrix(f,eye_offset[i]);
        fprintf(f,",\"entry_frame\":%llu,\"exit_frame\":%llu,\"entry_state\":",
          (unsigned long long)(after?entry_frame[i]:0),(unsigned long long)(after?exit_frame[i]:0));
        if(after)stamp_json(f,entries+i);
        else fprintf(f,"null");
        fprintf(f,",\"exit_state\":");
        if(after)stamp_json(f,exits+i);
        else fprintf(f,"null");
        fputc('}',f);
    }
    fprintf(f,"]}\n");
    int ok=!ferror(f);
    return fclose(f)==0 && ok;
}
static int cache_dump(const char *name)
{ FILE *f=fresh(name);
    if(!f)return 0;
    int ok=acn22_proof_cache_dump(f);
    return fclose(f)==0 && ok;
    }
static int resources_same(void)
{ return same("palette.bin",direct_palette,sizeof direct_palette) && same("texture.bin",g_texture_data,TEXTURE_TOTAL_SIZE) && same("tilemap.bin",g_texture_tilemap,TEXTUREMAP_SIZE) && same("fog.bin",table,sizeof table);
    }
static int scene_same(int eye)
{
    char original[64],current[80],path[_MAX_PATH];
    uint8_t bytes[225+641*2];
    snprintf(original,sizeof original,"scene_%llu.unhashed",(unsigned long long)lease);
    snprintf(current,sizeof current,"pair_%llu.eye_%d.check.unhashed",(unsigned long long)sequence,eye);
    if(!snapshot(current) || snprintf(path,sizeof path,"%s/%s",root,current)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    size_t n=(size_t)(225+641*count);
    int ok=fread(bytes,1,n,f)==n && fgetc(f)==EOF && !ferror(f);
    if(fclose(f)!=0)ok=0;
    return ok && same(original,bytes,n);
}
static void point(geo_vert *v,int x,int y,int z,int uf,int vf)
{
    memset(v,0,sizeof *v);
    v->valid=1;
    v->acn_view[0]=x;
    v->acn_view[1]=y;
    v->acn_view[2]=z;
    v->z=z;
    v->sx16=(int32_t)llround((320+400.0*x/z)*16);
    v->sy16=(int32_t)llround((240-400.0*y/z)*16);
    v->uf=uf;
    v->vf=vf;
    v->u=(uint32_t)(uf>>16);
    v->v=(uint32_t)(vf>>16);
    v->bri=64;
    v->bf=64*65536;
}
static void quad(geo_quad *q,int group,int x,int y,int z,int hw,int hh,int order)
{
    memset(q,0,sizeof *q);
    q->nrv=4;
    q->color=(64u<<16)|((uint32_t)group<<8);
    q->order=order;
    q->zsort=z;
    q->clip[1]=639;
    q->clip[3]=479;
    q->uvbox[1]=q->uvbox[3]=63;
    for(int i=0;i<4;i++){point(q->v+i,x+((i==1||i==2)?hw:-hw),y+(i>=2?-hh:hh),z,
        (i==1||i==2)?63*65536:0,i>=2?63*65536:0);
        q->rv[i]=q->v[i];
        }
}
static int acquire(int which)
{
    if(lease_active || acn22_native_active())return 0;
    current_case=which;
    lease++;
    g_eng_frame++;
    scene_frame=g_eng_frame;
    lease_active=1;
    count=which?1:2;
    memset(&config,0,sizeof config);
    config.shade=1;
    config.texel_centre=1;
    config.fog=which;
    config.fog_quad=which?fog:NULL;
    if(!which){quad(scene,1,-300,150,1536,64,48,0);
        quad(scene+1,2,600,-250,4096,256,128,1);
        eng_quad_sort(scene,2,0);
        }
    else {
        quad(scene,6,0,0,1024,1,1,0);
        scene[0].zsort=8000;
        scene[0].cz_type=2;
        scene[0].uvbox[1]=255;
        scene[0].uvbox[3]=127;
        for(int i=0;i<4;i++){point(scene[0].v+i,proof_cz_xyz[i][0],proof_cz_xyz[i][1],proof_cz_xyz[i][2],proof_cz_uv[i][0],proof_cz_uv[i][1]);
            scene[0].rv[i]=scene[0].v[i];
            }
    }
    char name[64];
    snprintf(name,sizeof name,"scene_%llu.unhashed",(unsigned long long)lease);
    return acn22_proof_prime(scene,count,which?&material:NULL) && snapshot(name) && resources_same();
}
static void inputs(int phase)
{
    const uint32_t *h=current_case?proof_head_cz:(phase?proof_head_1:proof_head_0);
    const uint32_t *p=current_case?proof_projection_cz:(phase?proof_projection_1:proof_projection_0);
    floats(head,h);
    floats(anchor,proof_anchor_bits);
    floats(projection,p);
    for(int i=0;i<2;i++){
        memset(eyes+i,0,sizeof eyes[i]);
        ACVR_INIT(eyes+i);
        eyes[i].eye_index=(uint32_t)i;
        eyes[i].rect_x=i*640;
        eyes[i].rect_width=640;
        eyes[i].rect_height=480;
        const uint32_t *v=current_case?(i?proof_view_cz_1:proof_view_cz_0):
          (phase?(i?proof_view_1_1:proof_view_1_0):(i?proof_view_0_1:proof_view_0_0));
        floats(eyes[i].view_from_scene,v);
        floats(eyes[i].projection_from_view,p);
        floats(eye_offset[i],i?proof_eye_1:proof_eye_0);
    }
}
static int command(const char *wanted,char *digest,char *composition)
{
    char line[192];
    size_t n=0;
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
    while(alive()){
        SDL_Event e;
        while(SDL_PollEvent(&e))if(e.type==SDL_QUIT)return 0;
        DWORD available=0;
        if(!PeekNamedPipe(input,NULL,0,NULL,&available,NULL))return 0;
        if(!available){Sleep(1);
            continue;
            }char c;
        DWORD got=0;
        if(!ReadFile(input,&c,1,&got,NULL) || got!=1)return 0;
        if(c=='\n'){
            line[n]=0;
            size_t prefix=strlen(wanted);
            size_t required=prefix+(composition?130:65);
            if(n!=required || memcmp(line,wanted,prefix) || line[prefix]!=' ')return 0;
            for(size_t i=prefix+1;i<n;i++){
                if(composition && i==prefix+65){if(line[i]!=' ')return 0;
                    continue;
                    }
                if(!((line[i]>='0' && line[i]<='9') || (line[i]>='a' && line[i]<='f')))return 0;
            }
            memcpy(digest,line+prefix+1,64);
            digest[64]=0;
            if(composition){memcpy(composition,line+prefix+66,64);
                composition[64]=0;
                }return 1;
        }
        if(c=='\r')continue;
        if(n+1>=sizeof line)return 0;
        line[n++]=c;
    }
    return 0;
}
static int pair(SDL_Window *window,int phase)
{
    int width,height;
    SDL_GL_GetDrawableSize(window,&width,&height);
    if(!lease_active || width!=1280 || height!=480 || (SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED))return 0;
    inputs(phase);
    sequence++;
    readback_qpc=0;
    bound_snapshot[0]=0;
    bound_image[0]=0;
    bound_composition[0]=0;
    for(int i=0;i<2;i++)if(acn22_pose_stage(scene,(uint32_t)count,eyes+i,packets[i],4,packet_count+i)!=ACVR_OK || !packet_count[i])return 0;
    char name[80],ack[80];
    snprintf(name,sizeof name,"pair_%llu.before.json",(unsigned long long)sequence);
    if(!metadata(name,phase,0))return 0;
    snprintf(name,sizeof name,"pair_%llu.cache_before.json",(unsigned long long)sequence);
    if(!cache_dump(name))return 0;
    printf("ACN22/1 PREFLIGHT %llu\n",(unsigned long long)sequence);
    fflush(stdout);
    snprintf(ack,sizeof ack,"DRAW %llu",(unsigned long long)sequence);
    if(!command(ack,bound_snapshot,bound_composition) || !resources_same() || !acn22_proof_cache_ready())return 0;
    for(int i=0;i<2;i++){
        if(!resources_same() || !scene_same(i) || !acn22_proof_cache_ready())return 0;
        entry_frame[i]=g_eng_frame;
        glViewport(i*640,0,640,480);
        glEnable(GL_SCISSOR_TEST);
        glScissor(i*640,0,640,480);
        glClearColor(12.f/255,18.f/255,28.f/255,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        eng_draw_begin();
        int ok=stamp(i,entries+i);
        for(uint32_t k=0;k<packet_count[i] && ok;k++)ok=acn22_native_draw_fragment(packets[i]+k,&config)==ACVR_OK;
        eng_draw_end();
        exit_frame[i]=g_eng_frame;
        if(!stamp(i,exits+i))ok=0;
        if(!ok || entry_frame[i]!=lease || exit_frame[i]!=lease || glGetError()!=GL_NO_ERROR || !acn22_proof_cache_ready())return 0;
    }
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,1280,480,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    if(glGetError()!=GL_NO_ERROR || !resources_same())return 0;
    readback_qpc=qpc();
    for(int y=0;y<480;y++)memcpy(export_rgb+y*1280*3,rgb+(479-y)*1280*3,1280*3);
    snprintf(name,sizeof name,"pair_%llu.rgb",(unsigned long long)sequence);
    if(!blob(name,export_rgb,1280*480*3))return 0;
    snprintf(name,sizeof name,"pair_%llu.after.json",(unsigned long long)sequence);
    if(!metadata(name,phase,1))return 0;
    snprintf(name,sizeof name,"pair_%llu.cache_after.json",(unsigned long long)sequence);
    if(!cache_dump(name))return 0;
    snprintf(name,sizeof name,"pair_%llu.after.unhashed",(unsigned long long)sequence);
    if(!snapshot(name))return 0;
    printf("ACN22/1 IMAGE %llu\n",(unsigned long long)sequence);
    fflush(stdout);
    snprintf(ack,sizeof ack,"SWAP %llu",(unsigned long long)sequence);
    if(!command(ack,bound_image,NULL))return 0;
    SDL_GL_GetDrawableSize(window,&width,&height);
    if(width!=1280 || height!=480 || !resources_same())return 0;
    SDL_GL_SwapWindow(window);
    snprintf(name,sizeof name,"pair_%llu.swap.json",(unsigned long long)sequence);
    FILE *sw=fresh(name);
    if(!sw)return 0;
    fprintf(sw,"{\"pair_sequence\":\"%llu\",\"lease_id\":\"%llu\",\"scene_frame_id\":\"%llu\",\"target_generation\":\"%llu\","
       "\"image_rgb_sha256\":\"%s\",\"counter_qpc\":\"%llu\",\"clock_domain\":\"producer-qpc\",\"clock_frequency\":\"%llu\",\"presentation\":\"swap-requested\"}\n",
       (unsigned long long)sequence,(unsigned long long)lease,(unsigned long long)scene_frame,(unsigned long long)target_generation,
       bound_image,(unsigned long long)qpc(),(unsigned long long)frequency.QuadPart);
    if(fclose(sw)!=0)return 0;
    printf("ACN22/1 SWAPPED %llu\n",(unsigned long long)sequence);
    fflush(stdout);
    uint64_t end=GetTickCount64()+1100;
    while(alive() && GetTickCount64()<end){SDL_Event e;
        while(SDL_PollEvent(&e))if(e.type==SDL_QUIT)return 0;
        Sleep(5);
        }
    return alive();
}
static int release(const char *role)
{
    if(!lease_active || acn22_native_active())return 0;
    lease_active=0;
    packet_count[0]=packet_count[1]=0;
    char name[64];
    snprintf(name,sizeof name,"release_%llu.json",(unsigned long long)lease);
    FILE *f=fresh(name);
    if(!f)return 0;
    fprintf(f,"{\"role_id\":\"%s\",\"lease_id\":\"%llu\",\"lease_generation\":\"%llu\",\"scene_frame_id\":\"%llu\","
      "\"native_frame_counter\":\"%u\",\"last_pair_sequence\":\"%llu\",\"counter_qpc\":\"%llu\","
      "\"clock_domain\":\"producer-qpc\",\"clock_frequency\":\"%llu\",\"invalidated\":true}\n",role,
      (unsigned long long)lease,(unsigned long long)lease,(unsigned long long)scene_frame,g_eng_frame,
      (unsigned long long)sequence,(unsigned long long)qpc(),(unsigned long long)frequency.QuadPart);
    return fclose(f)==0;
}
int main(int argc,char **argv)
{
    if(!ACN22_NATIVE_GPU_PROOF || argc!=3 || strcmp(argv[1],"--paired-proof"))return 2;
    if(sizeof(float)!=4 || sizeof(double)!=8 || strlen(argv[2])>180 || !_fullpath(root,argv[2],sizeof root) || !strstr(root,"\\.local\\"))return 2;
    deadline=GetTickCount64()+14000;
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0)return 2;
    int result=1;
    SDL_Window *window=NULL;
    SDL_GLContext context=NULL;
    g_texture_data=calloc(TEXTURE_TOTAL_SIZE,1);
    g_texture_tilemap=calloc(TEXTUREMAP_SIZE,1);
    rgb=malloc(1280*480*3);
    export_rgb=malloc(1280*480*3);
    if(!g_texture_data || !g_texture_tilemap || !rgb || !export_rgb)goto done;
    memset(g_texture_data,2,256);
    memset(direct_palette,0,sizeof direct_palette);
    memset(g_eng_pal_gen,0,sizeof g_eng_pal_gen);
    direct_palette_loaded=1;
    const uint8_t colors[3][3]={{30,155,245},{240,160,35},{80,100,120}};
    const int groups[]={1,2,6};
    for(int i=0;i<3;i++)memcpy(direct_palette[groups[i]][2],colors[i],3);
    table[4]=130;
    table[5]=250;
    material.tab=table;
    material.alpha_const=-1;
    material.rgb[0]=10;
    material.rgb[1]=20;
    material.rgb[2]=30;
    if(!blob("palette.bin",direct_palette,sizeof direct_palette) || !blob("texture.bin",g_texture_data,TEXTURE_TOTAL_SIZE) ||
       !blob("tilemap.bin",g_texture_tilemap,TEXTUREMAP_SIZE) || !blob("fog.bin",table,sizeof table))goto done;
    g_eng_frame=0;
    g_scene_x0=0;
    g_scene_x1=640;
    g_eng_quad_dx=NULL;
    g_eng_tex_tile16=0;
    g_tex_clipbox=0;
    g_eng_degen_uv_legacy=0;
    g_perf_enabled=0;
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)!=0)goto done;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0);
    window=SDL_CreateWindow(ACN22_FIXTURE_TITLE,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,480,SDL_WINDOW_OPENGL);
    if(!window || !(context=SDL_GL_CreateContext(window)))goto done;
    target_generation++;
    int width,height;
    SDL_GL_GetDrawableSize(window,&width,&height);
    if(width!=1280 || height!=480)goto done;
    int profile,major,minor,bits[4],dbl,samples;
    if(SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,&profile) || profile!=SDL_GL_CONTEXT_PROFILE_COMPATIBILITY ||
       SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major) || SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor) ||
       major<3 || (major==3 && minor<3) || SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,&dbl) || dbl!=1 ||
       SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS,&samples) || samples!=0)goto done;
    const SDL_GLattr attrs[]={SDL_GL_RED_SIZE,SDL_GL_GREEN_SIZE,SDL_GL_BLUE_SIZE,SDL_GL_ALPHA_SIZE};
    for(int i=0;i<4;i++)if(SDL_GL_GetAttribute(attrs[i],bits+i) || bits[i]!=8)goto done;
    SDL_version version;
    SDL_GetVersion(&version);
    if(version.major!=2 || version.minor!=32 || version.patch!=10)goto done;
    char dll_path[_MAX_PATH];
    DWORD dll_size=GetModuleFileNameA(GetModuleHandleA("SDL2.dll"),dll_path,sizeof dll_path);
    if(!dll_size || dll_size>=sizeof dll_path || !blob("loaded_sdl_path.txt",dll_path,dll_size))goto done;
    FILE *device=fresh("device.json");
    if(!device)goto done;
    fprintf(device,"{\"width\":%d,\"height\":%d,\"color_bits\":[%d,%d,%d,%d],\"doublebuffer\":%d,"
      "\"multisamplebuffers\":%d,\"profile\":%d,\"major\":%d,\"minor\":%d,\"window_id\":%u,\"pid\":%lu}\n",
      width,height,bits[0],bits[1],bits[2],bits[3],dbl,samples,profile,major,minor,SDL_GetWindowID(window),(unsigned long)GetCurrentProcessId());
    if(fclose(device)!=0)goto done;
    glDisable(GL_DITHER);
    glDisable(0x8DB9);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,640,480,0,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(!acn22_proof_cache_configure() || glGetError()!=GL_NO_ERROR)goto done;
    if(!acquire(0) || !pair(window,0) || !pair(window,1) || !pair(window,0))goto done;
    /* Invalidate borrowed packet lease before acquiring the second. */
    if(!release("release-markers"))goto done;
    if(!acquire(1) || !pair(window,0) || !release("release-cz"))goto done;
    result=0;
done:
    lease_active=0;
    packet_count[0]=packet_count[1]=0;
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();
    free(g_texture_data);
    free(g_texture_tilemap);
    free(rgb);
    free(export_rgb);
    printf("ACN22/1 EXIT %d\n",result);
    fflush(stdout);
    return result;
}
