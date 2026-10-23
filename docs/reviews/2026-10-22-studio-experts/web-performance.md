# Web platform and performance review: the refresh (2026-10-22)

Reviewer: web platform and performance lens. Report only; no site file touched.

## 1. Scope and evidence

Read in full: `website/index.html`, `style.css`, `models.js`, `bench.js`, `page.js`, `tools/sync_models.py`; `arcade.js` lines 1-80, 900-1125, 1160-1377 (model, batching, loop, governor). Measured locally: GLB node/vertex counts from each file's JSON chunk, and gzip size of each GLB. Measured on the preview with `curl -I` only: `Content-Type`, `Content-Encoding`, `Cache-Control` for a GLB, `models.json`, `arcade.js`. No GPU, device or browser run. Labels: **[obs]** read or measured, **[est]** computed from those numbers, **[inf]** follows from three r160 behaviour, **[hyp]** needs a device test.

Unity lens, translated: Addressables/bytes-to-playable maps to the `later()` lazy path; static batching and GPU instancing map to `mergeGeometries`/`InstancedMesh`; LOD maps to the hidden LOD1 nodes; dynamic resolution maps to the existing governor. Brotli/WASM heap, SRP Batcher and texture compression do not apply (no textures in any GLB: `tex=0`).

The 17 Oct fixes are in and hold: min build plus modulepreload for three and `arcade.js`, `antialias:false` with FXAA, `failIfMajorPerformanceCaveat`, a frame governor with a 60 fps cap, a debounced resize, static batching of the room, a fall-back-to-text path.

## 2. Findings, ranked

### F1. HIGH: 2.85 MB of GLBs on every visit, uncompressed, a quarter of it unused
[obs] The 13 files arcade.js loads sum to 2,852 KB; all 19 files are 3,875 KB. Cloudflare serves them as `model/gltf-binary` with **no `Content-Encoding`** and `Cache-Control: max-age=0, must-revalidate`. [obs] `models.json` and JS do get brotli. [obs, local] The same 13 files gzip to about 814 KB (all 19: about 1.0 MB), so the data is highly redundant float32. [obs] Every GLB carries LOD1 nodes that `models.js:38` only hides: 25-30% of vertices are never drawn.
All 13 download after the first frame whether or not the visitor leaves the mall, because `later()` fires them all at once (`arcade.js:967-1074`).
Remedy, in order of payoff:
1. In `sync_models.py` write web variants: drop LOD1 nodes, then `gltf-transform optimize --compress meshopt --texture-compress false` (quantize plus `EXT_meshopt_compression`), and `loader.setMeshoptDecoder(MeshoptDecoder)` from `three/addons/libs/meshopt_decoder.module.js` (about 20 KB gz, a lazy import). Keep node names (`grip`, `fx_muzzle`, trigger and slide nodes): `models.json` motion tables depend on them. `assets/` stays untouched. Expected 2.85 MB to about 0.4-0.7 MB [hyp: gzip says at most 0.8 MB; meshopt normally beats it].
2. Or, cheaper and no decoder: strip LOD1 only (about 0.7 MB saved) and serve GLBs with a `Content-Type` Cloudflare compresses. Unverified whether `_headers` can force that; test with `curl -I`.
3. Load by proximity, not at idle: stop-1 set (hand plus two guns, 641 KB) when `p < 1.5`; the stop-2 cycle when `p < 2.5`; ghost controls (366 KB) at stop 3; kiosks at 4; skip it all on `navigator.connection.saveData`. A visitor who reads the hero and leaves pulls 0 KB instead of 2.85 MB.

### F2. HIGH: the models roughly triple the per-frame draw cost and defeat the room's batching
[obs] A gun is not one mesh: 9-50 visible mesh nodes each (`con-revolver-chunky` 43, `mnt-mg-heavy` 50), each its own draw, plus the same number of hidden LOD1 nodes that `updateMatrixWorld` still walks. [est] The scene now adds about 521 mesh draws: stop 1 about 96, stop-2 cycle 233, mounted gun 50, kiosks 70, two ghost racers 72, plus 13 muzzle-flash spheres. The batching block runs before `later()` loads anything, and models sit under `userData.dynamic` holders, so none is merged. I did not measure the room's current count, but the 17 Oct work targeted about 150-250; the models are now likely the larger share.
Worst case, [obs] `arcade.js:994-999`: all eight stop-2 guns stay in the scene and the seven inactive ones are scaled to `1e-4`. Scale does not remove a draw: [est] about 200 wasted draws at stop 2, the stop that also runs the bench.
Remedy:
- `g.pivot.visible = (k === n)` instead of scale 1e-4 (one line; about 200 draws gone at stop 2). Same for each flash sphere: `flash.visible = kick > 0.55` (an opacity-0 transparent mesh is still drawn).
- In `models.js`, remove LOD1 nodes (`o.removeFromParent()`) rather than `visible=false`.
- After load, per model, merge static meshes by material (4 materials) with `mergeGeometries`, leaving the nodes named in `entry.motion` and `grip`/`fx_muzzle` separate: 35 draws becomes about 8 per gun. Better still, do it once in the web-variant export so GLB size shrinks too.
- Set `matrixAutoUpdate = false` on static child nodes after merge (about 1,000 matrix updates per frame saved).
- Ghost controls are additive and transparent, so they cannot be sorted into the opaque batch; merging them into one mesh per model matters most there.

