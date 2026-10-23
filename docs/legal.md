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
