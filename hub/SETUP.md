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

## Game library

The "Where is it?" step of Set-up has a **Game library** card. You choose one or
more big folders of your own games, subfolders and all (`libraryRoots`), and the
Hub finds every game it plays under them.

**How it looks:**
- It goes up to six folders down and never enters links, junctions or system
  folders. It runs off the GUI thread, shows progress, and can be cancelled.
- It is read-only: nothing under the roots is written, renamed or moved.

**How it knows each game:**

| System | How it is known |
|---|---|
| Wii | the game id in the disc header (`.iso`, `.wbfs`, `.rvz`, `.wia`) |
| PS2 | the serial in the disc's SYSTEM.CNF (`.iso`, `.bin`, `.chd`); `.cso` and `.gz` by name |
| Dreamcast | the product number in the disc's own header (`.gdi`, `.cdi`, `.cue`, `.chd`); a `.bin` named in a cue sheet is read through that sheet |
| Model 3 and Model 2 | the zip's set name; the Set-up report then checks what is inside by CRC, as for a system folder |
| Pinball | `.vpx` tables, counted per folder; "Use this folder for Visual Pinball" sets the pinball folder |

**What it keeps:** each game's own file. The result goes to
`user/cache/library.json`, and "Look again" searches the folders once more.

**Where it fits:** a folder chosen for a system still comes first. The library
fills in what that folder does not have, in the Set-up report, the game rows and
the launch.

**When there are several copies:** the Hub picks one by its rule:

1. read from the disc itself;
2. the release with a VR profile;
3. USA, then World, Europe, Japan;
4. the plainest format;
5. a folder named for the system;
6. the shortest path.

The game's page lists the copies under "Copy used". Picking one there remembers it
(`libraryChoice`).

**What it reports:**
- per system, the games found and the discs that are not in the Hub's list;
- the files it read but did not recognise, as counts of disc images and archives.

On a real collection with about 70 system folders, it read 31,568 candidate files
in about 96 seconds. That first search is cached; the next one runs only on
request.

## One SteamVR policy

- **Who starts SteamVR:** only the Hub's Play in headset does, and only when it is
  pressed. The launch service no longer starts SteamVR's monitor. A catalog game
  that needs SteamVR goes through the same Play in headset path: start SteamVR,
  wait, with Cancel. Without that path, a SteamVR game with SteamVR off is refused
  with a plain message.
- **Kill switch:** the setting "Let the Hub start SteamVR" (`startSteamVr`), or
  the environment variable `AC_NO_STEAMVR_START`, stops the Hub from ever starting
  SteamVR. Play in headset then asks you to start it yourself.
- **OpenXR runtime:** every headset launch, and every catalog SteamVR game, gets
  `XR_RUNTIME_JSON` pinned to SteamVR's own OpenXR runtime, when the PC has one.
  An OpenXR game never lands on another runtime the PC has set as its default.
- **Overlay:** the headset panel only joins a SteamVR that is already running. Adding
  the Hub to SteamVR uses a background connection, which never starts SteamVR:
  start SteamVR first, then add the Hub.
- **Tests:** no test starts SteamVR. A Hub given no starter starts nothing, and no
  test reaches a real VR_Init.

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
other way. They show on a strip in the window and the panel, with Stop and Stop all. This includes a game started outside the Hub, from a .cmd
launcher for example, as long as it runs from a set-up folder. Stopping is graceful first: PS2's
stop file, Model 2's (the Hub's own recent ones, and the viewer's default beside
itself), and every window asked to close. After 4 seconds, the program and
everything it started is ended, as long as it is still that same program.

**Stop:** a game the Hub started shows as Now Playing in both the desktop window
and the headset panel. This includes a game started from the panel or with Play in
headset. Stop works the same from either place. It sends the system's graceful quit
first: Model 2's stop file (the viewer gives MAME 30 seconds to save), PS2's stop
file, VPX's ordinary close, the gun room's close, or every window asked to close.
The game's whole process tree is then ended if it is still running: after 40
seconds for Model 2, 15 seconds for PS2, pinball and the gun room, and 5 seconds
for the others. Each Model 2 launch has a stop file of its own, removed when the
game ends.

A game's process tree is set to end with the Hub from the moment it starts. If the
Hub closes or crashes, nothing of the game is left running. SteamVR is never
ended with it, even when a game started SteamVR itself.

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

## What the Hub runs and keeps

