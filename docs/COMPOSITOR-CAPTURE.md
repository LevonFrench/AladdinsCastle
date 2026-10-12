# Compositor snapshot contract

The explicit Hub diagnostic `--capture-compositor-dir <absolute fresh directory>`
exports `left.png`, `right.png` and `receipt.json` below the selected catalog's
`.local` directory. It reads OpenVR compositor mirror textures, rather than the
Hub source texture. The mode loads no game/catalog model, creates no dashboard
overlay and does not activate Hub over another scene. An existing SteamVR server
is required; it performs no screenshot-library operation or settings change.

`--compositor-dashboard hidden|visible|any` checks observed dashboard state;
`any` is the default. A mismatch or visibility change refuses the snapshot.
The exporter accepts only bounded desktop GL linear RGBA8 mirrors, reads their
actual sizes and reverses bottom-first rows once. API failure, invalid pixels
or cleanup failure never produces an accepted pair. Shared textures are locked
for each read, unlocked and released through OpenVR on every exit. They are
never deleted with GL texture deletion. Borrowed GL read/pack/texture bindings
are restored through the existing readback guard.

Separately, `--overlay --show-dashboard` explicitly requests this Hub's own
dashboard key. It is not a presentation proof and is never triggered by the
compositor snapshot mode. The public API has no HideDashboard operation here;
the exporter checks hidden state rather than trying to hide other interfaces.
The accepted runtime order is a Pinball scene with the hidden guard first,
then Hub dashboard presentation with the visible guard.

The snapshot is one-shot. A supervisor must bound any blocking native driver
call; `--quit-after-ms` is refused for this mode rather than promising that a Qt
timer can interrupt a blocking GL call. The receipt labels the compositor
endpoint, eye order, dimensions, format and dashboard state. It explicitly
requires visual content/orientation review, disclaims exact frame synchronization
and makes no physical headset or controller acceptance claim. Runtime availability
under the null driver and intended scene inclusion remain separate live gates.

CPU tests exercise fake mirror acquisition/read/release errors, both-eye cleanup,
fresh private output and command-mode guards without creating a GL context or
calling VR_Init. Their results must not be reported as actual compositor pixels.
