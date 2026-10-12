# Fresh native background input

The fresh Umbrella and Darkside native recipes now seed `[Input] BackgroundInput=True` in `Dolphin.ini`. The existing absent-only seed policy preserves any user configuration. The lead owns the separate sitting configuration change.

A failing-first metadata check reported the missing setting in both recipes. New engine tests use each actual authored user-config step with a temporary synthetic app fixture: fresh defaults enable background input, existing False/unknown settings remain byte-identical, and repair/update retain user tuning. Lightweight recipe and privacy checks pass; hosted native compilation/software tests are pending. No software installation, download, game-file access or runtime was performed. Actual unfocused native-button behavior remains unverified while GPU/runtime is unavailable.
