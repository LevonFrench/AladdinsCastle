# Controls catalog: every cabinet control, its VR mapping, and its 3D model

[controls.md](controls.md) covers light guns and the basic racing ghost controls. This catalog covers **every control type** found on the cabinets in our catalog, including the unusual ones: bicycle, skis, skateboard, horse, raft paddle, hang-glider, planes, tanks, jet skis, the fire hose and the special guns. For each type it gives:

1. what the real cabinet has, with the game's input axes from MAME;
2. how two motion controllers and the headset drive it, with the maths;
3. the 3D model we build for it.

Sources: cabinet research on 80+ titles (MAME `INPUT_PORTS`, operator manuals, Arcade Museum, Sega Retro, TeknoParrot) and VR prior-art research (VTOL VR, Carve Snowboarding, VR Skater, Rival Stars, Kayak VR: Mirage, Vox Machinae). Full tables are in the wiki: topic `vr-arcade-gun-racing`, `raw/data/2026-10-08-controls-cabinets-*.md` and `raw/articles/2026-10-08-vrcontrols-*.md`.

Status: **draft 1**. Every mapping is a default that the control-set file can override.

## 1. The signal pipeline (every analog control)

```
 hand/head pose ─► gesture extractor ─► calibrate ─► deadzone ─► curve ─► smooth ─► game range ─► game input
                   (angle, offset,       (per-user    (inner/     (gamma,   (One Euro   (MAME min/max/
                    rate, cadence)        centre and   outer)      expo)     filter)     centre, reverse)
                                          span)
```

- **Gesture extractors** (§2) turn poses into one number per axis: an angle, an offset, a rate or a cadence.
- **Calibrate:** each user sets centre and span once per control type (e.g. how far *you* lean). They're stored in `user/profiles/<player>.toml`.
- **Deadzone and curve:** per element, as in `controls.toml` today (`curve = { deadzone, gamma }`).
- **Smooth:** a One Euro filter (adaptive low-pass), low lag when moving fast and steady when still. Each element sets `min_cutoff` and `beta`.
- **Game range:** taken from the game's own input definition (e.g. MAME `PORT_MINMAX(0x20,0xe0)`, centre `0x80`, `PORT_REVERSE`). libacvr outputs a normalised −1…1 or 0…1 value, and the backend maps it to the game's range. MAME's `PORT_SENSITIVITY`/`PORT_KEYDELTA` only matter for keyboard and relative devices; we drive absolute values.
- **Grab model:** VTOL VR rules everywhere. Input binds only while the grip is held, a soft lock keeps the control bound when the hand drifts, and the control springs back on release if the real one did.
- **Haptics:** detents, limit stops, force feedback from the game where it outputs it, texture rumble from speed or terrain.

## 2. Gesture extractors (reused across controls)

| Extractor | Definition |
|---|---|
| `bar_yaw` | Two hands gripping a bar: yaw of the hand-to-hand line around the vertical axis, relative to calibrated straight |
| `bar_roll` | Two hands on a bar: roll of the hand-to-hand line around the forward axis (one hand up, one down) |
| `bar_push` | Two hands on a bar: forward/back offset of the hands' midpoint from its calibrated rest |
| `bar_lift` | Two hands on a bar: up/down offset of the midpoint (Prop Cycle climb/drop) |
| `grip_twist` | Roll of one hand around the axis it's gripping (motorbike twist throttle, hose nozzle collar) |
| `stick_tilt` | Hand on a grabbed stick: pitch and roll of the hand around the stick's base pivot (VTOL VR) |
| `lever_slide` | Hand on a lever: position along the lever's travel path (throttle, handbrake) |
| `body_lean` | Head lateral offset from the calibrated seat/stance centre (optionally blended with head roll) |
| `body_bob` | Head vertical oscillation: amplitude and frequency (rocking, stepping in place) |
| `body_rock` | Head forward/back offset from rest, used directly as a position |
| `hand_cadence` | Frequency × amplitude of a repeating hand motion (paddle strokes, arm pumping) |
| `flick` | Angular-velocity spike above a threshold (whip, pole jab) → button pulse |
| `trigger` / `grip` / `stick` | The controller's own analog trigger, grip squeeze (analog on Touch) and thumbstick |

## 3. Control types

Each row lists the games, the real control, the default VR mapping and the 3D model. **Alt** means an alternative mode the user can pick.

### 3.1 Cars and trucks

