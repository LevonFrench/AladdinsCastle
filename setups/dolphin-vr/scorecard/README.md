# Private Wii W0 scorecard

`scorecard.py` is a standard-library JSON aggregator. It reads the launcher manifest and explicit receipt files, then writes only `SCORECARD.md` and `scorecard.json` in the supplied private `.local` output directory. It never opens referenced discs, configurations, captures or executables; launches a process; changes settings; or contacts a runtime/network service. Keep all manifests, receipts and generated outputs private. Do not commit them.

Check the caller-supplied `HEADSET-ON` guard before any execution, including these CPU-only commands. If it exists, write code only. All variables below are explicit caller-supplied absolute paths or identifiers; the examples contain no machine defaults. `$Out` must be in an ignored `.local` directory inside the intended checkout.

```powershell
$Repo = '<absolute repository checkout>'
$Manifest = '<absolute private games manifest>'
$Out = '<absolute private .local lane directory>'
$Marker = '<absolute owner HEADSET-ON marker>'
$Receipt = '<absolute private receipt JSON>'
$ToolDir = Join-Path $Repo 'setups/dolphin-vr/scorecard'
if (Test-Path -LiteralPath $Marker) { throw 'Headset in use; code edits only.' }
python -B "$ToolDir/scorecard.py" init --manifest $Manifest --out $Out
python -B "$ToolDir/scorecard.py" import --out $Out --receipt $Receipt
```

`init` refuses to overwrite either existing output. `import` accepts repeated `--receipt` arguments and rejects the whole batch before saving if any receipt is invalid. Exact duplicate receipt bytes are ignored per game/view. `render --out <absolute-private-directory>` regenerates Markdown from the JSON. It does not reread the baseline manifest. Existing runs remain separate by view, baseline/before/after phase and title/menu/gameplay stage; a later partial receipt never erases earlier evidence. To change the manifest baseline, create a new private output directory.

The manifest's game title/key, view configuration, paths, environment and optional game INI text form an E0 source baseline. A game ID inferred from an INI filename is explicitly unverified against the disc header. Prior status, control prose and view notes are stored separately as unverified historical claims. No pass, game acceptance, headset delivery or numeric measurement is inferred from these claims.

## Offline runner, metrics and ingest

Use only an already reviewed, hash-pinned private synthetic probe with metrics and scripted-input instrumentation. The metrics patch applies to a private **copy** of `Present.cpp`, never the shared source. Build/download authority is separate from these commands. The plan maps game keys to `seed_user`, `script`, `gameplay_start_s` and optional `capture_times_s`; every path refers to caller-selected private files. Source settings and save seeds are copied to a fresh run directory. Game storage and live settings remain read-only.

```powershell
$CopiedPresentCpp = '<absolute private copied Present.cpp>'
$ProbeExe = '<absolute reviewed synthetic probe executable>'
$ProbeHash = '<reviewed executable SHA-256>'
$Plan = '<absolute private replay plan JSON>'
$Game = '<manifest game key>'
$View = '<manifest view key>'
$Run = '<absolute run directory produced by runner>'
$WindowStart = '<reviewed measurement start in emulated seconds>'
if (Test-Path -LiteralPath $Marker) { throw 'Headset in use; code edits only.' }
python -B "$ToolDir/apply_metrics.py" $CopiedPresentCpp
python -B "$ToolDir/run_offline.py" --exe $ProbeExe --exe-sha256 $ProbeHash --manifest $Manifest --plan $Plan --out $Out --game $Game --view $View --marker $Marker --rate native --phase before --capture-policy before-window --start $WindowStart --tag before-window --seconds 600
python -B "$ToolDir/analyze_metrics.py" "$Run/frames.csv" --start-emulated $WindowStart --duration 60 --out "$Run/analysis.json"
python -B "$ToolDir/ingest_run.py" $Run --out $Out
```

The Windows runner uses a hidden, silent, below-normal-priority child. It checks the marker before preparation, before launch and while running; stops only its owned child; refuses reused run directories; and allows a wall deadline of 1–1800 seconds (default 600). Required settings are D3D, panic/OSD/confirmation dialogs off, no audio output, volume zero and muted, automatic HMD VBI off and VI overclock off. `EnableOpenXR=True` is required solely by the reviewed synthetic-render branch, which must log `OpenXR not initialized; synthetic eyes.` and never initialize a runtime. A missing runtime sentinel and the pinned probe are required together. This is not a normal executable launch recipe.

