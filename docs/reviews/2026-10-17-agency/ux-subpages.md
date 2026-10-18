# U1 UX review: How It Works and Game List (status: proposed)

Lens: UX Architect, UI Designer secondary. Report-only; no site files edited. File paths are under `J:\projects\games\aladdinscastle\.local\worktrees\website\site\`. Reasoned from CSS plus the two captures (both low-resolution, so type legibility is judged from CSS, not pixels).

## Ranked findings

### 1. HIGH - Phone: the How page has no route to the Game list
`style.css:295` and `pipeline.css:228` hide every HUD link after the first at 720px and below. On `how-it-works.html` the first link is "The arcade", so "Game list" disappears. The closing CTA (`how-it-works.html:236-237`) offers only "Back to the arcade" and GitHub. A phone visitor cannot reach `games.html` from How at all. (`games.html:119-120` does link back to How, and `index.html:164-165` keeps both CTAs.)
Remedy:
- Add `<a class="btn btn-ghost" href="games.html">Game list</a>` after `how-it-works.html:236`.
- Replace `pipeline.css:228` with `.how-page .hud-links a:nth-child(n+3) { display: none; }` so two links show on phone. Width check: brand about 120px plus two 8px-font links about 190px fits 375 only barely, so also set `.how-page .hud { flex-wrap: wrap; }`.
- Add `aria-label="Site"` to the HUD `<nav>` (`how-it-works.html:22`, `games.html:23`); a second nav is already labelled "Worlds".

### 2. HIGH - World 4 column header does not align with the lanes
`pipeline.css:135` is `200px repeat(4,1fr)` with no padding. The lane at `:144` is `200px 1fr` inside a 2px border plus 14px padding, and the track at `:157` is `repeat(4,1fr)`. Computed drift of each header label from its box: Game -16px, Stereo hook -8px, OpenXR 0, Headset +8px. At 1280 each label sits visibly off its box. `1fr` also lets long words resize columns unevenly.
Remedy:
```css
.lane-head { grid-template-columns: 200px repeat(4, minmax(0,1fr)); column-gap: 10px; padding: 0 16px 8px; }
.track     { grid-template-columns: repeat(4, minmax(0,1fr)); }
```
Also delete the redundant `:140`.

### 3. HIGH - Stage-card connectors break whenever the grid wraps
`pipeline.css:93` is `auto-fit minmax(170px,1fr)`. World 3 has six cards. At 1280 the track fits only five, so card 6 drops to a lone second row and card 5's dotted trail dangles into the box padding. Between 721 and 1000px the trail ends every row pointing at nothing. Between about 500 and 720px the grid is still 2 to 3 columns but the connector is vertical (`:226`), so it appears to link card 3 to card 6 instead of 3 to 4. The path reads as a path only at exactly five columns.
Remedy: make the layout deterministic so the connector direction is always true.
```css
.stages { grid-template-columns: 1fr; gap: 22px; }   /* base: vertical path, phone and tablet */
@media (min-width: 1000px) {
  .stages { grid-template-columns: repeat(5, minmax(0,1fr)); }
  .stages:has(> .stage:nth-child(6)) { grid-template-columns: repeat(6, minmax(0,1fr)); gap: 14px; }
}
```
Apply the horizontal connector (`:105-112`) only at 1000px and up and the vertical one (`:226`) below that, with no separate 720 split. Add an arrowhead so the trail has direction (a 6px `border-left` triangle, or `content: "▶"`). At 375 the vertical stack costs about 200px per card, which is acceptable and mirrors the lane rail.

### 4. MED-HIGH - 8px Press Start 2P is too small for information
Affected: chips (`pipeline.css:53`), `.lane-head` (`:138`), `.return span` (`:198`), `.stage-num` (9px, `:115`), `.hud-links` on phone (`style.css:294`). The font is built on an 8px grid, so 8px is only crisp at integer device-pixel ratios; browser zoom or fractional DPR (125% or 150% Windows, 2.625 Android) smears it. Chips carry the project's core claim (status) and are the smallest text on the page.
Remedy: floor all information text at 10px, in rem so user settings apply.
```css
.chip { font-size: .625rem; padding: 6px 8px 5px; }
.lane-head, .return span { font-size: .625rem; }
```
"IN PROGRESS" at 10px is about 130px, which fits the 142px card text width. Keep 8px only for decoration. Also give phone HUD links `min-height: 36px` (currently about 24px, the WCAG 2.2 target-size floor).

### 5. MED - Status totals (7 / 19 / 418) are ambiguous, and the 19 is off by one
`games.html:40-44`. The three tiles look like peers, so readers can add 7+19 or read 19 as including the 7. The tile wording ("played in a headset", "running, headset check next") also differs from the chip words ("In headset", "Runs"). Counting the page: 7 In headset, 20 Runs, 1 Set up (28 cards). The 19 excludes "Your VR-ready pinball tables" (`games.html:111`), but that card still carries a Runs chip, so a visitor counting chips gets 20.
Remedy:
- Key the tiles to the chips: `★ 7 In headset`, `▶ 19 Runs, not yet in a headset`, `■ 418 in the catalog`, each tile border in the chip colour.
- Add one proportion bar under the tiles: 7 green, 19 yellow, the rest grey. It shows honestly that about 6% of the catalog is accounted for.
- Resolve the pinball discrepancy: give `games.html:111` a neutral "Launcher" chip, or say "19 games plus the pinball launcher".
- `aria-label` on a bare `<div>` (`games.html:40`) is ignored; use `<ul role="list">` or `role="group"`.

### 6. MED - World 4 diagram: readable structure, weak direction cue
It reads at a glance mainly because every lane has the same four steps. The directional cue is weak:
- The rail (`pipeline.css:160-167`) is hidden behind opaque boxes (`li` z-index 1), so only a 10px dash shows in each gap.
- The riding square (`:168-181`) also hides behind the boxes (it has no z-index), so it flickers in the gaps. Under reduced motion it parks at the far right, behind the last box, so no cue remains.
- The pink "Input" return strip is a good idea, but its "◀" is easily lost beside the "→" characters inside the text.
Remedy: add static arrowheads and keep the rail as texture.
```css
.track li:not(:last-child)::after { content:""; position:absolute; top:50%; right:-10px; margin-top:-5px;
  border:5px solid transparent; border-left:8px solid var(--world); }
