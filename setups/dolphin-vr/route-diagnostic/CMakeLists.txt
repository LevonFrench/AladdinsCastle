cmake_minimum_required(VERSION 3.20)
project(ac_dolphin_route_trace_tests LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME ac-dolphin-route-clock-protocol-synthetic
  COMMAND "${Python3_EXECUTABLE}" -B -m unittest -v test_clock_metadata.py)
set_tests_properties(ac-dolphin-route-clock-protocol-synthetic PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 10)
add_executable(ac-dolphin-route-trace-test test_route_trace.cpp)
target_compile_features(ac-dolphin-route-trace-test PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(ac-dolphin-route-trace-test PRIVATE /W4 /WX /EHsc)
else()
  target_compile_options(ac-dolphin-route-trace-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME ac-dolphin-route-trace-synthetic
  COMMAND ac-dolphin-route-trace-test "${CMAKE_CURRENT_BINARY_DIR}/synthetic-records")
