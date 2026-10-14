# First-run set-up and the VR games tab

The first start opens **Set-up**; later it is under **···**, **Set-up**. Hub
ships no emulator and no game files. Everything it starts is the user's own, found
through Hub's `user/hub-settings.toml`, which is never part of the repository.

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
| Sega Model 2 | `model2GameFolder` | `model2Executable` (the emulator; its viewer and working folder sit beside its folder), optional `model2RuntimePath` for the build's own libraries | The emulator build is asked for its own list of the game (`-listxml`, no window, nothing written): the game's files by checksum, and each device set that has files of its own. A stand-in for the billboard set is accepted and said. View only: no controls yet |
| Namco System 22 | none | `system22Executable` | The native build and a non-empty `roms` folder beside it |
| Visual Pinball | `pinballFolder` | `pinballExecutable` | The table scan: VR-ready, readable, ROM present |
| Sega Dreamcast | `dreamcastGameFolder` | `dreamcastExecutable` | The disc's own product number, read from `.chd`, `.gdi`, `.cdi` or `.cue` images; the file name only when the number cannot be read |

Systems and their settings are listed in `src/core/setup/Setup.cpp`. Each check
is one function in `src/core/setup/Report.cpp`; a system without one reports
that it cannot be checked yet.

## Starting

**VR games** lists Model 3 (see [MODEL3.md](MODEL3.md)), Model 2, Time Crisis and
the Dreamcast discs of the last report. Model 2 runs two programs: the emulator,
which shows no window, and then its viewer, which is given the emulator's process
id; when the viewer ends the emulator is ended too. Its whole launch line is in
`src/core/model2/Model2.cpp`. A game that is not ready is greyed with its
reason. A Dreamcast game without a VR profile opens its page with a warning
before it can be started. Dreamcast starts the emulator from its own folder with
`-config config:vr.Xr=yes|no <disc image>`; Time Crisis starts its program from
its own folder. Dreamcast in a headset is new and untested.
