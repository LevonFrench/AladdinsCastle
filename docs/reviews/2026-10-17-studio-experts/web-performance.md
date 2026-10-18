# Web platform and performance review: AC VR Hub website

Reviewer: web platform and performance lens (Unity Web + Performance roles translated to a hand-written Three.js 0.160 page). Report only; no site file touched.

## 1. Scope, evidence, limits

Read in full: `site/index.html`, `site/arcade.js`, `site/style.css`, `site/how-it-works.html`, `site/pipeline.css`, `site/sprites.js`; captures `stop3-racing.jpg`, `phone-mall-older.jpg`. I measured one thing: jsDelivr download sizes of `three.module.js` and `three.module.min.js` (a plain GET). Nothing was run in a browser; no GPU, device or network profiling. Labels: **[obs]** seen in source or capture, **[est]** counted or computed from source, **[inf]** derived from three r160 behaviour, **[hyp]** needs a test.

Where the Unity lens does not translate: no Brotli/WASM heap/Build Profile work (a ~65 KB ES module site has none), no Profiler, SRP Batcher, GPU Resident Drawer or Addressables. What does carry over: "name the bound before fixing", "bytes to first playable", dynamic resolution, static batching (here `mergeGeometries` / `InstancedMesh`), PSO warm-up (here `compileAsync`), and "load must degrade, not break".

## 2. Findings, ranked

### F1. HIGH: about 865 meshes, ~850 materials, ~880 geometries, no sharing (`arcade.js` 69-86 and every call site)
[obs] `std()`, `glow()`, `box()` and `plane()` allocate a new `MeshStandardMaterial` / `BoxGeometry` / `Mesh` on every call.
[est] Count from the builders: upright cabinet 18 meshes (21 with guns) x 21 = 339; sit-down 16-20 x 4 = 72; claw machine 36 x 4 = 144 (26 separate plush meshes each, each with its own material); prize counter 65; two ticket wheels 24; guns, controllers and kiosks about 134 (the five stop-2 guns at lines 1014-1033 are all submitted, four of them at scale 1e-4); room shell, storefront and props about 90. Total roughly 850-900 draw calls before frustum culling; perhaps 400-600 in view at a stop, plus about 20 for bloom/output. Distinct looks are probably under 80.
[inf] CPU submission, per-material uniform upload and ~900 `matrixWorld` updates per frame are the likely bound on phones and low-end laptops; desktop CPUs absorb it. Per-geometry buffers (about 3,500 GL buffers) also lengthen startup.
Remedy, in order:
1. Memoise `std()`/`glow()` by key (colour + options) and `box()` geometry by size: about 850 to 80 materials, 880 to about 150 geometries, with no visual change.
2. Merge static geometry per material with `BufferGeometryUtils.mergeGeometries` (room shell, cabinet bodies, prize shelves). Merge by world position into a few chunks so frustum culling still works. Expected 865 to about 150-250 draws.
3. Plush and prizes (about 150 meshes) become one `InstancedMesh` with `instanceColor`.
4. Freeze static objects: `matrixAutoUpdate = false` after the build.
5. Do not toggle `visible` on lights or add/remove lights to save cost: changing the light count recompiles every material program.

### F2. HIGH: no pixel or frame-rate governor; eight lights in every standard fragment (`arcade.js` 16, 36, 483-488, 1201-1248)
[obs] DPR cap is 1.75 desktop / 1.5 small (line 36), but `isSmall` is read once at load (line 12) and a 768 px iPad portrait counts as desktop. The loop runs at the display's rAF rate with bloom on; a 120 Hz iPhone/iPad or laptop panel renders at 120 fps.
[est] A 1440x900 laptop at DPR 2 renders about 2520x1575 = 4.0 Mpx into half-float targets: roughly 64 MB for the composer pair, ~20 MB bloom mips, ~65 MB for the default-framebuffer MSAA (see F4), so about 150 MB GPU. All ~790 `MeshStandardMaterial` meshes evaluate 7 point lights plus hemisphere plus an env sample per fragment, with no culling by light range.
[hyp] Integrated GPUs at DPR >1.5 miss 60 fps; low-end Android is fill-rate bound. Needs a device check.
Remedy:
- Rolling frame-time governor (first 2 s, then every 1 s): step DPR 1.75 to 1.25 to 1.0, then disable bloom (render via `renderer.render`). This is the dynamic-resolution analogue.
- Cap rendering to 60 Hz (30 on `isSmall`) by skipping frames in `frame()`.
- Pass `failIfMajorPerformanceCaveat: true` so software GL (SwiftShader) gets the static page, not 1 fps.
- Cut the light cost: merge the two mall lights into one, use `MeshLambertMaterial` for matte surfaces (carpet, walls, ceiling, black bodies), and use `MeshStandardMaterial` for the few shiny props.
- Replace the single `MeshPhysicalMaterial` glass (line 531) with `MeshStandardMaterial`; it adds a heavier program for one plane.