```
Optionally dim the Not yet lane with `.lane:has(.chip.locked){border-style:dashed}` so the unbuilt shared runtime does not carry the same weight as the three working routes.

### 7. MED - Column meaning is lost on phone and for screen readers
`.lane-head` is `aria-hidden` and `display:none` at 860px and below (`:214`). "Stereo hook" is jargon explained nowhere else. Phone and screen-reader users see four unlabelled boxes.
Remedy: add `<span class="step">Stereo hook</span>` as the first child of each `li` (Game / Stereo hook / OpenXR / Headset). Make it visually hidden on desktop and a 10px pixel label at the narrow breakpoint. Raise that breakpoint from 860 to 1000px: at 900 each text column is about 110px wide and wraps badly.

### 8. MED-LOW - Game list scanning: no filter needed, add wayfinding
28 cards in 4 system sections, already sorted headset-first inside Model 3. A JS filter is overkill. Instead:
- Add a jump nav (copy `.world-select`, `how-it-works.html:39-44`) and `id`s on the four sections.
- Add a per-section tally in each head ("6 In headset, 4 Runs").
- PS2 is 11 identical "Runs / 3D screen" cards. Render it as a compact two-column list to cut about 40% of the scroll.
- Heading outline: the `h2` is the tool name ("supermodel-vr"); the system name is only in a span (`games.html:49-50`). Put "Sega Model 3 · supermodel-vr" in the `h2`.

### 9. LOW-MED - Colour semantics collide
World frame colours reuse the chip palette (Model 3 and World 4 are status-green; World 1 is wip-yellow). Chip contrast itself passes on `#1a1230` (computed: green about 10.8:1, yellow about 11:1, grey `#8b86a0` about 5.1:1), and each chip also has a glyph, so this is semantic, not a contrast failure. On `games.html` use a neutral brass frame (`--world: var(--brass)`) so green and yellow mean status only. The same ★ means "Clear" on one page and "In headset" on the other; the legends explain it, so that is acceptable.

### 10. LOW - Accessibility polish
- `.chip::before` glyphs (★ ▶ ■, `pipeline.css:58-62`) are not in Press Start 2P and fall back to a system font. Use `content: "★" / ""` so screen readers skip them, or draw them as CSS shapes.
- No skip link; optional given the short HUD.
- Focus is good (global cyan 2px outline, `style.css:268`). `prefers-reduced-motion` is handled for blink, bob, ride and smooth scroll. Keep the lane marker visible once finding 6 is applied.
- Heading order is correct on both pages (h1, h2, h3). Landmarks are fine apart from the unlabelled HUD nav.

## Direct answers
- World 4 lane diagram at a glance: yes structurally; the direction cue needs arrowheads (6).
- Header alignment: no, it drifts -16 to +8px (2).
- 8px too small: yes for chips and headers; use 10px (4).
- Dotted connectors: read as a path only at exactly five columns; fix per (3).
- Game list filter or grouping: no filter; add jump nav, tallies, a headset-first summary (8).
- Totals 7 / 19 / 418: not clear; the 19 conflicts with 20 Runs chips (5).
