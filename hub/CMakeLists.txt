# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.28)
project(AladdinsCastleHub VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(cmake/ProductIdentity.cmake)
include(CTest)
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS
    Core Network Concurrent Quick QuickControls2 OpenGL ShaderTools)
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Use shared Qt: portable packages require replaceable Qt DLLs.")
endif()

include(cmake/Dependencies.cmake)
include(cmake/ArchiveDependencies.cmake)
include(cmake/SevenZip.cmake)
include(cmake/ChdMetadata.cmake)
include(cmake/ProjectWarnings.cmake)
qt_standard_project_setup(REQUIRES 6.8)

get_filename_component(_ac_hand_root "${CMAKE_CURRENT_SOURCE_DIR}/../libacvr" ABSOLUTE)
add_library(hubhandmetadata STATIC "${_ac_hand_root}/src/hand_assembly.cpp")
target_compile_features(hubhandmetadata PUBLIC cxx_std_20)
set_target_properties(hubhandmetadata PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hubhandmetadata PUBLIC "${_ac_hand_root}/src")
target_link_libraries(hubhandmetadata PRIVATE
    tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json)
ac_project_warnings(hubhandmetadata)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/licenses")
configure_file("${_ac_hand_root}/LICENSE" "licenses/libacvr-MIT.txt" COPYONLY)
set(_ac_hand_receipt "Project source: https://github.com/LevonFrench/AladdinsCastle\nApplication source commit: see ../BUILD-INFO.txt\n")
foreach(_ac_hand_file src/hand_assembly.cpp src/hand_assembly.hpp src/gun_asset.hpp LICENSE)
    set(_ac_hand_path "${_ac_hand_root}/${_ac_hand_file}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ac_hand_path}")
    file(SHA256 "${_ac_hand_path}" _ac_hand_sha256)
    string(APPEND _ac_hand_receipt "libacvr/${_ac_hand_file}: ${_ac_hand_sha256}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/licenses/libacvr-source.txt" "${_ac_hand_receipt}")

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
    src/core/app/LaunchFailure.cpp
    src/core/app/LaunchFailure.h
    src/core/app/LogBundle.cpp
    src/core/app/LogBundle.h
    src/core/app/NetworkPolicy.cpp
    src/core/app/NetworkPolicy.h
    src/core/config/Config.h
    src/core/filter/Filter.h
    src/core/scan/Scan.h
    src/core/scan/Scan.cpp
    src/core/scan/Headers.cpp
    src/core/scan/DiscReader.cpp
    src/core/scan/DiscIndex.cpp
    src/core/scan/DiscMemo.h
    src/core/scan/DiscMemo.cpp
    src/core/pinball/Art.cpp
    src/core/pinball/Art.h
    src/core/pinball/Build.cpp
    src/core/pinball/Build.h
    src/core/pinball/Gamepad.cpp
    src/core/pinball/Gamepad.h
    src/core/pinball/Pinball.cpp
    src/core/pinball/Pinball.h
    src/core/pinball/Script.cpp
    src/core/pinball/Script.h
    src/core/pinball/Roms.cpp
    src/core/pinball/Roms.h
    src/core/pinball/RomList.cpp
    src/core/pinball/RomList.h
    src/core/pinball/Scores.cpp
    src/core/pinball/Scores.h
    src/core/pinball/Identity.cpp
    src/core/pinball/Identity.h
    src/core/pinball/VpsMatch.cpp
    src/core/pinball/VpsMatch.h
    src/core/match/TitleMatch.cpp
    src/core/match/TitleMatch.h
    src/core/model3/Model3.cpp
    src/core/model3/Model3.h
    src/core/ps2/Ps2.cpp
    src/core/ps2/Ps2.h
    src/core/ps1/Ps1.cpp
    src/core/ps1/Ps1.h
    src/core/n64/N64.cpp
    src/core/n64/N64.h
    src/core/saturn/Saturn.cpp
    src/core/saturn/Saturn.h
    src/core/model2/Model2.cpp
    src/core/model2/Model2.h
    src/core/dreamcast/Dreamcast.cpp
    src/core/dreamcast/Dreamcast.h
    src/core/scripted/Scripted.cpp
    src/core/scripted/Scripted.h
    src/core/system22/System22.cpp
    src/core/system22/System22.h
    src/core/setup/Setup.cpp
    src/core/setup/Setup.h
    src/core/setup/Report.cpp
    src/core/setup/Report.h
    src/core/scan/HeaderSafety.cpp
    src/core/scan/HeaderSafety.h
    src/core/scan/ScanController.cpp
    src/core/scan/ScanController.h
    src/core/install/Install.h
    src/core/install/Install.cpp
    src/core/install/Support.cpp
    src/core/install/Support.h
    src/core/install/ErrorText.cpp
    src/core/install/ErrorText.h
    src/core/install/Archive.cpp
    src/core/install/ConfigEdit.cpp
    src/core/install/ArtifactStore.cpp
    src/core/install/ArtifactStore.h
    src/core/install/FlatLaunch.cpp
    src/core/update/Update.h
    src/core/update/Update.cpp
    src/core/launch/Launch.h
    src/core/launch/Launch.cpp
    src/core/launch/ProcessTree.cpp
    src/core/launch/ProcessTree.h
    src/core/launch/Runtime.cpp
    src/core/gunroom/GunRoom.cpp
    src/core/gunroom/GunRoom.h
    src/core/steam/Steam.h
    src/core/steam/Steam.cpp
    src/core/steam/BinaryVdf.cpp
    src/core/settings/Settings.h
    src/core/art/Art.h
    src/core/art/Art.cpp
    src/core/art/Download.h
    src/core/art/Download.cpp
    src/core/launch/SteamVrStart.h
    src/core/launch/SteamVrStart.cpp
    src/core/app/PanelAttacher.h
    src/core/app/PanelAttacher.cpp
    src/core/launch/Watchdog.h
    src/core/launch/Watchdog.cpp
    src/core/wii/Wii.h
    src/core/wii/Wii.cpp
    src/core/libretro/Libretro.h
    src/core/libretro/Libretro.cpp
    src/core/library/Library.h
    src/core/library/Library.cpp
    src/core/settings/Secret.h
    src/core/settings/Secret.cpp)
