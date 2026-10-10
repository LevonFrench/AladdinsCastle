# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AladdinsCastleControls LANGUAGES CXX)
    include(CTest)
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.8)
endif()
get_filename_component(AC_CONTROLS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ProjectWarnings.cmake")
if(TARGET tomlplusplus::tomlplusplus)
    set(_ac_controls_toml_target tomlplusplus::tomlplusplus)
else()
    set(AC_CONTROLS_TOML_INCLUDE_DIR "" CACHE PATH "Existing toml++ include directory; never downloaded here")
    if(NOT EXISTS "${AC_CONTROLS_TOML_INCLUDE_DIR}/toml++/toml.hpp")
        message(FATAL_ERROR "Provide existing AC_CONTROLS_TOML_INCLUDE_DIR for standalone controls builds")
    endif()
    add_library(accontrols_toml_headers INTERFACE)
    target_include_directories(accontrols_toml_headers SYSTEM INTERFACE "${AC_CONTROLS_TOML_INCLUDE_DIR}")
    set(_ac_controls_toml_target accontrols_toml_headers)
endif()
qt_add_library(accontrols STATIC ControlsViewModel.h ControlsViewModel.cpp ControlsResolver.h ControlsResolver.cpp)
target_include_directories(accontrols PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(accontrols PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2)
target_link_libraries(accontrols PRIVATE ${_ac_controls_toml_target})
ac_project_warnings(accontrols)
qt_add_resources(accontrols controls_views PREFIX /controls
    BASE "${AC_CONTROLS_ROOT}/hub/qml/controls"
    FILES "${AC_CONTROLS_ROOT}/hub/qml/controls/ControlsView.qml"
          "${AC_CONTROLS_ROOT}/hub/qml/controls/ControllerOutline.qml")
qt_add_resources(accontrols controls_icons PREFIX /controls/icons
    BASE "${AC_CONTROLS_ROOT}/assets/controls/controllers"
    FILES "${AC_CONTROLS_ROOT}/assets/controls/controllers/touch-left.svg"
          "${AC_CONTROLS_ROOT}/assets/controls/controllers/touch-right.svg")
if(BUILD_TESTING)
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Test)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    qt_add_executable(controls-view-tests tests/ControlsViewTest.cpp)
    target_link_libraries(controls-view-tests PRIVATE accontrols Qt6::Test)
    target_compile_definitions(controls-view-tests PRIVATE
        AC_CONTROLS_ROOT="${AC_CONTROLS_ROOT}" AC_CONTROLS_PYTHON="${Python3_EXECUTABLE}")
    ac_project_warnings(controls-view-tests)
    set_target_properties(controls-view-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME controls-view-offscreen COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:controls-view-tests>
        -DTEST_LOG=${CMAKE_CURRENT_BINARY_DIR}/Testing/controls-view.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/RunQtTest.cmake)
    set_tests_properties(controls-view-offscreen PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_OPENGL=software;QSG_RHI_BACKEND=software;PYTHONDONTWRITEBYTECODE=1;PYTHONIOENCODING=utf-8")
    qt_add_executable(controls-resolver-tests tests/ControlsResolverTest.cpp)
    target_link_libraries(controls-resolver-tests PRIVATE accontrols Qt6::Test)
    target_compile_definitions(controls-resolver-tests PRIVATE
        AC_CONTROLS_ROOT="${AC_CONTROLS_ROOT}" AC_CONTROLS_PYTHON="${Python3_EXECUTABLE}")
    ac_project_warnings(controls-resolver-tests)
    set_target_properties(controls-resolver-tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME controls-resolver-native COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:controls-resolver-tests>
        -DTEST_LOG=${CMAKE_CURRENT_BINARY_DIR}/Testing/controls-resolver.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/RunQtTest.cmake)
    set_tests_properties(controls-resolver-native PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;PYTHONIOENCODING=utf-8")
endif()
