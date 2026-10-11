# Sidecar brief: Wii frame rate and VR presets

Status: 2026-10-10, research only. **Nothing below has been run by us.** Every code is untested and every preset value is a starting value to be measured. This brief belongs to the main thrust in [W-wii-chronicles.md](W-wii-chronicles.md) and is judged against [3d-mode-target.md](../../3d-mode-target.md).

Outside review, 2026-10-10: the Umbrella and Overkill codes, the override setting and the frame handoff were checked against the source of Dolphin and of the pinned DolphinXR build (commit `85c4266`). Lines marked "checked" come from that; everything else is as the research left it.

## Goal

**Owner's target (2026-10-10): each game runs at the headset's own refresh rate: 90 on a 90 Hz headset, and 72, 80, 120 or 144 on headsets set to those.** 60 is a step on the way, not the goal. This matches the first checkpoint in [W-wii-chronicles.md](W-wii-chronicles.md) ("at the headset's refresh rate").

For the five tier 1 games, find out, on a monitor first and in the headset second:

1. how close each game gets to the headset's refresh rate in Dolphin, by which route, and what that costs or breaks;
2. which graphics and streaming settings to ship as the starting preset for each class of headset.

The output is a filled-in results table per game and a corrected preset table, with the measurements behind them.

## Lane

A fifth session beside the four in [README.md](README.md), under the same rules: branch from `next`, its own worktree, pull requests into `next`, one report per checkpoint in `docs/status/framerate/`, questions to the lead.

- **Owns:** `data/performance/` (new: per-game frame-rate results and the headset-class presets; agree the layout with the lead in the first report) and `docs/status/framerate/`.
- **Does not edit:** `setups/dolphin-vr/`, `tools/wii-dolphin-profile/`, `data/controls/`, `hub/`, `libacvr/`. A code or setting that belongs in a shipped profile is a request to the lane that owns it, with the measurement behind it.
- **Base:** the DolphinXR build already pinned in `setups/dolphin-vr/upstream-pin.json`, run with a separate user directory under `.local/` as that setup does, so nobody's settings are touched. No other build is downloaded without asking.
- **Headset time:** this lane holds no sittings of its own. Its headset checks go into the lead's combined checklist.
- **The owner's desktop:** do not drive it. Prefer runs that need no input and no window kept on top; where a run needs either, batch those runs and ask the owner for a time.

## What is known

Hub wiki topic `vr-arcade-gun-racing`, articles `wii-frame-rate-unlocks` and `vr-presets-by-headset-class` (research 2026-10-10). "Unverified" means one source, or a search result whose page was not read.

| Game | Ids | Native rate | 60 fps code |
|---|---|---|---|
| Resident Evil: The Umbrella Chronicles | RBUE08, RBUP08, RBUJ08, RBUK08 | 30 (community list, unverified) | Gecko. In mainline Dolphin's game list, off by default (checked). Not in the pinned DolphinXR build, which has no file for this game: add by hand |
| Resident Evil: The Darkside Chronicles | SBDE08, SBDP08, SBDJ08, SBDK08 | 30 (user reports) | Gecko, added by hand |
| The House of the Dead: Overkill | RHOE8P, RHOP8P, RHOJ8P | About 30, not locked (second-hand press quote, unverified) | Gecko. In mainline Dolphin's game list, off by default (checked). The pinned DolphinXR build's file has the game's settings but not the code: add by hand |
| Dead Space: Extraction | RZJE69, RZJP69, RZJD69, RZJJ13 | 30 on NTSC, 50 on PAL (Dolphin wiki) | Action Replay, one for NTSC-U and one for PAL, added by hand |
| Sin & Punishment: Star Successor | R2VE01, R2VP01, R2VJ01 | 60 (developer interview) | None found, none needed |

