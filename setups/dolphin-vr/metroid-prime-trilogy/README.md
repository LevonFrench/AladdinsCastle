# Metroid Prime Trilogy single-core option

`single-core.ini` is an optional per-game fragment. Merge its `[Core]` setting into a separately selected game INI after preserving that file; it does not replace the controls or other settings. The offline runner equivalent is `--config Dolphin.Core.CPUThread=False`. Removing this override restores the selected user's threading default.

Earlier offline source-frame tests favored single-core emulation with the experimental XFB stereo-preservation patch off. That patch remains default-off. Single-core does not establish a clipping fix, distinct rendered eyes, GPU completion, headset pacing or owner acceptance. Compare matched gameplay states and capture policies before recommending this option; menu timing is insufficient.

This fragment is not installed automatically and changes no launcher or shared user folder. Keep each disc region's configuration separate. Gameplay checkpoints, saves and captures remain private.