| Type | Games (examples) | Real control → game axes | VR mapping (default) | Alt | Model |
|---|---|---|---|---|---|
| `wheel` | Most racers; Tokyo Wars (tank) | Wheel pot (MAME PADDLE, e.g. Power Drift `0x20-0xE0` centre `0x80`); FFB on Ace Driver, Hard Drivin', Arctic Thunder | Grab rim with one or two hands; angle = `bar_yaw` around the wheel's own axis; range per cabinet (270°/360°/540°) | `stick` X for seated-relaxed play | Wheel with hub, rim, spokes; pivot + `grab_rim` ring |
| `pedal` | All | PEDAL / PEDAL2 / PEDAL3 | Right trigger = gas, left trigger = brake | Thumbstick Y (forward gas / back brake) when triggers are taken by fire buttons (§5) | Pedal box (visual only) |
| `clutch` | Hard Drivin' (PEDAL3 analog) | Third pedal | **Left grip squeeze** (analog on Touch) | Auto-clutch toggle | 3rd pedal |
| `handbrake` | GTI Club, Xtrial, Sega Rally | Lever | Grab lever, `lever_slide` pull → axis | Button | Lever with ratchet detent |
| `shifter_hl` | OutRun, Power Drift, Rave Racer | 2-position toggle | Grab knob, push/pull → LOW/HIGH buttons | Button toggle | Stubby lever |
| `shifter_h4` / `h6` | Daytona, Sega Rally, F355, Initial D | H-gate (gear decoder) | Grab knob, hand position snaps through the gate (magnetic gates, haptic clunk) → gear buttons | Sequential on face buttons | H-gate plate + lever |
| `shifter_seq` | Battle Gear, GTI Club, ECA, Ace Driver | Up/down | Push/pull grabbed lever | A/B buttons | Sequential lever |
| `shifter_xy` | Hard Drivin' (analog X/Y lever) | AD_STICK X/Y | Grabbed knob position in the gate → X/Y directly | — | 5-position gate |
| `gear_buttons` | Cruis'n USA, California Speed, Crazy Taxi (D/R), 18 Wheeler (H/L/R) | Buttons | Poke the panel buttons, or face buttons | — | Button panel |
| `horn` / `view` / `boost` | 18 Wheeler, many | Buttons | Poke, or controller buttons | — | Buttons |

### 3.2 Bikes, motorbikes, jet skis, snowmobiles

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `moto_bars` | Harley-Davidson (PADDLE steer, PEDAL throttle, PEDAL2 brake), GP Rider, Cyber Cycles (PADDLE "Bike Bank", PEDAL "Throttle Grip", PEDAL2 "Brake Lever"), Road Burners, 500 GP | Handlebars; twist throttle; brake lever | Grab both grips. Steer/bank = `bar_yaw` blended with `bar_roll`. Throttle = **right `grip_twist`** (twist the grip like a real bike; VRider SBK and UEVR mod prior art). Front brake = right trigger, rear brake = left trigger. | Motorbike bars, tank top, twist grip with visible rotation |
| `lean_platform` | Hang-On / Super Hang-On sit-down (PADDLE `0x20-0xE0` REVERSE), Manx TT and Motor Raid (BANK PADDLE REVERSE), Motocross Go! / 500 GP (bank/tilt), Wave Runner (ROLL) | The whole bike tilts; rider leans | Lean = **`body_lean`** (head lateral offset; calibrate "full lean" once), blended with `bar_roll` (default 70/30). Seated: shoulder roll is enough. | Seat + tank; the bike model visibly tilts with you |
| `wheelie_pull` | Enduro Racer ("bank up/down" AD_STICK_Y) | Pull bars back to lift the front | `bar_push` backwards → lift axis | Trail-bike bars |
| `jetski_bars` | Wave Runner (HANDLE, ROLL, THROTTLE lever), Aqua Jet (3 axes, physical form unverified), Jet Wave | Jet-ski bars, throttle lever, body roll | Steer = `bar_yaw`; roll = `body_lean` + `bar_roll`; throttle = right trigger (the lever on the bar animates) | Jet-ski bars with throttle lever |
| `bicycle` | **Prop Cycle** (AD_STICK_X handle L/R, AD_STICK_Y climb/drop, both `0x0BF-0x33F` centre `0x1FF`; pedal flywheel with rotation sensor), Downhill Bikers (PADDLE steering; brake buttons) | Bars turn and pitch; pedals spin | Turn = `bar_yaw`; climb/drop = **`bar_lift`/`bar_push`** (push the bars down/forward to dive, pull up to climb). **Pedalling:** default = **pedal-in-place cadence**: you march/pedal standing or seated and `body_bob` frequency becomes pedal speed (Race Yourselves-style head micro-motion). Alt 1: right trigger = pedal effort. Accessories are out of scope (D39). | Bicycle bars, frame top tube, pedals that spin at the game cadence |

