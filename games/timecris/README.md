# Time Crisis (Namco, 1995) — experimental local VR setup

The Hub can prepare your own Time Crisis application and game files for the pinned DR-89 Windows host. This setup is experimental: the native host has shown real desktop scene images, while Hub-to-headset launch, tracked aim, clean-close persistence and stage play still need validation.

The application is a **private local build** containing reconstructed original game code. It is supplied by you and stays in your local installation. The separate support archive supplies the OpenXR loader, the currently approved CC0 gun model and copyright notices. It contains no application and cannot run by itself. The Hub has no application/game download or automatic Steam shortcut step.

## Install and play

1. Use **Find files** to scan your own matching Time Crisis World TS2 Ver.B set with the available MAME metadata. Matching uses chip content metadata rather than the archive name. Missing or changed chips produce an explicit error; no game files are fetched.
2. Select **Time Crisis VR (experimental local build)**. Keep `runtime-support.zip` beside the local-build JSON receipt supplied with your approved build. The receipt identifies your executable, its SHA256 and the exact port/engine source commits.
3. Choose **Install**. When the installer requests the local build, use **Choose file** in recovery to select that receipt and retry the selected game. The support ZIP is verified separately; supplying only it does not satisfy the application requirement.
4. After installation, **Start in VR** starts the owned child through your currently selected OpenXR runtime. Use the Hub's running-game card to return or close the owned child. Headset validation remains pending; installation success is not stage-play acceptance.

Preparation copies only the nominated local application and verified support members into the installation's `User` directory. It checks and unpacks the required game chips into `User/roms`, normalizing the source-declared program aliases. Original input folders are read-only. The host changes directory to its own executable folder, so placing the executable inside `User` keeps settings, EEPROM, recordings, logs and captures there. No unsupported `--user` argument is forwarded. Existing options are retained; the first options seed disables physical crouching and enables the native laser.

The detail page selects the adopted `namco22-vr` control labels. Time Crisis has one player and one active gun: either trigger can select the firing hand. These labels are source mappings awaiting controller/gameplay validation; they are not a second gun, an offscreen-reload mapping or proof that our authored gun has been integrated.

## Requirements and provenance

- Your own matching game set and private local application.
- Port source commit `e5a5f7d204e4be8e96ed2396ebea3886c833f021`; engine commit `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`.
- The separate, verified support package and a working current OpenXR runtime with OpenGL support. The Hub does not change the system runtime.

The stored local-build receipt records your selected executable's byte identity and declared source provenance. It does not independently prove how an arbitrary user build was compiled. Private executable paths/hashes, game files and generated original-program material are not shipped in this repository or its artifacts.

The flat MAME route remains available when its emulator and media are found. The earlier standalone native host remains parked. Our authored gun/labels and racer integration follow separately.

Credits: [DR-89 Time Crisis VR](https://github.com/DR-89/time-crisis-vr), [namco22-decompile](https://github.com/spacestate1/namco22-decompile), and Namco for the original game. Retain the included source, dependency and model notices.
