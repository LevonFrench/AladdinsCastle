# MAME light-gun theatre viewer

GPL-3.0-or-later viewer on the shared publish presenter. The unmodified generic
MAME publisher supplies ACVRMG v4 mono frames and calibrated gun ports. Build
the viewer with `build-viewer.ps1 -OpenXRInclude <existing-header-directory>
-OutputDirectory <private-output> -RunGatePath <headset-marker>`. Existing
MinGW tools are used; nothing is downloaded. CMake is an alternative with
`ACVR_OPENXR_INCLUDE_DIR` pointing to existing headers.

`acvr_mono_frames.h` is the exact BSD-3-Clause wire header from the generic
publisher. The viewer validates magic/version, dimensions, stride and physical
aspect under its named mutex. It never rotates the picture again: cabinet
orientation has already been applied. Pixels, telemetry and input are exchanged
by lock-or-skip; a skipped frame retains the last complete picture.

The shared presenter's separate theatre mode locates the current eye views and
tracked right aim/grip, then draws a 3.2 m flat screen at 2.5 m, the GLB gun and
the aim dot into one opaque projection layer with two depth-tested eye images.
Each target has the runtime's own recommended dimensions. There are no quad
layers in this mode. Screen height uses physical aspect, including non-square
pixels. Native pixels are integer-prescaled by nearest blit, then sampled with
linear filtering and mipmaps. The shared renderer and GLB/metadata loader come
from this project's PS1 theatre implementation; their single shared copy is in
`libacvr/publish-presenter`. CRT scanlines are optional and off by default.

Right trigger shoots; off-screen trigger reloads through MAME's normal gun
mapping. Right A starts, right B inserts a coin, right grip is button 2. Hold
left Y for three seconds to request normal exit (left B on the alternative
controller profile). Home recentres. Focus loss releases input and hides the
tracked gun; rendering does not require focus. MAME's 250 ms heartbeat expiry
protects against a dead viewer. A stop file requests the same normal MAME exit
as the hold gesture. Vibration is one pulse per trigger edge; it does not claim
the game acknowledged a hit.

`launch-mame-gun.ps1` accepts explicit existing executable, media, gun and
private writable paths. It supports the five starter game keys and keeps all
MAME cfg/NVRAM/state/output files in private directories. It launches both
children hidden at BelowNormal priority. It waits for normal exit before an
owned-process fallback; a forced close is reported and cannot establish saves.
The launcher opens a runtime for play; never use that route for offline proof.

For proof, launch MAME hidden with `MAME_VR_PUBLISH=1`, `MAME_VR_GUN=1` and
`MAME_VR_CROSSHAIR=1`, no video/sound, bounded duration and private writable
paths. Run the viewer with `--pid <publisher-pid> --model <local-glb>
--metadata <local-toml> --seconds <wall-budget> --offline-proof <private-folder>
--script`. This path never starts the presenter or opens an XR loader. Fixed
head/hand poses aim at the picture centre; scripted coin/start/shot/off-screen
and second-button input uses the same v4 mapping as play. At 36 emulated seconds
it saves the original mono image and picture/front/behind/dot views of both
eyes, then requests MAME's clean exit. The JSON records MAME's own calibrated
crosshair, integer scale, occlusion and GL results. Pixel fixtures do not prove
headset tracking, comfort, a successful hit, reload in every game or real saves.

`validate_proof.py <private-capture-directory>` (Pillow/NumPy) compares both
rendered eyes against the original mono picture: upright mapping must correlate
above 0.97 and beat the vertically flipped alternative. Near/behind gun masks
and the projected dot-to-MAME-crosshair distance are checked, allowing the gun
board's half-pixel quantization. `test-viewer.ps1` runs the new mode against
injected API functions and native hidden GL: per-eye recommended dimensions,
actions, focus release, current poses, a no-draw update, and bounded image wait
retry. No test opens a loader or runtime.

The original and shared Model 2 viewer sources remain unchanged. Their existing
build and test scripts still select the same implementations. MAME, the Hub,
catalog, PS1 and other emulator clients are outside this viewer change.
