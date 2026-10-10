include(FetchContent)

# First declaration wins, so declare json before the schema validator.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_INSTALL OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0 GIT_SHALLOW TRUE SYSTEM)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0 GIT_SHALLOW TRUE SYSTEM)
FetchContent_Declare(json_schema_validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG 2.4.0 GIT_SHALLOW TRUE SYSTEM)
# Only consume upstream headers and its prebuilt library, never its build system.
FetchContent_Declare(openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG v2.15.6 GIT_SHALLOW TRUE
    SOURCE_SUBDIR _headers_and_prebuilt_only SYSTEM)
FetchContent_MakeAvailable(tomlplusplus nlohmann_json json_schema_validator openvr)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ac_processor)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
        NOT _ac_processor MATCHES "^(amd64|x86_64|x64)$")
    message(FATAL_ERROR "M1 uses the OpenVR x86_64 prebuilt library. ARM64 needs a later source-build lane.")
endif()
add_library(OpenVR::OpenVR SHARED IMPORTED GLOBAL)
set_target_properties(OpenVR::OpenVR PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${openvr_SOURCE_DIR}/headers")
if(WIN32)
    set_target_properties(OpenVR::OpenVR PROPERTIES
        IMPORTED_IMPLIB "${openvr_SOURCE_DIR}/lib/win64/openvr_api.lib"
        IMPORTED_LOCATION "${openvr_SOURCE_DIR}/bin/win64/openvr_api.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(OpenVR::OpenVR PROPERTIES
        IMPORTED_LOCATION "${openvr_SOURCE_DIR}/bin/linux64/libopenvr_api.so")
else()
    message(FATAL_ERROR "M1 supports Windows x64 and Linux x86_64 only.")
endif()
get_target_property(_openvr_runtime OpenVR::OpenVR IMPORTED_LOCATION)
if(NOT EXISTS "${_openvr_runtime}")
    message(FATAL_ERROR "The pinned OpenVR runtime is missing: ${_openvr_runtime}")
endif()

# Preserve dependency license texts alongside build output for packaging.
set(AC_DEPENDENCY_LICENSE_DIR "${CMAKE_BINARY_DIR}/licenses")
file(MAKE_DIRECTORY "${AC_DEPENDENCY_LICENSE_DIR}")
configure_file("${tomlplusplus_SOURCE_DIR}/LICENSE" "${AC_DEPENDENCY_LICENSE_DIR}/tomlplusplus-MIT.txt" COPYONLY)
configure_file("${nlohmann_json_SOURCE_DIR}/LICENSE.MIT" "${AC_DEPENDENCY_LICENSE_DIR}/nlohmann-json-MIT.txt" COPYONLY)
configure_file("${json_schema_validator_SOURCE_DIR}/LICENSE" "${AC_DEPENDENCY_LICENSE_DIR}/json-schema-validator-MIT.txt" COPYONLY)
configure_file("${openvr_SOURCE_DIR}/LICENSE" "${AC_DEPENDENCY_LICENSE_DIR}/OpenVR-BSD-3-Clause.txt" COPYONLY)
