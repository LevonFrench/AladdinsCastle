# Offline append inbox — source implementation, verification pending

This optional input-probe extension supports W0 navigation inside an isolated synthetic emulator. It adds no game fix, live settings change, desktop control, runtime startup, shell command or network access. Fixed replay parsing and control behavior remain unchanged. No build, test or emulator run was performed while preparing this source.

`DOLPHINXR_INPUT_APPEND` selects an existing, caller-created run-local text file. The runner must supply a unique private file and enable this only for an explicitly bounded observation run. The fixed `DOLPHINXR_INPUT_SCRIPT` and the diagnostic `DOLPHINXR_INPUT_PROBE` must also be set. The new inbox is off when its environment variable is absent. The append gate requires `AC_STEREO_PROBE` to equal `1` exactly, a writable probe log and a null `VR::g_openxr` manager. The manager check reads only the pointer; it invokes no XR function. A live manager appearing later permanently revokes appended actions.

The existing `apply.py` BTEmu insertion is under `ENABLE_VR` only: the call site itself does **not** establish offline status. The new inbox checks its own stricter gate. Do not use the synthetic input snapshot's `runtime_active` field for this decision: fixed replay deliberately sets that field true to exercise bindings. The reviewed synthetic renderer must still bypass XR initialization. This extension makes no claim that a build has passed those runtime gates until independently verified.

## Commands and diagnostics

Append newline-terminated commands using the same timed syntax as the fixed script:

```text
# Example only: replace these times with values safely ahead of current emulated time.
120 125 R u -0.20
120 125 R v -0.14
122 122.3 R primary 1
shot 124 file-selection-01
```

Use at least several seconds of lead time: the inbox polls at most once per emulated second. Commands are validated at the poll's current emulated time, not the time of the writer's last observation. Each new step must begin at or after current time, end strictly later, last no longer than 120 seconds and remain within a 600-second future horizon. Shots must also be at or after current time and within that horizon. Old commands are not triggered late. Partial final lines wait for a later newline, then face the same future-time checks. Paused emulation does not advance polling.

Only L/R hands and existing controls are accepted. Digital primary/secondary/thumbclick values are exactly 0 or 1; trigger/squeeze are 0..1; stick axes are -1..1; u/v are -5..5; orientation angles are -360..360 degrees; angular velocity components are -100..100. All times and values must be finite. The reserved `menu` control is rejected for both hands, even a zero/release value. Arbitrary control names, extra tokens and malformed lines are rejected.

Screenshot basenames are 1–64 ASCII letters, digits, underscores or hyphens. Paths, extensions, traversal and Windows device names are rejected. Names must be unique against all fixed and appended shots, case-insensitively, including shots already completed. This avoids accidental overwrite/replay. Appended shots are marked done by the existing screenshot path and remain done.

Once the offline gate passes and the bounded inbox opens successfully, the probe log receives a flushed line containing the exact marker `AC_INPUT_APPEND ready`. Accepted commands and rejected-line reasons also use the `AC_INPUT_APPEND` prefix and flush immediately. Accepted logging means queued, not a proven in-game response or completed screenshot. Existing `shot` completion-attempt records flush immediately while append mode is configured. There is no new diagnostic file or independent UI. Missing readiness, a disabled marker or missing/invalid captures remains a blocker for the parent runner; no diagnostic behavior has yet been claimed from a real run.

## Static bounds and failure behavior

- File size and total session read cursor: at most 64 KiB; each poll reads only the newly appended suffix, with one persistent read handle.
- Line length: at most 256 bytes. At most 2,048 lines and 1,024 accepted appended actions per process.
- Total vectors: no appended command may exceed 2,048 steps or 512 shots, including the fixed replay. A larger pre-existing fixed replay is preserved and leaves no capacity for that command type.
- Truncation, unreadable/short reads, byte/line/action-cap violations and clock rewind disable the inbox permanently. The wrapper removes only appended steps/shots and preserves fixed entries, including their existing done flags.
- Consumed offsets never reset; file replacement does not reopen the inbox handle. In-place edits to consumed bytes are unsupported and are never reread or replayed. The caller must append, never rewrite, truncate or replace the file.
- Lost offline gate also permanently revokes appended actions. No inbox command can change guards, open files, invoke executables, start a runtime or affect another process.

The parent must retain executable/source pins, inspect diagnostic/capture receipts and provide an owned-process stop mechanism. Polling and screenshot work add unmeasured overhead; append-driven observation runs are not 60-second frame measurements or E3 acceptance.

## Compile and check plan, after the timing queue is idle

1. Compile `test_input_append.cpp` with an existing C++17-or-later compiler, strict warnings and assertions enabled. The helper is standard-library-only and has no Dolphin/runtime dependencies. Keep the object, executable and fresh fixture directory private. Example MSVC flags are `/std:c++20 /EHsc /W4 /WX`; do not define `NDEBUG`. Pass one explicit existing private fixture directory when running the test binary. The test creates/removes only its named synthetic fixture and refuses an existing fixture file.
2. After that CPU check, stage **copies** of `ACInputProbe.h` and `ACInputAppend.h` at the parent's private include root as `Core/ACInputProbe.h` and `Core/ACInputAppend.h`. Apply the existing Feed/Record insertion to a private copied `BTEmu.cpp`; `apply.py` now copies/removes both headers when used against a private source copy. Preserve shared-source hashes and existing gun edits.
3. Parent compiles the copied BTEmu object using the pinned existing build response/options, replaces that member in a private copy of `core.lib`, then relinks a distinct private probe binary. The new header requires the existing `VideoCommon/VR/OpenXRManager.h` include path and `VR::g_openxr` link symbol; no extra third-party package is needed. Do not write a new core library or executable into shared/live build paths.
4. Parent reviews the source/binary hash and runs only its guarded, hidden, silent observation mode. Verify the readiness timeout, future accepted/rejected commands, unique tool-owned screenshots and STOP handling first. Preserve failures plainly. Neither a standalone parser test nor successful compilation proves bindings, offline rendering or headset behavior.
