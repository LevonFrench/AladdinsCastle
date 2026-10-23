# Verification of the website fix pass (uncommitted diff on c54ca59)

Method: read the full diff, the three JS files, the three pages, `website/tools/make_games.py`, `docs/status/current.md` and the repo sources behind the claims. `node --check` passed on `website/arcade.js`, `website/bench.js` and `website/models.js` (observed). `make_games.py` regenerated into a scratch folder is byte-identical to `website/games.html` (observed). No browser was run, so rendering findings are inferred from source. Nothing in the site was edited.

## A. Done / partly / not done

Studio actions (`docs/reviews/2026-10-22-studio-experts/SUMMARY.md`)

| # | Verdict | Evidence |
|---|---|---|
| 1 Composition | Done, with one gap | `data-side` on stops 1-8 (`website/index.html:86-246`); CSS `website/style.css:197-203`; yaw offset `website/arcade.js:1185-1188`; spotlight `arcade.js:1385-1395` and `style.css:165-168`; phone lift `arcade.js:1183-1184`, `style.css:372`; kiosk guns +-0.6 `arcade.js:1096-1097`. Gap: scrim periphery peaks at 0.45 (`arcade.js:1384`), the plan said about 0.30 (observed). |
| 2 Failure-proof loading | Done | `sceneStarted` gate `arcade.js:48-51,1417`; bench catches `bench.js:52,64` and `start()`; `allSettled` `arcade.js:1007,1038`; failed promises evicted `models.js:14-16,24-26`. |
| 3 Colours | Done | suffix strip `models.js:62`; controls keep GLB colour `models.js:42-52`; glove tint synced `tools/sync_models.py:75-79` and `models/models.json` (matches `assets/hands/grip-hand-right.toml`); invented fallback removed. |
| 4 Bench look | Partly | Lambert, no tone mapping, camera-space light 0.35/0.65 x PI matches `assets/guns/runtime-lighting.json` (observed). No scale cue was added (the model-presentation finding 9); only the old `length_mm` text remains. |
| 5 Load and draws | Done except compression | `whenNear` `arcade.js:976-978,1374`; `visible=false` `arcade.js:1009,1013`; centring `arcade.js:941-943`; ghost depth pre-pass `models.js:65-69`. No GLB compression (deliberate). |
| 6 Serving | Done | `website/_headers`, `website/404.html`, `?v=20261023c` consistent across `models.js` `VER`, both JS imports and every page (observed). |
| 7 Accessibility | Done | `aria-pressed` `bench.js:44,59`; `aria-live` `index.html:130`; nav labels `index.html:34,41`; skip link `index.html:24`; gun click targets. |

UX findings (`ux-panels.md`)

| # | Verdict |
|---|---|
| 1 Glossary | Done: `index.html:106-114`. "Theatre", "diorama" and "surround view" are gone from all pages (grep, observed). |
| 2 E-pill legends | Partly: one-line legends at `index.html:180,200`; pills say "E2 · offline proof". Mixed pills ("E3 ... · rest E2") and all-caps (`style.css:237`) remain. |
| 3 First screen | Mostly: Windows, SteamVR and "none are included" now in the lede (`index.html:68`, about 55 words, not 35); 561 stat swapped for "3 accepted". "12 systems, from Model 3 to Wii" (`index.html:79`) is kept although the review called it wrong. |
| 4 Reading order | Not done (deliberate, scene-wired). |
| 5 Trim panels | Not done except the Model 2 claim (`index.html:185`, now correctly scoped to House of the Dead). The rules panel (`:103`), the 59-word head (`:125`) and the stop 4 and 5 panels are unchanged. |
| 6 Specific titles | Not done. |
| 7 Status scales | Partly: key note at `how-it-works.html:37`. Flow steps still use green/amber/grey "Working / In progress / Not yet", the same colours as E3/E2/E1. |
| 8 Accent colours and caution notes | Not done (`games.html` accents and note styling unchanged). |
| 9 Saturn/GameCube split | Not done. |
| 10 Duplication | Mostly: "27 designs planned" is now "the project's gun data lists 27" (true: 27 TOML files in `data/guns`); the "behind this panel" wording is fixed. Line `index.html:183-184` still repeats the pill in the body. |
| 11 Landmarks | Partly: skip link, nav labels, `main` focusable. Dot labels still differ from the h2 topics. |

## B. Honesty (ranked)

1. MEDIUM, `website/how-it-works.html:149`, "Analog plunger and nudge are still to come." Contradicted by `hub/PINBALL.md:60-67` (analog plunger on right stick, nudge on left stick for a gamepad). Fix: "Gamepad: flippers, analog plunger, nudge and magna-save work; VR-controller plunger and nudge are still to come" (only if true), or drop the sentence. Observed.
2. MEDIUM, `how-it-works.html:152-157`, Clean exit shown as grey "Not yet". `hub/PINBALL.md:103-107` says Exit game requests normal VPX closure with no forced-stop countdown and the job object ends leftovers. Fix: mark "Working" (or "In progress" if the owner says unproven) and say what is built. Observed.
3. MEDIUM, `website/index.html:104` says no game control uses the left Menu button, while `index.html:260` and `games.html` N64 note say N64 Start still sits on it. Fix: "No new game control uses..." or "Controls avoid ... except N64 Start (host change pending)". Observed.
4. LOW, `how-it-works.html:107` "Steam library shortcuts launch the same game and view" is marked Working. `current.md` does not mention it; `docs/LAUNCH-STEAM-M1.md` has a write API needing owner approval and an "outstanding acceptance" section. Same for `index.html:240` ("can also appear in your Steam library"). Fix: say "can be added" or mark In progress. Inferred.
5. LOW, `how-it-works.html:34` key defines Working as "covered by tests or owner checks", so green overlay steps read as accepted. Fix: neutral wording (UX 7).
6. LOW, `games.html` key "E2 ... (every game unless marked)" while the notes list games that are view-only or unsupported (`Resident Evil Survivor`, `Virtua Fighter 2`). Fix: "most games".
7. LOW, `games.html` Model 3 note "Also tried in a headset by the developer" is supported by `setups/supermodel-vr/README.md:21-25` (13 Oct) but is not an E-level and sits beside one E3. Fix: add "none accepted".
8. LOW, `index.html:222` "send fixes upstream": not in `current.md`; repo evidence is only guarded build-time patches. Fix: "carry patches on top of" the upstream builds. Inferred.

