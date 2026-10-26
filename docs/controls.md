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
| Start / coin | **Universal**: left stick click = Start, right stick click = Select or Coin (§2.5) | Fixed, the same in every game |
| Recenter | **Universal**: both stick clicks together (§2.5) | Fixed, the same in every game |

- **A gun in each hand:** in every two-player gun game the right hand is player 1's gun and the left hand is player 2's; player 2 joins with that hand's primary button (A or X), the one controller button the universal layer leaves to the second gun (§2.5). With one gun out, pulling the other trigger moves it to that hand. Rules and exceptions: [guns.md](guns.md) §1.
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

### 2.5 The universal layer: stick clicks only, identical in every game and every host

Contract v1 (2026-10-25). The owner's words: the menu buttons are off limits on both pads; one universal quit; one universal recentre; Select and Start mapped to something in every game, the same everywhere; motion setups as universal as possible. The one sentence every Hub card, hint and help text shows is `ac::universal::controls()` in `hub/src/core/launch/UniversalControls.h`; the website and this page repeat it word for word, and `hub/tests/universal_controls_text.py` checks that.

> Left stick click = Start. Right stick click = Select or Coin. Click both sticks together = put the screen in front of you again. Hold both stick clicks for 3 seconds = quit and save.

| # | Intent | Gesture | Notes |
|---|---|---|---|
| U1 | **Start** (Plus, pause) | **Left stick click**, alone | Arcade: Start. Wii: Plus |
| U2 | **Select** (Minus, Coin, Back where a system calls Select "Back") | **Right stick click**, alone | Arcade: Coin or credit. Wii: Minus |
| U3 | **Recentre the view** | **Both stick clicks together**: the second within 250 ms of the first, both released within 1 s | Re-seats the screen or table in front of the head at the current height |
| U4 | **Quit and save** | **Hold both stick clicks for 3 s** | Saves where the host can (save RAM, memory card, NVRAM; never a save state unless the host already does), then exits to the Hub. A progress ring or countdown where the host draws anything; at minimum a log line |
| U3p | **Pinball: adjust the table** | Click both sticks, keep them held, then move the hands | Hands apart or together scales the table, turning the pair turns it, moving both moves it; release to keep. Hand travel over 5 cm during the hold cancels the U4 countdown, so adjusting never quits |

Chord resolution (every host):

- On a left or right click press, wait up to **150 ms** for the other click. If it comes, it is a chord (U3 or U4) and **no Start or Select is sent at all**. Otherwise Start (or Select) is sent **pressed** for as long as the click stays held, then released.
- A U3 chord fires on release when both were released within 1 s. Holding for 3 s fires U4 instead, and no recentre is sent.
- A host that cannot delay (a patched emulator with a plain button map) says so in its report. It may send Start or Select at once and accept the small chance of a stray press before a chord.
- **Native L3 and R3** now belong to the universal layer. A game that needs them (PS2, some PS1 Dual Analog titles) gets them only through a documented exception on its card. Default proposal: **left Y = L3, left X = R3**, only in games that need them.
- Left Y no longer selects, quits or starts anything. It is free for games (for example Wii Home as a 0.5 s hold).
- Index controllers: the stick clicks are the same; their system buttons are off limits like Quest's.

### 2.6 Off-limits menu buttons, and how to get back to the Hub

- **No host, profile, map, cabinet set or alias may bind the left Menu button or the right Oculus/system button.**
- Under ALVR on SteamVR the **left Menu button opens the SteamVR dashboard**, and the **Hub panel is in the dashboard**. That is the way back to the Hub from any game: press the left Menu button, open the Hub panel, choose **Exit game** (or **Stop**) there. The right Oculus button stays with the headset.
- While the SteamVR dashboard is open, SteamVR does not deliver B to the Hub (its input log showed B unbound for a whole session), so the Hub panel goes back with its **on-panel Back** button.
- The Hub keeps no gesture of its own on the controllers while a game runs: a Hub action on the stick clicks would take them away from the game. U4 lives in each host.
- **Pinball is the one exception to U4.** VPX cannot time a hold, and the Hub cannot read the controllers while VPX has them, so a table is left with the left Menu button, then the Hub panel's Exit game (the table closes the ordinary way and saves), or Esc on the keyboard. The Pinball card says so. U1, U2 and U3p work in a table; both clicks align the table to the hands, not in front of the head (VPX's own controller alignment).
- No emulator or Hub error dialog may reach the desktop: offline runs switch off panic and message boxes (for example Dolphin `UsePanicHandlers=False`).

### 2.7 Motion: one setup wherever the game allows

