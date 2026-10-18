# Lighting and rendering review: AC VR Hub website

Reviewer: lighting and rendering lens (Blender lighting skill applied to Three.js 0.160). Report only; no site files touched.

## 1. Scope, evidence, limits

Examined: `arcade.js` lines 36-49 (renderer, fog, env), 69-74 (`glow`), 482-488 (lights), 556-595 (room, wall neon, cabinet body), 597-680 (cabinets, marquees), 870-1034 (gun and controller models), 1068-1073 (bloom). Captures `stop0`-`stop7`, `stop8/9-older`, `sign-t3-final`. I cropped and sampled `stop2-gun.jpg` and `stop3-racing.jpg` for pixel values.
Not done: no GPU run, no histogram of the live page, no mobile device. Labels: **[obs]** seen in source or a capture, **[inf]** derived from the code plus three r160 behaviour, **[hyp]** needs a test.

Where Blender technique does not translate: no light groups or comp rebalance (one forward pass, one composite); no probes or GI (everything is direct light plus a hemisphere term, so black albedo stays black); no shadows are enabled; `exposure` in the view transform is the right idea and maps directly to `renderer.toneMappingExposure`; "darken the set before brightening the subject" and "check desaturated" transfer fully.

## 2. Findings, ranked

### F1. HIGH: the white gun clips to flat white and veils its backdrop (`stop2-gun.jpg`, `stop5-wii.jpg`; `arcade.js` 877-878, 1014, 1062, 1071)
[obs] Sampled gun pixels in `stop2-gun.jpg` are (252,246,248), a clipped plateau with a pink halo roughly 100 px wide. In `stop5-wii.jpg` the same `shell` model reads as a flat white slab. The model is `gunModel('shell', '#f2f2f5', ...)`, the only near-white albedo in the scene (linear about 0.89).
[inf] Cause, in order of contribution:
1. **White albedo under a hot, close light (main cause).** The pink `PointLight` at (-3.5, 3.0, -5) with intensity 14, decay 1.4 sits about 2.6 m from the gun at (-5, 1.45, -6.5), delivering roughly 3.6 cd-equivalent irradiance. Diffuse is 0.89 x 3.6 / pi x cosine, near 0.7-1.0 linear before specular. Add the (0, 3.0, -2) warm light (about 0.27), hemisphere (about 0.1) and the `RoomEnvironment` env term at `envMapIntensity` 0.4. The pre-tone-map value is already around 1.0-1.5.
2. **Bloom threshold 0.9 (line 1071).** `UnrealBloomPass` thresholds linear HDR luminance before tone mapping. A lit white surface above 0.9 is treated as a light source and blooms. Strength 0.7 and radius 0.5 spread it into the cabinet screens behind.
3. **ACES at exposure 1.05.** Three scales input by exposure/0.6, so linear 1.0 already maps near 0.85 display-linear. There is no headroom for a white object to keep form.
4. **Emissive self-light (line 877, 0.12) is minor.** It adds about 0.11 linear (about 8 % of the total). Removing it alone will not fix the clip. It does flatten the shading on light bodies, because emissive is added regardless of normal.
Also: the gun is scaled 2.5x (line 1016) so the clipped area is large, and it sits against a bright pink skyline screen, so separation after the bloom veil is near zero.

Remedy (all Three.js, no new passes):
- Cap prop albedo: change `'#f2f2f5'` to about `'#9ea3b0'` (linear about 0.35) at lines 1014 and 1062; accent `'#4fd2ff'` stays. Rule: no hero prop above about 0.55 linear luminance.
- In `gunModel`, set emissive intensity to 0 for light bodies (or 0.03 for dark ones). Keep the "read against the dark room" job with a rim, not self-light (F4).
- Raise `UnrealBloomPass` threshold to 1.1 (see F3), so only intentional emitters bloom.
- Lower the pink point light (line 487 first entry) to about 9, or raise it to y 3.2, so props sit near 0.5-0.8 linear.
- Better subject separation: a dedicated `SpotLight('#fff1d8', 30, 6, 0.4, 0.8, 2)` above and in front of the turntable, no shadow, aimed at the gun. This is the Blender "key on the subject, darken the set" move. Acceptance: sampled gun pixels peak below about 235 with visible form change across the body.

### F2. HIGH: `toneMapped:false` is a no-op, so every sign, screen and marquee is ACES-compressed (`arcade.js` 516, 604, 634, 647, 649, 705, 739, 756, 773, 852, 860, 872, 1048, 1066)
[inf] In three r160 the renderer applies per-material tone mapping only when drawing to the canvas. Under `EffectComposer` the scene renders to a render target, so the flag does nothing and `OutputPass` (line 1073) tone-maps the whole frame. Screens and the red-gold sign therefore go through ACES's desaturating shoulder. This explains why the `sign-t3-final.png` gold reads slightly washed and why attract screens never reach "emitter" brightness relative to the neon strips. [hyp] confirm by flipping one material to `toneMapped:true` and seeing no change.
Impact: the screens and marquees, the things meant to be the brightest, are not distinguishable by value from lit props, and bloom picks neon strips (F3) over screens.
Remedy: delete the misleading `toneMapped:false` flags (or leave them but stop relying on them) and give emitters real HDR level via the colour multiplier: for example `mat.color.setScalar(1.35)` on screen materials and `1.6` on sign and marquee materials. With the threshold at 1.1 (F3) screens bloom softly; lit props never do.

