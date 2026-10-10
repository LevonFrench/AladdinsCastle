# Guns: a gun in each hand, and our own gun models

Status: 2026-10-10, design (owner requests D45, D46). Data: [`data/guns/`](../data/guns/). Check it with `python tools/gun_models.py`.

Two rules for every gun game:

1. **Two-player gun games put a gun in each hand.** Your right hand is player 1's gun, your left hand is player 2's.
2. **You always see the gun you are holding.** We draw our own 3D model of the game's gun in your hand: a GunCon lookalike for PlayStation games, a Virtua Gun / Stunner lookalike for Saturn games, the cabinet's pistol, shotgun or mounted machine gun for arcade games. It is the same idea as the ghost controls for racing ([controls-catalog.md](controls-catalog.md) §7): the real control is in view, and it moves 1:1 with you.

## 1. A gun in each hand (D45)

### 1.1 Which games

Of the catalog's 152 gun games, 100 have two or more guns. **94 of them are eligible for a gun in each hand.** Eligibility is a fact about the game; whether it works today depends on the installed route (§1.3). The other 6 give each player a separate view (twin linked cabinets with one screen per player, or split screen), so one headset can't show both; they stay one-gun until a two-view setup exists. The list is `[two_guns].separate_views` in [`data/guns/defaults.toml`](../data/guns/defaults.toml): Time Crisis II, 3, 4 and 5 in the arcade, and Time Crisis II and 3 on PlayStation 2.

A cabinet with three or four guns (Beast Busters, Laser Ghost, Crypt Killer, Death Crimson 2) still maps two: players 1 and 2.

### 1.2 How it plays

| Rule | Default | Options |
|---|---|---|
| Hands | Right hand = player 1, left hand = player 2 | `p1_hand = "left"` (also what a left-handed profile sets) |
| When the second gun appears | **When player 2 joins.** Until then the off hand shows a faint copy of its gun. Pressing that hand's Start button joins, as it would on the cabinet, and the gun turns solid. When player 2 is out of the game, it goes faint again. | `two_guns = "on_join"` (default), `"always"` (player 2 joins as soon as the game allows), `"off"` |
| One gun, switching hands | With one gun out, pulling the other hand's trigger moves the gun to that hand | `hand_switch = "trigger"` (default), `"off"` |
| Fire, reload | Each hand's trigger fires its own gun. Each gun reloads by itself with the game's own rule (off-screen shot, or point down / flick) | Per hand |
| Cover pedal (two-player cover games on one screen) | Each hand's grip button is that player's pedal | As [controls.md](controls.md) §1.3 |
| Laser and aim dot | One per gun, in the player's colour | Off, laser, dot, per game |
| Gun angle | Set per hand; the left hand starts as a mirror of the right | Control Mapping mode |
| Credits | Player 2 joining uses a credit, as on the cabinet. Free play is the suggested setting. | Coin on each hand's coin button |

- **Joining is a button, never a hand pose.** A review of one VR shooter that wakes the second gun with a grip pose calls that fiddly.
- **Reloads that need the other hand are off while both hands hold guns.** Racking a slide or pumping a fore-end with the free hand only works with one gun. With two, a pump shotgun reloads by the game's off-screen rule or a flick.
- **Two-handed guns become one-handed.** A rifle, shotgun or SMG in each hand is held by its pistol grip, with no fore-grip hand. A mounted machine gun is held by its rear grip and its yoke follows.
- **Aim is per gun.** Each ray is traced into the scene and projected to its own gun input, exactly as for one gun ([controls.md](controls.md) §1.1). Nothing is shared.

### 1.3 What each route must support

Two guns need two independent gun inputs in whatever runs the game:

| Route | Two guns today |
|---|---|
| Our own true-3D setups (libacvr) | Yes. The contract already has gun slots and players ([libacvr-contract.md](libacvr-contract.md)); the first two-gun games arrive with the Model 3 and Flycast setups, since Time Crisis on System 22 is one player. |
| hotd2-vr (third party) | Yes, built in (`vr.DualWield`) |
| Theatre through an emulator | Only where the emulator takes two separate guns: `max_guns` under `[input.gun]` in its manifest ([emulator-manifests.md](emulator-manifests.md)). MAME, Supermodel and Flycast do. PCSX2 reads one absolute pointer, so two GunCons need a fork. |

The Hub shows a "2 guns" badge on a game only when its installed route supports it. The catalog's eligibility list (`python tools/gun_models.py`) is not that check, and no route has been accepted with two guns in a headset yet.

## 2. The gun model library (D46)

### 2.1 What we build

23 original models cover all 152 gun games. Tier 1 is built first.

