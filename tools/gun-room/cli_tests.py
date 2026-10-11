# SPDX-License-Identifier: MIT
import json
import os
import pathlib
import subprocess
import sys
import tempfile


def main() -> None:
    executable = pathlib.Path(sys.argv[1])
    build = pathlib.Path(sys.argv[2])
    with tempfile.TemporaryDirectory(prefix="room-cli-", dir=build) as temp:
        root = pathlib.Path(temp)
        (root / "data" / "guns").mkdir(parents=True)
        (root / "assets" / "guns").mkdir(parents=True)
        (root / "data" / "product.json").write_text(
            json.dumps({"schema": 1, "display_name": "Fixture Product"}), encoding="utf-8"
        )
        (root / "data" / "guns" / "fixture.toml").write_text("id='fixture'\n", encoding="utf-8")
        (root / "assets" / "guns" / "fixture.glb").write_bytes(b"fixture")

        def run(*args: str) -> int:
            return subprocess.run(
                [str(executable), "--root", str(root), "--model", "fixture", *args],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                timeout=5,
            ).returncode

        def prequeued_control(payload: bytes, *args: str, eof: bool = False) -> int:
            read_fd, write_fd = os.pipe()
            try:
                if payload:
                    os.write(write_fd, payload)
                if eof:
                    os.close(write_fd)
                    write_fd = -1
                process = subprocess.Popen(
                    [str(executable), "--root", str(root), "--model", "fixture", "--xr",
                     "--control-stdin", *args],
                    stdin=read_fd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                )
                os.close(read_fd)
                read_fd = -1
                try:
                    process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
                    raise
                return process.returncode
            finally:
                if read_fd >= 0:
                    os.close(read_fd)
                if write_fd >= 0:
                    os.close(write_fd)

        assert run("--preview-svg", "sample.svg", "--right-angle-deg", "garbage") == 2
        assert not (root / ".local").exists(), "invalid number wrote output directory"
        assert run("--preview-svg", "../escape.svg") == 2
        assert not (root / ".local").exists(), "invalid path wrote output directory"
        assert run("--preview-svg", "bad-hand.svg", "--hand", "both") == 2
        assert not (root / ".local").exists(), "invalid hand wrote output directory"
        for bad in ("../escape", "a/b", "name.ppm", ""):
            assert run("--xr", "--capture-eyes", bad) == 2
        assert run("--preview-svg", "safe.svg", "--capture-eyes", "eyes-one") == 2
        assert run("--preview-svg", "safe.svg", "--seconds", "4") == 2
        assert run("--preview-svg", "safe.svg", "--seconds", "10") == 2
        assert run("--preview-svg", "safe.svg", "--control-stdin") == 2
        assert run("--preview-svg", "safe.svg", "--calibrate-grip") == 2
        assert run("--xr", "--calibrate-grip", "--capture-eyes", "eyes-two") == 2
        assert run("--xr", "--calibrate-grip", "--angle-file", "old.txt") == 2
        assert not (root / ".local").exists(), "invalid capture options wrote output directory"
        wrong_handle = subprocess.run(
            [str(executable), "--root", str(root), "--model", "fixture", "--xr", "--control-stdin"],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            check=False, timeout=5,
        )
        assert wrong_handle.returncode == 2, "non-pipe stdin must fail before XR create"
        assert not (root / ".local").exists(), "invalid control handle wrote output directory"
        assert prequeued_control(b"close\n") == 0, "queued close must exit before XR create"
        assert prequeued_control(b"", eof=True) == 0, "queued EOF must exit before XR create"
        assert prequeued_control(b"cl", eof=True) == 0, "queued partial EOF must exit before XR create"
        assert prequeued_control(b"close\n", "--calibrate-grip") == 0
        assert prequeued_control(b"wrong\n") == 9, "queued malformed input must fail before XR create"
        assert not (root / ".local").exists(), "precreate control changed output directory"
        assert run("--preview-svg", "sample.svg") == 0
        preview = root / ".local" / "gun-room" / "sample.svg"
        before = preview.read_bytes()
        assert b"<svg" in before
        assert run("--preview-svg", "sample.svg") == 2
        assert preview.read_bytes() == before, "existing preview changed without replace"
        assert run("--preview-svg", "sample.svg", "--replace-preview") == 0
        assert run("--xr", "--angle-file", "grip.txt", "--right-angle-deg", "12oops") == 2
        assert not (preview.parent / "grip.txt").exists()
        assert run("--preview-svg", "another.svg", "--angle-file", "grip.txt", "--right-angle-deg", "12") == 0
        angle = preview.parent / "grip.txt"
        saved = angle.read_text(encoding="utf-8")
        assert "right 12" in saved
        assert run("--preview-svg", "unused.svg", "--angle-file", "grip.txt", "--right-angle-deg", "15") == 2
        assert angle.read_text(encoding="utf-8") == saved
        assert run("--preview-svg", "updated.svg", "--angle-file", "grip.txt", "--right-angle-deg", "15", "--update-angle-file") == 0
        assert "right 15" in angle.read_text(encoding="utf-8")
        assert run("--preview-svg", "left.svg", "--hand", "left", "--angle-file", "grip.txt",
                   "--left-angle-deg", "-7", "--update-angle-file") == 0
        assert "right 15" in angle.read_text(encoding="utf-8")
        assert "left -7" in angle.read_text(encoding="utf-8")
        calibration = preview.parent / "calibration"
        calibration.mkdir()
        (calibration / "fixture-right.json").write_text("invalid", encoding="utf-8")
        assert prequeued_control(b"close\n", "--calibrate-grip") == 2, "bad calibration must fail before XR"
        assert run("--xr", "--preview-svg", "blocked.svg") == 2
        assert not (preview.parent / "blocked.svg").exists(), "invalid saved calibration wrote preview"
        assert prequeued_control(b"close\n", "--angle-file", "grip.txt") == 0, "legacy yaw path must keep precedence"
        (calibration / "fixture-right.json").unlink()
        assert prequeued_control(b"wrong\n", "--capture-eyes", "control-bad") == 9
        receipt = json.loads((preview.parent / "control-bad" / "receipt.json").read_text(encoding="utf-8"))
        assert receipt["status"] == "error" and receipt["reason"] == "control_stdin"
        outside = root / "outside.txt"
        outside.write_text("untouched", encoding="utf-8")
        alias = preview.parent / "alias.txt"
        try:
            os.symlink(outside, alias)
        except OSError:
            pass  # Windows developer mode may be unavailable in a CPU test environment.
        else:
            assert run("--preview-svg", "alias.svg", "--angle-file", "alias.txt",
                       "--right-angle-deg", "9", "--update-angle-file") == 2
            assert outside.read_text(encoding="utf-8") == "untouched"


if __name__ == "__main__":
    main()
