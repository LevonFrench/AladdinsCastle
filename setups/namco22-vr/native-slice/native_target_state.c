/* SPDX-License-Identifier: GPL-3.0-only */
#include "native_target_state.h"
#include <string.h>
acvr_result acn22_target_colour_begin(const acn22_target_ops *ops,acn22_target_selection *out)
{
    if(!ops || !ops->integer || !ops->draw_buffers || !ops->error || !out)
        return ACVR_BAD_ARGUMENT;
    acn22_target_selection candidate={0};GLint count=0;
    ops->integer(0x8824,&count); /* GL_MAX_DRAW_BUFFERS */
    if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
    if(count<1 || count>32)return ACVR_UNSUPPORTED;
    candidate.count=count;
    for(int i=0;i<count;++i){
        GLint selection=0;ops->integer(0x8825+i,&selection); /* GL_DRAW_BUFFERi */
        if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
        candidate.selections[i]=(GLenum)selection;
    }
    candidate.restore_pending=1;*out=candidate;
    const GLenum colour0=0x8ce0;
    ops->draw_buffers(1,&colour0);
    /* On a selection failure, caller still restores this same target. */
    return ops->error()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR;
}
acvr_result acn22_target_colour_restore(const acn22_target_ops *ops,acn22_target_selection *state)
{
    if(!ops || !ops->draw_buffers || !ops->error || !state)return ACVR_BAD_ARGUMENT;
    if(!state->restore_pending)return ACVR_OK;
    if(state->count<1 || state->count>32)return ACVR_BAD_STATE;
    ops->draw_buffers(state->count,state->selections);
    state->restore_pending=0;
    return ops->error()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR;
}
acvr_result acn22_target_level_zero(const acn22_target_ops *ops)
{
    if(!ops || !ops->attachment || !ops->error)return ACVR_BAD_ARGUMENT;
    GLint level=-1;
    ops->attachment(0x8d40,0x8ce0,0x8cd2,&level); /* attached colour texture level */
    if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
    return level==0?ACVR_OK:ACVR_UNSUPPORTED;
}
static acvr_result object(const acn22_target_ops *ops,GLenum image,GLint *type,GLint *name)
{
    ops->attachment(0x8d40,image,0x8cd0,type);
    if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
    *name=0;
    if(*type!=GL_NONE){
        ops->attachment(0x8d40,image,0x8cd1,name);
        if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
    }
    return ACVR_OK;
}
static acvr_result dimensions(const acn22_target_ops *ops,GLenum image,GLint type,
                             GLint name,const acvr_render_target *t,GLint expected_format)
{
    GLint previous=0,width=0,height=0,format=0,samples=0;
    if(type==GL_TEXTURE){
        GLint level=-1,face=-1,layered=-1;
        ops->attachment(0x8d40,image,0x8cd2,&level);
        ops->attachment(0x8d40,image,0x8cd3,&face);
        ops->attachment(0x8d40,image,0x8da7,&layered);
        if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
        if(level || face || layered)return ACVR_UNSUPPORTED;
        ops->integer(0x8069,&previous); /* GL_TEXTURE_BINDING_2D */
        if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
        ops->texture(GL_TEXTURE_2D,(GLuint)name);
        GLenum fault=ops->error();
        if(fault==GL_NO_ERROR){
            ops->texture_parameter(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
            ops->texture_parameter(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
            ops->texture_parameter(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
            fault=ops->error();
        }
        ops->texture(GL_TEXTURE_2D,(GLuint)previous);
        if(ops->error()!=GL_NO_ERROR || fault!=GL_NO_ERROR)return ACVR_ERROR;
    }else if(type==0x8d41){ /* GL_RENDERBUFFER */
        ops->integer(0x8ca7,&previous); /* GL_RENDERBUFFER_BINDING */
        if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
        ops->renderbuffer(0x8d41,(GLuint)name);
        GLenum fault=ops->error();
        if(fault==GL_NO_ERROR){
            ops->renderbuffer_parameter(0x8d41,0x8d42,&width);
            ops->renderbuffer_parameter(0x8d41,0x8d43,&height);
            ops->renderbuffer_parameter(0x8d41,0x8d44,&format);
            ops->renderbuffer_parameter(0x8d41,0x8cab,&samples);
            fault=ops->error();
        }
        ops->renderbuffer(0x8d41,(GLuint)previous);
        if(ops->error()!=GL_NO_ERROR || fault!=GL_NO_ERROR)return ACVR_ERROR;
    }else return ACVR_UNSUPPORTED;
    return width==(GLint)t->width && height==(GLint)t->height &&
           format==expected_format && !samples?ACVR_OK:ACVR_UNSUPPORTED;
}
acvr_result acn22_target_images(const acn22_target_ops *ops,const acvr_render_target *t)
{
    if(!ops || !ops->integer || !ops->attachment || !ops->error || !ops->texture ||
       !ops->texture_parameter || !ops->renderbuffer || !ops->renderbuffer_parameter || !t)
        return ACVR_BAD_ARGUMENT;
    GLint count=0,type=0,name=0;
    ops->integer(0x8cdf,&count); /* GL_MAX_COLOR_ATTACHMENTS, independent of draw selectors */
    if(ops->error()!=GL_NO_ERROR)return ACVR_ERROR;
    if(count<1 || count>32)return ACVR_UNSUPPORTED;
    /* An inactive attachment still limits framebuffer extent. Reject unsupported
     * layouts rather than detaching images or trusting completeness alone. */
    for(int i=1;i<count;++i){
        acvr_result r=object(ops,0x8ce0+i,&type,&name);if(r!=ACVR_OK)return r;
        if(type!=GL_NONE)return ACVR_UNSUPPORTED;
    }
    acvr_result r=object(ops,0x8d20,&type,&name);if(r!=ACVR_OK)return r;
    if(type!=GL_NONE)return ACVR_UNSUPPORTED; /* stencil */
    r=object(ops,0x8ce0,&type,&name);if(r!=ACVR_OK)return r;
    if(type!=GL_TEXTURE || (GLuint)name!=t->colour_image)return ACVR_BAD_ARGUMENT;
    r=dimensions(ops,0x8ce0,type,name,t,(GLint)t->colour_format);if(r!=ACVR_OK)return r;
    r=object(ops,0x8d00,&type,&name);if(r!=ACVR_OK)return r;
    if(!t->depth_image)return type==GL_NONE?ACVR_OK:ACVR_UNSUPPORTED;
    if((type!=GL_TEXTURE && type!=0x8d41) || (GLuint)name!=t->depth_image)
        return ACVR_BAD_ARGUMENT;
    return dimensions(ops,0x8d00,type,name,t,(GLint)t->depth_format);
}
