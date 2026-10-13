// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include <filesystem>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    const auto present=std::filesystem::absolute(argv[1]);
    const auto missing=present.parent_path()/L"synthetic-absent-openvr.dll";
    auto probe=[](const std::filesystem::path& path) {
        return LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    };
    auto absent=probe(missing);
    if(absent) { FreeLibrary(absent); return 1; }
    auto library=probe(present);
    if(!library) return 1;
    const bool exported=GetProcAddress(library,"VR_InitInternal")!=nullptr;
    FreeLibrary(library);
    if(!exported) return 1;
    std::cout << "Missing/present DLL native-path probes passed; VR_Init not called\n";
    return 0;
}
