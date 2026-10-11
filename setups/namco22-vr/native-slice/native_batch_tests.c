/* Actual patched native bb_emit CPU seam. No GL initialization or drawing. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "native_batch.h"
static unsigned checks,failures;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;} } while(0)
static int near(double a,double b){return fabs(a-b)<0.002;}
static geo_quad quad(void)
{
    geo_quad q={0};q.nrv=4;q.clip[1]=639;q.clip[3]=479;q.zsort=8000;q.order=7;
    q.color=0x400600;q.cz_type=2;
    q.uvbox[1]=255;q.uvbox[3]=127;
    for(int i=0;i<4;i++) {
        int right=i==1||i==2;geo_vert v={0};v.valid=1;v.z=right?1536:768;
        v.acn_view[0]=right?200:-100;v.acn_view[1]=i>=2?80:-80;v.acn_view[2]=v.z;
        v.uf=(int32_t)((right?255.25:0.25)*65536);v.vf=(int32_t)((i>=2?127.5:0.5)*65536);
        v.bf=(right?96:32)*65536;v.u=(uint32_t)(v.uf>>16);v.v=(uint32_t)(v.vf>>16);v.bri=v.bf>>16;
        v.sx16=100+i*120;v.sy16=200+i*80;q.v[i]=q.rv[i]=v;
    }
    return q;
}
static acvr_eye eye(void)
{
    acvr_eye e={0};ACVR_INIT(&e);e.rect_width=640;e.rect_height=480;
    double a=0.18;float *m=e.view_from_scene;
    m[0]=(float)cos(a);m[2]=(float)-sin(a);m[5]=1;m[8]=(float)sin(a);m[10]=(float)cos(a);
    m[12]=200;m[14]=600;m[15]=1;
    float *p=e.projection_from_view;p[0]=p[5]=1;p[10]=-(8192.0f+160)/(8192-160);
    p[11]=-1;p[14]=-2.0f*8192*160/(8192-160);return e;
}
static void attributes(const acn22_pose_fragment *f,float rgba[9][4],float st[9][4])
{
    for(uint32_t i=0;i<f->count;i++) {
        CHECK(acn22_native_uv(f,i,0,0,256,128,0.25f,0.5f,0.5f,0.5f,0.125f,0.25f,st[i])==ACVR_OK);
        for(int c=0;c<3;c++)rgba[i][c]=(float)(f->v[i].brightness/256*(0.8-0.1*c));
        rgba[i][3]=1;
    }
}
static unsigned tex_index(double s)
{
    /* Independent GL2.1 clamp-to-edge + nearest oracle, not a material sampler. */
    double low=0.5/8192,high=1-low;s=fmax(low,fmin(high,s));return (unsigned)floor(s*8192);
}
static void rotated_clipped(void)
{
    geo_quad q=quad(),snapshot=q;acvr_eye e=eye();acn22_pose_fragment f[2];uint32_t n;
    CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);CHECK(n==2 && f[0].count==4);
    if(n!=2)return;
    acn22_native_vertex batch[21],replay[21];float rgba[9][4]={0},st[9][4]={0};
    attributes(f,rgba,st);uint32_t count=0,rn=0;
    CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,batch,21,&count)==ACVR_OK);
    CHECK(count==(f[0].count-2)*3);
    CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,replay,21,&rn)==ACVR_OK);
    CHECK(rn==count && memcmp(batch,replay,count*sizeof *batch)==0);
    for(uint32_t i=0;i<count;i++) {
        int source=acn22_native_fan_index((int)(i/3+1),(int)(i%3));
        CHECK(near(batch[i].board[0]*ACN22_CZ_DIVISOR,f[0].v[source].board_z));
        CHECK(batch[i].primary[3]==1 && batch[i].board[3]==1);
        CHECK(batch[i].position[3]>0 && memcmp(batch[i].colour,rgba[source],sizeof *rgba)==0);
    }
    const double lambda[3]={0.2,0.3,0.5};double den=0,board=0,u=0,b=0;
    for(int i=0;i<3;i++) {
        double weight=lambda[i]/batch[i].position[3];den+=weight;
        board+=weight*batch[i].board[0];u+=weight*batch[i].primary[0];b+=weight*batch[i].colour[0];
    }
    board=board/den*ACN22_CZ_DIVISOR;u/=den;b/=den;double eye_w=1/den;
    acn22_pose_vertex tri[3]={f[0].v[0],f[0].v[1],f[0].v[2]};acn22_pose_sample s;
    CHECK(acn22_pose_interpolate(tri,lambda,&s)==ACVR_OK);
    CHECK(near(board,s.board_z));CHECK(fabs(board-eye_w)>500);
    CHECK(near(u,0.125+(s.uv[0]+0.5)/1024));CHECK(near(b,s.brightness/256*0.8));
    uint8_t table[8192]={0},bytes[8192];table[4]=130;table[5]=250;
    eng_fog fog={0};fog.tab=table;fog.alpha_const=-1;
    CHECK(acn22_native_cz_bytes(&fog,bytes)==ACVR_OK);
    CHECK(tex_index(board/ACN22_CZ_DIVISOR)==4);
    CHECK(bytes[tex_index(board/ACN22_CZ_DIVISOR)]==125);
    CHECK(eng_fog_alpha(&fog,(int32_t)board)==125);
    CHECK(eng_fog_alpha(&fog,(int32_t)eye_w)==255);
    double affine=0,vertex_alpha=0;
    for(int i=0;i<3;i++) {
        affine+=lambda[i]*f[0].v[i].board_z;
        vertex_alpha+=lambda[i]*eng_fog_alpha(&fog,(int32_t)f[0].v[i].board_z)/f[0].v[i].clip[3]/den;
    }
    CHECK(eng_fog_alpha(&fog,(int32_t)affine)==5);CHECK(fabs(vertex_alpha-125)>100);
    CHECK(q.zsort==8000 && q.order==7 && memcmp(&q,&snapshot,sizeof q)==0);
    CHECK(acn22_native_active()==NULL && !acn22_native_capture_active());
    eng_draw_cfg cfg={0};cfg.shade=1;
    CHECK(acn22_native_draw_fragment(f,&cfg)==ACVR_UNSUPPORTED);
    CHECK(acn22_native_bind(f,NULL,0,NULL)==ACVR_UNSUPPORTED);
}
static void discrete_lookup(void)
{
    uint8_t table[8192],bytes[8192];for(unsigned i=0;i<8192;i++)table[i]=(uint8_t)((i*53+19)%256);
    const double z[]={-1,0,1,255.5,256,256.5,511.5,512,2097151,2097152,2097408,2147483647};
    const int deltas[]={-256,-1,0,255};eng_fog f={0};f.tab=table;f.alpha_const=-1;
    for(unsigned d=0;d<4;d++) {
        f.sdelta=deltas[d];CHECK(acn22_native_cz_bytes(&f,bytes)==ACVR_OK);
        for(unsigned k=0;k<sizeof z/sizeof *z;k++) {
            /* Packed float coordinate, including exact CZ step endpoints. */
            float s=(float)(z[k]/ACN22_CZ_DIVISOR);
            CHECK(bytes[tex_index(s)]==eng_fog_alpha(&f,(int32_t)z[k]));
        }
    }
    f.tab=NULL;f.sdelta=0;
    const int constants[]={-1,0,255};for(int i=0;i<3;i++) {
        f.alpha_const=constants[i];CHECK(acn22_native_cz_bytes(&f,bytes)==ACVR_OK);
        CHECK(bytes[0]==(constants[i]<0?255:constants[i]) && bytes[8191]==bytes[0]);
    }
    memset(bytes,0xa5,sizeof bytes);f.alpha_const=256;
    CHECK(acn22_native_cz_bytes(&f,bytes)==ACVR_BAD_ARGUMENT);CHECK(bytes[0]==0xa5 && bytes[8191]==0xa5);
    f.alpha_const=0;f.sdelta=256;CHECK(acn22_native_cz_bytes(&f,bytes)==ACVR_BAD_ARGUMENT);
}
static void legacy_and_failure(void)
{
    geo_quad q=quad();float rgba[9][4]={0},st[9][4]={0};acn22_native_vertex out[21],sentinel[21];uint32_t n;
    for(int i=0;i<4;i++) {rgba[i][0]=i*0.2f;rgba[i][3]=1;st[i][0]=i*0.3f;st[i][3]=1.0f/q.rv[i].z;}
    CHECK(acn22_upstream_batch_capture(&q,NULL,rgba,st,out,21,&n)==ACVR_OK);CHECK(n==6);
    const int expected[6]={0,1,2,0,2,3};for(int i=0;i<6;i++) {
        int s=expected[i];CHECK(out[i].position[0]==q.rv[s].sx16/16.0f && out[i].position[1]==q.rv[s].sy16/16.0f);
        CHECK(out[i].position[3]==1 && memcmp(out[i].primary,st[s],sizeof *st)==0);
        CHECK(memcmp(out[i].colour,rgba[s],sizeof *rgba)==0);
    }
    acvr_eye e=eye();acn22_pose_fragment f[2];CHECK(acn22_pose_stage(&q,1,&e,f,2,&n)==ACVR_OK);
    attributes(f,rgba,st);memset(out,0xa5,sizeof out);memcpy(sentinel,out,sizeof out);
    n=99;CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,out,1,&n)==ACVR_BAD_ARGUMENT);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);
    st[0][3]=0.5f;CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,out,21,&n)==ACVR_BAD_ARGUMENT);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);st[0][3]=1;
    rgba[1][0]=NAN;CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,out,21,&n)==ACVR_BAD_ARGUMENT);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);rgba[1][0]=0;
    q.objectflags=1;CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,out,21,&n)==ACVR_UNSUPPORTED);
    CHECK(n==0 && memcmp(out,sentinel,sizeof out)==0);q.objectflags=0;
    q.order++;CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,out,21,&n)==ACVR_UNSUPPORTED);q.order--;
    uint32_t outer=0;CHECK(acn22_native_bind(f,out,21,&outer)==ACVR_OK);
    CHECK(acn22_upstream_batch_capture(&q,f,rgba,st,sentinel,21,&n)==ACVR_BAD_STATE);
    CHECK(acn22_native_unbind()==ACVR_OK);CHECK(acn22_native_active()==NULL);
}
int main(void)
{
    rotated_clipped();discrete_lookup();legacy_and_failure();
    printf("Native batch CPU: %u checks, %u failures; rotated/clipped boardZ vs eyeW; GL proof OFF\n",checks,failures);
    return failures?1:0;
}
