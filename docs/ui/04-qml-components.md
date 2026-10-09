# 04. QML component map: the Hub built from specs 01 to 03

Status: design draft, 2026-10-08. Turns [01](01-shell-and-filters.md), [02](02-game-cards.md) and [03](03-detail-and-install-flow.md) into Qt 6 / QML components. Models and roles: [05-models.md](05-models.md). Tokens: [theme.toml](theme.toml). Stack decision: D4 in [../workshop.md](../workshop.md) (Qt 6 / QML, decided 2026-10-08) and [../frontend.md](../frontend.md) section 6.

Conventions
- Paths are repo-relative. Hub references are relative to `_refs/PCVR-Mods-Installer-Hub/` (local, gitignored) and follow the spec files.
- Each component entry lists: spec sections it implements, properties, signals, model roles, states and timings, keyboard and laser behaviour.
- Tags: **[DIVERGE]** a deliberate change from the Hub; **[NEW]** no Hub equivalent; **[proposal]** not in 01 to 03; **[open]** owner decision pending; **[conflict Cn]** specs disagree (section 10).
- Sizes are logical px at 100 % scale. Token names refer to `theme.toml`.

---

## 1. Scope and decisions taken here

- Covers the v1 shell, the library (list and portrait grid), Explore, the detail page with the install flow, settings, scan, and the overlay menus.
- Out of scope for v1 (section 7): Classic card style, gems, add-on tags, PC Power, trailers, the random banner-effect pool, in-VR game list.
- Where the specs disagree, this file uses the value marked in section 10 and does not resolve the conflict silently.

---

## 2. Qt 6 / QML mapping

| Concern | Choice | Verify |
|---|---|---|
| Modules | `QtQuick`, `QtQuick.Controls` (Basic style, custom skin), `QtQuick.Effects` (`MultiEffect` for shadow, glow and masks), `QtQuick.Shapes` for the spinner and icons | `MultiEffect` needs Qt 6.5 or later. Confirm `ShapePath` dash offset for the spinner, or draw it in a small custom item |
| Qt version | Qt 6, minimum 6.5. The LTS line is an open choice | [open] |
| Single window and overlay | Desktop: one `ApplicationWindow`. SteamVR overlay: the same root rendered offscreen (`QQuickRenderControl`, [frontend.md](../frontend.md) section 6) at 1280 x 800 logical px | the overlay path in section 9 |
| Layout | Set card width and height from tokens. Do not use `scale` for S/M/L, because a transform does not reflow (02 section 4.1). `scale` is used only for the hover preview (1.4), which sits in an overlay host | |
| Gradient text | Text layer with a `MultiEffect` mask, not a drop shadow on the same item (02 section 1.13) | |
| Glow | Glow on a sibling or on the border item, never on text (02 section 1.13) | |
| Static background | The dot grid is a 24 px tile image, outside the scroll view, with `layer.enabled` (01 trap 4, section 3.3) | |
| Fonts | `Segoe UI` from the theme with fallbacks. Segoe UI is absent on Linux and SteamOS (D12): font choice is [open] | [open] |
| Accessibility | `Accessible.name` on every control; `Accessible.checked` or expanded on toggles (03 section 3) | |
| Threads | GUI thread owns models. Workers emit queued signals with value types only (05 section 0) | |
| Reduce motion | `reduceMotion` setting turns off shimmer, dot pulse, click pulse, sweep and attention pulses. Status animations (spinner) stay | 02 section 5 #5 |

Proposed QML layout (mirrors [architecture.md](../architecture.md) section 6, `hub/`): `hub/qml/App.qml`; `hub/qml/shell/` (Header, FilterBar); `hub/qml/grid/` (GameGrid, GameCard, GridRow); `hub/qml/banner/`; `hub/qml/detail/`; `hub/qml/install/` (InstallConsole, RecoveryPanel); `hub/qml/scan/`; `hub/qml/settings/`; `hub/qml/overlay/` (SortMenu, HelpPanel, Dialogs, Toasts); `hub/qml/common/` (primitives, section 4.19).

---

## 3. Component tree

```
App (ApplicationWindow | offscreen root)
├─ Background: DotGrid (static, outside scroll)
├─ Shell (Column)
│  ├─ Header ─ HeadsetGlyph, TitleGroup (Title, VersionPill, Tagline), UpdateBanner,
│  │           LibraryButton, HelpButton, OrderPill, SearchPill (+ SearchHint)
│  ├─ FilterBar
│  │   ├─ Row 1: BackArrow, GenreChips, FiltersButton (+ count), ScanProgress (ScanButton), SizeSwitch
│  │   └─ Row 2: StateChips, InLibraryToggle, ActiveFacetChips, ClearAll
│  └─ ContentArea (Flickable)
│     ├─ Library/List page
│     │   ├─ FeaturedBanner (ListBanner)
│     │   ├─ RecentlyPlayedRow (heading, RecentTiles)
│     │   └─ GameGrid (sectioned: SectionHeader + GridRow of GameCard)
│     ├─ LibraryView (portrait grid, GridView of LibraryTile, LibBanner)
│     ├─ ExplorePage (ExploreBanner, GenreBuckets, VrToggle, ExploreRow x 8)
│     ├─ DetailPage (sections 1 to 17, section 4.11)
│     │   └─ InstallConsole, RecoveryPanel (inline, replaces What-you-need while active)
│     └─ SettingsPage
├─ PreviewHost (card preview, above content, clipped to the content area)
└─ OverlayLayer (z order, bottom to top)
    ├─ Scrim (#01000000, click to close)
    ├─ SortMenu, HelpPanel, FiltersDrawer
    ├─ Dialogs (ConfirmDialog, UninstallPreview, ExePicker, ProbeConfirm, RunConfirm)
    └─ Toasts
```

Page model: one `StackView`-like `pageStack` in `App`. Views: List (default), Library (grid toggle), Explore, Detail(gameId), Settings. Overlays do not enter the stack.

---

## 4. Components

### 4.1 App: window, navigation, geometry

- **Spec:** 01 sections 1.1 to 1.3, 1.13 (geometry), 1.16, 2.1, 2.12 rule 5.
- **Properties:** `view` (enum List, Library, Explore, Detail, Settings), `detailGameId`, `size` per view (`sizeLibrary`, `sizeExplore`, `sizeDetail`, `sizeList`), `narrow` (width < 780), `iconsHidden` (width < 620), `vrOverlayMode`, `reduceMotion`, `scanned` (bool, set after the first scan).
- **Signals:** `navigated(view, gameId)`, `geometryChanged`, `closeRequested` (goes to the scan confirm when a scan runs, C-close-scan).
- **Model roles:** none directly. Reads `DetailModel` for the open game.
- **Geometry:** default 1120 x 720, minimum 500 x 400 (desktop) [conflict C-minwin]. Restore rules from 01 section 1.3 (`winWidth` and so on, on-screen check).
- **Breakpoints:** at and above 780, full header. 620 to 779: version pill and tagline hidden. Below 620: title group and headset hidden, ORDER prefix hidden, ORDER pill 112, search column 115, header padding 14 (01 section 1.3, 2.1).
- **Navigation:** Show and card click push Detail. Back (on Detail or Explore) pops to the previous view. Typing in search on Detail pops to List with the query. Enter in search on Explore or Detail goes to List with the query applied (01 section 1.10, 1.16). No page transition animation (Hub parity: 01 section 1.15 "instant").
- **Keyboard:** see section 5.
- **Laser:** every navigation control is a pointer target. No navigation depends on hover.

### 4.2 Header

- **Spec:** 01 sections 1.4, 2.1; responsive rules 01 section 1.3.
- **Children and sizes:**
  - HeadsetGlyph: 32 x 17, stroke `color.brand.orange`, two 3 px eyes, glow `glow.headset_*`. Click toggles the card style (dropped in v1, so no action).
  - Title: "AladdinsCastle", 22 Bold, gradient `type.gradient.title_top` to `title_bottom`, glow `glow.title_*`.
  - VersionPill: `v{APP_VERSION}` (read from the app, not a constant). 10 px, `color.text.faint` on `color.surface.pill`, radius 4.
  - Tagline: "3D light gun & racing games in true VR", 11 Medium, `color.text.faint`.
  - UpdateBanner: hidden unless a new release exists. Colours `color.update_banner.*`. Click runs the updater and closes the app.
  - LibraryButton: 34 x 34, radius 8. Active or idle gradient border (`brand.discover_*`). Tooltip "Open library / discover view".
  - HelpButton: 34 x 34, three 3.5 px dots `color.text.muted`. Tooltip "Help & feedback".
  - OrderPill: 144 x 34 (112 narrow), radius 6. "ORDER" 9 SemiBold blue, mode label 11 with elide. Chevron rotates 180 degrees while open.
  - SearchPill: 165 x 34 (115 narrow), radius 6, the input fills the pill (click anywhere focuses). Placeholder "Search". SearchHint: 10 px, on its own layer below the pill, so it never moves the header.
