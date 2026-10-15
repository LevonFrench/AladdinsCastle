# First-run set-up and the VR games tab

The first start opens **Set-up**; later it is under **···**, **Set-up**. Hub
ships no emulator and no game files. Everything it starts is the user's own, found
through Hub's `user/hub-settings.toml`, which is never part of the repository.

## In the headset

The desktop window is a full way to use the Hub. Starting the Hub from the desktop
opens the window only; it never pops the SteamVR dashboard open.

The same Hub is also a panel in SteamVR's dashboard whenever SteamVR runs:

- It attaches when SteamVR is seen, whether SteamVR was running first, was
  started later, or was started by the Hub's own Play in headset.
- If SteamVR is still coming up, it tries again every 5 seconds, for a minute.
- When SteamVR quits, the panel goes and the window carries on.
- Window and panel show the same Hub. A game started in either shows as Now
  Playing in both, and Stop works from either.
- `--desktop` keeps the Hub a window only.
- Adding Hub to SteamVR (below) does not by itself start it. SteamVR starts the
  Hub only once "Choose Startup Overlay Apps" lists it as on.

Every game has explicit buttons. **Play in headset** is on every game.
**Play on monitor** appears where the system has a monitor mode: Dreamcast, and
Pinball's Launch on desktop. Model 3's screen, wide screen and inside-the-game are
choices under Play in headset.

If SteamVR is not running when Play in headset is pressed, the Hub:

1. starts SteamVR, using SteamVR's own starter from OpenVR's paths file, or
   otherwise Steam;
2. shows "Starting SteamVR…" with a Cancel button;
3. starts the game once SteamVR has been running for 5 seconds.

If SteamVR cannot be started, takes over two minutes, or Cancel is pressed,
nothing is started and the Hub says why. SteamVR is started only when that button
is pressed, never by a diagnostic or offscreen run, and never by a test.

## Watchdog

The Hub watches the programs of the systems it has been set up for.

**What it counts as a game program:** a program in a system's own program folder
(the folder of the program chosen in Set-up), or a program with one of that
system's known names in the folders around it:

- Visual Pinball, PinMAME, the B2S backglass server, DMD hosts and DOF.
- Supermodel.
- The Model 2 MAME build and its viewer.
- PCSX2.
- Flycast.
- Time Crisis VR.

It goes by folder, never by name alone, so the same program anywhere else is
never touched. SteamVR and Steam are never touched, wherever they are.

**Running in the background:** on start and every 3 seconds, the Hub lists any
of those programs running outside the game it is running now. That covers
programs left behind by a crash or a closed window, and programs started some
other way. They show on a strip in the window and the panel, with End and End
all. Ending is graceful first: PS2's and Model 2's own stop files, and every
window asked to close. After 4 seconds, the program and everything it started
is ended, as long as it is still that same program.

**Crashes:** a game the Hub started counts as crashed when it ends with a crash
code (an access violation, a stack overflow, heap corruption and the like, said
in words), or when Windows Error Reporting opens for it while it runs. The Hub
then:

- shows "<game> crashed: <why>" on the last-session banner;
- ends the error report and the game;
- ends what the game left running outside its own tree;
- keeps a record in `user/logs/crashes.json`, at most the last 30.

Each record holds the time, the game, the exit code and its meaning. It also
holds the end of the emulator's own log where one exists (Supermodel.log,
VPinballX.log, flycast.log), otherwise the Hub's launch log. Settings shows the
last 10.

**In the background:** a running game that is not responding, has no visible
window, or is minimised for 5 seconds after its first 15 seconds is said on the
Now Playing card. The card offers Bring to front (not for a hung game) and End
game. A Model 2 game has no window of its own and is never called hidden. A
headset game is judged by its processes and windows only: the Hub does not read
SteamVR's own frame state.

## Pages

