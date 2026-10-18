# V1 independent verification: candidate d664d4f vs baseline 84ee3d2 (status: proposed)

Scope: report only, nothing else edited. Paths are relative to the repository root. Line numbers refer to `site\arcade.js` unless stated. The preview at localhost:8765 serves an `arcade.js` whose md5 equals the working tree (clean at d664d4f).

## Owner decisions

All hold: title "AC VR Hub" (`index.html:6`); sign ARCADE, then AC, then AC VR HUB (`:241-310`, final state also drawn for reduced motion, `:298`); `marqueeTex` draws no text (`:312-345`) and the five cabinet screen canvases draw no text (`:394-500`; the wall scoreboard and ticket wheel do carry words and numbers, as before); stop 2 shows one floating gun cycling through five models, four scaled to 1e-4 (`:1077-1100`); ghost wheel position and tilt unchanged (`:717`); no em or en dashes in `site/*.html`. The visible text has no "Aladdin's Castle" or "Bally". The string `AladdinsCastle` appears only inside the GitHub repo URLs, as in the baseline.

## SUMMARY actions

| # | Verdict | Evidence |
|---|---|---|
| 1 Blow-out, bloom, HDR | verified (reading, captures) | Bloom 0.45/0.35/1.1 `:1168`; neon 3 to 1.6 `:619`; no emissive on gun bodies `:956`; shell gun `#a7acb8` `:1079`; HDR pass `:1181-1191`. d-stop5 and d-stop6 guns not blown. Stop 1 and the slide/twin models at stop 2 not captured. Plush `#ffffff` remains. |
| 2 Whip pan | verified | `aimStops` and `poseAt` `:1216-1273`. I recomputed every stop's yaw: transition deltas -38, -32, 157, -165, 167, 3, -67, -63, 50 degrees, all via `wrapAngle`, none near 180. |
| 3 Cards cover subject | verified (computed) | My projection of `STOPS` at 1280x800: subject at 27, 27, 69, 36, 64, 34, 66, 31, 29% of width for stops 1-9 against card sides R, R, L, R, L, R, L, R, R. Phone: subject 29-44% from top, above a card capped at 48vh. Stops 5 and 6 now mirror each other (d-stop5, d-stop6) but still show the same two kiosks. |
| 4 Ghost wheel | partially | Over-seat camera `:1199`, ghost x2.2 and 0.55 `:715` (d-stop3, p-stop3 show it clearly). Seats not shortened or tinted (`:708-710` unchanged). |
| 5 Robustness | verified (reading) | `fallBackToText` `:38-45` restores `--vis` and `data-hidden`; `body:not(.js)` hides the canvas (`style.css:43`). Context loss `:48`. Curtain removed. |
| 6 Draw calls | verified by reading, not measured | Caches `:91-120`; batching `:1141-1162`; governor `:1344-1362`. |
| 7 Gun silhouettes | **regressed** | Extruded profiles with hole and bevel exist, but see R1. |
| 8 Kiosks | partially | Consoles 1.6x with lit edges `:1117-1125`, gun offset `:1130`. d-stop5 and d-stop6 show the gun still covering the left part of the TV. |
| 9 Cabinet detail | not addressed | Only the body tint changed (`#121016` to `#1d1a27`). Screen y still 1.38 `:648-650`, coin door unchanged `:638`, no glass frames, "file" props unchanged. |
| 10 Path clearance | verified (reading) | Waypoint now (-2.4,1.6,-4.9) `:1208`, about 0.9 m from the island corner. Not simulated on the spline. |
| 11 Accessibility | mostly verified | Phone dots keep names via clipped spans (`style.css:300`); visibility:hidden removed (`:191`); modifier guard `:1318`; fov and aspect re-read `:1272-1276,1326`. Not done: resize still rebuilds targets (debounce only, `:1338`). |
| 12 Load | verified, one caveat | Min build and modulepreload `index.html:14-19`; FXAA last `:1171`. Fonts not self-hosted. See R4. |

## Top UX findings

| # | Verdict | Evidence |
|---|---|---|
| 1 No route to Game list | verified | `how-it-works.html:24`, `:236`; phone rule `pipeline.css:246` (specificity beats `:11`). The `aria-label="Site"` on the HUD nav was not added (all pages). |
| 2 Lane header drift | verified (arithmetic) | `pipeline.css:146-150` (200px + four `minmax(0,1fr)`, gap 10, padding 16) matches `.lane` padding 14 plus border 2. Not rendered. |
| 3 Connectors | verified for the layout, but see R2 and R6 | `--cols` 5/6 and a single-column break at 1000px (`:93-107`). |
| 4 8px type | partially | Chips, lane head, return, stage-num are 10px in px, not rem. Phone HUD links stay 8px (`style.css:296`), no min-height. |
| 5 Totals | partially | Counted: 7 in headset, 19 games running plus the pinball launcher (20 run chips by title), 418 is asserted in `hub/tests/CatalogTest.cpp:117`. Section tallies and the pinball note are added. No chip-keyed tiles or bar; the pinball card still carries a "Runs" chip; `aria-label` on a bare div remains (`games.html:40`). |

