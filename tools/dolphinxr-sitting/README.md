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
