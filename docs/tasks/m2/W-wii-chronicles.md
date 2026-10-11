# Main thrust: Wii games in 3D mode, top five first

Status: 2026-10-10, owner decision D52. This replaces "Time Crisis on System 22" as the lead's main thrust. System 22 keeps its place as the next board and is parked where it stands. The rules in [README.md](README.md) and the method in [R1-redirect.md](R1-redirect.md) still apply.

## Goal

**Resident Evil: The Umbrella Chronicles** and **Resident Evil: The Darkside Chronicles** (Wii), played in the headset in **3D mode** as defined in [3d-mode-target.md](../../3d-mode-target.md): the HUD on a steady panel, the world filling a wide frame, the gun in the world at the hand. The VR controllers are the Wii Remotes, with full motion: aim, fire, shake to reload, swing the knife, shake for quick-time events, Nunchuk stick and buttons. One gun first, then a gun in each hand for two-player co-op.

**Order of games (owner, 2026-10-10): the top five for sure, then the next five, and so on.**

| Tier | Games | Notes |
|---|---|---|
| 1 | Resident Evil: The Umbrella Chronicles, Resident Evil: The Darkside Chronicles, The House of the Dead: Overkill, Dead Space: Extraction, Sin & Punishment: Star Successor | Start with Umbrella Chronicles (steady camera, no Dolphin problems listed), then Darkside. Sin & Punishment gets a dedicated custom control setup |
| 2 | Ghost Squad, The House of the Dead 2 & 3 Return, Link's Crossbow Training, Gunblade NY & L.A. Machineguns Arcade Hits Pack, Wild West Guns | Wild West Guns is WiiWare and not in the catalog yet |
| 3 | Zombie Panic in Wonderland, Nerf N-Strike Elite, Heavy Fire: Afghanistan, Big Buck Hunter Pro, Cabela's Dangerous Hunts 2011 | Add the missing ones to the catalog when tier 2 is under way |
| 4 | Toy Story Mania!, Target: Terror, Attack of the Movies 3D, Martian Panic | Full-motion-video games stay out of scope |

A tier is done when each of its games passes the acceptance list in the target with its own control set. Do not start a tier before the one above it is playable.

## What is known (research 2026-10-10)

Hub wiki topic `vr-arcade-gun-racing`, article `wii-games-in-vr-dolphin`. "Unverified" means no fetched source states it.

**Renderers**

