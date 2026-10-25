# SPDX-License-Identifier: GPL-3.0-only
# Qt's Windows logger can choose the debugger when no console is attached.
# Keep deterministic text receipts and surface them through CTest on failure.
get_filename_component(_log_dir "${TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_log_dir}")
if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 120)
endif()
file(REMOVE "${TEST_LOG}")
execute_process(COMMAND "${TEST_EXECUTABLE}" ${TEST_ARGUMENTS} -o "${TEST_LOG},txt"
    RESULT_VARIABLE _result ERROR_VARIABLE _stderr TIMEOUT ${TEST_TIMEOUT})
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
