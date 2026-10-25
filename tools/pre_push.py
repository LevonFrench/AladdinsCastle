#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pre-push check: refuse protected refs, then run the public privacy check.

Git runs a pre-push hook with the remote name and URL as arguments and one line
per ref on stdin: "<local ref> <local sha> <remote ref> <remote sha>".
tools/hooks/pre-push calls this script. It is not installed by the repository;
see docs/agents/pre-push.md for how the owner turns it on.

Refused, with no override from the environment:
  - any push whose remote ref is main or master (including deleting it);
  - any push whose local or remote ref name contains "libacvr-contract".
`git push --no-verify` skips the hook. That is the owner's call, never an agent's.
"""
import pathlib
import subprocess
import sys
import unittest

PROTECTED_BRANCHES = ("main", "master")
PROTECTED_SUBSTRINGS = ("libacvr-contract",)
ZERO_SHA = "0" * 40


def refusals(lines):
    """Reasons to refuse, from the pre-push stdin lines."""
    reasons = []
    for line in lines:
        parts = line.split()
        if len(parts) < 4:
            continue
        local_ref, _, remote_ref, _ = parts[:4]
        for ref in (remote_ref, local_ref):
            name = ref[len("refs/heads/"):] if ref.startswith("refs/heads/") else ref
            if ref == remote_ref and name in PROTECTED_BRANCHES:
                reasons.append(f"push to {name} is refused: open a pull request; the owner merges")
            if any(s in ref for s in PROTECTED_SUBSTRINGS):
                reasons.append(f"{ref} is protected and never pushed")
    return list(dict.fromkeys(reasons))


def repo_root():
    out = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True)
    return pathlib.Path(out.stdout.strip()) if out.returncode == 0 and out.stdout.strip() else None


def main(argv=None, stdin=None):
    lines = (stdin if stdin is not None else sys.stdin).read().splitlines()
    reasons = refusals(lines)
    if reasons:
        for reason in reasons:
            print(f"pre-push: {reason}", file=sys.stderr)
        return 1
    root = repo_root()
    check = root / "tools" / "check_public_privacy.py" if root else None
    if check is None or not check.is_file():
        print("pre-push: tools/check_public_privacy.py not found; refusing the push", file=sys.stderr)
        return 1
    result = subprocess.run([sys.executable, str(check)], cwd=root)
    if result.returncode:
        print("pre-push: check_public_privacy.py failed; fix it before pushing", file=sys.stderr)
        return 1
    return 0


class Tests(unittest.TestCase):
    def test_main_and_master_refused(self):
        for branch in ("main", "master"):
            line = f"refs/heads/work abc refs/heads/{branch} def"
            self.assertTrue(refusals([line]))
        self.assertTrue(refusals([f"(delete) {ZERO_SHA} refs/heads/main abc"]))

    def test_local_main_to_a_feature_branch_is_allowed(self):
        self.assertEqual(refusals(["refs/heads/main abc refs/heads/claude/x def"]), [])

    def test_contract_branch_refused_either_side(self):
        self.assertTrue(refusals(["refs/heads/codex/libacvr-contract abc refs/heads/codex/other def"]))
        self.assertTrue(refusals(["refs/heads/other abc refs/heads/codex/libacvr-contract def"]))

    def test_ordinary_branch_and_tags_allowed(self):
        self.assertEqual(refusals(["refs/heads/claude/fix abc refs/heads/claude/fix def",
                                   "refs/tags/v1 abc refs/tags/v1 def", "", "short"]), [])

    def test_branch_names_containing_main_are_allowed(self):
        self.assertEqual(refusals(["refs/heads/a abc refs/heads/main-menu def",
                                   "refs/heads/a abc refs/heads/domain def"]), [])


if __name__ == "__main__":
    if "--test" in sys.argv:
        sys.argv.remove("--test")
        unittest.main()
    sys.exit(main())
