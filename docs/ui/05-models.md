# 05. Models: the C++ to QML contract

Status: design draft, 2026-10-08. Companion to [04-qml-components.md](04-qml-components.md) (which views read these roles) and [theme.toml](theme.toml) (tokens). Specs 01, 02 and 03 are the behaviour source; this file says where each value comes from and who owns it. Data facts in section 11 come from a scan of all 413 `games/*/game.toml` and 5 `games/*/install.toml` on 2026-10-08.

Conventions: repo-relative paths. "Hub" means PCVR Mods Installer Hub (commit `a64401f`, MIT); Hub references are relative to `_refs/PCVR-Mods-Installer-Hub/` (local, gitignored). Tags as in `theme.toml`: **[proposal]**, **[open]**, **[conflict Cn]** (IDs in sections 12 and 04 section 10).

---

## 0. Rules

1. **Model first.** One `GameRecord` per game, built once at load. Views render roles. No per-title strings, colours or tiers in code (Hub lesson, 03 section 3 and 01 section 1.21).
2. **Single writer.** The GUI thread owns `GameRepository` and every `QAbstractItemModel`. Workers send plain value types through queued signals, never `QObject` pointers or model indexes.
3. **Reorder and filter never rebuild.** Sort and filter are proxies over one source model. Source rows, and therefore delegates, persist (01 section 1.11, 02 section 1.12).
4. **Fail open.** A bad record stays visible (`loadWarning` or `loadError`). A bad search term never hides a game (01 section 2.12, rule 1).
5. **Deterministic order.** Every sort ends with title, then `gameId`.
6. **Partial updates.** A change emits `dataChanged` with the changed roles only.
7. **Batching.** Scan and install results are applied in batches, at most one batch every 80 ms (01 section 1.12, "yield").

---

## 1. Object graph

```
games/*/game.toml ─┐                                 ┌─► GameFilterModel (facets + search + sort) ─► GameGrid, SectionedGrid
games/*/install.toml┼─► CatalogLoader (worker) ─┐     │
data/vocab/*.toml ──┘                           ▼     │
                                  GameRepository (GUI, single writer) ─► GameListModel (source, roles in section 5)
                                       ▲   │   │                             ├─► ExploreBucketModel x 8 ─► ExploreRow
                                       │   │   └─► FeaturedPicker ─────────────┴─► FeaturedBanner
ScanService (worker) ── batches ──────┤   │
InstallService (job) ── state ────────┤   └─► DetailModel (one per open game) ─► DetailPage sections
UpdateChecker (worker) ── versions ───┘        │   ├─► VariantModel, NeedsModel, SimilarModel, ReadmeSectionModel
HeadsetProbe ── adb state ──────────────────────┘   └─► InstallConsoleModel (InstallService events)
SettingsStore (hub-state.json) ◄──► every view setting; RecentModel reads recentlyPlayed
ArtProvider (image provider "art") ◄── GameRecord ids; resolves files and generates fallbacks
```

Ownership: `VocabRegistry` (read only), `GameRepository` (records), `SettingsStore` (settings), `InstallService` (jobs and manifests), `ScanService` and `UpdateChecker` (background, results only through the repository).

---

## 2. Load and validation

`CatalogLoader` runs on a worker. For each `games/<id>/`:

| Step | Rule | On failure |
|---|---|---|
| Parse `game.toml` | TOML 1.0 | `loadError` = parse message. The record still exists with `state = DataError`, title = folder name. |
| Check `id` | Equals the folder name | `loadWarning`. Folder name wins for lookups. |
| Vocab ids | `genre`, `subgenre`, `manufacturer`, `hardware`, `graphics`, `controls.type` exist in `data/vocab/` | `loadWarning`. Show the raw id with an "Unknown (id)" label. |
| Required fields | `title`, `genre`, `year`, `manufacturer`, `hardware`, `graphics`, `players`, `[hub]` | Missing field: `loadWarning`, neutral default. |
| Parse `install.toml` | Optional in v1 (408 of 413 games have none, section 11) | No file: `hasRecipe = false`. |
| Duplicate ids | Across folders | Second one gets `loadError`. |

Loading produces `QVector<GameRecord>` and a `CatalogReport` (counts per warning type) for the Help panel's "Logs" item.

---

## 3. GameRecord (C++ value type)

