# R4: one blocker, the mission picture

Status: 2026-10-11, outside review of `next` at 1f53e44 after the owner's second sitting and soft stop. Read after [R3-new-directions.md](R3-new-directions.md). Where this page differs from an earlier brief, this page wins. It does not restart work; the owner does that in chat.

## Verdict

The stop is clean, the branch is published and its combined checks pass. The second sitting was real progress: buttons, facing, recentre and the save prompt work, and the owner reached the first mission.

**The first mission's picture is wrong, and that is now the top priority:** the world shows in a flat window with a hard edge, the scene splits into displaced bands, the stereo is wrong and shots land off target. Since the sitting, about 74,000 lines went into the branch. Almost none of it went at this problem: it got one source note and no experiment.

## What was checked

| Checked | Result |
|---|---|
| Published state | `next` at 1f53e44, all six combined checks pass, `main` untouched, pull request 9 still a draft |
| Catalog, gun assets, tool tests | Run on a scratch copy: 413 games, 0 errors; gun assets 0 errors; 88 tool tests, 25 bridge tests, 52 profile tests pass |
| The second sitting's record | Read: the scorecard, the frame sheet, the settings the run used, the aim note |
| The fake-headset attempts | Read: neither attempt tested the question (below) |
| Private details in public files | One finding (below) |

## The blocker, and where to look

**Likely cause. A hypothesis from DolphinXR's own source, not proven.** Its settings text describes this failure: games draw full-screen effects (bloom, motion blur and the like) "as 2D quads sampling a copy of the rendered frame", and if those land on the 2D virtual screen they "smear the whole-scene effect across the 2D screen". Umbrella Chronicles leans on such effects. A flat window with a hard edge, bands that do not line up, and a HUD that stays readable all fit that.

**DolphinXR's answer is a per-game profile, and it has the tools to make one.**

- It ships 73 profiles under `Sys/GameSettingsVR`, none for our games. A profile holds `[Graphics.VR]` settings plus overrides that tell the build, draw by draw or texture by texture, how to treat it: `screen`, `fullscreen`, `headlocked`, `screen_pane`, `controller_anchor`, `camera_anchor` or `skip`.
- It has hotkeys for hunting textures and shaders in a running game, which is how those overrides are found.
- Its Metroid Prime Trilogy profile pins the game's own arm cannon to the right controller with full rotation. That is "the gun in the world at the hand" working in this build today, for that game.
- The sitting ran with no game profile, the virtual screen on, automatic effect exclusion at its default (on) and **4x internal resolution**. This family of effects is known to misalign above native size (see the Darkside Chronicles notes in [W-wii-framerate-presets.md](W-wii-framerate-presets.md)).

**Do not write our own draw instrumentation first.** Use the build's own switches and hunting tools. Add instrumentation only for a question they cannot answer.

### Experiments, in this order

Each is one launch at the same mission scene and one capture of each eye. Change one thing at a time and keep a table.

1. **A save state at the start of the mission,** so every run begins there. Scripted pointer and button input can make it; otherwise the owner saves one in ten seconds.
2. **Native internal resolution (1x),** everything else as in the sitting. Do the bands go?
3. **Dolphin's own copy options** at 1x and 4x: scaled EFB copy off, EFB copies to texture only on and off.
4. **A shipped profile's settings as a first guess.** The one other profile for a game from the same publisher sets `AutoNativeEfbEffects`, `DontClearScreen`, `ExactScreenDepth`, `LockHeadPosePerFrame`, `UnitsPerMeter = 10` and `VirtualScreen`. Try that set unchanged.
5. **DolphinXR's switches, one at a time:** automatic effect exclusion, virtual screen, frame size from the XFB copy, don't clear screen, render-target detection, small viewports on screen, cinematic bar removal.
6. **Hunt the elements.** List what is drawn in that frame with the build's texture and shader hunting. Give the effect passes `fullscreen` or `skip`, the HUD `screen`, and write the result as this game's profile.
7. **Ten minutes of Metroid Prime Trilogy** with its shipped profile, as the reference for what a tuned game looks like in this build.

