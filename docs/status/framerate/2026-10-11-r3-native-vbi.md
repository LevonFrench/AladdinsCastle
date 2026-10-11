# R3 native VBI preparation — 2026-10-11

This lane preserves PR 39 and continues on codex/r3-native-vbi-phase1 from next
990cee4. The native switch is Graphics.VR.ForcedVBIFrequency: Auto is -1, Off is
0, with fixed choices 72, 90 and 120. Source 85c4266 gates its timing effect on
EnableOpenXR, overrides the Advanced VBI percentage, and divides the selected
frequency by 59.94. PAL60 and the actual video mode still need counter evidence.
Auto needs a positive startup XR refresh value; a VR-off desktop run cannot
validate it. AutoVBIFromHMD is explicitly false so Off cannot inherit the legacy
90 fallback. The older harness's GFX CLI namespace was silently ignored by this
parser; it is corrected to Graphics. No old profile is runtime evidence.

Ten fresh profiles are prepared for RBUP08 and SBDP08 across Off, Auto, 72, 90
and 120. Cheats remain off, with one fixed graphics/input configuration. The
generated flat profile is verified against its shared manifest, and the lead's
scripted parser checks the private 180-second Cross/A boot candidate without
starting a socket. Those button timings are untested; no title or mission is
claimed. Native collection requires the lead's quiet runtime slot, approved XR
conditions and ready DSU server. Later modes require a reviewed Off baseline
with matching scene, revision, build and input hashes. One measured stretch is
bounded to 60 seconds inside an attempt of at most 180 seconds; configured time
does not establish real game speed. The lead supervises capture and system
prompts and owns the server; the collector sends no OS input and closes its own
process.

Twenty-five Python tests pass. Windows PowerShell 5.1 and 7 pass lease/legacy-code,
native admission, disabled-code, 28 active-build refusals, an idle control,
argument-mode/quoting and hash checks. The 5.1 retry passed without a policy
change after an initial sandbox denial. These prove source admission and
preparation only. The later native queue includes RHOP8P, RZJE69, R2VE01,
RZPP01 and SQDE8P; WBFS identification remains Hub-owned. Dead Space's separate
Action Replay preview admits only RZJE69 revision 0, leaves its code disabled
and cheats off, and cannot launch through this collector. Its direct source
fetch is unavailable; transcription review and a same-scene baseline remain
required after the native comparison.

No game-rate measurement, actual Auto resolution, PAL video-mode proof,
same-scene game-speed comparison, CPU/GPU emulation telemetry, audio/QTE/HUD
check or headset acceptance occurred. A quiet runtime slot, observed scene
cues, effective-switch/counter evidence and the lead's XR conditions are the
remaining measurement dependencies. Private profiles, scripts and receipts
remain outside Git; recommendations about per-refresh gun/camera rendering
remain open. No download, installation, system runtime change, mouse/keyboard
input or account activity occurred.

Independent PR 51 review found that ready=true/port and an arbitrary live lease
PID could admit stale DSU readiness. The correction binds the producer PID,
timeline hash, port and a fresh per-run token to both attachment and lease, then
checks the live interpreter path/hash. Two real-collector regressions reject the
old receipt and unrelated PID; twenty additional mismatch cases and matching
controls pass without launching anything. Old preparations lack this contract
and remain historical. The lead owns the matching scripted.py protocol change;
integration and runtime remain held pending review.

Per-game attachment now verifies the exact game manifest paths and both generated
control hashes, keeping scripted DSU active and the native pair as a reference.
Dead Space's fresh private identity receipt matches RZJE69 revision 0. Its native
Off baseline and disabled-code preview are staged; attachment does not activate
the code or make its unavailable primary source verified. Canonical bytes must
come from the corrected Guns checkpoint before future runtime.