| Model | Tier | Hold | Games | Resembles |
|---|---|---|---|---|
| `arc-pistol-slide` | 1 | one-hand | 15 | Namco's tethered cabinet pistols (Time Crisis series, with recoil; Point Blank series; Ninja Assault; Vampire Night) |
| `arc-pistol-twin` | 1 | one-hand | 17 | Sega's tethered cabinet pistols (Virtua Cop, The House of the Dead, Confidential Mission, Lost World and the NAOMI gun games) |
| `con-pistol-dpad` | 1 | one-hand | 12 | Namco GunCon 2 / G-Con 2 for PlayStation 2 (2001): A and B above the trigger, C under the grip, Select and Start on the left of the shaft, d-pad at the rear; black, orange and blue versions |
| `con-pistol-slim` | 1 | one-hand | 10 | Namco GunCon / G-Con 45 for PlayStation (1997): A and B buttons, one on each side below the barrel; black in Japan, grey and later orange elsewhere |
| `con-revolver-chunky` | 1 | one-hand | 4 | Sega Virtua Gun for Saturn (sold as the Stunner in North America): trigger and a Start button; the Japanese unit is black with a yellow trigger and Start button |
| `generic-pistol` | 1 | one-hand | 2 | Nothing in particular: the fallback when a game has no better match |
| `mnt-mg-heavy` | 1 | mounted | 21 | Cabinet-mounted machine guns (Sega Gunblade NY, L.A. Machineguns and the two-handed guns of Line of Fire; Midway Terminator 2 and Revolution X; SNK Beast Busters; Sega Let's Go Jungle) |
| `arc-pistol-optical` | 2 | one-hand | 4 | The plain optical pistols on Midway, Atari Games, Konami, Taito and Sammy cabinets (Area 51, Maximum Force, Jurassic Park III) |
| `arc-revolver` | 2 | one-hand | 2 | Konami Lethal Enforcers cabinet revolvers |
| `arc-rifle-selector` | 2 | two-hand | 8 | Cabinet rifles with a stock (Ghost Squad: fire selector, trigger and a fore-grip action trigger, force-feedback recoil); also the rifle-style guns on hunting and film-licence cabinets |
| `arc-shotgun-pump` | 2 | two-hand | 6 | Pump-action cabinet shotguns (The House of the Dead III, CarnEvil, Big Buck Hunter): pumping the fore-end reloads |
| `arc-smg` | 2 | two-hand | 6 | Cabinet submachine guns (The House of the Dead 4, Crisis Zone, Razing Storm): hold the trigger for automatic fire |
| `con-pistol-slot` | 2 | one-hand | 5 | Sega Dreamcast Gun (HKT-7800): pistol with one expansion slot for a memory unit or a vibration pack |
| `con-revolver-long` | 2 | one-hand | 4 | Konami Justifier (1993): revolver in the style of a Colt Python, blue, with a pink second-player gun; the PlayStation Hyper Blaster is green (black in Japan) |
| `mnt-smg` | 2 | mounted | 6 | Taito Operation Wolf (1987): Uzi-style gun that swivels and elevates on the cabinet, automatic trigger, grenade button near the muzzle, recoil vibration; also Operation Thunderbolt, Operation Tiger and Space Gun |
| `rifle-sniper-scope` | 2 | two-hand | 9 | Konami Silent Scope (rifle fixed to the console, with a scope that shows a close-up of part of the screen) and Namco Golgo 13 (sniper-rifle light gun with a scope) |
| `arc-blaster-toy` | 3 | one-hand | 1 | NERF Arcade foam-dart blasters |
| `arc-crossbow` | 3 | two-hand | 1 | The Walking Dead arcade crossbow |
| `arc-pistol-duty` | 3 | one-hand | 3 | Konami Police 911 / Police 24/7 and Lethal Enforcers 3 pistols (semi-automatic service pistol shape) |
| `con-pistol-subgrip` | 3 | two-hand | 2 | Namco GunCon 3 for PlayStation 3 (2007): sub-grip under the barrel on the left with an analog stick and two shoulder buttons, a second stick and two buttons at the rear, two more on the left; black in Japan, orange in the US |
| `con-remote-shell` | 3 | two-hand | 10 | Wii Zapper (2007): submachine-gun style shell with the Wii Remote in the barrel and the Nunchuk in the rear grip |
| `con-wand-rifle` | 3 | two-hand | 3 | PlayStation Move Sharp Shooter: rifle-style shell with an adjustable stock that holds the Move controller |
| `spc-hose-nozzle` | 3 | two-hand | 1 | Sega Brave Firefighters hose nozzle |

The "Resembles" column is a reference for the modeller and holds only facts we have a source for. Shapes of the cabinet guns are thinly documented in text; the modeller works from the shape notes in each model file and from photographs, and 14 game assignments are marked `review` until a source for the gun's shape turns up.

### 2.2 Lookalike, not copy

- Every model is an **original design** that evokes the real gun's silhouette, button layout and colours. Proportions are our own. No logos, brand names, model numbers or regulatory marks appear on a model.
- Model ids are descriptive (`con-pistol-slim`), never a trademark. The real product is named only in the `resembles` text.
- Models are never extracted from games, scans or other people's meshes. (hotd2-vr builds the agent's hands from the user's own game at run time; that is its choice, and nothing like it ships with us.)
- The references are toy-like game peripherals, not real firearms. The models stay toy-like.

### 2.3 Format

Same conventions as the control models ([controls-catalog.md](controls-catalog.md) §6.2): glTF 2.0 (`.glb`), metres, +Y up, −Z forward, built by a parametric Blender script. Files: `assets/guns/<id>.glb`, source `assets/guns/src/<id>.py`, metadata `data/guns/<id>.toml`.

Named nodes libacvr binds to:

| Node | Meaning |
|---|---|
| `grip` | Where the hand holds it. Aligned to the controller's OpenXR grip pose. |
| `muzzle` | Front of the barrel, −Z along the bore. The aim ray starts here. |
| `grip_two` | Second-hand hold (fore-grip, pump, rear handle). Optional. |
| `sight_front`, `sight_rear` | Iron sights, for players who aim down them |
| `slide_recoil` | Part that travels back on a shot (`limit_recoil` gives the travel) |
| `slide_pump` | Pump fore-end (`limit_pump`) |
| `pivot_trigger`, `pivot_selector`, `pivot_hammer` | Small moving parts |
| `pivot_yaw`, `pivot_pitch` | Mounted guns: the yoke's two axes |
| `button_<id>` | One per `[[button]]` in the model's file |
| `fx_muzzle`, `fx_shell`, `fx_laser` | Flash, ejected shell, laser origin |
| `screen_scope` | Scope lens that shows a zoomed render |

- **Materials:** `body` and `accent` take their colours from the model's `[tints]`; `dark` and `glass` are fixed. Each model lists named tints plus which one players 1 and 2 get.
- **Budget:** a gun sits close to the eyes, so up to 8,000 triangles at full detail, a second level at 2,000, one 1k texture set or vertex colours.
- **Recoil is visual.** The slide or the whole gun kicks, and the controller pulses; the aim ray does not move.

### 2.4 Which game gets which gun

1. `gun_model` under `[controls]` in the game's `game.toml`, or in a pack or user override.
2. The first matching rule in [`data/guns/defaults.toml`](../data/guns/defaults.toml): by game id, by console (the peripheral the release was made for), then by arcade maker.
3. For PC and Switch ports played with a mouse or stick, the arcade original's gun.
4. `generic-pistol`.

The player can override it: a different gun for one game, one favourite gun for every game, or a gun of their own. A user gun is a folder `user/guns/<id>/` with a `.glb` that has at least `grip` and `muzzle`, and a `.toml` like ours.

### 2.5 Where the gun is drawn

- **True-3D setups:** libacvr draws each gun at its controller, after the game's scene and depth-tested against it, so the gun is in the world with you.
- **Theatre:** the same guns, in front of the screen.
- **Control Mapping mode:** the gun with every mappable part labelled; pull the trigger, press a button or pump the fore-end and watch the part move ([controls-catalog.md](controls-catalog.md) §7).
- Buttons on the gun map to controller buttons through the game's control set: a GunCon lookalike's A and B light up when you press the buttons they are mapped to.
- `show_gun = false` hides the model and leaves the laser, for players who prefer it.

### 2.6 Licence (D47, default)

Scripts are GPL-3.0 like the rest of the repository. The built `.glb` models are released under **CC0-1.0**, so any VR port can ship them whatever its own licence is. (hotd2-vr had to move its builds to GPL-3.0 because of one CC BY gun model.)

## 3. What changes elsewhere

- **libacvr:** load a gun model per slot; align `grip` to the grip pose and take the ray from `muzzle`; per-slot tint, gun angle and laser; recoil and button events animate named nodes; the second slot follows the two-gun policy (faint until player 2 joins); a pedal input per player. These go into the contract after the M1 pull request merges, since that pull request edits the contract file.
- **Game schema:** optional `gun_model` and `two_guns` under `[controls]` (same timing, same reason).
- **Hub:** a "2 guns" badge, the gun picker in a game's settings, and the gun shown on the detail page's controls section.
- **Recipes:** third-party setups keep their own guns. hotd2-vr draws its own.

## 4. Order of work

| When | What |
|---|---|
| Now (needs no GPU) | Build tier 1 models from scripts: `generic-pistol`, `arc-pistol-slide`, `arc-pistol-twin`, `con-pistol-slim`, `con-pistol-dpad`, `con-revolver-chunky`, `mnt-mg-heavy` ([tasks/m2/guns-models.md](tasks/m2/guns-models.md)) |
| M2 (namco22-vr) | libacvr gun model loader and renderer; Time Crisis with `arc-pistol-slide`; gun in Control Mapping mode |
| M3 (theatre) | Two guns through MAME and Flycast; console guns for PS1, PS2, Saturn and Dreamcast releases; tier 2 models |
| M4 (supermodel-vr) | Two guns in true 3D (Lost World, L.A. Machineguns, Ocean Hunter); mounted guns |
| Later | Tier 3 models; a two-view setup for the six separate-view games |

## 5. Open points

1. Shapes and cabinet colours of most arcade guns have no text source yet (14 `review` assignments). Photographs or operator manuals would settle them.
2. Whether the Time Crisis 4 cabinet gun has a weapon-select button on the gun itself.
3. PlayStation 2 Time Crisis II and 3: confirm that every two-player mode uses separate views.
