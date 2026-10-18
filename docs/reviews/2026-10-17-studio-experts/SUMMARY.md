# Studio review summary: AC VR Hub website

Date: 2026-10-17. Mode: review only. Target: `site/` on branch `claude/website` (commit 84ee3d2 plus the ARCADE→AC sign and the game list page, uncommitted at review time).

## Panel

| Deliverable | Lens (role) | Report |
|---|---|---|
| Lighting, exposure, bloom, neon balance | scenario-blender-lighting-rendering | [lighting-rendering.md](lighting-rendering.md) |
| Shot per stop, moves between stops | scenario-blender-previs-storyboard | [camera-composition.md](camera-composition.md) |
| Procedural props: cabinets, guns, controllers, kiosks | scenario-blender-hard-surface | [hard-surface-props.md](hard-surface-props.md) |
| Load, runtime cost, robustness, accessibility | scenario-unity-web + scenario-unity-performance | [web-performance.md](web-performance.md) |

None of the five studio applications is involved. The roles were used as expert lenses on a hand-written Three.js scene. Gap: graphic and UX design of the How It Works and Game List pages has no owner in this collection; it goes to The Agency.

Evidence level for every finding: source reading plus 1280×800 captures. Nothing was profiled on a GPU or run on a phone or headset.

## Coordinator verification of the top claims

| Claim | Check | Result |
|---|---|---|
| White gun blows out at stops 2 and 5 | `captures/stop2-gun.jpg`, `stop5-wii.jpg` | Confirmed (observed) |
| `toneMapped:false` is ignored under EffectComposer | three r160: RenderPass draws into a render target, so the renderer skips per-material tone mapping; OutputPass tone-maps every pixel | Confirmed (source knowledge of r160) |
| Look target lerp whips 180° between opposite-wall stops | `poseAt` lerps `STOPS[i].look` to `STOPS[i+1].look`; stops 3→4 and 4→5 face opposite walls, so the look point crosses the camera | Confirmed (by reading the maths) |
| Curtain stays up forever if init throws | curtain is removed only after `frame()` starts at the end of the module | Confirmed (source) |
| About 865 meshes / 850 materials | count from builders | Estimate, not measured |
| Gun grip rake inverted | rotation sign on line 881 | Inferred, not verified visually |

## Prioritised actions (one owner each in the fix pass)

1. **Gun and white props blow out.** Grey the white albedos (`#9ea3b0` to `#aeb4c2`), no emissive on gun bodies, raise the bloom threshold above 1.0 and give emitters real HDR level (`color.setScalar(1.35–1.6)`) so signs and screens still glow. Lower the 26 m wall neon (emissive 3 → about 1.6) and bloom strength 0.7 → about 0.45, radius 0.5 → 0.35. (Lighting, props)
2. **Camera whip pan.** Interpolate yaw and pitch angles between stops instead of lerping look points. (Camera)
3. **Cards cover the subject.** Re-frame each stop so the subject sits near 27% of width for right-side cards and near 69% for left-side cards; use the report's pos/look/fov table. Separate stops 5 and 6, which are the same shot today. Narrower fov (46–52°) on hero props. (Camera)
4. **Ghost wheel hidden at stop 3.** Over-the-seat camera from the report, plus a brighter ghost (colour ×2.2, opacity 0.55) and shorter, tinted seats. (Camera, lighting, props)
5. **Robustness.** try/catch around init that falls back to the static page, `webglcontextlost` handler, curtain removed on failure. (Web)
6. **Draw calls and frame cost.** Memoise `std()`/`glow()` materials and box/plane geometries, instance plush and prizes, merge static meshes; cap the frame rate and step DPR or bloom down when frames run long. (Web)
7. **Gun silhouettes.** One extruded side-profile Shape per pistol with the trigger guard as a hole and a small bevel; keep slide, drum and hammer separate so the shot animation still works. Check grip rake. (Props)
8. **Wii and PS2 kiosks.** Move the gun off the camera-to-TV axis, scale the consoles up about 1.6×, give them visible edges. (Props, camera)
9. **Cabinet detail.** Derive screen height from cabinet height, lighter coin door, frame edges on glass cases, visible "file" props at the prize counter. (Props)
10. **Path clearance.** Replace waypoint (-0.6, 1.6, -4.4) with (-2.4, 1.6, -4.9). (Camera)
11. **Accessibility and phones.** Accessible names on the phone stop dots, do not hide inactive cards from screen readers, ignore arrow keys with modifiers, re-read aspect and fov on rotation, avoid rebuilding targets on iOS toolbar resize. (Web)
12. **Load.** Minified three build, `modulepreload`, `antialias` is lost under the composer (add SMAA or FXAA, or render at a higher DPR); consider self-hosted fonts. (Web)

## Disagreements and open questions

- Lighting suggests a dedicated SpotLight key on the gun turntable; web performance warns that every extra light costs every standard-material pixel. Resolve by keying the gun with an emissive rim or a light limited by layers, not another scene-wide light.
- The fov changes per stop (camera) interact with the narrow-screen fov rule (web). Keep one rule: per-stop fov on desktop, wider on portrait.
- Owner decisions held: one gun at stop 2, blank marquees, ARCADE→AC sign, ghost wheel lying back.
