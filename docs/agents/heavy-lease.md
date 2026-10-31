# Heavy-run lease

On 2026-10-25 a build overlapped a video render and locked the PC up. Each project and
lane had its own lock, and none of them covered the other tool. `tools/heavy_lease.py` is
one lease for the whole machine. It uses only the Python standard library.

## What counts as heavy

By resource, not by tool: more than about two cores for over a minute (builds, ctest,
ffmpeg, Blender renders), any GPU use (emulators, encoders, renders), or long disk reads
such as a library survey. Git, short Python scripts and documents are light and need no lease.

## Use

```
python tools/heavy_lease.py run --lane <name> --purpose "<why>" --minutes 30 -- <command> <args>
python tools/heavy_lease.py status
python tools/heavy_lease.py check
```

`run` grants the lease only when all of these hold. Otherwise it prints why and exits 3
without starting the command:

- no live lease is held (a dead holder is recovered, a live one is never taken over);
- no heavy process is already running (ffmpeg, cl, link, ninja, cmake, ctest, msbuild,
  blender, the emulators, the Hub, SteamVR processes);
- the `HEADSET-ON` marker does not exist;
- any mandatory gap from the previous hold has passed.

Then it runs the command at below-normal priority with a hard deadline (`--minutes`, 1 to
45, default 30), kills the whole process tree at the deadline or when `HEADSET-ON`
appears, and releases the lease after owned-process cleanup completes. If cleanup
cannot be confirmed, it fails and retains the lease. Exit codes: the command's own code, 3 not
granted, 124 deadline, 125 headset. `--gap-min 15` adds a mandatory gap after the hold
(measurement lanes use 15). `check` lists heavy processes and exits 3 when the host is not quiet.
For a synchronous build whose main command waits for all useful work, use
`--stop-descendants-on-exit` before `--` to terminate lingering owned helpers such as
MSVC's PDB server. The default waits for descendants; do not use this option with
a launcher that deliberately exits before its useful child finishes.

## The lease folder

Order of choice: `--lease-dir`, the `ACVR_LEASE_DIR` environment variable, the
`.agent-rules/lease` folder in the nearest parent directory that has a `.agent-rules`
folder (the owner keeps one beside the projects folder, shared by Claude and Codex), then
`.agent-rules/lease` under the home folder. The tool creates it when it runs.

| File | Meaning |
|---|---|
| `heavy.lock` | The lease: id, lane, purpose, holder pid and creation time, planned end, child pid |
| `ledger.jsonl` | One line per acquire, release and recovery; feeds the cost section of a report |
| `last-release.json` | End of the mandatory gap, when one was asked for |
| `HEADSET-ON` | Marker the owner (or a headset script) creates to stop all heavy work; a file named by `ACVR_HEADSET_MARKER` counts too |

## How it stays safe

- **Atomic.** The record is written to a temporary file and published with `os.link`, which
  fails if `heavy.lock` exists. A reader never sees half a record, and two contenders cannot
  both win. The file system must support hard links (NTFS does).
- **Stale detection.** The holder is the recorded pid plus its creation time, so a reused pid
  is not mistaken for the holder. A dead holder means a stale lease: it is moved aside, any
  recorded orphan child with a matching creation time is killed, and the recovery goes in
  the ledger. A holder that is alive but more than 30 minutes past its planned end is
  reported as overdue and never taken over; the wrapper's deadline should have killed it.
  An unreadable lock older than a minute is stale.
- **The deadline is a hard kill.** `planned_end` is enforced by the wrapper, not a promise.
- **Descendant lifetime.** On Windows 10 or newer the command is created atomically in
  a private, non-inheritable Job Object with kill-on-close and no breakaway flags.
  The lease stays held until all job members exit, even if the launcher exits first.
  A wrapper crash closes its job handle and terminates the job. If job setup fails,
  the command never starts. Linux uses a new process group and waits for live members.
- **Uncertain stale children.** A recorded child is killed only when its saved and
  current creation times are both readable and equal. An unreadable identity leaves
  the process and lock alone, records `recovery-blocked`, and refuses the new run.
  A readable mismatch means a reused PID: it is left alone.

## Limits

- A command started outside the wrapper is only caught by the process check, which sees
  the names above. It cannot see a heavy job under another name.
- Windows Job Objects contain ordinary descendants. Processes launched through an
  external service are outside that job. Linux process groups do not prevent a child
  deliberately creating another session. These are containment limits, not permission
  to launch work outside the wrapper.
- Nothing forces an agent to use it. The owner's shared agent rules and a Bash/PowerShell
  deny hook would make that enforceable; both live outside this repository.
- Tests: `python -B -m unittest -v test_heavy_lease` from `tools/`. They use a temporary
  folder and take a few seconds.