Everything else checked against `current.md` and the repo: only Mario Kart 64 3D, Sega Rally 2 DX with wheel and Time Crisis big screen carry E3; counts 11/26/14/1/5/22/22/12/5/5/19/1 = 143; 49, 40 and 10 route totals; 5 and 8 Wii games; sixteen control GLBs; 27 gun TOML; CC BY exception is `arc-light-gun` (not on the site). All observed.

## C. JS correctness (ranked)

1. MEDIUM (inferred), `website/arcade.js:941-943` with `:1013`. Centring offset is computed at scale 2.4 but the stop-2 cycler scales `root` from near 0 to 2.4 about that fixed offset, so each gun swoops from displaced to centred during grow-in and grow-out. Fix: scale `pivot` instead of `root` (`g.pivot.scale.setScalar(...)`), or set `root.position` to `mid0 * (s/2.4)` each frame.
2. MEDIUM (inferred), `arcade.js:1185` and `style.css:197,355,372`. Camera shifts apply whenever not portrait, but the side layout only applies at 900px or wider, and the phone room-above rule only at 720px or less. Landscape 721-899 and portrait 721-899 (tablet, small window) get a shifted camera under full-width panels. Fix: pass a `sideLayout = innerWidth >= 900` flag, use it for the yaw branch, and use `innerWidth <= 720` for the portrait lift (or move the CSS breakpoints to match).
3. LOW (observed), `arcade.js:1275`: `Raycaster` ignores `visible`, so the seven hidden stop-2 guns remain clickable and show a pointer cursor over empty space. Fix: skip hits whose ancestor chain is not visible.
4. LOW (observed), `arcade.js:1385-1394`: at stop 0 (no `subject`) and when `spot.z >= 1`, `--sx/--sy` keep the previous stop's values. Fix: reset both to 50%/45% in those branches.
5. LOW (observed), `models.js:90`: a failed grip-hand load rejects the whole gun, so both stop-1 guns vanish. Fix: wrap the hand load in try/catch and continue without the hand.
6. LOW (observed), `arcade.js:46-51`: after `sceneStarted`, a render-loop exception is only logged and `requestAnimationFrame` stops silently. Fix: wrap `frame()` body in try/catch that re-queues the next frame once and falls back after repeated errors.
7. LOW, `bench.js:80`: if the first `pick` fails, `init` rejects and the panel is hidden, but the render loop keeps running. Fix: `renderer.setAnimationLoop(null)` in the `start()` catch (needs the renderer in scope).

Checked and fine: one `models.js` URL used everywhere (single instance); `markClickable` is a hoisted function at `arcade.js:144` and always defined before use; `Math.round(p)` is safe because `progress()` clamps to 0..N-1; tint, suffix and `allSettled` logic; ghost pre-pass does not break LOD1 or node lookup; no unhandled rejections found.

## D. Branding and privacy

- HIGH for the stated rule, `docs/reviews/2026-10-22-agency/ux-panels.md:4` and `docs/reviews/2026-10-22-studio-experts/model-presentation.md:5` contain absolute local paths (drive letter, `.local/worktrees`). Fix: replace with repository-relative paths. Observed.
- LOW, `docs/reviews/2026-10-22-studio-experts/BRIEF.md:16` repeats the phrase "Aladdin's Castle name" as an instruction. Fix: reword to "no operator name, logos or photos".
- LOW, `website/index.html:37,267`, `how-it-works.html:25,256`, `games.html:25` link to the repo URL containing the account name and the repo name "AladdinsCastle". It is the real public repo URL, but it is the only place the old name appears. Owner decision: keep, or rename the repo before launch.
- `website/` has no photos, no logos, no absolute drive paths and no private names beyond the repo URL; GLBs and `token.svg` contain no such strings (grep, observed).

## E. `_headers` and 404

- `website/_headers` syntax is valid for Cloudflare Pages: comment lines, path rules at column 0, indented `Name: value`, one splat per rule (inferred from the documented format). `/*.css` and `/*.js` are legal; HTML stays uncached.
- MINOR: models get a 1-day cache and unversioned `token.svg` gets none; fine.
- `website/404.html` sits at the output root, so Pages serves it with a 404 status for unmatched paths because there is no SPA fallback (inferred from Pages behaviour). It uses absolute `/style.css` and `/flow.css`, which resolve at any depth. A missing `models/x.glb?v=` now fails as 404, as intended.

## Fixes that matter before publishing

1. Remove absolute paths from the two review files (D).
2. Correct the three honesty items B1-B3.
3. Fix the stop-2 swoop (C1) and the breakpoint mismatch (C2).
4. Add a scale cue or record it as deferred (A4).
