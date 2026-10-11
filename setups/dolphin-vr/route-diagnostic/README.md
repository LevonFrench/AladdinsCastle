# Opt-in renderer route diagnostics

This diagnostic is for DolphinXR pin `85c4266b29eb308d173fefa0f21f5cb77df3516a` plus the preserved draw-trace and facing deltas. It does not repair rendering or change any rendering default. It runs after the primary submitted draw, excluding the coverage prepass, and records the GS uniforms that were uploaded rather than claiming which pixels they produced.

`apply.py` accepts only an owned source checkout under `.local` at the exact pin. It checks all five existing files against `baseline-sha256.json` (UTF-8 text with line endings normalized) and prepares all replacements before writing. It refuses a different baseline or an existing diagnostic helper. Apply once to a separate private source tree:

```text
python -B setups/dolphin-vr/route-diagnostic/apply.py <absolute-private-source-path>
```

`dolphinxr-85c4266-trace-facing-route.patch` is a separate delta over that baseline. The original `ACDrawTrace.h` is unchanged. Keep the previous candidate, original trace, diagnostic patch and any later executable independently hashed. Only the standalone CPU test has been compiled so far; native integration has not been compiled or executed.

## Fields and limits

The CSV records final GS route independently from an inferred reason: inactive/flat, explicit per-draw request, actual automatic EFB selection request, pane request applied/not applied, preserved target, small viewport, perspective world, natural ortho or unclassified. Requests may be superseded and do not prove the final route. A pane requires perspective projection, a valid frame and final route 2 or 3 before it is described as applied. `unclassified` deliberately leaves a gap rather than inventing causality. Viewport class IDs match the pinned `VRViewportClass` enum. Texture provenance is numeric: bound/EFB/XFB flags, native dimensions, layer count and presence of a retained stereo EFB reference. It omits texture names, content, vertices and emulated addresses. The used-stage mask distinguishes an unused stage from a used-but-unbound stage; unavailable fields are `-1`. A stereo reference alone does not establish that the destination preserved both eyes.

Original game projection coefficients, viewport/scissor/depth/blend/cull state, completed XFB frame region, effective presentation aspect, configured/cached units per meter, cached GS screen/depth/eye tables and shader UID hashes provide linkage. Table columns are explicitly labeled `cached_*`: a route may use different entries or the head-locked table instead. Non-finite floating values serialize as `NA`. Inactive, FlatScreen and native-fullscreen route 0 mask unused VR table fields as `NA`. Shader UID hashes identify pipeline structure, not full game shader/element signatures. The frame region is the previous completed XFB region, not a fresh declaration by the current draw. Presentation aspect uses the presenter's non-window-stretch policy; explicit desktop stretch is excluded from this calibration value.

Off means no output path and no file writes. Atomic exclusive creation refuses any existing destination, preserving earlier receipts without an exists/open race. Enabled limits are: one sampled frame per 30 game frames, at most 100,000 complete rows, 32 MiB including the header and 600 seconds after an optional start delay. Delay is limited to 0–600 seconds; invalid/non-finite values fail closed. Samples may stop within a frame at a bound. Missing a draw in a bounded sampled file does not establish that it never occurred. A failed output open is inert; check that the header/file exists before treating a capture as usable.

## Next private capture recipe — requires a new lease and reviewed build

1. Review the source delta, then separately authorize/build and identify a diagnostic executable. Preserve the current executable; this source artifact is not a runnable replacement.
2. Keep Default/Sharp, the isolated user profile and the same title/settings as the failed sitting. Do not guess world scale, change the target classifier or combine Eager/quality changes with this diagnosis.
3. Before starting the authorized process, set `AC_DOLPHIN_VR_ROUTE_LOG` to a fresh absolute private CSV path. Optionally set `AC_DOLPHIN_VR_ROUTE_START_SECONDS` to a documented integer delay so the bounded sample window covers the mission. Setting it after process creation cannot enable that process's logger. Ensure the private parent directory exists and use a unique receipt filename.
4. If desired, set the preserved logger's separate `AC_DOLPHIN_DRAW_LOG` to another private CSV. It samples every 30 frames with its own 200,000-row bound. It records original projection/BP state but not final route or EFB provenance. Its FOV/UID fields aid correlation without proving the culprit.
5. Record exact executable/patch/config hashes, process start time, delay and phase markers. Correlate mission world draws, ortho composites, automatic effects, viewport classification, native copy dimensions and eye-layer preservation around the reported failure. Keep all runtime CSVs and supplied imagery private. Mono imagery does not prove correct stereo.
6. End the bounded run, verify owned process cleanup and clear only these diagnostic environment variables from the launcher process. Diagnose the actual offending route before changing it or calibrating world aiming.

## Synthetic CPU verification

With an already configured compiler/CMake/Ninja:

```text
cmake -S setups/dolphin-vr/route-diagnostic -B .local/route-diagnostic-ci -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
cmake --build .local/route-diagnostic-ci --target ac-dolphin-route-trace-test
ctest --test-dir .local/route-diagnostic-ci --output-on-failure
```

MSVC `/W4 /WX` and CTest passed (1/1). Fixtures contain synthetic numbers only. Coverage checks reason precedence/unclassified states, stable CSV arity and locale, unavailable/non-finite values, opt-out, sampling interval, row/byte/time caps, no partial rows, invalid bounds, delayed capture, exclusive preservation of a pre-existing sentinel, actual pane request/apply branches and unused FlatScreen/native table masking. GCC/Clang are configured with warnings as errors but were not run locally. Lead owns CI wiring and independent native source review.

Once an offending pass is established, the renderer regression should reproduce its numeric route decision and EFB provenance with synthetic data, assert unchanged native projection for preserved targets, coherent two-eye transforms for scene geometry, per-eye effects without screen reprojection, and matching screen/viewport remap. Any scale/near-plane repair needs cardinal yaw plus pitch/roll fixtures with asymmetric frusta, near/far points and triangles crossing the eye plane. These are plans; no visual or native integration acceptance is claimed.
