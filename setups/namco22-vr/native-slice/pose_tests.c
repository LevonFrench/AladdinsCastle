/* Adversarial synthetic stage tests. No GL initialization or content loading. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "pose_stage.h"
#include "quad_gl.h"

static unsigned checks,failures;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;} } while(0)
static int close_to(double a,double b){return fabs(a-b)<1e-8*(1+fabs(b));}
static acvr_eye eye(double yaw,double tz,double scale)
{
    acvr_eye e={0};ACVR_INIT(&e);e.rect_width=720;e.rect_height=640;
    float *m=e.view_from_scene;
    m[0]=(float)(cos(yaw)*scale);m[2]=(float)(-sin(yaw)*scale);
    m[5]=(float)scale;m[8]=(float)(sin(yaw)*scale);m[10]=(float)(cos(yaw)*scale);
    m[14]=(float)tz;m[15]=1;
    float *p=e.projection_from_view;p[0]=1;p[5]=1;
    p[10]=-21.0f/19;p[11]=-1;p[14]=-40.0f/19;
    return e;
}
static geo_quad quad(void)
{
    geo_quad q={0};q.nrv=4;q.clip[1]=639;q.clip[3]=479;
    q.zsort=900;q.order=7;q.color=0x400600;q.cz_adjust=123;q.cz_type=2;
    for(int i=0;i<4;i++) {
        int right=i==1 || i==2;
        geo_vert v={0};v.valid=1;
        v.acn_view[0]=right?0.6:-0.4;v.acn_view[1]=i>=2?0.25:-0.25;
        v.z=right?6:2;v.acn_view[2]=v.z;
        v.uf=(int32_t)((right?128.75:0.25)*65536);
        v.vf=(int32_t)((i>=2?63.5:0.5)*65536);
        v.bf=(right?96:32)*65536;
        v.u=(uint32_t)(v.uf>>16);v.v=(uint32_t)(v.vf>>16);v.bri=v.bf>>16;
        q.v[i]=q.rv[i]=v;
    }
    return q;
}
static double distance_plane(const acn22_pose_vertex *v,int p)
{
    return v->clip[3]+((p%2)?-v->clip[p/2]:v->clip[p/2]);
}
static void verify_packet(const acn22_pose_fragment *f)
{
    CHECK(f->count>=3 && f->count<=9);
    for(uint32_t i=0;i<f->count;i++) {
        const acn22_pose_vertex *v=f->v+i;
        CHECK(v->clip[3]>0 && isfinite(v->clip[3]));
        for(int p=0;p<6;p++)CHECK(distance_plane(v,p)>-1e-8*(1+v->clip[3]));
        double pos[4],tex[4];CHECK(acn22_pose_pack(v,pos,tex)==ACVR_OK);
        /* Independent native glOrtho multiply gives exactly original clip. */
        CHECK(close_to(2*pos[0]/640-pos[3],v->clip[0]));
        CHECK(close_to(-2*pos[1]/480+pos[3],v->clip[1]));
        CHECK(close_to(-pos[2],v->clip[2]));CHECK(pos[3]==v->clip[3]);
        CHECK(tex[0]==v->uv[0] && tex[1]==v->uv[1] && tex[2]==0 && tex[3]==1);
    }
}
static void near_crossing(void)
{
    geo_quad q=quad(),saved=q;acvr_eye e=eye(0,1.5,1);
    acn22_pose_fragment f[2];uint32_t n=99;
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2);
    unsigned intersections=0;
    for(uint32_t k=0;k<n;k++) {
        verify_packet(f+k);CHECK(f[k].native==&q);CHECK(f[k].fan_index==k);
        CHECK(f[k].painter_key==900 && f[k].emission_order==7);
        for(uint32_t i=0;i<f[k].count;i++) {
            acn22_pose_vertex *v=&f[k].v[i];
            if(fabs(distance_plane(v,4))<1e-8) {
                intersections++;
                /* Original Z2->6, translated eye nearZ1 => boardZ2.5.
                 * One edge t=1/8 clips UV and brightness together. */
                CHECK(fabs(v->board_z-2.5)<1e-6);
                CHECK(fabs(v->uv[0]-16.3125)<1e-4);
                CHECK(fabs(v->brightness-40)<1e-5);
                CHECK(fabs(v->clip[3]-1)<1e-6);
            }
        }
    }
    CHECK(intersections>=3);CHECK(memcmp(&q,&saved,sizeof q)==0);
    acn22_pose_fragment replay[2];uint32_t rn;
    CHECK(acn22_pose_stage(&q,1,&e,replay,2,&rn)==ACVR_OK);
    CHECK(rn==n && memcmp(replay,f,n*sizeof *f)==0);
    /* An original vertex is now behind eye W0; homogeneous clipping works. */
    e=eye(0,3,1);CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2);
    for(uint32_t k=0;k<n;k++)verify_packet(f+k);
    e=eye(0,10,1);CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==0);
}
static void perspective_attributes(void)
{
    acn22_pose_vertex t[3]={0};const double ws[3]={1,2,4};
    const double zs[3]={512,1024,2048};const double lambda[3]={0.2,0.3,0.5};
    for(int i=0;i<3;i++) {
        t[i].clip[3]=ws[i];t[i].clip[2]=ws[i]*(i==0?-1:(i==1?0.05:0.57));
        t[i].brightness=32*(i+1);t[i].board_z=zs[i];
    }
    t[1].uv[0]=120;t[2].uv[1]=240;
    acn22_pose_sample s;CHECK(acn22_pose_interpolate(t,lambda,&s)==ACVR_OK);
    /* Rational object weights8/19,6/19,5/19 independently give attributes. */
    CHECK(close_to(s.uv[0],720.0/19));CHECK(close_to(s.uv[1],1200.0/19));
    CHECK(close_to(s.board_z,20480.0/19));CHECK(close_to(s.brightness,1120.0/19));
    CHECK(close_to(s.ndc_z,0.1));
    double double_num=0,double_den=0;
    for(int i=0;i<3;i++){double_num+=lambda[i]*t[i].uv[0]/(ws[i]*zs[i]);double_den+=lambda[i]/(ws[i]*zs[i]);}
    CHECK(fabs(s.uv[0]-double_num/double_den)>5);
    uint8_t tab[8192]={0};tab[4]=130;tab[5]=250;
    eng_fog fog={0};fog.tab=tab;fog.alpha_const=-1;
    CHECK(eng_fog_alpha(&fog,(int32_t)s.board_z)==125);
    CHECK(eng_fog_alpha(&fog,(int32_t)(0.2*512+0.3*1024+0.5*2048))==5);
    CHECK(eng_fog_alpha(&fog,(int32_t)(1/0.475))==255);
    double interpolated_alpha=(8*eng_fog_alpha(&fog,512)+6*eng_fog_alpha(&fog,1024)+5*eng_fog_alpha(&fog,2048))/19.0;
    CHECK(fabs(interpolated_alpha-125)>80);
    /* Brightness and board depth carried through source clipping are still
     * rationally correct on an interior point of the new clipped triangle. */
    geo_quad q=quad();acvr_eye e=eye(0,1.5,1);acn22_pose_fragment f[2];uint32_t n;
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);
    if(n) {
        acn22_pose_vertex tri[3]={f[0].v[0],f[0].v[1],f[0].v[2]};
        CHECK(acn22_pose_interpolate(tri,lambda,&s)==ACVR_OK);
        CHECK(close_to(s.uv[0],0.25+(s.board_z-2)*128.5/4));
        CHECK(close_to(s.brightness,32+(s.board_z-2)*64/4));
    }
}
static void order_and_failures(void)
{
    geo_quad q[3]={quad(),quad(),quad()};q[0].order=0;q[1].order=1;q[2].order=2;q[0].zsort=8000;
    eng_quad_sort(q,3,0);CHECK(q[0].order==0 && q[1].order==2 && q[2].order==1);
    geo_quad saved[3];memcpy(saved,q,sizeof q);
    acvr_eye e=eye(0.35,1,1);e.projection_from_view[8]=0.11f;
    acn22_pose_fragment out[6],sentinel[6];uint32_t n;
    CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_OK);CHECK(n==6);
    for(uint32_t i=0;i<n;i++) {
        CHECK(out[i].source_index==i/2 && out[i].fan_index==i%2);
        CHECK(out[i].emission_order==q[i/2].order && out[i].painter_key==q[i/2].zsort);
        verify_packet(out+i);
    }
    CHECK(memcmp(q,saved,sizeof q)==0);
    memset(out,0xa5,sizeof out);memcpy(sentinel,out,sizeof out);
    n=99;CHECK(acn22_pose_stage(q,3,&e,out,1,&n)==ACVR_BAD_ARGUMENT);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);
    q[2].direct=1;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);q[2]=saved[2];
    q[2].rv[0].acn_view[0]=NAN;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);q[2]=saved[2];
    e.view_from_scene[0]=2;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    e=eye(0,0,1);e.view_from_scene[0]=-1;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    e=eye(0,0,1);e.view_from_scene[4]=0.2f;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    e=eye(0,0,1);e.view_from_scene[14]=NAN;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_BAD_ARGUMENT);
    e=eye(0,0,1);e.size=8;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_BAD_ARGUMENT);
    e=eye(0,0,1);e.version=99;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_BAD_VERSION);
    e=eye(0,0,1);CHECK(acn22_pose_stage(q,ACN22_POSE_MAX_QUADS+1,&e,out,6,&n)==ACVR_BAD_ARGUMENT);
    q[2]=saved[2];q[2].ndv=4;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    q[2]=saved[2];q[2].behind=1;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    q[2]=saved[2];q[2].clip[1]=200;CHECK(acn22_pose_stage(q,3,&e,out,6,&n)==ACVR_UNSUPPORTED);
    CHECK(memcmp(out,sentinel,sizeof out)==0);
}
static void boundaries_and_sweep(void)
{
    geo_quad q=quad();acvr_eye e=eye(0,0,1);acn22_pose_fragment f[2];uint32_t n;
    /* Far plane crosses original Z2..6 with a far plane at4. */
    e.projection_from_view[10]=-5.0f/3;e.projection_from_view[14]=-8.0f/3;
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2);
    for(uint32_t i=0;i<n;i++)verify_packet(f+i);
    /* Exact near and side boundary vertices stay finite and deduplicated. */
    for(int i=0;i<4;i++) {
        q.v[i].z=q.rv[i].z=1;q.v[i].acn_view[2]=q.rv[i].acn_view[2]=1;
        q.v[i].acn_view[0]=q.rv[i].acn_view[0]=(i==1||i==2)?1:-1;
        q.v[i].acn_view[1]=q.rv[i].acn_view[1]=i>=2?1:-1;
    }
    e=eye(0,0,1);e.projection_from_view[10]=-1;e.projection_from_view[14]=-2; /* infinite far */
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2);
    for(uint32_t i=0;i<n;i++)verify_packet(f+i);
    q=quad();e=eye(0.1,0,0.001);e.projection_from_view[14]=-0.001f;e.projection_from_view[10]=-1;
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2);
    for(uint32_t i=0;i<n;i++)verify_packet(f+i);
    /* Deterministic adversarial pose/side-clip sweep, no random dependencies. */
    unsigned visible=0,hidden=0;
    for(int k=0;k<121;k++) {
        e=eye((k-60)/37.0,(k%11-5)*1.4,1);
        e.view_from_scene[12]=(float)((k%7-3)*2.0);
        e.view_from_scene[13]=(float)((k%5-2)*1.2);
        e.projection_from_view[8]=(float)((k%3-1)*0.13);
        CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);
        if(n)visible++;else hidden++;
        for(uint32_t i=0;i<n;i++)verify_packet(f+i);
    }
    CHECK(visible>20 && hidden>20);
}
int main(void)
{
    near_crossing();perspective_attributes();order_and_failures();boundaries_and_sweep();
    printf("Synthetic headpose clipping: %u checks, %u failures; no GL/context/game\n",checks,failures);
    return failures?1:0;
}
