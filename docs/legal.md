# Legal and content policy

This is not legal advice. It sets out the project's rules.

## Game content

- AladdinsCastle **never ships, downloads or links to** ROMs, BIOS files, disc images or game binaries.
- Users supply their own dumps and PC game installs. Config files describe what is needed by name and hash only.
- Pull requests, issues or packs that contain or link to game content will be removed.
- Third-party setups are separate projects with their own terms. Recipes download them only from their original release pages. Some third-party releases may bundle game content. We point users to content-free builds where they exist and do not mirror any release.

## Code licences

- Project-owned code defaults to **GPL-3.0-only** ([LICENSE](../LICENSE)). `libacvr/` uses [MIT](../libacvr/LICENSE), subject to any separately marked upstream files. Built assets have separate terms below. See the [notice index](../THIRD-PARTY-NOTICES.md).
- Preserve upstream copyright, licence and attribution when publishing emulator patches or modified builds. A repository-level GPL notice does not change an upstream file's licence. In particular, GPL-2.0-only code cannot simply be relabelled GPL-3.0-only; review the pinned files and the terms of the added code before distributing a combined build.
- Upstream projects keep their licences. namco22-decompile, DR-89 Time Crisis VR, VC2VR, UEVR and OpenXR-Toolkit are MIT. Supermodel, PCSX2 and PenguinScreen2 are GPL. The pinned MAME 0.289 distribution specifies GPL version 2, with per-file exceptions; do not assume an or-later grant. Check each file's own header: DuckStation, for example, has files under CC-BY-NC-ND-4.0, which must not be copied into our tree.
- PenguinScreen2's bundled VR profiles use a separate non-commercial licence.

## Art and trademarks

- Arcade game names, logos, marquees and cabinet designs belong to their owners (Sega, Bandai Namco, Konami and others). The adapted `arc-light-gun` model is third-party art under the terms below. Game artwork downloaded by users stays on their machine, with a record of where it came from.
- "Aladdin's Castle" was the name of a Bally/Midway arcade chain. Before any commercial or store release, clear the project name for trademark use.
- Cabinet 3D models we ship must be original or properly licensed (for example CC-BY with attribution).
- **Original gun, hand and control models** are CC0-1.0 under their directory notices. They use descriptive ids and omit logos and maker lettering. Their Python builders are separately licensed source code.
- **Exception: `arc-light-gun` and its previews are CC BY 4.0.** They adapt "Namco Arcade Gun" by Martoscar, through hotd2-vr's converted mesh. Preserve the author, original source and licence links, and the record of modifications in [the model metadata](../data/guns/arc-light-gun.toml) and [asset licence](../assets/guns/LICENSE). Removing lettering does not remove the attribution requirement. The [CC BY 4.0 terms](https://creativecommons.org/licenses/by/4.0/) apply; this asset is not CC0.

## Metadata services

Scrapers use each service's API under its own terms and quotas, from the user's machine, with the user's credentials where required. We don't bulk-mirror metadata databases.

## Disc identity data

`data/serial-index.json`, `data/wii-disc-index.json` and `data/dreamcast-disc-index.json` map public disc IDs (serials, product numbers, GameTDB-style IDs) to catalog games. They hold facts for catalog titles only: no game content, no local paths, no artwork. This is a partial extract of the sources below, not a mirror of them. Each row's `source` names where it came from. Terms for the sources are **not yet verified**; confirm each one before the 0.01 tester release and record the answer here.

| Data | Source and pinned revision | Licence or terms |
|---|---|---|
| PS1 rows in `serial-index.json` | MAME `hash/psx.xml` at `1a38c50b5d5e0e9e6ed3d944056b3312ad490676` | To verify: the file's own header and the MAME licence page (the pinned MAME 0.289 distribution is GPL version 2 with per-file exceptions) |
| PS2 rows in `serial-index.json` | PCSX2 `bin/resources/GameIndex.yaml` at `904bd9da01c68bb31ae86f4ea6e0e9a1fb0743e2` (bulk) and `aa7ab4306e269075784c7ac3eb4b45e6e6c53445` (a few rows) | To verify: PCSX2 is GPL-3.0-or-later as a project; confirm the data file carries no separate terms |
| Wii and GameCube rows in `wii-disc-index.json` | GameTDB English title list, version `20250826093514` (the copy distributed with Dolphin), recorded per row as `sourceVersion` and `sourceId` | To verify: GameTDB's terms of use for the title lists. No answer is recorded yet |
| Dreamcast rows in `dreamcast-disc-index.json` | Flycast `core/cheats.cpp`, `core/emulator.cpp` and `core/network/picoppp.cpp` cited at `blob/master`: **not pinned**. The repo's own Flycast pin, `setups/flycast-hotd2-vr/upstream-pin.json` (`f0c7a2fd864b6134cecac8f63dcf9b401d960ef5`), is the hotd2-vr fork and is not the revision these rows were read from | To verify: Flycast is GPL-2.0-or-later per `setups/flycast-hotd2-vr/README.md`; the exact licence of each cited file. Re-pin the rows to a commit before release |
| A few single rows (PS1 and PS2) | Redump disc pages, PsxPalDB, PSX Data Center, the PCSX2 wiki, one retail page | To verify: each site's terms. Facts only (an ID next to a title); replace with a pinned source where one exists |
| A few Wii, GameCube and Dreamcast rows | The project's earlier Hub tables (`hub/src/core/wii/Wii.cpp` and `hub/src/core/dreamcast/Dreamcast.cpp`, commit `ee04fbd`) and Dolphin wiki pages | Project-owned tables; Dolphin wiki terms to verify |

Refresh: only the PS2 rows have a generator (`tools/build_serial_index.py`, which takes a local `GameIndex.yaml` and the commit it came from, and touches no other row). Everything else is edited by hand. Raise any `master` or unpinned citation, or an unverified licence, before shipping a build that includes these files.