No source says which disc region or revision the Umbrella, Darkside and Overkill codes are for. Each is a single write to a fixed address, so treat it as good for one region (most likely North America) until tried on another. Dead Space: Extraction has separate addresses per region, which is the reason for the caution. No source says whether any code changes game speed, cutscene sync, audio pitch or quick-time-event timing: that is the main thing this brief has to find out.

## The target: the headset's own refresh rate

No source describes any of these games running above 60. What follows is our plan, built from Dolphin's documented settings and arithmetic. All of it is untested.

**Three layers, and each one has to reach the refresh rate.**

| Layer | What it is | How it gets to the refresh rate |
|---|---|---|
| 1. What we draw | Head tracking, the gun, the HUD panel's place in the room | Needs a pass that runs every refresh, drawn from the newest head and hand poses, whatever the game is doing. **The pinned base has no such pass** (see "Frame handoff"): between game frames it shows the last picture again and the headset's compositor turns it to the new head direction. Head turns are covered. Head movement, the hand and anything drawn into the game's picture are not |
| 2. The game's camera | The rail camera moving through the level | Either the game itself runs at the refresh rate (layer 3), or our renderer redraws the last game frame with the camera moved along its path between game frames. The second is new work inside the fork; the old Dolphin VR fork replayed frames with the head pose only, and the pinned base does not replay at all |
| 3. The game's world | Enemies, animation, effects, game logic | The game has to produce a frame every refresh: routes A and B below. Failing that, the headset runtime's motion smoothing invents the in-between frames |

**Route A: raise the console's video rate to the headset's rate.** The game makes at most one frame per video interrupt, and the console makes 60 of those a second (59.94 on NTSC), so 60 is the ceiling until that rate is raised. Dolphin's VBI Frequency Override does that, from 1% to 500%. Dolphin's own text (checked): it "also adjusts the emulated CPU's clock rate, to keep it relatively the same"; it "may affect gameplay speed, as it is often tied to the frame rate"; and changing it "can and will break games and cause glitches". The setting is `VIOverclockEnable` and `VIOverclock` (1.5 is 150%) under `[Core]`. Both exist in the pinned DolphinXR source (checked), so the override can be set per run on the command line or per game in a game settings file.

| Headset rate | VBI Frequency Override | Number 60.0 becomes | As a code value |
|---|---|---|---|
| 72 | 120% | 72.0 | `42900000` |
| 75 | 125% | 75.0 | `42960000` |
| 80 | 133% (79.7; the setting moves in steps of 1%) | 80.0 | `42A00000` |
| 90 | 150% | 90.0 | `42B40000` |
| 120 | 200% | 120.0 | `42F00000` |
| 144 | 240% | 144.0 | `43100000` |

The percentages assume a 60 Hz game. A PAL disc needs Dolphin's PAL60 setting on (`E60` under `IPL` in the Wii system settings; on by default, checked). If the game still runs at 50, the base is 50 and the value is the refresh rate divided by 50: 144% for 72, 160% for 80, 180% for 90, 240% for 120, 288% for 144.

**Route B: tell the game the new rate.** Two of the codes write the number 60.0 into the game (Overkill, Dead Space: Extraction). The same write with the value from the table is the obvious thing to try with route A, for example `055AB344 42B40000` for Overkill at 90 and `04603B44 42B40000` for Dead Space: Extraction (NTSC-U) at 90. **These variants are ours, derived by arithmetic, from no source, and untested.** Whether that number is a frame limit or the game's time step is not known; if it is the time step, the game keeps its speed at the new rate.

**What to expect, and what decides it.**

