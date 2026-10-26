# SPDX-License-Identifier: GPL-3.0-only
"""Policy: the tolerant title matcher (core/match) is for titles only.

Disc identity is bytes read from the disc and matched exactly. Nothing that scans files, reads
discs or builds the library may use the matcher, so a file name can never supply a disc identity.
"""
import pathlib
import sys

core = pathlib.Path(__file__).resolve().parents[1] / "src" / "core"
forbidden = ("scan", "library", "setup")
users = sorted(
    path.relative_to(core).as_posix()
    for path in core.rglob("*")
    if path.is_file() and path.suffix in {".cpp", ".h"} and "match/TitleMatch" in path.read_text(encoding="utf-8", errors="replace")
    and path.parent.name != "match")
bad = [user for user in users if user.split("/")[0] in forbidden]
if bad:
    print("the title matcher must not be used by disc or library code:", ", ".join(bad))
    sys.exit(1)
if not users:
    print("expected the matcher to be used by art or pinball code")
    sys.exit(1)
print("ok, used by:", ", ".join(users))