1. **Welcome**: what Hub is, and that the user's folders are only read.
2. **SteamVR**: if SteamVR's own application list already names Hub, the page
   says so. Otherwise it asks; **Yes** registers Hub's dashboard manifest and
   nothing else. Plain steps follow for finding Hub in the headset and for
   starting it with SteamVR.
3. **What you have**: Arcade, Pinball, Consoles, and under them the systems Hub
   can start today.
4. **Where it is**: per ticked system, the folder of the user's files and the
   program that plays them. A folder found beside one already chosen is offered.
5. **Report**: each game is Ready, Not right (with the reason) or Missing (only
   the name Hub looks for). The report is saved in `user/cache/` and **Rescan**
   repeats the read. Hub never says where to get game files.

## Systems

| System | Folder setting | Program setting | How a game is checked |
|---|---|---|---|
| Sega Model 3 | `model3GameFolder` | `model3Executable` | Zip directory checksums against the emulator's own `Config/Games.xml`; optional regions ignored; another version of the same game inside the zip is accepted, as the emulator does |
| Sega Model 2 | `model2GameFolder` | `model2Executable` (the emulator; its viewer and working folder sit beside its folder), optional `model2RuntimePath` for the build's own libraries | The emulator build is asked for its own list of the game (`-listxml`, no window, nothing written): the game's files by checksum, and each device set that has files of its own. A stand-in for the billboard set is accepted and said. Controls on Virtua Cop 2 only, new and untested; the others are view only |
| Namco System 22 | none | `system22Executable` | The native build and a non-empty `roms` folder beside it |
| Visual Pinball | `pinballFolder` | `pinballExecutable` | The table scan: VR-ready, readable, ROM present |
| Sony PlayStation 2 | `ps2GameFolder` | `ps2Launcher` (the user's launcher script, run with the `ps2Python` it needs; `ps2StopFile` is the file that asks a running game to quit) | The serial in the disc's own SYSTEM.CNF (`.iso`, `.bin`, `.chd`); the file name for `.gz` and when no serial is read. A game is startable when the launcher has an entry for it. The launcher plays its own disc copies. Exit and Stop write the stop file and wait; SteamVR must already be running |
| Sega Dreamcast | `dreamcastGameFolder` | `dreamcastExecutable` | The disc's own product number, read from `.chd`, `.gdi`, `.cdi` or `.cue` images; the file name only when the number cannot be read |

Systems and their settings are listed in `src/core/setup/Setup.cpp`. Each check
is one function in `src/core/setup/Report.cpp`; a system without one reports
that it cannot be checked yet.

## Four separate facts

For each game Hub keeps apart: it is in Hub's list; how to start it is known;
it was found on this PC (the report's "Found, can start"); and the owner has
accepted it in the headset. Only the last needs a person: Hub reads it from
`user/accepted.toml`, which it creates empty and never writes to again. Each
line names a game by its id (such as `model3-scud`) or its title, with a date.
Finding a game is never shown as accepting it.

## Starting

**VR games** lists Model 3 (see [MODEL3.md](MODEL3.md)), Model 2, Time Crisis and
the Dreamcast discs of the last report. Model 2 runs two programs: the emulator,
which shows no window, and then its viewer, which is given the emulator's process
id; when the viewer ends the emulator is ended too. Its whole launch line is in
`src/core/model2/Model2.cpp`. Model 2 quits through a stop file the Hub owns,
`user/runtime/model2-<game>.stop`, cleared before each launch and passed to the
viewer as `ACVR_M2_STOP_FILE`: Exit game, Restart, Stop and closing the Hub write
it, the viewer asks the emulator to quit, and the emulator saves its high scores
and settings. Stop forces both programs only if they are still running 40 seconds
later. A game that is not ready is greyed with its
reason. A Dreamcast game without a VR profile opens its page with a warning
before it can be started. Dreamcast starts the emulator from its own folder with
`-config config:vr.Xr=yes|no <disc image>`; Time Crisis starts its program from
its own folder. Dreamcast in a headset is new and untested.