### F3. HIGH: one stray rejection or error switches off the whole 3D page
[obs] `arcade.js:46-47` calls `fallBackToText` on **any** window `error` or `unhandledrejection`, permanently (`stopped = true`, loop ends, `js` class removed). The bench is a new source: `bench.js:45` runs `pick()` on chip click with no `.catch`; a failed or interrupted GLB fetch rejects, and the visitor loses the arcade behind the panels. [obs] Also: `models.js:11,16` cache a **rejected** promise forever (`catalogPromise`, `files`), so one dropped request breaks that model for the session; `arcade.js:994` and `:1022` use `Promise.all`, so one bad file drops all eight guns or all three ghost controls, leaving the floating rings with nothing in them. [obs] A missing file on Pages returns 200 `text/html` (SPA fallback; checked `models/nope.glb`), so the failure arrives as a parse error, not a 404. [obs] `webglcontextlost` falls back at once, without waiting for `webglcontextrestored` (three r160 restores itself); with two contexts on a low-memory phone, [hyp] the older (main) one is the likelier victim.
Remedy: catch inside `pick()` and show "could not load" in `.bench-info`; in `models.js` delete the entry on rejection and retry once; use `Promise.allSettled` and place what loaded; give `fallBackToText` a filter (ignore errors whose `filename` is not `arcade.js`/`models.js`); on context loss wait about 3 s for restore before falling back. Beware: a `ResizeObserver` loop notice fires a window `error`; filter `/ResizeObserver/` before adding one (F6).

### F4. MEDIUM: `later()` can starve, then lands as one burst
[obs] `arcade.js:967`: `requestIdleCallback` has no `timeout`. On a page rendering every frame there may never be idle time on a slow device, so the models may never appear on exactly the devices that struggle [inf]. Safari has no `requestIdleCallback`; the 400 ms `setTimeout` fallback is fine but fires all 14 callbacks together. [inf] Result on arrival: 13 parses, 80+ material allocations and a first-use shader compile in the same few frames (a hitch on the camera walk). Remedy: `requestIdleCallback(cb, { timeout: 1500 })`; a small queue that starts one model per callback; `renderer.compileAsync(holder, camera)` before `scene.add`. Also start `fetch('models/models.json')` early with `<link rel="preload" as="fetch" crossorigin href="models/models.json">` so the catalog is not on the critical chain (HTML, arcade.js, up to 3 s font wait, idle, catalog, GLB).

### F5. MEDIUM: two contexts share one frame budget and the governor cannot tell them apart
[obs] `bench.js:96` renders a second scene (antialias, DPR up to 2) in the same frame as the main loop. The governor times total frame gaps (`arcade.js:1292`), so bench cost reads as arcade cost, and it "never steps back up". [inf] A visitor who lingers at the bench can permanently drop the arcade to DPR 1 and no bloom. Two contexts are within every browser's limit (8-16); the risk is GPU memory on phones. [hyp] Remedy while the bench is on screen: have bench.js set `window.__benchOn`; arcade.js then renders at 30 fps and holds the governor. Bench also: starts at 300 px rootMargin and does a synchronous PMREM plus compile ([inf] visible hitch while scrolling); render only when something changed (it redraws a static scene every frame under reduced motion); dispose the previous model's materials in `pick`; bail when `body` has no `js` class (otherwise it loads a GLB for a hidden canvas on the text fallback); handle its own `webglcontextlost`.

