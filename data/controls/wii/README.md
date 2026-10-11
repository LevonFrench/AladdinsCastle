# Wii Remote and Nunchuk mappings

Tier 1 has one base Remote/Nunchuk set, four game action overlays, and a separate
character-and-pointer set for Sin & Punishment. These original data describe
parts, actions and controller bindings; they emit no inputs. The initial world
gun is the built `generic-pistol`. A profile may select another model.

`tools/wii_controls.py --write` compiles these TOMLs into `view-catalog.json`.
Run without `--write` to check that the committed catalog matches its sources.
Both the native Hub resolver and Python export consume the same compiled data.
Wii semantics remain outside the arcade libacvr input vocabulary.

| Part | VR input | DSU / Dolphin input |
|---|---|---|
| Remote pointer | Gun aim ray | Right stick, absolute pointer |
| Remote B | Trigger | R2 |
| Remote A | A / X | Cross |
| Nunchuk Z | Grip | L2 |
| Nunchuk C | B / Y | Circle |
| Nunchuk stick | Thumbstick | Left stick |
| Remote + | Application menu | Options |
| Remote - / 1 / 2 | Keyboard or custom binding | Share / Square / Triangle |
| Remote directional pad | Keyboard arrows or custom binding | DSU directional buttons |
| Remote movement | Physical controller motion | Accelerometer in g; gyro in degrees/second |
| Nunchuk movement | Nunchuk-hand physical motion | Nunchuk accelerometer binding |
| Seated shake / swing | R / V in bridge; Ctrl+Alt+R / V in Dolphin | Inject motion on the same accelerometer/gyro path |
| Laser | Thumbstick click | Local action, no Wii button |

Slot 0 is right, slot 1 left. With one gun, the other hand is the Nunchuk;
in co-op each hand is its own Remote with the same set. The view switches those
roles and mirrors A/B to X/Y. Physical axis signs, cross-hand profile integration,
fallback waveform recognition and laser runtime implementation need their
separate bridge/profile checks; the table is not a claim that they already work.

Dolphin Motion Input is the single motion source: leave simulated Shake and
Swing bindings empty. Relative pointer input and auto-hide are off. DSU's pointer
is limited to 256 steps per axis and has no rumble. The fifth setup session owns
`tools/wii-dolphin-profile/`; the lead owns `tools/wii-bridge/`.

The owner's defaults in `docs/tasks/m2/W-wii-chronicles.md` are implemented.
Exact game actions that the brief did not verify remain explicit questions in
each game overlay for the first sitting, rather than invented bindings. Sin &
Punishment deliberately reserves dodge, melee, lock-on and charged-shot choices
for that owner sitting; it has character movement and no generic reload row.

Primary-manual lookup did not settle those questions: Nintendo's linked Sin &
Punishment PDF returned 404; the retrieved text for the Capcom Darkside-labelled PDF described unrelated
Resident Evil controls; the Umbrella manual mirror yielded no manual body.
No manual content or artwork was imported. No disc files were read.

Sources: the owner brief above, the agreed lead DSU mapping, and the protocol/
Dolphin references documented in `tools/wii-bridge/README.md`. Nintendo's manual
index: https://en-americas-support.nintendo.com/app/answers/detail/a_id/16890/p/172
