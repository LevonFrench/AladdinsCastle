# SPDX-License-Identifier: GPL-3.0-only
# Qt's Windows logger can choose the debugger when no console is attached.
# Keep deterministic text receipts and surface them through CTest on failure.
get_filename_component(_log_dir "${TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_log_dir}")
if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 120)
endif()
file(REMOVE "${TEST_LOG}")
set(_powershell "")
if(CMAKE_HOST_WIN32)
    find_program(_powershell NAMES powershell)
endif()
if(_powershell)
    # execute_process(TIMEOUT) ends only the direct child; RunTree.ps1 ends the whole process tree on timeout.
    set(_line "")
    foreach(_argument IN LISTS TEST_ARGUMENTS ITEMS -o "${TEST_LOG},txt")
        string(REPLACE "\"" "\\\"" _argument "${_argument}")
        string(APPEND _line " \"${_argument}\"")
    endforeach()
    set(ENV{AC_RUN_EXE} "${TEST_EXECUTABLE}")
    set(ENV{AC_RUN_ARGS} "${_line}")
    set(ENV{AC_RUN_TIMEOUT} "${TEST_TIMEOUT}")
    math(EXPR _backstop "${TEST_TIMEOUT} + 60")
    execute_process(COMMAND "${_powershell}" -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_LIST_DIR}/RunTree.ps1"
        RESULT_VARIABLE _result ERROR_VARIABLE _stderr TIMEOUT ${_backstop})
    unset(ENV{AC_RUN_EXE})
    unset(ENV{AC_RUN_ARGS})
    unset(ENV{AC_RUN_TIMEOUT})
else()
    execute_process(COMMAND "${TEST_EXECUTABLE}" ${TEST_ARGUMENTS} -o "${TEST_LOG},txt"
        RESULT_VARIABLE _result ERROR_VARIABLE _stderr TIMEOUT ${TEST_TIMEOUT})
endif()
set(_receipt_ok FALSE)
if(EXISTS "${TEST_LOG}")
    # A timed-out logger may still hold its report. Reading it must not mask
    # the original timeout/nonzero result with a CMake file-read failure.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E cat "${TEST_LOG}"
        RESULT_VARIABLE _read_result OUTPUT_VARIABLE _receipt ERROR_VARIABLE _read_error TIMEOUT 10)
    if(_read_result STREQUAL "0" AND NOT _receipt STREQUAL "")
        set(_receipt_ok TRUE)
        message("${_receipt}")
    else()
        message("Qt test report unavailable or empty: ${_read_error}")
    endif()
endif()
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "Qt test failed (${_result}): ${_stderr}")
endif()
if(NOT _receipt_ok)
    message(FATAL_ERROR "Qt test exited without a readable, nonempty report: ${TEST_LOG}")
endif()
