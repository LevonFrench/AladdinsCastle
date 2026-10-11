#include <math.h>
#include <string.h>
#include "pose_stage.h"

static acvr_result validate_eye(const acvr_eye *e)
{
    if (!e || e->size < sizeof *e) return ACVR_BAD_ARGUMENT;
    if (e->version != ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if (e->array_layer || !e->rect_width || !e->rect_height ||
        e->rect_width>16384 || e->rect_height>16384 || e->rect_x<0 || e->rect_y<0 ||
        e->rect_x>16384 || e->rect_y>16384) return ACVR_UNSUPPORTED;
    const float *m=e->view_from_scene,*p=e->projection_from_view;
    for(int i=0;i<16;i++) if(!isfinite(m[i]) || !isfinite(p[i])) return ACVR_BAD_ARGUMENT;
    if(m[3] || m[7] || m[11] || m[15]!=1) return ACVR_UNSUPPORTED;
    double n[3]={0};
    for(int c=0;c<3;c++)for(int r=0;r<3;r++)n[c]+=(double)m[c*4+r]*m[c*4+r];
    if(n[0]<1e-18 || n[0]>1e18) return ACVR_UNSUPPORTED;
    for(int c=0;c<3;c++) {
        if(fabs(n[c]-n[0])>3e-5*n[0])return ACVR_UNSUPPORTED;
        for(int d=c+1;d<3;d++) {
            double dot=0; for(int r=0;r<3;r++)dot+=(double)m[c*4+r]*m[d*4+r];
            if(fabs(dot)>3e-5*n[0])return ACVR_UNSUPPORTED;
        }
    }
    double det=(double)m[0]*(m[5]*m[10]-(double)m[9]*m[6])
              -(double)m[4]*(m[1]*m[10]-(double)m[9]*m[2])
              +(double)m[8]*(m[1]*m[6]-(double)m[5]*m[2]);
    if(det<=0)return ACVR_UNSUPPORTED;
    if(p[0]<=0 || p[5]<=0 || p[11]!=-1 || p[15] ||
       p[1] || p[2] || p[3] || p[4] || p[6] || p[7] || p[12] || p[13] ||
       p[10]>-1 || p[14]>=0)return ACVR_UNSUPPORTED;
    return ACVR_OK;
}

static int finite_vertex(const acn22_pose_vertex *v)
{
    for(int i=0;i<4;i++)if(!isfinite(v->clip[i]) || fabs(v->clip[i])>1e30)return 0;
    for(int i=0;i<2;i++)if(!isfinite(v->uv[i]) || v->uv[i]<0 || v->uv[i]>=4096)return 0;
    return isfinite(v->brightness) && v->brightness>=0 && v->brightness<=32768 &&
           isfinite(v->board_z) && v->board_z>0 && v->board_z<=2147483647;
}
static double plane(const acn22_pose_vertex *v,int p)
{
    const double *c=v->clip;
    switch(p) {
    case 0:return c[3]+c[0]; case 1:return c[3]-c[0];
    case 2:return c[3]+c[1]; case 3:return c[3]-c[1];
    case 4:return c[3]+c[2]; default:return c[3]-c[2];
    }
}
static acn22_pose_vertex lerp(const acn22_pose_vertex *a,const acn22_pose_vertex *b,double t)
{
    acn22_pose_vertex v;
    for(int i=0;i<4;i++)v.clip[i]=a->clip[i]+(b->clip[i]-a->clip[i])*t;
    for(int i=0;i<2;i++)v.uv[i]=a->uv[i]+(b->uv[i]-a->uv[i])*t;
    v.brightness=a->brightness+(b->brightness-a->brightness)*t;
    v.board_z=a->board_z+(b->board_z-a->board_z)*t;
    return v;
}
static int append(acn22_pose_vertex *v,uint32_t *n,const acn22_pose_vertex *x)
{
    /* Suppress identical clip endpoints created at exact boundary vertices. */
    if(*n && memcmp(v[*n-1].clip,x->clip,sizeof x->clip)==0)return 1;
    if(*n>=ACN22_POSE_MAX_VERTS)return 0;
    v[(*n)++]=*x;return 1;
}
static int nondegenerate(const acn22_pose_vertex v[3])
{
    /* Source triangle tested in board camera space by caller separately.
     * Clipped raster area: zero-area fragments are successful culls. */
    double x[3],y[3];
    for(int i=0;i<3;i++) { if(v[i].clip[3]<=0)return 0; x[i]=v[i].clip[0]/v[i].clip[3]; y[i]=v[i].clip[1]/v[i].clip[3]; }
    return fabs((x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]))>1e-14;
}
static acvr_result clip_triangle(const acn22_pose_vertex in[3],acn22_pose_fragment *f)
{
    acn22_pose_vertex a[ACN22_POSE_MAX_VERTS],b[ACN22_POSE_MAX_VERTS];
    memcpy(a,in,sizeof *in*3);uint32_t n=3;
    for(int p=0;p<6 && n>=3;p++) {
        uint32_t next=0;
        for(uint32_t i=0;i<n;i++) {
            const acn22_pose_vertex *v=&a[i],*w=&a[(i+1)%n];
            double dv=plane(v,p),dw=plane(w,p);
            int vi=dv>=0,wi=dw>=0;
            if(vi && !append(b,&next,v))return ACVR_UNSUPPORTED;
            if(vi!=wi) {
                double t=dv/(dv-dw);
                if(!isfinite(t) || t<0 || t>1)return ACVR_ERROR;
                acn22_pose_vertex x=lerp(v,w,t);
                /* Pin the newly intersected plane to avoid cancellation drift. */
                x.clip[p/2]=(p%2)?x.clip[3]:-x.clip[3];
                if(!finite_vertex(&x) || !append(b,&next,&x))return ACVR_UNSUPPORTED;
            }
        }
        if(next>1 && memcmp(b[0].clip,b[next-1].clip,sizeof b[0].clip)==0)next--;
        memcpy(a,b,sizeof *a*next);n=next;
    }
    f->count=0;
    if(n<3)return ACVR_OK;
    for(uint32_t i=0;i<n;i++)if(!finite_vertex(&a[i]) || a[i].clip[3]<=0)return ACVR_ERROR;
    int area=0;
    for(uint32_t i=1;i+1<n;i++) {
        acn22_pose_vertex tri[3]={a[0],a[i],a[i+1]};
        if(nondegenerate(tri))area=1;
    }
    if(!area)return ACVR_OK;
    memcpy(f->v,a,sizeof *a*n);f->count=n;
    return ACVR_OK;
}