| # | Intent | Rule |
|---|---|---|
| M1 | Point / aim | The aiming hand's **aim-pose ray** is the pointer in every game that points: light guns, the Wii pointer, the 3DS stylus, in-game panels. Right hand by default, left for left-handed play and two-gun games. The same ray origin and orientation convention in every host (libacvr's) |
| M2 | Fire / primary | Trigger of the aiming hand |
| M3 | Reload | Point off the screen and pull the trigger, in every gun game whose original reloads off screen. No extra reload button by default |
| M4 | Laser sight toggle | Secondary face button of the gun hand (right B, left Y), wherever a host draws a laser |
| M5 | Cover or pedal (Time Crisis family) | Either grip |
| M6 | Racers | Left stick steers, right trigger accelerates, left trigger brakes, grips are the hold actions. Roll or tilt steering is opt-in only, never default |
| M7 | Gyro, tilt, shake | Off by default; opt-in per game |
| M8 | Hands and guns | The shared gun and hand assets (`assets/guns`, `assets/hands`, libacvr). No host-private model or colour rules |
| M9 | Two-handed and mounted guns | The second hand's grip steadies; the trigger hand fires. The same in every host that supports two hands |

A host may deviate only where its game physically cannot follow the rule; the deviation is written on the game's card and in the table below.

### 2.8 Everything else stays per game

Face buttons, triggers and grips keep the per-system table below. It no longer contains the stick clicks, the menu buttons or left Y's old duties; those are 2.5 and 2.6.

| Intent | Button (Quest Touch; Index in brackets where it differs) | Exceptions |
|---|---|---|
| Aim / point | Right controller (M1) | Not used where the game has no pointer (3DS Star Fox: the Circle Pad steers; N64 pad and wheel games) |
| Fire, accelerate, primary | Right trigger (M2) | Wii B; PS1 gun trigger; 3DS Star Fox laser (3DS A); N64 pad Z |
| Secondary | Right A | Wii A; PS1 GunCon A; 3DS Star Fox bomb (3DS Y); N64 A. Arcade games (Model 2, Model 3, System 22): A is no longer Start, so each game's own use of A and B is on its card (cabinet event buttons, views) |
| Tertiary, **Back / cancel in menus** | Right B | N64 B and PS1 Circle are right B. Wii has no Back button: right B is Remote 1, and the game's own cancel is Remote B (right trigger). 3DS Star Fox: right B banks right, and the game's cancel (3DS B, brake) is the left grip |
| Hold action (boost, pedal, drift) | Right grip | Wii: per-game "second finger" button (A, Z or B), shown on the game's card |
| Brake, second hold | Left grip (racers: left trigger as the brake pedal) | 3DS Star Fox: left grip brakes. N64 racers: left trigger brakes, grips are R and Z |
| Move, steer | Left stick | Wii Nunchuk stick; racers steer with the stick (M6) |
| Home / in-game menu | None universal | Wii only (Remote Home): the host's choice, for example a 0.5 s hold on left Y, which U4 freed |
| Gyro, tilt | Off by default (M7) | Opt-in launcher only; 3DS Star Fox must be answered "no" to its gyro question |
| **Left Menu, right Oculus** | **Unused** | See 2.6 |

Per-system exceptions to the contract. The lane reports (A retro host, B PCSX2, C Dolphin, D libacvr and System 22, E Model 2, Model 3 and Dreamcast, F pinball) fill the rows marked **TO FILL**. Until then every row is the contract default, and the Hub card text for each game states U1-U4.

| Platform | U1-U4 today | Exceptions |
|---|---|---|
| PS1 (gun and pad), N64, Saturn, MAME gun, 3DS (acvr-ps1) | Contract default in the cards; the host change is lane A's | **TO FILL** (lane A): chord delay, N64 C buttons on the right stick, L3 and R3 |
| PS2 (PCSX2) | Contract default in the card | **TO FILL** (lane B): PS2 games that need L3 and R3 |
| Wii, GameCube (Dolphin) | Contract default in the card | **TO FILL** (lane C): Wii Home, Z on the right stick click |
| System 22 (Time Crisis), libacvr hosts | Contract default in the card | **TO FILL** (lane D) |
| Model 2, Model 3, Dreamcast, Atomiswave | Contract default in the card. The cabinet sets in `data/controls/cabinets/` are generated by `tools/model3_control_art.py`: Start1 is the left stick click, Coin1 the right stick click, and racers' Change view moved to right A | **TO FILL** (lane E): host-side chord, player 2 Start2 and Coin2 |
| Pinball (VPX) | Start, Coin and U3p as in 2.5; **no U4** (2.6) | Recentre aligns the table to the hands. Start or Coin can fire just before a chord (VPX has no 150 ms wait). Nudge by physical push is unsupported |
| Gun control sets (`data/controls/gun-*.toml`) | `p1-start` is the left stick click, one `p1-coin` the right stick click, the laser the gun hand's secondary; player 2 joins with that hand's primary and has no separate coin | None |

Retired by this contract (2026-10-25): left Y as Select (tap), quit (hold 3 s) and Start alias; both grips held for 1 s as recentre; A = Start and B = Coin as the only Start and Coin on arcade games; SteamVR's Exit game as the only quit for Dolphin. They came from the 2026-10-19 to 2026-10-21 headset sessions and were replaced because they put different start, quit and recentre gestures on different systems.

## 3. Pause overlay controls

Superseded by 2.5. The pause or Start button is the left stick click (U1), recentre is both stick clicks (U3) and quit is holding both for 3 seconds (U4). The overlay's items are chosen by pointing the laser and pulling the trigger.
