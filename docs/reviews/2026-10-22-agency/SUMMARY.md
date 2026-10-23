# Agency fix-and-verify summary: website refresh (2026-10-22)

Target: `website/` on `claude/website-refresh`, from commit c54ca59. Mode: fix and verify. Inputs: the studio review in [../2026-10-22-studio-experts/SUMMARY.md](../2026-10-22-studio-experts/SUMMARY.md) and the panel UX review in [ux-panels.md](ux-panels.md). Independent check: [verification.md](verification.md) (read-only, source level).

## Roles

| Deliverable | Owner | Evidence |
|---|---|---|
| Studio actions 1-7 | coordinator (writer) | source diff; preview at 1280x800, 2000x966, 820x700, 375x812 |
| Panel reading order and density | UX reviewer, then coordinator | [ux-panels.md](ux-panels.md); copy changes below |
| Independent check | verifier (separate agent, read-only) | [verification.md](verification.md) |

## What changed

- **Composition:** panels sit on the side away from each exhibit (`data-side`), the camera yaws the exhibit into the open side from 900px up, portrait windows get room above the first panel, the scrim caps at 0.30 with a spotlight on the exhibit.
- **Robust loading:** every model load is caught, failures are not cached, `allSettled` skips one bad file, a missing hand leaves the gun without a hand, and only start-up errors switch the page to text.
- **Colours:** material suffixes stripped, authored colours kept for controls, glove tint from the hand TOML.
- **Bench:** the headset's runtime lighting (ambient 0.35, camera directional 0.65, Lambert, no tone mapping).
- **Load and draws:** models load as you approach their stop; cycling guns toggle `visible` and grow about their centre; ghost controls get a depth pre-pass.
- **Serving:** `_headers` (models a day, CSS/JS an hour), `404.html`, one version tag (`VER` in `models.js`) for the catalog and GLBs.
- **Accessibility:** skip link, labelled navs, `aria-pressed` chips, live region on the bench, loaded guns are click targets, hidden guns are not.
- **Copy:** a "Four ways to see a game" glossary; a lede that says Windows, SteamVR and no games included; "3 accepted in the headset" replaces the pinball stat; E-level legends at the console stop and the scoreboard; "big screen" replaces "theatre"; Model 2 aim claim names The House of the Dead; undefined "diorama" and "surround view" dropped; How It Works separates flow-step status from E-levels; pinball controls and clean exit corrected to match `hub/PINBALL.md`; the Menu-button rule notes N64 Start.

## Verification

- Observed in the preview: bench renders with flat runtime lighting; exhibits load by proximity (mesh count grows near stops 1-5); ghost wheel reads cyan, not white; no horizontal scroll at 375, 820, 1280 or 2000 wide; no console errors; Game List 128 titles; 404 page renders.
- Verifier findings fixed: stop-2 swoop, 721-899px camera shift, hidden-gun clicks, scrim cap, hand failure, pinball and exit claims, Menu rule, upstream claim, absolute paths in review docs.

## Deferred and open

- GLB compression (needs `gltfpack`, not installed).
- Bench scale cue (studio action 4) and the longer panel trims, title-specific copy, Saturn/GameCube split and accent colours from the UX review.
- Owner decision: the GitHub link carries the repository name; keep it, or rename the repository before a public launch.
