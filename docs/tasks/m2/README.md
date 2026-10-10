# After M1: four Codex sessions (draft for the workshop)

Status: 2026-10-10, **draft**. Nothing here starts until the M1 work is finished and checked: PR #8 reviewed at its final head, the agency review read, and the open points below decided. The final brief for each session is written then.

## The shape

One lead session and three lane sessions. The lead does the main thrust itself and directs the other three from their reports.

| # | Session | In one line | Owns |
|---|---|---|---|
| 1 | **Lead: first true-3D game from the Hub** | libacvr v0, the first game you can stand inside, launched from the Hub; integrates everything | `main`, merges, the shared contracts, `libacvr/`, headset acceptance runs |
| 2 | **Front end: point at a folder and it lights up** | Hub UI, finding files, art, the VR menu controls | `hub/qml`, `hub/src/ui`, scanner, art, overlay input |
| 3 | **Guns and controls** | Gun and control models, their moving parts, and one universal view of "this part does this, and it is this button on your controller" | `assets/`, `data/guns/`, control sets, the controls view |
| 4 | **Stereo on one board** | Real per-eye rendering for one arcade board family, delivered as a renderer the lead plugs into libacvr | one `setups/<board>-vr/` |

### 1. Lead: first true-3D game from the Hub

- **Goal:** install a true-3D setup from the Hub, press Start in VR, and play it in the headset with a tracked gun.
- **Does itself:** libacvr v0 on the [contract](../../libacvr-contract.md) (OpenXR session, frame loop, controller input, gun slots and aim projection, haptics, pause); drawing gun models in the hand ([guns.md](../../guns.md)); Hub install and launch of VR variants; the quick wins that need little code (the hotd2-vr recipe; Dolphin's built-in stereo on a 3D screen).
- **Directs:** reads each lane's report, answers its questions, sets its next target, and decides merge order. Publishes the contracts the lanes build against in its first days: the libacvr header, the control-set format, the scan-result interface.
- **Done when:** one game is played start to finish in true 3D from a Hub install on the owner's PC.

### 2. Front end: point at a folder and it lights up

- **Goal:** first run: choose your games folder; a minute later every game the Hub knows turns Ready with its art, the emulators you already have are found, and anything missing says exactly what is missing.
- **Work:** the "Find my files" flow end to end with progress and a found / not found / why report; disc identification beyond PlayStation 2 (Dreamcast, Saturn, GameCube, Wii serials; GDI and CUE sets); art from the folders beside the ROMs; the leftovers from the M1 review (file work still on the UI thread, settings that apply at once); the VR menu control scheme and its diagnostic page ([ui/06-vr-menu-controls.md](../../ui/06-vr-menu-controls.md)); the Steam library entries.
- **Done when:** on a fresh portable folder, pointing at the owner's library lights up every owned game with no manual steps, and the dashboard overlay passes its headset checklist.

### 3. Guns and controls

- **Goal:** every game shows the real control in your hand and tells you what each part does.
- **Work:** the gun models from scripts, tier by tier ([guns-models.md](guns-models.md)), then the ghost controls for racing; moving parts (trigger, slide, pump, selector, buttons, wheel, shifter) driven by named nodes; a **control set for every game**, by rule first and by hand where needed: cabinet part → game input → default controller button; and the **universal controls view** built on that data: the model with a callout per part, "Pump → Reload → flick", "A → Pedal → right grip". The same view is the detail page's Controls section on the desktop and the Control Mapping mode in VR.
- **Done when:** every gun game resolves to a model and a complete control set, the view renders from data alone for any game, and tier 1 and 2 models pass their checker.

### 4. Stereo on one board

- **Goal:** one board family's games drawn correctly once per eye.
- **Work:** take the 3D data at the right seam for that board ([landscape.md](../../landscape.md); wiki article `stereo-3d-for-arcade-games`), render per eye with the quality rules (off-axis frusta, world scale, HUD on a plane, aim dot at hit depth, re-draw at headset rate), and expose it to libacvr's host interface. Until libacvr is ready it outputs side-by-side to a desktop window, so it can be built and checked without a headset.
- **Done when:** every game on the chosen board renders in stereo side-by-side with correct depth order and no missing geometry when the view turns, and the first one runs through libacvr in the headset.

## How the lead directs

- Each lane works on its own branch and worktree and never commits to `main`.
- A lane ends every working block with a short report on its branch: `docs/status/<lane>/<date>.md` with done, verified and how, blocked on, questions for the lead, next. No private data.
- The lead fetches the branches, reads the reports, and writes `docs/status/lead/<date>.md` on `main`: answers, the next target per lane, merge decisions.
- Headset time is the scarce thing. The lead keeps one acceptance checklist across all lanes, so one sitting with the owner checks everything that is ready.
- Pull requests go to the lead's review, then to the owner. Nobody merges their own work.

## To decide in the workshop

1. **Which board for lane 4.** Namco System 22 (Time Crisis, Rave Racer and the other racers; the decided M2 target; open MIT source; already proven in VR by DR-89), Sega Model 3 through Supermodel (the cleanest re-render and a larger catalog), or NAOMI / Dreamcast by building on hotd2-vr.
2. **What the main thrust is.** A first true-3D game (above), or first making every gun game playable with tracked guns on a flat or 3D screen and adding true 3D board by board.
3. **Whether PR #8 merges first**, so all four sessions start from one `main` that has the Hub.
4. **Where the universal controls view lives first:** desktop detail page, VR, or both from the start.
5. **When Linux and the Steam Frame get a turn.** Everything so far is exercised on Windows; the Frame is the first-priority platform.
6. **Who writes control sets for racing games** (lane 3 after guns, or a later lane).

## Things that have no owner yet

- The input bridge that lets VR controllers act as two light guns for flat emulators (MAME, Flycast): the fastest way to make the whole gun catalog playable.
- Per-game gun calibration (the constants that map a screen point to each game's gun input).
- Recoil and lamp outputs from games to haptics.
- Releases: versioned portable builds and a tester quickstart.
- The six two-player games that need a view per player.