target_compile_features(hubcore PUBLIC cxx_std_20)
target_include_directories(hubcore PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(hubcore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent
    tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator::validator)
target_link_libraries(hubcore PUBLIC archive_static)
target_compile_definitions(hubcore PUBLIC NOMINMAX)
target_link_libraries(hubcore PRIVATE ac7zmetadata)
target_link_libraries(hubcore PRIVATE hubhandmetadata)
target_link_libraries(hubcore PRIVATE chdr-static)
target_link_libraries(hubcore PRIVATE zlibstatic)
target_link_libraries(hubcore PUBLIC Qt6::Quick)
if(WIN32)
    target_link_libraries(hubcore PRIVATE version shell32 ole32 oleaut32 advapi32 uuid crypt32)
endif()
ac_project_warnings(hubcore)
qt_add_resources(hubcore disc_metadata PREFIX /data BASE ../data FILES ../data/serial-index.json ../data/wii-disc-index.json ../data/dreamcast-disc-index.json ../data/disc-profiles.json)
qt_add_resources(hubcore scan_metadata PREFIX /scan BASE ../data FILES ../data/serial-index.json ../data/wii-disc-index.json)
# High score locations from the Pinball Memory Maps project (ODbL), built by tools/pinball_score_maps.py.
qt_add_resources(hubcore pinball_scores PREFIX /pinball BASE ../data FILES ../data/pinball-score-maps.json)

add_library(huboverlay STATIC
    src/overlay/OverlayInput.cpp src/overlay/OverlayInput.h
    src/overlay/QuickTextureRenderer.cpp src/overlay/QuickTextureRenderer.h
    src/overlay/OverlayRuntime.cpp src/overlay/OverlayRuntime.h
    src/overlay/OverlayCapture.cpp src/overlay/OverlayCapture.h
    src/overlay/CompositorCapture.cpp src/overlay/CompositorCapture.h
    src/overlay/OverlayKeyboard.cpp src/overlay/OverlayKeyboard.h
    src/overlay/OverlayHost.cpp src/overlay/OverlayHost.h src/overlay/SpikeState.h)
target_include_directories(huboverlay PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(huboverlay PUBLIC hubcore Qt6::Quick Qt6::OpenGL OpenVR::OpenVR)
ac_project_warnings(huboverlay)

qt_add_library(hubui STATIC)
add_subdirectory(src/controls)
target_link_libraries(hubui PUBLIC hubcore accontrols Qt6::Quick Qt6::QuickControls2)
# Archivo (SIL Open Font License 1.1), shipped so the Hub looks the same everywhere and downloads nothing.
qt_add_resources(hubui hub_fonts PREFIX /fonts BASE resources/fonts FILES resources/fonts/Archivo-Medium.ttf resources/fonts/Archivo-Bold.ttf resources/fonts/Archivo-Black.ttf resources/fonts/OFL.txt)
file(GLOB AC_QML_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")
foreach(qml_file IN LISTS AC_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set_source_files_properties(qml/spike/OverlayTest.qml PROPERTIES QT_RESOURCE_ALIAS spike/OverlayTest.qml)
qt_add_qml_module(hubui URI AladdinsCastle.Hub VERSION 1.0 NO_PLUGIN
    RESOURCE_PREFIX /qt/qml OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/AladdinsCastle/Hub"
    QML_FILES ${AC_QML_FILES} qml/spike/OverlayTest.qml
    SOURCES src/ui/Theme.h src/ui/Theme.cpp src/ui/UiSettings.h src/ui/UiSettings.cpp
        src/ui/UiController.h src/ui/UiController.cpp
        src/ui/DetailControlsController.h src/ui/DetailControlsController.cpp)
ac_project_warnings(hubui)
add_library(hubapp STATIC src/app/HubServices.cpp src/app/HubServices.h src/app/LaunchProof.cpp src/app/LaunchProof.h)
target_link_libraries(hubapp PUBLIC hubui)
ac_project_warnings(hubapp)
qt_add_executable(aladdinscastle-hub src/app/main.cpp)
target_link_libraries(aladdinscastle-hub PRIVATE hubapp huboverlay Qt6::OpenGL Qt6::ShaderTools OpenVR::OpenVR)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")
configure_file("../docs/ui/theme.toml" "resources/theme.toml" COPYONLY)
qt_add_resources(aladdinscastle-hub theme
    PREFIX /resources BASE "${CMAKE_CURRENT_BINARY_DIR}/resources"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/theme.toml")
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")
configure_file(resources/dashboard.vrmanifest.in "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/${AC_PRODUCT_short_id}.vrmanifest" @ONLY)
configure_file(resources/overlay-thumbnail.png "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/overlay-thumbnail.png" COPYONLY)
foreach(_ac_input hub-actions.json hub-bindings-oculus_touch.json hub-bindings-knuckles.json)
    configure_file(resources/${_ac_input} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/${_ac_input}" COPYONLY)
endforeach()
add_custom_target(ac-hub-runtime-resources
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aladdinscastle-hub>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/${AC_PRODUCT_short_id}.vrmanifest"
        "$<TARGET_FILE_DIR:aladdinscastle-hub>/resources/${AC_PRODUCT_short_id}.vrmanifest"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/overlay-thumbnail.png"
        "$<TARGET_FILE_DIR:aladdinscastle-hub>/resources/overlay-thumbnail.png"
    VERBATIM)
add_dependencies(aladdinscastle-hub ac-hub-runtime-resources)
ac_project_warnings(aladdinscastle-hub)
add_custom_command(TARGET aladdinscastle-hub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OpenVR::OpenVR>" "$<TARGET_FILE_DIR:aladdinscastle-hub>"
    VERBATIM)
if(MSVC)
    target_link_options(aladdinscastle-hub PRIVATE "/DELAYLOAD:openvr_api.dll")
    target_link_libraries(aladdinscastle-hub PRIVATE delayimp)
endif()
set_target_properties(aladdinscastle-hub PROPERTIES
    OUTPUT_NAME "${AC_PRODUCT_program_name}"
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN" WIN32_EXECUTABLE TRUE)

qt_add_executable(hubtool src/app/hubtool.cpp)
target_link_libraries(hubtool PRIVATE hubcore)
ac_project_warnings(hubtool)
qt_add_executable(arttool src/app/arttool.cpp)
target_link_libraries(arttool PRIVATE hubcore)
ac_project_warnings(arttool)
qt_add_executable(steamtool src/app/steamtool.cpp)
target_link_libraries(steamtool PRIVATE hubcore)
ac_project_warnings(steamtool)
set_target_properties(steamtool PROPERTIES WIN32_EXECUTABLE FALSE)

if(BUILD_TESTING)
    find_program(_ac_resource_ninja NAMES ninja)
    if(_ac_resource_ninja)
        add_test(NAME hub-runtime-resources COMMAND ${CMAKE_COMMAND}
            "-DHUB_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DTEST_ROOT=${CMAKE_BINARY_DIR}/Testing/runtime-resources"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DNINJA=${_ac_resource_ninja}"
            "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RuntimeResources.cmake)
    else()
        message(STATUS "Skipping hub-runtime-resources: Ninja is unavailable for the extra generator regression.")
    endif()
    qt_add_executable(gun-room-tests tests/GunRoomTest.cpp)
    target_link_libraries(gun-room-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(gun-room-tests)
    add_test(NAME hub-gun-room COMMAND gun-room-tests)
    qt_add_executable(model3-tests tests/Model3Test.cpp)
    target_link_libraries(model3-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(model3-tests)
    set_target_properties(model3-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-model3 COMMAND model3-tests -o -,txt)
    qt_add_executable(setup-tests tests/SetupTest.cpp)
    target_link_libraries(setup-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(setup-tests)
    set_target_properties(setup-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-setup COMMAND setup-tests -o -,txt)
    qt_add_executable(library-tests tests/LibraryTest.cpp)
    target_link_libraries(library-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(library-tests)
    set_target_properties(library-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-library COMMAND library-tests -o -,txt)
    qt_add_executable(disc-format-tests tests/DiscFormatsTest.cpp)
    target_link_libraries(disc-format-tests PRIVATE hubcore Qt6::Test zlibstatic)
    ac_project_warnings(disc-format-tests)
    set_target_properties(disc-format-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-disc-formats COMMAND disc-format-tests -o -,txt)
    qt_add_executable(disc-index-tests tests/DiscIndexTest.cpp)
    target_link_libraries(disc-index-tests PRIVATE hubcore Qt6::Test)
    target_compile_definitions(disc-index-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(disc-index-tests)
    set_target_properties(disc-index-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-disc-index COMMAND disc-index-tests -o -,txt)
    qt_add_executable(disc-inventory tests/DiscInventory.cpp)
    target_link_libraries(disc-inventory PRIVATE hubcore)
    ac_project_warnings(disc-inventory)
    set_target_properties(disc-inventory PROPERTIES WIN32_EXECUTABLE FALSE)
    qt_add_executable(wii-tests tests/WiiTest.cpp)
    target_link_libraries(wii-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(wii-tests)
    set_target_properties(wii-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-wii COMMAND wii-tests -o -,txt)
    qt_add_executable(libretro-tests tests/LibretroTest.cpp)
    target_link_libraries(libretro-tests PRIVATE hubcore Qt6::Test)
    target_compile_definitions(libretro-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(libretro-tests)
    set_target_properties(libretro-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-libretro COMMAND libretro-tests -o -,txt)
    qt_add_executable(watchdog-tests tests/WatchdogTest.cpp)
    target_link_libraries(watchdog-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(watchdog-tests)
    set_target_properties(watchdog-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-watchdog COMMAND watchdog-tests -o -,txt)
    qt_add_executable(log-bundle-tests tests/LogBundleTest.cpp)
    target_link_libraries(log-bundle-tests PRIVATE hubcore Qt6::Test zlibstatic)
    ac_project_warnings(log-bundle-tests)
    set_target_properties(log-bundle-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-log-bundle COMMAND log-bundle-tests -o -,txt)
    qt_add_executable(art-download-tests tests/ArtDownloadTest.cpp)
    target_link_libraries(art-download-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(art-download-tests)
    set_target_properties(art-download-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-art-download COMMAND art-download-tests -o -,txt)
    qt_add_executable(update-tests tests/UpdateTest.cpp)
    target_link_libraries(update-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(update-tests)
    set_target_properties(update-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-update COMMAND update-tests -o -,txt)
    qt_add_executable(title-match-tests tests/TitleMatchTest.cpp)
    target_link_libraries(title-match-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(title-match-tests)
    set_target_properties(title-match-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-title-match COMMAND title-match-tests -o -,txt)
    qt_add_executable(vps-match-tests tests/VpsMatchTest.cpp)
    target_link_libraries(vps-match-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(vps-match-tests)
    set_target_properties(vps-match-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-vps-match COMMAND vps-match-tests -o -,txt)
    qt_add_executable(pinball-settings-tests tests/PinballSettingsTest.cpp)
    target_link_libraries(pinball-settings-tests PRIVATE hubcore Qt6::Test)
    ac_project_warnings(pinball-settings-tests)
    set_target_properties(pinball-settings-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-pinball-settings COMMAND pinball-settings-tests -o -,txt)
    qt_add_executable(pinball-cache-tests tests/PinballCacheTest.cpp)
    target_link_libraries(pinball-cache-tests PRIVATE hubapp Qt6::Test)
    ac_project_warnings(pinball-cache-tests)
    set_target_properties(pinball-cache-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-pinball-cache COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:pinball-cache-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/pinball-cache.txt
        -DTEST_TIMEOUT=60
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    set_tests_properties(hub-pinball-cache PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME emulator-pin-tool COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pin_artifact.py --test)
    add_test(NAME emulator-manifest-lint COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate_emulator_manifests.py --test)
    add_test(NAME emulator-manifests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate_emulator_manifests.py)
    add_test(NAME disc-id-survey-formats COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/disc_ids.py --test)
    add_test(NAME hub-title-match-policy COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/title_match_policy.py)
    add_test(NAME hub-pinball COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-pinball-script COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_script_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-pinball-roms COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_rom_inventory_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-pinball-rom-check COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_rom_check_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-pinball-scores COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_score_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-pinball-identity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pinball_identity_fixtures.py $<TARGET_FILE:hubtool>)
    add_test(NAME hub-product-identity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/product_identity.py ${CMAKE_COMMAND})
    add_test(NAME hub-universal-controls-text COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/universal_controls_text.py)
    add_test(NAME hub-test-runner COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/runner_fixtures.py
        ${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    add_test(NAME serial-index-policy COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/build_serial_index.py --test)
    add_test(NAME catalog-policy COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate_catalog.py --test)
    add_test(NAME build-policy COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_build_policy.py)
    add_test(NAME public-privacy COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_public_privacy.py --test)

    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Test)
    qt_add_executable(detail-controls-tests tests/DetailControlsTest.cpp)
    target_link_libraries(detail-controls-tests PRIVATE hubui Qt6::Test)
    target_compile_definitions(detail-controls-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(detail-controls-tests)
    set_target_properties(detail-controls-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-detail-controls COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:detail-controls-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/detail-controls.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    qt_add_executable(hub-tests tests/SmokeTest.cpp)
    target_link_libraries(hub-tests PRIVATE hubcore Qt6::Test)
    add_dependencies(hub-tests aladdinscastle-hub hubtool)
    add_dependencies(hub-tests pinball-cache-tests)
    if(MSVC)
        # Link the real entry point without its adjacent runtime DLL. Smoke tests must not copy a large
        # executable into the system temporary folder just to exercise the missing-runtime path.
        qt_add_executable(hub-without-runtime src/app/main.cpp)
        target_link_libraries(hub-without-runtime PRIVATE hubapp huboverlay Qt6::OpenGL Qt6::ShaderTools OpenVR::OpenVR)
        qt_add_resources(hub-without-runtime smoke_theme PREFIX /resources
            BASE "${CMAKE_CURRENT_BINARY_DIR}/resources" FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/theme.toml")
        set_target_properties(hub-without-runtime PROPERTIES WIN32_EXECUTABLE TRUE
            OUTPUT_NAME "${AC_PRODUCT_program_name}" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Testing/no-runtime/$<CONFIG>")
        target_link_options(hub-without-runtime PRIVATE "/DELAYLOAD:openvr_api.dll")
        target_link_libraries(hub-without-runtime PRIVATE delayimp)
        add_custom_command(TARGET hub-without-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:hub-without-runtime>/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/${AC_PRODUCT_short_id}.vrmanifest"
                "$<TARGET_FILE_DIR:hub-without-runtime>/resources/${AC_PRODUCT_short_id}.vrmanifest" VERBATIM)
        ac_project_warnings(hub-without-runtime)
        add_dependencies(hub-tests hub-without-runtime)
        target_compile_definitions(hub-tests PRIVATE AC_NO_RUNTIME_HUB="$<TARGET_FILE:hub-without-runtime>")
    endif()
    ac_project_warnings(hub-tests)
    add_test(NAME hub-smoke COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:hub-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/hub-smoke.txt
        -DTEST_ARGUMENTS=-v1
        -DTEST_TIMEOUT=240
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    qt_add_executable(catalog-tests tests/CatalogTest.cpp)
    target_link_libraries(catalog-tests PRIVATE hubcore Qt6::Test)
    target_compile_definitions(catalog-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.." AC_PYTHON_EXECUTABLE="${Python3_EXECUTABLE}")
    ac_project_warnings(catalog-tests)
    add_test(NAME hub-catalog COMMAND catalog-tests)
    qt_add_executable(install-tests tests/InstallTest.cpp)
    target_link_libraries(install-tests PRIVATE hubcore Qt6::Test)
    add_dependencies(install-tests hubtool)
    ac_project_warnings(install-tests)
    target_compile_definitions(install-tests PRIVATE AC_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME hub-install COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:install-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/hub-install.txt
        -DTEST_ARGUMENTS=-v1
        -DTEST_TIMEOUT=1440
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    qt_add_executable(ui-tests tests/UiTest.cpp)
    target_link_libraries(ui-tests PRIVATE hubui Qt6::Test)
    if(WIN32)
        target_link_libraries(ui-tests PRIVATE psapi)
    endif()
    target_compile_definitions(ui-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(ui-tests)
    add_test(NAME hub-ui COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:ui-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/hub-ui.txt
        -DTEST_ARGUMENTS=-v1
        -DTEST_TIMEOUT=180
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    set_tests_properties(hub-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    qt_add_executable(scan-art-tests tests/ScanArtTest.cpp)
    target_link_libraries(scan-art-tests PRIVATE hubcore Qt6::Test)
    target_compile_definitions(scan-art-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    ac_project_warnings(scan-art-tests)
    add_test(NAME hub-scan-art COMMAND scan-art-tests)
    set_tests_properties(hub-scan-art PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(overlay-tests tests/OverlayTest.cpp)
    target_link_libraries(overlay-tests PRIVATE huboverlay hubcore Qt6::Test)
    ac_project_warnings(overlay-tests)
    set_target_properties(overlay-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME overlay-synthetic COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:overlay-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/overlay-synthetic.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    qt_add_executable(overlay-capture-tests tests/OverlayCaptureTest.cpp)
    target_link_libraries(overlay-capture-tests PRIVATE huboverlay Qt6::Test)
    ac_project_warnings(overlay-capture-tests)
    set_target_properties(overlay-capture-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME overlay-capture-cpu COMMAND overlay-capture-tests -o -,txt)
    qt_add_executable(compositor-capture-tests tests/CompositorCaptureTest.cpp)
    target_link_libraries(compositor-capture-tests PRIVATE huboverlay Qt6::Test)
    ac_project_warnings(compositor-capture-tests)
    set_target_properties(compositor-capture-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME compositor-capture-cpu COMMAND compositor-capture-tests -o -,txt)
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
    # The stand-in game the tests copy under emulator names and start: small and Qt-only, because a copy of the
    # whole test program under game names in the temporary folder was flagged and deleted by Windows Defender.
    qt_add_executable(test-stand-in tests/StandIn.cpp)
    target_link_libraries(test-stand-in PRIVATE Qt6::Gui)
    ac_project_warnings(test-stand-in)
    qt_add_executable(integration-tests tests/IntegrationTest.cpp)
    target_link_libraries(integration-tests PRIVATE hubapp Qt6::Test)
    target_compile_definitions(integration-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.." AC_STAND_IN="$<TARGET_FILE:test-stand-in>")
    ac_project_warnings(integration-tests)
    add_dependencies(integration-tests test-stand-in)
    add_dependencies(hub-tests integration-tests test-stand-in)
    # Through the receipt runner: a Windows GUI test has no console for -o -,txt, so a crash left no report.
    add_test(NAME hub-integration COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:integration-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/hub-integration.txt
        -DTEST_ARGUMENTS=-v1
        -DTEST_TIMEOUT=300
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    set_tests_properties(hub-integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    qt_add_executable(launch-tests tests/LaunchSteamTest.cpp)
    target_link_libraries(launch-tests PRIVATE hubcore Qt6::Test)
    add_dependencies(launch-tests steamtool hubtool test-stand-in)
    target_compile_definitions(launch-tests PRIVATE AC_CATALOG_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.." AC_STAND_IN="$<TARGET_FILE:test-stand-in>")
    ac_project_warnings(launch-tests)
    set_target_properties(launch-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME hub-launch-steam COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:launch-tests>
        -DTEST_LOG=${CMAKE_BINARY_DIR}/Testing/hub-launch-steam.txt
        -DTEST_ARGUMENTS=-v1
        -DTEST_TIMEOUT=240
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake)
    if(NOT WIN32)
        set_tests_properties(hub-launch-steam PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    # The outer deadline also covers wrappers and their cleanup, including direct Python tests.
    get_property(_ac_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${_ac_tests} PROPERTIES TIMEOUT 180)
    set_tests_properties(hub-smoke hub-launch-steam PROPERTIES TIMEOUT 270)
    # hub-install: the inner budget is about 3x the slowest observed Windows run (478.5 s on a loaded PC; 137 s on CI).
    set_tests_properties(hub-install PROPERTIES TIMEOUT 1500)
    set_tests_properties(hub-ui PROPERTIES TIMEOUT 210)
    set_tests_properties(hub-integration PROPERTIES TIMEOUT 330)
    if(TEST hub-runtime-resources)
        set_tests_properties(hub-runtime-resources PROPERTIES TIMEOUT 300)
    endif()
    # The heavy offscreen Qt tests are CPU-bound; under `ctest -j` they must not run together.
    set_tests_properties(hub-smoke hub-install hub-integration hub-launch-steam hub-ui PROPERTIES RESOURCE_LOCK heavy_gui)
    # A failed smoke test copies its temporary folder here, where CI uploads it.
    set_tests_properties(hub-smoke PROPERTIES ENVIRONMENT "AC_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/Testing/artifacts")
    # Optional capture/benchmark modes require an explicit direct invocation, not an inherited CTest environment.
    set_tests_properties(hub-ui PROPERTIES ENVIRONMENT_MODIFICATION
        "AC_UI_PANEL_PICTURES=unset:;AC_UI_BENCH_OUTPUT=unset:")
    set_property(TEST ${_ac_tests} APPEND PROPERTY ENVIRONMENT_MODIFICATION "PYTHONDONTWRITEBYTECODE=set:1")
endif()
