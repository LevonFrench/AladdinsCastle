// SPDX-License-Identifier: MIT
#include "control_stdin.hpp"
#include <algorithm>
#include <cstring>

namespace room {
namespace {
bool disconnected(DWORD error) {
    return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED ||
           error == ERROR_NO_DATA || error == ERROR_HANDLE_EOF;
}
} // namespace

ControlStdin::~ControlStdin() {
    if (pipe_ != INVALID_HANDLE_VALUE) CloseHandle(pipe_);
}

bool ControlStdin::attach(HANDLE input) {
    if (pipe_ != INVALID_HANDLE_VALUE || input == nullptr || input == INVALID_HANDLE_VALUE ||
        GetFileType(input) != FILE_TYPE_PIPE) return false;
    DWORD flags = 0;
    if (!GetNamedPipeInfo(input, &flags, nullptr, nullptr, nullptr)) return false;
    DWORD available = 0;
    if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr) &&
        !disconnected(GetLastError())) return false; // Reject a write-only pipe end.
    HANDLE owned = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), input, GetCurrentProcess(), &owned,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) return false;
    pipe_ = owned;
    return true;
}

ControlPoll ControlStdin::poll() {
    if (pipe_ == INVALID_HANDLE_VALUE) return ControlPoll::Error;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr))
            return disconnected(GetLastError()) ? ControlPoll::Close : ControlPoll::Error;
        if (available == 0) return ControlPoll::Continue;
        if (available > max_command - used_) return ControlPoll::Error;
        char bytes[max_command]{};
        DWORD read = 0;
        if (!ReadFile(pipe_, bytes, available, &read, nullptr))
            return disconnected(GetLastError()) ? ControlPoll::Close : ControlPoll::Error;
        if (read == 0) return ControlPoll::Close;
        std::memcpy(command_ + used_, bytes, read);
        used_ += read;
        constexpr char lf[] = "close\n";
        constexpr char crlf[] = "close\r\n";
        if (used_ == sizeof(lf) - 1 && std::memcmp(command_, lf, used_) == 0)
            return ControlPoll::Close;
        if (used_ == sizeof(crlf) - 1 && std::memcmp(command_, crlf, used_) == 0)
            return ControlPoll::Close;
        const bool lf_prefix = used_ < sizeof(lf) && std::memcmp(command_, lf, used_) == 0;
        const bool crlf_prefix = used_ < sizeof(crlf) && std::memcmp(command_, crlf, used_) == 0;
        if (!lf_prefix && !crlf_prefix) return ControlPoll::Error;
        // Drain any already-buffered suffix and re-check EOF before XR creation.
    }
}

} // namespace room
