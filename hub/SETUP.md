# First-run set-up and the VR games tab

The first start opens **Set-up**; later it is under **···**, **Set-up**. Hub
ships no emulator and no game files. Everything it starts is the user's own, found
through Hub's `user/hub-settings.toml`, which is never part of the repository.

## In the headset

Started from the desktop while SteamVR is running, Hub also opens as its
SteamVR dashboard panel and shows the dashboard on it; its window stays on the
monitor. `--desktop` keeps it a window only. Started while SteamVR is not running,
it is a window only, as before. Adding Hub to SteamVR (below) does not by itself
start it: SteamVR starts it only once "Choose Startup Overlay Apps" lists it as on.

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
