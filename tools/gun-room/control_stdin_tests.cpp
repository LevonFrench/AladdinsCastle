// SPDX-License-Identifier: MIT
#include "control_stdin.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
void require(bool okay, const char *message) {
    if (!okay) { std::fprintf(stderr, "control stdin: %s\n", message); std::exit(1); }
}

struct Pipe {
    HANDLE read = INVALID_HANDLE_VALUE;
    HANDLE write = INVALID_HANDLE_VALUE;
    Pipe() { require(CreatePipe(&read, &write, nullptr, 0) != 0, "CreatePipe"); }
    ~Pipe() {
        if (read != INVALID_HANDLE_VALUE) CloseHandle(read);
        if (write != INVALID_HANDLE_VALUE) CloseHandle(write);
    }
    void send(const char *message) {
        const auto length = static_cast<DWORD>(std::strlen(message));
        DWORD written = 0;
        require(WriteFile(write, message, length, &written, nullptr) != 0 && written == length,
                "WriteFile");
    }
    void finish() { require(CloseHandle(write) != 0, "CloseHandle writer"); write = INVALID_HANDLE_VALUE; }
};
} // namespace

int main() {
    using room::ControlPoll;
    {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "accept anonymous pipe");
        require(!input.attach(pipe.read), "reject second attachment");
        require(input.poll() == ControlPoll::Continue, "empty live pipe must not block or close");
        pipe.send("cl");
        require(input.poll() == ControlPoll::Continue, "fragment one");
        pipe.send("ose\r");
        require(input.poll() == ControlPoll::Continue, "fragment two");
        pipe.send("\n");
        require(input.poll() == ControlPoll::Close, "fragmented CRLF close");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        require(!input.attach(pipe.write), "reject anonymous pipe write end");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "attach LF pipe");
        pipe.send("close\n");
        require(input.poll() == ControlPoll::Close, "LF close");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "attach EOF pipe");
        pipe.finish();
        require(input.poll() == ControlPoll::Close, "EOF close");
    }
    {
        Pipe pipe;
        pipe.finish();
        room::ControlStdin input;
        require(input.attach(pipe.read), "accept disconnected read end");
        require(input.poll() == ControlPoll::Close, "disconnected read end closes");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "attach partial EOF pipe");
        pipe.send("cl");
        require(input.poll() == ControlPoll::Continue, "partial command");
        pipe.finish();
        require(input.poll() == ControlPoll::Close, "broken pipe closes even after fragment");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        pipe.send("cl");
        pipe.finish();
        require(input.attach(pipe.read), "attach buffered partial EOF pipe");
        require(input.poll() == ControlPoll::Close,
                "buffered partial EOF closes on first poll before XR creation");
    }
    for (const char *bad : {"Close\n", "close\nx", "close\r\nx", "close\rX"}) {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "attach malformed pipe");
        pipe.send(bad);
        require(input.poll() == ControlPoll::Error, "malformed/oversized input fails");
    }
    {
        Pipe pipe;
        room::ControlStdin input;
        require(input.attach(pipe.read), "attach overlong fragment pipe");
        pipe.send("close\r");
        require(input.poll() == ControlPoll::Continue, "maximum prefix remains pending");
        pipe.send("\nx");
        require(input.poll() == ControlPoll::Error, "fragment exceeding seven-byte bound");
    }
    {
        room::ControlStdin input;
        require(!input.attach(INVALID_HANDLE_VALUE), "reject invalid handle");
        require(!input.attach(nullptr), "reject null handle");
        const HANDLE file = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, nullptr,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(file != INVALID_HANDLE_VALUE, "open NUL handle");
        require(!input.attach(file), "reject non-pipe handle");
        CloseHandle(file);
    }
    return 0;
}
