# DolphinXR sitting kit

Windows PowerShell 5.1 is the supported operator shell. The public runner takes
explicit executable/hash/disc/private-state paths; owner-specific wrappers stay
under `.local`. Nothing installs, downloads, changes Steam, or chooses a system
runtime. `-CheckOnly` checks the executable and a 32-byte Wii header without
writing settings. `-PrepareOnly` prepares settings without starting any process.

Default and Eager modes consume the exact shared `shell-stick/1` native
`split-right.ini` bytes and verify their generated manifest hash. The pinned
native route uses one right-hand Remote and a left-hand attachment. Left-hand
and multiple-player routing require separate native admission; swapping button
expressions alone does not change the source's IR override.

Settings live in new, explicitly managed `User-v2-<Mode>` directories. Older
sitting directories are not migrated. Before each preparation, Dolphin must be
closed. Previous INI files are backed up; owned keys are repaired while unrelated
keys and game saves remain. The native bindings are a managed whole-file payload.
The kit creates `Logs` before launch, because the pinned logger opens its file
once and does not create that parent directory.

Both stick clicks request recenter; Shift+R remains available. Isolated hotkeys
do not require the desktop window to have focus. The 15-second startup delay is
time to put on the headset, not wear detection. The original release's immersive
recenter does not reset yaw; a separately identified source candidate is needed
for that fix. Normal head turns must never continuously reanchor the world.

| Quality option | Internal resolution | MSAA | Anisotropic filtering |
|---|---:|---:|---:|
| Native | 1x | Off | 1x |
| Sharp (candidate default) | 4x | Off | 8x |
| SharpAA (desktop comparison) | 4x | 4x | 16x |

The kit explicitly sets `GFX.ini` `[VR] ResolutionScale = 1.0` and the matching
launch override. This uses the OpenXR runtime's recommended per-eye dimensions
without downscaling. Those lens-corrected render dimensions can differ from the
headset's physical panel resolution. They are independent of the game's internal
resolution (4x is 2560x2112); the native OpenXR log records recommended and actual
eye dimensions when a VR session is initialized.

These are configuration candidates, not measured performance presets. Desktop
mode explicitly passes `Graphics.VR.EnableOpenXR=False` and stereoscopy 0.
`GFX` is the file name, but **not** a valid command-line system prefix. Compare
the same menu/mission before picking AA; unsupported MSAA can fall back to off.

Runs retain before/after configs, launch identity, exact owned PID, output,
`dolphin.log`, and a separate log check. Old logs are archived before starting so
a new run cannot pass on stale OpenXR lines. Windows stdout is not the native
log sink. A VR-off OpenXR-category diagnostic must explicitly say no session was
initialized; it is not headset evidence. Only the owned process is stopped at
the bounded deadline or interruption.

Run `Test-Sitting.ps1` in Windows PowerShell 5.1 for synthetic admission,
configuration, running-process refusal and log checks. No game or app launches
in those tests. The real desktop run and both headset modes are separate checks.

R3 adds explicit native options: `-ForcedVBI Auto|Off|72|90|120`,
`-Mirror Both|Left|Right|None`, `-ControllerAnchor`, `-CameraAnchor` and
`-CameraSmoothing` (0..1, default 0.85). Off is the conservative VBI default;
it also clears the legacy AutoVBIFromHMD flag. Native Forced VBI is conditional
on OpenXR being enabled. Auto needs the runtime's startup refresh measurement;
a VR-off run cannot prove it. Anchor switches still require an applicable
object/profile selection; controller anchoring supplies position, not a complete
tracked gun implementation. No tuned per-game profile is claimed by these switches.

For unattended input, pass `-DsuProfile` pointing at the shared generated
`flat-dolphin/split-right.ini` and optionally `-DsuPort` (26760 by default).
The manifest is checked before any settings write. This selects Source 1 and
loopback DSU independently of Default/Eager/Desktop rendering, with background
input enabled. Run the bounded DSU service described in `tools/wii-bridge/SCRIPTED.md`;
it never takes over the OS mouse or keyboard. Starting this runner does not start
that service. The supervisor must own and close both processes.
