# Trying a Windows development build

There is no published 0.01 release yet. This is the short path from a supplied
development package to a first game. It is not a claim that the source download
is a complete game system: the Hub comes with no emulators and no games, and the
release owner must say which artifact (and which separately supported system
build) you were given.

## Identify the candidate first

Ask the release owner for the exact workflow/run and source commit, the ZIP name
and SHA-256, and the supported first game and view. The current workflow artifact
is `AladdinsCastle-Windows-x64`, containing `AladdinsCastle.zip`; after extraction,
check the source commit in `BUILD-INFO.txt` against the handoff. An artifact from a
draft PR can differ from main and from a development installation.

The handoff must also identify the separate emulator/host build and all required
DLLs, cores, profiles and launcher scripts. A successful compile or a passing
`fresh-install` job does not supply or test that complete route. If any required
piece is missing, ask for a complete handoff before trying to play.

## What you need

- Windows x64 and the complete portable Hub folder or ZIP. The package is checked
  before it is made: every runtime file must be inside and no private path, game
  or BIOS file may be. Extract the **whole** folder somewhere writable; do not
  move just `aladdinscastle-hub.exe`.
- A headset connected to the PC and SteamVR. The Hub does not install or set up
  the headset connection. (Quest through ALVR has historical headset results;
  the supplied candidate still needs its own check.)
- The supported emulator or VR program for the system you want to try. These are
  separate from the Hub; see the matching [setup README](../setups/). Stock
  emulator downloads are not interchangeable with the documented VR builds.
  Some routes also need a launcher and its interpreter, manifest or profiles;
  see the [system fields](../hub/SETUP.md#systems) and the supplied handoff.
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
5. **Put the headset on.** The intended universal controls below require the
   compatible patched build; verifying them is part of this test:
   - left stick click = Start;
   - right stick click = Select or Coin;
   - click both sticks together = put the screen in front of you again;
   - hold both stick clicks for 3 seconds = quit and save.

   The left Menu button opens the SteamVR dashboard, where the Hub panel is:
   that is the way back to the Hub from any game. Use the large **Back** button
   in the panel to go back a page. For pinball, use **Exit game** in the Hub panel unless the handoff explicitly
   supplies the patched hold-to-quit VPX build. Its compile artifact alone does
   not establish that the control works in your headset.

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

## Record the first-game result

Record the Hub source commit and ZIP hash, system-build identity, game and view,
and whether Set-up, launch, real display, headset picture, controls, recentre,
Exit game and restart each worked. Keep private file paths and raw logs out of
public reports. Record any skipped step as untested. A scan result, compile or
synthetic clean-folder check does not count as a real clean-PC game pass; a past
headset result does not accept the new candidate. Use the
[release gates](release/0.01-checklist.md#before-saying-001) for the full checklist.

## Reporting a problem

In the Hub, open **Settings** (or the help panel) and choose **Save a problem
report**. It writes one ZIP of the Hub's logs and the last game's log, with your
folder names, account name, emails and keys taken out. No game files or saves go
in. **Open the reports folder** shows where it is. Open the ZIP and look through
it before you send it; the redaction is checked by tests but is not perfect (a
folder name with spaces and no file extension at the very end of a line can stay).
Settings and the raw logs in `user/logs/` stay private.

## Limits of this guide

These steps follow `hub/qml/SetupPage.qml` and the code behind it. They were
traced in code and tests, not run on a clean tester PC. See
[SETUP.md](../hub/SETUP.md) for detail and [current status](status/current.md)
for the offline and headset evidence. Before 0.01, test the exact package, system
build and first game on a clean machine.