| Field | Type | Source | Notes |
|---|---|---|---|
| `id` | string | folder name / `id` | key everywhere, including settings and art paths |
| `title`, `altTitles`, `series`, `developer` | string, list, string, string | `title`, `alt_titles`, `series`, `developer` | `developer` may be empty (17 games) |
| `genre` | string | `genre` | `gun` or `racing` |
| `subgenres` | list | `subgenre` | may be empty (1 game) |
| `year` | int | `year` | 1981 to 2025 in data |
| `manufacturer` | string | `manufacturer` | vocab id |
| `hardware` | string | `hardware` | vocab id |
| `graphics` | string | `graphics` | `polygon-3d`, `mixed`, `sprite-scaler`, `2d` |
| `players` | int | `players` | 1 to 4 in data |
| `controlsType` | string | `controls.type` | empty for 2 games; treated as `other` |
| `controlsPedals`, `controlsExtras`, `guns`, `shifter` | list / int / string | `controls.*` | used by the controls table (03 section 2.3 #13) |
| `vrBest` | string | `routes.vr.best` | `true3d`, `theatre`, `none` |
| `vrPlanned` | bool | `routes.vr.planned` | true when the value is `true3d-planned` (21 games). See C-planned |
| `vrVia` | string | `routes.vr.via` | optional label (2 games) |
| `original` | string | `original` | port link (68 games). Not shown in specs 01 to 03 yet |
| `badgesRaw` | list | `hub.badges` | free tags only; see section 4.3 and C-badges |
| `pillRaw`, `colourRaw`, `accentRaw`, `blurb` | string | `[hub]` | all 413 present and valid |
| `mediaKinds` | list of (kind, set, serial, note) | `[[media]]` | kinds in data: `mame-romset`, `bios`, `disc`, `other`, `pc-game` |
| `featured` | bool | `hub.featured` | **not in the schema**; default true. C-featured |
| `seriesOrder` | int | none | **not in the schema**. C-series |
| `addedOn` | date | none | **not in the schema**; needed for "new". C-new |
| `video`, `quip`, `notice`, `supportUrl`, `infoUrl`, `steamAppId` | string | none | **not in the schema or data**; listed in 03 as proposals. C-video, C-steamid |
| `metaChecked` | date | `meta.checked` | not a UI field |
| `variants` | list of Variant | `install.toml` `[variant.<id>]` | empty if no recipe |
| `loadWarning`, `loadError` | string | section 2 | |

`Variant` (from `install.toml`): `id`, `title`, `quality` (`true3d` | `theatre`), `status` (`planned` | `wip` | `stable`), `needs.media[]`, `needs.tools[]`, `needs.headset` (proposal), `installedWhen` (path template), `steps[]` (kind and params), `launch`, `upstream`, `license`, `controls`, `uninstall`.

Runtime fields that are not in any file: `installed` (per variant, from the manifest), `mediaFound[]`, `toolsOk[]`, `updateAvailable`, `updateVersion`, `lastPlayed`, `firstSeen`, `hiddenByUser` (from `hiddenPublishers` [open]).

---

## 4. Derived values

### 4.1 Labels and colours

| Value | Rule | Source |
|---|---|---|
| `genreLabel` | `genres.toml` label: `gun` to "Light gun", `racing` to "Racing" | `data/vocab/genres.toml` |
| `subgenreLabels` | labels for each subgenre id | `data/vocab/genres.toml` |
| `manufacturerLabel` | `manufacturers.toml` label | `data/vocab/manufacturers.toml` |
| `manufacturerShort` | **[proposal]** short label for the meta line. Not in the vocab yet (0 of 31) | 02 section 6.4 |
| `hardwareLabel`, `hardwareFamily`, `hardwareKind`, `hardwareOrder` | from `hardware.toml`; `hardwareOrder` is the file index | `data/vocab/hardware.toml` |
| `graphicsLabel` | from `graphics.toml` | `data/vocab/graphics.toml` |
| `pill` | `hub.pill` uppercased, at most 12 characters. Fallback: `GUN` or `RACING` | 02 section 2.2 |
| `colourBase` | `hub.colour` if valid, else `theme color.surface.card` | 02 section 2.2 (base colour is an owner call, 04 section 10) |
| `accent` | `hub.accent` if valid, else the genre colour (`color.genre.gun` or `racing`) | 02 section 2.2; 01 section 2.11 |
| `neonAccent` | `ColorMath::neonLift(accent)`: scale each channel by `140 / lum`, clamp to 255, one pass. Cached | Helpers.ps1:1513-1527 |
| `bucketMask` | bit i set when any subgenre belongs to Explore bucket i (section 9.4) | 01 section 2.5 |

### 4.2 Playing modes and VR

| Value | Rule |
|---|---|
| `playersBucket` | `"1"` if players is 1, else `"2+"` (Filters drawer and sort) |
| `vrBadge` (enum `VrBadge`: `None`, `True3D`, `Theatre`, `Planned`) | `True3D` or `Theatre` from `vrBest`. `Planned` only when `vrBest` is `none` and `vrPlanned` is true (02 section 2.2). In the current data that case never occurs: all 21 planned games are also `theatre`. See C-planned |
| `vrPlannedMark` | `vrPlanned` shown as a separate "True 3D planned" mark in the detail page |
| `featuredEligible` | `vrBest` is `true3d` or `theatre`, and `featured` is not false |

### 4.3 Badges (display list, fixed order)

The card and detail page show badges in this order (03 section 2.3 #2): GUN, WHEEL, HANDLEBARS, BIKE, ROOMSCALE, SEATED, 2 PLAYERS, WIP, QUEST STANDALONE.

| Badge | Derived from |
|---|---|
| GUN, WHEEL, HANDLEBARS, BIKE | `controlsType` is `gun`, `wheel`, `handlebars`, `bike` |
| ROOMSCALE | `badgesRaw` contains `roomscale` (68 games) |
| SEATED | `badgesRaw` contains `seated` (165 games) |
| 2 PLAYERS | `players >= 2`. **Not** from `badgesRaw` `2-players` (C-badges) |
| WIP | any variant `status = wip`, or `badgesRaw` contains `wip` (33 games). Until C-wip is decided, both count |
| QUEST STANDALONE | `badgesRaw` contains `quest-standalone` (1 game), or a variant needs a headset |

Ignored tags: `2-players` and `3-players` (C-badges). Their meaning is covered by `players`.

---

## 5. GameListModel: roles

`GameListModel` is a `QAbstractListModel` over `GameRepository` records. QML names are camelCase. Every role is read-only in QML.

### 5.1 Identity and text

| Role | Type | Source | Changes when |
|---|---|---|---|
| `gameId` | string | `id` | never |
| `title` | string | `title` | never |
| `altTitles` | string list | `alt_titles` | never (search only) |
| `series` | string | `series` | never |
| `developer` | string | `developer` | never |
| `developerShown` | bool | developer set and differs from `manufacturerLabel` | never (02 section 2.2) |
| `blurb` | string | `hub.blurb` | never |
| `pill` | string | section 4.1 | never |
| `colourBase`, `accent`, `neonAccent` | color | section 4.1 | never |
| `loadWarning`, `loadError` | string | section 2 | never |

### 5.2 Classification

| Role | Type | Notes |
|---|---|---|
| `genreId`, `genreLabel`, `genreColour` | string, string, color | colour from `color.genre` |
| `subgenreIds`, `subgenreLabels` | string list | chip and card text |
| `bucketMask` | int | Explore membership, one bit per bucket |
| `year`, `decade` | int | `decade` is the year rounded down to the decade (chip filter) |
| `manufacturerId`, `manufacturerLabel`, `manufacturerShort` | string | `manufacturerShort` falls back to the label |
| `hardwareId`, `hardwareLabel`, `hardwareFamily`, `hardwareKind`, `hardwareOrder` | string, string, string, string, int | tree filter and sort |
| `graphicsId`, `graphicsLabel` | string | Graphics facet (04 section 4.3) |
| `players`, `playersBucket` | int, string | |
| `controlsType`, `controlsLabel` | string | `other` when missing |
| `vrBest` (string), `vrBadge` (enum `VrBadge`), `vrPlanned` (bool) | string, enum, bool | section 4.2 |
| `badges` | string list | section 4.3 display order |
| `featuredEligible` | bool | section 9.5 |
| `originalId`, `hasRecipe`, `variantCount`, `quest`, `roomscale`, `seated` | string, bool, int, bool, bool, bool | |

### 5.3 Install state

| Role | Type | Changes when |
|---|---|---|
| `state` (enum `GameState`) | display state, section 6 | scan result, install job, update check, variant change |
| `baseState` (enum `GameState`) | state without job overlays; the STATE filter reads this | same |
| `stateLabel` | string | `state` (table in section 6.2) |
| `stateColour` | color | `state` (token names in section 6.2) |
| `statePill` | string: `toInstall`, `ready`, `updates`, `needsFiles`, or empty | `baseState` (section 6.3) |
| `inLibrary` | bool: at least one required media or PC game found | scan |
| `mediaStatus` (enum `MediaStatus`) | `missing`, `partial`, `found` (for the best variant) | scan |
| `toolStatus` (enum `ToolStatus`) | `ok`, `missing`, `older` | scan |
| `installedVariantId` | string | install job, manifest read at start |
| `updateAvailable`, `updateVersion` | bool, string | update check |
| `isWip` | bool | section 4.3 |
| `reinstallVisible` | bool | `state` is `Installed` or `Update` (02 section 2.3) |
| `jobStatus` (enum `JobStatus`), `jobProgress` | `none`, `running`, `failed`; string such as "3 of 5" | install job |
| `lastPlayed` | qint64 (epoch seconds, 0 = never) | launch |
| `recentIndex` | int (0 to 7, or -1) | launch |
| `firstSeen` | qint64 (scan date) | first scan that finds the game |

### 5.4 Art and search

| Role | Type | Notes |
|---|---|---|
| `artBanner` | url `image://art/<id>/banner` | resolved by `ArtProvider` (section 8) |
| `artTile` | url `image://art/<id>/tile` | for similar-game rows (C-art) |
| `artPortrait` | url `image://art/<id>/portrait` | detail and library tiles |
| `artSource` | enum `ArtSource`: `user`, `pack`, `generated` | sidecar read on demand |
| `searchIndex` | map | normalised (lower case, folded) strings per field: `title`, `alt`, `series`, `developer`, `manufacturer`, `hardware`, `subgenre`, `blurb`, `badges`, `vr`, `genre`, `id` |

Filtering uses the typed roles above. `searchIndex` is only used for text.

---

## 6. Game state

### 6.1 Display states (`GameState`)

| State | Meaning | Source |
|---|---|---|
| `DataError` | `loadError` set; card shows the title and "Data error" | section 2 [proposal] |
| `Installing` | an install job is running for this game | job overlay |
| `InstallFailed` | the last install job failed and was not retried | job overlay (03 section 2.6 "Last install failed") |
| `Update` | an installed variant has a newer upstream version | game-packages section 2 |
| `Installed` | a variant has passed its verify step and `installed_when` exists | game-packages section 2 |
| `NeedsFiles` | a required media is missing (for the chosen variant) | game-packages section 2 |
| `NeedsEmulator` | a required tool is missing | game-packages section 2 |
| `ReadyToInstall` | everything found, not installed | game-packages section 2 |
| `PlannedOnly` | every variant is `planned` and none is installed ("Coming soon") | game-packages section 2 |
| `NoRecipe` | no `install.toml` [proposal] (408 games) | C-recipes |

`Installing` and `InstallFailed` are overlays. `baseState` drops them, so the STATE filter still finds the game.

### 6.2 Labels, colours, and where each comes from

| `state` | `stateLabel` (card) | Neon token (card) | Detail primary button |
|---|---|---|---|
| `DataError` | "Data error" [proposal] | `color.state.planned_neon` | none; detail shows the error |
| `Installing` | "Installing 3 of 5" | `color.state.installed_neon` | disabled "Installing 3 of 5" (03 section 2.6) |
| `InstallFailed` | "Retry install" | `color.state.retry_line` | "Retry install" (03 section 2.6) |
| `Update` | "↓ Update" | `color.state.update_neon` | "Update to <version>" |
| `Installed` | "Ready" (rest), "Start in VR ▶" (hover) [conflict C-ready] | `color.state.ready_neon` | "Start in VR ▶" |
| `NeedsFiles` | "Find my files" | `color.state.needs_neon` | "Find my files" |
| `NeedsEmulator` | "Install emulator" | `color.state.needs_emulator` | "Install emulator" |
| `ReadyToInstall` | "Install" | `color.genre.<genre>` (accent) | "Install" |
| `PlannedOnly` | "Coming soon" | `color.state.planned_neon` | disabled "Coming soon", opens the roadmap item |
| `NoRecipe` | "No setup yet" [proposal] | `color.state.planned_neon` | none; detail says what is missing |

### 6.3 STATE filter buckets (spec 01 section 2.2, corrected names)

| Pill | Matches `baseState` in | Note |
|---|---|---|
| To install | `ReadyToInstall` | 01 section 2.2 writes `status = installed` here, which reads as the opposite. Taken as "ready to install". C-states |
| Ready | `Installed`, `Update` | 01 section 2.2 `{ready, update}` |
| Updates | `Update` | |
| Needs files | `NeedsFiles` | optional fourth pill, 01 section 4 #4 |

`NeedsEmulator`, `PlannedOnly` and `NoRecipe` have no pill. They show only under "All" (C-states).

### 6.4 Resolution (pure function)

Inputs per game: `hasRecipe`, variants with `status`, `installedWhen` result, needs media and tools, `updateAvailable`, `loadError`, and the job map.

```
resolve(game, jobs):
  if game.loadError:                          return DataError
  if jobs[game] == running:                   return Installing          # display only
  if jobs[game] == failed and no later job:   return InstallFailed       # display only
  base = resolveBase(game)
  return base

resolveBase(game):
  if not game.hasRecipe:                      return NoRecipe
  installed = variants where verified(v)
  if installed and any(v.updateAvailable):    return Update
  if installed:                               return Installed
  playable = variants where v.status != planned
  if playable is empty:                       return PlannedOnly
  v = best(playable)                          # prefer stable, then wip
  if v.needs.media not all found:             return NeedsFiles
  if v.needs.tools not all ok:                return NeedsEmulator
  return ReadyToInstall
```

Precedence matches 02 section 2.3 and 03 section 2.6: planned (when nothing is installed), then update, installed, needs files, needs emulator, ready to install. A game with one installed variant and one planned variant is `Installed` (timecris: `dr89-pcvr` stable, `namco22-vr` planned).

`verified(v)` requires the verify step to pass, `installedWhen` to exist, and the ownership manifest to exist (03 section 2.8, 2.12). Presence alone is not enough (03 section 1.21 #3).

The best variant for the card is the installed one; otherwise the first non-planned variant; otherwise the first planned one. The detail page can switch variants (03 section 2.4); the card does not.

### 6.5 Open items on state

- Media that is found but not verified (hash not checked yet): does it count as found for `NeedsFiles`? 03 section 2.5 shows "Found, not verified" but does not say whether install is allowed. [open]
- A game with `NoRecipe` but found media: it has no install path. Its card says "No setup yet". [proposal]

---

## 7. Filter and sort

### 7.1 FilterState

`FilterState` is a `QObject` with one property per facet and one `changed()` signal. `GameFilterModel` (a `QSortFilterProxyModel` subclass) connects to it and calls `invalidateFilter()`.

| Facet | Property | Semantics | Source |
|---|---|---|---|
| Genre (row 1) | `genre`: `all`, `gun`, `racing` | single select; `all` is inactive | 01 section 2.2 |
| STATE (row 2) | `statePills`: set of `toInstall`, `ready`, `updates`, `needsFiles` | OR within; inactive until the first scan ("show everything, not nothing") | 01 section 1.5 and 2.12 rule 2 |
| In my library | `inLibraryOnly`: bool | on: `inLibrary` true; disabled until a scan | 01 section 2.2 |
| Hardware | `hardwareIds`: set | OR. Selecting a node selects every descendant id | 01 section 2.2; schema section 3 |
| Manufacturer | `manufacturerIds`: set | OR | |
| Year | `yearMin`, `yearMax` (inclusive; null = open) | decade chips set the range; moving the slider clears the chips [proposal] | 01 section 2.2 |
| Graphics | `graphicsIds`: set | OR | schema section 3 (C-graphics-drawer) |
| VR | `vrKeys`: set of `true3d`, `theatre`, `planned` | OR. `planned` matches `vrPlanned`, not `vrBest` (C-planned) | 01 section 2.2 |
| Players | `playersBuckets`: set of `1`, `2+` | OR | |
| Controls | `controlsTypes`: set | OR | |
| Search | `query`: SearchQuery | section 7.2 | 01 section 2.6 |

Combination: AND across facets, OR within a facet. An inactive facet does not filter. Fail open: a record with `loadError` always passes, and so does any record whose predicate cannot be evaluated.

### 7.2 Search grammar

| Input | Meaning |
|---|---|
| `word` | text match (AND with other words). Fields: `title`, `altTitles`, `series`, `developer`, `manufacturerLabel`, `hardwareLabel`, `subgenreLabels`, `blurb`, `badges` |
| `-word` | exclude. Matches manufacturer id or label, genre and subgenre labels, and VR keyword. **Never the title** (01 section 2.6) |
| `+word` | re-include: lifts a `hiddenPublishers` entry for this query only [open] |
| `gun`, `racing` | genre keyword (same as the genre pills) |
| `true3d`, `theatre`, `planned` | VR keyword. `-theatre` excludes `vrBest` theatre |
| `new` | `addedOn` or `firstSeen` within 10.5 days (C-new) |
| `roomscale` | `roomscale` flag |

Matching is case-insensitive. Diacritics folding is [proposal]. There is no debounce: each keystroke applies (01 section 1.10). A term that fails to parse is treated as plain text.

### 7.3 Sort keys

| Mode (key) | Pill label | Ordering (ties go to title, then id) | Notes |
|---|---|---|---|
| `title` (default) | Title | `title` ascending; within a series, `seriesOrder` [open] | C-series |
| `year` | Year | `year` ascending | |
| `manufacturer` | Manufacturer | `manufacturerLabel`, then `year` | |
| `hardware` | Hardware | `hardwareKind` (arcade, console, pc), then `hardwareOrder`, then `year` | vocab file order |
| `recent` | Recently played | `lastPlayed` descending; never-launched last, by title | from `RecentModel` |
| `added` [proposal] | Added to library | `firstSeen` descending | the Hub's sixth mode; 01 section 2.7 offers it |

The sort reorders existing rows. Changing the mode calls `sort()` on the proxy. Source rows and delegates are untouched.

### 7.4 Explore

`ExploreBucketModel` is one proxy per bucket. Membership comes from `bucketMask` (4.1). The VR toggle (All, True 3D, Theatre, Planned) and the optional New chip are applied as extra predicates. A game can sit in several buckets (01 section 2.5).

| Bucket | Subgenres |
|---|---|
| 0 Rail shooters | `rail-shooter` |
| 1 Cover and sniper | `cover-shooter`, `sniper` |
| 2 Horror | `horror` |
| 3 Hunting, party and water | `hunting`, `machine-gun`, `party`, `water-gun` |
| 4 Circuit and street | `circuit`, `street` |
| 5 Rally and off-road | `rally`, `truck` (C-subgenre) |
| 6 Kart and futuristic | `kart`, `futuristic`, `vehicle-combat` |
| 7 Bikes, boats and more | `motorcycle`, `bicycle`, `boat`, `ski`, `mission-driving`, `flying` |

Not in any bucket: `horse` (2 games). Horse racing needs a home (C-subgenre).

### 7.5 Grid rows for the sectioned list [proposal]

`GameGrid` (SectionedGrid) is a `ListView` over `GridRowModel`, a derived model with one row per screen line:

| Role | Type | Meaning |
|---|---|---|
| `rowKind` | enum `header`, `cards` | a section header or a line of cards |
| `sectionTitle`, `sectionKind`, `sectionCount`, `sectionColour` | string, string, int, color | header content (SectionHeader) |
| `cards` | list of `QPersistentModelIndex` into the filter proxy, max N | the cards on this line; N = columns |
| `columns` | int | computed from the viewport width and card size |

Why: Qt 6 `GridView` does not provide section headers [verify], and a single grid of 250 or more cards with the card's effects costs too much to instantiate in full. Rows are virtualised by the `ListView`.

Rebuild rules: `GridRowModel` is rebuilt (its own reset, not the source) when the filter or sort changes, or when `columns` changes. A change to a record's roles does not rebuild it: each card holds a persistent index, so `dataChanged` reaches it directly.

Sections follow the genre order of `GameFilterModel` (Light guns, then Racing). A section with no visible cards is not emitted.

---

## 8. Art

`ArtProvider` is a `QQuickImageProvider` for `image://art/<id>/<kind>`. It returns a cached `QImage`; cache keys are `id`, `kind`, source path, and modification time. The cache lives in `user/cache/art/`.

Resolution, per kind:

| Kind | Chain | Source |
|---|---|---|
| banner | `user/art/<id>/banner.png` → `games/<id>/art/banner.png` → `user/art/<id>/marquee.png` or `games/<id>/art/marquee.png` (centre-cropped to 2.14:1) → generated | game-packages section 1 (user art first), 02 section 3.2 |
| tile | `…/tile.png` → banner crop to 160 x 75 → generated | 03 section 2.13 (C-art) |
| portrait | `…/portrait.png` → generated | 02 section 3.3 |

Rules:
- Art is never shipped in the repository for third-party games (`docs/legal.md`). Pack art may be in `games/<id>/art/`, and scraped art lives only in `user/art/<id>/`.
- Each non-generated file has a `*.source.toml` sidecar. `artSource` reads it; a missing sidecar means `pack` for `games/`, `user` for `user/`.
- Generated art uses `title`, `manufacturerLabel`, `year`, `colourBase` and `accent`. It is never blank, so every card gets the hover preview (02 section 2.4).
- Loading is asynchronous. The card shows the generated image or the colour wash until the real image is ready. Failed loads retry in the background; the wash stays if nothing loads (01 section 1.6).

---

## 9. Derived colour and layout maths

Pure C++ functions in `ColorMath`, unit-testable, no GUI types. Parameters are in `docs/ui/theme.toml`.

### 9.1 Card tint and border (02 section 1.5)

```
lum    = (0.299 R + 0.587 G + 0.114 B) / 255          # of the accent, 0-1
a      = (formula.tint.top_alpha + mid_alpha) / 2     # 0.06
if a < 0.09 and lum < 0.45:  a = a + (0.45 - lum) * 0.30    # lifts the alpha of dark accents, not the colour
tint   = accent * a + colourBase * (1 - a)            # per channel, rounded
border = accent * 0.22 + colourBase * 0.78
```

Source: `New-CardTintBrush` in `_refs/PCVR-Mods-Installer-Hub/Core/Modules/Helpers.ps1:2587-2630` (Frosted branch). The lift is on alpha `a`, which is easy to misread as a colour lift. The constants match 02 section 1.5.

### 9.2 Neon (02 section 1.5)

```
neonAccent = clamp255(channel * 140 / lum) for each channel, if lum < 140 (else unchanged)
neon       = neonAccent * 0.90
glowBlur   = 14 * scale
glowOpacity = max(0.42, 0.72 - 0.22 * lum01)     # lum01 = lum / 255
```

Caveat: the Hub's single-pass scale can stay below 140 when a channel clips at 255. The Hub does not iterate (Helpers.ps1:1513-1527). Keep the same behaviour and note it as a known limit. 137 of 413 accents are below 140 (section 11), so the lift matters for a third of the games.

### 9.3 Banner wash (01 section 1.6)

Computed from `accent`, not stored. Source: `_refs/PCVR-Mods-Installer-Hub/Core/Modules/OverviewPage.ps1:636-694`.

1. Convert accent to HSV. Read hue `h0`, saturation `s0`, value.
2. For each stop, pick a target value V (28, 28, 78, 115 at positions 0.00, 0.30, 0.66, 1.00).
3. If `h0` is between 35 and 78, pull hue toward 28 by `pull` (0.65, 0.65, 0.50, 0.20 for the four stops).
4. Saturation is `min(s0, 0.85)`. Convert back to RGB.
5. Build a horizontal `QLinearGradient` with those four stops.

Keep the stop values in `theme.toml` `[gradient.wash]`.

### 9.4 Featured picker (01 section 2.3)

```
pool = games where featuredEligible and gameId != current and not hiddenByUser
if any(pool.inLibrary): pool = those
half = genre gun or racing, chosen 50/50 [open: 01 section 4 #6]
pick random from pool restricted to half (if empty, use the other half)
never repeat the last 5 picks [proposal]
```

Explore Shuffle picks from the active bucket with the same rules. The Explore banner does not rotate.

### 9.5 Layout arithmetic

- Grid columns: `floor((viewportWidth - 2*padding + gap) / (cardWidth + gap))`. Columns centre with `leftMargin = (viewportWidth - columns*cellWidth + gap) / 2`.
- Card width at scale S/M/L: `175 * {1.00, 1.15, 1.30}` (base 175 x 160). Set width and height from the token, not with `scale`. A transform does not reflow (02 section 4.1).
- Similar rows shown: `clamp((gameInfoHeight - 26) / 93, 4, 12)` (03 section 1.8).

---

## 10. Update flows

Every row names the owner, the trigger, the path, and the timing.

### 10.1 Startup

| Step | Owner | Action |
|---|---|---|
| 1 | `CatalogLoader` (worker) | parse all folders; build `GameRecord`s; build `CatalogReport` |
| 2 | `GameRepository` (GUI) | `beginResetModel()` once; store records; `endResetModel()`. Emit `catalogReady` |
| 3 | `SettingsStore` | load `hub-state.json` (recovery copy if the primary is unreadable) |
| 4 | `RecentModel` | resolve `recentlyPlayed` ids to records (max 8) |
| 5 | UI | ready signal after the first frame (01 section 1.2, step 4) |
| 6 | `UpdateChecker`, `ScanService` (if "scan on startup") | start after first paint, at low priority (01 section 1.2) |

### 10.2 Scan

- `ScanService` runs on a worker. It walks Steam, GOG and Epic roots and user folders, then checks each game's required media (hash for ROM and disc sets; folder markers and exe for PC games).
- It emits `progress(done, total)` and `batch(QVector<GameScanResult>)` at most every 80 ms.
- `GameRepository::applyScanBatch` writes `mediaStatus`, `toolStatus`, `inLibrary`, `firstSeen` and re-resolves `baseState`, then `dataChanged` on the changed roles only.
- Heartbeat: the worker updates a timestamp each batch. A silent scan older than 60 s is treated as dead (01 section 1.12). A second scan is refused while one runs.
- Cancel: `ScanService::cancel()` sets a token. Closing the window during a scan prompts first (04 section 10: C-close-scan).
- `SettingsStore` batches its writes during a scan (01 section 1.13).
- When the scan ends, the STATE pills reveal after 520 ms (01 section 1.5).

### 10.3 Install

- `InstallService` runs one job at a time. The job reads the variant's `steps[]` and emits `InstallEvent{kind, text, stepIndex, stepCount, detail}`.
- `kind` is one of `step`, `ok`, `warn`, `fail`, `work`, `detail`, `prompt`, `skip`, `done` (03 section 2.8, plus `skip` for optional steps).
- Each event goes to two sinks: `InstallConsoleModel` (GUI, queued) and the log file `user/logs/<id>-<variant>-<timestamp>.log`. Same text in both (03 section 2.8).
- `prompt` carries an id. The UI answers with `respond(id, value)`. The job awaits the answer; it does not poll (03 section 3).
- On `done` with success: write the ownership manifest (`installed/<id>/<variant>/.ownership.csv`), set `installedVariantId`, re-resolve state.
- On `fail`: set `jobStatus = failed`, keep the failure (step index, message, log path) for `RecoveryPanel`, state becomes `InstallFailed`.
- Cancel is allowed between steps only. Cancelling runs the uninstall rules for what was written so far (03 section 2.12).

### 10.4 Update check

- `UpdateChecker` (worker) reads the TTL cache (6 h, `user/cache/`), calls the upstream source, and emits `result(id, version, available)`.
- Circuit breaker: after the first network failure in one pass, no more calls in that pass. A pass-wide deadline stops probing (03 section 1.17).
- Result updates `updateAvailable` and `updateVersion`, then re-resolves state.

### 10.5 Launch and recent

- On launch: `SettingsStore.recentlyPlayed` moves the id to the front and keeps 8. `RecentModel` changes. `lastPlayed` and `recentIndex` change on the affected records; if the sort is `recent`, the proxy re-sorts.

### 10.6 Headset

- `HeadsetProbe` runs `adb devices` only when the window gains focus and on "Check again" (03 section 2.5). It exposes `connected`, `unauthorized`, `notConnected` to the detail page. No background scanning.

### 10.7 Settings

- `SettingsStore` exposes one Q_PROPERTY per key (section 13) and a `changed(key)` signal. Writes are atomic (temporary file, then rename), and a recovery copy is kept. Booleans are JSON booleans only (01 section 1.13).

---

## 11. Data facts (scan of games/ on 2026-10-08)

| Fact | Value |
|---|---|
| Games with `game.toml` | 413 (all parse) |
| Vocab ids | all valid for `genre`, `subgenre`, `manufacturer`, `hardware`, `graphics` |
| Genre | racing 261, gun 152 |
| Graphics | polygon-3d 334, sprite-scaler 51, 2d 21, mixed 7 |
| Players | 1: 217, 2: 181, 3: 3, 4: 12 |
| Controls type | wheel 170, gun 150, other 41, handlebars 10, bike 15, joystick 8, boat 8, ski 5, yoke 4, missing 2 |
| `routes.vr.best` | theatre 222, none 189, true3d 2 |
| `routes.vr.planned` | present on 21 games, all `true3d-planned` (all also `best = theatre`) |
| Featured-eligible (true3d or theatre) | gun 79 (77 theatre, 2 true3d); racing 145 (theatre) |
| `hub.badges` values | seated 165, 2-players 175, roomscale 68, wip 33, 3-players 3, quest-standalone 1 |
| Games with 2+ players but no `2-players` badge | 21 |
| `install.toml` present | 5 games: `ps2-virtua-cop-elite-edition`, `raverace`, `scud`, `timecris`, `vcop2` |
| Variants | 9: status planned 6, stable 2, wip 1; quality true3d 7, theatre 2 |
| Step kinds in use | locate-package 2, extract 2, shortcut 2, copy-media 1, write-config 1, adb-install 1, run 1, github-release 1 (all in frontend.md section 3) |
| Media kinds | mame-romset 267, bios 134, disc 110, other 72, pc-game 9 |
| Optional fields present | developer 396 (17 missing), series 352, alt_titles 218, original 68, regions 15, contains 4 |
| Subgenre use | horse 2 (no Explore bucket), truck 6, water-gun 1, bicycle 1; one game has no subgenre (`overdriv`) |
| Year | 1981 to 2025 |
| `hub.colour` luminance | 11.6 to 25.7: all very dark, so the wash and tint read well |
| `hub.accent` luminance | 70.8 to 240.5, median 153; 137 below 140 (neon lift applies) |

Reproduction: a short Python script with `tomllib` over `games/*/game.toml` (kept outside the repository).

---

## 12. Data and model conflicts and gaps

Each item is a point where the specs, the schema, or the data disagree. None is resolved silently; the value used here is stated.

| ID | Conflict or gap | Used here |
|---|---|---|
| C-planned | `routes.vr.planned` (data) vs `routes.vr.best = true3d-planned` (01 section 2.2, 2.11) vs PLANNED badge from `routes.vr.best` (02 section 2.2). 21 games have both `best = theatre` and `planned`, so the card badge has to choose one | Filter reads `vrPlanned`. Card shows THEATRE (available now); PLANNED appears as a mark in the detail page. Owner to confirm |
| C-badges | `hub.badges` holds `2-players`, `3-players`, `wip`, `seated`, `roomscale`, `quest-standalone` (data). Schema lists only `roomscale` and `quest-standalone` as examples; 03 section 2.3 expects a fixed vocabulary; 02 section 2.2 derives 2P from `players` | Badges are derived (section 4.3). `2-players` and `3-players` ignored. Owner to decide whether to strip them from the data |
| C-wip | WIP: 33 games with `hub.badges` `wip`; 1 variant with `status = wip` (02 section 2.2 says WIP comes from the variant) | Both count (section 4.3). Owner to pick one source |
| C-recipes | 408 of 413 games have no `install.toml`. Game states (game-packages section 2) cannot be computed for them | `NoRecipe` state, label "No setup yet" [proposal] |
| C-variant-status | Variant status: `planned`, `wip`, `stable` (install.toml, game-packages section 4). 03 section 2.4 dots use `installed`, `ready`, `wip`, `planned` | Install.toml values. `installed` and `ready` are card and button states, not variant status |
| C-states | Three label sets: game-packages six states; 01 section 2.2 STATE pills ("To install", "Ready", "Updates") with an inverted filter rule; schema section 3 "Status: Installed / Ready / Needs files". The word "Ready" is the card label for an installed setup (02 section 2.3). The Hub's INSTALLED pill meant "game found, mod missing" (03 section 1.3); in our spec the detail page's INSTALLED pill means an installed setup (03 section 2.2, 2.6). The meaning of the word changes between the two | States from game-packages; pills per section 6.3; labels per section 6.2 |
| C-featured | `hub.featured` (01 section 2.11, NEW) is not in the schema and not in any game | Field added as optional, default true [proposal] |
| C-video | `video` is cited for the detail page (03 section 2.3 #12) as a game-packages section 3 field. It is in neither doc nor any game | No video role in v1 (section 3 lists it as a proposal) |
| C-steamid | `steam_app_id` in game.toml (02 section 3.2) vs `store.steam_appid` (03 section 2.7, 2.16 #8, OPEN) | Not used. Owner to pick one name |
| C-series | "Within a series, by series order" (01 section 2.7). No order field; 352 games have `series` | `seriesOrder` [open] |
| C-new | "Titles added in the last 10.5 days" (01 section 2.6, 2.8). No added date in the schema. `meta.checked` is a check date, not an add date | `addedOn` or scan `firstSeen` [open] |
| C-subgenre | `horse` (2 games) is in no Explore bucket (01 section 2.5). `truck` (6 games) sits under "Rally and off-road" | Horse racing needs a bucket; `truck` moves to "Rally and off-road" as the spec says. Owner call |
| C-controls | 2 games have no `controls.type` | `other`, label "Unknown" |
| C-art | Art chain: `games/<id>/art` first (01 section 2.3); banner, marquee, flyer (02 section 2.4); `user/art` first (game-packages section 1, 02 section 3.2). `tile.png` used for similar games (03 section 2.13) but "not needed" (02 section 3.3) | `user/art` first, then pack art, then generated (section 8). Tile optional |
| C-shortnames | `short` labels for manufacturers and hardware (02 section 6.4) are not in the vocab | `manufacturerShort` falls back to the label |
| C-vocab-list | 01 section 2.2 lists 25 manufacturers; the vocab has 31 | The vocab is the source |
| C-steps | `verify` (03 section 2.8) and `optional = true` (03 section 2.9) are not step kinds in frontend.md section 3 | Added to the step set here as `verify` and `skip` [proposal] |
| C-needs | `needs.headset` (03 section 2.5) and `installed_when` checks are not in game-packages section 4 | Added as proposal |
| C-hiddenPublishers | `hiddenPublishers` in settings (01 section 2.9) has no UI entry point in 01 section 2.6 (the Hub's search hint offers to hide a modder; no equivalent is specified) | Stored; no entry point [open] |
| C-scanfolders | `scanFolders` in `hub-state.json` (01 section 2.9) vs in the config (01 section 4 #13) | Config (01 recommendation) [open] |
| C-stale | Claims in 02 section 6.1-6.2 and 03 section 2.16 #1-3 (game-packages section 3 holds top-level colours; frontend.md section 1 lists the old five states; game-packages section 4 uses inline steps; `[media]` as a table) are out of date. The current docs already match the schema and game-packages | No action. Those claims should be removed from the specs |

---

## 13. Settings keys

| Key | Type | Default | Owner | Note |
|---|---|---|---|---|
| `winWidth`, `winHeight`, `winLeft`, `winTop`, `winMaximized` | int, bool | none | `App` | restore rules from 01 section 1.3 |
| `sizeLibrary`, `sizeExplore`, `sizeDetail` | string (S, M, L) | L, M, M | views | 01 section 1.14 |
| `sizeList` | double (1.0, 1.5, 2.0) | 1.0 | `GameGrid` | renamed from `scaleList` (01 section 2.9) |
| `gameSort` | string | `title` | `GameFilterModel` | renamed from `catalogSort` |
| `checkOnStartup` | bool | false | `ScanProgress` | |
| `shimmerDisabled` | bool | false | `ScanProgress` | also reachable in Settings (04 section 9) |
| `bannerListDisabled`, `bannerLibDisabled` | bool | false | `FeaturedBanner` | also reachable in Settings (04 section 9) |
| `hiddenPublishers` | string list | empty | `GameFilterModel` | C-hiddenPublishers |
| `recentlyPlayed` | string list (max 8) | empty | `RecentModel` | game ids |
| `recentlyPlayedHidden` | bool | false | `RecentModel` | |
| `genre`, `facets` | string, object | `all`, empty | `FilterState` | last choices (01 section 2.9) |
| `reduceMotion` | bool | false | all animated views | 02 section 5 #5 (owner: keep, with a setting) |
| `vrOverlayMode` | bool | false | `App` | 04 section 9 [proposal] |
| `startView` | string (`list`, `library`) | `list` | `App` | Hub key (01 section 1.13) |
| `firstSeen` | map id to date | empty | `GameRepository` | "added" sort and "new" [proposal] |
| `desktopShortcut` | bool | false | `SettingsPage` | |
| `scanFolders` | list | empty | config (C-scanfolders) | |

Booleans are JSON booleans. Unknown keys pass through unchanged (01 section 2.9; `docs/workshop.md`).

---

## 14. Open decisions for the owner (data side)

1. C-planned: badge choice for titles that are both theatre and planned.
2. C-badges and C-wip: strip the `hub.badges` tokens that duplicate fields?
3. C-recipes: state and label for games without an install recipe.
4. C-featured, C-series, C-new, C-steamid, C-video: add these fields to the schema, and under which names?
5. C-subgenre: where horse racing goes.
6. Media found but not verified: may install proceed? (section 6.5)
7. `scanFolders` location (C-scanfolders).
