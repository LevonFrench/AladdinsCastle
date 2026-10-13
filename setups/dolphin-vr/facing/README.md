# Pinned DolphinXR facing candidate

This source delta targets DolphinXR commit `85c4266b29eb308d173fefa0f21f5cb77df3516a`. It changes the immersive scene and VirtualScreen heading together, so recentering can face the wearer while retaining subsequent head movement. Eye and controller positions/orientations use the same inverse home yaw before existing camera offsets and anchor rotation. Native compositor poses remain in OpenXR reference space.

The separate native FlatScreen quad retains its existing reference-space placement. Full6DoF retains translation; Rotation3DoF retains its pinned head-center policy. None mode already synthesizes stationary eyes in game space, so it does not rotate those synthesized eye poses again; physical controller rays still use the home heading. These source policies need runtime verification, especially outside the primary Full6DoF path.

`XR_EXT_user_presence` is enabled only when advertised, and automatic capture requires system support plus a known-present event. Initial known presence and absent-to-present transitions queue one capture. Focus regain, repeated presence and ordinary head turns do not move the heading. A queued capture waits for focus, two eyes, valid position/orientation and a finite horizontal heading. Unsupported presence requires an explicit controller-confirmed recenter after putting on the headset. Focus is not treated as wear detection. A known-absent headset defers manual requests too.

The D3D VR-disabled boot emits an INFO OpenXR message that no session was initialized. File logging still requires the isolated user's Logs directory to exist before startup, plus enabled INFO categories. The launcher must use the exact CLI system name `Graphics`, not `GFX`.

## Apply and build

Only apply to an owned private clone under `.local`, already at the exact pin:

```text
python -B setups/dolphin-vr/facing/apply.py <absolute-private-source-path>
```

The script verifies the pin and checks every target against either pristine pinned text or its exact desired result before writing. A different existing edit is rejected. It modifies three existing native files and adds `Source/Core/VideoCommon/VR/ACFacing.h`; it does not change the separate draw-trace files. `dolphinxr-85c4266-facing.patch` contains only this delta. Keep the native draw-trace patch separately identified when building a combined candidate. No dependencies are downloaded by this script.

## Synthetic CPU checks

Run from the repository root with an already configured compiler, CMake and Ninja:

```text
cmake -S setups/dolphin-vr/facing -B .local/facing-ci -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
cmake --build .local/facing-ci --target ac-dolphin-facing-test
ctest --test-dir .local/facing-ci --output-on-failure
```

For a multi-configuration generator, add `--config Release` to the build and `-C Release` to CTest. The test target is `ac-dolphin-facing-test`; the CTest name is `ac-dolphin-facing-synthetic`. Windows uses `/W4 /WX`; GCC/Clang use `-Wall -Wextra -Wpedantic -Werror`. This target uses only synthetic math and state transitions. It does not load Dolphin, OpenXR, graphics backends, games or headset services.

Coverage includes cardinal headings, invalid/vertical orientation, queued manual requests, unsupported/unknown/absent/present states, don/redon, focus loss, invalid poses, no reanchor on head turns, asymmetric stereo offsets, pitch/roll preservation, corner-ray registration, and a nonidentity camera-anchor regression. The anchor fixture detects incorrectly composing `A * rawR` instead of `A * H * R`.

## Acceptance boundary

The installed MSVC compiler built the patched native source and passed synthetic tests. No candidate runtime or headset sitting has run in this lane. Source/build success does not establish launch, presence support, stable facing, aiming, logs on disk, image quality, mission rendering or headset acceptance. A bounded sitting must explicitly identify the pin, separate trace/facing patch hashes and executable hash. Preserve the original release and earlier source candidate.
