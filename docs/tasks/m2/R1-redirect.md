# M2 R1: redirect after the first block

Outside review of `next` at `4b2efe6` (draft PR #9, 23 merged lane pull requests, seven read-only reviewers). Read this before resuming any lane. It replaces the "next target" in each brief; the briefs' goals and ownership stand.

## What the review found

The work is careful, honest about its limits and clean (CI green, no private data, licences in order). But after the first block **nothing a player can see exists**: no gun model has been built, libacvr cannot open a headset session, no real game geometry has been drawn, and the overlay is expected to feel about the same in the headset. Three causes:

1. **Approvals went unanswered, and the lanes built around the gap** instead of stopping for it. The stereo lane re-implemented upstream's renderer on the CPU; the lead extended a runtime core nothing can drive; the guns lane wrote plumbing around models that were never built.
2. **Caution was applied to decisions already made.** Bindings the owner decided (cover on the grip, Start to join) were left "pending" because no backend had "verified" them.
3. **Process outgrew product:** 69 status reports and lanes reviewing other lanes in about six hours.

By area:

| Area | Verdict |
|---|---|
| Hub integrity (rollback lock, scan isolation, recovery, shared locks, readiness labels) | Real fixes with tests. Sound. |
| Overlay input | Position, scroll target and cancel fixed; the freeze, click slop, drag threshold and smooth-scroll scale from [ui/06](../../ui/06-vr-menu-controls.md) were left out. A wrong Y direction would explain every symptom the owner saw, and has not been tested. A new focus gate can leave the panel dead. |
| libacvr | View and aim maths correct. Nothing runs: `acvr_runtime_create` returns unsupported. About a third of the code is machinery nothing can exercise. The gun was aimed along the grip pose (the handle), which was an error in the model brief, now corrected in [guns.md](../../guns.md) §2.3. |
| System 22 stereo | Core maths matches upstream and DR-89. No path takes real upstream data to an image. Materials, fog, fades and composition were re-derived from upstream's renderer instead of patching it, and the depth model (z-test) contradicts the board (painter's order). |
| Guns and controls | Build pipeline and checker are real; no model built; all seven "models" are one box blockout. Control sets carry four rows for every game and 1,581 open gaps; the completeness tool fails and CI does not run it. Tests assert the unfinished state. |

## Changes of method (all sessions)

1. **The real thing first.** A mock is for what cannot run, not a substitute for asking. If the next real step needs an approval, stop that step, say so in one line, and do other real work.
2. **Every working block ends with something a person can look at:** a picture, a window, a frame, a list a player would read. If a block cannot, it is the wrong block.
3. **Decisions in the design docs are defaults, not hypotheses.** Write them into data and code. Mark only what is truly unknown.
4. **One report per checkpoint or blocking question,** not per block. Half a page. Lanes do not review other lanes; the lead reviews.
5. **CI runs each lane's completeness check,** and no test may assert that work is unfinished.
6. **Prefer deleting to parking.** Code that the redirect below makes unnecessary leaves the tree; git keeps it.

## Standing approvals

These replace one-at-a-time requests **once the owner has confirmed them in chat**. Until then they are not granted. Anything not listed still needs asking.

| # | Approval | Scope |
|---|---|---|
| 1 | Run the installed Blender headless on the CPU | Building gun and control models and their previews from our scripts, as often as needed |
| 2 | Download and build the official OpenXR SDK | From the Khronos GitHub release, pinned by version and hash, into `.local/sdk/` |
| 3 | Use the SDL2 archive already downloaded, and a GCC/MinGW toolchain if one is needed | Unpack into `.local/sdk/`; ask before installing a compiler system-wide |
| 4 | Use the GPU for short checks | Hidden or desktop windows, shader compiles, side-by-side output, when the owner is not rendering |
| 5 | Read the owner's Time Crisis set | Stereo lane and lead only, that one game, to build and run upstream's engine locally; nothing derived from it leaves `.local/` |
| 6 | SteamVR and the headset | Only in a sitting with the owner present |

## Next target per session

### Lead

1. **A gun visible in the headset with no game.** Build the OpenXR host (session, swapchains with depth, Touch actions, haptics), wire `acvr_runtime_create`, and draw a clear colour plus the gun at the hand. A box stands in until lane 3 delivers a model.
2. **Gun direction:** position from the grip pose, direction from the aim pose plus the per-hand angle. Test with grip and aim poses that differ.
3. **Errors:** separate fatal from skip-this-frame. One bad ray or repeated time must not blank the headset.
4. **Shot placement tests:** four corners, a moved and rotated camera, an inset viewport.
5. **Freeze and cut the ABI** to what a backend needs today: create, per-eye draw, one aim call, inputs, haptics. Remove multiview, HUD, Vulkan and D3D11 fields, size tails, gun policy and the output router until something uses them.
6. **Button budget** (decide, then tell lane 3): trigger fires; grip is the pedal; A/X is Start and join; B/Y is the special, grenade or weapon switch; thumbstick click toggles the laser; thumbstick directions are the d-pad; coin is free play by default with a hold on A/X.
7. **Authored (VR) variants must launch** from the UI, the command line and Steam. Today every path refuses anything but generated flat variants.
8. One control-set parser, shared, instead of three.

