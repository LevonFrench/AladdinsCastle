# Scene and panels: composition and staging review

Reviewer role: previs/staging (scenario-blender-previs-storyboard lens). Scope: the relationship between the Three.js backdrop and the HTML panels in each of nine sections, desktop and phone. Method: source plus captures; screen positions of the exhibits were computed from `STOPS` (arcade.js:1138-1147) with a small projection script, so those numbers are **inferred**, not measured. Captures are 800x500 desktop (16:10), 450x974 phone.

Lens used: one fact per beat, and after each cut "the eye must find one thing". Staging test: in each frame, can the viewer find the exhibit the text names, while the text stays readable?

## Verdict

The walk works as a transit device and the hero (d0) is correctly staged. From stop 1 on, every desktop section is a centred, near-full-width block of panels. The camera is placed to frame the exhibit near the middle of the screen, which is exactly where the panels sit, so the exhibit is hidden at the one moment (arrival at the stop) the camera has stopped to show it. The arcade has become a dark texture, not a backdrop with a subject.

## Findings, ranked

### 1. HIGH: the named exhibit is behind the panels in stops 1, 2, 3, 4 and 5 (desktop)
Evidence: d1 vs scene-stop1-before-panels; d3 vs scene-stop3-ghost-before-panels; d2-gun-models, d4, d5.
- **Stop 1** (arcade.js:970-985 guns, STOPS[1]). Before: two grip hands with a red pistol and revolver, in the middle third of the frame. d1: nothing of them is visible. They sit in y ~0.45-0.7 and the three-panel row covers y 0.47-0.83, x 0.08-0.92. Visible backdrop is a left strip about 60 px wide, a top band with the coin, and 16 px gutters. Computed screen position of the guns: x 0.15-0.40, y ~0.55 (inferred).
- **Stop 2** (STOPS[2], arcade.js:989). Subject computes to x 0.27, y 0.52, which is under the bench panel's left half. Inside that panel the bench canvas shows the same red pistol. So the scene turntable is hidden by its own bench, and the bench replaces it. d2-gun-models shows only cabinet fragments at the right edge. The "gun turntable plus the bench panel" beat is one gun, not two, and you cannot see which is which.
- **Stop 3**. Before: two racers filling the frame, one with a glowing cyan ghost wheel at the lower right. d3: the screen of the right cabinet is half-covered by the hero panel, and the ghost wheel (computed x 0.69, y 0.54) lands in the 2% gap between the hero panel and the card row. Only a faint cyan smudge is visible. The ghost effect, the point of the section, is invisible.
- **Stop 4**. Computed: Model 3 cabinet x 0.36 and heavy mounted gun x 0.26, y 0.60. d4 shows panel rows over y 0.38-1.0. What you see is the top 10% of the picture and a fragment of cabinet at the left edge. No cabinet, no gun.
- **Stop 5**. The two kiosk guns sit at x ~0.08 and ~0.92 (gunDzWide ±1.05, arcade.js:1076-1077), i.e. in the margin band or under the section dots; the kiosks themselves are behind the card grid (d5).
- Note on comparison: the "before" captures are 4:3 (800x600), the "after" 16:10. The panel-over-subject conclusion holds either way, but the before frames show more floor.

Remedy: per-section panel alignment (finding 2) plus per-stop yaw offsets (finding 3), so the exhibit lands in a window that is clear of panels at the stop pose.

### 2. HIGH: no panel-to-subject staging; `.wrap` is a centred 83% column (style.css:183-190)
`.wrap` is `min(1120px, 100vw - 2*clamp(16,4vw,56) - 120px)` with `margin: 0 auto`: about 1058 px of 1280 (83%), 106 px free each side. Every grid spans it. The hero (`.panel-hero`, max-width 620px, style.css:208) proves the alternative: d0 leaves the sign and corridor clear on the right. Apply that idea to every section.

Concrete CSS (desktop, `min-width: 900px`): add `data-side` to each `.stop`, and
```css
.stop[data-side="right"] .wrap { margin-left: auto; width: min(640px, 56vw); }
.stop[data-side="left"]  .wrap { margin-right: auto; margin-left: clamp(16px, 4vw, 56px); width: min(640px, 56vw); }
.stop[data-side] .grid { --cols: 1; }              /* or 2 where cards are short */
.stop[data-side] .grid.stats, .stop[data-side] .grid-2 { --cols: 2; }
```
Computed subject x per stop and the side it implies: 1 (.15-.40) right; 2 (.27) right; 3 (.69) left; 4 (.26-.36) right; 5 (guns .08/.92, kiosks centre) centred 52% wrap, pull guns in (finding 4); 6 (.66, prize counter, nothing named) left; 7 (.31) right; 8 (.29) right. That reads left/right rhythm instead of one repeated block. Cost: taller sections (a 6-card stop becomes two columns of three), which also lengthens each camera glide, an acceptable trade. On a 1920-wide screen `.wrap` already exposes 400 px each side, so the effect is mostly a 1280-and-below problem; do not rely on that.

### 3. MEDIUM-HIGH: STOPS framing is centre-weighted; add a per-stop screen offset
`aimStops()` (arcade.js:1158-1167) aims at `look` on desktop and under the subject on portrait only. Add an optional `frame: { x }` per stop (desktop only), applied as a yaw delta in `aimStops`: `st.yaw += st.frame.x`. Camera yaw positive moves the subject left on screen. For a subject at x 0.38 and fov 50 (aspect 1.6), pushing it to x 0.22 needs about +0.22 rad; use `th = tan(fov/2) * aspect`, `angle = atan((0.5 - x) * 2 * th)` and apply the difference. Suggested targets: stop 1 x 0.22; stop 2 x 0.22; stop 3 mirror (+ push wheel to x 0.72 with negative yaw); stop 4 x 0.20; stop 7, 8 x 0.22. Keep `look` as is for the portrait branch and for the in-between glide: `poseAt` already lerps yaw/pitch (arcade.js:1211-1218), so no other change.

