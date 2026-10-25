# SPDX-License-Identifier: GPL-3.0-only
"""The Qt receipt wrapper must preserve failure and stop a stuck child."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def alive(pid):
    """Whether a process is still running (Windows only; the tree kill is a Windows feature)."""
    listing = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True).stdout
    return f" {pid} " in listing


def check_tree_kill(cmake, runner, root, program):
    """A timeout must end the child's own children too, or they keep its log and temporary files locked."""
    pid_file = root / "grandchild.pid"
    try:
        result = subprocess.run(
            [cmake, f"-DTEST_EXECUTABLE={sys.executable}", f"-DTEST_ARGUMENTS=-B;{program}",
             f"-DTEST_LOG={root / 'tree-report.txt'}", "-DTEST_TIMEOUT=8", "-P", runner],
            env={**os.environ, "AC_QT_RUNNER_FIXTURE": "tree", "AC_QT_RUNNER_GRANDCHILD": str(pid_file)},
            capture_output=True, text=True, timeout=40,
        )
        output = result.stdout + result.stderr
        assert result.returncode != 0 and "timeout" in output.lower(), output
        assert pid_file.is_file(), "the fixture never started its grandchild: " + output
        grandchild = int(pid_file.read_text())
        assert not alive(grandchild), f"grandchild {grandchild} survived the timeout: " + output
        print("PASS Qt runner tree kill")
    finally:
        if pid_file.is_file():
            subprocess.run(["taskkill", "/F", "/PID", pid_file.read_text()], capture_output=True)


def main():
    cmake, runner = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="qt-runner-") as temporary:
        root = Path(temporary)
        program = root / "fixture.py"
        program.write_text(
            "import os, pathlib, sys, time\n"
            "mode = os.environ['AC_QT_RUNNER_FIXTURE']\n"
            "if mode not in ('missing', 'missing-success'): pathlib.Path(sys.argv[2].rsplit(',', 1)[0]).write_text('synthetic Qt report\\n')\n"
            "if mode == 'hang': time.sleep(4)\n"
            "if mode == 'tree':\n"
            "    import subprocess\n"
            "    child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])\n"
            "    pathlib.Path(os.environ['AC_QT_RUNNER_GRANDCHILD']).write_text(str(child.pid))\n"
            "    time.sleep(60)\n"
            "sys.exit(7 if mode in ('fail', 'missing') else 0)\n",
            encoding="utf-8",
        )
        # Run Python directly: timeout ownership must not end at an intermediate shell.
        for mode in ("pass", "missing-success", "fail", "missing", "hang"):
            result = subprocess.run(
                [cmake, f"-DTEST_EXECUTABLE={sys.executable}", f"-DTEST_ARGUMENTS=-B;{program}",
                 f"-DTEST_LOG={root / 'report.txt'}",
                 f"-DTEST_TIMEOUT={1 if mode == 'hang' else 15}", "-P", runner],
                env={**os.environ, "AC_QT_RUNNER_FIXTURE": mode},
                capture_output=True, text=True, timeout=25,
            )
            output = result.stdout + result.stderr
            if mode == "pass":
                assert result.returncode == 0 and "synthetic Qt report" in output, output
            elif mode == "missing-success":
                assert result.returncode != 0 and "without a readable, nonempty report" in output, output
            elif mode == "hang":
                assert result.returncode != 0 and "Qt test failed" in output and "timeout" in output.lower(), output
            else:
                assert result.returncode != 0 and "Qt test failed (7)" in output, output
            print(f"PASS Qt runner {mode}")
        if sys.platform == "win32":
            check_tree_kill(cmake, runner, root, program)


if __name__ == "__main__":
    main()