- A game that counts frames runs faster by the same factor as the override: 1.5 times at 90, twice at 120. A game that measures time keeps its speed. Which kind each game is, nobody has written down. The 60 fps code is the test: if 60 plays at normal speed, the game is scaling by time or by its frame interval, and route A has a chance.
- Umbrella Chronicles' code removes a wait and Darkside's sets a frame interval of 1, so both should follow the video rate straight up under route A. Speed is the open question.
- Sin & Punishment is already at 60 with no code. Route A alone is the only lever, and a 60 fps arcade-style game is the most likely of the five to speed up.
- The cost lands on the one CPU thread Dolphin depends on. Our estimate, not a measurement: a 30 fps game at 90 does three times the emulated work per second, at 120 four times, at 144 nearly five. Expect the CPU, not the GPU, to set the limit, and expect 144 to be out of reach before 90 is.
- The console's 59.94 is not 60. At 150% the game makes 89.91 frames for a 90 Hz headset, so one frame repeats about every 11 seconds unless the VR build paces the emulator from the headset's clock. Look for that hitch; it is there at 60 on a 120 Hz headset too (about every 8 seconds).
- Audio follows the video rate. Listen for pitch and crackle.

**If a game cannot hold the refresh rate,** fall back in this order, with layers 1 and 2 still at full rate:

1. Half the refresh rate by the same route. The override also goes below 100%: 45 on a 90 Hz headset is 75%, 36 on 72 is 60%, 40 on 80 is 67%, 72 on 144 is 120%; 60 on 120 needs no override. Same speed caveat.
2. The game's plain 30 or 60 on a refresh rate it divides (table further down).
3. The runtime's motion smoothing.

Write down which one each game ends on, per refresh rate.

## Per game: what to try, in order

The Umbrella and Overkill codes below were checked character for character against Dolphin's raw game files on 2026-10-10. The Darkside code and the Dead Space NTSC-U code each appear on the wiki and in the forum thread, and the Dead Space PAL code on the wiki only; all three were read through a summarising tool and both sites refused a direct read, so copy those from their source page.

**Use the disc on hand and record its id and revision.** A single-address code is for one build of the game; on another region it may crash or do nothing. Then either find the same value in that build with Dolphin's memory search (the Dead Space author describes doing exactly that, and it is in scope here) or go by route A alone, which needs no code.

For every step, record the numbers in "How to measure".

### Common steps

0. **Cheats on.** Dolphin ignores Gecko and Action Replay codes unless Enable Cheats is on (`EnableCheats` under `[Core]`, off by default, checked). A code is switched on per game under `[Gecko_Enabled]` in the game's settings file in the user directory.
1. **Baseline.** No code, emulated CPU clock at 100%, the class preset. Play the test stretch (below).
2. **The code.** Turn it on, restart the game, play the same stretch.
3. **Only if the game's own frame counter drops under load:** raise Emulated CPU Clock Override in steps of 25%. A Dolphin developer suggests raising it with 60 fps codes, "not always as much as 200%". Dolphin warns this setting "can and will break games".
4. **Above 60, once the code holds 60 at normal game speed:** set the headset to 90 and VBI Frequency Override to 150% with the code still on (route A). For Overkill and Dead Space: Extraction also try the code with the 90.0 value (route B). Then repeat for each other refresh rate the headset has, from the table above.
5. **If there is no code, or the code fails:** VBI Frequency Override on its own, at 200% for a 30 fps game to reach 60, and watch game speed.
6. Leave VBI Skip off and the speed limit at 100%.

### Common watch list

- Crash or hang at boot or at the first mission (the sign of a wrong-region code).
- Game speed: time the test stretch with the code off and on.
- Cutscenes and pre-rendered video: speed, audio sync, a frame-rate drop that starts at a cutscene and does not recover (two user reports describe exactly this).
- Audio crackle or pitch.
- Quick-time events: are the windows shorter?
- Shake to reload and swing to slash: do the gestures still register?
- Pointer feel: lag or overshoot against the baseline.
- The HUD: every element still present, and still on its panel in 3D mode.
- Bloom and other screen effects: the same in both eyes.

### Resident Evil: The Umbrella Chronicles

Code (Gecko, by Nick Reynolds; untested). Sources: https://wiki.dolphin-emu.org/index.php?title=Resident_Evil:_The_Umbrella_Chronicles and Dolphin's own `Data/Sys/GameSettings/RBU.ini`.