### 3.3 Snow, skate and water boards

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `ski_plates` | **Alpine Racer 1/2** ("Steps Swing" AD_STICK_X `0x080-0x380` centre `0x200`; "Steps Edge" AD_STICK_X), Alpine Surfer (same block) | Two footplates swing side-to-side and tilt onto their edges; step-lock motor | **Swing = `body_lean`** (shift hips and head sideways, as on the real plates). **Edge = head roll + both hands' average roll** (hold the pole grips, tilt your wrists in), with a deadzone. Stand mode default. | Two ski plates + pole grips at hand height |
| `ski_stand` | Sega Ski Super G (INCLINING / SWING), Ski Champ (PADDLE + PEDAL + pole buttons) | Foot controller tilts and swings | Same as `ski_plates`. **Pole buttons = `flick` down with either hand** (jab the poles). | Foot stand + poles |
| `skateboard` | **Top Skater** (CURVING AD_STICK_X, SLIDE AD_STICK_X, both `0x00-0xFF` centre `0x80`), Air Trix | Deck tilts left/right (curving) and swings (slide) | **Curving = `body_lean`** (lean like on a board). **Slide = hip/head yaw swing.** Hands free: Carve-style option where **both hands held low like holding the board edges**: their roll = curving, their yaw = slide. Tricks: pull both hands up fast = jump (Carve prior art). | Deck with trucks under your feet; railings |
| `surf_stand` | Surf Planet (one analog axis; stand-up joystick per LaunchBox, not a board) | Stand-up joystick | Grab stick, tilt → axis | Stand-up stick |
| `snowmobile` | Arctic Thunder (FFB wheel + throttle lever with thumb boost; not handlebars) | Wheel + lever | `wheel` + right trigger throttle; boost = A | Wheel + lever |
| `boat_throttle` | Hydro Thunder (wheel or yoke disputed + throttle lever + turbo) | Throttle lever | Left hand on a grabbed throttle lever (`lever_slide`), right on wheel; turbo = thumb button | Boat throttle |

### 3.4 Animals and paddles

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `horse` | **Final Furlong 1/2** ("Swing" AD_STICK_Y = saddle rock; "Handle" AD_STICK_X = reins; whip button on the rein) | Rocking saddle (rock rate = horse speed), reins, whip | **Swing = `body_rock`**: your upper body's forward/back position drives the saddle position directly, and the game computes speed from your rocking rhythm as it did with the real saddle. Alt: both hands' forward/back pumping on the reins (Rival Stars). **Reins = grab both reins; steer = one hand pulled back relative to the other.** **Whip = right-hand `flick`.** | Saddle, horse neck, reins with handles |
| `raft_paddle` | **Rapid River** (paddle channels HORIZONTAL / VERTICAL; manual: rotate faster = faster, back = slow, incline = steer) | Oversized paddle on a two-axis mount; 1-2 riders; seat jolts | Two-hand grab on the **pivoted paddle model**. Its two gimbal angles drive the two channels directly (position-mapped, like the real mount). Stroke speed comes naturally from how fast you rotate it. Haptic "water bite" during the stroke (VR SUP prior art). | Mounted paddle on a 2-axis gimbal, raft rim |

### 3.5 Flight

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `flight_stick` | Wing War (STICK X/Y REVERSE + THROTTLE + boost), Galaxy Force II (stick X/Y + throttle; motion cockpit), Sega Strike Fighter (stick + throttle; rudder pedals not in MAME), Landing Gear (lever X/Y + throttle) | Centre stick with trigger and thumb buttons; throttle lever | **Right hand grabs the stick: `stick_tilt` → X/Y** (VTOL VR; soft lock; spring centre). Stick trigger = controller trigger (fire), thumb button = A. **Left hand grabs the throttle: `lever_slide`.** Rudder (Strike Fighter) = left stick X or stick twist. | Stick on a base with a boot, throttle quadrant |
| `yoke` | Landing High Japan (X/Y yoke + throttle) | Yoke | **Two-hand grab:** roll = `bar_roll` (turning the yoke), pitch = `bar_push` (push/pull). Throttle lever reached with the right hand when needed. | Yoke + column + throttle |
| `twin_throttle` | Star Wars Racer Arcade (two throttle levers, squeeze brake on the right) | Two levers | Each hand grabs a lever; `lever_slide` each → left/right throttle (the game steers by difference); **brake = right grip squeeze (analog)**; boost = A | Two pod levers |
| `glider_bar` | **Hang Pilot** (AN0 "Rudder" AD_STICK_X `0x1C5-0x24A` REVERSE; AN1 AD_STICK_Y; push/pull switches; rider stands, knees on pads) | Control bar: push/pull = speed, swivel = turn | Two-hand grab on the bar. Push/pull = `bar_push` (with push/pull limit switches at the ends). Turn = `bar_yaw` blended with `body_lean` (you shift your body like a hang-glider pilot). | A-frame control bar |