## Regressions

| ID | Severity | Where | Failure scenario |
|---|---|---|---|
| R1 | High | `arcade.js:949` | `geo.rotateY(-Math.PI/2)` maps profile +x to +z, but muzzle, slide, barrel, flash, ray and trigger sit at -z (comment says -z). Each gun faces backward relative to its moving parts: the trigger sits outside the guard hole, and the barrel and aim ray leave the butt. Visible in d-stop2-gun-revolver (hole behind the grip, barrel on the other side) and d-stop2-gun-dpad and d-stop6 (slide bar and sights at the rear). Fix: `rotateY(Math.PI/2)`. Affects stops 1, 2, 5 and 6. |
| R2 | Medium | `pipeline.css:174` | `content: "B6"` prints the literal text "B6" between lane steps (12 places on the How page; rotated on phone). A backslash escape (`\25B6`) was lost. No capture shows it. |
| R3 | Medium-low | `arcade.js:1346,1366` | The 16.4 ms gate quantises to whole display intervals: 144 Hz renders at about 48 fps, 75 Hz at 37.5, 90 Hz at 45. A 72 Hz headset browser averages 27.8 ms against the 28 ms downgrade threshold, and a 30 Hz power-saver display will downgrade DPR and bloom permanently. Tab-hidden and warm-up are handled (`:1350`). |
| R4 | Low-medium | `index.html:14-16` | Both modulepreloads precede the import map. If a browser treats a started module fetch as closing import-map registration, bare `three` fails and the page falls back to text. The Chromium used for the captures evidently worked; Firefox and Safari not checked. |
| R5 | Low | `style.css:191` | The stop 9 card buttons are now keyboard-focusable while the card is at opacity 0, so focus is invisible when Tab reaches them early. |
| R6 | Low-medium (estimate) | `pipeline.css:50-56,93` | In World 3 at 6 columns the card text box is about 133 px at 1280, while "▶ IN PROGRESS" at 10px is about 150 px, so the nowrap chip touches or overflows the card border (worse at 1000-1200 px). The 5-column worlds overflow only below about 1085 px. Not rendered. |
| R7 | Low | `captures\p-mall.jpg`, `p-how.jpg`, `p-games.jpg` | They still show the brand text and wrapped HUD links, so they predate `style.css:294-295`. The phone HUD on How and Games is unverified (p-stop2 and p-stop3 do show the new HUD). |
| R8 | Low | `arcade.js:1153-1157` | Merged meshes have scene-wide bounding spheres and are never frustum-culled. About 25 ceiling-panel materials (`:551,:608`) stay unmerged. |
| R9 | Low | `arcade.js:46,22` | Any window `error` (even a cross-origin "Script error." from an extension) ends the walk. `failIfMajorPerformanceCaveat` sends blocklisted-GPU users to the text page. Both are intentional trade-offs. |

Batching and shared-material checks (no defect found): the 7 `animated.push` sites map to 6 `userData.dynamic` flags plus the transparent ghost material, which is excluded by `.transparent` (`:1146`). The bucket key includes `stop`, so `userData.stop` survives, and `clickables` is rebuilt from the scene (`:1160-1161`). Nothing mutates a cached `std`, `glow` or `basicMat` material after creation: the only runtime writes (`:722,:1006,:1008`) hit per-instance materials. The HDR pass multiplies each basic material once through the `seen` set, and the sign's colour is set, not multiplied.

## Checks run

- `node --check` on `arcade.js` and `sprites.js`: both pass.
- Link and anchor script across the three pages: no broken links or anchors.
- games.html card and chip counts by script, and 418 traced to the test file.
- Yaw and projection arithmetic for all stops, desktop and portrait, by a scratch script.
- HEAD requests (no download) to the four three@0.160.0 CDN files: 200.
- Source read of all of `arcade.js`, the CSS diffs and HTML diffs; thirteen captures viewed.

## Not run

Any rendering (the brief ruled out a browser). Three.js itself (no local copy, and I did not download it), so no mesh counts, extrude triangulation or spline clearance. GPU timing, phone or headset behaviour, cross-browser import maps, and the How page at desktop width.
