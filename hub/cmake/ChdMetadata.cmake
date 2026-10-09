# Sparse CHD sector reader only. All bundled codecs are GPL-compatible.
set(CHDR_WANT_TESTS OFF CACHE BOOL "" FORCE)
set(CHDR_LOWRAM_TARGET OFF CACHE BOOL "" FORCE)
set(WITH_LZMA_ASM OFF CACHE BOOL "" FORCE)
set(CHDR_FLAC_BACKEND drflac CACHE STRING "" FORCE)
FetchContent_Declare(libchdr
    GIT_REPOSITORY https://github.com/rtissera/libchdr.git
    GIT_TAG 607694ca0812edfc9cc2030c64634fc2393668de SYSTEM)
set(_ac_previous_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(libchdr)
set(BUILD_SHARED_LIBS ${_ac_previous_shared})
configure_file("${libchdr_SOURCE_DIR}/LICENSE.txt" "${AC_DEPENDENCY_LICENSE_DIR}/libchdr-BSD-3-Clause.txt" COPYONLY)
configure_file("${libchdr_SOURCE_DIR}/deps/lzma-26.02/LICENSE" "${AC_DEPENDENCY_LICENSE_DIR}/libchdr-LZMA-public-domain.txt" COPYONLY)
# Codec notices are embedded in these source distributions. Preserve them in full.
configure_file("${libchdr_SOURCE_DIR}/deps/miniz-3.1.2/miniz.h" "${AC_DEPENDENCY_LICENSE_DIR}/libchdr-miniz-MIT.txt" COPYONLY)
configure_file("${libchdr_SOURCE_DIR}/deps/zstd-1.5.7/zstddeclib.c" "${AC_DEPENDENCY_LICENSE_DIR}/libchdr-zstd-BSD-GPL.txt" COPYONLY)
configure_file("${libchdr_SOURCE_DIR}/include/dr_libs/dr_flac.h" "${AC_DEPENDENCY_LICENSE_DIR}/libchdr-drflac-notices.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/scanner-dependencies.json" "${AC_DEPENDENCY_LICENSE_DIR}/scanner-dependencies.json" COPYONLY)