### F3. HIGH: failure paths leave an unusable page (`arcade.js` 15-34, 1257-1259; `style.css` 276-287)
[obs] The no-WebGL path is good: the constructor throws before `body.js` is added and the static card layout remains. Module-fetch failure (jsDelivr down, importmap unsupported) also leaves the static page, since `arcade.js` never runs.
[obs] But after line 21 adds `js` and line 27 appends the full-screen "INSERT TOKEN..." curtain (z-index 8), any later exception (PMREM needing float render targets on an odd GPU, a shader compile error, 3 s font race then a throw) leaves the curtain permanently up and the cards hidden behind it. There is no `webglcontextlost` handler, so on iOS memory pressure the canvas goes black and the loop keeps running.
[obs] Google Fonts CSS (index.html line 12, how-it-works line 12) is a render-blocking `<link>` to a third party; if it is slow or blocked the first paint waits. Using it also sends visitor IPs to Google, which is a privacy point for a public project page.
Remedy: wrap lines 22-1259 in `try { ... } catch { document.body.classList.remove('js'); curtain.remove(); }`; add `canvas.addEventListener('webglcontextlost', e => { e.preventDefault(); cancelRAF; body.classList.remove('js'); })`; self-host the three font files as woff2 with `font-display: swap` and preload the two the sign uses.

### F4. MEDIUM: `antialias: true` buys nothing under the composer (`arcade.js` 16, 1069-1073)
[inf] `EffectComposer` renders into its own half-float target with 0 samples; the final `OutputPass` draws a full-screen quad, so the context's MSAA never touches scene edges. Result: aliased edges (visible on cabinet silhouettes in `stop3-racing.jpg` [obs]) and a wasted multisampled backbuffer.
Remedy: `antialias: false`; pass `new WebGLRenderTarget(w, h, { type: HalfFloatType, samples: 4 })` to the composer (or add SMAA only on DPR 1 devices). At DPR >=1.75 samples 2 or none is enough. Also call `pmrem.dispose()` after line 49 and dispose the RoomEnvironment scene.

### F5. MEDIUM: load waterfall, library size, first-frame hitch (`index.html` 14-22, `arcade.js` 28-34, 1257)
[obs, measured] `three.module.js` is 1.27 MB raw / 257 KB gzip on jsDelivr; `three.module.min.js` is 671 KB / 166 KB. The importmap points at the unminified file.
[inf] Discovery chain: HTML to `arcade.js` to six addon modules and three to their shader/pass dependencies, about three serial round trips on a cold cache. The font race (up to 3 s) then runs after the scene could already have started building. The first `composer.render` compiles all programs synchronously, so the curtain freezes. `body.js` is added only after all of that, so the visitor sees the static layout, then the 160vh sticky layout jumps in (layout shift) [obs]. Also, `document.fonts.load` is only awaited so that canvas text draws in the right face; nothing needs to block on it.
Remedy: point the importmap at `three.module.min.js` (about 90 KB less transfer, about 600 KB less to parse); add `<link rel="modulepreload">` for `arcade.js`, three and the five addons; better, vendor them into `site/vendor/` and drop the CDN. Replace the font await by building the scene, then redrawing the sign/token/board canvases on `document.fonts.ready`. Call `await renderer.compileAsync(scene, camera)` before the first frame. Add the `js` class and the curtain in inline head script so the layout never swaps.

### F6. MEDIUM: canvas redraws are unconditional (`arcade.js` 233-262, 339-346, 1231-1232)
[obs] The sign repaints a 2048x360 canvas every frame for about 4.6 s per play, using `shadowBlur` 40 on nine glyphs, then re-uploads about 2.9 MB and regenerates mipmaps. That runs even in the deep-link case where the sign is off-screen. Six screen canvases redraw at 20 Hz whether or not any cabinet showing them is in view, each with `needsUpdate` and mipmap regeneration; the race screen alone issues about 320 `fillRect` calls per redraw.
Remedy: set `generateMipmaps = false`, `minFilter = LinearFilter` on screen textures; mark each screen mesh's `onBeforeRender` to record "seen this frame" and redraw only seen screens; throttle the sign to 30 Hz and skip it when `p > 0.6`; freeze screens when `reducedMotion`. Expected: about half the canvas work removed at any one stop.

