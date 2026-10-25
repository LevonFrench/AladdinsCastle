# Offline round-2 route evidence

The round-2 text scripts record progression beyond setup screens and independently restored input-driven scenes for all20 titles. E2 means the named offline scene only. E1 text-format checks do not prove a route, and no E3/headset acceptance is claimed. Game data, builds, saves, states and game-screen captures are private and are not distributed.

Use the existing boot/play scripts and each continuation header as a chain. A continuation that says restore requires the locally created matching private state and copied game-save/configuration seed. Times are absolute emulated seconds from the tested state, not wall time. These are reviewed replays from one local setup; cold boot timing and cross-build state compatibility are not guaranteed. Independently restoring a state proves that scene, not a portable boot route.

Pass the explicit per-title profile documented in the control catalogue. The runner pins copied per-game first-remote selectors after the view INI is written; otherwise a title can silently select another profile. The optional preload preserves the copied input source. Requested settings, file values and probe packets are distinct evidence; none alone establishes effective gameplay response.

All pointer extremes are qualitative observations, without numeric calibration. The five gun-title replays show early fire/weapon feedback and four visible extremes; Dead Space is guided weapon control, and Gunblade does not establish scored kills. Reduced-health encounters cannot be used as continuous60-second combat windows.

| Title | Private checkpoint name | Text prefix in `input-probe/` | Reviewed scope and limits |
|---|---|---|---|
| Mario Kart Wii | `RMCP01-race-105.sav` | `mkw-round2-` | Luigi Circuit timer and steering; kart later against a wall. |
| Metroid Prime Trilogy | `R3ME01-tutorial-245.sav` | `metroid-round2-` | Tutorial movement, firing flash/impact and jump; native clipping remains. |
| New Super Mario Bros. Wii | `SMNE01-level1-1-114.sav` | `nsmbw-round2-` | 1-1 movement; later death. Jump and sustained survival unproved. |
| Crossbow Training | `RZPP01-target-practice-145.sav` | `crossbow-round2-` | Mono target practice and score gains; numeric aim calibration unproved. |
| Sin and Punishment | `R2VE01-tutorial-control-92.sav` | `sin-and-punishment-round2-` | Tutorial movement/weapon effects; sustained enemy combat unproved. |
| Super Mario Galaxy | `RMGP01-opening-plaza-531.sav` | `galaxy-round2-` | Opening plaza movement, jump and star-bit gain; later areas untested. |
| Super Mario Galaxy 2 | `SB4P01-interactive-book-212.sav` | `galaxy2-round2-` | Interactive storybook movement/jump; later 3D and central duplicate unresolved. |
| Donkey Kong Country Returns | `SF8P01-level1-1-90.sav` | `dkcr-round2-` | Opening 1-1 movement/shake; unexplained close-up and later death. |
| Kirby's Return to Dream Land | `SUKE01-stage1-1-815.sav` | `kirby-rtdl-round2-` | Mono Stage1-1 movement/jump; original HUD intact. Gear relocation rejected. |
| Mario Strikers Charged | `R4QE01-match-205.sav` | `strikers-round2-` | Active match movement/clock; pass and shot response unisolated. |
| Excitebike World Rally | `Excitebike-Kyoto-race-576.sav` | `excitebike-round2-` | Kyoto race throttle/lanes, clock and heat. No race-completion/B-grade acceptance. |
| House of the Dead Overkill | `RHOP8P-first-combat-495.sav` | `overkill-round2-` | Early combat shot/score gain and four pointer extremes; later death. |
| Dead Space Extraction | `RZJE69-armed-intro-1123.sav` | `dse-round2-` | Armed guided introduction, rivet fire/ammo and four extremes. Enemy combat unproved. |
| Darkside Chronicles | `SBDP08-first-combat-717.sav` | `darkside-round2-` | Early enemy impact/fall and four extremes. Infinite tutorial ammo; reduced health. |
| Punch-Out | `R7PE01-sideways-round1-1166.sav` | `punch-round2-` | Sideways Wii1/2 punch/stamina/clock. Natural motion, clean dodge and victory unproved. |
| Umbrella Chronicles | `RBUP08-train-combat-495.sav` | `umbrella-round2-` | Early train fire effects/enemy reaction and four live extremes. Reload/kill unproved; dies quickly. |
| Gunblade NY and LA Machineguns | `SQDE8P-area1-active-281.sav` | `gunblade-round2-` | Area1 aiming-marker extremes, firing effects and counter. No scored-kill claim; health falls. |
| Wii Sports Resort | `RZTP01-Duel-ready-855p5.sav` | `resort-round2-` | Actual Duel sword-orientation response to held gyro. Hit/block success and win unproved. |
| Zelda Twilight Princess | `RZDP01-Ordon-controlled-803.sav` | `zelda-round2-` | Ordon field Nunchuk movement. Persistent named save and later combat unproved. |
| Kirby's Epic Yarn | `RK5E01-PatchCastle-play-773.sav` | `yarn-round2-` | Patch Castle platform movement/jump. Bead counter remains0000; later stage unproved. |

The Resort and Zelda `partial` scripts preserve earlier observations; the later Duel/Ordon replays supersede those limited route conclusions. Yarn state729 is a loading screen; state773 is the actual platform scene. Unsupported early axis spellings and Punch dodge attempts are excluded from accepted movement proof.

Most later comparison restores use mono theatre at1x/MSAA1/single-core for inspection. They do not repair stereo, prove3x pacing or accept physical controls. Original native-view clipping remains visible in several games. The rejected Kirby gear move remains out; optional mono fragments preserve the original HUD. Crossbow retains its existing mono fallback. Metroid offers a diagnostic single-core fragment; matched source windows do not demonstrate a blanket speed gain. The experimental XFB patch remains default-off.

For timing use the scorecard runner and retain before/end gameplay captures. Source eligibility is not distinct-eye disparity, game-content FPS, GPU completion or delivered headset frames. Treat menus, films, maps, loading and death screens as separate scopes. No live Hub, launcher or owner Dolphin file was changed by this round.
