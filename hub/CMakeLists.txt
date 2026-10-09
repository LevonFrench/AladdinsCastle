cmake_minimum_required(VERSION 3.28)
project(AladdinsCastleHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS
    Core Network Concurrent Quick QuickControls2 OpenGL ShaderTools)
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Use shared Qt: portable packages require replaceable Qt DLLs.")
endif()

include(cmake/Dependencies.cmake)
include(cmake/ArchiveDependencies.cmake)
include(cmake/ProjectWarnings.cmake)
qt_standard_project_setup(REQUIRES 6.8)

add_library(hubcore STATIC
    src/core/catalog/CatalogLoader.cpp
    src/core/catalog/GameRecord.cpp
    src/models/GameListModel.cpp
    src/models/GameListModel.h
    src/models/FilterSortModel.cpp
    src/models/FilterSortModel.h
    src/core/catalog/CatalogPaths.cpp
    src/core/catalog/CatalogPaths.h
    src/core/app/LaunchOptions.cpp
    src/core/app/LaunchOptions.h
    src/core/config/Config.h
    src/core/filter/Filter.h
    src/core/scan/Scan.h
    src/core/install/Install.h
    src/core/install/Install.cpp
    src/core/install/Support.cpp
    src/core/install/Support.h
    src/core/install/Archive.cpp
    src/core/install/ConfigEdit.cpp
    src/core/install/ArtifactStore.cpp
    src/core/install/ArtifactStore.h
    src/core/install/FlatLaunch.cpp
    src/core/update/Update.h
    src/core/launch/Launch.h
    src/core/launch/Launch.cpp
    src/core/launch/Runtime.cpp
    src/core/steam/Steam.h
    src/core/steam/Steam.cpp
    src/core/steam/BinaryVdf.cpp
    src/core/settings/Settings.h
    src/core/art/Art.h)
target_compile_features(hubcore PUBLIC cxx_std_20)
target_include_directories(hubcore PUBLIC src)
target_link_libraries(hubcore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent
    tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator::validator)
target_link_libraries(hubcore PUBLIC archive_static)
target_compile_definitions(hubcore PUBLIC NOMINMAX)
ac_project_warnings(hubcore)

qt_add_executable(aladdinscastle-hub src/app/main.cpp)
target_link_libraries(aladdinscastle-hub PRIVATE hubcore Qt6::Quick
    Qt6::QuickControls2 Qt6::OpenGL Qt6::ShaderTools OpenVR::OpenVR)
set_source_files_properties(qml/HubRoot.qml PROPERTIES QT_RESOURCE_ALIAS HubRoot.qml)
set_source_files_properties(qml/DesktopShell.qml PROPERTIES QT_RESOURCE_ALIAS DesktopShell.qml)
qt_add_qml_module(aladdinscastle-hub
    URI AladdinsCastle.Hub VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/AladdinsCastle/Hub"
    QML_FILES qml/HubRoot.qml qml/DesktopShell.qml)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")
configure_file("../docs/ui/theme.toml" "resources/theme.toml" COPYONLY)
qt_add_resources(aladdinscastle-hub theme
    PREFIX /resources
    BASE "${CMAKE_CURRENT_BINARY_DIR}/resources"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/theme.toml")
ac_project_warnings(aladdinscastle-hub)
add_custom_command(TARGET aladdinscastle-hub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OpenVR::OpenVR>" "$<TARGET_FILE_DIR:aladdinscastle-hub>"
    VERBATIM)
set_target_properties(aladdinscastle-hub PROPERTIES
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")

qt_add_executable(hubtool src/app/hubtool.cpp)
target_link_libraries(hubtool PRIVATE hubcore)
ac_project_warnings(hubtool)
qt_add_executable(steamtool src/app/steamtool.cpp)
target_link_libraries(steamtool PRIVATE hubcore)
ac_project_warnings(steamtool)
set_target_properties(steamtool PROPERTIES WIN32_EXECUTABLE FALSE)

if(BUILD_TESTING)
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Test)
    qt_add_executable(hub-tests tests/SmokeTest.cpp)
    target_link_libraries(hub-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(hub-tests)
    add_test(NAME hub-smoke COMMAND hub-tests)
    qt_add_executable(catalog-tests tests/CatalogTest.cpp)
    target_link_libraries(catalog-tests PRIVATE hubcore Qt6::Test)
    target_compile_definitions(catalog-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(catalog-tests)
    add_test(NAME hub-catalog COMMAND catalog-tests)
    qt_add_executable(install-tests tests/InstallTest.cpp)
    target_link_libraries(install-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(install-tests)
    target_compile_definitions(install-tests PRIVATE AC_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME hub-install COMMAND install-tests)
    qt_add_executable(launch-tests tests/LaunchSteamTest.cpp)
    target_link_libraries(launch-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(launch-tests)
    set_target_properties(launch-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-launch-steam COMMAND launch-tests)
    if(NOT WIN32)
        set_tests_properties(hub-launch-steam PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
