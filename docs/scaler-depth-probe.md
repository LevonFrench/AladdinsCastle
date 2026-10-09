# OutRun sprite-depth probe — MAME 0.289

Experiment date: 2026-10-08. Branch: `codex/mame-sprite-probe`, isolated worktree `J:\projects\games\aladdinscastle-codex-probe`, based on main `f1161b1`. Tools are original GPL-3.0-only code. No Cannonball code, game pixels, screenshots, ROM bytes, or game-specific world-Z memory was used in the probe. The requested wiki notes were read as leads only; the actual decoder/API behavior was checked against the pinned MAME `mame0289` source and the owner's installed **0.289 (mame0289)** executable.

**Verdict:** a single per-game `z = k / effective_sprite_scale` does **not** provide reliable automatic depth for all OutRun sprites in this run. The pooled fit has r=0.371549, median relative error 50.16% and p90 error 177.90%. Some individual source-address groups fit well, supporting authored/classified ground billboards as an experiment, but that does not establish automatic depth for every sprite or physical VR correctness. The road comparison is a **flat-ground screen proxy**, not measured world Z. A good correlation against this proxy would still require independent world-depth/headset validation.

## What ran and what remains private

- `J:\projects\games\aladdinscastle-codex-probe\tools\mame-probe\outrun_sprites.lua`: MAME autoboot observer, default 3,000 displayed emulation frames, automatic clean exit.
- `J:\projects\games\aladdinscastle-codex-probe\tools\mame-probe\analyze.py`: Python 3.12, standard library only; distributions, both zoom conventions, regressions, temporal holdout, identity groups, numeric outliers and optional text histogram.
- Successful local capture: `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\outrun-20261008-214106.jsonl`.
- Numeric local summary: `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\outrun-20261008-214106-summary.json` (and optional `.txt`).

Captures, source downloads, runtime state and test fixtures stay under ignored `.local/`; `git check-ignore` confirmed the capture is ignored. Only the two tools and this document belong in the commit. The public document does not contain the owner's emulator/content paths. Those were read from the private main-checkout AGENCY.md; the ROM folder was only read by MAME. No plugin was installed, removed or altered. `-noreadconfig -nowriteconfig -noplugins` avoids the owner's configuration/plugins; all writable cfg/NVRAM/state/input/snapshot/diff/home directories were explicitly redirected to this worktree's `.local/mame-probe/runtime/`.

The successful capture is an untouched **attract-mode** run: no coin/start/pedal/steering automation, no emulated input writes and no save-state loading. It contains 3,000 frame records plus metadata and completion trailer; road latches occur at roughly half the screen callback cadence. Runtime speed 135.33% from MAME is observation overhead/host throughput, not a VR performance measurement.

## Source pin and exact APIs