- **A found emulator is run only with a yes.** Reading what each game needs
  from a MAME build (`-listxml`) means running that program. The Hub asks once,
  on the Report page, and keeps the answer as `queryEmulators` (Settings, Ask
  found emulators for their game lists). Until then Model 2 games and the main
  scan's MAME sets are found by their names only, and the report says so. A
  list saved by an earlier run is still read.
- **The user's SDL library never loads into the Hub.** The pinball gamepad check
  runs in a copy of the Hub started for that alone
  (`aladdinscastle-hub --probe-gamepads <SDL library>`): no window, at most 8
  seconds, and only a device list comes back.
- **Shareable text names no account.** Crash records (`user/logs/crashes.json`)
  and launch failure logs write the home folder as `%USERPROFILE%`. The caches
  in `user/cache/` keep the full paths they need to find your files; they stay
  on this PC and are not meant for sharing.
- **The SteamGridDB key is kept protected**: see [ARTWORK.md](ARTWORK.md).

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
   repeats the read. Hub never says where to get game files. Until it is
   answered, the page asks whether the Hub may run the emulators it found for
   their game lists.

## Systems

| System | Folder setting | Program setting | How a game is checked |
|---|---|---|---|
| Sega Model 3 | `model3GameFolder` | `model3Executable` | Zip directory checksums against the emulator's own `Config/Games.xml`; optional regions ignored; another version of the same game inside the zip is accepted, as the emulator does |
| Sega Model 2 | `model2GameFolder` | `model2Executable` (the emulator; its viewer and working folder sit beside its folder), optional `model2RuntimePath` for the build's own libraries | With `queryEmulators` on, the emulator build is asked for its own list of the game (`-listxml`, no window, nothing written): the game's files by checksum, and each device set that has files of its own. A stand-in for the billboard set is accepted and said. Controls on Virtua Cop 2 only, new and untested; the others are view only |
| Namco System 22 | none | `system22Executable` | The native build and a non-empty `roms` folder beside it |
| Visual Pinball | `pinballFolder` | `pinballExecutable` | The table scan: VR-ready, readable, ROM present |
| Sony PlayStation 2 | `ps2GameFolder` | `ps2Launcher` (the user's launcher script, run with the `ps2Python` it needs; `ps2StopFile` is the file that asks a running game to quit) | The serial in the disc's own SYSTEM.CNF (`.iso`, `.bin`, `.chd`); the file name for `.gz` and when no serial is read. A game is startable when the launcher has an entry for it. Views: 3D screen, and Inside the game for the games listed in `src/core/ps2/Ps2.cpp` (`kInsideGames`, with the launcher's flag in `kInsideFlag`), none until each is confirmed; the choice is kept per game The launcher plays its own disc copies. Exit and Stop write the stop file and wait; SteamVR must already be running |
| Sega Dreamcast | `dreamcastGameFolder` | `dreamcastExecutable` | The disc's own product number, read from `.chd`, `.gdi`, `.cdi` or `.cue` images; the file name only when the number cannot be read |
| Nintendo Wii | `wiiGameFolder` | `wiiExecutable` (the DolphinXR build's Dolphin.exe; its User folder beside it holds its settings) | The game id in each disc's own header (.iso, .wbfs, .rvz, .wia), any region; what the build still lacks per game is said |

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

The Wii gun games run in the headset through the user's DolphinXR build, by the authored route of the
Chronicles recipes (D52-D54): `Dolphin.exe --user <build>/User -C Graphics.VR.EnableOpenXR=True
-C Graphics.VR.ResolutionScale=1.0 -C Graphics.VR.EagerHeartbeat=False -C Graphics.Stereoscopy.StereoMode=0
-C Dolphin.Interface.ConfirmStop=False --exec <disc>`, started from the build's folder. The last option
is the Hub's: without it Dolphin asks on the monitor before it closes, so Exit game would wait. The seven
listed games are the planned gun games: Umbrella and Darkside Chronicles, The House of the Dead: Overkill,
Gunblade NY & L.A. Machineguns, Sin & Punishment: Star Successor, Dead Space: Extraction and Link's
Crossbow Training. Umbrella Chronicles, played this way, is ready. Any other game starts behind "Read first", which says what the build lacks: its own VR settings
(`User/GameSettingsVR/<game id>.ini`) and, for the two held docked, docked controls. The build's
controls are the generated split-hold set for the right hand. Every other disc is listed by its game id.
