# Pinball tab: VR-ready Visual Pinball tables from the user's own collection

Status: 2026-10-11, owner decision D59. Hub lane. Read with [README.md](README.md) (rules for every session) and [R3-new-directions.md](R3-new-directions.md).

## Goal

A **Pinball** tab in the Hub, usable from the SteamVR dashboard. It lists only the tables in the user's own folder that are VR-ready, starts one in the headset with one click, and comes back when the table exits.

Why: there is no good in-headset launcher for Visual Pinball X (VPX). The desktop pickers make you take the headset off. The one in-headset launcher the owner tried runs on a modified older VPX build with its own self-updater, and the owner rejected it.

## Ground rules

- **The library is read in place and read-only.** No copies, no files written beside the tables, no deletes. Library folders are often junctions or network shares: never follow one with a recursive operation.
- **No downloads** of tables, ROMs, backglasses or art, and no links to download pages. The Hub never shows a download address, even when a metadata file contains one.
- **Tests use synthetic `.vpx` fixtures** built by a script. No real table, ROM or backglass enters the repository or a report.
- Paths, table names and counts from anyone's collection stay out of the repository.

## What "VR-ready" means

VPX draws any table in a headset, but only some tables carry a room, a cabinet and a backglass built for VR. Without them the playfield floats in a void. A table is listed when at least one of these holds:

| Evidence | How | State of knowledge |
|---|---|---|
| The Virtual Pinball Spreadsheet (VPS) database lists the matched table file with the feature `VR` | Match the local file to a VPS machine by title, maker and year, then to a table file by author and version | Checked on a copy of the database on 2026-10-11: the feature list is a fixed vocabulary that includes `VR` (1,192 of 4,907 VPX files), `Mixed Reality`, `FSS`, `no ROM`, `incl. B2S`, `incl. ROM`. Matching rules are to be built and tested |
| The table's own script carries a VR room | Read the script out of the `.vpx` and look for the room switch and room objects | Unverified. Newer tables expose the room in the in-game menu; older ones use a script variable such as `VRRoom`. Establish the markers from a sample before trusting this |
| The user marked it | A per-table override in the Hub's own data, both ways | |

The detail page says why a table is listed ("VPS: VR", "script: VR room", "marked by you"). The tab shows "N VR-ready of M tables" so the user can see what was left out.

## Where the facts come from

| Fact | Source |
|---|---|
| Title, maker, year, type, theme, players | The VPS database, or the table info inside the `.vpx` when there is no match |
| Authors, version, features | The VPS table file entry |
| ROM needed and present | The game name in the table's script, checked against the user's own ROM folder. States: ready, needs a ROM that is missing, needs no ROM. The Hub never offers to fetch one |
| Wheel image, backglass | Same-named files in the user's own media folders. Otherwise a generated tile with title, maker and year |

- A `.vpx` is an OLE compound file with the table info and script inside. The Hub needs a small reader for it (Windows structured storage is enough to start; a portable reader later for Linux). `vpxtool` is a good reference for the layout.
- The VPS database is **not shipped in this repository**; its licence is unverified. The Hub reads a copy from a path the user gives, or fetches one only with the user's yes. Its download addresses are never read into the Hub's model.

## The tab

- A top-level tab beside the games, named Pinball.
- A grid of wheels. Filters: maker, decade, type (solid state, electromechanical, original), ready to play, favourites, recently played. Search and sort.
- Detail: title, maker, year, authors, version, features, the VR evidence, the ROM state, and the controls card.
- It must work with the laser and thumbstick rules in [../../ui/06-vr-menu-controls.md](../../ui/06-vr-menu-controls.md): large targets, smooth scroll, no accidental launch while scrolling.

## Launch

- Add VPX to the emulator manifests as a located program: the **BGFX build of VPX 10.8**, which is the one with OpenXR. The OpenGL build has no VR. No automatic install until the official download channel and the licence are checked.
- Start it with the chosen table and have it close when the table ends. The flags are to be read from the build's own help or source, not from memory (`-play`, `-minimized` and `-exit` are the names to look for).
- VR is governed by `AskToTurnOn` under `[PlayerVR]` in VPX's settings (0 on, 1 autodetect, 2 off). Prefer a Hub-owned settings file if VPX accepts one on the command line. Never edit the user's own VPX settings without asking; if they are not set for VR, say so on the page.
- Runtime is OpenXR. While the table runs the Hub stays out of the way; when VPX exits the Hub is back on the same tab.

## Controls card

VPX 10.8.1 has a controller layout built in (read from its source for the hub wiki article `vpx-vr-openxr-quest`): triggers are the flippers, grips are Magna Save, A starts, B inserts a coin, X opens the in-game menu, Y exits the table, the left stick nudges and its click recentres, the right stick pulled down and released is the plunger and its click launches. Show it with the controls view, and warn that Y, next to X, exits the table.

## Not now

Future Pinball, Pinball FX and other engines. Switching a table's VR room on from the Hub. Anything to do with a physical cabinet.

## Done when

| Level | Evidence |
|---|---|
| Software | A scan of a synthetic library classifies every fixture correctly: VR by database, VR by script, not VR, needs no ROM, ROM present, ROM missing. The tab passes QML tests. `hubtool` prints the launch plan for a table without starting anything |
| On a real collection, kept private | How many tables were found VR-ready and by which evidence, a hand check of twenty of them, and one table started on the desktop through the Hub's launch plan |
| Headset, with the owner | Pick a table from the dashboard, play it, exit, and land back on the Pinball tab |
