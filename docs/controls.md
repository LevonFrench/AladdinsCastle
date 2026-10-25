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
| Pedal / cover (Time Crisis) | **Button: hold either grip = pedal down (out of cover, shoot); release = take cover / reload** | `hold_to_shoot` (default, mirrors the arcade pedal), `hold_to_cover` (inverted), `toggle` |
| Special weapon / grenade (HotD 4 shake, Let's Go Jungle) | Shake gesture | `shake` (acceleration threshold), `button` |
| Weapon switch (Ghost Squad, Razing Storm) | B/Y | Any button |
| Start / coin | A/X | Any button |
| Recenter | Hold both grips | Any chord |

- **A gun in each hand:** in every two-player gun game the right hand is player 1's gun and the left hand is player 2's; player 2 joins with that hand's Start button. With one gun out, pulling the other trigger moves it to that hand. Rules and exceptions: [guns.md](guns.md) §1.
- **You see the gun you hold:** our own 3D model of the game's gun (a GunCon lookalike, a Virtua Gun lookalike, the cabinet's pistol, shotgun or mounted machine gun) is drawn at the controller. Recoil moves the model without moving the aim ray. Library and assignment: [guns.md](guns.md) §2.
- **Laser / crosshair:** off, laser, dot or game-native crosshair, each setting saved per game.
- **Haptics:** fire pulse, recoil from the game's output events (MAMEHooker-style lamp/solenoid outputs or decompiled hooks), and a damage hit.

### 1.3 Cover button (Time Crisis, Crisis Zone, Razing Storm)