- **Properties:** `narrow`, `iconsHidden`, `sortMode`, `query`, `updateVersion`, `libraryOn`, `helpOpen`, `orderOpen`.
- **Signals:** `libraryToggled`, `helpRequested`, `orderRequested`, `queryEdited(text)` (every keystroke, no debounce: 01 section 1.10), `updateRequested`.
- **States and timings:** header hover grow factors (version 1.10, headset 1.15, title 1.04, tagline 1.04) are instant (01 section 1.4). Menu open and close: no animation. SearchHint cycles every 2600 ms while focused and empty, then stops when text is entered.
- **Keyboard:** `/` or Ctrl+K focuses search [new, 01 section 2.6]. Esc in search clears the text if there is any; a second Esc leaves the field. Tab order: Library, Help, ORDER, Search.
- **Laser:** visual 34 x 34 with a hit area of at least 40 (desktop) and 56 (overlay mode, section 9) [DIVERGE: the spec sizes are visual]. Pointing does not reveal anything that a click needs.

### 4.3 FilterBar

Two rows [DIVERGE from 01 section 1.5, 2.2], plus the Filters drawer [NEW].

```
+----------------------------------------------------------------------------------------------+
| [<]  GENRE  [All] [Light gun] [Racing]      [Filters 2 v]   [Scan my files]      [S][M][L]   |  row 1
|      STATE  [To install] [Ready] [Updates 4]              IN MY LIBRARY (o)                  |  row 2
|      Active: Manufacturer: Namco x   Year 1995-2001 x   Clear all                          |
+----------------------------------------------------------------------------------------------+
```

#### 4.3.1 Row 1

- **Spec:** 01 section 2.2 row 1; pill chrome 01 section 1.5.
- **Children:**
  - BackArrow: 24 x 24, radius 6, glass tint. Grey on List, white on Detail and Explore (01 section 2.2).
  - GenreChips: All, Light gun, Racing. Pill chrome: 13 SemiBold, padding 15 x 9, radius 6, 1 px hairline border. Active: cream ring (`glass.ring`, halo `ring_halo`). Icon 14 to 18 px in the genre colour on the two genre pills. Single select. Gap 7.
  - FiltersButton: pill with "Filters" and a count badge (20 px minimum, radius 10, `scan.updates_*` colours) showing the number of active facets. Opens FiltersDrawer.
  - ScanButton: part of ScanProgress (section 4.14), label "Scan my files".
  - SizeSwitch: S / M / L, 30 x 28, radius 6, gap 5. Active border `brand.active_sml`, text white.
- **Properties:** `genre` (enum), `activeFacetCount`, `sizeValue`.
- **Signals:** `genreChosen(id)`, `filtersToggled`, `backPressed`, `sizeChosen(value)`.
- **Keyboard:** Left and Right move between pills, Enter or Space selects. Tab leaves the row.
- **Laser:** pill hit height 40 (desktop), 44 (overlay mode). Visual height stays near 34.

#### 4.3.2 Row 2

