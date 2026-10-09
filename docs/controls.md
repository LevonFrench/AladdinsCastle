# Controls

All input comes from the motion controllers and the headset. Gamepads, wheels and real light guns are optional extras. Every behavior below is a field in a control set file (see [config-spec.md](config-spec.md)), not code.

## 1. Light guns

### 1.1 Aiming in true 3D

The game itself still thinks it is talking to a light gun pointed at a flat screen. It wants a screen coordinate. The VR backend renders the game's world around the player, so we have to turn a ray in VR space back into the screen point that the game's own camera would see.

Per frame:

1. **Controller ray.** Take the aim pose from OpenXR (`/input/aim/pose`) and apply the per-hand gun-angle offset, as DR-89's Time Crisis VR does with its -60..+60 degree setting. Transform it into game world space using the current *world anchor*, which is the transform that puts the game camera at the player's head.
2. **Find what the player is pointing at.** Intersect the ray with the depth buffer of the stereo frame we just rendered, or with the reconstructed geometry. The hit point H is the thing the player sees under their laser.
3. **Project H through the game's camera.** `s = Viewport(Projection_game * View_game * H)`. This is the screen point that hits the same object in the game's own logic. Projecting the hit point rather than the ray direction makes it parallax-correct even though the gun is not at the game camera's position.
4. **No hit (sky, far away):** take a point at a fixed far distance along the ray and project that.
5. **Map to the gun range.** Convert `s` from normalized screen space into the device's coordinate range (GunCon 2 counts, MAME analog port range and so on) with the game's calibration from its game file.
6. **Outside the original view:** if `s` falls outside the game's frustum, the shot counts as off-screen. Most arcade games treat that as a reload.

The game camera's projection must match exactly. DR-89's v0.8.3 fix found that a 500 px focal length versus the arcade's real 772.5625 px made the flat layer about 1.545 times too wide. That broke aim away from the centre. Every backend must report its real projection per frame.

### 1.2 Gun actions

| Action | Default binding | Options |
|---|---|---|
| Fire | Trigger of the hand holding the gun | Analog threshold, auto-fire rate |
| Reload | Off-screen shot | `button` (A/X), `offscreen`, `point_down`, `holster` (put the gun to your hip), `flick` (quick wrist snap) |
| Pedal / cover (Time Crisis) | Physical ducking | `duck` (head below a set fraction of standing height), `grip_hold`, `grip_toggle` |
| Special weapon / grenade (HotD 4 shake, Let's Go Jungle) | Shake gesture | `shake` (acceleration threshold), `button` |
| Weapon switch (Ghost Squad, Razing Storm) | B/Y | Any button |
| Start / coin | A/X, or a coin slot on the cabinet in the hall | Coin animation optional |
| Recenter | Hold both grips | Any chord |

- **Hand switching:** pulling a trigger moves the gun to that hand (as in DR-89). Two players on one headset play akimbo, as in VC2VR. In true 3D, the second controller becomes player 2's gun.
- **Gun model per game:** GunCon blue, Virtua Gun, HotD pistol, Ghost Squad machine gun and so on, all set in the cabinet file. Recoil moves the model without moving the aim ray.
- **Laser / crosshair:** off, laser, dot or game-native crosshair, each setting saved per game.
- **Haptics:** fire pulse, recoil from the game's output events (MAMEHooker-style lamp/solenoid outputs or decompiled hooks), and a damage hit.

### 1.3 Physical cover (Time Crisis, Crisis Zone, Razing Storm)

- Calibrate standing head height on first launch and on recenter.
- Duck threshold defaults to 80% of standing height, with hysteresis so you don't flicker in and out of cover.
- Leaning sideways can optionally count as cover too, for seated play.

## 2. Racing: ghost controls

Every racing cabinet has a **control set**: a list of controls, each with a shape, a position relative to the seat, a range of motion and a mapping to game inputs. In VR they appear as faint see-through *ghost* copies of the real cabinet controls. Reach out and grab one with the grip button to take control. Let go and it springs back the way the real control would.

### 2.1 Control types

| Type | Used by | Interaction | Output |
|---|---|---|---|
| `wheel` | Most racers | Grab the rim with one or both hands. With two hands, the angle comes from the line between them. With one hand, it comes from the hand's angle around the hub. | Steering axis; `range_deg` (270/360/540/900) maps to the game's axis |
| `shifter_hl` | Daytona, Rave Racer, Ridge Racer | Two-position lever: push forward/back | Low/High button |
| `shifter_h` | Sega Rally 2, F355, Le Mans | Grab the knob and move it through the H gate, with magnetic gate snapping | Gear 1-6, N, R |
| `shifter_seq` | Sega Rally, Initial D | Push forward or pull back | Shift up / down |
| `pedal` | All | Analog trigger, no grab needed | Throttle (right trigger), brake (left trigger), with curve and deadzone |
| `handlebars` | Manx TT, Cyber Cycles, Harley-Davidson | Grab both grips; turning and leaning both count | Steering + lean |
| `lean_body` | Manx TT, Alpine Racer, Top Skater | Lean of headset and hips relative to the seat | Lean axis |
| `pump` | Prop Cycle | Pump the controllers like pedals; speed of the motion = pedal speed | Pedal speed axis |
| `stick` | Tokyo Wars, plane and boat games | Grab-and-tilt, VTOL VR style | X/Y axes |
| `button` | View change, start, nitro | Touch or press with a finger (poke) | Button |
| `lever` | Hydro Thunder throttle, train games | Grab and slide along a rail | Axis |

### 2.2 Grab rules (borrowed from VTOL VR)

- **Deliberate grip only.** Input is bound only while the grip is held.
- **Soft lock.** Once grabbed, your hand may drift off the exact control shape without losing it. Input stays bound until you let go.
- **Pick-up radius** and **hand ghost:** the hand model snaps onto the control and the controller's real position shows faintly.
- **Return spring:** on release, the wheel recentres at a speed set per cabinet. Shifters stay in gear.
- **Haptic detents:** centre of the wheel, gear gates, rev limiter and bumps, plus the game's force feedback output (Ace Driver has it, per namco22-decompile) shown as vibration.

### 2.3 Comfort

Racing in VR is the biggest motion sickness risk. All of these are options:

- A cockpit anchor: the cabinet, seat and dashboard are static geometry around you.
- Speed-based vignette.
- Horizon lock: optional roll damping for games that roll the camera.
- Seated mode and standing mode.
- Fade or blink on hard camera cuts.
- The "ride the screen" fallback: the game on a large curved screen in front of the cabinet seat.

## 3. Hall controls

| Action | Default |
|---|---|
| Move | Left stick smooth locomotion, or teleport arc (option) |
| Turn | Snap turn on the right stick (option: smooth) |
| Interact | Point the laser and pull the trigger, or touch directly |
| Menu | Left menu button: game list, settings, search |
| Insert coin / play | Pull the trigger at the cabinet screen, or drop a coin in the slot |
| Exit game | Hold the menu button for 1 s, then confirm |