```
$60FPS [Nick Reynolds]
04158204 60000000
```

- Mainline Dolphin lists it for this game. The pinned DolphinXR build has no settings file for this game, so add the code by hand there.
- The Dolphin wiki says only "A few minor problems with this one." Find out what they are.
- One unlocated forum report says it crashed for a user and is confirmed only on the North American release. Try other regions last.
- The value is a "no operation" instruction: the code removes a wait instead of setting a rate, so the result may be "as fast as possible", not a locked 60. Watch the game's counter for values above 60 and for a changed game speed.

### Resident Evil: The Darkside Chronicles

Code (Gecko, by solitarius; untested). Sources: https://wiki.dolphin-emu.org/index.php/Resident_Evil:_The_Darkside_Chronicles and https://forums.dolphin-emu.org/Thread-game-modification-60-fps-hacks-and-patches?pid=510240

```
$60FPS [solitarius]
04736D44 00000001
```

- Not in Dolphin's shipped list; add it by hand. The wiki lists it for SBDE08 and SBDP08, the forum for SBDE08 only.
- The wiki says 60 is hard to hold in all areas; expect dips at chapter starts, which slow down even without the code.
- Dolphin's shipped settings for this game turn off "Store EFB Copies to Texture Only". Bloom doubles above native resolution: use a graphics mod for native-size bloom, or turn off Scaled EFB Copy. Dialogue repeats unless DSP LLE is used.
- This is the shaky-camera game. Judge 60 with the stabiliser both off and on.

### The House of the Dead: Overkill

Code (Gecko, by Nick Reynolds; untested). Sources: https://wiki.dolphin-emu.org/index.php?title=The_House_of_the_Dead:_Overkill and Dolphin's own `Data/Sys/GameSettings/RHO.ini`.

```
$60FPS [Nick Reynolds]
055AB344 42700000
```

- Mainline Dolphin lists it for this game. The pinned DolphinXR build's file for this game has the settings below but not the code, so add it by hand there.
- The wiki warns it "may cause elements of the UI to disappear". For us that is the HUD panel: list exactly which elements go and when.
- Dolphin ships this game with dual core off, so it leans hardest on one CPU thread. Leave that as shipped.
- The wiki tags the game as supporting VBI Frequency Override, with no values. That makes this the best first candidate for the headset's refresh rate: try step 4 here early, and step 5 if the code loses the HUD.
- Turn on Speed Up Disc Transfer Rate for stutter at area loads. Bloom needs the same care as Darkside.

### Dead Space: Extraction

Codes (Action Replay; untested). The NTSC-U code is by maratnugmanov; the PAL code is on the Dolphin wiki only. Sources: https://wiki.dolphin-emu.org/index.php?title=Dead_Space:_Extraction and https://forums.dolphin-emu.org/Thread-game-modification-60-fps-hacks-and-patches?pid=534681

```
NTSC-U (RZJE69)
$60fps
04603B44 42700000

PAL (RZJP69)
$60fps
04603CE4 42700000
```

- Try NTSC-U first. One user says it "works perfectly"; another reports a drop to about 30 with stuttering audio from the first cutscene on, cured by removing the code.
- The wiki says a higher emulated CPU clock "may be required" for a stable 60 and gives no value.
- The wiki's settings: Store EFB Copies to Texture Only off, Defer EFB Copies to RAM off, Texture Cache on Safe.
- The PAL disc runs at 50 with no code. 50 pairs with no headset refresh rate, so measure it once for comparison and do not ship it as the route.

### Sin & Punishment: Star Successor

- No code. Confirm the game's counter reads 60 in play.
- Above 60 there is only VBI Frequency Override. Time the test stretch at 150%: if the game runs half as fast again, this game stays at 60 and is shown on a 120 Hz setting, or at 60 with our camera fill-in on 90.
- If the game goes into slow motion at screen transitions, turn on Skip Presenting Duplicate Frames (one 2020 user report says this cures it) and note what it does to pacing in the headset.
- Dolphin ships this game with "Store EFB Copies to Texture Only" off. Bloom needs the same care as Darkside.

