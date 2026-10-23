# Trying a Windows development build

There is no published 0.01 release yet. This is a checklist for a supplied
development package, not a claim that downloading the source gives you a
complete game system. CI creates a Windows x64 Hub artifact; the release owner
must identify the artifact's source commit and the separately supported system
build before handing it to a tester.

## What you need

- Windows x64 and a complete portable Hub folder, including its Qt DLLs, plugins,
  `resources/`, `games/`, `data/` and licence notices. Extract the entire folder
  somewhere writable; do not move just `aladdinscastle-hub.exe`.
- A working Quest-to-PC connection and SteamVR. The Hub does not install or
  configure the headset transport. The development controller notes describe
  Quest through ALVR; other transports need their own checks.
- The supported emulator/native VR program for your chosen system. These builds
  are separate from the Hub. Read the matching [setup README](../setups/).
- Your own legally obtained game files and any BIOS that system needs. The Hub
  does not supply them.

Most systems also use separately prepared `play/<system>/launch-<system>.ps1`
and `games.json` files. Those private development folders are not reconstructed
by Set-up. A stock emulator executable is not automatically equivalent to the
patched VR build named in a setup README.

## First start

1. Connect the headset and confirm that SteamVR works before testing the Hub.
2. Start `aladdinscastle-hub.exe`. First start opens **Set-up**. Settings, caches
   and logs stay in its portable `user/` folder. A missing catalog stops startup
   with **Catalog missing**; an incomplete folder is not a valid package.
3. Follow **Welcome → SteamVR → What you have → Where it is → Report**.
   **Yes, add it** registers the Hub with SteamVR. If registration fails, start
   SteamVR and retry. Registration does not turn on **Choose Startup Overlay
   Apps**; that is a separate SteamVR setting. **Not now** leaves desktop use
   available.
4. Select only systems for which you have the required program and game files.
   Choose their folders/programs in **Where it is**. Advancing runs the report.
   Review **Found, can start**, **Found, not right** and **Not found**, including each reason.
   Detection is not a guarantee of valid game content or a successful VR session.
5. Finish, open a game and select a supported view, then **Play in headset**.
   If SteamVR is stopped, the Hub may start it after this click, wait up to two
   minutes and offer Cancel. The setting **Let the Hub start SteamVR**, or
   `AC_NO_STEAMVR_START`, can require you to start SteamVR yourself.
6. Use the large **Back** button in the SteamVR panel. Use **Stop** or the
   supported game exit route to finish. Keep settings and diagnostic logs private;
   redact local paths and game-library details before reporting a problem.

## A conditional first-game example

For **Namco System 22 → Time Crisis**, choose the supported **Time Crisis VR
build** executable in Set-up. It needs its own non-empty `roms` directory beside
the executable; this route has no separate game-folder picker. No build selected
produces **The Time Crisis VR build is not set up on this PC**. An empty sibling
folder produces the report's missing-game-files message.

The folder check does not validate a complete working game. You still need the
correct, separately prepared native build and its required legal game inputs.
The repository's synthetic System 22 slice is not a substitute for that build.
If those inputs are unavailable, stop here: there is no complete public
download-to-first-game route to recommend yet.

## Limits of this guide

These steps were traced through `hub/qml/SetupPage.qml`,
`hub/src/core/setup/Setup.cpp`, `hub/src/core/setup/Report.cpp`,
`hub/src/app/HubServices.cpp` and `hub/src/core/launch/SteamVrStart.cpp` at main
`71f983b`. They were not executed on a clean tester PC. See [SETUP.md](../hub/SETUP.md)
for detailed behavior and [current status](status/current.md) for the separate
offline and headset evidence. Before 0.01, test the exact supplied package,
system build and first game on a clean machine.
