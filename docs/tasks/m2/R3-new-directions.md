# R3: after the first headset sitting, new directions

Status: 2026-10-11. Owner decisions D54 to D61 ([../../workshop.md](../../workshop.md)). Read after [R2-first-image.md](R2-first-image.md). Where this page differs from an earlier brief, this page wins. The owner is not available for headset testing for now: **prepare, prove what a desktop can prove, and queue the rest for one sitting. Nothing is called accepted without the headset.**

## What the first sitting showed

The pinned DolphinXR build, Umbrella Chronicles, default and eager modes, then the VR menu check.

| Seen | Cause | Certainty |
|---|---|---|
| A picture appears. The safety screens, logos and the save prompt sit on a flat panel fixed in the room. The pointer follows the controller and lights the button under it | DolphinXR as designed: flat content goes to its screen panel and the pointer is fed directly | Seen |
| Nothing could be pressed, so the game never left the save prompt. **The 3D world has not been seen** | The setup installed the Wii Remote with a source and no device or button bindings. The build ships a bindings profile for VR controllers that was not used | High |
| The picture was behind the player | DolphinXR takes its home position when it starts, and it was started at the desk. Its recentre exists only as a keyboard hotkey | High |
| Coarse, jagged picture | Internal resolution was left at the Wii's native size, with no anti-aliasing or filtering | High |
| The VR menu was drawn upside down and its Flip Y control could not be operated | The overlay picture is flipped top to bottom in the diagnostic build, and the control sits inside the flipped panel | Seen; cause to be found |
| No log from either run | No log folder was created and the program's output was empty | Cause unknown |
| The Hub lane's script could not run | It needed PowerShell 7; the machine has Windows PowerShell 5.1 | Seen |

## Decisions

| # | Decision |
|---|---|
| D54 | **A gun per game.** Each game names the gun that fits it. The console shell and its stick controller are still built: they are the faithful set and the picture on the controls page |
| D55 | **The default hold is per game:** split (gun in one hand, stick controller in the other) or docked two-handed. It is a field of the game's control set |
| D56 | **Build the gun test room now:** a VR room of our own with no emulator |
| D57 | **No simulator runtime is installed.** SteamVR's own null driver stays an option under the conditions in R2 |
| D58 | **Link's Crossbow Training moves up** beside Umbrella Chronicles as a second showcase |
| D59 | **Pinball gets its own tab:** only VR-ready Visual Pinball tables from the user's own collection ([P-pinball-tab.md](P-pinball-tab.md)) |
| D60 | **All the workshop ideas are accepted,** in the order below |
| D61 | **Never stall.** When Wii work is blocked on a headset sitting or on the owner, sessions move to the next unblocked item on this page, and after that to the arcade boards in the D49 order (System 22 first, where that work is parked). They come back when the block clears |

## Order of work

### 1. Fix what the sitting found (no headset needed)

| Lane | Work |
|---|---|
| Lead | The setup and every sitting script install working button bindings, generated from the game's control set. The picture appears in front of the player once the headset is worn, and a controller button recentres it. Sharper defaults: internal resolution up, anti-aliasing and filtering tried on the desktop first. A log file that exists. Every script handed to the owner runs on Windows PowerShell 5.1 and is tested there |
| Hub | Find why the overlay is upside down (the M1 overlay was upright), fix the picture, then the pointer. Flip Y becomes switchable from outside the panel. This blocks every in-headset use of the Hub, the Pinball tab included |

### 2. Use what DolphinXR already has

Read from its source; none tried yet. Wire each into the setup as a switch and into the next sitting's list.

- **Forced VBI Frequency** (Auto, Off, 72, 90, 120 Hz): the game's video rate at the headset's rate. This is route A of [W-wii-framerate-presets.md](W-wii-framerate-presets.md), already built. The frame rate lane tests it before writing anything of its own.
- **Desktop mirror view** (both eyes, left, right, none): how a session sees each eye without a person in the headset.
- **Controller Anchor:** moves a game object to a controller, position only. **Camera Anchor with smoothing:** moves the VR camera to a game object and damps its motion, a candidate for the Darkside stabiliser.
- **Per-game VR profiles:** the build ships dozens and none for our five games. Ours are tuned in the headset and offered upstream.
- **Save states:** the owner saves one at the start of the first mission; later sittings start from it.

### 3. Scripted input, so desktop runs need no hands

Our own DSU server can press buttons from a script. Use it to get past prompts and through a rail section on the desktop without anyone touching the mouse, keyboard or headset. It unblocks the title-screen proof, the draw classification in the first mission and the frame rate measurements.

### 4. The gun test room (D56)

A small VR room drawn by libacvr in its own OpenXR session, started from the Hub. It needs the public runtime creation that is still unsupported, which is the piece everything else needs too.

- Any gun model at the hand, lit, with a hand on it.
- Targets to shoot, with the ray shown.
- Grip angle set per hand and saved.
- Hold a button and a label appears on each part of the gun: the part, the game action, the controller button.
- Two-handed aiming along the line between the hands when the off hand is on the second grip.

Without a headset: it builds, its maths and drawing pass headless tests, and its eye picture can be captured. Holding it is for the sitting.

### 5. Guns and controls

- The console shell and the stick controller (the lane's current target).
- **A gun for each tier 1 game (D54).** The lane proposes one per game with a line of reasoning and shows them on one sheet; the owner confirms.
- The default hold per game (D55).
- Hands on the gun and on the stick controller.
- Label anchor points on every model for the test room and the controls page.
- Reload choices per game: shake, flick down, point off-screen, or a button.
- One source of truth: the control set generates the controls page, the DolphinXR bindings and the DSU profile.

### 6. Pinball tab (D59)

[P-pinball-tab.md](P-pinball-tab.md). Hub lane, after the overlay fix is under way. Scanning and the tab need no headset.

### 7. Link's Crossbow Training (D58)

Catalog entry, control set and profile beside Umbrella Chronicles. It needs the owner's yes for the disc before any session reads it.

### 8. Later, most per-game work

- The gun changes with the weapon selected in the game (read from the game's memory, per region).
- Holsters: reach to the hip or shoulder to switch weapon or take a grenade.
- The head drives the game's look stick, to see whether it reduces things vanishing at the edges.

### 9. When all of the above is blocked (D61)

The arcade boards, in the D49 order. System 22 first: pick up where [4-stereo-boards.md](4-stereo-boards.md) and [R1-redirect.md](R1-redirect.md) left it. Then Model 3, NAOMI and Dreamcast, System 23. The same rule applies there: real picture first, headset acceptance later.

## The next sitting, when the owner is back

One list, in this order. Each item is a few minutes.

1. Umbrella Chronicles: press through to the first mission and save a state there.
2. The three things in [../../3d-mode-target.md](../../3d-mode-target.md): the HUD, the wide frame, and what a gun would need.
3. Both heartbeat modes, then Forced VBI Frequency on Auto: smoothness, and whether the game speeds up.
4. The VR menu, upright: corners with both controllers, the keyboard page, the cards page.
5. The gun test room: hold each gun, set the grip angle, read the labels, try two-handed aim.
6. The Pinball tab: pick a table, play, exit back.
7. The fallback viewer, only if a real capture has been seen on the desktop.
