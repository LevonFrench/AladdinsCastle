/* SPDX-License-Identifier: GPL-3.0-only */
#include "native_display.h"
#include <limits.h>
#include <string.h>
static uint64_t fingerprint(const acvr_render_target *t)
{
    /* Private correlation, not a cryptographic acceptance/resource root. */
    const uint64_t words[]={t->api,t->width,t->height,t->array_layers,t->sample_count,
        (uint64_t)t->colour_format,(uint64_t)t->depth_format,t->colour_image,
        t->depth_image,t->framebuffer,t->colour_state,t->depth_state};
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<sizeof words/sizeof words[0];++i)
        for(int b=0;b<8;++b){h^=(words[i]>>(b*8))&255;h*=UINT64_C(1099511628211);}
    return h;
}
static acvr_result key(const acvr_draw_info *d,uint32_t eye,acn22_display_key *out)
{
    if(!d || d->size<sizeof(*d) || d->version!=ACVR_STRUCT_VERSION ||
       d->target.size<sizeof(d->target) || d->target.version!=ACVR_STRUCT_VERSION)
        return ACVR_BAD_VERSION;
    if(!d->frame_id || !d->display_id || d->predicted_display_time_ns<=0 ||
       d->view_count!=1 || !d->views || d->view_stride<sizeof(acvr_eye) ||
       d->view_stride%_Alignof(acvr_eye) || (uintptr_t)d->views%_Alignof(acvr_eye))
        return ACVR_BAD_ARGUMENT;
    const acvr_eye *e=d->views;
    const acvr_render_target *t=&d->target;
    if(e->version!=ACVR_STRUCT_VERSION || e->size<sizeof(*e) || e->size>d->view_stride)
        return ACVR_BAD_VERSION;
    if(t->api!=ACVR_GRAPHICS_GL || t->colour_format!=0x8058 || t->sample_count!=1 ||
       t->array_layers!=1 || e->array_layer || d->command_buffer ||
       t->colour_view || t->depth_view || t->colour_state || t->depth_state)
        return ACVR_UNSUPPORTED;
    if(e->eye_index!=eye || !t->width || !t->height || t->width>16384 || t->height>16384 ||
       !t->framebuffer || t->framebuffer>UINT32_MAX || !t->colour_image ||
       t->colour_image>UINT32_MAX || t->depth_image>UINT32_MAX ||
       !e->rect_width || !e->rect_height || e->rect_x<0 || e->rect_y<0 ||
       (uint64_t)e->rect_x+e->rect_width>t->width ||
       (uint64_t)e->rect_y+e->rect_height>t->height)
        return ACVR_BAD_ARGUMENT;
    if((t->depth_image==0)!=(t->depth_format==0) ||
       (t->depth_image && t->depth_format!=0x81a6)) return ACVR_UNSUPPORTED;
    memset(out,0,sizeof *out);
    out->frame=d->frame_id;out->display=d->display_id;
    out->predicted_ns=d->predicted_display_time_ns;
    out->target_fingerprint=fingerprint(t);
    out->eye.size=e->size;out->eye.version=e->version;
    out->eye.eye_index=e->eye_index;out->eye.array_layer=e->array_layer;
    out->eye.rect_x=e->rect_x;out->eye.rect_y=e->rect_y;
    out->eye.rect_width=e->rect_width;out->eye.rect_height=e->rect_height;
    memcpy(out->eye.view_from_scene,e->view_from_scene,sizeof e->view_from_scene);
    memcpy(out->eye.projection_from_view,e->projection_from_view,sizeof e->projection_from_view);
    return ACVR_OK;
}
acvr_result acn22_display_stage(const geo_quad *source,uint32_t count,uint64_t frame,
    uint32_t draws,uint32_t stride,const acvr_draw_info *records,acn22_display_pair *out)
{
    if(!source || !count || count>2 || !out || !records || draws!=2 ||
       stride<sizeof(*records) || stride%_Alignof(acvr_draw_info) ||
       (uintptr_t)records%_Alignof(acvr_draw_info))
        return ACVR_BAD_ARGUMENT;
    size_t extent=(size_t)stride+sizeof(*records);
    if(extent<stride || (uintptr_t)records>UINTPTR_MAX-extent)return ACVR_BAD_ARGUMENT;
    acn22_display_pair candidate={0};
    const acvr_draw_info *d[2]={records,(const acvr_draw_info *)((const char *)records+stride)};
    for(uint32_t i=0;i<2;++i){
        if(d[i]->size>stride) return ACVR_BAD_ARGUMENT;
        acvr_result r=key(d[i],i,candidate.key+i);if(r!=ACVR_OK)return r;
        if(candidate.key[i].frame!=frame)return ACVR_BAD_STATE;
    }
    if(candidate.key[0].display!=candidate.key[1].display ||
       candidate.key[0].predicted_ns!=candidate.key[1].predicted_ns)return ACVR_BAD_ARGUMENT;
    /* Initial adapter is two distinct eye targets, never an atlas/SBS route. */
    if(d[0]->target.framebuffer==d[1]->target.framebuffer ||
       d[0]->target.colour_image==d[1]->target.colour_image ||
       (d[0]->target.depth_image && d[0]->target.depth_image==d[1]->target.depth_image))
        return ACVR_BAD_ARGUMENT;
    for(uint32_t i=0;i<2;++i){
        acvr_result r=acn22_pose_stage(source,count,&candidate.key[i].eye,
            candidate.packets[i],4,candidate.count+i);
        if(r!=ACVR_OK)return r;
    }
    candidate.prepared=1;*out=candidate;return ACVR_OK;
}
acvr_result acn22_display_match(const acn22_display_pair *pair,const acvr_draw_info *draw)
{
    if(!pair || !pair->prepared || pair->next_eye<0 || pair->next_eye>1)return ACVR_BAD_STATE;
    acn22_display_key actual;
    acvr_result r=key(draw,(uint32_t)pair->next_eye,&actual);if(r!=ACVR_OK)return r;
    const acn22_display_key *expected=pair->key+pair->next_eye;
    const acvr_eye *a=&actual.eye,*e=&expected->eye;
    if(actual.frame!=expected->frame || actual.display!=expected->display ||
       actual.predicted_ns!=expected->predicted_ns || actual.target_fingerprint!=expected->target_fingerprint ||
       a->size!=e->size || a->version!=e->version || a->eye_index!=e->eye_index ||
       a->array_layer!=e->array_layer || a->rect_x!=e->rect_x || a->rect_y!=e->rect_y ||
       a->rect_width!=e->rect_width || a->rect_height!=e->rect_height ||
       memcmp(a->view_from_scene,e->view_from_scene,sizeof a->view_from_scene) ||
       memcmp(a->projection_from_view,e->projection_from_view,sizeof a->projection_from_view))
        return ACVR_BAD_STATE;
    return ACVR_OK;
}
