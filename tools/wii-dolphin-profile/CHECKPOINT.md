# Ordinary Dolphin fallback checkpoint

Fresh isolated one-gun and two-Remote profiles, per-game PAL selectors,
loopback DSU, absolute aim and physical Motion Input are implemented. One gun
uses the opposite hand for Nunchuk; two Remotes use separate hand feeds with
same-hand Nunchuk attachments. Cheats, analytics and automatic updates are
disabled in newly generated profiles. Existing/live user directories are never
regenerated automatically. The command printer does not launch applications.

The original Windows SBS theatre helper builds from the existing OpenVR SDK.
It captures only the selected Dolphin client, rejects ambiguous/occluded or
stale windows, bounds image memory/run duration, checks black pixels, and
cleans up its fixed overlay. Explicit recenter publishes a shared HMD matrix
for the lead's bridge. It is a3D theatre fallback, not the true-3D scene target.

Verified: eight Python profile tests; MSVC Release build with warnings treated
as errors; four headless CTests covering plane/aspect/selection/pixel bounds,
invalid run requests and missing/present native DLL probes. No VR_Init, game
launch, headset run or actual game capture was performed by this lane.

Open: bridge consumption of shared calibration; actual Dolphin config loading,
GPU-render capture through GDI, colour/eye order/orientation, headset stereo,
physical motion signs/feel, co-op and progression. Live work belongs to the
lead's coordinated sitting. See README.md and theatre/README.md for operation.