static acvr_result stage_quad(const geo_quad *q,const acvr_eye *e,uint32_t index,
                             acn22_pose_fragment fragments[2])
{
    if(q->direct || q->behind || q->ndv || q->nrv!=4 ||
       q->clip[0]>0 || q->clip[1]<639 || q->clip[2]>0 || q->clip[3]<479)return ACVR_UNSUPPORTED;
    acn22_pose_vertex v[4];
    const float *m=e->view_from_scene,*p=e->projection_from_view;
    for(int i=0;i<4;i++) {
        const geo_vert *raw=&q->v[i],*rv=&q->rv[i];
        if(!raw->valid || raw->z<=0 || raw->acn_view[2]!=raw->z)return ACVR_UNSUPPORTED;
        for(int c=0;c<3;c++)if(!isfinite(raw->acn_view[c]) ||
            fabs(raw->acn_view[c])>2147483647.0 || rv->acn_view[c]!=raw->acn_view[c])return ACVR_UNSUPPORTED;
        if(rv->uf<0 || rv->vf<0 || rv->uf>=4096*65536 || rv->vf>=4096*65536 || rv->bf<0)return ACVR_UNSUPPORTED;
        double source[4]={raw->acn_view[0],raw->acn_view[1],-raw->acn_view[2],1},eye[4]={0};
        for(int r=0;r<4;r++)for(int c=0;c<4;c++)eye[r]+=(double)m[c*4+r]*source[c];
        memset(&v[i],0,sizeof v[i]);
        for(int r=0;r<4;r++)for(int c=0;c<4;c++)v[i].clip[r]+=(double)p[c*4+r]*eye[c];
        v[i].uv[0]=rv->uf/65536.0;v[i].uv[1]=rv->vf/65536.0;
        v[i].brightness=rv->bf/65536.0;v[i].board_z=raw->acn_view[2];
        if(!finite_vertex(&v[i]))return ACVR_UNSUPPORTED;
    }
    static const int fan[2][3]={{0,1,2},{0,2,3}};
    for(int t=0;t<2;t++) {
        /* Reject truly degenerate source triangles, not a valid edge-on pose. */
        const double *a=q->v[fan[t][0]].acn_view,*b=q->v[fan[t][1]].acn_view,*c=q->v[fan[t][2]].acn_view;
        double ab[3],ac[3],cross[3];for(int r=0;r<3;r++){ab[r]=b[r]-a[r];ac[r]=c[r]-a[r];}
        cross[0]=ab[1]*ac[2]-ab[2]*ac[1];cross[1]=ab[2]*ac[0]-ab[0]*ac[2];cross[2]=ab[0]*ac[1]-ab[1]*ac[0];
        if(cross[0]==0 && cross[1]==0 && cross[2]==0)return ACVR_UNSUPPORTED;
        acn22_pose_fragment *f=&fragments[t];memset(f,0,sizeof *f);
        f->native=q;f->source_index=index;f->fan_index=(uint32_t)t;f->painter_key=q->zsort;f->emission_order=q->order;
        acn22_pose_vertex tri[3]={v[fan[t][0]],v[fan[t][1]],v[fan[t][2]]};
        acvr_result result=clip_triangle(tri,f);if(result!=ACVR_OK)return result;
    }
    return ACVR_OK;
}

