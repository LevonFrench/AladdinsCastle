# R2 viewer checkpoint

Windows.Graphics.Capture now targets the selected Dolphin window and crops to
its client. GDI, overlap rejection and the cursor exception are gone; process,
window identity and minimisation guards remain. A capture-only mode saves two
private PNGs without initializing SteamVR. Two compositor frames at least
1 second apart must have different RGB; plain grey and alpha-only noise fail.
Frame-size, crop and output-path checks remain bounded. The existing D3D11
BGRA8/Gamma overlay submission is preserved, with hardware acceptance open.

Fresh profiles set a 1280 x 720 render window with auto-size disabled. Pinned
Cursor source confirms IR/Total Yaw and IR/Total Pitch are full degree ranges:
77.319617 and 48.455491 degrees match a 3.2 x 1.8 m plane at 2 m. IR/Hide uses
Square independently on each Remote. Existing profiles are preserved and prior
published manifests remain recognized. Four-corner and origin checks remain live.

MSVC Release builds with strict warnings. Fourteen Python profile tests and
eight headless CTests pass, including full-size software crop/PNG round-trip,
D3D upload, frame timing/difference, ownership and lifecycle rejection cases.
The second proven frame seeds the overlay before display. The live theatre loop
now polls WGC without waiting for a fresh frame, so it
services recenter and quit promptly and exits 0 at stop or deadline after the
startup gate. The startup gate still fails if two fresh, different frames are
never proved. A CPU regression covers empty-pool wait and stop/deadline decisions.
The new integration build and all eight native headless CTests pass. These are
software-only checks; no real window capture, game, desktop or headset run
occurred in this integration check.

Earlier live attempts failed to establish accepted game SBS pixels or a headset
image. Desktop SBS proof remains pending a coordinated turn.
Native DolphinXR first boot is not blocked by this fallback. No tool install,
new download, system setting or Steam write occurred. This checkpoint claims
source/software readiness; real game pixels and headset acceptance stay open.
