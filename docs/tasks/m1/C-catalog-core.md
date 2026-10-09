# M1 lane C: catalog core, layered config, models

**Branch:** `m1/c-catalog`. **Depends on:** A. **Read:** [game-schema.md](../../game-schema.md), [game-packages.md](../../game-packages.md), [config-spec.md](../../config-spec.md) (§1, §2, §8 layering), [ui/05-models.md](../../ui/05-models.md), [hub-architecture.md](../../hub-architecture.md) §4-§5, `data/vocab/*.toml`.

## Build (in `hub/src/core/` and `hub/src/models/`)

1. **Catalog loader:** reads `games/*/game.toml`, `install.toml`, `setup/*.toml`, `data/vocab/*.toml`, `data/emulators/*.toml`. Layering: built-in → `packs/*` (by priority) → `user/overrides/` with the merge rules in config-spec §8 (tables merge, arrays of tables merge by `id`, `"!delete"` removes). Unknown keys are kept and exposed.
2. **Validation (warn mode):** same checks as `tools/validate_catalog.py` (vocab membership, required fields, id rules). Warnings are shown in a dev panel, never blocking.
3. **`GameRecord`** + **`GameListModel`** with every role in ui/05 §3 (derive hardware family/kind/label from vocab; badges derived as ui/05 says).
4. **`FilterSortModel`:** facets genre, graphics, manufacturer (multi), year range + decade chips, hardware tree (kind → family → board), VR quality, players, controls type, in-library, state; search (title, alt titles, developer, with the `-term`/`+term` grammar from ui/01); sorts title / year / manufacturer / hardware / recently played. Filters fail open on unknown values. Reorder and hide only, no delegate rebuilds.
5. **State resolution:** game states from [game-packages.md](../../game-packages.md) §2 (Needs your files / Needs an emulator / Ready to install / Installed / Update available / Coming soon), plus **No setup yet** for games without `install.toml`. Inputs come from lane F (media found) and lane E (installed state) through interfaces; stub them here.
6. **`hubtool explain <game-id>`:** prints the merged record with the file each value came from.

## Tests

Synthetic fixtures (fake games, no real titles needed) covering layering, `!delete`, array merge by id, every filter facet, search grammar and sort stability. One test loads the real `games/` folder and asserts 413 records with 0 errors.

## Acceptance

`hubtool explain timecris` shows the merged record; the desktop window lists all games with working filters (minimal QML list is enough; lane D builds the real UI); tests pass in CI.
