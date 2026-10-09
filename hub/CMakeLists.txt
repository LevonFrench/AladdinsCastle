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
include(cmake/ProjectWarnings.cmake)
qt_standard_project_setup(REQUIRES 6.8)

add_library(hubcore STATIC
    src/core/catalog/CatalogPaths.cpp
    src/core/catalog/CatalogPaths.h
    src/core/app/LaunchOptions.cpp
    src/core/app/LaunchOptions.h
    src/core/config/Config.h
    src/core/filter/Filter.h
    src/core/scan/Scan.h
    src/core/install/Install.h
    src/core/update/Update.h
    src/core/launch/Launch.h
    src/core/settings/Settings.h
    src/core/art/Art.h)
target_compile_features(hubcore PUBLIC cxx_std_20)
target_include_directories(hubcore PUBLIC src)
target_link_libraries(hubcore PUBLIC Qt6::Core Qt6::Network Qt6::Concurrent
    tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator::validator)
ac_project_warnings(hubcore)

add_library(huboverlay STATIC
    src/overlay/OverlayInput.cpp src/overlay/OverlayInput.h
    src/overlay/QuickTextureRenderer.cpp src/overlay/QuickTextureRenderer.h
    src/overlay/OverlayRuntime.cpp src/overlay/OverlayRuntime.h
    src/overlay/OverlayHost.cpp src/overlay/OverlayHost.h src/overlay/SpikeState.h)
target_include_directories(huboverlay PUBLIC src)
target_link_libraries(huboverlay PUBLIC Qt6::Quick Qt6::OpenGL OpenVR::OpenVR)
ac_project_warnings(huboverlay)

qt_add_executable(aladdinscastle-hub src/app/main.cpp)
target_link_libraries(aladdinscastle-hub PRIVATE hubcore huboverlay Qt6::Quick
    Qt6::QuickControls2 Qt6::OpenGL Qt6::ShaderTools OpenVR::OpenVR)
set_source_files_properties(qml/HubRoot.qml PROPERTIES QT_RESOURCE_ALIAS HubRoot.qml)
set_source_files_properties(qml/DesktopShell.qml PROPERTIES QT_RESOURCE_ALIAS DesktopShell.qml)
set_source_files_properties(qml/spike/OverlayTest.qml PROPERTIES QT_RESOURCE_ALIAS spike/OverlayTest.qml)
qt_add_qml_module(aladdinscastle-hub
    URI AladdinsCastle.Hub VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/AladdinsCastle/Hub"
    QML_FILES qml/HubRoot.qml qml/DesktopShell.qml qml/spike/OverlayTest.qml)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")
configure_file("../docs/ui/theme.toml" "resources/theme.toml" COPYONLY)
qt_add_resources(aladdinscastle-hub theme
    PREFIX /resources
    BASE "${CMAKE_CURRENT_BINARY_DIR}/resources"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/theme.toml")
ac_project_warnings(aladdinscastle-hub)
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")
configure_file(resources/aladdinscastle.vrmanifest "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/aladdinscastle.vrmanifest" COPYONLY)
configure_file(resources/overlay-thumbnail.png "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/overlay-thumbnail.png" COPYONLY)
add_custom_command(TARGET aladdinscastle-hub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aladdinscastle-hub>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/aladdinscastle.vrmanifest"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/overlay-thumbnail.png"
        "$<TARGET_FILE_DIR:aladdinscastle-hub>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OpenVR::OpenVR>" "$<TARGET_FILE_DIR:aladdinscastle-hub>"
    VERBATIM)
set_target_properties(aladdinscastle-hub PROPERTIES
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")

qt_add_executable(hubtool src/app/hubtool.cpp)
target_link_libraries(hubtool PRIVATE hubcore)
ac_project_warnings(hubtool)

if(BUILD_TESTING)
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Test)
    qt_add_executable(hub-tests tests/SmokeTest.cpp)
    target_link_libraries(hub-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(hub-tests)
    add_test(NAME hub-smoke COMMAND hub-tests)
    qt_add_executable(overlay-tests tests/OverlayTest.cpp)
    target_link_libraries(overlay-tests PRIVATE huboverlay hubcore Qt6::Test)
    ac_project_warnings(overlay-tests)
    set_target_properties(overlay-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME overlay-synthetic COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:overlay-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/overlay-synthetic.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    qt_add_executable(overlay-scene-tests tests/OverlaySceneTest.cpp)
    target_link_libraries(overlay-scene-tests PRIVATE huboverlay Qt6::Test Qt6::QuickControls2)
    target_compile_definitions(overlay-scene-tests PRIVATE AC_SPIKE_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/qml/spike/OverlayTest.qml")
    ac_project_warnings(overlay-scene-tests)
    set_target_properties(overlay-scene-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME overlay-scene COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:overlay-scene-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/overlay-scene.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    set_tests_properties(overlay-scene PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
