# The Agency: fix-and-verify pass on the AC VR Hub site

Date: 2026-10-17/18. Mode: implement and verify. Branch `claude/website`. Baseline 84ee3d2; fix pass d664d4f; repair round on top (this commit).

## Team

| Role | Owner | Why |
|---|---|---|
| Writer, integration, skeptic | coordinator (Claude Opus) | owner wants Claude to implement one piece at a time |
| UX review of How It Works and Game List | agency persona `design-ux-architect` (+ `design-ui-designer`), Sonnet | the studio panel had no owner for page UX |
| Independent verification | Sonnet verifier (different model from the writer) | consequential, broad change |

Inputs: [studio review SUMMARY](../2026-10-17-studio-experts/SUMMARY.md), [ux-subpages.md](ux-subpages.md). Verdicts: [verification.md](verification.md).

## What changed

- Lighting: bloom threshold 1.1, strength 0.45, radius 0.35; emitters lifted to real HDR level (OutputPass ignores `toneMapped:false`); wall neon 3 → 1.6; white gun albedo greyed; dark bodies lifted; ghost wheel brighter.
- Camera: yaw/pitch interpolation (no whip pan), per-stop pos/look/fov from the composition report, path waypoint clearance, portrait phones aim at each subject with a wider lens, parallax scaled by aspect, walk eases on real time.
- Props: guns are extruded side profiles with the trigger guard as a hole; kiosk consoles 1.6×, gun moved off the TV axis.
- Performance: shared materials/geometry, static batching (draw calls 856 → 251–405 measured with `?debug`), refresh-aware frame cap, quality governor with warm-up and display-relative threshold, FXAA, minified three, modulepreload after the import map.
- Robustness: init errors, unhandled rejections and context loss fall back to the readable page; `failIfMajorPerformanceCaveat`.
- Accessibility: phone stop dots keep accessible names; inactive cards stay in the accessibility tree; focusing a card link walks to its stop; arrow keys ignore modifiers; HUD 10px on phones.
- Subpages: phone HUD shows two links and the How page links to the Game list; World 4 header aligned, fixed arrows and per-step labels on phones; stage paths in fixed columns or a vertical path below 1000px; pixel text 10px minimum; Game List jump nav, system headings with tallies, PS2 compacted, clearer totals.

## Verification receipts (evidence ladder)

| Level | What ran | Result |
|---|---|---|
| 1 Source | verifier diff review; `node --check` on arcade.js and sprites.js | pass |
| 3 Runtime | local preview, console clean, `?debug` draw-call counts | pass |
| 4 Visual | all 10 stops at 1280×800, phone 375×812 for mall, gun, racing, Wii, How, Game List; gun direction re-checked after repair | pass after repair |
| 6 Live | Cloudflare Pages preview https://preview.ac-vr-hub.pages.dev | deployed |
| 5 Device / 7 Owner | real phones, GPUs, owner look-and-feel | not run; owner check |

Verifier regressions: R1 gun profiles mirrored (fixed: `rotateY(+π/2)`), R2 World 4 arrow escape lost (fixed). Smaller risks fixed: frame cap quantising on 72/144 Hz and false downgrades on 30 Hz, modulepreload order, focusable controls in hidden cards, World 3 chip overflow, 8px phone HUD, pinball card counted as a game.

## Open items

- Studio action 9 (cabinet detail: screen height from cabinet height, coin door, glass edges, file props) not done. Low impact.
- Racer seats not shortened or tinted (stop 3 now frames over the seat, so the wheel is visible anyway).
- iOS toolbar resize is debounced but still rebuilds render targets.
- No real-device or cross-browser run.