### Stereo boards

1. **Stop re-implementing upstream's renderer.** Remove `gl_renderer`, `texture_plan`, the material sampler, `composition` and their tests. Keep the scene maths, the patch pinning tool, the worker and the analytic tests.
2. **Use upstream's own split:** it already prepares a frame once and can draw it more than once. Patch its draw to run once per eye with a per-eye view, as DR-89 does with about a dozen edits.
3. **Capture view-space X, Y, Z before projection** in `geo_hw` instead of unprojecting screen coordinates.
4. **Depth as the board does it:** painter's order with the board's sort key; write depth only so the gun's ray has something to hit.
5. **World scale:** 1,000 units per metre as a labelled starting value; the frustum from the game's camera, not fixed near, far and field of view.
6. **Aim through upstream's own `aim` host callback.** The gun flash is a screen fade with an existing switch, not polygons.
7. First, with no approval needed: a converter from upstream's captured quads to our scene, upstream's `geo_hw` compiled with it, a formula-built display list in, a side-by-side picture out.
8. Then, with approvals 3 to 5: build stock upstream, add the per-eye patch, and show Time Crisis side by side in a desktop window.
9. Fix the README's claim that no upstream source is included (the patch manifest and one sampler transcribe upstream code): attribute it.

### Guns and controls

1. **Build one model and look at it** (approval 1): `generic-pistol`. Fix what the preview shows. The review's reading of the build plan expects the grip to rake the wrong way, button caps to float off the body and several buttons to share one spot; confirm against the real picture. Then give each tier 1 model its own shape function, not one function with branches.
2. **Fill the control sets by rule:** one base set plus per-model differences, player 2 generated as the mirror.
   - Start and join on A/X for every game.
   - Cover on the grip for every game whose catalog entry has a cover pedal; those games reload by releasing it.
   - No reload row on mounted guns or the hose.
   - Selector, grenade, special and the GunCon-style A/B/C per the lead's button budget.
3. **Gate CI on `tools/control_sets.py` reporting no gaps.** Flip the tests that assert gaps, no cover or no preview.
4. Promote and delete `data/controls/proposed-game-overrides/`. Remove the unused export path.
5. **The view:** leader lines to parts instead of pills on top of them; player wording ("Reload: flick up"), never "pending" or "awaiting validation"; no empty preview box when there is no image; nothing at all on non-gun games.
6. `gun_model` may name a user or pack model: the tools must accept ids outside the built-in list.

### Hub

1. **Make the diagnostic page settle everything in one sitting.** Live switches for Flip Y, press freeze (ms), click slop (px), drag threshold (px) and smooth-scroll scale (120 or 2880). Show where the press and the release landed, the drift between them, and the last twenty events. Readable in the headset, not 11 px.
2. **The focus gate:** a valid move inside the panel counts as focus. Losing an "up" must not swallow the next click.
3. **Remove the multi-cursor and chord machinery** unless the multi-cursor overlay flag is actually set and SteamVR is seen to send more than one cursor.
4. **Stuck launch marker:** reclaim it when its holder is dead, and give the user an in-app way to clear it.
5. **Shared lock location:** resolve the profile folder to its real path before checking for links, so a relocated profile does not disable installs and launches. Remove lock folders when done. Tests must not write into the real user profile.
6. With the lead: authored variants launchable (lead item 7).
7. The table-shaped invalid variant that still hides a generated route, then the remaining small items, then the find-my-files flow from the brief.

## First headset sitting (when at least two lanes have something)

1. `--overlay --window --spike`: hit the four corner targets and compare the marker with SteamVR's laser dot; toggle Flip Y; scroll and read which events arrive and how large.
2. Set the switches until ten cards in a row open from the grid, the stick scrolls and every panel key types. Record the values.
3. The gun in the hand with no game: both hands, the angle, the laser, recoil that does not move the aim.
4. Time Crisis side by side on the desktop.

## Merging

PR #9 stays a draft until checkpoint 1 is real: a gun visible in the headset from libacvr, Time Crisis side by side on the desktop, one built gun model, and the overlay passing its checklist.
