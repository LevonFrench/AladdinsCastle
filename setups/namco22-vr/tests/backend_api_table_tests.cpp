// SPDX-License-Identifier: GPL-3.0-only
#include "n22_api_table.h"
#include <array>
#include <cstdio>
#include <cstring>
static int failures;
static void check(bool condition,const char *name)
{ if(!condition){std::fprintf(stderr,"FAIL: %s\n",name);++failures;} }
static void query_size(uint32_t size,acvr_result expected)
{
    alignas(acvr_backend_api) std::array<unsigned char,sizeof(acvr_backend_api)+32> bytes;
    bytes.fill(0xa5);
    const uint32_t version=ACVR_STRUCT_VERSION;
    std::memcpy(bytes.data(),&size,4);std::memcpy(bytes.data()+4,&version,4);
    const auto before=bytes;
    auto *out=reinterpret_cast<acvr_backend_api *>(bytes.data());
    check(acvr_backend_query(ACVR_ABI_VERSION,out)==expected,"query size result");
    if(expected!=ACVR_OK){check(bytes==before,"rejected table untouched");return;}
    check(std::memcmp(bytes.data(),before.data(),8)==0,"caller prefix preserved");
    const size_t covered=size<sizeof(acvr_backend_api)?size:sizeof(acvr_backend_api);
    check(std::memcmp(bytes.data()+covered,before.data()+covered,bytes.size()-covered)==0,
          "old table/caller tail canaries preserved");
    acvr_backend_api copy{};std::memcpy(&copy,bytes.data(),covered);
    check(copy.abi_version==ACVR_ABI_VERSION && copy.supported_graphics==0 &&
          copy.game_open && copy.game_draw_eye && copy.game_release_frame,
          "real production table remains disabled and complete");
#ifdef ACVR_BACKEND_API_V1_SIZE
    check(copy.game_prepare_display==nullptr,"optional preflight callback remains null");
#endif
}
int main()
{
    query_size(N22_API_TABLE_OLD_SIZE,ACVR_OK);
    query_size(sizeof(acvr_backend_api),ACVR_OK);
    query_size(sizeof(acvr_backend_api)+24,ACVR_OK);
    query_size(8,ACVR_BAD_ARGUMENT);
    query_size(N22_API_TABLE_OLD_SIZE-1,ACVR_BAD_ARGUMENT);
    for(uint32_t n=N22_API_TABLE_OLD_SIZE+1;n<sizeof(acvr_backend_api);++n)
        query_size(n,ACVR_BAD_ARGUMENT);
    acvr_backend_api out{};ACVR_INIT(&out);const auto before=out;
    check(acvr_backend_query(99,&out)==ACVR_BAD_VERSION &&
          std::memcmp(&out,&before,sizeof(out))==0,"wrong ABI untouched");
    out.version=99;const auto wrong=out;
    check(acvr_backend_query(ACVR_ABI_VERSION,&out)==ACVR_BAD_VERSION &&
          std::memcmp(&out,&wrong,sizeof(out))==0,"wrong version untouched");
    check(acvr_backend_query(ACVR_ABI_VERSION,nullptr)==ACVR_BAD_ARGUMENT,"null table rejected");
    return failures?1:0;
}
