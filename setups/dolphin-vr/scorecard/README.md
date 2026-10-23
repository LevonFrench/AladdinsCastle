# Private Wii W0 scorecard

`scorecard.py` is a standard-library JSON aggregator. It reads the launcher manifest and explicit receipt files, then writes only `SCORECARD.md` and `scorecard.json` in the supplied private `.local` output directory. It never opens referenced discs, configurations, captures or executables; launches a process; changes settings; or contacts a runtime/network service. Keep all manifests, receipts and generated outputs private. Do not commit them.

Check the owner's `J:/projects/games/aladdinscastle/.local/review/HEADSET-ON` guard before any execution, including these CPU-only commands. If it exists, write code only.

```powershell
python -B J:/projects/games/aladdinscastle/.local/worktrees/wii-overnight/setups/dolphin-vr/scorecard/scorecard.py init --manifest J:/projects/games/aladdinscastle/.local/play/wii/games.json --out J:/projects/games/aladdinscastle/.local/lanes/2026-10-22/wii-overnight
python -B J:/projects/games/aladdinscastle/.local/worktrees/wii-overnight/setups/dolphin-vr/scorecard/scorecard.py import --out J:/projects/games/aladdinscastle/.local/lanes/2026-10-22/wii-overnight --receipt J:/projects/games/aladdinscastle/.local/lanes/2026-10-22/wii-overnight/example.receipt.json
```

`init` refuses to overwrite either existing output. `import` accepts repeated `--receipt` arguments and rejects the whole batch before saving if any receipt is invalid. Exact duplicate receipt bytes are ignored per game/view. `render --out <absolute-private-directory>` regenerates Markdown from the JSON. It does not reread the baseline manifest. Existing runs remain separate by view, baseline/before/after phase and title/menu/gameplay stage; a later partial receipt never erases earlier evidence. To change the manifest baseline, create a new private output directory.

The manifest's game title/key, view configuration, paths, environment and optional game INI text form an E0 source baseline. A game ID inferred from an INI filename is explicitly unverified against the disc header. Prior status, control prose and view notes are stored separately as unverified historical claims. No pass, game acceptance, headset delivery or numeric measurement is inferred from these claims.

## Receipt schema version 1

Each receipt describes one game/view/phase/stage and one evidence scope. Required identity fields are `game_key` and `view`, matching the baseline. `phase` defaults to `baseline`; allowed values are `baseline`, `before`, `after`. Optional `stage` is `title`, `menu` or `gameplay`. `notes`, timestamps, config snapshots and other top-level extension fields are retained. Evidence pairs must be `E0`/`source`, `E2`/`offline`, or `E3`/`owner`. E3 additionally requires nonempty `owner_confirmation.by`, `.at` and `.statement` strings. This checks the presence of an explicit owner record; it cannot authenticate the writer. No tool-generated E3 receipt is appropriate.

`evidence_paths` is a required nonempty list of absolute paths. The importer records its own receipt path and SHA-256 as well. It does not assert that the listed files exist or validate their contents. A receipt's provenance and supplied values need review. E0 may contain only launch-plan metrics; measurements need E2 or an explicit owner receipt.

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
  "evidence_paths": ["J:/private/.local/example/present-trace.json"],
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

The overview lists each game and view but keeps aggregate verdicts unknown; detailed scoped receipts contain measured fields and remaining unknowns. A single stage, one view or one after run cannot establish all-game acceptance. E2 receipts do not promote the baseline game's E0 label to E3. Keep Metroid's before/after timing scopes comparable and distinguish an observed count from a claimed historical drop percentage.

CPU tests from this directory: `python -B -m unittest -v test_scorecard.py`. They use only synthetic data in temporary folders and do not read actual games or run an engine.