### F6. MEDIUM: CSS cost over a live canvas
- `backdrop-filter: blur(10px)` (`style.css:201`) on every panel, 6-9 on screen at stops 4 and 5, over a canvas that changes every frame [inf]: the blur reruns each frame. No override on phones (`:344-368`). [hyp] Measurable on integrated GPUs and DPR-3 phones. Remedy: in the 720 px query and `@media (prefers-reduced-transparency: reduce)` set `backdrop-filter:none; --card-bg: rgba(16,10,28,.92)`; keep blur on desktop but only on `.panel-hero` and section heads.
- `arcade.js:1353` writes `--dim` on `.scrim` **every frame**; `style.css:167` also transitions `background`. [inf] A full-viewport gradient is re-rasterised on each change. Remedy: static gradient plus `opacity` driven from JS (compositor only), or bake the dimming into the OutputPass.
- `arcade.js:1359-1360` write `creditFill.style.width` (layout) and `creditNum.textContent` every frame, even when `.credit` is `display:none` on phones. Guard on last value; use `transform: scaleX`.
- `arcade.js:1338-1339` allocate two `Vector3` per frame. Trivial.

### F7. MEDIUM: scroll mapping goes stale after the bench loads
[obs] `measure()` (`arcade.js:1185`) runs at load and on resize only. `bench.js` fills about 19 chips and the info block later, and web fonts can swap after the 3 s race, so section heights change after `tops[]` was taken. [inf] On a short laptop viewport (stop 2 content exceeds 130vh) every later stop shifts, the camera drifts off its stop, and `scrollToStop` (live layout) and `progress()` (stale) disagree. Remedy: `new ResizeObserver(() => { measure(); p = progress(); }).observe(document.getElementById('walk'))`, and reserve `.bench-side` a `min-height` so the panel does not jump.

### F8. MEDIUM-LOW: accessibility and layout
- Bench: `<canvas aria-label>` needs `role="img"`; chips need `aria-pressed`; `.bench-info` needs `aria-live="polite"`; rotation is pointer-only (add `tabindex=0` with arrow keys). When the bench fails, `.bench-off` hides only the view; the copy "Pick a model to see it..." (`index.html:120`) and the empty column stay.
- Landmarks: two `<nav>`s, only one labelled (`hud-links` needs `aria-label="Site"`); no skip link past the HUD and nine dots; sections have no `aria-labelledby`; active dot has a class but no `aria-current`.
- Order is sound: one `h1`, `h2` per section, `h3` in panels. `.reveal` honours reduced motion (`style.css:219`), and `html` smooth scroll too. The 3D idle motion is gated except `fire()` timing and the bench auto-spin (gated).
- First paint: the static text page shows until three.js is fetched, then the curtain covers it and the layout jumps to 130vh sections under it. Add the `js` class and curtain markup from an inline head script (guarded by a WebGL2 probe) to avoid the flash.
- Google Fonts remains a render-blocking cross-origin stylesheet (`index.html:12`) and the curtain waits on it for up to 3 s; self-host two woff2 files.

### F9. LOW: caching and Pages
- [obs] `?v=` tags cover `style.css`, `arcade.js`, `bench.js`, `page.js` but not `models.js` (imported by both), `models.json` or any GLB. Safe today because Pages revalidates (`max-age=0`, ETag), but it costs about 20 conditional requests per repeat visit and `models.js`/`arcade.js` can pair across a deploy. Remedy: have `sync_models.py` write content-hash names (`arc-pistol-slide.3f9a1c.glb`) into `models.json`, and add `website/_headers`: `/models/*.glb` `Cache-Control: public, max-age=31536000, immutable`; HTML and `models.json` short revalidate; plus `X-Content-Type-Options` and `Referrer-Policy`. Import `./models.js?v=` consistently.
- Add `website/404.html` so a missing file is a real 404 and not HTML with status 200.
- `markClickable(holder, stop)` (`arcade.js:975`, `:958`) runs before the gun exists, so the guns themselves are not click targets; only the rings are. Call it again after `holder.add(gun.pivot)`, or leave guns unclickable on purpose.
- Raw `three.module.min.js` (about 166 KB gz) and the addons still come from jsDelivr; add `modulepreload` for the five composer addons, `GLTFLoader.js` and `BufferGeometryUtils.js` to shorten the discovery chain.

## 3. Recommended order and checks

1. F3 (catch in `pick`, retry, filter the global handler): small, removes the worst failure.
2. F2 first two bullets and F1.2/F1.3 (visibility not scale, drop LOD1, proximity loading): minutes of work, large effect.
3. F1.1 plus F2 merge in `sync_models.py`; then F6, F7, F4, F5, F8, F9.
Checks: `?debug` draw calls at stop 2 before/after (target under 250 total in view); throttled 4x CPU and Fast 3G, with `models/*.glb` blocked, then click a bench chip (page must stay up); a 120 Hz phone with the bench open for two minutes (governor level must not drop); repeat-visit request count. Counts above are estimates; none was run on a GPU.
