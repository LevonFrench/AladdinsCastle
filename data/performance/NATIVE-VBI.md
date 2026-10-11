# Native Forced VBI Frequency, R3

The current run plan is native-vbi-phase1.json. Start with RBUP08, then SBDP08;
the later ISO queue includes RHOP8P and RZJE69, followed by RZPP01 and SQDE8P.
R2VE01 profile preparation waits for
the Hub's WBFS identification contract. Nothing is measured yet. Legacy PAL
candidate-code runs are parked; this experiment set never enables cheats.
The separate Dead Space candidate uses an exact RZJE69 revision 0 guard and
ActionReplay_Disabled. `prepare-deadspace-candidate` creates a code-off preview,
with VR off and cheats false. Its direct source fetch is unavailable; it cannot
run through the collector. Source review and a measured same-scene baseline are
still required before enabling it after the native comparison. No PAL or
alternative-revision address is guessed.

The exact source setting is `[VR] ForcedVBIFrequency` in GFX.ini, with the CLI
name `Graphics.VR.ForcedVBIFrequency`. Dolphin's configuration-system name is
**Graphics**; the parser silently ignores the unknown name GFX. The earlier
lane used GFX in its CLI overrides; those arguments are corrected here, and
old private profiles should be preserved as history and replaced with fresh runs.

| Choice | Stored integer | Source behavior |
| --- | --- | --- |
| Auto | -1 | Uses the startup XR display refresh value, selecting the nearest of 72, 90 and 120. A tie keeps the lower value. No positive startup value resolves to Off. |
| Off | 0 | Leaves native forcing off when AutoVBIFromHMD is false. The legacy boolean can otherwise resolve or migrate Off to 90. |
| 72, 90, 120 | 72, 90, 120 | Replaces the Advanced VBI percentage while EnableOpenXR is true. |

In pinned source 85c4266, both CoreTiming and VideoInterface compute the native
factor from the chosen frequency divided by 59.94. VideoInterface divides its
nominal half-line ticks by that factor; CoreTiming multiplies the emulated CPU
work factor by it. This is not a game-specific unlock and does not prove normal
game speed, unique game frames or the selected output rate. In particular, a PAL
disc still needs its actual video mode checked. PAL60 is requested, not presumed.
With EnableOpenXR false, every native mode is inert; a monitor run with VR off
cannot validate the switch. Do not substitute a Core.VIOverclock percentage and
call it a native result.

Auto's startup value is initially zero. The pinned manager first tries
XR_FB_display_refresh_rate after creating a session, then falls back to the first
successful xrWaitFrame predictedDisplayPeriod. Recording a positive value invokes
the configuration refresh callbacks. The session-start log and actual counters
must establish the effective mode. A selected value in an INI is insufficient.
No simulator installation, system runtime change or headset acceptance follows
from this source finding; runtime conditions remain the lead's decision.

Prepare with phase1.py's `prepare` command and an experiment from `native-off`,
`native-auto`, `native-72`, `native-90`, `native-120`. Each gets a fresh private
User folder, counters/log directories, unchanged inherited game settings and
the same initial graphics settings. Cheats, manual VI/CPU overclock and V-Sync
are off; EagerHeartbeat is held off and MirrorView is held at BothEyes (0).
Each profile initially refuses runtime because scripted input is not attached.

Use `attach-input --run-directory <run> --profile <generated-split-right.ini>
--manifest <generated-files.json> --script <private-timeline.json> --entrypoint
<lead-scripted.py> --private-root <root>`. The attach step verifies the shared
shell-stick/1 manifest, Source 1, the named DSU slot and exact A/B/Plus mappings,
then calls the lead parser with --check only. It copies exact profile bytes and
a checked timeline, creates the loopback DSU configuration, and hashes both.
It never starts a server or sends a button. A valid timeline is a syntax result,
not evidence that the game reached the selected scene. The same script/profile
hashes must be used for the whole comparison.

The lead starts its DSU server and owns its shutdown. Native collection needs an
exact private lease with quiet_machine, runtime_slot and xr_session_authorized
true, granted_by="lead", run_directory, expires_utc, dsu_server_pid and
dsu_ready_file (the server's stdout log, whose first JSON line reports readiness
and port). This records actual scheduling and XR permission; creating it cannot
grant either. No Computer Use, mouse or keyboard injection occurs. The lead
supervises target capture and system-prompt detection, and can request immediate
stop by creating stop-requested.txt in the run directory. Any system prompt ends
the item; no prompt interaction or workaround.

Run Off first. Discard shader warmup and identify actual scene start/end cues.
Auto and fixed-rate runs require a reviewed Off observations/execution pair with
the same game/revision/build and script/profile hashes, counter confirmation,
normal game speed and CPU headroom. Compare one 60-second measured stretch in
attempts bounded to 180 seconds; the collection duration alone is not a scene
duration. Stop immediately for altered game speed, pitch, saturation, missing
counters, crash/hang or an unreached scene. One diagnosis and one retry, then
record and move on. No automatic climb beyond 120 is included.

Required private results: startup XR refresh and resolved Auto choice; actual
FPS/VPS/speed readings with evidence; same-cue scene/cutscene timings; logical-core
and available GPU-engine telemetry; audio/watch-list results; exact other running
work; and explicit unreached tests. Presentation intervals include duplicates.
Neither those logs nor a DSU completion receipt proves unique game frames,
normal speed or headset behavior. No measured preset or per-refresh gun/camera
recommendation is available yet.

Source anchors (all pinned to 85c4266):

- [Config.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Common/Config/Config.cpp#L156) and [CommandLineParse.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/UICommon/CommandLineParse.cpp#L49): accepted CLI namespaces and ignored unknown names.
- [GraphicsSettings.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Config/GraphicsSettings.cpp#L238) and [GraphicsSettings.h](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Config/GraphicsSettings.h#L276): constants, normalization and Auto selection.
- [CoreTiming.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/CoreTiming.cpp#L44) and [VideoInterface.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/HW/VideoInterface.cpp#L62): gating, precedence and factors.
- [ConfigManager.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/ConfigManager.cpp#L348): legacy migration.
- [OpenXRManager.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/VR/OpenXRManager.cpp#L1423): startup capture and config refresh; WaitFrame later supplies the fallback period.