## Frame handoff: read from the source, to be confirmed in the headset

How does the VR build hand frames to the headset when the game runs at 30? This is read from the pinned DolphinXR source (the pacing thread in `OpenXRManager.cpp`; setting `EagerHeartbeat` under `[VR]` in the graphics settings). Nobody here has seen it run.

| Mode | What it does | What the player gets |
|---|---|---|
| **Paced to the game** (`EagerHeartbeat` off, the default on PC) | Starts a headset frame only when the game has published one, so the runtime sees the game's true rate | The runtime's motion smoothing (SteamVR, Virtual Desktop, Meta Link) fills the gaps, with its artefacts. With smoothing off, the game's rate shows as judder on head turns |
| **Every refresh** (`EagerHeartbeat` on, the default on standalone headsets) | Hands over the last game picture again each refresh; the headset's compositor turns it to the current head direction | Smooth head turns. Nothing else moves between game frames: not the world, not head position, not anything drawn into the picture |

Neither mode redraws the scene between game frames; the source says the second replaces the old fork's frame replay. So in this base **the gun, the hand and the HUD panel move at the game's rate if they are drawn into the game's picture.** A gun at 30 behind a hand at 90 fails the target ("does not lag the hand"). That makes this lane's answer the lead's input: per game, either the game reaches the refresh rate, or the gun and panel need a pass of their own every refresh. Report which as soon as phase 1 shows whether the game can get there.

To confirm in the headset with a 30 fps game: the runtime's frame timing should show an application rate of 30 in the first mode and the refresh rate in the second. Try both, with motion smoothing on and off, and write down which looks best per headset class.

## When the game runs below the refresh rate

The fallback pairings. Arithmetic: a pairing is clean when the refresh rate is a whole multiple of the game's rate.

| Game rate | Clean headset rates |
|---|---|
| 30 | 60, 90, 120 |
| 60 | 60, 120 |
| 50 (PAL) | none of 60, 72, 75, 80, 90, 120, 144 |

72, 80 and 144 pair with neither 30 nor 60. On a headset that stops at 90, compare 30 at 90 against 60 at 90 by eye; the first may look steadier.

## Starting presets

All values are starting values, to be measured, unless a source is named. Internal-resolution multiples are of Dolphin's native 640x528 frame: 3x is 1920x1584, 4x is 2560x2112, 5x is 3200x2640, 6x is 3840x3168. If the VR build sizes its eye buffers from the runtime and not from this setting, use its resolution scale and read the row as a target height.

