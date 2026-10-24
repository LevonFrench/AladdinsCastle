# Experimental XFB stereo preservation

This default-off DolphinXR patch preserves eligible two-layer GPU copies when
stitching a RAM-backed XFB. It is an experimental source fix, not an accepted
Metroid graphics profile. The best observed offline source-health result used
single-core emulation with this feature **off**; visible clipping remains. The
patch is not necessary for that result.

Set exactly `AC_W2_PRESERVE_STEREO_XFB=1` in the emulator environment to enable
the changed path. Active `StereoMode::OpenXR` is also required. All other values
retain the original stitching path. The patch never initializes a runtime,
changes CPU threading, selects a game profile or changes frame cadence.

## What changes

A RAM container starts with mono provenance. If valid overlapping GPU copies
contain two eye layers, a compatible container can be promoted while preserving
its cache identity and references. RAM pixels seed both layers; layered GPU
regions then retain their eye ordering. Each rectangle is mapped using its own
texture allocation dimensions.

Scaled mono contributors and unsupported layered scaling prevent promotion.
Incompatible cached layered containers are rebuilt through the mono fallback;
allocation failure also retains legacy behavior. This can intentionally leave
mono output. The patch cannot reconstruct missing eye pixels or repair upstream
black content. Two allocated layers do not establish two correct eye images.

## Evidence, 23 October 2026

E2 offline, same fresh binary, configuration, replay and seed reference; seed
contents were not hash-pinned. Separate 60-wall-second native windows had no
scheduled captures inside the window:

| Gate, normal CPU threading | Source events | Fresh identities | Fresh two-layer | Fresh mono/blank | Whole-run FIFO mentions |
|---|---:|---:|---:|---:|---:|
| Off | 3597 | 3590 | 4 | 3586 | 5039 |
| On | 3597 | 3591 | 3591 | 0 | 407 |

These count available XFB sources. They do not measure unique content FPS, GPU
completion, headset delivery or comfort. Both runs had FIFO errors, so the
increase is not a clean-render acceptance or proof that this patch fixes FIFO.
Whole-run error counts are not error counts within the measured window.
Normal threading here means no CPUThread override and a source default of true;
the effective threading state was not instrumented.

In matched sampled observations with `Dolphin.Core.CPUThread=False`, both gate
values produced 3597/3597 fresh two-layer sources over emulated seconds
`[230,290)`, with zero FIFO mentions. Four corresponding PNG pairs were byte
identical. This is an observation interval, not a performance window, and shows
no incremental layer benefit from the patch under single core.

A separate 60-wall-second single-core timing run, patch **off**, recorded 3596
events, all fresh and two-layer, zero mono/blank or duplicate sources, and
59.9933 emulated seconds. Whole-run FIFO mentions were zero. This isolates CPU
threading as a useful configuration candidate on this build and route. The
captures still show clipping around the expanded view. The late sampled
single-core images did not reproduce the older Loading panel; its cause is
unproven. No complete graphics or headset acceptance is claimed.

## Apply and build

The patch was applied and built against DolphinXR commit
`6061c9dd8e69cd88858d4819abbabd5ec0add7c1`. Its SHA-256 is
`6857b4b3506d60be3f147c8da17dea9bd0ce7981be149329fdbf6bfc9ec037cc`.
Only `Source/Core/VideoCommon/TextureCacheBase.cpp` and
`Source/Core/VideoCommon/TextureCacheBase.h` change.

From the intended source checkout, with `PATCH` set to the patch's absolute
path, first preserve local edits and run `git apply --check "$PATCH"`. Apply
with `git apply "$PATCH"` only when that check passes. An already applied copy
instead passes `git apply --reverse --check "$PATCH"`. These checks do not change
files. Do not reset or replace unrelated source or gun-bake edits.

Rebuild the complete dependent Core/VideoCommon/D3D graph: the header changes
class layout. Do not transplant one object into old probe archives. The private
Windows full build and a fresh synthetic/input/metrics probe both compiled and
linked successfully. Those builds establish compilation, not title acceptance.

For offline validation, use the hash-pinned, hidden, muted, bounded
[scorecard runner](../scorecard/README.md) and the reviewed
[synthetic stereo probe](../stereo-probe/README.md). Follow the headset marker
guard and run only one emulator at a time. A normal uninstrumented executable
must not be substituted for the synthetic probe.

## Optional bounded diagnostics

`AC_W2_SYNTHETIC_DIAGNOSTICS=1` additionally requires `AC_STEREO_PROBE=1` and no
live OpenXR manager. Set finite nonnegative `AC_W2_TRACE_START_S` and a positive
`AC_W2_TRACE_DURATION_S` of at most 60 seconds. Output is capped at 64 stitch and
64 region records per cache instance. Timestamps are the cached after-present
event time, not current GPU completion. Logs include source hashes and addresses;
keep actual game-run logs private. Diagnostics are unnecessary for normal use.

Other titles/backends, allocation failures, scale changes and headset operation
remain unvalidated. No live binary or game profile is promoted by this package.