### F7. MEDIUM: iOS Safari and portrait handling (`arcade.js` 12, 46, 1096, 1188-1195; `style.css` 13, 299-306)
[inf] iOS fires `resize` when the toolbar collapses; the handler reallocates the composer and all ten bloom targets each time, and there is no debounce. `camera.aspect < 1` and `isSmall` are evaluated once, so rotating a phone keeps the portrait camera and FOV 70 (and the mutated `STOPS[0].pos`). `vh`-based stop heights and `max-height: 48vh` ignore the dynamic toolbar. [obs: phone capture shows the ceiling filling the top half and the card covering the bottom half; the sign is small.]
Remedy: ignore height-only resizes under ~120 px; recompute FOV/camera start on `orientationchange` and width changes; derive the vertical FOV from a target horizontal FOV; use `svh`/`dvh` with `vh` fallback; set the canvas to `100lvh`.

### F8. MEDIUM: accessibility of nav, cards and motion (`style.css` 124, 191, 298; `arcade.js` 11, 675-680, 792-795, 1181-1186)
[obs] On phones `.stops a span { display: none }` removes the only text from ten links, leaving them unnamed (WCAG 4.1.2); the dots are 12 px (WCAG 2.5.8 wants 24 px). No `aria-current` on the active stop.
[obs] Inactive cards get `visibility: hidden`, so screen-reader users and heading navigation see only the current card. Remedy: keep cards in the accessibility tree (opacity only plus `pointer-events: none`), and on `focusin` inside a card call `scrollToStop`.
[obs] The arrow-key handler calls `preventDefault` regardless of modifiers, breaking Alt+Left and Cmd+arrow history navigation. `reducedMotion` is read once; the ghost wheel opacity pulse and swing (675-678), screen animations, token and ticket-wheel spin, and pointer parallax ignore it. The sign and head bob are handled correctly.
Remedy: label via visually-hidden text, enlarge targets with padding, add `aria-current="step"`, skip keys with modifiers, listen to the media query's `change` event and gate the remaining animations.

### F9. LOW: smaller costs
- `backdrop-filter: blur(8px)` on cards (style.css 184) sits over a canvas that repaints every frame, so each visible card is re-blurred at the full frame rate [inf]. On phones use an opaque background (about 0.9 alpha) and no blur.
- `hover()` raycasts about 400 meshes on every `pointermove`, including over empty `.stop` areas (lines 1163-1175). Cache one ray per rAF and skip touch pointers.
- Per-frame allocations: two `Vector3` at 1220-1221; `creditNum.textContent` and `creditFill.style.width` are written every frame (1245-1246). Guard with last-value checks.
- `pipeline.css` 175-180, 221 animates `left`/`top` (layout) on four infinite loops; use `transform`. `background-attachment: fixed` (line 8) repaints on scroll and is ignored on iOS. Chips and lane headers use 8 px pixel type.
- `how-it-works.html` and `sprites.js` are otherwise fine: no JS dependency beyond decorative inline SVG.
- Cloudflare Pages: nothing in the repo sets headers. Add `site/_headers` with `X-Content-Type-Options: nosniff`, a CSP (inline importmap needs a hash), `Referrer-Policy`, and short revalidation for non-hashed JS/CSS. Pages serves `/how-it-works` and 308-redirects `.html`, so link to `/` and `/how-it-works`. Add `og:image`/`canonical` for link previews. `.wrangler/` is untracked: add it to `.gitignore`. `site/README.md` still says seven stops (now ten).

## 3. Recommended sequence

1. F3 (try/catch, context-lost, self-hosted fonts): small, removes the unusable-page risk.
2. F1.1 and F4 (memoise, antialias): trivial and safe, then F1.2-4 (merge, instance).
3. F2 governor and F5 (min build, modulepreload, `compileAsync`): measurable; needs one real-device pass.
4. F6, F7, F8, then F9.
Lighting changes (see `lighting-rendering.md`) interact with F2: dropping lights changes look, so do it with captures.

## 4. Acceptance checks and uncertainty

- Add a `?stats` overlay printing `renderer.info.render.calls`, `triangles`, `geometries`, `textures`. Target after F1: under 250 calls in view.
- Throttled test (4x CPU, Fast 3G) with jsDelivr and Google Fonts blocked: static page reads in full; no curtain stuck.
- Real iPhone and a 120 Hz device: frame time, no resize churn, no context loss over 5 minutes of scrolling.
- Counts above are estimates; GPU and memory figures are not measured. Priorities F1-F3 are robust to that error; F2's severity depends on the device test.
