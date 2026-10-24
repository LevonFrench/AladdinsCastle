# Bounded offline draw discovery

This exporter records CPU primary-draw metadata for reviewed synthetic Dolphin probes. It creates no placement rules, changes no game configuration and infers no HUD/menu/world roles. The repository contains only the helper, anchored copy preparer, bounded stdlib CSV reviewer and synthetic tests. Generated Dolphin translation units, build inputs, game data and captures belong in a private `.local` folder.

Supply caller-reviewed absolute paths and the SHA-256 of the original `VertexManagerBase.cpp`:

```powershell
python -B setups/dolphin-vr/draw-discovery/prepare_draw_copy.py --source "$SourceFile" --source-sha256 "$SourceSha256" --out "$PrivateCopyDir"
python -B setups/dolphin-vr/draw-discovery/analyze_draws.py --input "$PrivateCsvDir" --output "$PrivateInventoryDir" --start 120 --duration 0.5 --marker "$HeadsetMarker"
```

All variables above are caller-supplied. The Python tools enforce fresh output directories within the nearest checkout's `.local`; existing directories/files are preserved. The preparer validates the input hash and every exact anchor before writing, copies the helper beside the generated source, and exclusively creates a source-copy receipt with source/output/helper/preparer hashes. Failed writes preserve partial evidence. It never edits the input, builds or launches anything. Anchor drift, extra/missing texture-load branches and previously instrumented source fail closed. The source size cap is 4 MiB.

The caller separately reviews and builds the generated source against the matching Dolphin revision and its existing dependencies. This tool supplies no build/link commands or synthetic-renderer bypass. The caller must verify the probe's pre-loader synthetic guard independently and retain its binary/configuration/source lineage. A successful source transformation is not compilation or runtime proof.

## Admission and bounds

The header is off by default. It requires all of these environment settings:

| Variable | Requirement |
|---|---|
| `AC_STEREO_PROBE` | Exact string `1` |
| `AC_DRAW_DISCOVERY_START_S` | Finite nonnegative cached-present emulated time |
| `AC_DRAW_DISCOVERY_DURATION_S` | Finite duration greater than zero, at most five emulated seconds |
| `AC_DRAW_DISCOVERY_CSV` | Fresh private draw CSV path |
| `AC_DRAW_DISCOVERY_TEXTURE_CSV` | Fresh, distinct private texture CSV path |

`ENABLE_VR` and a null `VR::g_openxr` are required at draw admission and record submission. The helper only reads that pointer; it initializes/invokes no XR function. Losing the guard closes the export. This check cannot retroactively prevent a bad probe from initializing XR. Both CSVs use exclusive `wx` opens. If either open fails, capture stops and any partial output remains. Parent directories must already exist. CSV path privacy is a **caller/runner contract**: the C++ helper does not check absolute paths, checkout membership or `.local`, so the reviewed runner must supply fresh, distinct absolute paths inside its private output folder.

Start with a separate 0.25–1-second metadata observation. Hard caps are 20,000 primary draw rows, eight texture rows per draw and 60 host seconds after opening, checked at admission, primary submission and boundaries. Submission rechecks after any texture/shader/driver stall and uses the checked timestamp in the row. They do not replace an externally bounded owned-process runner if emulation hangs. Check the caller's headset safety marker before builds/runs. CSV formatting, cached-family lookups, flushes and screenshots add unmeasured overhead; discovery observations establish no performance result.

## Recorded semantics

The copy utility hooks both existing texture-load branches without performing new loads. It records the primary `DrawCurrentBatch` path before draw statistics, excluding the coverage prepass. Draw order is emulated state order and may have gaps for skipped draws. Per-draw fields include regular lookup UID CRC32s, selected regular/uber base UID CRC32s, cached lookup families, replacement flags, raw GX projection/viewport/scissor/blend/depth state and submitted geometry stereo/screen constants. A cached family value of zero means unavailable. CRCs describe UID configuration, not compiled shader bytecode; custom/pink replacements delimit their meaning.

Time is the cached last-present event, shared by many draws, rather than exact per-draw emulation time. Boundaries are emulator frame grouping, not VI, changing-content FPS, GPU completion or compositor delivery. Texture dimensions/layers are snapshots immediately after the existing load, before overrides. `hash_changed` is 0=same observed bound hash, 1=different, 2=missing load snapshot. Same hash does not prove unchanged final allocation/dimensions/layers. Missing snapshots do not establish zero-sized textures. Raw GX viewport/scissor state is not necessarily the final host raster viewport.

The reviewer requires exact ordered CSV schemas, finite floats, nonnegative integers, contiguous draw IDs, monotonic clocks/boundaries/order, valid flags, matching texture joins/stages/masks and the requested window/caps. Each CSV is read into a bounded byte snapshot (at most128 MiB; fields at most4096 characters); parsing and receipt SHA-256 use those same bytes even if the file changes later. Use completed capture pairs: the two snapshots are not an atomic cross-file snapshot, and source files may later differ from the recorded hashes. An inventory does not prove the entire requested interval was captured.

Semantic/count/join errors collected during validation produce an exclusive inventory with `valid=false` and a failing exit code. Syntax/schema, nonfinite, oversized, malformed-row and row-cap parsing failures abort before an inventory is created; original inputs remain untouched. Callers must treat any nonzero exit as failure, including cases with no receipt. Valid inventories include input/analyzer hashes and bounded candidate identities. Candidates retain `role=unknown`; neither orthographic projection nor texture hash establishes semantic role. No shader source, game-code bytes, vertex streams or pixel readback is exported.

CPU-only checks, when the timing lane is idle, from this directory:

```text
python -B -m unittest -v test_discovery
```

Fixtures stay in the checkout's ignored `.local`. These checks cover transform drift/hash/exclusive outputs and synthetic CSV schema/count/join rejection; they do not compile the Dolphin helper or run an engine. Runtime captures and physical scale, control response, stereo quality and owner acceptance require separate evidence.
