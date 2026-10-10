# Session 2: Hub. Point at a folder and everything lights up

You own the Hub application: everything under `hub/` except `hub/qml/controls/` and `hub/src/controls/` (lane 3). You report to the lead session. Read [README.md](README.md) first: lane table, rules for every session, report format.

## Goal

On a fresh portable folder the owner chooses their games folder once. A minute later every game the Hub knows and the owner has turns Ready with its art, the emulators already on the PC are found, and anything missing says exactly what is missing. In the headset, the same Hub is comfortable to control with the laser and the stick.

## Start here: the backlog from the M1 reviews

Fix these first, each with a test that fails before the fix. Ids are from the agency review summary (on the owner's machine under `.local/handoffs/2026-10-10-stop/agency-review/`) and from `docs/reviews/`.

1. **A4** Rollback runs after the operation lock is released (the lock lives inside the `try` block in `Engine::install` and `uninstall`). Hold the lock through rollback.
2. **A5** A malformed catalog record can throw during a scan, and the scan's completion reads the result with no exception boundary. Skip the bad record, guard completion, keep the last good state.
3. **A6** Recovery's inverse rename trusts the journal's destination path; artifact promotion can return an existing cache object without checking its bytes. Validate both.
4. **A7** An invalid authored variant with the same id suppresses a working generated flat route.
5. **A8** Two Hub copies do not share a lock for one Steam target; launch and shared-tool removal use different locks.
6. **A2 / A3** Wording: catalog capability, recipe, installed, ready and accepted are different states. Make labels and the README say which one is meant. Fix the spike instructions that still describe a keyboard that opens on focus.
7. Remaining lows from the R3 re-check: an install clicked during the startup sweep is refused instead of waiting; the sweep compares paths as case-sensitive strings; a failed uninstall shows "INSTALL STOPPED"; the overlay fallback reason is overwritten by "Scan requested"; quitting the Hub while a game runs loses last-played; the CHD refusal reason is never shown; restoring saved scan results re-walks the whole file list for every binding on the UI thread; `tools/validate_catalog.py` still crashes when `controls.type` is a list; a truncated uncompressed CHD passes the bounds check; a broken recipe with no flat route shows "Coming soon" instead of an error; the privacy audit still flags ordinary image names such as `card01.png`.
8. Deferred from R1: file work still on the UI thread (uninstall preview, Steam preview and save, README reads); settings that only apply at the next install.

## The VR menu controls

Spec: [ui/06-vr-menu-controls.md](../../ui/06-vr-menu-controls.md). The owner's first headset session: clicks failed all over the panel, the stick did not scroll, on-panel letter keys failed (digits worked), on stock Quest Touch controllers.

1. A patch of unbuilt, untested overlay input changes from the previous session is saved on the owner's machine (`.local/handoffs/2026-10-10-stop/overlay-wip.patch`, with the XR reviewer's findings XR-01 to XR-05). Treat it as a candidate to read, not as working code.
2. Build the **diagnostic page** first (§3 of the spec), then the input contract: button events at the last move position per cursor; pointer frozen from trigger-down; a release within the click slop counts as a click on the pressed control; overlay drag threshold; smooth scroll × 2880 and discrete × 120, sent to the list under the scrolling controller; cancel without click on focus loss or hide.
3. Tests through the real QML scene, offscreen: every panel key by hit geometry, wheel events that move the list's `contentY`, two cursors, stale and missing coordinates.
4. Then ask the lead for a headset sitting and run the spec's §4 checklist.

## Find my files

- One flow: choose folders → scan with progress and cancel → a report grouped as found / found but incomplete (say what is missing) / not recognised → cards update live.
- Disc identification beyond PlayStation 2: Dreamcast, Saturn, GameCube and Wii ids; GDI and CUE sets; per-platform id indexes built by a tool, never from the owner's files.
- Arcade sets with CHDs, BIOS sets and clones keep the rules fixed in the M1 reviews.
- Art from the folders beside the ROMs ([art-pipeline.md](../../art-pipeline.md) §0), generated fallback otherwise.
- Emulators: found, or offered for install where the licence gate allows.
- The Steam library entries: the first real entry needs the owner's explicit go-ahead and its own check.

## With the other lanes

- The lead needs VR variants to install and launch from the normal UI; today the UI only allows generated flat variants. Agree the change with the lead.
- Lane 3 gives you a controls view component to place on the detail page. You place it; you do not edit it.

## Done when

- The backlog above is closed with tests, CI green on Windows and Linux.
- On the owner's PC, pointing a fresh Hub at the owner's library lights up every owned game with no manual step, and the report explains every file it could not use.
- The dashboard overlay passes the headset checklist in the menu spec.
