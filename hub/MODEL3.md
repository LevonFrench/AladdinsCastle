# Playing Sega Model 3 games from Hub

Open **VR games**. The tab lists ten Model 3 games that the stereo Supermodel build
draws in real 3D for the headset. Hub ships no emulator and no game files: you
point it at your own.

## Set up, once, on the desktop

**Set-up** on the VR games page (or ··· then Set-up) asks for both:

- **The program**: your Supermodel program. Hub starts it from the
  folder that holds `Config/Games.xml`, which is the program's own folder or one
  above it (a source build keeps the program a few folders below).
- **The folder**: the folder holding your own game files, one
  `<set>.zip` per game, named as Supermodel names them.

Both choices are saved in Hub's own `user/hub-settings.toml` (keys
`model3Executable` and `model3GameFolder`), which is never part of the
repository. A card says **Not ready** and names what is missing when the
program, its `Config` folder, the game folder or that game's file cannot be
found. **Check again** re-reads the folders.

## Choose a view, then start

Select a card to choose how you see the game (decision D65). The choice is
remembered per game, and the card's button starts with it directly.

| View | What you get |
|---|---|
| Screen | A 3D screen straight ahead, the size of the arcade picture |
| Wide screen | A camera and screen one and a half times wider. Experimental |
| Inside the game | The camera follows your head; score and text float on a panel |

Screen is the default until you choose. The game page says which view Play
uses and whether it is your choice or the default, and marks it on its card.

Start SteamVR with the headset connected first. Hub does not start or select a
VR runtime for these games; without a headset the emulator plays on the monitor.

## What Hub does when you start

Hub starts your emulator with its own folder as the working folder, the game
file and window options as arguments, and the view's `ACVR_*` variables in the
child's environment only. Any `ACVR_*` variable already set for Hub is dropped
first, so a view is always exactly its own settings. One game needs a start-up
input script to leave its first-boot service screen; Hub adds it for that game.
A game can have its own stereo for the two screen views, measured in the
headset: Sega Rally 2 uses an eye separation of 0.02 m and a zero-parallax plane
at 3.6 m, so the car sits on the road; the others keep 0.065 m and the screen's
own distance. The exact settings per view are in `src/core/model3/Model3.cpp`.

Hub writes only its own launch log under `user/logs/`. It never writes beside
the emulator or the game files; the emulator keeps its own settings and saves
as usual.

While a game runs, the now-playing card shows its title and view. **Exit game**
asks the emulator to close normally; the list returns when it has closed.
**Restart** closes it and starts the same game and view again. In the headset:
left stick click = Start, right stick click = Select or Coin, both stick clicks together
put the screen in front of you again, and holding both for 3 seconds quits and saves
(the same in every game; see `docs/controls.md`). On the PC keyboard: 1 starts, 3 inserts a coin, Esc quits.

## Not covered yet

The tab cannot be used to choose the emulator or the game folder from inside
the headset. Hub does not check the emulator build, so an ordinary Supermodel
will start and simply ignore the 3D settings. Headset behaviour of each game and
view is a property of the emulator build, not of Hub.
