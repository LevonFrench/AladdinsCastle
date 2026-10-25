#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Unit tests for heavy_lease.py. Each test uses its own temporary lease folder."""
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from datetime import timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import heavy_lease as hl  # noqa: E402

QUIET = lambda: []  # no heavy processes, whatever the host is doing


def dead_pid():
    proc = subprocess.Popen([sys.executable, "-c", "pass"])
    proc.wait()
    return proc.pid


def wait_dead(pid, seconds=10):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if not hl.pid_alive(pid):
            return True
        time.sleep(0.1)
    return False


class LeaseTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name) / "lease"
        self.addCleanup(self._tmp.cleanup)

    def lock(self):
        return json.loads((self.dir / hl.LOCK_NAME).read_text(encoding="utf-8"))

    def events(self):
        path = self.dir / hl.LEDGER_NAME
        return [json.loads(line)["event"] for line in path.read_text(encoding="utf-8").splitlines()]

    def test_acquire_writes_complete_json_and_blocks_a_second_holder(self):
        record = hl.acquire(self.dir, "a", "build", 10, lister=QUIET)
        self.assertEqual(self.lock()["id"], record["id"])
        self.assertEqual(self.lock()["pid"], os.getpid())
        self.assertEqual(self.lock()["lane"], "a")
        self.assertEqual([p.name for p in self.dir.glob("lease.*.tmp")], [])
        with self.assertRaises(hl.Busy) as ctx:
            hl.acquire(self.dir, "b", "render", 10, lister=QUIET)
        self.assertIn("held by a", str(ctx.exception))
        hl.release(self.dir, record)
        self.assertFalse((self.dir / hl.LOCK_NAME).exists())
        hl.release(self.dir, hl.acquire(self.dir, "b", "render", 10, lister=QUIET))

    def test_minutes_are_capped(self):
        for bad in (0, hl.MAX_MINUTES + 1):
            with self.assertRaises(hl.Busy):
                hl.acquire(self.dir, "a", "x", bad, lister=QUIET)

    def test_refuses_while_a_heavy_process_runs(self):
        lister = lambda: [(4242, "ffmpeg.exe"), (1, "python.exe")]
        with self.assertRaises(hl.Busy) as ctx:
            hl.acquire(self.dir, "a", "x", 5, lister=lister)
        self.assertIn("ffmpeg", str(ctx.exception))
        self.assertFalse((self.dir / hl.LOCK_NAME).exists())

    def test_heavy_name_matching(self):
        rows = [(1, "ffmpeg.exe"), (2, "CL.EXE"), (3, "Dolphin-VR.exe"), (4, "claude.exe"), (5, "python.exe"),
                (6, "cmake"), (7, "explorer.exe"), (8, "ninja.exe")]
        names = sorted(n for _, n in hl.heavy_processes(lambda: rows))
        self.assertEqual(names, ["CL.EXE", "Dolphin-VR.exe", "cmake", "ffmpeg.exe", "ninja.exe"])
        self.assertEqual(hl.heavy_processes(lambda: rows, own_pids=(1, 2, 3, 6, 8)), [])

    def test_headset_marker_blocks(self):
        self.dir.mkdir(parents=True)
        (self.dir / hl.HEADSET_NAME).write_text("on")
        with self.assertRaises(hl.Busy) as ctx:
            hl.acquire(self.dir, "a", "x", 5, lister=QUIET)
        self.assertIn("HEADSET-ON", str(ctx.exception))
        extra = Path(self._tmp.name) / "marker"
        (self.dir / hl.HEADSET_NAME).unlink()
        extra.write_text("on")
        with self.assertRaises(hl.Busy):
            hl.acquire(self.dir, "a", "x", 5, lister=QUIET, extra_marker=extra)

    def test_stale_lock_from_a_dead_holder_is_recovered(self):
        self.dir.mkdir(parents=True)
        old = {"id": "old", "lane": "ghost", "pid": dead_pid(), "pid_created": None,
               "planned_end": hl.iso(hl.now() + timedelta(minutes=30))}
        (self.dir / hl.LOCK_NAME).write_text(json.dumps(old))
        self.assertEqual(hl.classify(self.dir)[0], "stale")
        record = hl.acquire(self.dir, "b", "x", 5, lister=QUIET)
        self.assertEqual(self.lock()["id"], record["id"])
        self.assertIn("recovered", self.events())
        self.assertEqual([p.name for p in self.dir.glob("heavy.lock.stale.*")], [])

    def test_pid_reuse_is_detected_by_creation_time(self):
        created = hl.process_create_time(os.getpid())
        if created is None:
            self.skipTest("no process creation time on this platform")
        record = {"pid": os.getpid(), "pid_created": created + 12345}
        self.assertFalse(hl.holder_alive(record))
        record["pid_created"] = created
        self.assertTrue(hl.holder_alive(record))

    def test_live_holder_is_never_taken_over_even_when_overdue(self):
        self.dir.mkdir(parents=True)
        old = {"id": "old", "lane": "slow", "pid": os.getpid(), "pid_created": hl.process_create_time(os.getpid()),
               "planned_end": hl.iso(hl.now() - timedelta(minutes=hl.STALE_GRACE_MIN + 5))}
        (self.dir / hl.LOCK_NAME).write_text(json.dumps(old))
        self.assertEqual(hl.classify(self.dir)[0], "overdue")
        with self.assertRaises(hl.Busy) as ctx:
            hl.acquire(self.dir, "b", "x", 5, lister=QUIET)
        self.assertIn("overdue", str(ctx.exception))

    def test_old_unreadable_lock_is_stale_but_a_fresh_one_is_not(self):
        self.dir.mkdir(parents=True)
        lock = self.dir / hl.LOCK_NAME
        lock.write_text("")
        self.assertEqual(hl.classify(self.dir)[0], "live")
        old = time.time() - hl.EMPTY_LOCK_GRACE_S - 5
        os.utime(lock, (old, old))
        self.assertEqual(hl.classify(self.dir)[0], "stale")

    def test_mandatory_gap_after_release(self):
        record = hl.acquire(self.dir, "a", "x", 5, lister=QUIET, gap_min=15)
        hl.release(self.dir, record)
        with self.assertRaises(hl.Busy) as ctx:
            hl.acquire(self.dir, "b", "x", 5, lister=QUIET)
        self.assertIn("gap", str(ctx.exception))

    def test_release_leaves_a_lease_that_is_no_longer_ours(self):
        record = hl.acquire(self.dir, "a", "x", 5, lister=QUIET)
        other = dict(record, id="someone-else")
        hl.write_atomic(self.dir, other, exclusive=False)
        hl.release(self.dir, record)
        self.assertEqual(self.lock()["id"], "someone-else")

    def test_run_returns_the_exit_code_and_releases(self):
        code = hl.run(self.dir, "a", "ok", 5, [sys.executable, "-c", "import sys; sys.exit(7)"], lister=QUIET)
        self.assertEqual(code, 7)
        self.assertFalse((self.dir / hl.LOCK_NAME).exists())
        self.assertEqual(self.events(), ["acquired", "released"])

    def test_run_holds_the_lease_while_the_command_runs(self):
        seen = Path(self._tmp.name) / "seen.json"
        script = ("import os, shutil, sys; "
                  f"shutil.copy(os.path.join({str(self.dir)!r}, 'heavy.lock'), {str(seen)!r})")
        self.assertEqual(hl.run(self.dir, "a", "hold", 5, [sys.executable, "-c", script], lister=QUIET), 0)
        held = json.loads(seen.read_text(encoding="utf-8"))
        self.assertEqual(held["lane"], "a")
        self.assertEqual(held["pid"], os.getpid())

    def test_run_kills_the_whole_tree_at_the_deadline(self):
        grandchild_file = Path(self._tmp.name) / "grandchild.pid"
        script = (
            "import subprocess, sys, time\n"
            "g = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'])\n"
            f"open({str(grandchild_file)!r}, 'w').write(str(g.pid))\n"
            "time.sleep(120)\n")
        started = time.monotonic()
        code = hl.run(self.dir, "a", "slow", 5, [sys.executable, "-c", script], deadline_s=3, lister=QUIET)
        self.assertEqual(code, hl.EXIT_DEADLINE)
        self.assertLess(time.monotonic() - started, 40)
        self.assertFalse((self.dir / hl.LOCK_NAME).exists())
        self.assertTrue(grandchild_file.exists(), "the grandchild never started; raise the deadline")
        self.assertTrue(wait_dead(int(grandchild_file.read_text())), "grandchild survived the kill")
        self.assertIn("killed-deadline", (self.dir / hl.LEDGER_NAME).read_text(encoding="utf-8"))

    def test_run_stops_when_the_headset_marker_appears(self):
        marker = Path(self._tmp.name) / "HEADSET-ON"
        script = (f"import pathlib, time; pathlib.Path({str(marker)!r}).write_text('on'); time.sleep(120)")
        code = hl.run(self.dir, "a", "x", 5, [sys.executable, "-c", script], lister=QUIET, extra_marker=marker)
        self.assertEqual(code, hl.EXIT_HEADSET)
        self.assertFalse((self.dir / hl.LOCK_NAME).exists())

    def test_run_refused_when_busy_does_not_start_the_command(self):
        record = hl.acquire(self.dir, "a", "x", 5, lister=QUIET)
        sentinel = Path(self._tmp.name) / "ran"
        with self.assertRaises(hl.Busy):
            hl.run(self.dir, "b", "x", 5, [sys.executable, "-c", f"open({str(sentinel)!r}, 'w')"], lister=QUIET)
        self.assertFalse(sentinel.exists())
        hl.release(self.dir, record)

    def test_default_lease_dir_honours_the_environment(self):
        saved = os.environ.get("ACVR_LEASE_DIR")
        os.environ["ACVR_LEASE_DIR"] = str(self.dir)
        try:
            self.assertEqual(hl.default_lease_dir(), self.dir)
        finally:
            if saved is None:
                del os.environ["ACVR_LEASE_DIR"]
            else:
                os.environ["ACVR_LEASE_DIR"] = saved

    def test_command_line_status_and_busy_exit_code(self):
        self.assertEqual(hl.main(["--lease-dir", str(self.dir), "status"]), 0)
        record = hl.acquire(self.dir, "a", "x", 5, lister=QUIET)
        code = hl.main(["--lease-dir", str(self.dir), "run", "--lane", "b", "--purpose", "x", "--",
                        sys.executable, "-c", "pass"])
        self.assertEqual(code, hl.EXIT_BUSY)
        hl.release(self.dir, record)


if __name__ == "__main__":
    unittest.main()
