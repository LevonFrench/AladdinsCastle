# After M1: four Codex sessions

Status: 2026-10-10, decided with the owner. One lead session and three lane sessions, all new. The lead does the main thrust itself and directs the other three from their reports.

| # | Session | In one line | Owns | Brief |
|---|---|---|---|---|
| 1 | **Lead** | Time Crisis in true 3D, launched from the Hub; libacvr; integrates and directs | `libacvr/`, the shared contracts, the `next` branch | [1-lead.md](1-lead.md) |
| 2 | **Hub** | Point at a folder and everything lights up; the VR menu controls; the M1 review backlog | `hub/` except the controls view | [2-hub.md](2-hub.md) |
| 3 | **Guns and controls** | Gun and control models, moving parts, a control set for every game, and the universal controls view | `assets/`, `data/guns/`, `data/controls/`, `hub/qml/controls/`, `hub/src/controls/` | [3-guns-controls.md](3-guns-controls.md) |
| 4 | **Stereo boards** | Per-eye rendering, one arcade board family at a time, starting with Namco System 22 | `setups/<board>-vr/` | [4-stereo-boards.md](4-stereo-boards.md) |

## Decisions (owner, 2026-10-10)

- **Main thrust:** the first true-3D game played from a Hub install. The game comes from lane 4's first board: **Time Crisis** on Namco System 22.
- **Board order for lane 4:** Namco System 22, then Sega Model 3 (Supermodel), then NAOMI / Dreamcast (Flycast), then Namco System 23. Lane 4 moves to the next board when one is done.
- **Linux and Steam Frame:** not exercised until someone with the hardware can test. CI keeps building and testing on Linux. Windows with SteamVR (Quest over ALVR) is the working target.
- **The lead directs.** The owner gives the lead three other sessions to get reports from and to steer.
- **Controls view:** one data model, drawn on the desktop detail page first and in VR's Control Mapping mode when libacvr can draw it.
- **Racing control sets:** lane 3, after the guns.
- Previously without an owner: the input bridge that makes VR controllers two light guns for flat emulators is the lead's second target; per-game gun calibration belongs to lane 4 for each board; game outputs to haptics is lane 3's data and the lead's runtime; releases are the lead's.

## Rules for every session

1. **Start state:** `main` after the M1 pull request is merged. Read [README](../../../README.md), [workshop.md](../../workshop.md), this page and your brief before anything else. The M1 ground rules still hold ([tasks/m1/README.md](../m1/README.md)): privacy, licences, tests in CI, a report with every pull request.
2. **Your own branch and worktree.** Lanes branch from `next` and open pull requests into `next`. Nobody commits to `main`. The lead opens `next` → `main` pull requests at checkpoints for outside review.
3. **Stay in your folders.** The "Owns" column is exclusive. If you need a change in another session's area, ask for it in your report.
4. **Ask the owner first, every time, for:** installing or updating any tool; using the GPU, SteamVR or the headset; touching the owner's game files, even one game; downloading any third-party release; writing Steam files. Say exactly what and why. One approval covers one thing.
5. **No game content, ever,** in the repository, in tests or in reports. Tests use synthetic fixtures. Private receipts stay under `.local/`.
6. **GPU-free by default.** Everything must build and test with `QT_QPA_PLATFORM=offscreen` and software rendering, as CI does. Headset and GPU checks are separate, owner-run acceptance steps, and passing tests never counts as passing them.
7. **Reports.** End every working block with `docs/status/<lane>/<date>-<n>.md` on your branch, pushed: done; verified and how; not verified; blocked on; questions for the lead; next. Under one page. No private data.
8. **The lead answers** in `docs/status/lead/<date>-<n>.md` on `next`: answers, the next target for each lane, merge order, and one combined headset checklist so a single sitting with the owner checks everything that is ready.
9. **Previous work to read, not to redo:** the M1 reviews in `docs/reviews/`, and on the owner's machine the stop handoff and the six-report agency review under `.local/handoffs/2026-10-10-stop/` in the main checkout (private; cite findings by their ids, never copy private details into the repository).

## Order

1. The M1 pull request merges to `main`; the lead creates `next`.
2. Days one and two: the lead publishes the contracts the lanes build against (the libacvr header with gun slots and models, the control-set format, the renderer interface for lane 4) and the first directions file.
3. Lanes run in parallel. Lane 2 starts with the review backlog and the overlay input. Lane 3 starts with tier 1 gun models. Lane 4 starts with System 22 side-by-side output.
4. First checkpoint: Time Crisis side-by-side on the desktop (lane 4), guns drawn from data (lane 3), overlay passes its headset checklist (lane 2), libacvr shows a tracked gun in an empty scene (lead).
5. Second checkpoint: Time Crisis in the headset from a Hub install.
