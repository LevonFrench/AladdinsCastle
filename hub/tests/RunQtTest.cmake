# SPDX-License-Identifier: GPL-3.0-only
# Qt's Windows logger can choose the debugger when no console is attached.
# Keep deterministic text receipts and surface them through CTest on failure.
get_filename_component(_log_dir "${TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_log_dir}")
execute_process(COMMAND "${TEST_EXECUTABLE}" -o "${TEST_LOG},txt"
    RESULT_VARIABLE _result ERROR_VARIABLE _stderr TIMEOUT 30)
if(EXISTS "${TEST_LOG}")
    file(READ "${TEST_LOG}" _receipt)
    message("${_receipt}")
endif()
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "Qt test failed (${_result}): ${_stderr}")
endif()
