#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Machine-wide lease for heavy runs (builds, renders, emulators, measurements).

One heavy job may run on the PC at a time, whichever agent or project starts it.
The lease is a JSON file in a shared directory, created atomically (temp file,
then os.link, so the content is never partial and a second contender fails).

    python tools/heavy_lease.py run --lane NAME --purpose WHY --minutes 30 -- <command...>
    python tools/heavy_lease.py status
    python tools/heavy_lease.py check

`run` refuses while a heavy process is already running, while the HEADSET-ON
marker exists, or while another live lease is held. It runs the command at
below-normal priority with a hard deadline, kills the whole process tree on
timeout (or when the headset marker appears). Cleanup must finish before release.

Lease directory: --lease-dir, else the ACVR_LEASE_DIR environment variable, else
`.agent-rules/lease` in the nearest parent folder that has a `.agent-rules`
folder, else `.agent-rules/lease` under the home folder. Python standard library
only. Never aim the tests at the shared folder; they use a temporary one.
"""
import argparse
import csv
import io
import json
import os
import subprocess
import sys
import time
import uuid
from datetime import datetime, timedelta, timezone
from pathlib import Path
from lease_process import OwnedProcess

MAX_MINUTES = 45
STALE_GRACE_MIN = 30          # a live holder past its planned end is only reported
EMPTY_LOCK_GRACE_S = 60       # an unreadable lock this old is treated as stale
LOCK_NAME = "heavy.lock"
LEDGER_NAME = "ledger.jsonl"
GAP_NAME = "last-release.json"
HEADSET_NAME = "HEADSET-ON"
EXIT_BUSY = 3
EXIT_DEADLINE = 124
EXIT_HEADSET = 125

# Process image names (lower case, without .exe) that count as heavy work.
HEAVY_NAMES = (
    "ffmpeg", "cl", "link", "ninja", "cmake", "ctest", "msbuild", "blender",
    "dolphin", "azahar", "citra", "cemu", "pcsx2", "duckstation", "flycast",
    "supermodel", "mame", "retroarch", "ppsspp", "acvr-ps1", "acvr-hub",
    "aladdinscastle", "vrserver", "vrcompositor", "vrmonitor",
)

WINDOWS = os.name == "nt"


class Busy(Exception):
    """The lease cannot be granted; the message says why."""


def default_lease_dir():
    env = os.environ.get("ACVR_LEASE_DIR")
    if env:
        return Path(env)
    for parent in Path(__file__).resolve().parents:
        if (parent / ".agent-rules").is_dir():
            return parent / ".agent-rules" / "lease"
    return Path.home() / ".agent-rules" / "lease"


def now():
    return datetime.now(timezone.utc)


def iso(moment):
    return moment.isoformat(timespec="seconds")


# --- processes ---------------------------------------------------------------

if WINDOWS:
    import ctypes
    from ctypes import wintypes

    _k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _k32.OpenProcess.restype = wintypes.HANDLE
    _k32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    _k32.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    _k32.GetProcessTimes.argtypes = (wintypes.HANDLE,) + (ctypes.POINTER(wintypes.FILETIME),) * 4
    _k32.CloseHandle.argtypes = (wintypes.HANDLE,)
    _QUERY_LIMITED = 0x1000
    _STILL_ACTIVE = 259

    def process_create_time(pid):
        """Process creation time as an integer (FILETIME), or None if unreadable."""
        handle = _k32.OpenProcess(_QUERY_LIMITED, False, pid)
        if not handle:
            return None
        try:
            created, exited, kernel, user = (wintypes.FILETIME() for _ in range(4))
            if not _k32.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                        ctypes.byref(kernel), ctypes.byref(user)):
                return None
            return (created.dwHighDateTime << 32) | created.dwLowDateTime
        finally:
            _k32.CloseHandle(handle)

    def pid_alive(pid):
        handle = _k32.OpenProcess(_QUERY_LIMITED, False, pid)
        if not handle:
            # Access denied still means the process exists.
            return ctypes.get_last_error() == 5
        try:
            code = wintypes.DWORD()
            if not _k32.GetExitCodeProcess(handle, ctypes.byref(code)):
                return True
            return code.value == _STILL_ACTIVE
        finally:
            _k32.CloseHandle(handle)

    def list_processes():
        """[(pid, image name)] from tasklist."""
        out = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True,
                             timeout=30, creationflags=subprocess.CREATE_NO_WINDOW).stdout
        rows = []
        for row in csv.reader(io.StringIO(out)):
            if len(row) >= 2 and row[1].isdigit():
                rows.append((int(row[1]), row[0]))
        return rows
else:
    def process_create_time(pid):
        try:
            with open(f"/proc/{pid}/stat") as f:
                return int(f.read().rsplit(")", 1)[1].split()[19])
        except (OSError, ValueError, IndexError):
            return None

    def pid_alive(pid):
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return False
        except PermissionError:
            return True
        return True

    def list_processes():
        out = subprocess.run(["ps", "-eo", "pid=,comm="], capture_output=True, text=True, timeout=30).stdout
        rows = []
        for line in out.splitlines():
            pid, _, name = line.strip().partition(" ")
            if pid.isdigit():
                rows.append((int(pid), name.strip()))
        return rows


def holder_alive(record):
    """True when the recorded holder process is the one that took the lease."""
    pid = record.get("pid")
    if not isinstance(pid, int) or not pid_alive(pid):
        return False
    created = record.get("pid_created")
    return created is None or process_create_time(pid) in (None, created)


def heavy_processes(lister=None, own_pids=()):
    """Running heavy processes, as [(pid, name)], ignoring our own pids."""
    found = []
    for pid, name in (lister or list_processes)():
        stem = name.lower()
        if stem.endswith(".exe"):
            stem = stem[:-4]
        if pid in own_pids:
            continue
        if any(stem == h or (len(h) > 3 and stem.startswith(h)) for h in HEAVY_NAMES):
            found.append((pid, name))
    return found


def kill_tree(pid):
    if WINDOWS:
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, timeout=30,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    else:
        import signal
        try:
            os.killpg(os.getpgid(pid), signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass


# --- the lease file ----------------------------------------------------------

def read_lock(lease_dir):
    """The lease record, None when there is no lock, or {} when it cannot be read."""
    path = Path(lease_dir) / LOCK_NAME
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return None
    except OSError:
        return {}
    try:
        record = json.loads(text)
    except ValueError:
        return {}
    return record if isinstance(record, dict) else {}


def lock_age_s(lease_dir):
    try:
        return time.time() - (Path(lease_dir) / LOCK_NAME).stat().st_mtime
    except OSError:
        return 0.0


def ledger(lease_dir, **event):
    event["at"] = iso(now())
    with (Path(lease_dir) / LEDGER_NAME).open("a", encoding="utf-8") as f:
        f.write(json.dumps(event, sort_keys=True) + "\n")


def write_atomic(lease_dir, record, target_name=LOCK_NAME, exclusive=True):
    """Write the record to a temp file, then publish it under target_name.

    exclusive=True uses os.link, which fails with FileExistsError when the target
    exists; exclusive=False replaces the target (used to update our own lease).
    """
    lease_dir = Path(lease_dir)
    tmp = lease_dir / f"lease.{uuid.uuid4().hex}.tmp"
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(record, f, indent=2, sort_keys=True)
        f.flush()
        os.fsync(f.fileno())
    try:
        if exclusive:
            os.link(tmp, lease_dir / target_name)
        else:
            os.replace(tmp, lease_dir / target_name)
    finally:
        try:
            tmp.unlink()
        except FileNotFoundError:
            pass


def classify(lease_dir, moment=None):
    """('free'|'live'|'overdue'|'stale', record) for the current lock."""
    record = read_lock(lease_dir)
    if record is None:
        return "free", None
    if not record:
        return ("stale" if lock_age_s(lease_dir) > EMPTY_LOCK_GRACE_S else "live"), record
    if not holder_alive(record):
        return "stale", record
    try:
        end = datetime.fromisoformat(record["planned_end"])
    except (KeyError, ValueError):
        return "live", record
    if (moment or now()) > end + timedelta(minutes=STALE_GRACE_MIN):
        return "overdue", record
    return "live", record


def clear_stale(lease_dir, stale_record):
    """Move a stale lock aside and write the ledger entry. Returns False if we lost a race."""
    lease_dir = Path(lease_dir)
    child = stale_record.get("child_pid")
    child_created = stale_record.get("child_created")
    current_created = process_create_time(child) if isinstance(child, int) and pid_alive(child) else None
    if isinstance(child, int) and pid_alive(child) and (child_created is None or current_created is None):
        ledger(lease_dir, event="recovery-blocked", lane=stale_record.get("lane"),
               lease_id=stale_record.get("id"), orphan_unverified=True)
        raise Busy("stale child identity is unreadable; left it and the lease untouched")
    aside = lease_dir / f"heavy.lock.stale.{uuid.uuid4().hex}"
    try:
        os.replace(lease_dir / LOCK_NAME, aside)
    except FileNotFoundError:
        return False
    try:
        moved = json.loads(aside.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        moved = {}
    if moved != stale_record:
        # Someone else recovered and took a fresh lease between our read and the move: put it back.
        try:
            os.link(aside, lease_dir / LOCK_NAME)
        except OSError:
            pass
        aside.unlink(missing_ok=True)
        return False
    orphan = False
    if isinstance(child, int) and pid_alive(child):
        observed = process_create_time(child)
        if child_created is None or observed is None:
            # Identity can become unreadable after the preflight. Restore the lock
            # when still free; preserve the aside record if another holder won.
            try:
                os.link(aside, lease_dir / LOCK_NAME)
            except FileExistsError:
                pass
            else:
                aside.unlink()
            ledger(lease_dir, event="recovery-blocked", lane=stale_record.get("lane"),
                   lease_id=stale_record.get("id"), orphan_unverified=True)
            raise Busy("stale child identity is unreadable; recovery refused")
        if observed == child_created:
            kill_tree(child)
            orphan = True
    ledger(lease_dir, event="recovered", lane=stale_record.get("lane"), lease_id=stale_record.get("id"),
           holder_pid=stale_record.get("pid"), planned_end=stale_record.get("planned_end"),
           orphan_killed=orphan)
    aside.unlink(missing_ok=True)
    return True


def headset_on(lease_dir, extra_marker=None):
    markers = [Path(lease_dir) / HEADSET_NAME]
    extra = extra_marker or os.environ.get("ACVR_HEADSET_MARKER")
    if extra:
        markers.append(Path(extra))
    return any(m.exists() for m in markers)


def acquire(lease_dir, lane, purpose, minutes, lister=None, extra_marker=None, gap_min=0):
    """Take the lease or raise Busy. Returns the lease record."""
    if not 0 < minutes <= MAX_MINUTES:
        raise Busy(f"minutes must be 1..{MAX_MINUTES}")
    lease_dir = Path(lease_dir)
    lease_dir.mkdir(parents=True, exist_ok=True)
    if headset_on(lease_dir, extra_marker):
        raise Busy("HEADSET-ON marker present: code and docs only")
    gap_file = lease_dir / GAP_NAME
    if gap_file.exists():
        try:
            gap_end = datetime.fromisoformat(json.loads(gap_file.read_text(encoding="utf-8"))["gap_end"])
            if now() < gap_end:
                raise Busy(f"mandatory gap until {iso(gap_end)}")
        except (ValueError, KeyError, OSError):
            pass
    state, held = classify(lease_dir)
    if state == "stale":
        if not clear_stale(lease_dir, held):
            raise Busy("lost a recovery race; try again")
    elif state in ("live", "overdue"):
        who = held.get("lane", "unknown") if held else "unreadable lock"
        raise Busy(f"lease held by {who} (pid {held.get('pid') if held else '?'}), "
                   f"planned end {held.get('planned_end') if held else '?'}" + (" [overdue]" if state == "overdue" else ""))
    busy = heavy_processes(lister, own_pids=(os.getpid(),))
    if busy:
        raise Busy("heavy processes already running: " + ", ".join(f"{n} ({p})" for p, n in busy[:8]))
    start = now()
    record = {
        "id": uuid.uuid4().hex, "lane": lane, "purpose": purpose, "pid": os.getpid(),
        "pid_created": process_create_time(os.getpid()), "start": iso(start),
        "planned_end": iso(start + timedelta(minutes=minutes)), "gap_min": gap_min,
    }
    try:
        write_atomic(lease_dir, record)
    except FileExistsError:
        raise Busy("another agent took the lease first")
    except OSError as exc:
        raise Busy(f"cannot create the lease with os.link on this filesystem: {exc}")
    ledger(lease_dir, event="acquired", lane=lane, lease_id=record["id"], purpose=purpose,
           planned_end=record["planned_end"])
    return record


def release(lease_dir, record, **outcome):
    """Remove our own lease (only if the file still holds our id) and note the outcome."""
    lease_dir = Path(lease_dir)
    current = read_lock(lease_dir)
    if current and current.get("id") == record["id"]:
        try:
            (lease_dir / LOCK_NAME).unlink()
        except FileNotFoundError:
            pass
    gap = record.get("gap_min", 0)
    if gap:
        (lease_dir / GAP_NAME).write_text(
            json.dumps({"gap_end": iso(now() + timedelta(minutes=gap)), "lane": record["lane"]}), encoding="utf-8")
    ledger(lease_dir, event="released", lane=record["lane"], lease_id=record["id"], **outcome)


def run(lease_dir, lane, purpose, minutes, command, deadline_s=None, lister=None, extra_marker=None,
        gap_min=0, poll_s=0.2, cwd=None, stop_descendants_on_exit=False):
    """Hold the lease, run the command, kill its tree on deadline. Returns the exit code."""
    record = acquire(lease_dir, lane, purpose, minutes, lister=lister, extra_marker=extra_marker, gap_min=gap_min)
    deadline_s = minutes * 60 if deadline_s is None else deadline_s
    child = None
    status, code = "failed-start", None
    started = time.monotonic()
    try:
        child = OwnedProcess(command, cwd=cwd)
        status = "finished"
        record.update(child_pid=child.pid, child_created=process_create_time(child.pid))
        try:
            write_atomic(lease_dir, record, exclusive=False)
        except OSError:
            pass  # the child pid is only for orphan cleanup; the lease itself stays valid
        while child.running():
            if stop_descendants_on_exit and child.poll() is not None:
                status = "finished-descendants-stopped"
                break
            if headset_on(lease_dir, extra_marker):
                status, code = "killed-headset", EXIT_HEADSET
                break
            if time.monotonic() - started >= deadline_s:
                status, code = "killed-deadline", EXIT_DEADLINE
                break
            time.sleep(poll_s)
        if code is None:
            code = child.poll()
    finally:
        # Close owns descendants even when the direct child exited first. Failure
        # to confirm cleanup leaves the lease in place instead of advertising free.
        if child is not None:
            child.close()
        release(lease_dir, record, status=status, exit_code=code,
                seconds=round(time.monotonic() - started, 1))
    return code


def describe(lease_dir):
    state, record = classify(lease_dir)
    if state == "free":
        return "free"
    return f"{state}: " + json.dumps(record, sort_keys=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--lease-dir", type=Path, default=None)
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_run = sub.add_parser("run", help="hold the lease while running a command")
    p_run.add_argument("--lane", required=True)
    p_run.add_argument("--purpose", required=True)
    p_run.add_argument("--minutes", type=int, default=30, help=f"hard deadline, 1..{MAX_MINUTES}")
    p_run.add_argument("--gap-min", type=int, default=0, help="mandatory gap after release (measure lanes: 15)")
    p_run.add_argument("--cwd", default=None)
    p_run.add_argument("--stop-descendants-on-exit", action="store_true",
                       help="stop owned background helpers when the main command exits (opt-in for synchronous builds)")
    p_run.add_argument("command", nargs=argparse.REMAINDER, help="-- then the command")
    sub.add_parser("status", help="print the current lease")
    sub.add_parser("check", help="list heavy processes; exit 3 when the host is not quiet")
    args = parser.parse_args(argv)
    lease_dir = args.lease_dir or default_lease_dir()

    if args.cmd == "status":
        print(describe(lease_dir))
        return 0
    if args.cmd == "check":
        busy = heavy_processes(own_pids=(os.getpid(),))
        for pid, name in busy:
            print(f"{name} {pid}")
        print("host quiet" if not busy else "host busy")
        return EXIT_BUSY if busy else 0
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("run needs a command after --")
    try:
        code = run(lease_dir, args.lane, args.purpose, args.minutes, command, gap_min=args.gap_min, cwd=args.cwd,
                   stop_descendants_on_exit=args.stop_descendants_on_exit)
    except Busy as exc:
        print(f"heavy_lease: not granted: {exc}", file=sys.stderr)
        return EXIT_BUSY
    except (OSError, RuntimeError) as exc:
        print(f"heavy_lease: run failed: {exc}", file=sys.stderr)
        return 1
    return code if code is not None else 1


if __name__ == "__main__":
    sys.exit(main())
