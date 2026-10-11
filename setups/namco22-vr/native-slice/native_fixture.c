/* SPDX-License-Identifier: GPL-3.0-only */
#include "native_fixture.h"
#include "quad_gl.h"
#include "paired_oracle.h"
#include <math.h>
#include <string.h>
static void point(geo_vert *v,int x,int y,int z,int32_t uf,int32_t vf)
{
    memset(v,0,sizeof *v);v->valid=1;v->z=z;
    v->acn_view[0]=x;v->acn_view[1]=y;v->acn_view[2]=z;
    v->sx16=(int32_t)llround((320+400.0*x/z)*16);
    v->sy16=(int32_t)llround((240-400.0*y/z)*16);
    v->uf=uf;v->vf=vf;v->u=(uint32_t)(uf>>16);v->v=(uint32_t)(vf>>16);
    v->bri=64;v->bf=64*65536;
}
static void quad(geo_quad *q,int group,int x,int y,int z,int hw,int hh,int order)
{
    memset(q,0,sizeof *q);q->nrv=4;q->color=(64u<<16)|((uint32_t)group<<8);
    q->order=order;q->zsort=z;q->clip[1]=639;q->clip[3]=479;
    q->uvbox[1]=q->uvbox[3]=63;
    for(int i=0;i<4;++i){
        point(q->v+i,x+((i==1||i==2)?hw:-hw),y+(i>=2?-hh:hh),z,
            (i==1||i==2)?63*65536:0,i>=2?63*65536:0);
        q->rv[i]=q->v[i];
    }
}
int acn22_fixture_scene(int cz,geo_quad out[2])
{
    if(!out || (cz!=0 && cz!=1))return 0;
    memset(out,0,2*sizeof *out);
    if(!cz){quad(out,1,-300,150,1536,64,48,0);quad(out+1,2,600,-250,4096,256,128,1);
        eng_quad_sort(out,2,0);return 2;}
    quad(out,6,0,0,1024,1,1,0);out[0].zsort=8000;out[0].cz_type=2;
    out[0].uvbox[1]=255;out[0].uvbox[3]=127;
    for(int i=0;i<4;++i){
        point(out[0].v+i,proof_cz_xyz[i][0],proof_cz_xyz[i][1],proof_cz_xyz[i][2],
              proof_cz_uv[i][0],proof_cz_uv[i][1]);out[0].rv[i]=out[0].v[i];
    }
    return 1;
}