Primary sources are the [MAME mame0289 sprite implementation](https://github.com/mamedev/mame/blob/mame0289/src/mame/sega/sega16sp.cpp), [OutRun driver](https://github.com/mamedev/mame/blob/mame0289/src/mame/sega/segaorun.cpp), [road implementation](https://github.com/mamedev/mame/blob/mame0289/src/mame/sega/segaic16_road.cpp), [Lua bindings](https://github.com/mamedev/mame/blob/mame0289/src/frontend/mame/luaengine.cpp) and [Lua memory documentation](https://github.com/mamedev/mame/blob/mame0289/docs/source/luascript/ref-mem.rst). The road implementation in this tag is in **segaic16_road.cpp**, rather than the broader segaic16.cpp suggested in the brief.

For precise local file:line receipts, downloaded source paths below are rooted at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source`. These are text-only public source snapshots; no game assets were downloaded. Line numbers are one-based and apply to the pinned tag, not current master.

| API actually called | Verification |
|---|---|
| `emu.app_version()` | `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine.cpp:899`; installed executable also printed `0.289 (mame0289)`. Script rejects other version families and systems. |
| `manager.machine`, `.system.name`, `.devices[tag].spaces["program"]` | Machine/device access is documented in `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\ref-core.rst:288` and memory-space instantiation in `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\ref-mem.rst:52`. Discovery verified `:maincpu`, `:subcpu`, `:sprites` and `:segaic16road`. |
| `machine.devices[":sprites"].items`, `emu.item(index)`, `item.size/count`, `item:read(word_index)` | Device-item enumeration binding at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine.cpp:1612`; item construction/read at `:1154` / `:1171`. Discovered item **0/m_buffer**, element size 2, count 2,048; checked dynamically rather than hard-coding its save-item index. |
| `machine.memory.shares[":segaic16road:roadram"]:read_u16(byte_offset)` | Named RAM-share reader documented at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\ref-mem.rst:337` and `:349`, binding at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine_mem.cpp:784`. It applies the share's endianness, so no native-endian byte-string decoding is needed. |
| `:subcpu` program space `:install_write_tap`, `:install_read_tap` | Docs at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\ref-mem.rst:122` and `:134`: callback receives address/data/mask; returning no integer leaves the access unchanged. All probe callbacks return nil. Retain handler objects as a frame-callback upvalue; otherwise GC removes the taps. |
| `:maincpu` program space `:read_u16(0x100000)` | Binding at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine_mem.cpp:547`. Captured only as an inactive-bank diagnostic; never used as the active sprite list. Reads use the actual address space and can have side effects (`:325`); the script never reads road-control registers itself. |
| `emu.register_frame_done(callback)` | Binding at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine.cpp:926`, “after frame is drawn” contract at `:809`. Run with frameskip 0/no autoframeskip, including with video none. Verified live; 3,000 callbacks produced 3,000 records. No overlay drawing occurs. |
| `emu.add_machine_stop_notifier(callback)`, `machine:exit()` | Bindings at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\frontend\mame\luaengine.cpp:881` and `:1420`. Frame limit closes JSONL, writes completion trailer and schedules exit; external stop closes a partial file. Analyzer refuses partial/error captures. |
| `io.open`, `stream:write/flush/close`, `os.getenv`, `os.date` | Standard embedded Lua library calls, exercised by actual captures. Output path is `ACVR_PROBE_OUTPUT` or `.local/mame-probe/outrun-<timestamp>.jsonl` relative to the worktree; prepare the directory before launch. `ACVR_PROBE_FRAMES` overrides the default 3,000. |

## Active sprite list and field decoding

MAME's OutRun device renders **buffer()**, not CPU-visible spriteram: `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\sega16sp.cpp:1097`. `draw_write` swaps the two banks then sets the CPU bank's first word to 0xffff (`:59`–`:75`). The rendered vector is registered as a save item at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\devices\video\sprite.h:164`. Reading CPU RAM **after** that swap would therefore capture the wrong list. `emu.item(...m_buffer):read` gives the correct retained list without reading ROM pixels or changing it.

CPU-visible sprite RAM allocation is 0x100000..0x100fff at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\segaorun.cpp:1188`; the programmable memory mapper can remap it (`:796`). The probe decodes up to 256 eight-word entries, stops at the first end marker, records that marker and hidden entries, and omits renderer-mutated scratch word 7. This avoids treating the first word's 0xffff swap marker as a real screen sprite.

Let w0..w6 be the first seven 16-bit words of an entry:

| Recorded field | Decode / interpretation | Source |
|---|---|---|
| end / hidden | w0 bit 15; either bit in mask 0x5000 | `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\sega16sp.cpp:1101` / `:1105` |
| bank / offset / slot | (w0 >> 9) & 7; w1; list index. These are source identifiers, not a stable game-object/world-Z ID. | same file `:1106` / `:1108` |
| screen Y start | (w0 & 0x1ff) − 256 | same file `:1107` |
| screen X anchor | w2 & 0x1ff; add 512 for x<0x80 when rendering right-to-left; subtract **189**, the actual OutRun origin | same file `:1110`, `:1122`, `:1036` |
| pitch | signed-16 reinterpretation of ((w2 >> 1) OR ((w4 & 0x1000) << 3)), arithmetic shift 8. Lua implements arithmetic division/floor explicitly. | same file `:1109` |
| raw vzoom / hzoom | w3 & 0x7ff / w4 & 0x7ff | same file `:1111` / `:1115` |
| Y/X direction / flip | w4 bit15 gives ±1 Y; bit13 gives ±1 X; clear bit14 means flipped | same file `:1112`–`:1114` |
| height / bounds | (w5 >> 8)+1 destination rows; last=start+ydelta×(height−1); top/min and bottom/max are inclusive raster bounds | same file `:1116`, `:1147`–`:1148` |
| palette / priority / shadow | w5 & 0x7f; (w3 >> 12)&3; w3 bit14 | same file `:1117`, format/shadow comment at `:1067`–`:1078` |

**Zoom correction:** the format comment at `:1069` says 0x100 is half and 0x300 is twice; the **executed** rasterizer says otherwise. Each output row advances source rows by accumulated vzoom/512 (`:1208`–`:1210`); each source pixel emits destination pixels while an accumulator is below 512 and adds hzoom (`:1184`–`:1191`). Therefore approximate rendered linear scale is **512/max(raw_zoom,64)**, with clamp at `:1138`. raw=256 doubles scale; raw=512 is unity; raw=768 is approximately 2/3. This is a source-verified sampling convention, not a new guessed world-depth formula. No wiki note was edited.

Screen X is an anchor, and recorded Y bounds are nominal destination rows. Transparent padding, exact opaque footpoints, source width/termination, occlusion and asset physical size are not available from these registers alone. The capture makes no claim that each nominal bottom is a foot touching the ground.

## Road latch and scanline state

The sub-CPU maps road RAM at 0x080000..0x080fff mirrored through 0x08ffff and road control at 0x090000..0x09ffff: `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\segaorun.cpp:1212`–`:1213`. Road rendering uses a private `info->buffer` at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\segaic16_road.cpp:353`. The control read swaps it with CPU RAM (`:587`–`:605`); road device_start registers no buffer save item (`:19`). So reading the RAM share after the swap is also the wrong rendering bank.

The observer initializes a shadow of the CPU RAM share, tracks every sub-CPU RAM write with data/mask merging and mirror normalization, and observes the **game's own** control reads. Read taps execute after the underlying read handler; write taps execute before the write handler, verified at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\emu\emumem_het.cpp:12`–`:13` and `:61`–`:62`. On a latch, the write shadow still contains the pre-swap CPU bank, which becomes `active_road`; read the now-swapped RAM share to seed the next CPU write shadow. No callback replaces data and no observer invokes the control read. This is an implementation of the visible hardware bank operation, not a patched emulator or a world-Z lookup.

Control writes provide bits 0..1 (`J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\segaic16_road.cpp:612`). Until a real control write and road latch are observed, samples are not eligible. For each screen scanline y=0..223, record:

- data0=buffer[y], data1=buffer[0x100+y], road-template row=(data>>1)&0xff, and solid/background flag data&0x800.
- Horizontal scroll: buffer[0x200+(data0&0x1ff)]&0xfff and buffer[0x400+(data1&0x1ff)]&0xfff.
- Colour/stripe controls: buffer[0x600+(data0&0x1ff)] and buffer[0x600+(data1&0x1ff)]. These are control words, never decoded pixel colours or graphics.
- Visible-road candidate: road0 non-solid in mode0, road1 non-solid in mode3, either non-solid in mode1/2.

Lookup paths are at `J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\source\src\mame\sega\segaic16_road.cpp:369`–`:370`, `:425`–`:436`; combination/mode selection at `:457` onward. OutRun control does not expose the X-board bit2 indexing mode. A template-row selector is a **ROM graphic row selector**, not metres or guaranteed linear world distance. This experiment captures no road graphics.

## Fit method and selection limits

For each eligible frame, infer h as its first scanline with non-solid road in the selected mode. Under a flat ground plane and pinhole camera, screen ground contact obeys y−h≈fH/Z. We use **z_road=1/(bottom−h)**, equivalent to setting the unknown camera-height×focal constant fH=1. Its units are inverse pixels. Setting fH=1 changes k's units, not correlation/relative error; it does not calibrate physical distance. Hills, camera pitch, road holes/occlusion and nominal sprite padding violate the assumption.

Default candidate filters are recorded in the JSON summary: frames 300..3000 inclusive; a latched/known road; visible non-hidden/non-end sprite; X anchor in 0..319; un-clipped nominal Y rows; height 4..150; raw zooms ≥64; max(hzoom,vzoom)/min(...) ≤1.25; nominal bottom at least eight rows below h on a non-solid road scanline. This is **possible ground contact**, not a semantic/opaque-pixel contact detector. It can accept HUD/player/screen overlays and reject tiny far objects or tall scenery. Outlier labels are numeric rejection classes, not recognition of game art.

Fit both x models against z_road:

1. Correct renderer convention: x=1/effective_vscale=raw_vzoom/512. Through-origin least-squares k=Σ(xz)/Σ(x²); also report ordinary OLS with intercept, Pearson r, r², normalized RMSE and relative-error quantiles.
2. Literal raw-register reciprocal: x=512/raw_vzoom. This diagnostic is intentionally the reversed convention; it is not the recommended depth formula.

Temporal validation fits k on frames ≤2400, checks later frames 2401..3000 without refitting. Also fit distinct (vzoom,bottom,horizon) tuples, fixed-horizon subsets, observations excluding raw zoom512, priority buckets and (bank,offset) groups with ≥30 observations and nonzero variance. Frames/object repeats are correlated; no statistical independence, confidence interval, ground truth or headset acceptance is claimed. Per-address groups can change with animation/LOD and can contain several instances of the same graphic.


## Results — numbers only

The successful capture ended with `reason=frame_limit`, 3,000 frames and 1,489 observed road latches. 2,987 frames had a latched road; 2,701 analyzed frames had eligible road state. The 299-frame startup exclusion is intentional. Capture file size is 159,395,507 bytes, private/ignored; the full capture is not a public deliverable.

| Measurement | Value |
|---|---:|
| Visible non-hidden sprite observations after startup | 125,009 |
| Raw hzoom and vzoom unique values | 124 each |
| Raw zoom min / p10 / median / p90 / max | 256 / 504 / 704 / 1008 / 1008 |
| Sprite priorities observed | All 125,009 at priority 3 |
| First-road scanline min / median / max | 113 / 152 / 193 |
| Road-contact candidate observations | 42,069 |
| Distinct zoom/footpoint/horizon tuples | 2,479 |
| Rejected nominal height outside 4..150 | 48,305 |
| Rejected above-road/near-horizon | 34,115 |
| Rejected offscreen X anchors | 520 |
| Hidden entries / end markers | 29,201 / 2,701 |

Priority 3 is therefore not a usable distance discriminator in this capture. hzoom/vzoom distributions agree; none of the retained observations needed the raw<64 enlargement clamp. The zoom histogram's most frequent values are 1008 (20,546), 504 (12,474), 512 (9,753) and 897 (9,403).

| Model | n | k through origin | Pearson r | Median relative error | P90 relative error |
|---|---:|---:|---:|---:|---:|
| Renderer-correct 1/effective scale | 42,069 | 0.0402747403 | 0.371549 | 50.16% | 177.90% |
| Literal 512/raw register | 42,069 | 0.0493271652 | −0.396182 | 57.85% | 262.58% |
| Distinct geometry tuples, correct convention | 2,479 | 0.0432760787 | 0.395323 | 44.62% | 160.33% |
| Correct convention excluding raw=512 | 32,882 | 0.0430609893 | 0.270456 | 46.71% | 150.21% |
| Fixed first-road scanline=152 | 13,452 | 0.0446206109 | 0.422278 | 56.97% | 159.27% |
| Fixed first-road scanline=123 | 14,656 | 0.0347218691 | 0.339903 | 57.03% | 139.58% |

Pooled correct-convention OLS slope=0.0383803951, intercept=0.00245690345, r²=0.138048; through-origin normalized RMSE=0.624672. **86.89%** of its candidates exceed 25% relative error. Time holdout: 31,301 training observations, 10,768 held-out observations, train k=0.0390071456; holdout median error=53.92%, p90=126.65%. Deduplication, removing unity-scale sprites, and selecting two common fixed horizons do not rescue the pooled model. These errors are disagreements against the chosen road proxy, not measured errors in metres.

Source-address-specific fits show why class knowledge may help: 34 (bank,offset) groups with ≥30 observations and varying zoom/proxy were eligible; 14 had r≥0.9. Their fitted k values ranged from **0.0149907937 to 0.106188287**, a factor of about **7.08**. Examples:

| Bank / offset | n | k | r | P90 relative error |
|---|---:|---:|---:|---:|
| 0 / 21178 | 74 | 0.0226777164 | 0.999354 | 4.22% |
| 0 / 5899 | 52 | 0.0325109610 | 0.998511 | 5.00% |
| 0 / 7081 | 60 | 0.0327450011 | 0.996667 | 5.76% |
| 1 / 34393 | 48 | 0.0601980430 | 0.987589 | 5.08% |

These are exploratory in-sample groups, selected by correlation; they are **not** independent class-specific holdout validation. They carry raw source addresses only, with no semantic/game-art identification. The largest group (bank1/offset0, n=4,977) had r=0.422856 and median error 35.79%, showing that selecting an address alone does not guarantee a trustworthy class.

Numeric outliers include frame305/slot37, bank0/offset484, raw zoom512, bottom221, first-road113: z_road=0.00925925926 while the pooled model predicts 0.0402747403, a 334.97% relative discrepancy. Several unity-scale observations have bottom221 through changing road horizons. They are possible screen overlays/HUD/player elements; no sprite artwork was read to identify them. The other rejection groups can contain tiny distant scenery, near-horizon backgrounds or giant painted elements, but the tool cannot truthfully label their art/role from these registers. Automatic recognition of those categories remains missing.

## Design implication and what needs authoring

The experiment rejects the **simple universal pooled calibration** as sufficient evidence for VR; it does not prove that every hardware-based reconstruction is impossible. Render-scale registers describe sampling within the currently selected image. If the game changes source image/LOD/dimensions, a similar physical object can use different zoom conventions or base sizes. Projected nominal footpoint is also not necessarily world-ground contact. Priorities here distinguish no depth at all. An absolute affine ground transform is absent from this OutRun device, and the road-table selectors do not reveal physical camera height or longitudinal distance by themselves.

A limited billboard reconstruction should therefore start with an authored/profiled classification, not advertise automatic true-3D placement for every scaler sprite:

1. Define road/camera units and height/pitch assumptions; preferably validate against an independently derived game-state road curve or known-world-depth reference. Capture source data alone cannot calibrate metres.
2. Identify grounded source/LOD families and their bottom-padding/footpoint corrections; use per-class scale normalization or a distance LUT. The observed k range makes one k per game inadequate under this selection.
3. Identify screen overlays/HUD, sky/background/horizon art, player-car parts/shadows and giant painted scenery; assign fixed/approximate layer policies explicitly. Never turn their nominal bottom into “road contact” blindly.
4. Treat road hills, bends, tunnels, splits and occlusion explicitly. The first non-solid row can change by 80 pixels here; a flat-ground inverse-y proxy is insufficient as independent ground truth.
5. Add identity/animation tracking, opaque-bound/width knowledge and occlusion handling before claiming stable 6DoF billboards. Source bank/offset is not a world object ID.
6. Use confidence/fallback labels for ambiguous sprites and verify stereo comfort/aim in the headset separately. This probe generated no stereo images or headset evidence.

Cannonball's documented `oentry.z` was used only as the lead that an independent game-side reference exists. No code or data was fetched/copied from it and no addresses into the game engine were assumed. Comparison to real game-side Z, physical scale and higher-dimensional road state remains a follow-up, not a result of this capture.

Optional extensions: local filename checks found Power Drift, Galaxy Force II and Chase H.Q. sets present. They were **not** run or decoded in this task. The OutRun fit/road-proxy ambiguity needs resolution first; the six Y-board affine parameters and Taito Z priorities require separate source/latch decoders, not a relabelled OutRun script. No result is implied for them.

## Reproduce without writing to the owner's installation

Read `$MameExe` and `$RomDirectory` from the private owner configuration/AGENCY.md; set both to full absolute local paths. The worktree and `.local/mame-probe` must exist. Run with default `ACVR_PROBE_FRAMES` unset or set to `3000`; unset `ACVR_PROBE_OUTPUT` to use timestamped capture files. Change only the worktree argument if moved to another directory.

```powershell
$ProbeRoot = 'J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe'
New-Item -ItemType Directory -Force -Path "$ProbeRoot\runtime" | Out-Null
Set-Location -LiteralPath 'J:\projects\games\aladdinscastle-codex-probe'
& $MameExe outrun -rompath $RomDirectory -noreadconfig -nowriteconfig -noplugins `
  -video none -sound none -window -nothrottle -frameskip 0 -noautoframeskip `
  -skip_gameinfo -autoboot_delay 0 `
  -autoboot_script 'J:\projects\games\aladdinscastle-codex-probe\tools\mame-probe\outrun_sprites.lua' `
  -cfg_directory "$ProbeRoot\runtime\cfg" -nvram_directory "$ProbeRoot\runtime\nvram" `
  -state_directory "$ProbeRoot\runtime\state" -input_directory "$ProbeRoot\runtime\inp" `
  -snapshot_directory "$ProbeRoot\runtime\snap" -diff_directory "$ProbeRoot\runtime\diff" `
  -homepath "$ProbeRoot\runtime\home" -seconds_to_run 65
python 'J:\projects\games\aladdinscastle-codex-probe\tools\mame-probe\analyze.py' `
  'J:\projects\games\aladdinscastle-codex-probe\.local\mame-probe\outrun-20261008-214106.jsonl' --text-plot
```

`-seconds_to_run 65` is a safety ceiling in emulated seconds, not the intended capture termination. Successful completion must say `frame_limit` and match the metadata's requested count. Avoid reading the road latch/control register from Lua: the address-space API does not automatically disable side effects. No MAME folders or private input configuration should be used as output directories. The script has no input-port or RAM-write calls; handler installation observes execution without returning modified values.

## Verification and open questions

Passed on the local MAME/Python installations:

- Lua API discovery identified the rendered sprite vector and RAM-share dimensions; a corrected 240-frame smoke test observed 109 road latches.
- Full 3,000-frame capture reached the scripted frame limit and exited MAME cleanly. All 3,000 frame lines parse; metadata/trailer counts match. No ROM or sprite/road graphics were read by the Lua observer.
- Analyzer printed both k/r fits and saved numeric summary/text under ignored `.local/` using Python 3.12.10 and stdlib only.
- Synthetic numeric checks recovered known k/r, detected zero variance and rejected a capture missing its completion trailer. Temporal holdout and distinct-geometry reports prevent treating repeated screen frames as independent trials.
- Pinned-source audit checked each API, active-buffer selection, read/write tap ordering, sprite bounds/origin and the renderer's inverse sampling convention. The first attempt's zero-road capture is retained privately as a failed diagnostic and excluded from all results.

Open questions: which subset of source/LOD families is truly grounded; whether opaque footpoint/road-profile metadata can be obtained without game-specific world-Z extraction; how to calibrate road depth on hills; whether class-specific fits generalize to driven gameplay and other tracks; what an independently validated OutRun world-Z reference permits; and whether Y-board affine ground materially reduces these ambiguities. The reported pooled fit is not good enough to settle those questions in favor of fully automatic VR depth.