### 3.6 Tanks and vehicle combat

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `twin_stick` | **Cyber Sled** (two joysticks, each X/Y, trigger + thumb) | Two arcade sticks | Each hand grabs its stick: `stick_tilt` X/Y. Trigger = fire, thumb = A. | Two sticks |
| `tank_wheel` | **Tokyo Wars** (PADDLE "Steering Wheel" `0x100-0x300`, PEDAL forward, PEDAL2 reverse, L/R trigger buttons on the wheel) | Wheel turns barrel and hull; two pedals; triggers on the wheel | `wheel` grab. **Fire = controller triggers** (they sit where the wheel's triggers are), so **pedals move to the thumbstick Y** (forward/back), per §5. | Tank wheel with trigger grips |
| `combat_car` | Desert Tank (PADDLE steer, PEDAL accel, AD_STICK_Y brake; machine gun, cannon, shift buttons) | Wheel + pedals + buttons | `wheel` + pedals on triggers; fire buttons on face buttons | Wheel + buttons |
| `trackball` | Armadillo Racing (optical trackball X/Y) | Trackball | Palm over a virtual ball: hand velocity tangent to the surface spins it, with inertia | Big trackball |

### 3.7 Special light guns and the hose

| Type | Games | Real control → game axes | VR mapping | Model |
|---|---|---|---|---|
| `pistol` | Time Crisis, Point Blank, Lethal Enforcers, most | Light gun (IPT_LIGHTGUN) | [controls.md](controls.md) §1 | Pistol per game |
| `pump_gun` | Big Buck Hunter (holstered optical gun; "Gun Pump" and "Gun Trigger" switches per manual), CarnEvil (two pump shotguns) | Shotgun/rifle with pump reload | Two-hand: right hand trigger; **left hand slides the pump** along the fore-end (`lever_slide` along the barrel) → reload button | Shotgun with slide |
| `machine_gun` | Ghost Squad (IR gun), Ghost Squad Evolution (SMG with single/burst/auto selector), Crisis Zone, Aliens | Held machine gun | Trigger held = auto-fire; **fire selector = left `grip_twist` on the selector or B**; second hand optional on the fore-grip | MG with selector |
| `stick_trigger` | Sega Jurassic Park (1993, System 32: joystick, not a gun) | Joystick with trigger | Grab stick, `stick_tilt`; trigger = fire | Stick |
| `mounted_gun` (positional) | Gunblade NY, Operation Thunderbolt, Terminator 2, Revolution X, Space Gun, L.A. Machineguns, Terminator Salvation, Halo: Fireteam Raven | Gun on a pivot read by **potentiometers** (MAME IPT_AD_STICK), not a light gun. Pivot angles weren't found in any source. | Grab the handles; the gun pivots within calibrated yaw/pitch limits. **Pivot angles map straight to the game's ADC counts** with a calibrated neutral (set in Control Mapping mode). Recoil haptics on both hands. Space Gun also has a pump and a halt/reverse pedal (thumbstick back). | Pivot mount + gun + handles |
| `mounted_lightgun` | Operation Wolf, Beast Busters, Line of Fire (MAME: LIGHTGUN; Wikipedia says pots), Let's Go Jungle, Ocean Hunter ("Shock Guns") | Gun on a fulcrum read **optically** (IPT_LIGHTGUN) | Pivot-constrained gun; aim = ray from the barrel, projected to screen coordinates as for pistols | Pivot mount + gun |
| `sniper_rifle` | Silent Scope 1/2/EX/Fortune Hunter | Rifle on a **positional yaw/pitch mount** (MAME AD_STICK `0x000-0x7FF`); a small scope screen (secondary sources) | Two-hand rifle on a virtual mount; **yaw/pitch → the game's ADC counts**. The scope shows a **render-to-texture zoomed view** of the game around the aim point, only while your eye is near it (performance). | Rifle + mount + scope (lens = render target) |
| `crossbow` | The Walking Dead | Crossbow | Like pistol, plus reload by pulling the string back (`lever_slide`) | Crossbow |
| `fire_hose` | **Brave Firefighters** (hose with spray switch held, drainage button, nozzle twist mist/stream/fog, strong vibration; not in MAME) | Two-hand hose nozzle | Aim like a gun from the nozzle pose; **spray = trigger held**; **nozzle mode = left-hand `grip_twist` on the collar** (detents for mist/stream/fog); drain = B; continuous rumble while spraying | Nozzle + collar + hose |
| `body_sensor` | Police 911 / 24-7 (body-tracking duck and dodge) | Camera sensor tracks the player | **Buttons, no body tracking:** duck = hold grip (as the cover button), dodge = left stick X | — |

### 3.8 Cabinet motion

Moving cabinets (OutRun Deluxe, Power Drift, Galaxy Force II, Hang-On, Harley Deluxe, Rapid River jolts, Power Sled, Truck): **never move the VR camera** to simulate them (motion sickness). Optional "cabinet feel": the cockpit model tilts a little in your peripheral view, plus haptic rumble on bumps. Off by default.

## 4. Assist modes (fatigue and accessibility)

Continuous gestures (pedalling, paddling, rocking, poling) tire arms fast. Every one has an assist:

| Control | Assist |
|---|---|
| Bicycle pedalling | Trigger = pedal effort; or auto-cadence |
| Horse rocking | Auto-rock at a speed set by stick Y (Rival Stars chose auto-gallop) |
| Raft paddle | Auto-stroke while the trigger is held; steer by tilting |
| Ski/skate lean | Seated mode: shoulder lean or stick X |
| Poles | Button instead of jab |

## 5. Input conflicts: the trigger rule

The **trigger is the fire button** whenever a cabinet's steering control carries fire buttons (Tokyo Wars, Desert Tank, gun-and-drive games like Lucky & Wild and Deadstorm Pirates). On those cabinets the **pedals move to the thumbstick Y axis**. Elsewhere the triggers are pedals. Gun games with foot pedals (Time Crisis cover pedal, TC5's two pedals, Razing Storm attack/cover, Space Gun halt/reverse, Crisis Zone) map pedals to the grip button (cover) or the thumbstick, never the trigger. No head-height ducking anywhere.