**Seeing the result without the owner.** The build's desktop mirror shows both eyes, but only while a VR session runs. Two ways to get one:

- SteamVR's null driver. **It has not been tested yet.** The first attempt failed inside its own launcher wrapper in half a second. The second was stopped by its own supervisor after four seconds, because SteamVR started its normal helper processes. Test it plainly: back up the one settings file, add the keys, start SteamVR, wait a full minute, read `vrserver.txt`. SteamVR starting `vrserver`, `vrmonitor` and `vrcompositor` is expected. Restore the file afterwards.
- The real headset left awake on the desk, if the owner sets that up.

### Aim, after the picture

DolphinXR aims by intersecting the controller's ray with its flat screen plane, then feeds the emulated sensor bar at Dolphin's default ranges. Nobody has matched those ranges to this game.

- **Measure it on the desktop:** send nine pointer positions through scripted input, capture where the game's reticle lands for each, and solve for the range and offset. Put the result in the game's control set so the generated bindings carry it. This is a measurement, not guesswork.
- Aiming into the world by depth comes after the picture is right.

## Rule changes

| Rule | Change |
|---|---|
| **One retry** | It applies only to attempts that use the owner's time, headset or desktop. Builds, tests, tools and offline experiments iterate like ordinary engineering, with no cap. Work parked under this rule in the last block is un-parked: the null driver, the native provider build, the adapter build, the packaging runner, the menu retry |
| **Scaffolding** | A check that takes one launch gets a script of a dozen lines, not a supervisor. If the safety wrapper is bigger than the experiment, drop the wrapper and keep the backup-and-restore |
| **Pinball launch** | Allowed. "Read-only library" in [P-pinball-tab.md](P-pinball-tab.md) means **the Hub** writes nothing there. Visual Pinball saving its own scores and settings in its own places is normal and wanted. The outside reviewer's wording caused this block |
| **Private run plans** | The list of which discs are tested, in which formats and in what order is the tester's library. It is in tracked files under `data/performance/` and a status report. Move it under `.local/` and make the tracked files game-neutral. The release will ship from a new repository (D62), so history need not be rewritten unless the owner asks |
| **Reports** | A stop needs one page. No hash manifests, no review of the review |

## Lanes: every session has real work

The mission picture is the top priority, not the only work. No session waits for it. (Corrected 2026-10-11 at the owner's request: the first version of this table idled most lanes.)

| Lane | Do |
|---|---|
| 3D mode | Owns the mission picture: the experiments above, the game's profile, then the same for Darkside Chronicles |
| Frame rate | Captures and the results table for those experiments, then the nine-point aim measurement. In parallel: boot every other disc on the list to its title screen on the desktop, and run the 60 fps code route on the one disc whose code matches its region. Timing numbers only on a quiet machine |
| Viewer | **Eyes without the owner.** Test the null driver plainly, then run it as a service the other sessions book, one at a time: the DolphinXR mirror for the mission experiments, an eye capture of the gun test room, a readback of the Hub's menu to prove it is upright, and the fallback viewer shown end to end |
| Lead | Directs and integrates. Gets the gun test room seen in a VR session. Reviews the game profile. Un-parks the native builds |
| Hub | The Pinball tab to the end: a private scan of a real collection, one table started on the desktop, the "now playing" card. Disc identification for WBFS. The product name behind one definition (D62). The private run plan out of tracked files |
| Guns | **Real guns per game (D54).** The current sheet uses library models as stand-ins. Design an original gun for each game in the spirit of its default weapon, with a realistic finish and the player colour as an accent only: a service pistol for the two Resident Evil games, a heavy revolver for Overkill, a tool-like gun for Dead Space: Extraction, a crossbow for Link's Crossbow Training, and one for Sin & Punishment. Then hands on each, reload choices and label points |
| Whoever finishes or is blocked | The arcade track (D61): Time Crisis on System 22, the real game drawn for both eyes on the desktop. Build what was parked; skip the validators and packaging work until there is a picture to validate |

## Done when

A capture of each eye at the mission scene shows the world filling the view with no window edge and no bands, the HUD on its panel, and a reticle that follows the controller across the screen. Then the owner looks at it.
