# R2: outside review of `next` at 312032a, and how to resume

Status: 2026-10-10, written after the owner's soft stop. Read it after [W-wii-chronicles.md](W-wii-chronicles.md). Where it differs from an earlier brief or from [R1-redirect.md](R1-redirect.md), this page wins. It does not restart work by itself: the owner restarts work in chat, and the approvals are the ones the owner gives there.

## Verdict

The stop is clean and the reports are honest about what was not proved. The code that was read holds up. **But there is still no picture in the headset and nothing a player can do.** The one experiment that needs none of our code, the pinned DolphinXR build running Umbrella Chronicles, has been unpacked and ready since the first checkpoint and has not been run.

Two causes, both of order and method, not of code:

1. **Order.** The lead prompt put the fallback (ordinary Dolphin, our input bridge, our viewer) before the DolphinXR sitting. That ordering came from the outside reviewer and it was wrong: the fallback needed three new programs to work together, and the sitting queued behind them. **DolphinXR goes first from now on.**
2. **Method.** The sittings depended on a session driving the owner's desktop and on a viewer that refuses to run when any other window overlaps the picture. Both stopped on ordinary desktop conditions. From now on the owner starts a sitting from one script and the session reads the logs.

## What was checked

| Checked | How | Result |
|---|---|---|
| Unpublished commits on local `next` | Read the seven commits past the published head; verified the handoff bundle | Intact. Pull requests 37 and 38 pass their hosted checks. The combined head has no hosted run yet |
| Catalog, gun assets, tool tests | Run on a scratch copy of `next` | 413 games, 0 errors; gun assets 0 errors; 46 tool tests, 6 bridge tests, 13 profile tests pass |
| The Umbrella Chronicles recipe | Read | Pins the DolphinXR release and executable by hash, installs into the game's own folder with its own settings, turns VR on only at launch. Sound. Never installed or launched for real |
| The Hub's C++ changes | Not built here | Relies on the hosted checks |
| Theatre viewer and profile tool | Independent read, two points re-checked in source | Findings 2 to 5 |
| Input bridge | Independent read, three points re-checked in source | The protocol reads correct and it binds to this machine only. Findings 13 to 17 |
| Agency and studio review | Read; two findings re-checked in source | Agreed. Findings 6 to 9 |

## Findings

