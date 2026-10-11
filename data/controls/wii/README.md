# Wii Remote and Nunchuk mappings

Tier 1 has one base Remote/Nunchuk set, four game action overlays, and a separate
character-and-pointer set for Sin & Punishment. These original data describe
parts, actions and controller bindings; they emit no inputs. The initial world
gun is the built `generic-pistol`. A profile may select another model.

`tools/wii_controls.py --write` compiles these TOMLs into `view-catalog.json`.
Run without `--write` to check that the committed catalog matches its sources.
Both the native Hub resolver and Python export consume the same compiled data.
Wii semantics remain outside the arcade libacvr input vocabulary.

`data.route = "flat-dolphin"` identifies these DSU mappings. The selected
`dolphinxr` route configures native controller mapping inside DolphinXR and
must not display these rows. The Hub owns selected-route gating; missing,
unknown or mismatched route keys clear the DSU view with an explanation.

| Part | VR input | DSU / Dolphin input |
|---|---|---|
| Remote pointer | Gun aim ray | Right stick, absolute pointer |
| Remote B | Trigger | R2 |
| Remote A | A / X | Cross |
| Nunchuk Z | Grip | L2 |
| Nunchuk C | B / Y | Circle |
| Nunchuk stick | Thumbstick | Left stick |
| Remote + | Binding pending: left menu or accessibility fallback | Intended Options; right Touch system button unavailable |
| Remote - / 1 / 2 | Planned keyboard or custom binding | Intended Share / Square / Triangle; bridge and profile omit these |
| Remote directional pad | Planned directional binding | Intended DSU directional buttons; producer emits no bits or keyboard arrows |
| Remote movement | Physical controller motion | Accelerometer in g; gyro in degrees/second |
| Nunchuk movement | Nunchuk-hand physical motion | Nunchuk accelerometer binding |
| Seated shake / swing | Player 1 only: R / V in bridge; Ctrl+Alt+R / V in Dolphin | Inject motion into DSU 0 on the same accelerometer/gyro path |
| Laser | Planned thumbstick click | Local action, no Wii button; visible laser not implemented |

Logical `slot` / `player` identifies the player. Physical DSU device 0 is right,
device 1 left. Each binding has `physical_dsu_slot_by_mode`: one gun uses Remote
0 / Nunchuk 1; two Remotes use player 1's Remote 0 / Nunchuk 0 and player 2's
Remote 1 / Nunchuk 1. Local laser/owner choices have no physical DSU slot.
`nunchuk_hand_by_mode` says other hand for one gun, same hand for co-op; two
hands cannot supply four independent poses. The view derives hands from those
physical assignments and mirrors A/B to X/Y. Left-primary selection is not
implemented by the current profile generator; selecting it marks the setup
unavailable and retains the actual physical device assignments.

D-pad, minus/1/2 and laser are planned, and Remote + has a pending accessible
binding. The R/V producer only targets physical slot 0: `fallback_player = 0`
keeps both keyboard fallback rows scoped to player 1, with player 2's generated
rows unavailable even in co-op. Player 2's physical-motion mapping still targets
slot 1. The view shows plain reasons and does not highlight unavailable controls
as active inputs. DSU IDs and implementation reasons stay in data/docs; the
one-gun notice reads "Right hand: Wii Remote. Left hand: Nunchuk."
Physical axis signs, fallback waveform recognition and game actions still need
separate bridge/profile checks. The profile lane's current game auto-selectors
cover the two Chronicles game IDs, not acceptance of all five sets.

Dolphin Motion Input is the single motion source: leave simulated Shake and
Swing bindings empty. Relative pointer input and auto-hide are off. DSU's pointer
is limited to 256 steps per axis and has no rumble. The fifth setup session owns
`tools/wii-dolphin-profile/`; the lead owns `tools/wii-bridge/`.

The owner's defaults in `docs/tasks/m2/W-wii-chronicles.md` are authored here;
implementation gaps above remain explicit.
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