## 6. 3D model library

### 6.1 What we build

One model per control type above (about 35), plus variants where cabinets differ visibly (e.g. Sega vs Namco gun styles, H4 vs H6 gates). All are **original designs**: evocative of the arcade hardware but not copies of manufacturer trade dress or logos.

### 6.2 Format and conventions

- **glTF 2.0** (`.glb`), metres, +Y up, −Z forward. One file per control in `assets/controls/<type>/<variant>.glb`.
- **Named nodes** that libacvr binds to:
  - `pivot_*`: rotation pivots (wheel hub, stick base, mount yaw/pitch, gimbal axes)
  - `slide_*`: linear travel (levers, pump, throttle)
  - `grab_*`: grab volumes (capsules/boxes) with a preferred hand pose
  - `limit_*`: end stops (rotation/travel limits)
  - `button_*`: pokeable buttons
  - `fx_*`: muzzle, nozzle, scope lens, etc.
- **Materials:** PBR (metallic-roughness), one base colour slot driven by the game's `[hub].accent` so controls match each game's colours. A translucent "ghost" material variant.
- **Budget:** Quest-class standalone ≤ 5k triangles per control, two LODs, 1k textures.
- **Source of truth:** Blender scripts in `assets/controls/src/<type>.py` that build each model procedurally from parameters (radius, travel, spoke count...). The `.glb` files are built outputs. Parametric scripts let a control set resize a model (e.g. a 280 mm vs 350 mm wheel) without new art.

### 6.3 Control set files bind models to axes

```toml
# games/propcycl/setup/controls.toml
id   = "namco-prop-cycle"
seat = { posture = "standing", height_m = 0.0 }

[[element]]
id     = "bars"
type   = "bicycle"
model  = "assets/controls/bicycle/prop-cycle.glb"
grab   = ["grab_left", "grab_right"]
axes   = [
  { node = "pivot_turn",  extractor = "bar_yaw",  out = "AD_STICK_X", min = 0x0BF, max = 0x33F, centre = 0x1FF, reverse = true },
  { node = "pivot_pitch", extractor = "bar_lift", out = "AD_STICK_Y", min = 0x0BF, max = 0x33F, centre = 0x1FF },
]
smoothing = { min_cutoff = 1.0, beta = 0.02 }

[[element]]
id        = "pedals"
type      = "bicycle_pedals"
model     = "assets/controls/bicycle/pedals.glb"
extractor = "body_bob"            # pedal-in-place cadence
assist    = "trigger"             # alt: right trigger = effort
out       = "pedal_rpm"           # backend converts to the game's rotation-sensor pulses
```

