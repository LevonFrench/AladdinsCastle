# Session 1: Lead. Time Crisis in true 3D from the Hub

You are the lead of four Codex sessions. The owner is giving you three other sessions (Hub, Guns and controls, Stereo boards). They report to you in files; you direct them in files. Read [README.md](README.md) first: it has the lane table, the rules that bind every session and the report format.

## Your two jobs

1. **Direct.** Read each lane's reports, answer its questions, set its next target, decide merge order, and keep one combined headset checklist for the owner.
2. **Build the spine yourself:** libacvr v0 and the path from a Hub install to a game running in the headset.

## Goal

The owner installs "Time Crisis (true 3D)" from the Hub, presses Start in VR, and plays it in the headset with a tracked gun drawn in the hand. Then the same for the next games as lane 4 delivers boards.

## Day one and two: publish what the lanes build against

1. Create `next` from `main` and push it. All lane pull requests target `next`.
2. **libacvr header v0.1** (`libacvr/include/acvr.h`, [libacvr-contract.md](../../libacvr-contract.md)): add what [guns.md](../../guns.md) §3 lists (a gun model per slot, grip and muzzle nodes, per-slot tint, gun angle and laser, recoil and button events that animate named nodes, the two-gun policy, a pedal per player). Keep the ABI rules already in the contract.
3. **Renderer interface for lane 4**: the functions a board backend gives libacvr (draw the scene for one eye given view and projection; cast a ray into the scene and return the hit point plus the gun coordinates the game expects; report world scale and the HUD layer) and what libacvr gives back (poses, timing). Lane 4 builds against this from its first day, with side-by-side desktop output standing in for the headset.
4. **Control-set format** with lane 3 (cabinet part → game input → default controller binding), and the `gun_model` / `two_guns` fields in [game-schema.md](../../game-schema.md) with both validators.
5. The first `docs/status/lead/` directions file: targets for each lane's first checkpoint.

## Then build

- **libacvr v0** (MIT, C ABI, Vulkan with an OpenGL path where a backend needs it): OpenXR session and frame loop, controller poses and inputs from the Touch profile, recentre and height, gun slots with aim rays, drawing gun models at the grip pose, haptics, a pause overlay. Tests without a headset: a null OpenXR backend that feeds recorded poses and checks frames, rays and events.
- **Hub side of a VR setup** (with lane 2, who owns `hub/`): a VR variant is installable and launchable from the normal UI, with truthful labels (agency finding A2: catalog capability, recipe, installed and ready are different things). Runtime pinning stays per launch.
- **Integrate lane 4's System 22 renderer** into the OpenXR loop: Time Crisis with `arc-pistol-slide` in the hand, cover on the grip button, reload off-screen.
- **Quick wins that need little code,** each only with the owner's go-ahead: the hotd2-vr recipe end to end (it needs the owner's own Dreamcast disc and a download approval); Dolphin's built-in stereo shown on a 3D screen.
- **Second target:** the input bridge that makes VR controllers two light guns for flat emulators (MAME first, through its Lua input hook), so every MAME gun game is playable with tracked guns before its board gets true 3D.
- **Releases:** versioned portable builds with the build's source commit and file hashes (agency RC-04), and a short tester quickstart.

## Directing

- Fetch lane branches, read `docs/status/<lane>/`, reply in `docs/status/lead/` on `next`.
- Review lane pull requests into `next`: tests present, CI green, nothing outside the lane's folders, no private data, no game content.
- Open `next` → `main` pull requests at the two checkpoints in [README.md](README.md) and at each board after that. Those get an outside review before the owner merges. You do not merge to `main`.
- Keep the combined headset checklist current. Ask the owner for a sitting only when more than one lane has something to check.
- When lanes disagree or a contract must change, decide, write the reason down, and bump the contract version.

## Done when

- Time Crisis is played start to finish in true 3D on the owner's PC from a Hub install, at the headset's refresh rate, with the gun model in hand.
- The three lanes' first and second checkpoints are merged into `next` with green CI.
- The owner has a one-page status of what works, what is unverified and what is next.
