// SPDX-License-Identifier: MIT
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>

namespace room {

enum class ControlPoll { Continue, Close, Error };

// The launcher must pass the child read end of its private CreatePipe as stdin.
// Windows exposes anonymous pipes as pipe handles, but a handle-type check
// cannot authenticate which process created the pipe.
class ControlStdin {
public:
    ControlStdin() = default;
    ~ControlStdin();
    ControlStdin(const ControlStdin &) = delete;
    ControlStdin &operator=(const ControlStdin &) = delete;

    bool attach(HANDLE input);
    ControlPoll poll();

private:
    static constexpr size_t max_command = 7; // "close\r\n"
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    char command_[max_command]{};
    size_t used_ = 0;
};

} // namespace room
