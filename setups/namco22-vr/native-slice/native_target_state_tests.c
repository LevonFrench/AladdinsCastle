/* SPDX-License-Identifier: GPL-3.0-only. Fake dispatch; never calls GL. */
#include "native_target_state.h"
#include <stdio.h>
#include <string.h>
static GLenum selections[3][4],pending_error;
static int bound,level,writes,last_write,fail_next_write,fail_query,queries,failures;
typedef struct {GLint type,name,level,face,layered;} attachment_image;
static attachment_image colours[8],depth,stencil;
static GLint texture_bound=71,renderbuffer_bound=72;
static GLint depth_width=640,depth_height=480,depth_format=0x81a6,depth_samples;
static int fail_image_query,fail_depth_query,fail_texture_bind,fail_renderbuffer_bind,image_queries;
static void check(int ok,const char *name){if(!ok){fprintf(stderr,"FAIL: %s\n",name);++failures;}}
static void APIENTRY integer(GLenum name,GLint *out)
{
    ++queries;
    if(fail_query){pending_error=GL_INVALID_OPERATION;fail_query=0;return;}
    if(name==0x8824){*out=4;return;}
    if(name==0x8cdf){*out=8;return;}
    if(name==0x8069){*out=texture_bound;return;}
    if(name==0x8ca7){*out=renderbuffer_bound;return;}
    if(name>=0x8825 && name<0x8829){*out=(GLint)selections[bound][name-0x8825];return;}
    pending_error=GL_INVALID_ENUM;
}
static void APIENTRY draw_buffers(GLsizei count,const GLenum *values)
{
    ++writes;last_write=bound;
    for(int i=0;i<4;++i)selections[bound][i]=i<count?values[i]:GL_NONE;
    if(fail_next_write){pending_error=GL_INVALID_OPERATION;fail_next_write=0;}
}
static void APIENTRY attachment(GLenum target,GLenum image,GLenum name,GLint *out)
{
    ++queries;
    if(target!=0x8d40){pending_error=GL_INVALID_ENUM;return;}
    /* The mip-only helper retains its original independent fixture. */
    if(image==0x8ce0 && name==0x8cd2){*out=level;return;}
    const attachment_image *attached=image>=0x8ce0 && image<0x8ce8?colours+image-0x8ce0:
        image==0x8d00?&depth:image==0x8d20?&stencil:NULL;
    if(!attached){pending_error=GL_INVALID_ENUM;return;}
    switch(name){
    case 0x8cd0:*out=attached->type;return;
    case 0x8cd1:*out=attached->name;return;
    case 0x8cd2:*out=attached->level;return;
    case 0x8cd3:*out=attached->face;return;
    case 0x8da7:*out=attached->layered;return;
    default:pending_error=GL_INVALID_ENUM;return;
    }
}
static GLenum APIENTRY error(void){GLenum e=pending_error;pending_error=GL_NO_ERROR;return e;}
static void APIENTRY texture(GLenum target,GLuint name)
{
    if(target!=GL_TEXTURE_2D)pending_error=GL_INVALID_ENUM;else texture_bound=(GLint)name;
    if(fail_texture_bind){pending_error=GL_INVALID_OPERATION;fail_texture_bind=0;}
}
static void APIENTRY renderbuffer(GLenum target,GLuint name)
{
    if(target!=0x8d41)pending_error=GL_INVALID_ENUM;else renderbuffer_bound=(GLint)name;
    if(fail_renderbuffer_bind){pending_error=GL_INVALID_OPERATION;fail_renderbuffer_bind=0;}
}
static void parameter(GLenum name,GLint *out,int is_depth)
{
    ++image_queries;
    if(fail_image_query){pending_error=GL_INVALID_OPERATION;fail_image_query=0;return;}
    if(is_depth && fail_depth_query){pending_error=GL_INVALID_OPERATION;fail_depth_query=0;return;}
    switch(name){
    case GL_TEXTURE_WIDTH:case 0x8d42:*out=is_depth?depth_width:640;return;
    case GL_TEXTURE_HEIGHT:case 0x8d43:*out=is_depth?depth_height:480;return;
    case GL_TEXTURE_INTERNAL_FORMAT:case 0x8d44:*out=is_depth?depth_format:0x8058;return;
    case 0x8cab:*out=depth_samples;return;
    default:pending_error=GL_INVALID_ENUM;return;
    }
}
static void APIENTRY texture_parameter(GLenum target,GLint mip,GLenum name,GLint *out)
{
    if(target!=GL_TEXTURE_2D || mip || (texture_bound!=11 && texture_bound!=12)){
        pending_error=GL_INVALID_OPERATION;return;
    }
    parameter(name,out,texture_bound==12);
}
static void APIENTRY renderbuffer_parameter(GLenum target,GLenum name,GLint *out)
{
    if(target!=0x8d41 || renderbuffer_bound!=12){pending_error=GL_INVALID_OPERATION;return;}
    parameter(name,out,1);
}
static const acn22_target_ops ops={integer,draw_buffers,attachment,error,texture,
    texture_parameter,renderbuffer,renderbuffer_parameter};
