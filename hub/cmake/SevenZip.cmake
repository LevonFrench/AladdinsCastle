# Only the upstream C metadata reader is linked. It is public domain; no C++/unRAR code.
FetchContent_Declare(sevenzip
    GIT_REPOSITORY https://github.com/ip7z/7zip.git
    GIT_TAG 5e96a8279489832924056b1fa82f29d5837c9469
    SOURCE_SUBDIR _headers_only SYSTEM)
FetchContent_MakeAvailable(sevenzip)
set(_7z ${sevenzip_SOURCE_DIR}/C)
add_library(ac7zmetadata STATIC
    ${_7z}/7zArcIn.c ${_7z}/7zBuf.c ${_7z}/7zBuf2.c ${_7z}/7zStream.c
    ${_7z}/7zDec.c ${_7z}/7zCrc.c ${_7z}/7zCrcOpt.c
    ${_7z}/LzmaDec.c ${_7z}/Lzma2Dec.c ${_7z}/CpuArch.c
    ${_7z}/Bcj2.c ${_7z}/Bra.c ${_7z}/Bra86.c ${_7z}/BraIA64.c ${_7z}/Delta.c
    ${_7z}/Ppmd7.c ${_7z}/Ppmd7Dec.c)
target_include_directories(ac7zmetadata SYSTEM PUBLIC ${_7z})
target_compile_definitions(ac7zmetadata PRIVATE Z7_PPMD_SUPPORT Z7_EXTRACT_ONLY)
configure_file("${sevenzip_SOURCE_DIR}/DOC/7zC.txt" "${AC_DEPENDENCY_LICENSE_DIR}/7zip-C-public-domain.txt" COPYONLY)
