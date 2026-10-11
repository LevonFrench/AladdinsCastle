# Chronicles monitor experiments

Phase 1 preparation for RBUP08, then SBDP08. No rates or preset performance are
measured yet. The three experiments are baseline, candidate 60 code, then the
same code with a 1.5 VBI override. 90 is a requested target, not a measured rate.
These are private experiment profiles; nothing changes shipped profiles.
Read-only header inspection found PAL revision 0 for both discs; this establishes
disc identity only. Neither candidate has been run on these revisions.

Use `python data/performance/phase1.py prepare --executable <pinned-release-exe>
--disc <exact-ISO> --game-id <id> --experiment <experiment> --output <new-run>
--private-root <private-framerate-root>` from the checkout. Preparation reads only
the exact executable and 32 bytes of the named disc header, pins revision and
hashes, and writes a fresh isolated User folder and private run.json. It never
starts Dolphin. All real profiles and receipts go into the lead-designated
framerate directory under .local; no library enumeration or existing NAND/config
copy. Baseline disables cheats. Code profiles contain one candidate Gecko entry.
Shipped game settings remain inherited, including Darkside's EFB-to-RAM setting.

The lead must grant both QUIET MACHINE and DESKTOP for an exact run before
`collect-phase1.ps1` is invoked. The private lease JSON has `quiet_machine: true`,
`desktop: true`, `granted_by: "lead"`, `run_directory` (exact absolute
directory) and `expires_utc` (ISO timestamp). Creating a lease is not permission;
it must transcribe the lead's actual grant. Keep the session identity and exact
grant provenance in the private handoff; the public harness needs no chat ID.
The script refuses an expired lease,
another Dolphin/build/render process, changed profile/executable/disc, or a reused
run. Known build processes include MSVC, GCC compiler/worker processes, make,
Clang variants and their linkers, plus versioned and target-prefixed compiler
names. This guard supplements the lead's actual quiet-machine scheduling.
It launches VR off, V-Sync off, emulated CPU clock 100%, speed limit 100%,
PAL60 on, 3x resolution, no AA, 16x AF and hybrid ubershaders. Those graphics values
are unmeasured starting values from the brief.

The bounded collector records logical-core utilization and available per-process
GPU engine counters. It records other process names/CPU totals privately. CPU
counters missing means stop; GPU counters missing means an explicit limitation.
An override stops after three samples with a logical core at least 95% busy.
That conservative safety threshold does not prove the emulation thread is full.
The script closes only its own process and records cleanup. An early exit is not
success. One diagnosis and one fresh-profile retry are allowed after a failed
live attempt, then park the experiment.

Use an unassisted attract sequence or opening cutscene. First establish exact
start/end cues and discard a shader warmup pass. Record private observations.json
with game_id, revision, experiment, stretch (cues), counter_evidence (private
captures or exact counter transcript), counter_confirmed, normal_speed_verified,
shader_warmup_discarded, busiest_core_full, baseline_duration_s, duration_s,
FPS/VPS/speed readings and each watch-list result. Booleans remain unknown until
observed. Both duration fields need real stopwatch observations of identical
cues; equal configured collection durations do not establish normal game speed.
Code requires a reviewed baseline observations/execution pair; 90 requires the
reviewed code pair, matching disc revision/build and CPU headroom. Supply it as
`-PredecessorRun`. The collector carries forward the required scene cues.
Observe code/override continuously; interrupt immediately for changed game speed,
audio pitch, crash or hang. No autonomous climb to other refresh rates is included.
If the sequence stops for input, report the unreached stretch instead of timing a
menu. Do not open Graphics settings while emulating.

Dolphin writes User/Logs/render_times.txt and vblank_times.txt. Use `phase1.py
summarize <log> --start-seconds <measured-offset> --duration-seconds <measured-length>
--kind presentation|vblank --output <new-private-json> --private-root <root>`.
These are interval distributions. Presentation events include duplicates: neither
the FPS overlay nor this log alone establishes unique animated game frames.
VBlank intervals and speed counters must be checked against the baseline first.
These logs are not CPU/GPU execution times or headset compositor measurements.
Keep counter captures private; no frame dumping is enabled. If a separate
approved diagnostic creates frame dumps, delete those dumps after measurement.

Source inspection uses DolphinXR commit 85c4266:

- [PerformanceMetrics.h](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/PerformanceMetrics.h#L53) names both log files;
  [PerformanceTracker.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/PerformanceTracker.cpp#L186) writes intervals in milliseconds.
- [Core.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Core.cpp#L917) counts host presentation callbacks before rejecting the duplicate reason for frame stepping.
  [VideoInterface.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/HW/VideoInterface.cpp#L942) counts VBlank events.
- [GraphicsSettings.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Config/GraphicsSettings.cpp#L40),
  [MainSettings.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Config/MainSettings.cpp#L236) and
  [SYSCONFSettings.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/Config/SYSCONFSettings.cpp#L18) define the counters, overrides and PAL60 flag.
- [GeckoCodeConfig.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/GeckoCodeConfig.cpp#L125) loads code definitions and enabled names;
  [SBD.ini](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Data/Sys/GameSettings/SBD.ini) preserves Darkside's shipped EFB settings.

Umbrella's candidate matches Dolphin's upstream RBU.ini. Darkside's candidate is
copied from the task brief: the direct source fetch returned a challenge page.
Neither code has been tried on either PAL revision. No headset-class preset,
per-refresh gun/camera recommendation, code compatibility or game-speed class
can be promoted until measurements exist. Phase 2 and the other three titles
remain outside this overnight lane.

Synthetic verification: `python -m unittest discover -s data/performance -p test_phase1.py`.
Launcher refusal checks: `pwsh -NoProfile -File data/performance/test_collector_gates.ps1`.