## 7. Control Mapping mode (owner request, 2026-10-08)

Every game ships with the **best default mapping** from this catalog. Control Mapping mode lets you **see every mappable element of the cabinet's controls and watch it move 1:1 with you**, then tune or rebind it.

### 7.1 What you see

- The cabinet's control set as **solid 3D models** (not ghosts), at their real positions around you: wheel, shifter, pedals, bars, skis, saddle, guns, buttons.
- **Every mappable element is outlined and labelled** with its name and the game input it drives (e.g. "Handlebar turn → AD_STICK_X, 0x0BF-0x33F, reversed"). Unmapped or digital-only parts are dimmed.
- **Live 1:1 motion:** each element is animated by the **exact same pipeline** the game receives (§1: extractor → calibration → deadzone → curve → smoothing → game range). Turn your hands and the wheel turns; lean and the bike tilts; rock and the saddle rocks. If it looks right here, it is right in the game.
- **Axis gizmos** on each element: arrows or arcs for each axis, end-stop markers, and a live value bar showing raw value → output → game value (hex/decimal).
- Your hands, and optionally the head-lean reference: a small marker showing your calibrated centre and current offset.

### 7.2 What you can do

| Action | How |
|---|---|
| **Select an element** | Point and trigger, or touch it. A panel opens beside it. |
| **Calibrate** | "Set centre" (hold still), then "Sweep" (move through your full comfortable range; min/max are recorded). Per element, per player. |
| **Learn a new binding** | Press *Learn*, then make the motion you want (e.g. lean, twist, bob). The studio scores every candidate extractor (§2) by how strongly it follows your motion and proposes the best one. Accept, or pick from the list. |
| **Change source** | Pick extractor and source from a list (hand L/R, both hands, head, trigger, grip, stick). Includes the alternative modes from §3 and the assists from §4. |
| **Tune feel** | Deadzone, curve (gamma), smoothing (One Euro `min_cutoff`/`beta`), invert, sensitivity. Sliders update the 1:1 motion instantly, with a small input→output graph. |
| **Grab rules** | Grab radius, soft lock on/off, spring-return speed, haptic detent strength. |
| **Test against the game** | **Live:** game paused or running in the background, with the panel showing the value the game actually receives. **Service mode:** launch the game's own operator input-test screen, which shows what the original game reads from each control. This is the best possible verification, and most arcade boards have one. |
| **Reset** | Reset one element, or the whole set, to the shipped default. |
| **Save / share** | Saved as **only the changed keys** to `user/overrides/games/<id>/setup/controls.toml` (per player). Export as a small file to share in packs, and import others'. |

### 7.3 Where it runs

- **In VR:** from the pause overlay ("Map controls") or the Hub detail page ("Map controls in VR"). Runs inside libacvr, so every setup gets it for free. It works with the game running (live test) or without it (preview).
- **On the desktop Hub:** the same element list as a 2D editor for typing exact numbers and editing bindings. No live motion without a headset.

### 7.4 Implementation notes

- Lives in libacvr as a **control studio** module. It reuses the in-game pipeline: no separate code path, so no drift between preview and game.
- The control set file already holds everything it edits (`extractor`, `source`, `axes`, `curve`, `smoothing`, grab rules). Calibration goes in the player profile.
- *Learn* uses a short capture window (about 3 s). It computes each candidate extractor's normalised variance and correlation with the motion's main component, and proposes the best fit.

## 8. Open questions

1. ~~Optional hardware~~: **decided no (D39)**. Controllers and headset only; every mapping must work with them.
2. **Cabinet motion feel:** the subtle peripheral cockpit tilt, on or off by default?
3. **Model production:** build the first models now with Blender (parametric scripts), starting with wheel, H-shifter, motorbike bars, bicycle, ski plates, flight stick and throttle, and mounted gun?
4. Unknowns to resolve per game: Rapid River's steer channel, Aqua Jet's and Alpine Surfer's physical form, Hydro Thunder wheel vs yoke, and shifter types for several Sega/Namco racers.
