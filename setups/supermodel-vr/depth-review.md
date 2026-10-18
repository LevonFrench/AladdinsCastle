# Offline screen-mode depth review

Use the existing hidden capture path with `ACVR_STEREO`, explicit eye separation,
world scale and convergence. Remove `ACVR_XR`, `ACVR_VR` and all fake-headset
variables from the capture environment. Capture both eyes in the same rendered
frame; two separate emulator runs are not a stereo pair. Keep captures, save
states, launchers and per-installation findings in private operator storage.

The screen camera uses parallel eye translations and off-axis frusta. For a
viewport of pixel width `w` and horizontal tangent bounds `l,r`, define
`f = w/(r-l)`. With world scale one and eye separation `s`, the horizontal
right-minus-left disparity is:

```
d = f*s*(1/c - 1/z)
z = 1/(1/c - d/(f*s))
```

`c` is convergence and `z` is camera depth in the emulator's chosen metre scale;
convergence zero means infinity (`1/c` is zero in this special case). These are
not measured physical distances. At other world scales, include that scale in
both convergence and eye translation before inferring game-coordinate depth.

Choose textured windows on the car body or tyre and on the road immediately
under it. Exclude HUD text, silhouettes crossing unrelated depths and animated
transparency. Mean-centre luminance, search horizontal integer shifts by mean
absolute difference, then interpolate the winning cost and its neighbours for
a subpixel estimate. Record the box, texture variation, matching cost and
viewport. Inspect both eyes and neighbouring windows: an ambiguous match is
an unavailable measurement, not a depth. A one-pixel error can dominate a small
reciprocal-depth estimate, so do not infer a precise far distance when
`1/c - d/(f*s)` is comparable to that error. A bounded exaggerated-separation
capture with convergence at infinity can improve identifiability; label it as
a measurement instrument, never a headset setting.

Review each 3D viewport separately. The same pixel disparity does not imply the
same depth if the car and road use different focal lengths. A title card or
bumper camera without a visible car cannot establish car-to-road agreement.

As a provisional screen-mode trial, place convergence near the measured road
contact depth and reduce separation conservatively. Capture the proposed pair
at the same saved state, frame, resolution and camera settings. Report measured
before/after disparity for both car and road, including their difference; a
common convergence shift alone cannot remove a true relative-depth mismatch.
Do not edit launchers as part of the review. Wide-camera and inside modes need
their own checks; screen convergence does not tune the inside-mode camera.

The private review should list current settings (including implicit defaults),
capture identity and binary hash, measured depths and uncertainty, proposed
settings, before/after disparities, and exact blockers. Record timeouts even
when a capture script returns success after killing its child. Offline matches
do not establish headset scale, comfort, sharpness or physical controller fit.