- **No physical ducking** (owner decision, 2026-10-08). Cover is always on a button.
- Default `hold_to_shoot`: holding either grip is the arcade pedal pressed (you're out of cover and can shoot); releasing it takes cover and reloads, exactly like letting go of the real pedal.
- Options: `hold_to_cover` (inverted) and `toggle`. Two-pedal games (Time Crisis 5) put the second pedal on the other grip.
- Haptic tick when entering and leaving cover.

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

## 2.4 Platform input notes (research 2026-10-08)

- **API:** OpenXR actions in libacvr (works on SteamVR, Meta and Monado runtimes; SteamVR shows its binding UI for OpenXR apps). OpenVR IVRInput only as a SteamVR fallback.
- **Base profile:** `/interaction_profiles/oculus/touch_controller` (trigger value, grip/aim pose, haptic). Touch Plus extras (`/interaction_profiles/meta/touch_controller_plus`) are optional: SteamVR 2.19's OpenXR runtime doesn't list that extension; Meta's PC runtime does. Steam Frame controller profile: to research.
- **Aim pose is runtime-dependent:** keep the per-hand gun-angle offset and an in-game calibration step (DR-89 needed −60..+60°). Meta's Horizon OS OpenXR had a 180° aim-pose bug in 1.1.49-1.1.52.
- **Tracking loss:** Quest 3 estimates controller position briefly when out of camera view (weaker above the head than Quest 2). Use `XR_SPACE_LOCATION_POSITION_TRACKED_BIT` vs `..._VALID_BIT`: freeze or fade the gun and don't fire on inferred poses. Holster/reload gestures behind the head will be unreliable.
- **Haptics:** amplitude 0-1, duration in nanoseconds, frequency in Hz. Over Meta Link to SteamVR, haptics were reported very weak until SteamVR's *Meta Plugin Compatibility* was turned off (2024). Test recoil through the connection used by the SteamVR headset.

### 2.5 What the owner's headset sessions settled (2026-10-19 to 2026-10-21)

These rules override the plans above wherever they disagree. They apply to the Quest through ALVR on SteamVR, the owner's setup.

| Rule | Why |
|---|---|
| No game control on the **left Menu** button or the **right Oculus** button | ALVR gives the left Menu to the SteamVR dashboard, and the headset keeps the Oculus button. With Start on the left Menu, F-Zero GX could not be started and Wii games could not be advanced. |
| **Start / Plus = left-stick click**; **Home = hold left Y (0.5 s)**; **Minus = tap left Y** (Dolphin) | Buttons a game can actually receive. In the GameCube profile, click-and-tilt is still the D-pad. In the Wii profiles Minus is a pulse sent when Y is released after under 0.5 s, so holding Y for Home sends no Minus first. |
| Racers: **stick steers, triggers pedal**, by default; roll or tilt steering is an opt-in view only | "Roll steering sucks, make it an option, always off by default." |
| The Hub's SteamVR panel goes back with its **on-panel Back** button, not B | While the SteamVR dashboard is open, SteamVR does not deliver B to the Hub: its input log showed B unbound for a whole session. |
| No emulator or Hub error dialog may reach the desktop | Offline runs switch off panic and message boxes (for example Dolphin `UsePanicHandlers=False`). |

### 2.6 One button table for every system

The convention below is the reference. A platform may only differ where its hardware has no such button, and the exception is written in the "Exceptions" column. "Pending" means the Hub text and data already say it but the host default (`acvr-ps1`) or a private launcher map still has to be changed; until then the headset behaves the old way.

| Intent | Button (Quest Touch; Index in brackets where it differs) | Exceptions |
|---|---|---|
| Aim / point | Right controller | Not used where the game has no pointer (3DS Star Fox: the Circle Pad steers; N64 pad and wheel games) |
| Fire, accelerate, primary | Right trigger | Wii B; PS1 gun trigger; 3DS Star Fox laser (3DS A); N64 pad Z |
| Secondary | Right A | Wii A; PS1 GunCon A; 3DS Star Fox bomb (3DS Y); N64 A; arcade (Model 2, Model 3, System 22, Pinball): A is Start / credits, see the last row |
| Tertiary, **Back / cancel in menus** | Right B | N64 B and PS1 Circle are right B. Wii has no Back button: right B is Remote 1, and the game's own cancel is Remote B (right trigger). 3DS Star Fox: right B banks right, and the game's cancel (3DS B, brake) is the left grip; the host adds the left grip as a menu Back alias. N64 Star Fox: N64 B is the bomb on right A, as on 3DS |
| Hold action (boost, pedal, drift) | Right grip | Wii: per-game "second finger" button (A, Z or B), shown on the game's card |
| Brake, second hold | Left grip (racers: left trigger as the brake pedal) | 3DS Star Fox: left grip brakes. N64 racers: left trigger brakes, grips are R and Z |
| Move, steer | Left stick | Wii Nunchuk stick; racers steer with the stick by default, roll steering is an opt-in launcher only |
| **Start / Plus / pause** | **Left stick click** (Index: left stick click) | Right stick click is a silent alias only where the right stick is not used in play (PS1 gun modes, racers). It is dropped where the right stick plays: N64 pad (C buttons), N64 and 3DS Star Fox (bank), Wii (it is Remote 2), GameCube (it is Z). Arcade games keep A = Start, B = Coin because that is the cabinet's own button (Model 2 and 3, System 22, Pinball) |
| Select / Minus | Tap left Y | PS1 face-button layout: right stick click. GameCube and N64 have no Select |
| Home / in-game menu | Hold left Y for 0.5 s | Wii only (Remote Home). Nothing else has a Home |
| Quit and save | Hold left Y for 3 s | Wii and GameCube Dolphin setups: use SteamVR's **Exit game** (the Wii 0.5 s hold is Home) |
| Recentre the screen | Both grips for 1 s, or follow SteamVR's own recentre | Pending (host). Today: the Home key on the PC. Wii shell profiles: both thumbstick clicks |
| Gyro, tilt | Off by default | Opt-in launcher only (roll or tilt steering); 3DS Star Fox must be answered "no" to its gyro question |
| **Left Menu, right Oculus** | **Unused** | ALVR gives the left Menu to the SteamVR dashboard and the headset keeps Oculus (2.5). No game control may sit on either |

What each platform does today against this convention:

| Platform | Start / pause | Back | Notes |
|---|---|---|---|
| PS1 (gun and pad) | Left stick click: Hub text, pad Start and the face layout in the host (`Ps1Host.cpp`). Not yet seen in the headset | Right B | Select is right stick click in the face layout (host and Hub text agree) |
| N64 | Left stick click: Hub text and the host defaults (`PadControls.h`: `lclick=start` in pad, wheel and aim; `lmenu` kept as an alias; `rclick` alias only in wheel and aim). Not yet seen in the headset | Right B (N64 B); Star Fox: right A | L moved to left X in wheel and aim. Star Fox 64 text now matches 3DS Star Fox; the launcher map is pending |
| 3DS (Star Fox) | Left stick click: the README map uses `lclick=start`, with no `rclick` alias (it collides with bank). Not yet seen in the headset | Left grip (game B), menu alias pending | Right B banks right |
| PS2 (PCSX2) | Left stick click: `setups/pcsx2-vr/unattended-capture.patch` (the VR menu action, not the pad's L3, so L3 and R3 games are unaffected). Not yet seen in the headset | n/a | Select is right stick click, as in the PS1 face layout, because left Y carries the 3 s quit hold. The left menu "spare Start" alias is dead under ALVR |
| Wii (Dolphin) | Left stick click = Plus (profiles) | Game's own (Remote B or 1) | Minus on a Y tap release; Home on a 0.5 s hold; holding Y no longer sends Minus |
| GameCube (Dolphin) | Left stick click (not while the stick is tilted) | Right B | |
| Model 2, Model 3, System 22, Pinball | A = Start (arcade) | n/a | System 22's pause overlay (`data/controls/system22/controls.toml`) is the left stick click, so pause and resume are one button. Its gun-angle reset moved off that click to the left `menu_chord` (no host sets it yet). The host that sends the click is pending |

## 3. Pause overlay controls (all setups)

> Planned before the headset sessions. On Quest through ALVR the left Menu opens the SteamVR dashboard (see 2.5), so the pause button is the **left stick click** (see 2.6). Setups use SteamVR's **Exit game** and their own button maps instead.

| Action | Default |
|---|---|
| Open / close the pause overlay | Left stick click |
| Select in the overlay | Point the laser and pull the trigger |
| Recenter + set standing height | In the overlay, or hold both grips |
| Quit to desktop / SteamVR | In the overlay, or hold left Y for 3 s |