### 4. MEDIUM: kiosk guns too far apart at stop 5; heavy gun and kiosk guns are scaled for readability but placed at edges
arcade.js:1076-1077: `gunDzWide: ±1.05`. Reduce to ±0.6 and centre the wrap to 52% (`width: min(560px, 52vw)`), so both guns land near x 0.25 / 0.75 flanking the card column, not under the dot nav. Hypothesis: verify visually, since the kiosk gun scale (2.4-3x) already makes them the largest objects.

### 5. MEDIUM: scrim crushes the exhibit even in the gaps (style.css:161-168, arcade.js:1353)
`--dim = 0.18 + 0.42 * smooth(0.15, 0.7, p)` where p is the stop index, so from stop 1 on it is 0.6 permanently. The gradient's alpha is 0.55*dim at the centre (0.33) and 1.1*dim at the ellipse edge (0.66), on top of ACES-toned dark neon, then panels at `--card-bg` 0.78 (style.css:11) with `blur(10px)`. In d4, d5, d6 the unpanelled area is near-black. The darkest part of the ellipse is the edge, which is where the side-aligned exhibits will sit.
Remedy: (a) drop the gallery ceiling to 0.30 (`0.14 + 0.16 * smooth(...)`); (b) replace the ellipse with a spotlight cut-out at the exhibit: in the render loop, project `STOPS[active].subject` (lerped with `p`) through `camera.project()` and set `--sx/--sy` on the scrim, then `radial-gradient(circle at var(--sx) var(--sy), transparent 0 14vmin, rgba(10,6,18,var(--a)) 42vmin)`. Costs one Vector3 project per frame. (c) For legibility, keep panel text on `--card-bg` but make exhibit-adjacent panels `rgba(16,10,28,.66)` with `blur(6px)`; hypothesis, check contrast.

### 6. MEDIUM: phone shows no exhibit at all (p1, p4; style.css:356-362)
`.stop` is `padding: 14vh 0 18vh` with a one-column stack of 5-8 panels, so panels cover y 0.14 to 1.0 continuously; only the 14vh top strip and 16 px gutters show the scene. `aimStops` lifts the subject by a fixed 0.55 m, which computes to y 0.29-0.42 on phone, i.e. under panel 1 (p1: panel 1 occupies y 0.14-0.38; the gun is hidden). Only stop 0 (p0: sign at y 0.43, hero panel from 0.46) is staged.
Remedy: (a) Give stops 1-5 and 7 the hero treatment: `.stop { padding-top: 42vh }` on `max-width:720px`, matching `.stop-hero` (style.css:361). (b) Replace the fixed lift with an angle: `target = subject + (0, -0.55*dist*tan(fovV/2), 0)` so the subject lands at y ≈ 0.22 independent of distance (arcade.js:1161-1162). (c) Panels then slide up over the exhibit as you read, which is the right "scroll over the move" behaviour. (d) Reduce panel count above the fold: make `.grid` in phone mode two header panels visible, the rest follow; no change needed in content.

### 7. LOW: camera moves read, but only in the lower band between sections
Panels occupy about 28vh-98vh of each 130vh section, so during the glide (`te = smooth(0.12, 0.88, t)`, arcade.js:1204) the lower 40-60% of the screen shows the backdrop in motion as section A exits and B enters. This is good previs grammar: a move that starts after the text leaves, and nothing to fight it. Two cautions: (1) arrival (t=0.88-1) coincides with the next panels covering the frame, so the exhibit appears and is buried at the same moment, fixed by findings 2-3; (2) glide time scales with section height (the t fraction is relative to `tops[i+1]-tops[i]`), so tall two-column sections would slow the glide, but they also lengthen the dwell (12% each end). Do not retime; the dwell is fine.

### 8. LOW: shot grammar is otherwise neutral and consistent
Eye height 1.6 m on all stops except stop 3 (1.8 m, over-the-seat, a stated reason), fov 46-58, short walks of 2-4 m. Stop 2 is the tightest (dist 2.0 m, fov 46), which is correct for a single hero object. No change requested to lens; if the screen offset in finding 3 is used, a narrower 46-50 fov at stops 1, 4 and 7 would make the exhibit larger in the window.

## Section-by-section summary

| Stop | Exhibit visible now (desktop) | Staging fix |
|---|---|---|
| 0 Welcome | Yes: sign, corridor (d0) | none; this is the model |
| 1 How you play | No (hands and guns hidden) | panels right, yaw offset, spotlight |
| 2 Gun models | No; bench shows one gun, scene gun behind it | panels right; bench narrower; scene turntable in the left window |
| 3 Ghost controls | Faint cyan smudge | panels left; wheel at x 0.72 |
| 4 Arcade boards | No | two-col panels right; gun left |
| 5 Consoles | Edge fragments | centred 52% wrap; kiosk guns inward |
| 6 Open source | Counter and shelf visible at top (d6) | panels left, optional |
| 7 The Hub | Cabinet marquees visible above (d7); body hidden | panels right |
| 8 Where it stands | Scoreboard partly visible at top right (d8) | panels right |

## Limits
Not verified: real device or GPU behaviour, `backdrop-filter` cost with larger blur areas, actual pixel positions at 1280x800 (computed positions assume the stop pose and ignore pointer sway and `bob`).