acvr_result acn22_pose_stage(const geo_quad *source,uint32_t count,const acvr_eye *eye,
                           acn22_pose_fragment *output,uint32_t capacity,uint32_t *written)
{
    if(!written)return ACVR_BAD_ARGUMENT;
    *written=0;
    if(count>ACN22_POSE_MAX_QUADS || (count && !source) || (capacity && !output))return ACVR_BAD_ARGUMENT;
    acvr_result result=validate_eye(eye);if(result!=ACVR_OK)return result;
    acvr_eye copy=*eye;
    uint32_t required=0;
    for(uint32_t i=0;i<count;i++) {
        acn22_pose_fragment f[2];result=stage_quad(source+i,&copy,i,f);if(result!=ACVR_OK)return result;
        for(int t=0;t<2;t++)if(f[t].count)required++;
    }
    if(required>capacity)return ACVR_BAD_ARGUMENT;
    /* Allocation-free preflight; no output byte is published before all
     * source polygons and output capacity have passed. Owner-thread immutable
     * source makes this deterministic second pass infallible. */
    uint32_t n=0;
    for(uint32_t i=0;i<count;i++) {
        acn22_pose_fragment f[2];(void)stage_quad(source+i,&copy,i,f);
        for(int t=0;t<2;t++)if(f[t].count){memcpy(output+n,f+t,sizeof *output);n++;}
    }
    *written=n;return ACVR_OK;
}

acvr_result acn22_pose_pack(const acn22_pose_vertex *v,double position[4],double texture[4])
{
    if(!v || !position || !texture || !finite_vertex(v) || v->clip[3]<=0)return ACVR_BAD_ARGUMENT;
    position[0]=320*(v->clip[0]+v->clip[3]);
    position[1]=240*(v->clip[3]-v->clip[1]);
    position[2]=-v->clip[2];position[3]=v->clip[3];
    texture[0]=v->uv[0];texture[1]=v->uv[1];texture[2]=0;texture[3]=1;
    return ACVR_OK;
}
acvr_result acn22_pose_interpolate(const acn22_pose_vertex tri[3],const double lambda[3],acn22_pose_sample *sample)
{
    if(!tri || !lambda || !sample)return ACVR_BAD_ARGUMENT;
    acn22_pose_sample out={0};double denominator=0,sum=0;
    for(int i=0;i<3;i++) {
        if(!finite_vertex(&tri[i]) || tri[i].clip[3]<=0 || !isfinite(lambda[i]) || lambda[i]<0)return ACVR_BAD_ARGUMENT;
        double w=lambda[i]/tri[i].clip[3];denominator+=w;sum+=lambda[i];
        out.uv[0]+=w*tri[i].uv[0];out.uv[1]+=w*tri[i].uv[1];
        out.brightness+=w*tri[i].brightness;out.board_z+=w*tri[i].board_z;
        out.ndc_z+=lambda[i]*tri[i].clip[2]/tri[i].clip[3];
    }
    if(fabs(sum-1)>1e-9 || !isfinite(denominator) || denominator<=0)return ACVR_BAD_ARGUMENT;
    out.uv[0]/=denominator;out.uv[1]/=denominator;
    out.brightness/=denominator;out.board_z/=denominator;
    if(!isfinite(out.uv[0]) || !isfinite(out.uv[1]) || !isfinite(out.brightness) ||
       !isfinite(out.board_z) || !isfinite(out.ndc_z))return ACVR_BAD_ARGUMENT;
    *sample=out;return ACVR_OK;
}
