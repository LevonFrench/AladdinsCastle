# Trying a Windows development build

There is no published 0.01 release yet. This is the short path from a supplied
development package to a first game. It is not a claim that the source download
is a complete game system: the Hub comes with no emulators and no games, and the
release owner must say which artifact (and which separately supported system
build) you were given.

## What you need

- Windows x64 and the complete portable Hub folder or ZIP. The package is checked
  before it is made: every runtime file must be inside and no private path, game
  or BIOS file may be. Extract the **whole** folder somewhere writable; do not
  move just `aladdinscastle-hub.exe`.
- A headset connected to the PC and SteamVR. The Hub does not install or set up
  the headset connection. (Quest through ALVR is the tested route.)
- The supported emulator or VR program for the system you want to try. These are
  separate from the Hub; see the matching [setup README](../setups/).
- Your own legally obtained game files, and any BIOS that system needs. The Hub
  never supplies them, and Set-up never says where to get them.

## From download to first game

1. **Start SteamVR** and check it shows the headset ready.
2. **Start `aladdinscastle-hub.exe`.** The first start opens **Set-up**. Settings,
   caches and logs stay in the `user/` folder beside it.
3. **Follow Set-up in order.** A line under the page names tells you which step
   you are on and the one thing to do now.
   1. *SteamVR and headset*: choose **Yes, add it** so the Hub appears in the
      SteamVR dashboard. If it fails, start SteamVR and try again. **Not now**
      is fine: the Hub still works in its window.
   2. *What you have*: tick the systems you own game files for.
   3. *Folders and programs*: for each system, choose the folder of your files and
      the program that plays them. Each card says what it still needs. **Skip**
      leaves a system out.
   4. *Read my files*: the Hub looks in the folders and changes nothing in them.
      The report says, per system, what **can start**, what is **not right** (with
      the reason) and what was **not found**, and the one thing to do next.
4. **Choose Finish**, open a game, pick a view and choose **Play in headset**.
   If SteamVR is not running the Hub may start it and wait up to two minutes
   (there is a Cancel). The setting **Let the Hub start SteamVR**, or the
   environment variable `AC_NO_STEAMVR_START`, turns that off.
5. **Put the headset on.** Controls are the same in every game:
   - left stick click = Start;
   - right stick click = Select or Coin;
   - click both sticks together = put the screen in front of you again;
   - hold both stick clicks for 3 seconds = quit and save.

   The left Menu button opens the SteamVR dashboard, where the Hub panel is:
   that is the way back to the Hub from any game. Use the large **Back** button
   in the panel to go back a page. A pinball table is the one exception to the
   quit: leave it from the Hub panel with **Exit game**.

If something is wrong, Set-up is always under **···**, **Set-up**, and **Rescan**
reads the folders again. "Found, can start" means the game's files and its
program are on this PC; it does not promise the game works well in the headset.

## A conditional first-game example

For **Namco System 22 → Time Crisis**, choose the supported **Time Crisis VR
build** program in Set-up. It needs its own non-empty `roms` folder beside the
program; this route has no separate game-folder picker. No build chosen gives
**The Time Crisis VR build is not set up on this PC**. An empty `roms` folder
gives the report's missing-game-files message.

The folder check does not prove a complete working game. If you do not have the
correct build and legal game files, stop here: there is no complete public
download-to-first-game route to recommend yet.

## Reporting a problem

Keep settings and logs private. Redact local paths and game-library details
before you send anything. The logs are in `user/logs/`.

## Limits of this guide

These steps follow `hub/qml/SetupPage.qml` and the code behind it. They were
traced in code and tests, not run on a clean tester PC. See
[SETUP.md](../hub/SETUP.md) for detail and [current status](status/current.md)
for the offline and headset evidence. Before 0.01, test the exact package, system
build and first game on a clean machine.