- **Spec:** 01 section 2.2 row 2; STATE mapping 05 section 6.3 [conflict C-states].
- **Children:**
  - StateChips: To install, Ready, Updates (with count), Needs files (optional fourth pill, 01 section 4 #4). Hidden until the first scan, then revealed 520 ms after the scan ends (01 section 1.5). Gap 6.
  - InLibraryToggle: chip "IN MY LIBRARY", styled like the Scan-on-startup toggle (`scan.toggle_*`). Disabled until a scan has run. Disabled state shows the reason as text below the chip ("Scan my files first") so it does not depend on hover.
  - ActiveFacetChips: one removable chip per active facet value, pill chrome at 12 px (`type.facet_chip`), padding 10 x 6.
  - ClearAll: 11 px, `color.text.menu_caption`, right aligned, hidden when no facet is active.
- **Properties:** `statePills` (set), `scanned`, `inLibraryOnly`, `facets` (list of chips).
- **Signals:** `statePillToggled(id)`, `inLibraryToggled`, `facetRemoved(key, value)`, `clearAllPressed`.
- **States:** STATE before the first scan: hidden, and the filter does nothing (01 section 1.5). The Updates chip shows its count only when the count is above zero.
- **Keyboard:** same as row 1.
- **Laser:** chips 40 (desktop) and 44 (overlay).

#### 4.3.3 FiltersDrawer [NEW]

- **Spec:** 01 section 2.2 (drawer table), schema section 3 (facets).
- **Shell:** anchored under the Filters button, width 400 (380 to 420), panel chrome (`pill` or `panel` fill, `line.button` border, radius 8, padding 6). Section labels use the explore-label box (10 px, `color.text.label` on `color.surface.explore_label`).
- **Sections, top to bottom:**
  1. HARDWARE: tree with three levels (kind, then family, then hardware). Source `data/vocab/hardware.toml`. Expanders with a chevron. Selecting a node selects all its descendants. Node counts in `color.text.count`, 11 px. The Hub-style tri-state look for partially selected nodes.
  2. GRAPHICS: chips 3D (`polygon-3d`), Mixed, Sprite scaler, 2D [conflict C-graphics-drawer: 01 section 2.2 omits it; schema section 3 lists it].
  3. MANUFACTURER: multi-select chips, with the vocab label and count. Only manufacturers with at least one game are shown [proposal].
  4. YEAR: range slider (track `color.line.strong`, fill `brand.orange`, thumb 12 px `glass.ring`) and decade chips for the decades that have games (1980s to 2020s in the data) [proposal: 01 section 2.2 says 1970s to 2020s].
  5. VR: multi-select chips: True 3D, Theatre, Planned (`planned` = `vrPlanned`) (05 section 7.1).
  6. PLAYERS: segmented control 1 / 2+.
  7. CONTROLS: multi-select chips: Gun, Wheel, Handlebars, Bike, Ski, Joystick, Yoke, Boat, Other.
  - Footer: Clear (left), Done (right).
- **Behaviour:** filters apply live. AND across sections, OR within a section. Counts shown are faceted (the current result set, ignoring the section's own facet) [proposal].
- **Properties:** one binding per facet from `FilterState` (05 section 7.1).
- **Signals:** `changed` (live), `doneRequested`, `clearRequested`.
- **States:** closed, open. Escape or scrim closes. Mouse wheel closes the menu, as ORDER and Help do (01 section 1.16).
- **Keyboard:** Tab through sections. Space or Enter toggles a chip. Arrow keys move through the tree; Enter toggles a node.
- **Laser:** chips and tree rows 40 (desktop) and 44 (overlay). Slider thumb visual 12 px, hit area 40. Drawer width unchanged.
- **Fail open:** a malformed facet value is ignored and logged, never hides a game (05 section 2, 7.1).

### 4.4 FeaturedBanner

- **Spec:** 01 sections 1.6 (list and library), 1.8 and 2.5 (Explore banner), 2.3 (pool and kicker), 1.15 (timings); 02 section 2.3 (button colours).
- **Variants:** ListBanner (height 140 / 156 / 174), LibraryBanner (same heights), ExploreBanner (184 / 200 / 224, no rotation, Shuffle button).

```
+------------------------------------------------------------------------------------------+
| (.) FEATURED GAME - TRUE 3D                                    | art (right, Uniform)  |
| Time Crisis                                                    |                       |
| Rail shooter . Cover shooter (pedal) . 1995                    |                       |
| [ Show ]   [ Explore all games > ]                 [Close][Always disable]  (after 5 s)  |
+------------------------------------------------------------------------------------------+
```

- **Layers:** frame (radius 8, `color.surface.banner`, border `line.strong`, clipped); wash (gradient from `ColorMath::washStops(accent)`, section 9.3 of 05); art image right-aligned, `Stretch Uniform`; fade (`gradient.list_banner_fade` or `explore_banner_fade`); text block with margins from `space.banner_text_pad`; buttons.
- **Kicker:** dot (7 px) plus "FEATURED GAME" and the VR label (TRUE 3D, THEATRE, PLANNED) [DIVERGE: 01 section 2.3 uses VR quality, not control type]. Dot colour is `accent`. Dot pulses on a 1700 ms auto-reverse loop, with scale and glow in opposite phase (`glow.dot_pulse_period`).
- **Title:** 22 SemiBold (S), 25 (M), 28 (L), gradient, max width 380. Words are dropped whole when too wide, no ellipsis mid-word (01 section 1.6).
- **Subtitle:** up to 3 subgenre labels joined with "  .  ", then the year. Max width 380, ellipsis.
- **Buttons:** Show (gold border `brand.gold`, text gold, radius 4, border 2 px; press border `gold_press`). Explore (orange border 1.5 px, text orange with "  >", press `orange_press`).
- **Properties:** `gameId`, `title`, `subtitle`, `vrBadge`, `accent`, `artBanner`, `size`, `hovered`, `pressed`, `closeChipsVisible`, `disabled` (setting), `rotating`.
- **Signals:** `showRequested(gameId)`, `exploreRequested`, `closeRequested` (session), `alwaysDisableRequested` (sets `bannerListDisabled` or `bannerLibDisabled`), `shuffleRequested` (Explore only).
- **States:**
  - Idle: border `line.strong`.
  - Hover: border `line.banner_hover`; frame scale 1.02 (instant); Show button sweep 700 ms, once per hover (01 section 1.15).
  - Press (art or title): border `banner_press` 2 px, scale 1.02.
  - Close chips: appear after 5000 ms of hover, hide 800 ms after leaving (01 section 1.15). Also reachable through the overflow button in overlay mode (section 9.4).
  - Show or Explore pressed: press glow shows, then navigate after 1200 ms (01 section 1.15). Navigation is deferred, not immediate.
  - Rotation (List and Library only): random 5 to 15 minutes, re-armed each tick, picks a new game from the pool (05 section 9.4). Explore does not rotate.
- **Keyboard:** Show and Explore are focusable (Tab), Enter activates. Focus ring on the frame when a button has focus.
- **Laser:** Show and Explore are 40 tall (desktop) and 56 (overlay), with text 14 and 16 in overlay mode.
- **Disabled:** hidden when `bannerListDisabled` or `bannerLibDisabled` is set. Re-enabled from Settings.

### 4.5 SectionHeader

- **Spec:** 01 sections 1.7 (pill), 2.4 (genre sections).
- **Shape:** pill, fill `glass.section_fill`, line `glass.section_line`, radius 11, padding 10 x 6. Icon 14 px in the genre colour (bolt glyph, 14 px stroke path). Title 13 SemiBold white (14 M, 17 L). Kind 13 Medium in the genre colour ("Light gun" or "Racing"). Count 11 Medium `color.text.count` ("14 games"), 9 px left margin.
- **Divider:** 1 px `color.line.rule`, bottom margin 24, after each group.
- **Spacing:** first header top 20, bottom 10. Grid below has top 22 and bottom 30 (01 section 1.7).
- **Properties:** `title`, `kind`, `count`, `colour`, `size`.
- **Signals:** none. Not focusable.
- **Order:** Light guns, divider, Racing, divider (01 section 2.4). Recently Played sits above both.

### 4.6 GameGrid

- **Spec:** 01 sections 1.7, 1.9, 1.11 (ORDER reorders, never rebuilds), 2.4; 02 sections 1.2, 1.12, 1.13, 2.5; 05 section 7.5 (rows).
- **Two forms:**
  - SectionedGrid (List page): a `ListView` over `GridRowModel` rows (05 section 7.5). Each row is either a SectionHeader or a row of up to N GameCards, N computed from the viewport width. Rows are virtualised by the ListView. GridView does not provide section headers in Qt 6 [verify]. A single `GridView` would instantiate every card of a section, which is costly with the card's effects.
  - PlainGrid (Library): a `GridView` over the filtered source, with tile cells. Virtualised by Qt.
- **Geometry:** cell width = card width + 12; cell height = card height + 12. Columns = `floor((viewport - 2 x 28 + 12) / cellWidth)`. Rows centre with a computed left margin (02 section 1.2 WrapPanel centring).
- **Properties:** `model` (GameFilterModel or GridRowModel), `cardScale` (S 1.00, M 1.15, L 1.30), `viewportWidth`, `count`.
- **Signals:** `activated(gameId)` (Enter or click), `previewRequested(gameId)`, `menuRequested(gameId)` (overflow, overlay mode).
- **States:**
  - Empty: no game matches. Text "No games match these filters" with a Clear filters button [proposal: no empty state in 01 to 03].
  - Scrolling: the Flickable's `moving` flag suppresses card previews. Re-armed 120 ms after the last scroll event (02 section 1.9, `scroll_quiet`).
  - Reorder: instant (Hub parity). No `displaced` animation [open].
- **Keyboard:** arrows move across rows and sections; Home and End go to the first and last card; Page Up and Page Down move one screen. Enter opens detail. Tab leaves the grid.
- **Laser:** the card is the target; the card's own button is a separate target (section 4.7).

### 4.7 GameCard

- **Spec:** 02 sections 0 to 2 (all), 1.9 (hover), 2.3 (button states), 2.4 (art), 2.2 (field mapping); 01 section 1.7 (sizes).
- **Size:** base 175 x 160 at S, 201 x 184 at M, 228 x 208 at L (scale 1.00, 1.15, 1.30 applied to width and height, `size.card`). Default in overlay mode is L (section 9).
- **Layout (AladdinsCastle, 02 section 2.1):**

```
+--------------------------------------------------+
| [TIMECRIS]                    [gun|2P| i ]       |  family pill (pill) | controls glyph, 2P, info
|                                                  |
| Time Crisis                       [WIP] [TRUE 3D]|  title (2 lines) | docked badges
| Namco . Namco Super System 22 . 1995             |  meta line, accent colour
|     by Namco                                     |  developer, only if different
| Duck behind cover for real, then pop up.         |  blurb, one line
| +----------------------------------------------+ |
| |                 Ready                        | |  neon button; label rule in 2.3
| +----------------------------------------------+ |
+--------------------------------------------------+
```

- **Elements (base px):**
  - Family pill: `hub.pill`, uppercase, 8 SemiBold, radius 3, padding 6 x 2, max width 104. Fill: accent at 0.18 over `surface.pill`. Text: accent at 0.5 with white at 0.5 (`formula.pill`).
  - Info pill: height 22, radius 11, border `color.card.info_border`, separator `info_sep`. Contains the controls glyph (gun, wheel, bike, or generic), "2P" when `players >= 2`, and an italic bold "i". The "i" opens the detail page (02 section 2.2).
  - Title: 13 Bold, gradient `text.primary` to `text.card_title_end`. Up to 2 lines.
  - Docked badges (in the title row, right): WIP in `badge.wip` (radius 2, 8 Bold); quality badge (TRUE 3D, THEATRE, PLANNED) (`badge.quality_*`, section 4.2 of 05).
  - Meta line: manufacturer short label, hardware label, year, joined with " . ", 10 px Medium in the accent colour (9 px at 31 or more characters). Drop order when too wide: manufacturer, then hardware short, then ellipsis (02 section 2.2). Needs short labels [conflict C-shortnames].
  - Developer line: "by <developer>", 9 px `text.author`, shown only if `developerShown` (8 px if longer than 28 characters).
  - Blurb: `hub.blurb`, 10 Medium `text.note`, one line, ellipsis, full text in the tooltip.
  - Button: radius 4, neon outline, fill `formula.neon.fill`, label 11 SemiBold in the neon colour (section 4.19). Label per state, section 4.7.1.
  - Reinstall pill: 28 wide, right inside the button, glyph ↻ (U+21BB), only when `reinstallVisible`. Divider `reinstall_card_line`.
- **Properties:** `gameId`, `title`, `pill`, `colourBase`, `accent`, `neonAccent`, `blurb`, `meta` (strings), `developerShown`, `badges`, `vrBadge`, `controlsType`, `players`, `state`, `stateLabel`, `reinstallVisible`, `artBanner`, `isWip`, `scale`, `focused`, `hovered`, `previewActive`.
- **Signals:** `bodyActivated(gameId)` (click on the card, not on a child), `primaryPressed(gameId)`, `reinstallPressed(gameId)`, `infoPressed(gameId)`, `menuPressed(gameId)`.
- **Background:** base `colourBase` (per-game `hub.colour`, fallback `surface.card`) with the tint from section 9.1 of 05. Border from the same formula.

#### 4.7.1 Button labels

From 05 section 6.2 and 02 section 2.3:

| State | Label | Colour token |
|---|---|---|
| Ready to install | Install | accent (`genre` or `hub.accent`) |
| Installed | Ready (rest). Hover or focus shows "Start in VR ▶" [conflict C-ready] | `state.ready_neon` |
| Update | ↓ Update | `state.update_neon` |
| Needs files | Find my files | `state.needs_neon` |
| Needs emulator | Install emulator | `state.needs_emulator` |
| Coming soon | Coming soon | `state.planned_neon` (neutral) |
| No setup yet | No setup yet [proposal] | `state.planned_neon` |
| Data error | Data error [proposal] | `state.planned_neon` |
| Installing | Installing 3 of 5 | `state.installed_neon` |
| Install failed | Retry install | `state.retry_line` |

#### 4.7.2 States and timings

| State | Behaviour | Timing and source |
|---|---|---|
| Rest | Flat tint, border tint | none |
| Hover-dwell | Pending after pointer enters the card body | 600 ms (`motion.hover_dwell`, Helpers.ps1:91). Cancelled if the pointer moves onto the button (02 section 1.9) |
| Preview | The card is shown at 1.4 in `PreviewHost`, z 1000 (above the content, below menus). Art strip (175 x 80, clipped to radius 7, bottom fade 18 px) shows `artBanner`. Pill, meta and developer lines hidden | `motion.preview_scale`; shadow `glow.card_hover_*` (Helpers.ps1:1162-1172) |
| Focus (keyboard) | Cream ring 2 px (`glass.ring`) and the same preview after 600 ms [proposal] | `motion.focus_preview` |
| Press | Click pulse `badge.click_pulse` blur 18, 1200 ms, then open detail on the next frame (02 section 1.10) | `glow.click_pulse_blur`, `motion` (1200) |
| Disabled | Not used. Coming-soon cards stay clickable; their button opens the roadmap item | 02 section 2.3 |

- Previews are suppressed during scrolling (section 4.6).
- Cards without art still preview, using the generated banner (02 section 2.4).

#### 4.7.3 Hit map (replaces the Hub's overlapping zones)

The Hub uses three overlapping zones (hot zone, pill guard, top shield) to route clicks (02 section 4.4). Replaced by one rule: the card is one target; children are on top.

| Z | Element | Action |
|---|---|---|
| 0 | Card body | open detail, after the click pulse |
| 1 | Info pill "i" | open detail (until `info_url` exists [open]) |
| 1 | Family pill, badges | none (decorative) |
| 2 | Primary button | primary action (section 4.7.1): Install opens the install flow, Start launches, Find my files opens the locator, and so on |
| 2 | Reinstall pill | reinstall: opens the install flow in Reinstall mode |

#### 4.7.4 Keyboard and laser

- Tab reaches the card, then its button, then its info pill and reinstall pill. Enter opens detail. Space presses the primary button [proposal].
- Laser: card body is one target; the button is 56 tall in overlay mode (card grows to 208 at L; fit checked in section 9). Reinstall pill 44 with the text "Reinstall" in overlay mode (section 9.2).
- Hover never reveals an action. The button label swap is information only, and the rest label carries the same meaning (C-ready).

#### 4.7.5 Model roles used

`gameId, title, pill, colourBase, accent, neonAccent, blurb, developer, developerShown, manufacturerShort, hardwareLabel, year, controlsType, players, vrBadge, badges, isWip, state, stateLabel, stateColour, reinstallVisible, artBanner, artTile` (05 section 5).

### 4.8 LibraryView (portrait grid)

- **Spec:** 01 section 1.9 (tiles and banner), 2.4 (library has no sections).
- **Tiles:** 220 x 330 (S), 250 x 375 (M), 275 x 413 (L). Portrait art (`artPortrait`), 2:3. Content: art only, plus a 4 px bottom bar in the state colour [proposal]. Tooltip is the title.
- **Layout:** one `GridView` centred, no section pills. Order follows the sort mode.
- **Banner:** LibBanner, same as the list banner, heights 140 / 156 / 174. Own rotation timer.
- **Keyboard and laser:** arrows, Enter opens. Tile target is the whole tile, 220 to 275 wide, which is well above the minimum.
- **States:** empty (as GameGrid).

### 4.9 ExplorePage

- **Spec:** 01 sections 1.8, 2.5, 2.3 (Shuffle), 2.6 (search hint), 2.9 (`sizeExplore`).

```
+------------------------------------------------------------------------------------------+
| [< Back to library]                                                                       |  back: 14, 14
|   (.) FEATURED PICK                                                                       |  ExploreBanner
|   Title (24)                                                                              |
|   subtitle                                                                                |
|   [ View this game ]  [ Shuffle ]                                    art on right          |
+------------------------------------------------------------------------------------------+
| [GENRE]   [All] [Rail shooters] [Cover and sniper] [Horror] [Hunting, party, water] ...   |
| [VR  All | True 3D | Theatre | Planned]                                                   |
| Rail shooters  (119 games)                                                 < [tile] [tile] >  |
+------------------------------------------------------------------------------------------+
```

- **Banner:** ExploreBanner (section 4.4). Button labels: "View this game" (gold) and "Shuffle" (orange). Shuffle picks within the active bucket with the picker rules in 05 section 9.4. The banner does not rotate.
- **Back button:** "Back to game list" when opened from List, "Back to library" from Library [proposal wording; 01 section 1.8 uses "Back to mod list"]. Outline style, border `line.button`, hover `brand.back_hover`.
- **Genre buckets:** chips All, eight buckets (05 section 7.4), New [open]. Chip pill: radius 5 (S, M) or 6 (L), accent bar 4 to 5 px on the left, label 11 to 12.5 SemiBold, min height 26 to 30, margin 0 0 8 8. Active fill `color.surface.chip_active`, border in the bucket colour.
- **VR toggle:** chips All (`badge.vr_chip_all`), True 3D, Theatre, Planned. The label "VR" shows the active choice [DIVERGE: replaces PC POWER; 01 section 2.5].
- **ExploreRow (x 8):**
  - Header: title 22 Bold gradient, count bubble "N games" (10 SemiBold, `count_*` colours), scaled 1.12 on hover (decorative). Bottom margin 10; row bottom margin 26.
  - Tiles: 140 x 210 (S), 175 x 260 (M), 215 x 320 (L).
  - Edges: 56 px host with a gradient fade. 30 px circle buttons (`surface.pill` fill, `line.button` border, 16 px orange chevron). In overlay mode 44 px.
  - End of row: the right button shows a clockwise-arrow glyph for 1200 ms, then resets (01 section 1.8).
  - Empty bucket: row hidden [proposal]. Currently the `horse` subgenre has no bucket (05 section 7.4).
- **Keyboard:** Left and Right move within a row, Up and Down between rows, Enter opens.
- **Laser:** edge buttons 44 (overlay mode); tiles are whole targets.
- **Properties:** `bannerGameId`, `bucket`, `vrChoice`, `newOnly`, `rows` (model of bucket proxies).

### 4.10 RecentlyPlayedRow

- **Spec:** 01 section 1.7 (heading, overlays), 1.13 (`recentlyPlayed`, max 8, `recentlyPlayedHidden`), 2.4 (first section).
- **Heading:** play triangle (teal `brand.recent_teal`, 11 x 14), title "Recently played" 13 SemiBold, caption "to launch in VR" 11 Medium `brand.recent_gold`, 20 px left margin.
- **Tiles:** up to 8, centred, at the card's S size [proposal; the Hub does not state a size].
- **Hidden:** when empty, or `recentlyPlayedHidden`.
- **Hub overlays:** heading hover 5000 ms shows "Close" and "Always disable"; tile hover 7000 ms shows the management overlay (01 section 1.7).
- **Replacement (required by section 9):** an overflow button on the heading (menu: Hide this row, Always hide) and on each tile (menu: Remove from recent). Always visible in overlay mode, and on desktop as well.
- **Keyboard and laser:** tiles activate on Enter or click; overflow buttons 44.
- **Model:** `RecentModel` (ordered ids, max 8) resolved to records.

### 4.11 DetailPage

- **Spec:** 03 sections 1 (how the Hub does it), 2.1 to 2.17 (our design), in the 03 section 2.3 order. Section numbers below match 03 section 2.3 #1 to #17.
- **Layout skeleton:**

```
< Back to library
+------------------------------------------------------------------+
|  hero: art/banner.png, 360 px, radius 10                         |  #1
+------------------------------------------------------------------+
Time Crisis   [TRUE 3D] [NEEDS YOUR FILES] [GUN] [COVER] [ROOMSCALE]  #2 title row
MANUFACTURER Namco | HARDWARE Namco Super System 22 | YEAR 1995       #3 meta strip
VARIANT  ( DR-89 PCVR  * )  ( DR-89 Quest 3 )  ( Our true 3D - planned )   #4
| amber bar  Needs your files: timecris.zip not found. We never download it.  #5 state line
 WHAT YOU NEED   (rows, section 4.11.2)                                       #6
 [ Find my files ]  [ Upstream page ]                                         #7 action row
 SETTINGS FOR THIS GAME   Cover [ Grip | Duck ]  Laser [ On | Off ] ...      #8
 ABOUT | SIMILAR GAMES   (two columns)                                       #10, #11
 [VIDEO]  (hidden: no video field, C-video)                                    #12
 CONTROLS (table)  ·  README  ·  WHAT IT INSTALLS  ·  UNINSTALL  ·  quip      #13 to #17
```

#### 4.11.1 Sections

| # | Section | Behaviour | Spec |
|---|---|---|---|
| 1 | Hero | 360 px, radius 10, background `hero.bg`, tint from accent (top 0.30, mid 0.10, `formula.hero_tint`). Image chain `art/banner.png` → marquee crop → generated. Above 1040 px window width: frozen 1040 centred, accent edges (alpha 150, fades 0.22 to 0.78). Hover scale 1.02 (decorative). No trailers. | 03 1.2 #3 to #5, 2.3 #1 |
| 2 | Title row | Title 28 Bold, gradient. Pills in order: family pill (`hub.pill`), quality badge (TRUE 3D, THEATRE, PLANNED), state pill (uppercase state name, section 6.1 of 05), then `badges` in fixed order. Pills hover 1.08, decorative. | 03 1.3, 2.3 #2 |
| 3 | Meta strip | MANUFACTURER, HARDWARE, YEAR; DEVELOPER only if `developerShown`. Labels 9 SemiBold `text.label`, values 13 Medium `text.value`, rule `line.rule`, gap 24. | 03 1.2 #7, 2.3 #3 |
| 4 | Variant picker | Section 4.11.3. | 03 2.4 |
| 5 | State line | 3 px bar, tint alpha 26, one sentence in the state colour (section 4.11.4). Always visible, never hover-only. | 03 1.4, 2.3 #5, 0.4 |
| 6 | What you need | Rows (section 4.11.2). Visible until all green; collapses to a single green line once installed. | 03 2.5, 2.3 #6 |
| 7 | Action row | Primary and companion buttons (section 4.11.2). | 03 2.6, 1.5, 1.6 |
| 8 | Settings strip | Per-game settings the variant supports (Cover, Laser, Gun angle, Hand). Save writes the profile and runs only the `write-config` step. | 03 2.10, 2.3 #8 |
| 9 | Performance | Hidden in v1. | 03 2.15 (OPEN) |
| 10 | About and notice | README About section with `blurb` first. Notice box (amber) when `notice` exists [proposal field]. | 03 1.8, 2.3 #10 |
| 11 | Similar games | Four rows, up to 12, count from box height (`clamp((h - 26) / 93, 4, 12)`). Each row: thumb 160 x 75 (`artTile`), title 13 Medium, sub-line with control type, quality badge and state. Click opens that game. Empty: "No similar games found" (12 Medium `text.muted`). | 03 1.8, 2.13 |
| 12 | Video | Hidden: no `video` field (C-video). | 03 1.9, 2.3 #12 |
| 13 | Controls | Table generated from `setup/controls.toml` (ghost controls) or `setup/gun.toml`, plus the pause-overlay footnote from `docs/controls.md` section 3. Columns: action, input, notes. Depends on config-spec section 5, which 03 section 2.16 #7 flags. | 03 2.3 #13 |
| 14 | README | Parsed from `games/<id>/README.md`. Order: About, About this mod, Where to get the game, What it installs, Requirements, Note; middle sections; tail (related, community, discord, support, donate, credit, deactivate, uninstall). Skip "How to use" when it asks the user to run files by hand (03 1.21 #9). Monospace code chips, copy chip for single-line launch options, quip box for `>>>` lines, table rows, images relative to the README, http and https links only. | 03 1.10, 2.3 #14 |
| 15 | What it installs | One row per component: name, pinned version, author, licence, upstream link, role. From `upstream`, `license`, `meta.sources`. | 03 2.14, 2.3 #15 |
| 16 | Uninstall | Only when installed. Uninstall guide (collapsible, `Accessible.expanded`) and Uninstall (preview dialog, section 4.17). | 03 1.11, 2.12, 2.3 #16 |
| 17 | Quip and support | Quip last, accent bar, italic (`quip` field [proposal]). Support box only with a `supportUrl` [proposal]. Discord block: not in v1 (03 2.1). | 03 1.12, 2.3 #17 |

Header action: "Back to library" (or "Back to game list"), outline style, hover `brand.back_hover`, press `brand.orange_press`.

#### 4.11.2 Action row and What you need

- **Primary button:** rules in 05 section 6.2 and 03 section 2.6. Size: 162 minimum width, padding 16 x 10, radius 7, 14 SemiBold (03 1.5). Colours: Ready (`state.ready_button_*`), Update (`state.update_neon` fill, `update_line` border, white text), Needs files (amber), Install (accent neon with tint 0.06), Coming soon (grey, disabled, opens the roadmap item), Installing (progress, disabled), Retry (`state.retry_line` border).
- **Companions:** as the table in 03 section 2.6. Shown in the order listed there. The list in that table is authoritative; this component shows exactly those.
- **Click rules:** Install opens the install flow only when every "What you need" row is green; otherwise the button is disabled and the reason is in the state line. Start calls `launch()` (section 4.11.5). Update runs the update recipe through the console.
- **What you need rows** (`NeedsModel`, 05 section 5): kind (media, tool, headset, runtime), name, status word, buttons.
  - Media: Found, Found (not verified), Missing, Wrong version; buttons Find my files, Search drives.
  - Tool: Found, Found (older), Missing; buttons Install emulator, Locate, Search.
  - Headset (Quest variants only): Connected, Accept the prompt in the headset, Not connected; buttons How to enable developer mode, Check again. Developer mode is inferred from the connection (03 2.16 #6).
  - Runtime (PCVR): Found: SteamVR, Meta Link or Virtual Desktop; Not found; button Open runtime setup.
  - Re-checked on window focus and on Check again, never in the background (03 2.5).
- **Keyboard:** Tab order follows the row: primary, then companions. Enter and Space press.
- **Laser:** primary 56 tall, companions 44, gaps 16 (section 9.2).

#### 4.11.3 Variant picker

- Chips for every variant, ordered installed, stable, wip, planned (03 2.4). Each chip: short variant title, quality tag (TRUE 3D or THEATRE), status dot. Dot: installed (green), ready (grey outline; "ready" is not an install.toml variant status, see C-variant-status), wip (amber), planned (dashed grey).
- Planned variants are shown and disabled.
- Default selection: the installed variant, else the install.toml `default`.
- Switching re-renders What you need, the action row and the settings strip.
- Keyboard: arrows move between chips, Enter selects. Laser: 44 tall.

#### 4.11.4 State line wording

| State | Text (from 03 1.4, 2.12, 2.8) |
|---|---|
| Installed | "Installed. Start in VR when you are ready." [proposal wording] |
| Installed with warnings | "Installed, with 1 warning." (03 2.8) |
| Installed with skipped steps | "Installed, with skipped steps." amber (03 2.9) |
| Update | "Update available: v1.3.0 (you have v1.2.0)." (03 2.12) |
| Needs files | "Needs your files: <file> not found. We never download it." (03 2.2) |
| Needs emulator | "Needs <tool>. Install it or point to an existing copy." |
| Ready to install | "Ready to install." |
| Coming soon | "Planned. See the roadmap item for this setup." |
| Install failed | "Install stopped at step 2 of 3. Nothing was marked installed." (03 2.8) |

#### 4.11.5 Launch

Uses 03 section 2.11 in this order: runtime check (new, with "Start SteamVR" and "Start anyway"), Quest adb check, record recent, minimise the window (restore when the game exits, [proposal]), start the variant's `launch.exe` in `launch.cwd`, track the process and show "Playing <title>". A missing folder clears the variant's install state and says so. No silent flat-screen fallback.

#### 4.11.6 Keyboard and laser

- Tab moves through the sections in order. Escape goes back.
- Hit targets follow section 9.2. Section-level focus rings as elsewhere.

### 4.12 InstallConsole

- **Spec:** 03 section 2.8 (layout, event types, log, steps, prompt, cancel, samples), 1.14 (console grammar), 3 (events and await).
- **Where:** replaces What you need while a job runs. Stays below the action row as "Last install" (collapsible) until the next action.
- **Layout:** monospace (`font.mono`), 13 px at M (`type.console`), line height 18, fill `console.bg`, border `line.rule` (box_rule), radius 6, max height 360, auto-scroll to the newest line unless the user has scrolled up [proposal]. "Copy log" button. Opening block: a 60-character rule in the accent colour, the variant title, "AladdinsCastle installer".
- **Event types** (`InstallConsoleModel`, one row per event):

| Kind | Text example | Colour |
|---|---|---|
| step | `--- [2/4] Extract ---` | `console.step` |
| ok | ` [OK] Extracted 412 files to installed/timecris/dr89-pcvr/` | `console.ok` |
| warn | ` [!!] 3 files differ from the package; kept your copies` | `console.warn` |
| fail | ` [XX] timecris.zip is not the set Time Crisis World TS2 Ver.B` | `console.fail` |
| work | ` [..] Checking the archive layout...` | `console.work` |
| detail | `  From: <url> [GitHub release]` (two-space indent) | `console.detail` |
| prompt | ` >>> Press Continue to start the install ` (yellow pill) | `console.prompt_fill` with `prompt_text` |
| done | Closing rule and "Install finished", with the next action | `state.status_installed` |

- The markers stay as text, so the log file and the screen match (03 2.8).
- **Prompt:** yellow pill with a Continue button. Enter continues (03 section 3). Escape cancels only where the step allows it.
- **States:** hidden, preflight, running, waiting for prompt, cancelling (only between steps), done, done with warnings, done with skipped steps, failed (RecoveryPanel shows).
- **Timing:** download progress lines update every 250 ms (`motion.download_progress`). Nothing is polled; the job is awaited (03 section 3).
- **Keyboard:** Enter continues a prompt. Ctrl+C copies the selected line [proposal]. Escape per the rule above.
- **Laser:** Continue and Copy log are 44 tall in overlay mode. Scroll the console with a visible scrollbar 16 px wide [proposal].

### 4.13 RecoveryPanel

- **Spec:** 03 section 2.9 (manual step and recovery), 1.15 (Hub fallback), 1.16 (folder).
- **Modes:**
  - Manual step (download failed): banner "Manual step needed: Download <name>", amber rule, one sentence "The automatic download of <name> failed: <reason>.", then: Open the download page (opens after the click, not on its own), drop the file or paste its path, Retry, Skip (optional steps only), Open folder.
  - Recovery (any unhandled failure): heading "INSTALL STOPPED AT STEP 3 OF 4 - nothing was marked installed", the failure message in one or two sentences, buttons Retry this step, Retry from start, Open log, Open install folder, Open Downloads, Clear handover (when a path was handed over). Drop zone: "Drag any downloaded file, game folder or archive here, or paste its path. It is handed to the next retry."
- **No Exit button** (03 2.9). The panel stays until the install succeeds or the window closes.
- **Drop validation:** file is staged, then checked by type, size and SHA-256 before it may replace anything. On failure: "That file is not <name>. The existing file was kept."
- **Properties:** `mode`, `stepIndex`, `stepCount`, `message`, `logPath`, `handoverPath`, `canSkip`.
- **Signals:** `retryStep`, `retryAll`, `openLog`, `openFolder`, `openDownloads`, `clearHandover`, `fileGiven(path)`, `skipStep`, `openDownloadPage`.
- **States:** shown, drag over (zone highlights), validating, accepted (handover set), rejected (message).
- **Keyboard:** Tab through buttons. The path field accepts Enter to submit.
- **Laser:** dragging is unreliable by laser. Add a "Choose file" button beside the drop zone that opens a file picker [proposal]. Buttons 44 in overlay mode.

### 4.14 ScanProgress (scan button, counter, spinner, shimmer)

- **Spec:** 01 sections 1.5 (scan button and counter, shimmer, scan on startup), 1.12 (flow and spinner), 2.8 (scan flow), 1.15 (timings).
- **States:**
  - Idle: label "Scan my files" (`type.scan_label`), magnifier 13 px, fill `scan.fill`, border `scan.line` 2 px, radius 6. Attention pulse: 3 pulses over about 2.4 s (400 ms steps) until the first scan has run (01 section 1.5).
  - Scanning: label "Scanning... 120 of 214" in `scan.scanning` 14 px (live count [NEW], 01 section 2.8). Second magnifier on the right. Counter and shimmer hidden. Spinner visible. A Cancel button appears [proposal]. Re-entry is refused while a scan runs.
  - Done: counter "[N] found | [M] ready" (numbers `scan.found`, 13 ExtraBold; separator `scan.separator` at 40 %; green triangle 7 x 8). The halo: blur 12, opacity 0.22 (01 section 1.12). The ready count is hidden when zero.
  - Stale: heartbeat older than 60 s (`motion.scan_heartbeat_stale`): label "Scan stopped responding", Reset button.
- **Spinner:** rounded rectangle, radius 6. Two lit segments, each 30 % of half the perimeter (`glow.spinner_lit_fraction`). Loop 2200 ms. Halo: thickness 3.2 x core, opacity 0.40, blur 9. Band: thickness 2, opacity 0.95, blur 2. Colour `scan.spinner`. Drawn on the render thread, so it keeps moving while the scan runs on a worker (01 section 1.12).
- **Shimmer:** 80 px band, gradient `scan.shimmer_edge` to `scan.shimmer_centre` to `shimmer_edge`, sweep 1500 ms, random pause 12 to 60 s. Only after a result. Off with `shimmerDisabled` or reduce-motion.
- **Opt-out:** after 5000 ms of dwell on the counter, two chips "Disable shimmer" and "Always disable" (01 section 1.5). Replacement (section 9.4): the same two options live in the scan overflow menu and in Settings.
- **Scan on startup:** hover-only toggle in the Hub (hides 450 ms after leaving, 01 section 1.5). Replacement: a toggle in the overflow menu and in Settings. Hover may show the toggle as decoration, never as the only route [DIVERGE].
- **Cancel and close:** closing the window during a scan asks first [conflict C-close-scan]: 01 section 2.8 "closing asks to cancel" vs section 2.12 rule 5 "cannot be closed away mid-run".
- **Keyboard:** Enter or Space starts a scan when focused. Escape during a scan asks to cancel.
- **Laser:** 40 tall (desktop) and 56 (overlay).

### 4.15 SettingsPage

- **Spec:** 01 section 1.13 and 2.9 (persisted keys), 1.17 (help items), 02 section 5 #5 (reduce motion), 01 section 4 #13 (scan folders). **No spec defines the Settings page itself**: the content below is [proposal] (section 10, C-settings).
- **Sections:**
  - Library folders: list of `scanFolders` (add, remove), Search drives, the Steam, GOG and Epic roots shown read-only (01 section 2.8 Sources).
  - Scanning: scan on startup (`checkOnStartup`), shimmer (`shimmerDisabled`).
  - Appearance: reduce motion (`reduceMotion`), banners (`bannerListDisabled`, `bannerLibDisabled`), Recently played (`recentlyPlayedHidden`), hidden publishers list (`hiddenPublishers`, with "Show again"; C-hiddenPublishers).
  - VR: overlay mode on or off (`vrOverlayMode`) [proposal]; the overlay's size and distance notes (section 9).
  - Help and feedback: Suggest a game, Logs and report a problem, Discord (only if the server exists), Desktop shortcut toggle (`desktopShortcut`). "Switch Hub Style" is dropped [DIVERGE; 01 section 4 #11].
  - About: version, licence (GPL-3.0), the catalog count and warnings report (05 section 2).
- **Controls:** toggles 44 tall in overlay mode, rows 56.
- **Keyboard:** standard form navigation; Escape goes back.
- **Model:** `SettingsStore` (05 section 13).

### 4.16 Toasts

- **Spec:** none in 01 to 03. The Hub shows failures in a modal (`Write-HubActionFailure`, Helpers.ps1:3403-3440) and writes `hub-errors.log`.
- **Proposal:** info toasts (update installed, location cleared, scan done) at the bottom right, auto-hide after 4000 ms. Errors are modal dialogs, as in the Hub, and are also written to the log. Toast close target 44 in overlay mode.
- **Properties:** `items` (list). **Signals:** `dismissed(id)`.
- **Keyboard:** Escape dismisses the newest toast.

### 4.17 Dialogs

- **Spec:** 03 sections 1.11 (uninstall), 1.16 (locate and clear), 1.15 and 2.9 (manual step), 2.12 (uninstall preview), 2.8 (run step). Strings from the Hub where given.
- **Generic:** `ConfirmDialog` with title, body, buttons, and a default button that is always the safe one (never the destructive one).
- **Specific dialogs:**
  - UninstallPreview: "Remove 412 files installed by <variant>? Your <media>, saves, profiles and any file you edited are kept." Three examples. Buttons Remove, Cancel. "Cancel changes nothing" (03 1.11, 2.12).
  - ForgetLocation: "Forget the saved location for <title>? It will go back to 'not found' until you locate it again." Buttons Forget, Cancel (03 1.16).
  - ExePicker: "Select the game exe for <title>", question "Which file launches the game?" (03 1.16).
  - ProbeConfirm: "This folder doesn't contain <probe>. Use it anyway?" Buttons Yes, Pick again (03 2.9).
  - RunConfirm: "This runs <tool>. Continue?" (frontend.md section 3, `run` step).
  - ScanCancel and CloseDuringInstall: "Stop the scan?" and "Stop the install after this step?" [proposal; C-close-scan].
- **Keyboard:** Escape cancels. Enter presses the default (safe) button.
- **Laser:** buttons 44 (overlay mode), centred with 16 px gaps.

### 4.18 Overlay menus: SortMenu and HelpPanel

- **SortMenu (ORDER):** spec 01 section 1.11, 2.7. Width 230 (overlay mode 320), top 60, right 203, panel chrome. Heading "SORT EVERY GAME LIST" (10 px). Rows: label 13 `text.inactive`, caption 11 `text.menu_caption`, active dot 8 px `brand.blue`, hover `color.surface.menu_hover`. Modes and captions (05 section 7.3): Title ("A-Z, series in order"), Year ("Oldest first"), Manufacturer ("A-Z, then year"), Hardware ("Arcade, then console, then PC, then year"), Recently played ("Last launched first"), Added to library [proposal]. Closes on scrim click, wheel, or Escape. Choosing a mode re-sorts the proxy; no rebuild (01 section 1.11).
- **HelpPanel:** spec 01 section 1.17, 2.1, 4 #11. Width 224 (overlay mode 300), top 60, right 200. Heading "HELP & FEEDBACK" (10 px). Items: label 13, caption 11 `menu_caption`, dot 8 px, radius 7, padding 8 x 9. Items: Suggest a game (orange dot, opens a short form), Logs and report a problem (`#d8923a` dot, opens the latest log), Join our Discord (`brand.discord`, only if a server exists [open]), Desktop shortcut (`#4ac07a` dot; caption "Currently on, click to turn off" or the opposite). Opening Help closes ORDER (01 section 1.17).
- **Keyboard:** Escape closes. Arrows move between items.
- **Laser:** items 44 (overlay mode).

### 4.19 Shared primitives (`hub/qml/common/`)

| Primitive | Spec | Notes |
|---|---|---|
| PillButton | 01 1.5 (chrome) | Idle, hover (brightened, no accent border), active (cream ring, 2 px halo at margin -1), disabled. Used for genre, state, facet and filter pills |
| NeonButton | 02 1.4, 1.5 | Fill `formula.neon.fill`; border neon; glow on the border item (`MultiEffect` shadow, blur 14 x scale, opacity 0.72 - 0.22 x lum, floor 0.42). The label is on a sibling so the glow does not blur it (02 1.13). Neon colour: `ColorMath::neonLift(accent) * 0.90` |
| SegmentedControl | 03 1.6 (VR / Flat) | Active `state.ready_label` bold, inactive `color.state.segment_inactive`, border `color.state.segment_line`. Used for Cover, Hand, Players and On or Off |
| ToggleChip | 01 1.5 (Scan on startup) | On and off colours in `scan.toggle_*` |
| GlassChip | 01 1.8 and 2.2 | Genre, VR and variant chips |
| GradientText | 01 1.4 | Text with a mask gradient |
| KickerDot | 01 1.6 | 7 px, pulse 1700 ms |
| DotGrid | 01 1.14 | Static background tile |
| Scrim | 01 1.17 | `#01000000`, click to close |
| CountBadge | 01 1.5 | 20 px minimum, radius 10, Updates colours |
| IconSet | 01 1.4, 1.5, 02 1.7, 02 6 #7 | Gun, wheel, bike, handlebars, generic, headset, bolt, play triangle, magnifier, grid, help dots, chevron (8 x 6, stroke 1.4), reinstall ↻ (U+21BB), play ▶ (U+25B6), update arrow ↓, check, italic "i", external link. The gun, wheel, bike and generic glyphs are new on the 24-unit grid, stroke 1.9, round caps (02 section 6 #7) |

---

## 5. Keyboard, focus and mouse

| Input | Action | Scope | Spec |
|---|---|---|---|
| `/` or Ctrl+K | Focus search | global, not in a text field | [new], 01 2.6 |
| Escape | Close the top overlay; else clear search; else go back | global | 01 1.16 (ORDER, screenshot) |
| Alt+Left or Backspace (not in a field) | Back | global | [new] |
| Mouse XButton1 / XButton2 | Back / forward | desktop | 01 1.16 |
| Enter / Space | Activate focused control | global | |
| Arrows | Move within grids, rows, trees and chip groups | per component | |
| Home / End, Page Up / Page Down | Grid start, end, one screen | GameGrid | [proposal] |
| Enter in search | Go to List with the query | Explore, Detail | 01 1.10 |
| Typing in search on Detail | Back to List with the query | Detail | 01 1.10 |
| Mouse wheel | Closes ORDER, Help and Filters | overlays | 01 1.16 |
| Middle-button drag-scroll | Dropped | | 01 1.16 [DIVERGE] |

- **Focus:** a visible focus ring for every focusable control (cream 2 px, `glass.ring`). Tab order follows the visual order, per page.
- **Gamepad (overlay mode, [proposal]):** Steam Input maps controller buttons to keys. A or trigger: Enter. B: Escape. X: the overflow menu (`menuRequested`). Stick or D-pad: arrows. Shoulder buttons: previous and next section. Menu: Settings.
- **Scan:** the Hub swallows all input during a scan (01 1.16). Not needed here, because the scan runs off the UI thread; Escape asks to cancel instead.

---

## 6. Motion and timing (consolidated)

| What | Value (ms unless noted) | Component | Reduce motion | Source |
|---|---|---|---|---|
| Card hover dwell | 600 | GameCard | keep | 02 1.9 |
| Card preview scale | 1.4 | GameCard | keep (instant) | 02 1.9 |
| Keyboard focus preview | 600 [proposal] | GameCard | keep | section 9 |
| Scroll quiet time | 120 | GameGrid | keep | 02 1.9 |
| Click pulse | 1200 | GameCard | off | 02 1.5, 1.10 |
| Banner Show or Explore defer | 1200 | FeaturedBanner | keep | 01 1.15 |
| Banner sweep | 700, once per hover | FeaturedBanner | off | 01 1.15 |
| Banner close chips | show 5000, hide 800 | FeaturedBanner | keep | 01 1.15 |
| Banner rotation | random 300000 to 900000 | FeaturedBanner | keep | 01 1.6 |
| Kicker dot pulse | 1700 auto-reverse | FeaturedBanner | off | 01 1.15 |
| Search hint cycle | 2600 | SearchHint | off | 01 1.10 |
| STATE pills reveal | 520 after scan | StateChips | off | 01 1.5 |
| Scan attention pulse | 400 x 3 | ScanProgress | off | 01 1.15 |
| Scan-on-startup hide | 450 (Hub only) | removed | | 01 1.5 |
| Spinner loop | 2200 | ScanProgress | keep | 01 1.12 |
| Shimmer sweep and pause | 1500; 12000 to 60000 | ScanProgress | off | 01 1.15 |
| Shimmer opt-out dwell | 5000 (Hub) | replaced in overlay mode | | 01 1.5 |
| Recently Played heading overlay | 5000 show, 800 hide (Hub) | replaced | | 01 1.7 |
| Recently Played tile overlay | 7000 dwell (Hub) | replaced | | 01 1.7 |
| Explore row end glyph | 1200 | ExploreRow | off | 01 1.8 |
| Download progress line | 250 | InstallConsole | keep | 03 1.15 |
| Update check TTL | 6 hours | UpdateChecker | | 03 1.17 |
| Download connect and read timeouts | 30 s, 120 s | InstallService | | 03 1.15 |
| GitHub lookup timeout | 10 s | UpdateChecker | | 03 2.9 |
| Scan heartbeat stale | 60 s | ScanProgress | | 01 1.12 |
| Scan batch interval | 80 | ScanService | | 01 1.12 |
| Toast auto-hide | 4000 [proposal] | Toasts | keep | section 4.16 |
| Menus, header, page changes | 0 (instant) | all | | 01 1.15 |
| Grid reorder | 0 (instant) [open] | GameGrid | | 01 1.11 |

Reduce motion turns off the items marked "off". It does not change any timing that carries information (scan, download, focus).

---

## 7. Spec coverage and v1 exclusions

Implemented in v1: sections 1.1 to 1.17 and 2.1 to 2.12 of spec 01 (with the DIVERGE notes); spec 02 sections 0 to 2 and 3 (art pipeline); spec 03 sections 2.1 to 2.14, with 2.15 excluded.

Not in v1 (owner or later):

| Item | Spec | Reason |
|---|---|---|
| Classic card style and Switch Hub Style | 02 2.6, 01 1.17 | 02 recommends Frosted only |
| Gem marker | 02 2.2, 4 #4 | "drop for v1" in 02 section 5 |
| Add-on and improvement tags | 02 2.2 | no data |
| Dual-install split on hover | 02 2.3 | variant chips replace it (03 2.4) |
| PC Power card | 03 1.7, 2.15 (OPEN) | owner open point; the tier data would be per variant |
| Flat and VR switch | 03 1.6 | theatre is a separate variant (03 2.1) |
| Steam Theatre button | 03 1.6, 1.21 #2 | instructions only (03 2.17 #3); add as static text in Requirements |
| Trailers and video | 03 1.9, 2.3 #12 | no `video` field (C-video) |
| Random banner effects (86 in the Hub) | 01 1.6, 2.3 | out of scope (01 section 1.6) |
| Elden Ring style strip | 03 1.20 | game-specific |
| In-VR game list | frontend.md section 2 (OPEN), D16 | SteamVR library shortcuts only in v1 |
| "Your PC" rating on Explore | 01 4 #7 (OPEN) | VR toggle only |
| Group by hardware | 01 4 #8 | genre sections only |
| Discord block | 03 2.1 | community links only from the upstream README |
| Hidden publishers entry point | 01 2.6 | C-hiddenPublishers |

---

## 8. Gaps the specs do not cover (filled as proposals)

- Settings page content (section 4.15).
- Toasts and error dialog policy (section 4.16).
- Empty states (no results, no games, no recipe for a game) (sections 4.6, 4.11).
- Keyboard and gamepad map (section 5).
- The SteamVR overlay host and its input (section 9).
- Font licence and fallback on Linux (section 2, [open]).
- Grid row model for virtualised sections (05 section 7.5).

---

## 9. VR overlay mode (dashboard use)

The same QML runs as the desktop window and as a SteamVR dashboard overlay, rendered offscreen (`frontend.md` section 6). This section sets the sizes and rules for laser use. All numbers are **[proposal]** and must be measured in the headset before they are locked (section 9.7). Tokens are in `theme.toml` `[vr]` and `[size.hit]`.

### 9.1 Canvas and derivation

- Logical canvas: 1280 x 800 px, shown on an overlay 2.0 m wide (`vr.overlay_width_m`). That gives 6.4 px per cm (1 px = 1.56 mm), and an overlay height of 1.25 m.
- Dashboard distance assumed: 1.5 m (`vr.dashboard_distance_m`). This is an assumption; the user can change it in SteamVR.
- Laser pointing error assumed: 1.0 degree (`vr.laser_jitter_deg`). To measure.
- Minimum target width, angular: about 2 x jitter = 2 degrees. At 1.5 m, 2 degrees is 5.2 cm, which is 33 px on this canvas.
- Targets: 40 px = 6.25 cm = 2.4 degrees (desktop minimum, meets the 2 degree rule with margin). 56 px = 8.75 cm = 3.3 degrees (primary buttons: 1.7 times the minimum).
- Text: 16 px body = 2.5 cm = 0.96 degrees (em height). 14 px labels = 0.84 degrees. Outside the cards, no text is below 14 px in overlay mode. Card text scales with the card (section 9.2).
- Gaps: 16 px between targets. A 1 degree error (17 px on the canvas) aimed at the centre of a 40 px target stays inside its 20 px half-width, so it does not reach the next target.

### 9.2 Sizes (desktop vs overlay mode)

| Element | Desktop (spec) | Overlay mode (proposal) |
|---|---|---|
| Canvas | window, 1120 x 720 default | 1280 x 800 logical |
| Primary button (Start, Install, Update, Find my files, Retry) | about 27 to 35 tall | 56 tall hit area; label 14 to 16 |
| Companion button | 27 to 34 | 44 |
| Card button | 27 | 56 (card L is 228 x 208; check fit, section 9.7) |
| Reinstall pill on card | 28 wide, icon only | 44 tall with text "Reinstall" |
| Header square buttons (library, help) | 34 x 34 | visual 34; hit area 56 |
| Filter, state, facet pills | about 33 | hit 44 |
| S / M / L switch | 30 x 28 | hit 44 |
| Genre and VR chips | 26 to 30 | 44 |
| Explore edge buttons | 30 circle | 44 circle |
| Banner Show and Explore | about 30 tall, 11 to 13 text | 40 tall desktop, 56 overlay; text 14 to 16 |
| Card default size | S | L |
| Card title | 13 | 17 at L (13 x 1.30 from the card scale) |
| Detail body | 12 to 16 | 16 minimum |
| Console text | 12 to 14 mono | 14 minimum |
| Menus (ORDER, Help) | 224 to 230 wide, items about 36 | 300 to 320 wide, items 56 |
| Gap between targets | 6 to 10 | 16 minimum |
| Scrollbar | default | 16 px thumb, always visible |

### 9.3 Layout rules in overlay mode

- The header keeps all elements (no breakpoint hiding at 1280 wide). The title group stays.
- The FilterBar keeps two rows; row 2 wraps to a third if needed rather than shrinking pills.
- Cards at L: 5 per row at 1280 with the 28 px side padding (computed by 05 section 9.5).
- No text outside the cards is smaller than 14 px. Card text follows the card scale (at L the meta line is about 13 px and the author line about 12 px).

### 9.4 Hover-only behaviour and its replacements

Rule: hover may decorate (preview, glow, lift, sweep), but no action and no label that a click needs may appear only on hover. Every hover-only element in 01 to 03 has a non-hover route:

| Hub hover-only element | Source | Route in v1 (desktop) | Route in overlay mode |
|---|---|---|---|
| Card preview after 600 ms | 02 1.9 | hover or focus preview; click opens detail | same; focus preview after 600 ms |
| Card button label "Start in VR" on hover | 02 2.3 | rest label (C-ready) | rest label "Start in VR ▶" |
| Dual-install split on hover | 02 2.3 | variant chips | variant chips |
| Reinstall icon, icon only | 02 1.4, 2.3 | tooltip and accessible name; detail page has the text button | text pill "Reinstall" (44) |
| Banner close chips after 5 s | 01 1.6 | Settings and banner overflow button | banner overflow button "⋯" (always visible) |
| Recently Played heading chips after 5 s | 01 1.7 | heading overflow button | same |
| Recently Played tile overlay after 7 s | 01 1.7 | tile overflow button | same |
| Scan-on-startup toggle (shown on hover) | 01 1.5 | scan overflow menu and Settings | same |
| Shimmer opt-out chips after 5 s | 01 1.5 | scan overflow menu and Settings | same |
| Disabled "Scan first" reason for IN MY LIBRARY | (new) | text under the chip | same |
| Hub tooltips on icon-only buttons | 01 1.4 | tooltip and accessible name | label text in overlay mode where space allows |
| Pill hover grow 1.08 | 03 1.3 | decorative | decorative |
| Header hover grow | 01 1.4 | decorative | decorative |

Overflow buttons are one visible "⋯" control (44 in overlay mode) placed where the hover element used to be. They open a small menu (the SortMenu pattern).

### 9.5 Focus and gamepad

- Focus is always shown (cream ring). Focus moves follow the visual order.
- Focus preview (600 ms) gives a card's detail without a pointer.
- Gamepad map: section 5.

### 9.6 Assumptions to replace with measurements

- Dashboard distance and overlay width (1.5 m, 2.0 m): measured from the user's SteamVR settings.
- Laser jitter (1.0 degree): measured with the owner's controllers in Desktop+ and in a native overlay.
- Text legibility at 16 px body: check on the Steam Frame and Quest 3 paths.

### 9.7 In-headset checks (acceptance for the overlay)

1. Hit a 40 px and a 56 px button 20 times each at 1.5 m. Record misses. Target: 0 misses for 56 px, under 5 % for 40 px.
2. Read a 14 px label and a 16 px body paragraph at 1.5 m. Target: legible without leaning in.
3. Card L grid at 1280 wide: the button and the reinstall pill fit within the card; no clipping by the content area.
4. Walk the whole flow with the laser only: start a game from the library, open detail, and reach Settings. No step needs hover.

---

## 10. Spec conflicts and gaps (unresolved; the value used is stated)

| ID | Conflict or gap | Used here |
|---|---|---|
| C-stack | D4 is "Decided, Qt 6 / QML" (workshop D4, frontend section 6, AGENCY.md). The headers of 01, 02 and 03 say D4 is still open; 01 section 4 #1 recommends Tauri 2; 02 section 4 and 03 section 3 still give WPF and Tauri branches; 01 section 2.8 and 3.2 describe a Rust worker and Rust settings and events. frontend.md says Tauri was dropped | Qt 6 / QML, C++ workers. The Tauri recommendation in 01 section 4 #1 is stale |
| C-theatre | THEATRE colour: 01 section 2.5 chip and 2.10 use `#6fa8ff`; 02 section 2.2 badge border `#5599ee`, text `#7ab5ff` (add-on style); 03 section 2.7 `#7ab5ff` | Badge text `#7ab5ff`, border `#5599ee`; Explore chip `#6fa8ff`. Owner to choose one |
| C-minwin | Minimum window: 500 x 400 (01 section 2.1, "same as the Hub") vs 720 x 480 (01 section 4 #10) | 500 x 400, with 720 x 480 as the recommendation [open] |
| C-graphics-drawer | Graphics facet: schema section 3 lists it; the brief asks for GraphicsChips; 01 section 2.2 drawer omits it | Added as a section of FiltersDrawer |
| C-ready | Card rest label "Ready" with hover swap to "Start in VR" (02 section 2.3, DIVERGE, 02 section 5 #7 recommends it) vs detail rest "Start in VR" (03 section 1.5, 2.7). Hover-only information breaks section 9 | Desktop: 02 recommendation. Overlay mode: rest label "Start in VR ▶" [open] |
| C-close-scan | Closing during a scan: 01 section 2.8 "asks to cancel" vs 01 section 2.12 rule 5 "cannot be closed away mid-run" | Ask to cancel (2.8) [open] |
| C-order | 03 section 0.2 order omits the variant picker; 03 section 2.3 places it after the meta strip; 0.2 merges About and Similar | 03 section 2.3 numbering |
| C-token-names | 03 section 2.7 `--card #16161a` is labelled "Card base"; 02 section 1.5 card base is `#0c0c10`; `#16161a` is the pill and glass base | `surface.card` `#0c0c10`, `surface.pill` `#16161a` |
| C-notation | 02 section 4.3 writes one colour as `#16161adc` (RRGGBBAA). Qt and the Hub use `#aarrggbb` | `#db16161a` in `theme.toml` |
| C-base-colour | Card base: `hub.colour` (02 section 2.2, recommended) vs fixed `#0c0c10` (Hub) (02 section 5 #1, owner call) | `hub.colour` per game, fallback `#0c0c10` [open] |
| C-explore-top | 01 section 1.8: title block top margin 68 vs S/M/L table 62 / 70 / 82 | 70 at M [open] |
| C-settings | No spec for the Settings page content | Proposal (section 4.15) [open] |
| C-empty | No spec for empty states | Proposal (sections 4.6, 4.11) |
| C-toasts | No spec for toasts or error dialogs | Proposal (section 4.16) |
| C-hover-only | Hover-only actions and labels in 01 to 03 (section 9.4). The Hub's own rule (03 section 1.21 #10) says to keep state visible at rest | Replaced as in section 9.4 |
| C-stale | 02 section 6.1-6.2 and 03 section 2.16 #1-3 say game-packages and frontend.md still contain old forms. They no longer do | No action; those claims should be removed from the specs |
| C-scanfolders | `scanFolders` in `hub-state.json` (01 section 2.9) vs config (01 section 4 #13) | Config [open] (also 05 section 12) |
| C-animations | Grid reorder: Hub is instant (01 section 1.11) | Instant [open] |

Also see 05 section 12 for data-side conflicts (C-planned, C-badges, C-wip, C-recipes, C-variant-status, C-states, C-featured, C-video, C-steamid, C-series, C-new, C-subgenre, C-controls, C-art, C-shortnames, C-steps, C-needs, C-hiddenPublishers).