`--phase baseline|before|after` labels each receipt independently of the unique run tag; its default is `baseline`. `--start` selects a measurement anchor in emulated seconds. It may target a menu or an unverified next state during route discovery and never implies gameplay. `--capture-policy sampled` is the default and schedules captures throughout the replay, including measurement checkpoints. `--capture-policy before-window` retains only nonnegative capture times no later than `start − 10` and adds a capture at `start − 10` when that time is nonnegative. This removes scheduled PNG readback/compression from the timing window; asynchronous capture completion and remaining instrumentation overhead are not separately measured. Visually review the final pre-window capture to identify the state and preserve identical capture policy when comparing timing runs.

For shorter prompt iteration, add `--observe-until <emulated-seconds>` with a finite value of at least 5, use a new tag and prefer `--capture-policy sampled`. The runner filters scheduled captures to at most observation end minus 5 seconds, adds a final checkpoint there when the capture policy permits, then stops its owned child when the trace reaches the requested emulated time plus the existing three-second wall flush. `--start` remains a recorded observation anchor and makes no gameplay claim. Observation receipts record `observe_until_emulated_s`, `stop_reason: observation_end`, `measurement_window_complete: false` and `accepted_as_measurement: false`. The wall deadline and headset/synthetic guards still apply. Ingest rejects these runs as timing evidence; review captures to decide the next route step before scheduling a separate 60-second performance run. Omitting `--observe-until` preserves the normal measurement-window behavior.

For a probe built with the [append inbox](../motion-controls/input-probe/APPEND.md), `--live-input` adds a run-local `live-input.txt` while observing. Append validated future replay lines there and use only the probe's in-program captures to inspect the result. The runner requires an `AC_INPUT_APPEND ready` acknowledgment after 30 seconds and records the final inbox hash. Creating the run-local `STOP` file stops the owned child; it does not stop other applications. This mode requires `--observe-until` and cannot create timing acceptance. Preserve the fixed script and appended commands as route evidence before preparing a separate timing pass.

Metrics tracing is bounded to 1800 seconds or 250,000 metadata rows. The runner seeks a complete 60-second wall window after the selected emulated start. `--rate 72` and `--rate 90` request separate forced-VI experiments; modeled 72/90 deadlines from a native trace are source-availability models, not separate runs or delivery measurements. XFB changes do not prove unique content frames. Ingest requires the synthetic guard, completed window, matching observed cadence and disabled VI overclock. It writes an E2 receipt and imports it into the scorecard. Add `--stage title`, `menu` or `gameplay` only after visually reviewing that run; stage remains unassigned by default. Real compositor delivery and owner acceptance remain unknown.

Frame receipts include `capture_policy`, scheduled capture times that fall inside the observed measurement window, and `capture_overhead_measured: false`. For older runs without schedule metadata, ingest recovers scheduled `shot` events from the preserved per-run input script and labels the policy `sampled`. Those are scheduled events, not proof of completed PNG writes or measured capture cost. Do not attribute a before/after timing difference solely to an emulator change when capture policies differ.

## Receipt schema version 1

Each receipt describes one game/view/phase/stage and one evidence scope. Required identity fields are `game_key` and `view`, matching the baseline. `phase` defaults to `baseline`; allowed values are `baseline`, `before`, `after`. Optional `stage` is `title`, `menu` or `gameplay`. `notes`, timestamps, config snapshots and other top-level extension fields are retained. Evidence pairs must be `E0`/`source`, `E2`/`offline`, or `E3`/`owner`. E3 additionally requires nonempty `owner_confirmation.by`, `.at` and `.statement` strings. This checks the presence of an explicit owner record; it cannot authenticate the writer. No tool-generated E3 receipt is appropriate.

`evidence_paths` is a required nonempty list of absolute paths. The importer records its own receipt path and SHA-256 as well. It does not assert that the listed files exist or validate their contents. A receipt's provenance and supplied values need review. E0 may contain only launch-plan metrics; measurements need E2 or an explicit owner receipt.

A correction can declare `supersedes_receipt_path` plus `supersedes_receipt_sha256`, or `supersedes: {receipt_path, receipt_sha256, reason}`. The referenced receipt must already exist in the same game/view/phase with both recorded identity fields matching. Cross-scope, missing and conflicting references are rejected. The old receipt and its measurements remain in detailed history with a superseded note, while the current overview excludes it. Stage may change as part of a reviewed correction; supersession never implies acceptance or silently merges metrics from the replaced receipt.

