# Working with agents on this repository

Claude Code and Codex sessions work here in lanes: one branch, one worktree and one
deliverable each. These pages are the rules a lane follows. They add no permission:
only the owner's own words in chat grant one.

| Page | What it covers |
|---|---|
| [lane-contract.md](lane-contract.md) | The one-page contract every lane starts from: budget, resources, authority, run status, recovery, cost report |
| [heavy-lease.md](heavy-lease.md) | `tools/heavy_lease.py`, the machine-wide lease for builds, renders, emulators and measurements |
| [pre-push.md](pre-push.md) | `tools/pre_push.py`, the push guard the owner can switch on |

Standing rules that apply to every lane:

- Heavy work (builds, ctest, ffmpeg, Blender, emulators, anything that uses the GPU or most
  cores for more than a minute) goes through the lease, never straight from a shell.
- A lane may commit to its own branch and push it after `python tools/check_public_privacy.py`
  passes. It does not push to `main`, does not merge, and does not touch
  `codex/libacvr-contract`.
- Every claim carries an evidence level: E1 started or partly works, E2 works in a hidden
  offline proof, E3 the owner played it in the headset. Agents cannot produce E3.
- Private paths, game files, saves, states, keys and owner receipts stay in ignored
  folders (`.local/`). Nothing private goes in a tracked file.
