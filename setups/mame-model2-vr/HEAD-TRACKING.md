# Head-tracked Model 2 VR: source findings only

No head-tracked renderer or widened-FOV experiment was built. These findings
refer to MAME commit `9a119fe94494433fb49b3a05ae7c38318bf20055`.

Model 2's HLE geometry path transforms object vertices to view space, then
`apply_focus` multiplies x/y by focal lengths. Depth division happens in
`model2_3d_project`. A head pose can enter before that divide: undo the stored
x/y focus, apply the inverse head translation/rotation and per-eye transform
in game units, then refocus, clip and project. The actor-height scale estimate
is approximate; each game's physical scale needs an appropriate reference.

Wider head-tracked views need wider four-plane frusta, an appropriate near
plane, larger/rescaled viewport/scissor bounds and render targets, and the
headset's per-eye projection. Moving only the existing screen cannot expose a
full VR scene. The presenter's quad layers implement a stereo screen; genuine
VR would need projection layers and game-camera-relative head/eye poses.

The current stereo patch keeps submitted polygons before central-frustum
clipping, so geometry at those edges remains available for the shifted eyes.
There is some offscreen geometry in the tested lists, as visible in the large
offset development pair. That does not establish a usable margin for head
turns. Geometry the game's i960 program never submits cannot be recovered by
widening MAME's clip planes. Backface flags, LOD, lighting, occlusion and view
selection are also based on the original game camera. Whether each game stops
submitting particular scenery beyond an edge is not measured here; no per-game
coverage percentage or claim of a complete surrounding world is justified.
Identical tile layers remain flat, and backgrounds/HUD would need separate
decisions for a freely moving camera.

Retained polygons can be reprojected without advancing emulation. A late
headset redraw would also need a coherent snapshot of textures, palettes and
viewport state, plus safe ownership of the software renderer's work queues.
The separate viewer currently receives bitmaps, not that geometry/state.
Higher headset resolution will require a measured software-rendering budget.

Virtua Cop's aim enters through `INPUT_PORTS_START(vcop)` in
[model2.cpp](https://github.com/mamedev/mame/blob/9a119fe94494433fb49b3a05ae7c38318bf20055/src/mame/sega/model2.cpp):
`P1_X`, `P1_Y`, `P2_X`, `P2_Y` are 10-bit light-gun ports. Player 1 ranges are
x `0x083..0x276`, y `0x024..0x1a9`; trigger bits are in `IN1`.
`model2o_state::vcop` binds the four ports to the Model 1 I/O 2 board.
[model1io2.cpp](https://github.com/mamedev/mame/blob/9a119fe94494433fb49b3a05ae7c38318bf20055/src/mame/sega/model1io2.cpp)
the I/O CPU maps `fpga_r` at `0x8100..0x810f`. Offsets 0..7 return low/high
coordinate bytes, in Y1/X1/Y2/X2 order;
offset 8 supplies offscreen flags using a 5% border. A VR pointing ray should
map into these original calibrated screen-coordinate ports and preserve the
trigger/reload behavior. Virtua Cop 2 instead uses the 315-5649 serial-channel-2
multiplexer with `lightgun_mux_r/w`, `lightgun_data_r` and `lightgun_offscreen_r`
in the same Model 2 driver. None of these input paths was changed.
