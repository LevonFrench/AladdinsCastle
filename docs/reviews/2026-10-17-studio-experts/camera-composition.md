# Camera, staging and composition review

Reviewer: camera/staging lens (previs-storyboard SKILL used as a lens on a scroll-driven Three.js walk). Report only; no site file was edited.

## Scope and evidence
- Read: `arcade.js` lines 1075-1260 (`STOPS`, `PATH`, `poseAt`, loop), the object placements (lines 597-870, 1012-1063), `index.html` card sides, `style.css` lines 170-190 and 290-313, and all 13 captures.
- **Observed** = in source or a capture. **Computed** = my re-implementation of `poseAt` (Three's centripetal Catmull-Rom, same easing) and a pinhole projection at 800x500, checked against the captures (gun at stop 2 projects to x=292, capture shows ~285; stop 5 gun 396, capture ~410). Scratch scripts are in the session scratchpad, not the repo. **Hypothesis** = needs a device or browser run.
- Limits: no GPU or device test; phone captures exist only for stop 0.
- Card rectangles at 1280x800 (shown at 800x500): left card x 40-315, right card x 420-695, top y 80, bottom y 237-372 depending on text.

## Findings, ranked

### 1. HIGH: 180-degree whip pans at 3->4 and 4->5 (and a fast swing at 2->3). Computed.
- `poseAt` line 1140 lerps the look *point* between the two stops' look targets. When the stops face opposite walls (racing looks +x, Model 3 looks -x, Wii looks +x), the lerped point passes through the camera's own position.
- Simulated: segment 3->4 min horizontal camera-to-look distance 0.17 m at t=0.68, yaw jumps from 123 to -171 to -111 degrees across about 0.04 of a segment, peak 65 degrees per 0.02 of scroll progress. Segment 4->5: 0.22 m, 75 degrees. Segment 2->3: 17.8 degrees, 0.85 m.
- Before the whip the camera is walking west while facing east (backwards, about 110 degrees off its travel direction).
- Effect: scrolling gives a violent snap in the middle of the walk, a likely nausea trigger.
- Remedy: interpolate yaw and pitch as angles, not look points. Precompute `yaw_i = atan2(look.x-pos.x, -(look.z-pos.z))` and `pitch_i` per stop; per segment `yaw = yawA + wrap(yawB-yawA)*smooth(0.2,0.85,t)`, same for pitch; rebuild `lookDir`, then `camPos + lookDir`. All new-pose swings are under 180 degrees (largest 163 at 4->5, through "north", so it reveals the deep room). Simulated peak with this method: 7 degrees per 0.02 progress versus 75 now. An optional smoother "walking" variant keys yaw to the path tangent at t=0.5 (peak 13 degrees).
- Bonus: the 0.95 end of the look easing (line 1140) means the camera is still turning about 15% when the next card is fully opaque (`vis` is 1 inside 0.22). Ending at 0.85 settles the shot under the text.

### 2. HIGH: subjects are centre-framed at 45-51% width while the card takes the other half. Observed in captures, confirmed by projection.
| Stop (card) | Subject now (x px of 800) | Problem | Capture |
|---|---|---|---|
| 1 (right) | controllers about 285-310, gun silhouette 330-410 | pressed to the card edge at 420; subject only about 7% wide | stop1-motion |
| 2 (right) | gun about 292 | acceptable; slightly right of the free-zone centre (210) | stop2-gun |
| 3 (left) | wheel 389 | left cab marquee under the card; see finding 3 | stop3-racing |
| 4 (right) | m3a 250, m3b 388 | right half of m3b and the whole racer under the card | stop4-model3 |
| 5 (left) | gun 396, spans 325-495 | 10 px from the card edge | stop5-wii |
| 6 (right) | console 325-480 | right part of the TV under the card, gun centred | stop6-ps2 |
| 8 (right) | Hub cab 395 | right 40 px behind the card (geometry unchanged since the older capture) | stop8-hub-older |
| 9 (right) | board 105-414 | 6 px from the card (older capture) | stop9-scoreboard-older |
- Stops 5 and 6 are also the same shot twice (same x=3.7, same distance, same backdrop); only the subject changes. Nothing separates the cuts.
- Remedy: stage each subject in the centre of the free zone: about 27% (x 216) for right cards, about 69% (x 550-565) for left cards. Do it by aiming, not by moving props. The recommended structure is `{pos, subject, at:[nx,ny], fov}` with a solver, so the numbers survive a card-width change. New values below.
- Parallax margin: pointer yaw is +/-0.12 rad (lines 1215-1219), about 58 px at 800 wide. The proposed targets leave over 100 px to the card edge, so parallax cannot push a subject under the card.

### 3. MEDIUM-HIGH: stop 3 hides its subject behind seat backs. Computed from source geometry.
- Camera (2.9, 1.8, -11.0) looks along +x. The seat back (line 663) tops out at about y=1.125, x=5.28. The ghost wheel (lines 669-672) sits at x=5.98, y=0.9 and is only +/-0.1 m tall because it lies back. The sight line from 1.8 m high passes 0.0-0.05 m above the seat top, so the wheel is mostly behind the headrest. In the capture the wheel reads as a faint ring over the real wheel, and a large black seat mass fills the lower frame.
- The additive material (opacity 0.28 +/-0.12) is also faint; hand to the look/lighting reviewer.
- Remedy: raise the pitch and move in. Over-the-seat shot, pos (4.6, 1.8, -11.45), look (10.04, -0.71, -11.72), fov 52. Computed: the sight line clears the seat top by about 0.11 m, the wheel lands at (565, 330), the cabinet screen at (538, 203) above it and the marquee at the top edge. The composition reads as "screen above, hands below", which is what a player sees. The sideways offset (z=-11.45) pushes the red ghost cab off-screen; the blue cab sits behind the card. Plan clearance to the yellow seat base is 0.60 m. Optional alternative (hypothesis): a seated point of view at (5.6, 1.25, -11.0), which would need a real shot test because the wheel would be 0.4 m from the lens.

### 4. MEDIUM: the 1->2 walk grazes the island cabinet. Computed.
- Route (0.9,-0.2) -> (-0.6,-4.4) -> (-2.9,-6.4) passes (-2.09, 1.6, -5.59) at q=1.66, 0.01 m from the first island cabinet's front panel footprint and 0.27 m from its body (lines 803-804, x=-1.4, z=-6.0, facing -x). The cabinet side will fill the frame for a moment.
- Remedy: replace the waypoint `v(-0.6,1.6,-4.4)` (line 1090) with `v(-2.4,1.6,-4.9)`. Computed minimum clearance becomes 0.54 m (token changer, 0.54 m) and 0.55 m (island corner). Optional scene fix: slide the island 0.6 m east, which widens the pass to over 1 m. The rest of the route clears everything by 0.6 m or more (race seats 0.60 m at q=2.86 with the new stop 3).

### 5. MEDIUM: one wide lens for every shot. Computed.
- fov 58 vertical is about 83 degrees horizontal at 16:10 (about 20 mm full-frame equivalent), 89 at 16:9, 105 at 21:9. For props shot at 2-3 m (guns, wheel, consoles) this stretches edges (the foreground seat in stop3-racing and stop4-model3 is visibly fat) and shrinks the hero (stop 1 controllers about 7% wide).
- The skill's guidance: long lens isolates; wide-and-close distorts. Keep 58 for the mall and transit.
- Remedy: add `fov` to each stop and ease it with the look blend (`camera.fov = lerp(a,b,s); camera.updateProjectionMatrix()`): stops 2, 5, 6 at 46; stop 1 at 50; stops 3, 8 at 52; others 58. The scene is never reframed at larger distance because the shots below already assume the new fov. Do not lower fov on portrait (hfov would drop to 22-25 degrees); keep 70 there.

### 6. MEDIUM (hypothesis): phones. Source observed, behaviour not captured except stop 0.
- Portrait vertical fov 70 gives only about 36 degrees horizontally. The card is bottom-anchored and up to 48vh (`style.css` lines 299-309), so everything below about 52% of the height is covered, and every stop aims the subject at the screen centre (50%), exactly at the card top.
- Any landscape x-offset from finding 2 would push subjects out of a 36-degree frame, so framing must be aspect-aware.
- `STOPS[0].pos.set(0,1.62,16.5)` (line 1096) and the fov (line 46) are decided once at load. Rotating a phone or resizing across 720 px keeps the wrong pose and fov.
- Remedy: in portrait, solve with target `at:[0.5, 0.26]` (subject in the top quarter, so a pitch-down of about 15 degrees from today) and the same `subject` points; recompute stop 0's position and fov in the resize handler; `camera.fov = innerWidth<=720 ? 70 : 58`. Needs a device capture of stops 1-9 to verify.

### 7. LOW: head bob frequency follows scroll fraction, not distance. Observed in source (lines 1209-1212).
- `walked += |dp|*9` and `sin(walked*2.2)` give 3.15 bobs per segment whatever its length: about 3.2 m per bob on the 10 m mall walk, about 0.8 m per bob on the 5->6 hop. Amplitude (2.5 cm) is fine.
- Remedy: accumulate arc length (`walked += |curve.getPoint step|`) and use `sin(arc*8.4)` (about 0.75 m per step). Optional roll of 0.002 rad. Gate on `p-pShown` as now.

### 8. LOW: drag-look decays slowly. Observed (line 1217).
- `exp(-dt*0.6)` has a half-life of 1.15 s, so a dragged view (up to +/-69 degrees) lingers for seconds after release while the next card is already visible. Use 1.5 for a half-life of 0.5 s. Parallax is a pure rotation of +/-6.9 degrees, so it is a head-turn, not parallax; fine as is.

### 9. Handoff, not mine: stop 2 figure-ground.
- In stop2-gun.jpg the gun is a blown-out white shape in front of lit pink skyline screens (cabinets 2.2 m behind), so the silhouette merges. Lowering the gun from y=1.45 to y=1.2 (ring y 0.88) puts it in front of the dark control-panel band; with that, `look.y` becomes 0.55 in the stop 2 line below. Bloom and material settings go to the look reviewer.

## Proposed values (landscape; fov per stop; screen targets at 800x500)
| Stop | pos | look | fov | Subject lands at (x, y) |
|---|---|---|---|---|
| 0 mall | (0, 1.62, 9.8) | (0, 2.55, 0) unchanged | 58 | sign y 185, door open at 400 (no action) |
| 1 | (0.9, 1.6, -0.2) | (-2.76, 1.49, -4.95) | 50 | controllers (200-230, 250-285), token (291, 144) |
| 2 | (-2.9, 1.6, -6.4) | (-8.53, 1.27, -8.46) | 46 | gun (216, 262) |
| 3 | (4.6, 1.8, -11.45) | (10.04, -0.71, -11.72) | 52 | wheel (565, 330), screen (538, 203) |
| 4 | (-3.3, 1.6, -12.0) | (-9.16, 1.34, -13.25) | 58 | m3a (135), m3b (292) |
| 5 | (3.2, 1.6, -15.5) | (9.19, 1.20, -15.55) | 46 | gun (565, 255) |
| 6 | (3.2, 1.6, -16.3) | (9.18, 1.19, -16.08) | 46 | gun (216, 255) |
| 7 | (1.2, 1.6, -18.3) | (3.70, 1.32, -23.75) | 58 | counter centre (550, 272), width 373-757 |
| 8 | (-1.8, 1.6, -16.9) | (-5.51, 1.18, -21.59) | 52 | Hub cab (250, 250), 2.9 m away instead of 3.7 |
| 9 | (0.6, 1.65, -19.4) | (1.82, 1.92, -25.27) | 58 | board 58-383, 37 px clear of the card |
- `PATH` becomes: `[S0, (0,1.61,5.2), (0,1.6,2.0), S1, (-2.4,1.6,-4.9), S2, (-2.9,1.6,-9.0), (0.8,1.6,-10.0), S3, (0.4,1.6,-11.6), S4, (0.2,1.6,-13.4), S5, S6, S7, (0,1.6,-17.4), S8, (-0.2,1.6,-18.4), S9]`. Stops 5 and 6 now truck 0.8 m sideways along a parallel look: the Wii gun exits right, the PS2 gun enters left, and the card swaps sides with them.
- Stops 0, 4 and 9 barely move; 4 and 9 only re-aim. The stop 0 hero shot is fine: sign dominant, interior glimpsed, card clear of it.

## Recommended sequence
1. Angle-based look interpolation (finding 1) and the added island waypoint (4): small code change, removes the worst motion issues.
2. Replace the pose table with the values above and add per-stop `fov` (2, 3, 5).
3. Subject-based solver and the portrait branch (6).
4. Bob and drag-decay tuning (7, 8).
5. Hand finding 3's wheel opacity and finding 9's bloom to the look reviewer.

## Acceptance checks
- Re-run the simulation: segments 3->4 and 4->5 stay under 10 degrees yaw per 0.02 of progress; minimum plan clearance at least 0.5 m.
- Re-capture stops 1-9 at 1280x800: each subject's bounding box at least 60 px clear of its card; stop 3 shows the wheel above the headrest.
- Capture stops 1-9 at 375x812: subject above the card's top edge.
- Not verified: actual motion feel, device performance, the seated point of view.