| | Streamed, older (Quest 2, Quest Pro, Pico 4) | Streamed, current (Quest 3, Quest 3S, Pico 4 Ultra, Steam Frame) | Wired, mid (Valve Index, PSVR2 with the PC adapter) | Wired, high (Bigscreen Beyond, Pimax Crystal) |
|---|---|---|---|---|
| Per-eye panel (maker or Wikipedia) | 1832x1920, 1800x1920, 2160x2160 | 2064x2208, 1832x1920, 2160x2160 (unverified), 2160x2160 | 1440x1600, 2000x2040 | 2560x2560, up to 3840x3840 |
| Fallback refresh if the game stays at 30 | 90 | 90 or 120 | 90 or 120 | 90 |
| Fallback refresh if the game stays at 60 | 120 on Quest 2; the others stop at 90 | 120; Pico 4 Ultra stops at 90 (unverified) | 120 | No clean rate; test 90 |
| Frame handoff (`EagerHeartbeat`) | Off, the base's PC default; compare with on | Same | Same | Same |
| Runtime render size per eye | 1440x1584 (Meta's documented 100% for Quest 2 and Pro) | 1680x1760 (Meta's documented 100% for Quest 3 and 3S); others: read from the runtime | Read from the runtime | Read from the runtime |
| Dolphin internal resolution | 3x | 3x, then 4x | 3x, then 4x | 4x, then 5x or 6x |
| Anti-aliasing | None | None, then 2x MSAA | 2x, then 4x MSAA | None at 5x or 6x; 2x MSAA at 4x |
| SSAA | Off | Off | Off | Off |
| Anisotropic filtering | 16x | 16x | 16x | 16x |
| Shader compilation | Hybrid Ubershaders | Hybrid Ubershaders | Hybrid Ubershaders | Hybrid Ubershaders |
| Post-processing, widescreen hack | Off | Off | Off | Off |
| EFB and XFB settings | As shipped per game, plus the per-game notes above | Same | Same | Same |
| Emulated CPU clock | 100% | 100% | 100% | 100% |
| Stream codec | HEVC | HEVC; AV1 on Quest 3 with a recent GPU (ALVR's documented limit) | n/a | n/a |
| Stream bitrate | ALVR's documented ranges: HEVC 100 to 150 Mbps on Wi-Fi; H.264 400 to 500 Mbps on Wi-Fi, 800 to 1000 Mbps on a cable | Same | n/a | n/a |
| Foveated encoding | On; ALVR's rule: reduce it while the streamer holds the refresh rate, increase it when it does not | Same | n/a | n/a |

What Dolphin itself says these cost: internal resolution "greatly increases GPU load"; SSAA is "much more demanding than MSAA"; anisotropic filtering "does not usually impact performance"; Hybrid Ubershaders have a "small performance impact" and avoid compile stutter; Exclusive Ubershaders are "incredibly demanding at higher resolutions". Dolphin's tooltip says its stereo mode "heavily decreases emulation speed" while its 2015 write-up calls the cost minimal: measure the second eye, do not assume.

Not documented in anything read: Virtual Desktop's and Steam Link's own settings pages, SteamVR's 100% size per headset, and Quest Link's default per GPU. Use the streamer's defaults there and record what they are.

## How to measure

Two phases. Change one thing at a time.

**Phase 1: on a monitor, no headset.** The pinned build with VR off, in its own user directory, V-Sync off. Runs 1 to 3 below for every game: frame rate, game speed, what breaks, and how full the busiest core is at each override value. Dolphin's video rate should not depend on the monitor's refresh rate with V-Sync off, so 90, 120 and 144 can be tried on any screen; confirm that on the first run. This phase classes each game (counts frames or measures time) and needs no sitting.

**Phase 2: in the headset,** only for the settings that survived phase 1: the two handoff modes, pacing, the by-eye checks and runs 4 to 7. These go into the lead's combined checklist.

**Test stretch.** Pick one checkpoint-to-checkpoint rail section of one to two minutes with a busy fight, plus one cutscene. Use the same two for every run of that game. For phase 1 prefer a stretch that plays without input (the attract demo, an opening cutscene, a rail section up to its first forced input), so no player is needed; a played stretch needs the owner. For runs nobody watches, Dolphin can write frame times to a file (`LogRenderTimeToFile` under `[Settings]` in the graphics settings, checked).

**Record for each run**

| What | Where to read it | Why |
|---|---|---|
| Game frames per second | Dolphin's on-screen FPS counter | The number a code should move from 30 to 60, and route A from 60 to the refresh rate |
| Video interrupts per second and speed percent | Dolphin's VPS and speed counters | 60 and 100% without the override; with it, VPS should read the headset's rate. Check on the baseline run that FPS and VPS mean this; the reading is unverified |
| Game speed | Stopwatch on the test stretch and on the cutscene, code off against code on | Equal times mean the code changed smoothness only |
| Frame time, CPU and GPU, in milliseconds | The VR runtime's frame timing view, or the streamer's performance overlay | Against the budget: 11.1 ms at 90 Hz, 8.3 ms at 120 Hz, 13.9 ms at 72 Hz |
| Frames the runtime filled in | The same view: reprojected or synthesised frame count | Shows which of the two cases above we are in |
| CPU load | The busiest single core, not the average | Dolphin's FAQ: speed depends on single-thread performance and extra cores do not help |
| GPU load | GPU utilisation; on a streamed headset also the video encoder's load | Separates "the game is slow" from "the second eye is expensive" |
| Per-eye resolution | The eye buffer size the runtime reports for the session, and Dolphin's internal resolution | The two may not be the same thing |
| Stream | Codec, bitrate, foveation setting, and the streamer's reported latency | Streamed class only |
| By eye | Judder on head turns, judder of the moving world, the HUD panel, bloom in each eye, pointer feel | The acceptance list in the target |

**Runs per game**

1. Baseline preset, no code.
2. Code on (60).
3. Code on, VBI Frequency Override at the headset's rate, once per refresh rate the headset offers, 90 first. Stop raising the rate when the busiest core is full or the game changes speed.
4. At the highest rate that held: internal resolution one step down, then one step up.
5. Same rate, anti-aliasing one step up.
6. Same rate, motion smoothing or spacewarp off, then on.
7. On a streamed headset: bitrate at the low and high end of the range, foveated encoding off and on.

In the higher-rate runs also watch for a single repeated frame every 8 to 11 seconds (the 59.94 against 60 mismatch).

**Pitfalls**

- Dolphin's FAQ: opening the Graphics settings window while a game runs replaces the game's own shipped settings with the global ones, and the game speeds up and glitches. Set everything before starting and do not open it during a run.
- A code that seems to do nothing: check Enable Cheats first (common step 0).
- Confirm in the log or the game's properties that the per-game settings and the code are in effect.
- Use a hardware video encoder for streaming; a software encoder competes with Dolphin for CPU.
- Let shaders compile: discard the first pass through the test stretch.

## Done when

- Phase 1 is reported for all five games before any headset time is asked for.
- The frame handoff read from the source is confirmed or corrected from a headset run, and the better mode is named per headset class.
- The lead has, per game, the answer to "does the gun need its own per-refresh pass?".
- For each of the five games there is a results row: region and revision of the disc tried, code used, frames per second with the code off and on, game speed changed or not, and every item on the watch list marked fine, broken (with what and where) or not reached.
- For each game and each refresh rate the test headset offers (90 first), one line: the game reaches it at normal speed (with which override and code value), reaches it but runs fast, or cannot hold it (and what gives out: the CPU thread, the GPU, or the game). Each game is classed as counting frames or measuring time.
- Each code is marked "works on (region)", "breaks (what)" or "crashes", with the Dolphin build and the date.
- The three open code questions are answered: what Umbrella's "minor problems" are, which HUD elements Overkill loses, and whether the cutscene drop reported for Dead Space and the Chronicles games happens to us.
- The preset table has measured values in place of starting values for the headset class tested, with the frame times behind them, and the classes not tested are still marked as starting values.
- `data/performance/` lists, per game, its code with source and tested-on date, or says "no code; runs at (rate)".
- A recommendation per game and per refresh rate: the game at the headset's rate, or the highest clean rate below it with our own drawing at full rate, and what fills the gap.
- A yes or no on whether camera fill-in inside the renderer (layer 2) is needed, with the games that need it.

## Limits

- The user supplies the discs. Nothing from a disc, no Nintendo system file and no emulator build enters the repository or a report. Ask before each third-party download and before reading any disc.
- Codes are for our own testing. A code goes into a game's profile only with its source and a tested-on date.
- Results name the headset class and the Dolphin build, not the test machine. Keep screenshots and logs out of the repository.
- Screenshots and frame dumps are game content: keep them under `.local/` and delete frame dumps once measured.
- No frame rate or frame time is written down unless it was read off a counter in a run.
