/* SPDX-License-Identifier: GPL-3.0-only. CPU-only; no native GL/context call. */
#include "native_display.h"
#include "native_fixture.h"
#include "paired_oracle.h"
#include <stdio.h>
#include <string.h>
static int failures;
static void check(int ok,const char *name){if(!ok){fprintf(stderr,"FAIL: %s\n",name);++failures;}}
static void words(float *out,const uint32_t *in){memcpy(out,in,16*sizeof(uint32_t));}
static void records(acvr_draw_info d[2],acvr_eye e[2],int cz)
{
    memset(d,0,2*sizeof *d);memset(e,0,2*sizeof *e);
    for(int i=0;i<2;++i){
        ACVR_INIT(d+i);ACVR_INIT(&d[i].target);ACVR_INIT(e+i);
        d[i].frame_id=1;d[i].display_id=1;d[i].predicted_display_time_ns=1;
        d[i].target.api=ACVR_GRAPHICS_GL;d[i].target.width=640;d[i].target.height=480;
        d[i].target.array_layers=d[i].target.sample_count=1;d[i].target.colour_format=0x8058;
        d[i].target.framebuffer=10+i;d[i].target.colour_image=20+i;
        d[i].view_count=1;d[i].view_stride=sizeof *e;d[i].views=e+i;
        e[i].eye_index=(uint32_t)i;e[i].rect_width=640;e[i].rect_height=480;
        words(e[i].view_from_scene,cz?(i?proof_view_cz_1:proof_view_cz_0):(i?proof_view_0_1:proof_view_0_0));
        words(e[i].projection_from_view,cz?proof_projection_cz:proof_projection_0);
    }
}
int main(void)
{
    geo_quad q[2];acvr_draw_info d[2];acvr_eye e[2];acn22_display_pair p;
    int count=acn22_fixture_scene(0,q);records(d,e,0);
    memset(&p,0xa5,sizeof p);const acn22_display_pair sentinel=p;
    d[1].target.sample_count=2;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_UNSUPPORTED &&
          !memcmp(&p,&sentinel,sizeof p),"bad right target leaves whole output untouched");
    records(d,e,0);e[1].projection_from_view[0]=0;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)!=ACVR_OK &&
          !memcmp(&p,&sentinel,sizeof p),"bad right projection leaves output untouched");
    records(d,e,0);d[1].display_id=2;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_BAD_ARGUMENT,"display mismatch rejected");
    records(d,e,0);d[1].target.framebuffer=d[0].target.framebuffer;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_BAD_ARGUMENT,"aliased overlapping target rejected");
    e[1].rect_x=640;d[0].target.width=d[1].target.width=1280;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_BAD_ARGUMENT,"shared framebuffer refused even with disjoint rectangles");
    records(d,e,0);d[1].target.colour_image=d[0].target.colour_image;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_BAD_ARGUMENT,"shared colour image refused");
    records(d,e,0);
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_OK && p.prepared &&
          p.next_eye==0 && p.count[0]==4 && p.count[1]==4,"both marker packets staged on one lease");
    check(acn22_display_match(&p,d)==ACVR_OK,"first draw matches");
    check(acn22_display_match(&p,d+1)==ACVR_BAD_ARGUMENT,"right-before-left refused");
    d[0].target.colour_image++;
    check(acn22_display_match(&p,d)==ACVR_BAD_STATE,"target identity drift refused");
    records(d,e,0);p.next_eye=1;
    check(acn22_display_match(&p,d+1)==ACVR_OK,"second draw matches original preparation");
    p.prepared=0;check(acn22_display_match(&p,d+1)==ACVR_BAD_STATE,"invalidated pair refused");
    struct { acvr_eye eye;unsigned char tail[16]; } extended;
    records(d,e,0);extended.eye=e[0];memset(extended.tail,0xa5,sizeof extended.tail);
    extended.eye.size=sizeof extended;d[0].views=&extended.eye;d[0].view_stride=sizeof extended;
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_OK,"oversized eye accepted without tail retention");
    memset(extended.tail,0x5a,sizeof extended.tail);
    check(acn22_display_match(&p,d)==ACVR_OK,"unknown eye tail not treated as identity");
    count=acn22_fixture_scene(1,q);records(d,e,1);
    check(acn22_display_stage(q,count,1,2,sizeof *d,d,&p)==ACVR_OK && p.count[0] &&
          p.count[1] && p.packets[0][0].native==q,"same clipped-CZ source stages into both eyes");
    check(acn22_display_stage(q,count,2,2,sizeof *d,d,&p)==ACVR_BAD_STATE,"stale source frame refused");
    check(acn22_display_stage(q,count,1,1,sizeof *d,d,&p)==ACVR_BAD_ARGUMENT,"one-record preparation refused");
    return failures?1:0;
}