Synthetic example (no actual run or measurement):

```json
{
  "schema_version": 1,
  "game_key": "example-game",
  "view": "3d",
  "phase": "before",
  "stage": "gameplay",
  "evidence_level": "E2",
  "evidence_kind": "offline",
  "evidence_paths": ["/synthetic/.local/example/present-trace.json"],
  "metrics": {
    "stereo": {"distinct_eyes": null, "near_px": null, "far_px": null, "eye_width_px": null, "method": null},
    "placement_2d": {"classification": null, "disparity_px_1920": null, "method": null},
    "scale": {"known_object": null, "assumed_size_m": null, "assumed_size_uncertainty_m": null, "measured_game_units": null, "units_per_metre": null, "uncertainty": null, "method": null},
    "frames": {
      "native": {"target_hz": null, "wall_duration_s": null, "emulated_duration_s": null, "present_count": null, "eligible_stereo_frame_count": null, "layers_min": null, "layers_max": null, "fresh_content_frames": null, "repeated_content_frames": null, "elapsed_wall_ticks": null, "tick_frequency_hz": null},
      "72": {},
      "90": {},
      "real_compositor": {}
    },
    "image": {"aliasing": null, "artefacts": null, "method": null},
    "controls": {"bindings": null, "aim_error_percent": null, "aim_points": null, "method": null},
    "launch": {"print_only": null, "exit_code": null, "command": null, "output_path": null}
  }
}
```

Metric groups are optional. Missing known fields become JSON null / Markdown `unknown`. Extension fields inside a metric group are retained. Unknown group or frame-channel names fail to catch common schema mistakes.

| Group | Interpretation |
|---|---|
| `stereo` | Signed near/far horizontal disparities. `near_px_1920` and `far_px_1920` are derived only when raw pixels and positive eye width are supplied. Direct normalized measurements may instead be supplied with their method. No scene depth or distinct-eye verdict is inferred from a median or screenshot count. |
| `placement_2d` | Classification: `face`, `room_screen`, `hidden`, `doubled`, `mixed`, `unknown` or null. Record method and normalized 2D disparity separately. |
| `scale` | Known-object identity, assumed metres and size uncertainty, measured game units, estimated units/metre and uncertainty/method. Units/metre is never guessed or derived automatically from an unidentified object. |
| `frames` | Separate `native`, `72`, `90` experiments. Record the actual wall/emulated duration, counter scope/ticks and observed present count. `expected_wall_frames` is computed only from supplied wall duration and target Hz. Native Hz is never guessed. A measured 60-second window is needed for the scope's 60-second requirement. |
| `frames.*.layers_min`, `layers_max` | Minimum/maximum texture-layer properties observed during the timing scope. These are texture properties, not counts of frames. The ambiguous former `stereo_layer_count` and `stereo_layers_eligible` fields are rejected. |
| `frames.*.eligible_stereo_frame_count` | Number of observed presents whose texture has >= 2 eligible eye layers. This must come from per-present instrumentation. It is never derived from min/max layers, capture count or a screenshot. It does not prove the eyes differ, fresh content, or actual headset delivery. Fresh and repeated content counters remain unknown unless actually measured. |
| `frames.real_compositor` | Always unknown for source/offline receipts. Only an explicit owner receipt may supply this channel. Offline counts, synthetic-head captures and layer counts never populate it. |
| `image` | Aliasing and artefact observations with the sampling method. Baseline MSAA/internal-resolution settings remain source evidence, not measured image quality. |
| `controls` | Binding-by-binding evidence (object/list as useful), aim error in percent, point count or samples, measurement method. No all-bindings or nine-point pass is inferred. |
| `launch` | `print_only: true`, exit code, command and absolute captured output path for a separate launch-wii.ps1 -PrintOnly validation. Aggregator never invokes the launcher. No launch is inferred from a config listing. |

The overview lists reported fields and stages with their view/phase/stage, per-view receipt ordinal and evidence level. It keeps different runs separate and leaves missing groups unknown; detailed receipts retain exact paths and all remaining unknown fields. An unassigned stage has not been verified. A single stage, one view or one after run cannot establish all-game acceptance. E2 receipts do not promote the baseline game's E0 label to E3. Keep Metroid's before/after timing scopes comparable and distinguish an observed count from a claimed historical drop percentage.

CPU tests from this directory: `python -B -m unittest -v test_scorecard.py`. They use only synthetic data in temporary folders and do not read actual games or run an engine.