| Option | What it gives | State |
|---|---|---|
| Mainline Dolphin stereoscopy | A stereo picture (side by side, top and bottom and others; settings `StereoMode`, `StereoDepth`, `StereoConvergence`, `StereoSwapEyes`, `StereoPerEyeResolutionFull` under `[Stereoscopy]`). Each primitive is emitted once per eye with a shift that scales with depth. | Shipping. A picture on a screen: the fallback tier, not 3D mode |
| **DolphinXR** (`iChris4/dolphinXR`, releases named DolphinVR Redux; GPL-2.0-or-later per its page) | OpenXR on Windows with Direct3D 11 and 12, Vulkan and OpenGL; head-tracked rendering inside the scene; a 2D "screen pane" override; camera and controller anchor overrides; a configuration panel inside the headset; per-game VR profiles; texture-hash-based element overrides; a VR input tester for the Wii Remote; "Wiimote pointer now uses absolute pointing" (v0.4) | Active. **No report found of any pointer shooter running in it.** How it treats the HUD, whether a VR controller aims the pointer, and whether two remotes work are unverified |
| Mainline OpenXR pull requests (#8380, #12564) | #8380 replaced the game's projection with per-eye view matrices on Direct3D 11 | Both closed unmerged in 2024 |
| DorsalVR | An outside program that mirrors Dolphin onto a virtual screen and sends controller motion over DSU | Fallback-tier idea only |

**Input, with no fork needed.** Dolphin's emulated Wii Remote pointer is a cursor bound to four direction inputs and is absolute by default. Dolphin's DSU client (the cemuhook UDP protocol, 127.0.0.1 port 26760 by convention) exposes sticks as absolute axes plus accelerometer (in g) and gyro (degrees per second). A program of ours can act as a DSU server with one slot per hand and supply the pointer position, real motion and buttons. Each emulated remote has its own pointer, so two hands can be two remotes.

- In Dolphin: Alternate Input Sources, DSU Client on; Wii Remote 1 and 2 emulated; Point bound to the DSU device's right stick with Relative Input and Auto-Hide off; the Motion Input tab's Accelerometer and Gyroscope bound to the DSU device; Shake and Swing left unbound so motion is not counted twice.
- Save the bindings as Wii Remote profiles and select them per game in the game INI: `[Controls]` `WiimoteProfile1`, `WiimoteProfile2`.
- Limits: a DSU stick axis is 8 bits (256 steps across the screen), DSU carries no rumble, and its touch pad is relative so it cannot carry the pointer. Two slots on one server need testing (an old cross-talk bug is reported). Inside a fork, feed the emulated remote directly at full precision instead; keep the DSU server as the no-fork route and for the fallback tier.

**The games in Dolphin**

| | Umbrella Chronicles | Darkside Chronicles |
|---|---|---|
| Game ids | RBUE08, RBUP08, RBUJ08, RBUK08 | SBDE08, SBDP08, SBDJ08, SBDK08 |
| Dolphin wiki rating | 4 stars, playable; no problems listed | 4 stars, playable; dialogue can repeat (use DSP LLE), slowdown at chapter starts, bloom problems tied to EFB copy settings |
| 60 fps code (untested by us) | `04158204 60000000`, "a few minor problems" | `04736D44 00000001` |
| Co-op | Two players, one shared view, a reticle each; unlocked by finishing a mission; needs two Remotes and two Nunchuks | Two players on one shared view with separate reticles, or one player with an AI partner |
| Camera | On rails, mostly steady | On rails and deliberately shaky; reviews call it the game's main flaw. No in-game option found |

**Controls, as far as sources go** (no official manual was found; fill the gaps from the game itself on first run)

| Action | Umbrella Chronicles | Darkside Chronicles |
|---|---|---|
| Aim | Pointer | Pointer |
| Fire | B | B |
| Reload | Shake the Remote | Unverified |
| Pick up, interact | A on highlighted objects | A |
| Look around | Nunchuk stick, limited range | Unverified |
| Melee, knife | A Remote motion on some enemies; exact input unverified | Unverified |
| Grenade | Unverified | A selectable weapon |
| Change weapon | Unverified | D-pad or Nunchuk stick |
| Quick-time events | Button press or shake | Timed prompts; shake unverified |
| Zapper layout | Supported | Recommended by one review |

## Plan

Work with the owner present; the headset and GPU are available. Each step ends with something the owner can see.

1. **See what exists, in one short sitting.** With the owner's go-ahead for the download and for reading the Umbrella Chronicles disc, run DolphinXR's current Windows release with Umbrella Chronicles in the headset. Score it against the three things in the target and note the frame rate, what the HUD does, whether a VR controller aims the pointer, and whether the 60 fps code holds. Write the result down with screenshots kept in `.local/`.
2. **Choose the base and say why.** Either build on DolphinXR (our changes in a GPL fork, offered upstream) or add our own OpenXR path to mainline Dolphin with libacvr. Decide on four questions: can 2D draws be told apart and sent to a panel; can the per-eye projection and the game's culling be widened; can libacvr draw the gun with depth against the scene; can our input feed reach the emulated Wii Remotes. Dolphin is GPL-2.0-or-later, so the fork is GPL; MIT libacvr may link into it.
3. **The three things on Umbrella Chronicles.**
   - HUD: orthographic (2D) draws go to a panel fixed in the play space, framed as the original.
   - Wide frame: per-eye projection from the headset; widen the game's field of view and culling through a per-game profile (settings, then codes, then a patch, in that order of preference).
   - Gun: libacvr draws the gun model at the hand in the same space. Default model `generic-pistol` until a Zapper-style shell is built; the player can pick any.
4. **The Wii Remote from the VR controller.**
   - Pointer: cast the gun's ray into the scene (or onto the HUD panel when it is aimed there), project the hit through the game's camera, and send that as the absolute pointer position.
   - **Real motion:** derive the Remote's accelerometer and gyro from the controller's own movement in the gun's frame, so shaking the gun is shaking the Remote. Reload, knife swings and quick-time shakes then work as they did on the console. In one-gun play the other hand is the Nunchuk, with its own motion.
   - Every motion also has a button, for seated play: a reload button that sends a shake, a melee button that sends a swing.
   - Defaults: trigger = B (fire); A/X = A (pick up, interact); grip = Z; B/Y = C; thumbstick = Nunchuk stick; thumbstick click = laser; menu = +.
   - A gun in each hand: the second controller is the second Remote with the same set.
5. **Darkside Chronicles.** Same, plus a camera stabiliser as a comfort option (smooth the game camera's rotation before it reaches the headset), on by default for this game.
6. **The rest of tier 1,** each game with its own profile and control set: The House of the Dead: Overkill, Dead Space: Extraction, then Sin & Punishment.
7. **Sin & Punishment: Star Successor needs a dedicated custom control setup** (owner, 2026-10-10). It is not a gun game in the same sense: the player moves an on-screen character with the stick while aiming with the pointer, with dodge, melee, lock-on and charged shots. Write its control set for that game alone, work out with the owner in the headset where the character and the aim sit in 3D mode, and do not force the generic gun set onto it.
8. **Then tier 2, tier 3 and tier 4,** in that order.

## Sidecar sessions

| Session | Its part of this |
|---|---|
| **Stereo boards** → Dolphin 3D mode | Plan steps 2 and 3 inside the chosen base: the HUD split, per-eye projection, widened field of view and culling, per-game profiles. System 22 is parked as it stands |
| **Guns and controls** | Control sets for the Wii games (Remote and Nunchuk part → game action → VR controller), including the motion gestures and their button fallbacks; the view that shows them; commit the second gun batch; bring the Zapper-style shell forward; the custom Sin & Punishment set |
| **Hub** | Find and identify Wii discs by the ids above; locate an existing Dolphin (the official download channel is not verified, so no automatic install yet); install and launch an authored VR variant for these two games; keep going on the VR menu fixes |
| **Lead** | Steps 1, 2 and 4, libacvr's OpenXR host and gun drawing, integration, sittings with the owner |
| **Frame rate and presets** (new) | Each tier 1 game at the headset's refresh rate, and starting presets per class of headset: [W-wii-framerate-presets.md](W-wii-framerate-presets.md). Its first answer feeds the lead's gun drawing: the pinned base does not redraw between game frames, so a gun drawn into the game's picture moves at the game's rate |

## Done when

- First checkpoint. Umbrella Chronicles: one full chapter in the headset that passes the acceptance list in [3d-mode-target.md](../../3d-mode-target.md), reload by shaking, at the headset's refresh rate, first with one gun and then with a gun in each hand.
- Darkside Chronicles: the same, with the stabiliser.
- Both installed and started from the Hub.
- Tier 1 complete: all five games pass the acceptance list with their own control sets, Sin & Punishment with its custom setup.

## Limits

- The user supplies the discs. Nothing from a disc, and no Nintendo system file, enters the repository or a report. Ask before reading any disc other than these two.
- Ask before each third-party download. Codes from cheat sites are for our own testing and are listed in a game's profile only with their source and a tested-on date.
- Nothing above has been run by us yet. Treat every row marked unverified as a question for the first sitting.
