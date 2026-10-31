# SPDX-License-Identifier: GPL-3.0-only
"""Own a command's descendants, including after the direct child exits.

Windows 10+: create the process atomically in a non-inheritable kill-on-close
Job Object (PROC_THREAD_ATTRIBUTE_JOB_LIST). No PID search or breakaway flags.
Linux: own the new process group; ignore dead/zombie members when waiting.
"""
import os
from pathlib import Path
import signal
import subprocess
import time


if os.name == "nt":
    import ctypes as C
    from ctypes import wintypes as W

    K = C.WinDLL("kernel32", use_last_error=True)
    SIZE = C.c_size_t

    class Limits(C.Structure):
        _fields_ = [("per_process", C.c_int64), ("per_job", C.c_int64),
                    ("flags", W.DWORD), ("min_ws", SIZE), ("max_ws", SIZE),
                    ("active", W.DWORD), ("affinity", SIZE),
                    ("priority", W.DWORD), ("scheduling", W.DWORD)]

    class ExtendedLimits(C.Structure):
        _fields_ = [("basic", Limits), ("io", C.c_uint64 * 6),
                    ("process_memory", SIZE), ("job_memory", SIZE),
                    ("peak_process", SIZE), ("peak_job", SIZE)]

    class Accounting(C.Structure):
        _fields_ = [("times", C.c_int64 * 4), ("faults", W.DWORD),
                    ("total", W.DWORD), ("active", W.DWORD), ("terminated", W.DWORD)]

    class Startup(C.Structure):
        _fields_ = [("cb", W.DWORD), ("reserved", W.LPWSTR), ("desktop", W.LPWSTR),
                    ("title", W.LPWSTR), ("x", W.DWORD), ("y", W.DWORD),
                    ("width", W.DWORD), ("height", W.DWORD),
                    ("chars_x", W.DWORD), ("chars_y", W.DWORD),
                    ("fill", W.DWORD), ("flags", W.DWORD),
                    ("show", W.WORD), ("reserved_size", W.WORD),
                    ("reserved_bytes", C.c_void_p), ("stdin", W.HANDLE),
                    ("stdout", W.HANDLE), ("stderr", W.HANDLE)]

    class StartupEx(C.Structure):
        _fields_ = [("startup", Startup), ("attributes", C.c_void_p)]

    class ProcessInfo(C.Structure):
        _fields_ = [("process", W.HANDLE), ("thread", W.HANDLE),
                    ("pid", W.DWORD), ("tid", W.DWORD)]

    for name, result, arguments in (
        ("CreateJobObjectW", W.HANDLE, [C.c_void_p, W.LPCWSTR]),
        ("SetInformationJobObject", W.BOOL, [W.HANDLE, C.c_int, C.c_void_p, W.DWORD]),
        ("QueryInformationJobObject", W.BOOL, [W.HANDLE, C.c_int, C.c_void_p, W.DWORD, C.c_void_p]),
        ("TerminateJobObject", W.BOOL, [W.HANDLE, W.UINT]),
        ("InitializeProcThreadAttributeList", W.BOOL, [C.c_void_p, W.DWORD, W.DWORD, C.POINTER(SIZE)]),
        ("UpdateProcThreadAttribute", W.BOOL, [C.c_void_p, W.DWORD, SIZE, C.c_void_p, SIZE, C.c_void_p, C.c_void_p]),
        ("DeleteProcThreadAttributeList", None, [C.c_void_p]),
        ("CreateProcessW", W.BOOL, [W.LPCWSTR, W.LPWSTR, C.c_void_p, C.c_void_p, W.BOOL,
                                   W.DWORD, C.c_void_p, W.LPCWSTR, C.c_void_p, C.POINTER(ProcessInfo)]),
        ("GetStdHandle", W.HANDLE, [W.DWORD]),
        ("WaitForSingleObject", W.DWORD, [W.HANDLE, W.DWORD]),
        ("GetExitCodeProcess", W.BOOL, [W.HANDLE, C.POINTER(W.DWORD)]),
        ("CloseHandle", W.BOOL, [W.HANDLE]),
    ):
        fn = getattr(K, name)
        fn.restype, fn.argtypes = result, arguments

    def checked(result):
        if not result:
            raise C.WinError(C.get_last_error())
        return result

    class OwnedProcess:
        def __init__(self, command, cwd=None):
            self._job = self._process = None
            self.returncode = None
            self._job = checked(K.CreateJobObjectW(None, None))
            try:
                limits = ExtendedLimits()
                limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                checked(K.SetInformationJobObject(self._job, 9, C.byref(limits), C.sizeof(limits)))
                size = SIZE()
                K.InitializeProcThreadAttributeList(None, 1, 0, C.byref(size))
                storage = C.create_string_buffer(size.value)
                checked(K.InitializeProcThreadAttributeList(storage, 1, 0, C.byref(size)))
                try:
                    jobs = (W.HANDLE * 1)(self._job)
                    checked(K.UpdateProcThreadAttribute(storage, 0, 0x2000D, jobs,
                                                       C.sizeof(jobs), None, None))
                    startup = StartupEx()
                    startup.startup.cb = C.sizeof(startup)
                    startup.startup.flags = 0x100  # STARTF_USESTDHANDLES
                    startup.startup.stdin = K.GetStdHandle(-10)
                    startup.startup.stdout = K.GetStdHandle(-11)
                    startup.startup.stderr = K.GetStdHandle(-12)
                    startup.attributes = C.cast(storage, C.c_void_p)
                    info = ProcessInfo()
                    flags = 0x80000 | 0x08000000 | 0x4000  # extended startup, hidden, below normal
                    line = C.create_unicode_buffer(subprocess.list2cmdline([os.fspath(x) for x in command]))
                    checked(K.CreateProcessW(None, line, None, None, True, flags, None,
                                             os.fspath(cwd) if cwd else None, C.byref(startup), C.byref(info)))
                    self._process, self.pid = info.process, info.pid
                    K.CloseHandle(info.thread)
                finally:
                    K.DeleteProcThreadAttributeList(storage)
            except BaseException:
                self.close()
                raise

        def poll(self):
            if self.returncode is None and K.WaitForSingleObject(self._process, 0) == 0:
                code = W.DWORD()
                checked(K.GetExitCodeProcess(self._process, C.byref(code)))
                self.returncode = code.value
            return self.returncode

        def running(self):
            accounting = Accounting()
            checked(K.QueryInformationJobObject(self._job, 1, C.byref(accounting),
                                                C.sizeof(accounting), None))
            return accounting.active != 0

        def close(self):
            try:
                if self._job and self.running():
                    checked(K.TerminateJobObject(self._job, 1))
                    until = time.monotonic() + 10
                    while self.running():
                        if time.monotonic() > until:
                            raise RuntimeError("job termination did not complete; lease retained")
                        time.sleep(.01)
                if self._process:
                    self.poll()
            finally:
                if self._job:
                    K.CloseHandle(self._job)
                    self._job = None
                if self._process:
                    K.CloseHandle(self._process)
                    self._process = None
else:
    class OwnedProcess:
        def __init__(self, command, cwd=None):
            self._process = subprocess.Popen(command, start_new_session=True,
                                             preexec_fn=lambda: os.nice(10), cwd=cwd)
            self.pid = self._process.pid

        def poll(self):
            return self._process.poll()

        def running(self):
            if self.poll() is None:
                return True
            # Linux process groups can contain only unreaped, already-dead zombies.
            proc = Path('/proc')
            if proc.is_dir():
                for p in proc.iterdir():
                    if not p.name.isdigit():
                        continue
                    try:
                        fields = (p / 'stat').read_text().rsplit(')', 1)[1].split()
                        if int(fields[2]) == self.pid and fields[0] not in ('Z', 'X'):
                            return True
                    except (OSError, ValueError, IndexError):
                        continue
                return False
            try:
                os.killpg(self.pid, 0)
                return True
            except ProcessLookupError:
                return False

        def close(self):
            try:
                os.killpg(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            self._process.wait(timeout=10)
            until = time.monotonic() + 10
            while self.running():
                if time.monotonic() > until:
                    raise RuntimeError("process group termination did not complete; lease retained")
                time.sleep(.01)
