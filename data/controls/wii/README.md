# Shell and stick Wii controls

`shell-and-stick.toml` is the single control policy for all five tier 1 Wii games.
It generates the controls-page route variants, native DolphinXR controller
settings, ordinary-Dolphin DSU settings, and bridge action profiles.
`remote-nunchuk.toml` is the historical pre-sitting mapping and is not consumed
by this compiler. Per-game TOMLs contain layout/action questions; confirmed
actions require explicit `confirmed=true` and screen-evidence references.
No game screens or content are shipped.

| Part | Split hold controller input |
|---|---|
| Remote B trigger | Firing trigger |
| Remote A | Firing primary face button |
| Remote d-pad | Firing thumbstick directions |
| Remote plus / minus | Firing grip + primary / secondary |
| Remote 1 / 2 | Firing grip + thumbstick up / down |
| Remote Home | Firing grip + thumbstick click |
| Remote shake / swing | Physical shell motion, plus secondary / stick click |
| Stick controller analog stick | Off-hand thumbstick |
| Stick controller Z / C | Off-hand trigger / grip |
| Stick controller shake / swing | Physical off-hand motion, plus primary / secondary |

Extra Remote bindings suppress the ordinary action while the modifier is held.
Both thumbstick clicks reserve recenter and suppress game buttons/fallbacks.
Handedness swaps the two physical hands. Native aim additionally requires its
matching `Android/QuestLeftHanded` companion setting; native dual-player export
is rejected because the pinned native aiming hand is global.

Holds are split (default), docked at shell `grip_two`, and two-remotes (two shells,
no accessory). Dock thresholds and grab modes are data for runtime integration.
In DSU dual mode each hand controls its own stick/C/Z; click is the Remote extra
button/d-pad layer. Generated DSU data remain **planned** in the controls page
until the lead's bridge and ordinary-Dolphin profile consumers use these bytes.
Generation does not prove effective fallback bindings or independent native aim.

`python tools/wii_controls.py --write` rebuilds all generated data. Without
`--write` it verifies freshness. `--route dolphinxr --game <id> --json` selects
the matching native view. The native resolver accepts a typed
`ControlsResolveOptions.route`; unknown or mismatched routes fail closed.
The view has per-model image/callout anchors and boolean chord highlighting.
No Python or input emitter runs in the player-facing controls page.

See `PROFILE-CONTRACT.md` for exact output paths/API. Installer and sitting
preparation consume identical generated native bytes. Physical recognition,
actual held rendering, docking, aiming and headset acceptance remain separate.
