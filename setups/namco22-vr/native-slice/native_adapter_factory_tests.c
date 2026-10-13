/* SPDX-License-Identifier: GPL-3.0-only. Query/early negatives only; no GL. */
#include "native_adapter.h"
#include <stdio.h>
#include <string.h>
#ifndef ACVR_BACKEND_API_V1_SIZE
#error Reviewed prepare-display contract required
#endif
int main(void)
{
    struct { acvr_backend_api api;unsigned char tail[16]; } extended;
    memset(&extended,0xa5,sizeof extended);extended.api.size=sizeof extended;
    extended.api.version=ACVR_STRUCT_VERSION;
    const uint32_t size=extended.api.size;
    if(acn22_native_adapter_query(ACVR_ABI_VERSION,&extended.api)!=ACVR_OK ||
       extended.api.size!=size || extended.api.version!=ACVR_STRUCT_VERSION ||
       !extended.api.game_prepare_display || !extended.api.game_draw_eye ||
       extended.api.supported_graphics!=ACVR_GRAPHICS_BIT(ACVR_GRAPHICS_GL))return 1;
    for(size_t i=0;i<sizeof extended.tail;++i)if(extended.tail[i]!=0xa5)return 1;
    acvr_backend_api api;memset(&api,0xa5,sizeof api);api.size=ACVR_BACKEND_API_V1_SIZE;
    api.version=ACVR_STRUCT_VERSION;acvr_backend_api before=api;
    if(acn22_native_adapter_query(ACVR_ABI_VERSION,&api)!=ACVR_OK || api.supported_graphics ||
       api.size!=before.size || api.version!=before.version ||
       memcmp((const char *)&api+ACVR_BACKEND_API_V1_SIZE,
              (const char *)&before+ACVR_BACKEND_API_V1_SIZE,sizeof api-ACVR_BACKEND_API_V1_SIZE))return 1;
    for(uint32_t n=ACVR_BACKEND_API_V1_SIZE+1;n<sizeof api;++n){
        api.size=n;before=api;
        if(acn22_native_adapter_query(ACVR_ABI_VERSION,&api)!=ACVR_BAD_ARGUMENT || memcmp(&api,&before,sizeof api))return 1;
    }
    /* Foreign/no backend must fail before probing a context or GL dispatch. */
    if(extended.api.game_prepare_display(NULL,NULL,2,0,NULL)!=ACVR_BAD_STATE ||
       extended.api.game_draw_eye(NULL,NULL,NULL)!=ACVR_BAD_STATE)return 1;
    puts("Private native factory/canaries only; no GL/context created");return 0;
}
