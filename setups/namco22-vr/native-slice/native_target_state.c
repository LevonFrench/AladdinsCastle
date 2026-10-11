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
