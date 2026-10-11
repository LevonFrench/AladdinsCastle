/* SPDX-License-Identifier: GPL-3.0-only. Fake dispatch; never calls GL. */
#include "native_target_state.h"
#include <stdio.h>
#include <string.h>
static GLenum selections[3][4],pending_error;
static int bound,level,writes,last_write,fail_next_write,fail_query,queries,failures;
static void check(int ok,const char *name){if(!ok){fprintf(stderr,"FAIL: %s\n",name);++failures;}}
static void APIENTRY integer(GLenum name,GLint *out)
{
    ++queries;
    if(fail_query){pending_error=GL_INVALID_OPERATION;fail_query=0;return;}
    if(name==0x8824){*out=4;return;}
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
    if(target!=0x8d40 || image!=0x8ce0 || name!=0x8cd2){pending_error=GL_INVALID_ENUM;return;}
    *out=level;
}
static GLenum APIENTRY error(void){GLenum e=pending_error;pending_error=GL_NO_ERROR;return e;}
static const acn22_target_ops ops={integer,draw_buffers,attachment,error};
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
    check(queries>0,"tests exercise real helper through fake dispatch");
    return failures?1:0;
}