### F3. HIGH: bloom is over-driven and neon strips blow to white, washing the top of every frame (`stop2`, `stop4`, `stop8-older`; lines 573, 1071)
[obs] A cyan/white band crosses the top 15-20 % of nearly every capture; in `stop2-gun.jpg` and `stop4-model3.jpg` its core is pure white with a wide teal veil that sits directly behind the card headline area.
[inf] Wall neon is `glow('#4ff3ff', 3)` at 26 m long. Emissive linear is about (0.23, 2.7, 3.0), tone-mapped to near white (ACES desaturates high values), then bloom threshold 0.9 and strength 0.7 smear it. Perspective makes the strip the longest bright edge in frame, so it wins the "highest contrast area is the focal point" test over the subject at stops 1-6.
Remedy: strip `glow('#4ff3ff', 1.6)` (linear peak about 1.6, hue survives ACES), bloom `strength 0.45, radius 0.35, threshold 1.1`. Optionally dim the strip with distance by splitting it into 4 m segments with lower intensity beyond the camera's reach. Acceptance: strip core stays visibly cyan, not white, and the top band no longer exceeds the headline card's luminance.

### F4. MEDIUM: black cabinet bodies and seats are value voids, no form read (`stop3-racing.jpg`, `stop4-model3.jpg`, `stop8-hub-older.jpg`; lines 584, 587, 633, 662-663)
[obs] Seat boxes at stop 3 are near-black silhouettes with no edge light; the lower fronts of the upright cabinets at stops 4 and 8 are black rectangles with two orange slot "eyes".
[inf] Albedo `#121016` is linear about 0.006; the hemisphere (0.35) and point lights cannot lift it, and only the `side` material carries an `envMap`. There is no shadow or contact darkening either, so the black masses also float (the strongest "game-y" tell).
Remedy: lift the body to `#1d1a27` (linear about 0.012), add `envMap: envTex, envMapIntensity: 0.5, roughness: 0.4` to `black`, the seats, and the control-panel boxes; add a faint cool rim: one `DirectionalLight('#7a9bff', 0.25)` from behind-left with no shadows. Add contact shadows without shadow maps: a 256 px radial-gradient `CanvasTexture`, multiply-blend, one transparent plane per cabinet and kiosk at y 0.004 (cheap, no extra passes).

### F5. MEDIUM: the ghost wheel at stop 3 is the stop's subject but is the dimmest thing on screen (`stop3-racing.jpg`; line 669)
[obs] In the crop the wheel reads as a faint grey-teal arc against the dark seat; the lit road screens and bright cabinets dominate.
[inf] Additive `#7ff8ff` at opacity 0.35 gives about 0.07-0.35 linear, below any bloom threshold.
Remedy: `gm.color.multiplyScalar(2.2); gm.opacity = 0.55;` so it exceeds the new threshold and haloes lightly; add a slow pulse (opacity 0.45-0.65) only if reduced-motion is off.

### F6. MEDIUM: blank marquees blow out as white slabs (`stop1-motion.jpg` at the first cabinet; lines 634, 649; `marqueeTex`)
[obs] At stop 1 the marquee above the cabinet is a hot white rectangle with bloom spill. [inf] Marquee texture fill is largely `#fff4c2`, linear about 0.9, above the 0.9 threshold. With blank multicolour marquees the colour is now the point, so a white fill loses it. Remedy: keep fills at 0.6-0.8 linear, push colour saturation instead, and let the F2 colour multiplier set the final level.

### F7. LOW: exposure and tone map choices (`arcade.js` 38-39)
[inf] ACES skews saturated neon (cyan toward white, blue toward purple), which is part of F3. Three 0.160 includes `AgXToneMapping`, which desaturates highlights more gently. [hyp] A/B `AgXToneMapping` at exposure 1.15 on stops 0, 2 and 4 only if F1-F3 leave the neon feeling chalky. Also consider easing `toneMappingExposure` from 1.15 (mall, stop 0) to 1.0 (arcade) by `p`, a cheap eye-adaptation cue for the doorway.

### F8. LOW: exposed metals have no environment (lines 533, 592, 629, 658)
`metalness 0.6-0.8` materials without `envMap` render dark and plasticky. Add `envMap: envTex, envMapIntensity: 0.6` (or set `scene.environment = envTex` once and drop the per-material copies; check the effect on cabinets first).

### Fine as is
- Stop 0 sign: the red band, checker border and gold wordmark are the clear first read (`stop0-mall.jpg`, `sign-t3-final.png`); value hierarchy there works. The bright ceiling panel (line 505) is slightly hot; `#b8b2a4` would calm it.
- Fog (`#0d0818`, 18-46, line 44) only touches the far arcade from stop 0 and the scoreboard; it helps depth cheaply and is not a problem.
- Carpet confetti against dark ground has strong value separation (all captures). Palette and hue contrast across stops is good.

## 3. Recommended sequence
1. F1 + F3 together (albedo cap, emissive off, spot key, threshold 1.1, strip 1.6, bloom 0.45/0.35): one retune, same capture set.
2. F2 and F6 (HDR multipliers on screens, marquees, sign) immediately after, since they depend on the new threshold.
3. F4 (black lift, rim, contact blobs), then F5.
4. F7/F8 only if the stops still feel chalky or flat.
Dependencies: composition (camera framing of the gun against the skyline screen) belongs to the layout reviewer; if the gun's background is changed, F1's separation check should be redone.

## 4. Acceptance checks
- Re-capture `stop2`, `stop3`, `stop4`, `stop5` at 1280x800; the gun's brightest pixels are below about 235 and show shading; the top band is cyan, not white.
- Desaturate each capture: subject must own a distinct value group from its backdrop.
- Check that screens still glow (they should, via the multiplier) and no new bloom leaks onto lit props.
- Remaining uncertainty: the `toneMapped` no-op and numeric irradiance values are inferred from r160 source behaviour and arithmetic; confirm with one test toggle and a pixel read. Mobile bloom cost and any per-stop exposure behaviour were not measured.
