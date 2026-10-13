# Runtime gun OpenGL renderer

`libacvr/src/gun_gl.hpp` implements a programmable desktop OpenGL 3.3 gun pass
under MIT. It shares the compatibility context required by the first System 22
world path. This is compiled source with mock-dispatch coverage; no real GPU
execution or model appearance is accepted yet.

## Provider integration

Enable `ACVR_BUILD_GL_GUN_RENDERER` together with the existing JSON/TOML include
options. The module uses installed GL headers and resolves every call through
`acvr_graphics_device.get_proc`; it never loads a driver/library or creates a
context. Host initialization calls `GunGlRenderer::initialize` with its current
desktop compatibility context. `Host::draw_gun` forwards the same eye and
`GunDraw` received after world rendering. Shutdown must happen on the owner
thread with that context current, before context destruction. The destructor
does not issue contextless GL calls.

Targets are complete nonzero runtime-owned FBOs with matching colour and depth
textures, one layer and one sample. Supported colour formats are RGBA8 and
SRGB8_ALPHA8. The renderer verifies both attachment names and preserves their
contents: its dispatch table has no clear, detach, swap, flush, readback or
texture-deletion entry. Native GL3.3 forward-depth perspective is required;
GLES, core-only contexts, reversed depth and laser modes currently return
UNSUPPORTED. This is a deliberately declared implementation boundary.

CPU packing is the actual upload path. It selects one LOD, applies prepared
scene transforms, multiplies vertex/material colour by linear body/accent tint,
and sorts translucent triangles per eye. Opaque triangles write shared depth;
translucent triangles use premultiplied blending without depth writes. Dark/glass
material roles keep their own base factors. The initial shader is unlit; material
lighting and laser line/hit-dot rendering remain open.

The pass saves/restores program, VAO, array buffer, draw framebuffer, attribute
state and relevant modern enable flags. Viewport/scissor are eye-local. Linear
gun colours use framebuffer sRGB conversion for sRGB targets. Existing caller
errors are reported before mutations; partial draw failures restore state and
propagate failure so the host can submit zero layers. API references:
[Khronos reference pages](https://registry.khronos.org/OpenGL-Refpages/).

## Checks and manual GPU gate

`acvr_gun_gl_mock` is a CPU test. It simulates shader/resource creation, uploads,
draw commands, attachment checks, failure paths and caller state; it never
imports a GL library or creates a context. Packing tests exercise transforms,
tints, LOD selection and transparent ordering. Those are not pixel comparisons.

An optional Windows-only `ACVR_BUILD_GPU_SMOKE` target uses the installed Qt
Gui/OpenGL SDK to create a hidden offscreen compatibility context. It is never
registered with CTest or enabled in hosted CI. Before platform initialization,
it requires `--owner-approved-gpu-smoke <private-output-directory>`; that flag
records an approval and does not grant it.

After explicit owner permission, the check would render only an original
synthetic triangle against a synthetic depth barrier for two eye transforms,
verify blocked/unblocked pixels and state restoration, then save a tiny capture
and private driver receipt. No game files, headset, SteamVR, installation,
download or Steam write is involved. The executable is compiled, not run.
Passing it would establish only this synthetic GPU path, never Time Crisis,
exported asset appearance, XR submission or headset acceptance.
