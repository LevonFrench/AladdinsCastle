# Studio review summary: website refresh (2026-10-22)

Target: `website/` on `claude/website-refresh`, commit c54ca59, preview https://preview.ac-vr-hub.pages.dev. Mode: review only.

## Panel

| Deliverable | Lens | Report |
|---|---|---|
| How the project's GLB models look on the web | scenario-blender-texturing-shading + lighting-rendering | [model-presentation.md](model-presentation.md) |
| Scene vs panels, per section, desktop and phone | scenario-blender-previs-storyboard | [scene-and-panels.md](scene-and-panels.md) |
| Download, draws, robustness, accessibility, serving | scenario-unity-web + scenario-unity-performance | [web-performance.md](web-performance.md) |

The panel UX itself (reading order, density) has no owner here and goes to The Agency.

## Coordinator verification

| Claim | Check | Result |
|---|---|---|
| Panels hide the exhibits at stops 1-5 | captures d1-d5 vs scene-stop1/3-before-panels | Confirmed (observed) |
| Control materials are suffixed, so they fall back to grey | GLB JSON: `bars-motorcycle`, `pedals-dual`, `shifter-sequential`, `stick-arcade` use `body.00N` etc. | Confirmed (4 of 6 controls) |
| The web glove colour is invented | `assets/hands/grip-hand-right.toml`: `glove = { body = "#8293a8", accent = "#a9b8c9" }` | Confirmed |
| GLBs are served uncompressed with `max-age=0` | `curl -I` on the preview | Confirmed |
| A missing file returns 200 text/html | `curl -I` on a missing GLB | Confirmed |
| One failed GLB fetch can turn the whole 3D page off | `bench.js:45` click has no catch; `arcade.js` falls back on any unhandled rejection | Confirmed (source) |
| ~520 extra draws from unbatched models; inactive guns still drawn at scale 1e-4 | node counts, `arcade.js` cycle code | Estimate; the scale trick is confirmed in source |

## Prioritised actions

1. **Let the exhibits show (composition).** Per-section panel side (`data-side`): panels about 56vw on the side away from the exhibit, plus a desktop yaw offset in `aimStops` so each subject lands in the open area (about x≈0.22 from that edge). Lighter scrim from stop 1 on (cap ≈0.30) with a soft spotlight at the exhibit. Phone: room above the first panel on stops 1-5 and 7, and an angle-based subject lift. Pull the stop-5 wide kiosk guns in (±0.6).
2. **Never let a model failure take the page down.** Catch every model load (bench click, `later`), drop rejected promises from the cache, use `Promise.allSettled` so one bad file skips one model, and limit the page-wide fallback to scene start-up errors.
3. **Right colours.** Strip `.NNN` suffixes when matching material names; when a model has no tint (controls), keep the GLB's own colours; sync the glove tint from the hand TOML and remove the invented fallback.
4. **Bench matches the headset look.** Use the project's runtime lighting (`assets/guns/runtime-lighting.json`: ambient 0.35 + directional 0.65 in camera space, no specular, no tone mapping), via Lambert materials, no tone mapping, and a camera-parented light. Add a scale cue.
5. **Less download and fewer draws.** Load models by scroll proximity instead of all at idle; hide inactive cycling guns with `visible = false`; turn models around their centre, not the grip; ghost controls get a depth pre-pass so stacked additive layers stop going white-hot.
6. **Serving.** `_headers` for caching (`/models/*` and static assets), version tags on `models.json` and `models.js`, and a real `404.html` so missing files fail as 404.
7. **Accessibility.** Bench chips `aria-pressed`, a live region for the selected model, labelled section nav, a skip link; gun meshes become click targets once loaded.

## Disagreements and open points

- Compression: meshopt/quantised web variants would cut downloads by ~70%, but need `gltfpack`, which is not installed here (no installs). Proximity loading and caching address most of the cost without it; revisit with the owner.
- Bench lighting fidelity (action 4) trades the scene's PBR look for the headset's flat look on the bench only; the scene keeps PBR so it matches the arcade.
- Owner decisions held: panels in front with the arcade as backdrop, one gun at stop 2, blank marquees, ARCADE→AC sign, honest statuses.
