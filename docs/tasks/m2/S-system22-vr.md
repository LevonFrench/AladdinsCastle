# System 22 in VR: build on the host that already works

For the lead and the Stereo boards session. Written 2026-10-12 by the outside reviewer after the owner's decision D63. It replaces the "Board 1" section of [4-stereo-boards.md](4-stereo-boards.md). The Wii work stays the main thrust (D52); this is the Stereo lane's work beside it.

## Why this changes

Two open-source VR ports of Namco gun games now run on Windows through OpenXR: [DR-89/time-crisis-vr](https://github.com/DR-89/time-crisis-vr) and [DR-89/time-crisis-2-vr](https://github.com/DR-89/time-crisis-2-vr). Both are MIT and both sit on the engine this lane already chose, [namco22-decompile](https://github.com/spacestate1/namco22-decompile). They have what our own host does not: a headset session, head tracking, controller aim, cover, a HUD plane, a saved world scale.

Our own work on the same problem reached the real game drawn twice in a desktop window with a fixed eye offset. Its newest part never ran. So we stop building a host from nothing and start from theirs.

Nobody has put the System 22 racers in VR. That is where this lane's own work goes.

## What may and may not be fetched

- **Source only.** Clone the three repositories above with git, each pinned to a commit that is written down in `setups/namco22-vr/`. Clones go in an ignored folder, never in the tree.
- **Never the release packages of the two DR-89 ports.** Their own notice and README say those packages include the original game files. Do not download them, mirror them, link them, or copy any file out of them. This is not something the owner's approval covers.
- Build everything from source. The engine's own release packages contain no game files, but we build it too, so the pin is the proof of what ran.
- **No models from their repositories** unless the file's licence is stated in the repository. We have our own guns ([guns-models.md](guns-models.md)); use those.
- Game files are the tester's own, placed by the tester. Which ones a tester has is private and stays under `.local/`.

## Order of games (D63)

1. **Time Crisis**, on DR-89's host, at DR-89's own engine pin first because that pair is known to work together.
2. **Rave Racer**, the first racer.
3. **Ace Driver**. The engine runs it on Rave Racer's code with one table of its own, so it should follow closely.
4. **Dirt Dash** and **Tokyo Wars**.
5. **Prop Cycle** and **Armadillo Racing**.

A game with no files to test on is skipped, not blocked on. Ridge Racer, Ridge Racer 2, Victory Lap, Cyber Cycles, the Alpine games and Aqua Jet are not in the engine: they stay on the flat route.

## Picture first

The last two rounds spent days on checks around a renderer that had not shown a game in a headset. This time the order is fixed:

1. Build DR-89's Windows host from source, unchanged.
2. Run Time Crisis in it on the null headset, under the approvals already given for that driver, and capture both eyes. That capture is the first report. Nothing else is written before it exists.
3. Only then change anything.

If step 1 or 2 fails, the report is the exact error and the one thing tried. No supervisor, validator or fixture is built around a step that has not yet produced a picture (R4's rule).

## What we add, in this order

1. **Install from the Hub** with the user's own files: find them by content, say what is missing, build or place the host, start and close it from the headset. Their `Install-ROM.ps1` is the reference for what the engine expects.
2. **Our guns**: the per-game model at the hand in place of theirs, with labels, and the laser choice.
3. **Two guns** where the game has two players.
4. **A per-game file** for each game: world scale and how it was derived, HUD rules, camera notes, known issues.

For the racers, on top of that:

- A seated camera at the driver's head, set per game, with a recentre.
- Wheel, pedals and shifter from our control sets instead of a gun.
- The speedometer, map and position on one plane.
- A comfort note per game. Prop Cycle is a flying bike and is the likeliest to cause motion sickness: say what was tried.

## What happens to the existing work

- The from-nothing host and the native launcher that never ran are parked. Do not run them and do not extend them.
- The synthetic tests stay where they still test something the new host uses (unprojection, aim round-trips, HUD separation). Anything that only tested the parked host is removed in the same change that removes its subject.
- The pending question to the owner about the rejected Time Crisis run is withdrawn.

## Credit and upstream

- Keep their copyright and licence notices with any file that came from them, and list both projects and their pins in our notices.
- Our changes are written as guarded patches on top of the pins, as before, so a new upstream version is a pin change.
- A fix that is not specific to us is worth offering upstream. That is the owner's decision each time.

## Done when

- Time Crisis is started from the Hub, in the headset, with our gun at the hand, and the owner has played a stage.
- Rave Racer is started the same way, seated in the car, steerable.
- Each game after that has its per-game file and a both-eyes capture.
- A clean checkout plus the written pins rebuilds all of it, with no game file in the tree, the history or a report.