int main(void)
{
    selections[0][0]=0x8ce3;selections[1][0]=GL_NONE;
    selections[1][1]=0x8ce1;selections[2][0]=0x8ce2;selections[2][2]=0x8ce0;
    GLenum original[3][4];memcpy(original,selections,sizeof original);
    /* Both eye FBOs differ from entry FBO0. Preserve all MRT selections on
     * success and an injected draw/resource error, before returning to entry. */
    for(int failed=0;failed<2;++failed)for(int eye=1;eye<=2;++eye){
        bound=eye;acn22_target_selection saved={0};
        check(acn22_target_colour_begin(&ops,&saved)==ACVR_OK,"borrowed eye selection captured");
        check(selections[eye][0]==0x8ce0 && selections[eye][1]==GL_NONE,"native selects only colour0");
        if(failed)pending_error=GL_INVALID_OPERATION;
        check(acn22_target_colour_restore(&ops,&saved)==(failed?ACVR_ERROR:ACVR_OK),"failure status surfaced after restoring");
        check(last_write==eye && !memcmp(selections[eye],original[eye],sizeof original[eye]),"same eye FBO restored before entry binding");
        bound=0;check(!memcmp(selections[0],original[0],sizeof original[0]),"entry FBO never used for target restore");
    }
    bound=1;acn22_target_selection saved={0};fail_next_write=1;
    check(acn22_target_colour_begin(&ops,&saved)==ACVR_ERROR && saved.restore_pending,"failed selection remains cleanup-pending");
    check(acn22_target_colour_restore(&ops,&saved)==ACVR_OK &&
          !memcmp(selections[1],original[1],sizeof original[1]),"failed selection restores target");
    fail_query=1;memset(&saved,0xa5,sizeof saved);const acn22_target_selection before=saved;
    int previous=writes;
    check(acn22_target_colour_begin(&ops,&saved)==ACVR_ERROR && writes==previous &&
          !memcmp(&saved,&before,sizeof saved),"capture failure does not change selectors/output");
    level=0;check(acn22_target_level_zero(&ops)==ACVR_OK,"level0 admitted");
    level=1;previous=writes;
    check(acn22_target_level_zero(&ops)==ACVR_UNSUPPORTED && writes==previous,"attached mip1 refused even if base image matches");
    level=2;check(acn22_target_level_zero(&ops)==ACVR_UNSUPPORTED,"other nonzero mip refused");
    level=0;colours[0]=(attachment_image){GL_TEXTURE,11,0,0,0};
    acvr_render_target target={0};target.width=640;target.height=480;
    target.colour_image=11;target.colour_format=0x8058;
    previous=writes;
    check(acn22_target_images(&ops,&target)==ACVR_OK,"single colour full extent admitted");
    target.depth_image=12;target.depth_format=0x81a6;
    depth=(attachment_image){0x8d41,12,0,0,0};
    check(acn22_target_images(&ops,&target)==ACVR_OK,"matching declared host depth renderbuffer admitted");
    check(texture_bound==71 && renderbuffer_bound==72,"query bindings restored on success");
    depth_width=320;depth_height=240;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"complete smaller depth renderbuffer refused");
    depth_width=640;depth_height=480;depth.type=GL_TEXTURE;
    check(acn22_target_images(&ops,&target)==ACVR_OK,"matching depth texture admitted");
    depth_width=320;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"smaller depth texture refused");
    depth_width=640;depth.level=1;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"nonzero depth mip refused");
    depth.level=0;depth.layered=1;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"layered depth refused");
    depth.layered=0;depth.face=0x8515;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"cube depth refused");
    depth.face=0;depth.type=0x8d41;depth_format=0x81a5;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"depth format mismatch refused");
    depth_format=0x81a6;depth_samples=4;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"multisample depth refused");
    depth_samples=0;depth.name=13;
    check(acn22_target_images(&ops,&target)==ACVR_BAD_ARGUMENT,"declared depth identity checked");
    depth.name=12;target.depth_image=0;target.depth_format=0;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"undeclared depth refused");
    memset(&depth,0,sizeof depth);colours[7]=(attachment_image){GL_TEXTURE,14,0,0,0};
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"inactive extra colour beyond draw selectors refused");
    memset(colours+7,0,sizeof colours[7]);stencil=(attachment_image){0x8d41,15,0,0,0};
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"extra stencil refused");
    memset(&stencil,0,sizeof stencil);level=1;int old_queries=image_queries;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED && image_queries==old_queries,
          "colour mip refused before dimensions query");
    level=0;colours[0].layered=1;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED,"layered colour refused");
    colours[0].layered=0;fail_image_query=1;
    check(acn22_target_images(&ops,&target)==ACVR_ERROR && texture_bound==71,
          "failed texture query restores binding");
    target.depth_image=12;target.depth_format=0x81a6;depth=(attachment_image){0x8d41,12,0,0,0};
    fail_depth_query=1;
    check(acn22_target_images(&ops,&target)==ACVR_ERROR && renderbuffer_bound==72,
          "failed depth query restores renderbuffer binding");
    fail_renderbuffer_bind=1;
    check(acn22_target_images(&ops,&target)==ACVR_ERROR && renderbuffer_bound==72,
          "failed depth bind restores renderbuffer binding");
    fail_texture_bind=1;
    check(acn22_target_images(&ops,&target)==ACVR_ERROR && texture_bound==71,
          "failed colour bind restores texture binding");
    depth_width=320;
    check(acn22_target_images(&ops,&target)==ACVR_UNSUPPORTED && renderbuffer_bound==72,
          "refused depth extent restores renderbuffer binding");
    depth_width=640;fail_query=1;
    check(acn22_target_images(&ops,&target)==ACVR_ERROR,"attachment-limit query failure surfaced");
    check(writes==previous && texture_bound==71 && renderbuffer_bound==72,
          "validation changes no draw selectors or persistent query bindings");
    check(queries>0,"tests exercise real helper through fake dispatch");
    return failures?1:0;
}