| # | Lane | Finding | Do |
|---|---|---|---|
| 1 | Lead | No headset image. The DolphinXR sitting was never run | Make it the first thing the owner does: one private script that starts the pinned build with its own settings folder, VR on and the disc, plus a short checklist |
| 2 | Viewer | The window guard (`theatre/viewer.cpp`, the loop before capture; rule in `theatre/core.h` `occludes`) aborts if any visible window above Dolphin overlaps its picture. Only the agent's own cursor overlay is exempt. The tool's console, a SteamVR window, a notification or a Dolphin dialog all abort the run, with no grace period. This is what refused both retries | Capture the window with Windows.Graphics.Capture, which does not care what is on top, and drop the guard. Keep only "window gone or minimised" |
| 3 | Viewer | Capture is a GDI `BitBlt` from the window. That often returns black or stale pixels from a GPU-drawn window, and the "not black" check passes on a plain grey background | Same change as 2. Replace "not black" with "two frames a second apart differ" |
| 4 | Viewer | Nothing sets Dolphin's render window size, so the picture is whatever the default is. The plane width assumes how SteamVR sizes a side-by-side overlay (`core.h`, the `*2.0`), which the header does not state | Set a 16:9 window size in the profile. Check the plane's proportions in the first headset image and correct the factor then |
| 5 | Viewer | Error 23 is `VROverlayError_RequestFailed`. The likely cause was the size limit of raw overlay uploads. The Direct3D texture path that replaced it reads correct but has never run on hardware | Nothing to change until it runs |
| 6 | Guns, Hub | The Wii control sets carry no route key, so the controls page shows the DSU bridge mapping when the native DolphinXR route is selected | Key control sets by route. Show the one that matches the selected setup, or say plainly that the native route is mapped inside DolphinXR |
| 7 | Lead, Guns | The gun shader is flat colour with no lighting (`libacvr/src/gun_gl.cpp`, the fragment shader). The model previews are lit, so they do not show what the headset would draw | Add simple lighting to the runtime shader (normals, one directional light, ambient) and render previews with the same model. Scene-matched lighting can come later |
| 8 | Guns | Sights on five models point a few degrees off the bore (up to 5.4), and every rear sight is a solid block | Fix `generic-pistol` first: one sight line parallel to the bore, a rear notch and a front blade, on both detail levels. Then the others |
| 9 | Lead | The pinned build never redraws between game frames ([W-wii-framerate-presets.md](W-wii-framerate-presets.md), "Frame handoff"). A gun drawn into the game's picture moves at the game's rate | Plan the gun as its own pass every refresh unless the frame rate lane shows the game reaches the refresh rate |
| 10 | All | Public files describe the test discs with words like "approved" and "supplied", and user instructions name one headset and one streamer | Test records say only "tested on (game id)". User instructions say "a SteamVR headset" |
| 11 | Hub | Disc identity at launch is the scanner's snapshot, refreshed on size and time. A same-size, same-time swap keeps the old identity (agency finding) | Re-read the disc header at launch. Low priority |
| 12 | Guns | The mounted gun declares yaw and pitch pivots that nothing drives, and moving them would separate the barrel from the muzzle | Leave as is until something drives them |
| 13 | Lead | Bridge motion signs are unproven (`tools/wii-bridge/motion.py`). The accelerometer negates all three axes and the gyro negates two, so the two disagree in handedness on at least one axis. Every test uses an identity pose and restates the author's own signs | Take the conventions from Dolphin's DSU client source, which is on disk. Add tests with rotated poses. Then confirm live in Dolphin's motion input screen: at rest it reads 1 g up; roll right, push forward and push right each give the expected sign |
| 14 | Lead | A hand that is not tracked is reported as a disconnected slot (`dsu.py`). If Dolphin only creates a DSU device for a connected slot, starting it while a controller sleeps or the dashboard is open leaves every binding dead | Report both slots as connected always and send neutral data while a hand is untracked |
| 15 | Lead | When the aim misses the plane the pointer jumps to the centre of the screen, and the trigger is blocked whenever the aim is off the plane. Games that reload by firing off-screen cannot | Off the plane: hold the pointer at the nearest edge or hide it (Dolphin's Point group has a Hide input, to be checked), and let the trigger through. Make it a per-game setting |
| 16 | Lead | Acceleration is the driver's velocity differenced on a UI timer of about 64 Hz, zeroed after any gap over 50 ms. A missed tick can swallow a shake peak. Whether the streamer fills velocity at all is unknown | Read poses on a thread at a fixed higher rate, take acceleration over a short window, and log peak g. Check live that velocity is not zero and what a hard shake reads |
| 17 | Lead | The generated profile sets no pointer yaw and pitch range, so the bridge's plane edges and Dolphin's cursor range may not agree | Set them in the profile to match the plane. Check the four corners live |

Smaller bridge points: the recentre and calibration hotkeys may be swallowed by the UI loop, and a calibration file without a revision never loads.

## Method from here

1. **DolphinXR first.** The fallback tier continues, but never ahead of it.
2. **The owner starts sittings.** One script per sitting, kept private under `.local/`, and a checklist of at most ten things to look at. The session reads logs afterwards.
3. **The desktop.** When the owner is at the PC, no session drives it. When the owner says it is free, one session at a time may, and the lead hands out the turn.
4. **Bounded attempts.** A failed live attempt gets one diagnosis and one retry. Then it is written down and the lane moves on.
5. **A quiet machine for measurements.** Frame-rate runs happen when no lane is building or rendering, and the report says what else was running.
6. **Plain reports.** Sentences with spaces around numbers and names. Half a page. A stop needs a README and the bundle of unpublished commits, not a manifest of hashes.

## The first sitting

One combined checklist, in this order:

1. **DolphinXR with Umbrella Chronicles** (about ten minutes). Does a picture appear? Is the world around you or on a screen? Where is the HUD and does it stay put when you turn your head? Does the scene reach the edges of your view? Does a controller move the pointer? Does it feel smooth on head turns? Then the same with `EagerHeartbeat` on.
2. **The VR menu Flip Y test** from [../../ui/06-vr-menu-controls.md](../../ui/06-vr-menu-controls.md), still owed.
3. **The fallback viewer,** only if finding 2 is fixed and a real capture has been seen on the desktop.

The base is chosen after item 1, from what was seen.
