# Metroid Prime Trilogy single-core option

`single-core.ini` is an optional per-game fragment. Merge its `[Core]` setting into a separately selected game INI after preserving that file; it does not replace the controls or other settings. The offline runner equivalent is `--config Dolphin.Core.CPUThread=False`. Removing this override restores the selected user's threading default.

Earlier offline source-frame tests favored single-core emulation with the experimental XFB stereo-preservation patch off. That patch remains default-off. Single-core does not establish a clipping fix, distinct rendered eyes, GPU completion, headset pacing or owner acceptance. Compare matched gameplay states and capture policies before recommending this option; menu timing is insufficient.

This fragment is not installed automatically and changes no launcher or shared user folder. Keep each disc region's configuration separate. Gameplay checkpoints, saves and captures remain private.

In the next matched restored-gameplay check, both requested threading modes completed a 60-wall-second native window at 3x/MSAA 4 with the XFB experiment off and zero logged FIFO errors. Single-core recorded 3,524 fresh stereo-eligible XFB sources and no repeats; default threading recorded 3,499 fresh stereo-eligible sources and 54 repeats. Emulated time advanced 58.792067 and 59.275883 seconds respectively. These are source-availability measurements, not unique game-content FPS or headset delivery. Effective threading was not separately instrumented. This scene did not reproduce a large stereo-source deficit, and single-core did not establish a blanket speed gain. Severe clipping remained in both pre-window captures; retain this as an optional comparison rather than replacing an existing default automatically.
