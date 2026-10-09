# 01. Window shell, header and filters: the PCVR Mods Installer Hub look, adapted for AladdinsCastle

> **Status note (2026-10-08):** the Hub stack is decided: **Qt 6 / QML** (workshop D4). Sections in this spec that discuss WPF, PowerShell or Tauri implementations are historical. The QML mapping is in [04-qml-components.md](04-qml-components.md).

> **File references:** paths starting `Core/`, `Start PCVR Mods Hub.bat` or `_screenshots/` are relative to [Mr-Nlce/PCVR-Mods-Installer-Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) at commit `a64401f` (MIT). Paths starting `docs/`, `data/`, `games/` are in this repo.

Status: research spec, 2026-10-08. Owner decision D19 (UI look "very close to the Installer Hub") is recorded in `docs/workshop.md`. D4 (Hub technology) is still open, so part 3 covers both PowerShell + WPF and a web UI (Tauri 2).

**Reference checked:** PCVR-Mods-Installer-Hub at commit `a64401f` (2026-10-06), local clone ``. Every file reference below is a full absolute path, as the global rules require. A bare `:N` or `:N-M` refers to the file named in the same row or in the section's Source line, which is also a full path. Source version is `0.8.7.6` (`Core/VRModHub.ps1:178`). The screenshot `_screenshots/hub-main.png` shows `v0.8.2` and a "Scan games" button, so it predates the source. This spec follows the source. Licence of the reference: MIT (`LICENSE`).

**Method:** read the source and the screenshot. The Hub was not run. Values are read from code. Where a height or position is only estimated, it is marked "est.".

**Scope:** window shell, startup, header, filter bar, featured banner, section headers, Explore (overview) page, search, sort, scan flow and spinner, persisted settings, tokens, timings, keyboard. Game cards are in the sibling `docs/ui/02-game-cards.md`. The detail page, install flow and the banner effect catalogue are not covered here.

---

## 0. The shell rules in brief

1. One window, three rows: header, filter bar, scrolling content. Default 1120 x 720, minimum 500 x 400, centred, geometry remembered.
2. Header: headset glyph, title, version pill, tagline on the left; grid (Library) button, help button, ORDER pill and search pill on the right. No header border; the filter bar owns the divider.
3. Filter bar: a back arrow, then TYPE pills, then STATE pills (hidden until the first scan), then the scan counter button, then S / M / L on the far right. Active = cream ring, not fill.
4. Featured banner on top of the list: 140 px tall, art on the right under a dark fade, a per-game colour wash, kicker, title, subtitle, and two buttons ("Show", "Explore all games").
5. The banner rotates every 5 to 15 minutes at random. Hovering it for 5 s offers "Close" or "Always disable".
6. Section headers are pills: icon + section name + kind + count, then a wrapping grid of cards, then a 1 px divider.
7. Explore is a separate page: its own banner, a GENRE chip row, a PC POWER chip row, then horizontal genre rows with edge arrows.
8. Search is live (no Enter), has a "-term" exclusion syntax, and a rotating example hint under the box.
9. ORDER reorders the existing cards on every page; it never rebuilds them.
10. Scan is a single button. While it runs, a neon light travels round its border. Results are a counter ("47 on PC | 42 VR Ready").

---

## 1. How the Mod Hub does it

### 1.1 Files that define the shell

| File (full path) | Role |
|---|---|
| `Start PCVR Mods Hub.bat` | Launcher: runs the splash, then starts two hidden background jobs. |
| `Core/Show-StartupSplash.ps1` | Console splash: progress bar and hint box while the Hub starts. |
| `Core/VRModHub.ps1` | Entry point: version, module load order (`:887-908`). |
| `Core/Modules/Window.Layout.ps1` | The main XAML (`:5-1531`) and responsive header code (`:1541-1594`). |
| `Core/Modules/Window.Controls.ps1` | S / M / L sizes, view switching, help menu, Recently Played overlays. |
| `Core/Modules/Window.BannerEffects.ps1` | Title gradient, header hover grow, banner effect dispatch and rotation. |
| `Core/Modules/BannerColors.ps1` | Hand-picked colour per game (about 280 entries). |
| `Core/Modules/BannerOvFilters.ps1` | Banner hover-close, Explore chips, Explore genre rows. |
| `Core/Modules/OverviewPage.ps1` | Featured-game picker, banner gradient and kicker, Explore banner. |
| `Core/Modules/Filter.Controls.ps1` | TYPE and STATE pills, filter predicate, `Apply-Filter`. |
| `Core/Modules/Filter.Banners.ps1` | Banner buttons, hover and press, deferred navigation. |
| `Core/Modules/Filter.Search.ps1` | Search box, hint, examples, hidden-modder hint, Enter key. |
| `Core/Modules/Filter.Scan.ps1` | Scan start, sources, counter, reveal timer. |
| `Core/Modules/ScanSpinner.ps1` | Neon light around the scan button (own STA thread). |
| `Core/Modules/DiscoverInit.ps1` | Library toggle, back arrow, mouse back / forward buttons. |
| `Core/Modules/CatalogSort.ps1` | ORDER menu and sort modes. |
| `Core/Modules/Startup.ps1` | Ready signal, startup scan, banner rotation timer, window close. |
| `Core/Modules/HubState.ps1` | Persisted state: location, recovery copy, legacy file. |
| `Core/.hub-settings.json` | Legacy user settings file (`playHistory`, `sizeLibrary`). |

### 1.2 Startup sequence

Source: `Start PCVR Mods Hub.bat`, `Core/Show-StartupSplash.ps1`, `Core/Modules/VRModHub.ps1`, `Core/Modules/Startup.ps1`.

1. The launcher runs the splash and waits for it (`Start PCVR Mods Hub.bat:9-12`). Only after the Hub is visibly ready does it start two hidden jobs, `Core/Update-Hub.ps1 -Silent` and `Core/Prefetch-Versions.ps1` (`:19-23`). The comment says they must not compete with the cold start.
2. The splash deletes the stale ready flag (`Core/Show-StartupSplash.ps1:30`), then starts the Hub minimised (`:40`). It owns the launch, so there is no race.
3. The splash is a **console window**, not WPF. It draws:
   - Banner: "PCVR  MODS  INSTALLER  HUB" between solid double rules, magenta rules and white title (`:261-270`).
   - Progress bar: 40 cells, `●` filled and `·` empty, green, with "loading" and a spinner glyph (`:332`, `:427-432`).
   - Estimate: the last measured load time from `%TEMP%\PCVRHub_lastload.txt`, default 12.0 s, accepted only when 2 to 15 s (`:52-58`).
   - Easing: 92% at the estimate, then a tail to 99%. It only reaches 100% when the ready flag appears (`:60-82`, `:507`).
   - Hint: a random line in a 54-column box, typed out 1.0 s after start at 30 ms per character (`:288`, `:330`, `:443`).
   - Stars: 9 twinkling glyphs (`·`, `.`, `+`, `*`) placed only in the margins, never over text (`:339-411`).
   - Redraw tick: 90 ms (`:494`).
4. The Hub writes the ready flag in `ContentRendered`. That handler activates the window, briefly sets Topmost to pull focus, writes the measured load time and writes `PCVRHub_ready.flag` (`Core/Modules/Startup.ps1:326-356`).
5. After first paint, in this order: library warm-up at `ApplicationIdle` (`Core/Modules/Startup.ps1:363-370`); the opt-in startup scan at `Background` priority so it never blocks the window (`:376-393`); the banner rotation timer (`:623-631`).

### 1.3 Window and responsive layout

Source: `Core/Modules/Window.Layout.ps1`.

```
+----------------------------------------------------------------------------------------------+
|  HEADER  bg #0d0d0f  pad 28/20/28/14                                 (:24)                   |
+----------------------------------------------------------------------------------------------+
|  FILTER BAR  bg #0d0d0f  pad 28/0/28/12  bottom border 1 px #1a1a22  (:272-273)              |
+----------------------------------------------------------------------------------------------+
|  CONTENT  bg #0f0f12 with 24 px dot grid  scroll  pad 28/20/28/24  (:682-710)                |
|     featured banner, Recently Played, section headers and grids, dividers                    |
+----------------------------------------------------------------------------------------------+
   Overlays (ORDER menu :1323, help menu :1433) sit in the root grid across all three rows.
```

- Window: title "PCVR Mods Installer Hub", `Width 1120 Height 720`, `MinWidth 500 MinHeight 400`, `CenterScreen`, resizable, background `#0f0f12` (`:5-12`).
- Icon: the headset glyph is drawn to a 32 x 32 bitmap and used as the window icon (`:1704-1742`). The process gets its own AppUserModelID so the taskbar does not group it under PowerShell (`:1677-1697`).
- Geometry restore happens at `SourceInitialized` (`:1606-1663`). `winWidth` and `winHeight` apply only if at least 500 and 400. `winLeft` and `winTop` apply only if the point starts on a screen (left at least X - 50, top at least Y, left below X + W - 100, top below Y + H - 50). `winMaximized` restores the maximised state. Geometry is saved in `Add_Closing` (`Core/Modules/Startup.ps1:539`).
- Responsive breakpoints (`:1541-1594`, function `Update-HubHeaderResponsiveLayout`):

| Window width | Header change |
|---|---|
| 780 px and up | Full caption "PCVR Mods Installer Hub"; version pill and tagline visible; ORDER pill 144 px; search column 165 px; header padding 28. |
| below 780 px | Caption becomes "PCVR Hub"; version pill and tagline hidden. |
| below 620 px | Title group and headset glyph hidden; "ORDER" prefix hidden; ORDER pill 112 px; search column 115 px; header padding 14. |

### 1.4 Header

Source: `Core/Modules/Window.Layout.ps1` unless noted.

```
+----------------------------------------------------------------------------------------------+
| (headset) PCVR Mods Installer Hub [v0.8.7.6]          [grid]  [more]  [ORDER Alphabetical v]  |
|           Install n!ce VR mods for your PC games                     [ Search          ]     |
+----------------------------------------------------------------------------------------------+
```

| Element | Value and behaviour | Lines |
|---|---|---|
| Header background and padding | `#0d0d0f`, padding `28,20,28,14` (narrow: `14,20,14,14`). No bottom border. | `:24`, `:1581-1585` |
| Headset glyph | 32 x 17 canvas, stroke `#dd6600` 1.5, two 3 px eyes `#dd6600`, glow `#dd6600` blur 9 opacity 0.6. Click toggles the card style (see 1.17). | `:36-45`, `Core/Modules/Window.Controls.ps1:1265` |
| Title | "PCVR Mods Installer Hub", 22 px Bold, Segoe UI. Fill is a top-to-bottom gradient `#ffffff` to `#d8dee3`. A glow layer below it (same text, `#3a8add`, DropShadow blur 16, opacity 0.45). | `:50-61`; gradient in `Core/Modules/Window.BannerEffects.ps1:14-27` |
| Version pill | Text `v{HUB_VERSION}`, 10 px `#555568` on `#1e1e2a`, radius 4, padding 6,2, margin 10,4,0,0. | `:63-69`; `Core/Modules/Window.BannerEffects.ps1:2560` |
| Tagline | "Install n!ce VR mods for your PC games", 11 px Medium `#555568`, left margin 42 (aligns under the title, after the glyph). | `:71-74` |
| Header hover grow | Instant scale on hover: version pill x1.10, headset x1.15, title x1.04, tagline x1.04. | `Core/Modules/Window.BannerEffects.ps1:2566-2583` |
| Update banner | Hidden unless an update is known. Background `#1a2e1a` (hover `#234a23`), border 1.2 px `#5fe08a`, glow `#4ade80` blur 13 opacity 0.85. Green dot `#8bff8b` 7 px; "Update X available" 11.5 SemiBold `#c8ffc8` then " - Click to update" `#8bff8b`. Click runs the updater and closes the Hub. | `:82-101`; `Core/Modules/Window.BannerEffects.ps1:2595-2620` |
| Library (grid) button | 34 x 34, radius 8, background `#16161a`, border 1 px `#3a3a48`, right margin 10. Icon: four 14 px squares in `#aaaaaa`. Tooltip "Open library / discover view". | `:116-149` |
| Library button, active | Background `#26262e`; border is a gradient from `#f47a1e` (top-left) to `#5566aa` (bottom-right). | `Core/Modules/DiscoverInit.ps1:560-574` |
| Library button, idle | Background `#16161a`; border gradient from `#3c4777` to `#ab5515` (the active colours at 70%). | `Core/Modules/DiscoverInit.ps1:576-595` |
| Help button | 34 x 34, same chrome as the library button, three 3.5 px `#aaaaaa` dots with a 2.5 px gap. Opens the help panel (1.17). | `:156-169` |
| ORDER pill | 144 x 34 (112 narrow), radius 6, background `#16161a`, border 1 px `#2a2a35` (active `#3a8add`). Text: "ORDER" 9 px SemiBold `#3a8add`, then the mode name 11 px `#d8dee3` with ellipsis. Chevron `#888899` 8 x 6, stroke 1.4, rotates 180 degrees while the menu is open. Applies to every game list (see 1.11). | `:180-204`; `Core/Modules/CatalogSort.ps1:248-263` |
| Search pill | Column 165 px (115 narrow). Radius 6, background `#16161a`, border 1 px `#2a2a35`. The TextBox fills the whole pill: 12 px white text, padding 10,8, so a click anywhere focuses it. Placeholder "Search", 11 px `#555568`, left margin 11. | `:205-261` |
| Search hint | 10 px `#555568` line that hangs below the pill (bottom margin -19, height 16). It is on its own layer, so showing it never moves the header. | `:230-237` |
| Top scan slot | An empty slot left of the buttons, margin-left 44 (0 when narrow). Reserved for an indicator; nothing in the screenshot uses it. | `:103`, `:1587-1593`; `Core/Modules/Filter.Scan.ps1:20` |

### 1.5 Filter bar

Source: `Core/Modules/Window.Layout.ps1`, `Core/Modules/Filter.Controls.ps1`, `Core/Modules/Filter.Scan.ps1`.

```
+----------------------------------------------------------------------------------------------+
| [<] TYPE [ All ] [ (motion) Motion Controls ] [ (pad) Gamepad ]  STATE [Needs Mod][VR Ready][Updates]  [ Scan installed games ]  [S][M][L] |
+----------------------------------------------------------------------------------------------+
```

The bar is one row. The back arrow and pills are on the left, and S / M / L docked far right.

**Back arrow** (`:327-336`): 24 x 24, radius 6, background `#09ffffff`, border `#12ffffff`, right margin 18 (so "All" starts under the "P" of the title). Chevron stroke 2 px, grey `#555560`. On Detail and Explore it turns to `#f2f2f5` as the in-page back button scrolls out of view (`Core/Modules/DiscoverInit.ps1:219-277`).

**Labels:** "TYPE" and "STATE", 11 px SemiBold `#6f6f7a` (`Core/Modules/Window.Layout.ps1:337`, `:412`).

**Pill chrome** (the same for all TYPE, STATE and Scan pills):

| Property | Value | Lines |
|---|---|---|
| Padding, radius, border | 15,9; radius 6; 1 px border | `:347-349` |
| Font | 13 px SemiBold, Segoe UI | `:350`, `:376`, `:397` |
| Inactive | background `#000000` (opaque glass base), border `#0fffffff`, text `#c7c7d0` (set in code; the XAML says `#aaaaaa`) | `Core/Modules/Filter.Controls.ps1:59-78`, `:161-184` |
| Active | border `#ffeeb0` (cream), text white. A 2 px outer ring `#80ffeeb0` at margin -1, radius 7, sits behind the button. A second crisp ring `#ffeeb0` sits on top. | `:344-357`; `Core/Modules/Filter.Controls.ps1:98-102` |
| Hover (inactive) | Background brightened one step, text brightened. No accent border, so hover and active stay distinct. | `Core/Modules/Filter.Controls.ps1:22-36` |
| Spacing | 7 px between TYPE pills; 6 px between STATE pills; 7 px before the scan button. | `:343`, `:426`, `:445` |
| Icons | Motion: 14 px glyph stroke `#44cc66`. Gamepad: 18 px glyph stroke `#dd6600`. | `:373-375`, `:394-396` |

**TYPE** (single select): All, Motion Controls, Gamepad. Default All (`Core/Modules/Filter.Controls.ps1:38`). Click runs `Set-FilterStyle` then `Apply-Filter` (`:742-744`).

**STATE** (`:412-470`): three pills, each hidden until the first scan. STATE is a tri-state: off, installed, ready, update. "Installed" in the UI is the Needs Mod set (base game on PC, mod missing). "VR Ready" is ready plus update. "Updates" is a subset of VR Ready (`Core/Modules/Filter.Controls.ps1:652-677`). Pills reveal 520 ms after a scan finishes (`Core/Modules/Filter.Scan.ps1:2250-2271`) and then stay visible (`Core/Modules/Filter.Controls.ps1:486-492`). The Updates pill shows a count badge: 20 px minimum, radius 10, background `#1F60A5FA`, text `#8FB6DD` 12 px SemiBold (`Core/Modules/Window.Layout.ps1:459-466`).

**Important rule:** until a scan has run, STATE filters do nothing. They show everything instead of an empty list (`Core/Modules/Filter.Controls.ps1:652-657`). The scan button pulses to point the user at the scan (`:758`).

**Scan button** (`:499-611`): the counter button. Full values are in 1.12. Key points:
- Pre-scan label: "Scan installed games", 12 px SemiBold `#e8f5ec`, with a magnifier glyph 13 px. Background `#034ade80` (almost transparent), border 2 px `#b35fff8f`, radius 6.
- Post-scan counter: `[N]` 13 px ExtraBold `#5fff8f`, " on PC" 12 px `#e8f5ec`, a separator "  |  " `#5aa880` at 40% opacity, a green triangle `#5fff8f` 7 x 8, `[N]` 13 px ExtraBold `#5fff8f`, " VR Ready" 12 px `#fcefb0`. Zero VR Ready hides the second half (`Core/Modules/Filter.Scan.ps1:2018-2036`).
- Shimmer: an 80 px band with gradient `#005fff8f`, `#605fff8f` (centre), `#005fff8f`, sweeping across the button. Visible only after a result. See 1.15 for timings.
- Shimmer opt-out appears after 5 s of dwell on the counter (post-scan only). Two chips: "Disable shimmer" (this session) and "Always Disable" (saved as `shimmerDisabled`). Chip style: `#1e1e2a`, border `#3a3a48`, radius 3, text 10 px `#aaaaaa` (`Core/Modules/Window.Layout.ps1:614-651`; `Core/Modules/Filter.Search.ps1:495-498`).

**Scan on Startup** (`:655-675`): a small toggle that appears only while hovering the scan button, and hides 450 ms after the pointer leaves (`Core/Modules/Startup.ps1:28-94`). Off: background `#000000`, border `#0fffffff`, text `#7e8a85`. On: background `#0e4ade80`, border `#5aa880`, text `#aaccbb`, check mark `#5aa880` 2 px. Text is 13 px SemiBold. It is hidden while a scan runs.

**S / M / L** (`:275-308`, `Core/Modules/Window.Controls.ps1:94-131`): three 30 x 28 buttons, radius 6, gap 5, background `#09ffffff`, border `#0fffffff`, text 12 px Bold `#aaaaaa`. Active: border `#5566aa`, text `#ffffff`. Each view keeps its own size (1.14 has the values).

### 1.6 Featured banner (list and library)

Source: `Core/Modules/Window.Layout.ps1` (list banner `:715-820`, library banner `:1022-1125`), `Core/Modules/OverviewPage.ps1`, `Core/Modules/Filter.Banners.ps1`, `Core/Modules/Window.Controls.ps1`.

```
+------------------------------------------------------------------------------------------+
| (*) FEATURED VR MOD  -  GAMEPAD                                     |  art (right,      |
| Tinykin VR                                                          |  Stretch=Uniform) |
| Platformer  .  Puzzle  .  Collectathon                              |                   |
| [ Show ]   [ Explore all games > ]                 [Close][Always disable]  (5 s hover)  |
+------------------------------------------------------------------------------------------+
   height 140 px (S), radius 8, 1 px border #2a2a35, bottom margin 22 px.
```

| Element | Value | Lines |
|---|---|---|
| Frame | Height 140 (S), 156 (M), 174 (L). Radius 8. Background `#0f0f15`, border 1 px `#2a2a35`. Clips to bounds. | `Core/Modules/Window.Layout.ps1:715-718`; `Core/Modules/Window.Controls.ps1:353-358`, `:199-204` |
| Art | Image, `Stretch=Uniform`, aligned right and vertically centred, so the full art shows. | `:723-724` |
| Fade over art | Left-to-right gradient: `#F00F0F15` at 0, `#C00F0F15` at 0.35, `#000F0F15` at 0.75. Keeps the text readable. | `:725-734` |
| Text block | Margin 22, 16, 22, 16 (top margin 16 / 20 / 24 at S / M / L). | `:736`; `Core/Modules/Window.Controls.ps1:216` |
| Kicker | 7 px dot, then "FEATURED VR MOD" + "  -  " + control label, 10 px SemiBold. Colour follows control type: motion `#44cc66`, gamepad `#dd6600`, both `#8888ff`. Free games append "FREE" in `#34d399`. | `:744-751`; `Core/Modules/OverviewPage.ps1:701-750` |
| Title | 22 px SemiBold white with the same gradient, max width 380. If too wide, whole trailing words are dropped (no ellipsis mid-word). | `:753-757`; `Core/Modules/OverviewPage.ps1:434-468`; gradient `Core/Modules/Window.BannerEffects.ps1:28-29` |
| Subtitle | 11 px `#bbbbbb`, top margin 4, max width 380, ellipsis. Genre tags joined with "  .  " (middle dot), SemiBold. Max 3 tags, or 2 when an improvement or injector tag is also shown. Only words from a fixed genre whitelist show (`Core/Modules/OverviewPage.ps1:475-539`). | `:758-763` |
| Show button | Transparent, radius 4, border 2 px `#bfa845`, text 11 Bold `#bfa845`, padding 14,7. Hover: diagonal white sweep (1.15). Press: border `#f0d860`. Click: opens the detail page after a 1200 ms defer, so the press glow is visible. | `:768-775`; `Core/Modules/Filter.Banners.ps1:213-226`, `:148-154`, `:164-190` |
| Explore button | Transparent, radius 4, border 1.5 px `#dd6600`, text 11 Bold `#dd6600` with a 13 px "  >" arrow. Press: border `#ffcc66`. Click: opens Explore after 1200 ms. | `:776-788`; `Core/Modules/Filter.Banners.ps1:228-238` |
| Hover (banner frame) | Scale 1.02 (instant), border `#4a4a55`. Press on the art or title: scale 1.02, border `#ff8822` 2 px. Idle: `#2a2a35`. The hover is wired to the outer frame, not the art, to stop jitter (see 3.3). | `Core/Modules/Filter.Banners.ps1:255-285` |
| Hover-close overlay | After 5 s of hover on the banner, two chips appear top right: "Close" (hides for this session) and "Always disable" (saves `bannerListDisabled` or `bannerLibDisabled`). Chips: background `#1a1a22`, border `#3a3a48`, radius 3, text 10 px `#cccccc`. Hides 800 ms after leaving. | `:796-818`; `Core/Modules/BannerOvFilters.ps1:8-65` |

**Rotation.** A DispatcherTimer swaps the list banner and the library banner every 5 to 15 minutes, chosen at random and re-armed each tick. Each swap picks a new game and a new random effect (`Core/Modules/Startup.ps1:623-631`; `Core/Modules/Window.BannerEffects.ps1:2273-2290`). The Explore banner does not rotate; its Shuffle button does the same job.

**Featured pick** (`Core/Modules/OverviewPage.ps1:322-373`):
- Pool: owned games (motion and gamepad lists) plus external entries. Two titles are blocked, UEVR and Pragmata VR (`:327`).
- The current banner game is removed from the pool, if that leaves anything (`:350-359`).
- Split: 70% motion pool, 30% gamepad pool. A game with "BOTH" controls counts as motion. A game with unknown controls counts as motion (`:338-347`, `:362-372`).
- Genre-aware variant for Explore chips: the pool is limited to games matching that genre's tag list (`:382-431`).

**Colour wash.** Each game has a hand-picked colour (`Core/Modules/BannerColors.ps1`). The lookup key is `name:<header base>` first, then `steam:<id>` (`Core/Modules/Helpers.ps1:3606-3622`). The colour is turned into a left-to-right HSV gradient (`Core/Modules/OverviewPage.ps1:636-694`):

| Stop | Position | Value (HSV V) | Hue handling | Notes |
|---|---|---|---|---|
| deep | 0.00 and 0.30 | 28 | hue pulled 65% toward amber if 35 to 78 | dark ground under the title |
| mid | 0.66 | 78 | hue pulled 50% toward amber if 35 to 78 | gentle tint |
| amber | 1.00 | 115 | hue pulled 20% toward amber if 35 to 78 | hidden behind the art |

Saturation is capped at 0.85. The amber pull stops yellow-gold from looking olive. If there is no colour, the banner is flat `#0f0f15` (`:695-697`).

**Art chain** (`Core/Modules/OverviewPage.ps1:589-625`): local image cache first, then the catalog header, then two Steam header CDNs, then portrait images, then a second portrait CDN. Retries happen in the background. The banner keeps its colour wash if no art loads.

**Motion effects.** Each banner also gets one random animated effect from a pool of 86 (`Core/Modules/Window.BannerEffects.ps1:2296`), plus genre extras (`:2466-2502`). Effects are out of scope here. The control dot before the kicker pulses on a 1700 ms auto-reverse loop, with scale and glow in opposite phase (`:2508-2556`).

### 1.7 Section headers and grid

Source: `Core/Modules/Window.Layout.ps1`.

```
+------------------------------------------------------------------------------------------+
| (bolt) Custom Installers  (plug) Motion Controls  80 mods                                |  <- pill, 13 px title
|                                                                                          |
|  [card]  [card]  [card]  [card]  [card]          <- WrapPanel, centred, not stretched    |
|                                                                                          |
| ----------------------------------------------------------------------- (1 px #1e1e26)   |
| (bolt) Custom Installers  (pad) Gamepad controls  n mods                                 |
|  [card] ...                                                                              |
+------------------------------------------------------------------------------------------+
```

| Element | Value | Lines |
|---|---|---|
| Pill | Radius 11, background `#0dffffff`, border 1 px `#22ffffff`, padding 10,6. Icon 14 px, then title 13 SemiBold white, kind 13 Medium in its colour, count 11 Medium `#7a7a86` at left margin 9. | `:890-909`, `:920-938`, `:954-975` |
| Icon colours | Motion `#44cc66`; Gamepad `#dd6600`; External `#6fa8ff`. The bolt icon is a 14 px stroke path (`M13,2 L6.5,13 ...`). | `:895-897`, `:925-927`, `:959-961` |
| Header spacing | First section header top margin 20, bottom 10. Grid below has top margin 22, bottom 30. | `:890`, `:913-914` |
| Divider | 1 px `#1e1e26`, bottom margin 24, after each group. | `:917`, `:951` |
| Count text | "80 mods" (set from the catalog). | `:907` |
| Recently Played | Above the list (after the featured banner). Header pill like above: teal `#3fb6c8` play triangle 11 x 14, title 13 SemiBold, "to launch in VR" 11 Medium `#c7a13a` with a 20 px left margin. Up to 8 tiles, centred. Hidden when empty or disabled. | `:822-887` |
| Recently Played overlays | Hover the heading for 5 s: "Close" and "Always disable" chips appear on the right. Hover one tile for 7 s: its management overlay appears. | `Core/Modules/Window.Controls.ps1:1015-1030`, `:929-939` |
| Section order | Recently Played, then Custom Installers (Motion), then Gamepad, then External. | `:831-978` |

Section size scale: header titles 12 / 14 / 17 px and sub text 10 / 11 / 13 px at S / M / L (`Core/Modules/Window.Controls.ps1:236-255`).

### 1.8 Explore (overview) page

Source: `Core/Modules/Window.Layout.ps1:1145-1317`, `Core/Modules/BannerOvFilters.ps1`, `Core/Modules/OverviewPage.ps1:8-24`, `Core/Modules/Window.Controls.ps1:319-322`, `:393-430`.

```
+------------------------------------------------------------------------------------------+
| [< Back to library]                                                                       |  banner 200 px (M)
|   (*) FEATURED PICK                                                                       |
|   Title (24 px)                                                                           |
|   subtitle                                                                                |
|   [ View this mod ]  [ Shuffle ]                                    art on right          |
+------------------------------------------------------------------------------------------+
| [GENRE]                                                                                   |
| [All] [Horror & survival] [Action & shooter] ... [Free] [New]                             |
| [PC POWER . Your PC v]                                                                    |
| [All] [Low] [Solid] [High] [Extreme]  [Rate my GPU]                                       |
| Horror & survival (14 games)                                                              |
| <|  [tile] [tile] [tile] [tile] [tile] ...                                         |>    |
+------------------------------------------------------------------------------------------+
```

| Element | Value | Lines |
|---|---|---|
| Banner | Height 200 (M), radius 8, bottom margin 18, background `#0f0f15`, border `#2a2a35`. Gradient is eight stops from `#F00F0F15` at 0 to `#000F0F15` at 0.85 (`#E6`, `#CC`, `#A6`, `#7A`, `#4E`, `#26` in between), so the art fades more gradually than on the list banner. | `:1155-1177` |
| Back button | Left 14, top 14. Padding 11,6,15,6, radius 6, border 1 px `#3a3a48`, text "Back to library" 11 SemiBold `#cccccc`, chevron stroke 1.8 `#cccccc`. Hover border `#dd6600`. The label changes to "Back to mod list" when the page was opened from the list. | `:1181-1201`; `Core/Modules/Filter.Banners.ps1:346-353` |
| Title block | Margin 22, 68, 22, 16 (the top margin clears the back button). Kicker "FEATURED PICK" (or "BROWSING FREE GAMES" / "BROWSING NEW VR MODS" for those chips). Title 24 SemiBold, max width 400. Subtitle 11 `#bbbbbb`, top margin 5. | `:1203-1233`; `Core/Modules/OverviewPage.ps1:857-907` |
| Buttons | "View this mod" (gold, same style as Show) and "Shuffle" (border 1.5 px `#dd6600`, text `#dd6600`). Padding 14,7 and 12,7. Shuffle picks a new game in the active genre and a new effect. | `:1235-1253`; `Core/Modules/Filter.Banners.ps1:366-381` |
| Explore S / M / L | Banner height 184 / 200 / 224. Title 21 / 24 / 27. Subtitle 11 / 11 / 12.5. Kicker 9 / 10 / 11. Top margin 62 / 70 / 82. Button font 11 / 11 / 12. | `Core/Modules/Window.Controls.ps1:422-425` |
| GENRE label | Box: background `#0d0d12`, border 1 px `#22222e`, radius 5, padding 8,5. Text 10 SemiBold `#666677`. | `:1264-1272` |
| Genre chips | Chip per genre, then FREE and NEW. Each chip: background `#16161a` (inactive) or `#2a1a08` (active), border `#2a2a35` or the accent colour, radius 5 (S, M) or 6 (L), left accent bar 4 / 4 / 5 px in the accent colour, label 11 / 12 / 12.5 SemiBold, min height 26 / 28 / 30, margin 0,0,8,8. | `Core/Modules/BannerOvFilters.ps1:72-110`, `:433-478` |
| Genre accents | ALL `#dd6600`, HORROR `#cc3344`, ACTION `#dd6600`, ADVENTURE `#44aa88`, RPG `#aa66dd`, COOP `#44aadd`, PUZZLE `#dd9922`, SIM `#88aa44`, FREE `#34D399`, NEW `#38BDF8`. | `Core/Modules/BannerOvFilters.ps1:228-239` |
| PC POWER toggle | Label "PC POWER", separator ` . `, mode name ("Your PC" or "Exact tier") 11 Bold `#ffaa66`, chevron. Click toggles cumulative ("Your PC") and exact. Same GENRE-style box. | `:1286-1311` |
| Power chips | All `#888888`, Low `#66cc66`, Solid `#ddcc44`, High `#dd6644`, Extreme `#dd3333`. Four buckets map from six tiers. A "Rate my GPU" control sits at the end of the row. | `Core/Modules/OverviewPage.ps1:19-24`; `Core/Modules/BannerOvFilters.ps1:480-516` |
| Genre rows | One row per genre bucket (7 in the Hub), each game in any row whose tags match. A row shows the header, then tiles. | `Core/Modules/OverviewPage.ps1:8-16`; `Core/Modules/BannerOvFilters.ps1:696-1000` |
| Row header | Title 22 Bold, same white-to-grey gradient. Count bubble: "N games", 10 SemiBold `#9a9aae`, on `#1e1e28`, border `#2c2c3a`, radius 8, padding 7,1, left margin 10, scales x1.12 on hover. Header bottom margin 10; row bottom margin 26. | `Core/Modules/BannerOvFilters.ps1:699-745` |
| Row edges | Edge hosts 56 px wide with a gradient fade. A 30 px circle button (background `#1e1e2a`, border `#3a3a48`) with a 16 px bold `#dd6600` chevron. At the end of the row the right button becomes a clockwise arrow glyph for 1200 ms, then resets. | `Core/Modules/BannerOvFilters.ps1:791-905` |
| Row duplicate | If the row overflows, the tiles are duplicated once so the strip reads as a loop. | `Core/Modules/BannerOvFilters.ps1:783-788` |
| Tile sizes | S 140 x 210, M 175 x 260, L 215 x 320 (2:3). | `Core/Modules/Window.Controls.ps1:319-322` |

### 1.9 Library view (portrait grid)

Source: `Core/Modules/Window.Layout.ps1:989-1132`, `Core/Modules/Window.Controls.ps1:311-390`.

- Own host that sits over the list. Toggled by the grid button in the header. Its own featured banner (`LibBanner`), built the same way as 1.6, with heights 140 / 156 / 174.
- Tiles: S 220 x 330, M 250 x 375, L 275 x 413 (`Core/Modules/Window.Controls.ps1:311-314`). Ratio 2:3 throughout.
- Layout: a WrapPanel, centred. No section pills.
- Each view keeps its own S / M / L value (`sizeLibrary`, `sizeExplore`, `sizeDetail`, `scaleList`).

### 1.10 Search

Source: `Core/Modules/Filter.Search.ps1`, `Core/Modules/Filter.Controls.ps1:560-740`.

- **Live filter.** `TextChanged` runs `Apply-Filter` on every keystroke, with no debounce (`Core/Modules/Filter.Search.ps1:307-338`). Enter is only needed on Explore or Detail (below).
- **Filter scope.** Every card and tile is filtered, not only the visible list, so switching views never shows a stale search (`Core/Modules/Filter.Controls.ps1:688-740`).
- **Text match.** Title, mod name, pill, author and tags (`Core/Modules/Filter.Controls.ps1:632-634`).
- **Exclusion.** A term starting with "-" removes matches. It matches the modder or author, mod, pill and genre tags, but not the title, so "-real" does not remove every game with "real" in its name. Several can be chained, and they combine with a normal term. Two-word names are supported ("-luke ross") (`:571-607`, `:615-628`).
- **Re-include.** "+name" lifts a hidden modder for one search (`:586-602`).
- **Hidden modders.** Names saved in `hiddenModders` act like a standing "-name". The search hint offers to hide a modder permanently or to bring one back (`Core/Modules/Filter.Search.ps1:17-30`, `:182-200`).
- **Keyword shortcuts** (`:641-647`): "free" (free titles), "wip" (WIP titles), "gem" or "gems" (gem-marked titles), "new" (titles added in the last 10.5 days, exclusively), "roomscale" or "room scale" (roomscale flag).
- **Enter** on Explore or Detail returns to the Library with the query applied (`Core/Modules/Filter.Search.ps1:346-372`).
- **Typing while a detail page is open** closes the detail page and shows the filtered library (`:315-328`).
- **Placeholder** "Search" is hidden once the box has text (`:331-336`).
- **Example hint.** While focused and empty, the placeholder cycles through six examples every 2600 ms ("e.g. cyberpunk", "e.g. praydog", "e.g. roomscale", "e.g. free", "e.g. -horror", "e.g. -praydog") (`:155-169`).
- **Focus rule.** The hidden-modder checkbox is `Focusable=False`. A focusable checkbox takes keyboard focus on mouse-down, the hint collapses, and the click never lands (`Core/Modules/Window.Layout.ps1:240-251`).

### 1.11 Sort (ORDER)

Source: `Core/Modules/CatalogSort.ps1`, `Core/Modules/Window.Layout.ps1:1323-1374`.

| Mode (key) | Label on the pill | Menu caption | Ordering |
|---|---|---|---|
| `hub` (default) | Alphabetical | "A-Z, with game series in order" | Catalogue order (`CatalogOrder`), ascending (`:47-71`) |
| `release` | VR mod release | "Newest verified release first" | Mod release date, newest first; undated last (`:72-78`) |
| `added` | Added to Hub | "Newest Hub additions first" | Date added, newest first (`:24-42`) |

- Persisted as `catalogSort` (`:10-13`, `:232`).
- Menu: 230 px wide, background `#16161a`, border `#3a3a48`, radius 8, padding 6, top margin 60, right margin 203. Title "SORT EVERY GAME LIST" 10 px. Each choice is a row with a 13 px label `#d8dee3`, an 11 px caption `#6a6a7e`, and an 8 px active dot `#3a8add`. Row hover background `#1e1e2a` (`:1326-1374`; `Core/Modules/CatalogSort.ps1:310`).
- Close: click the scrim, use the wheel, or press Escape (`Core/Modules/CatalogSort.ps1:287-299`, `:335-337`).
- Applies to every list by re-attaching the same card objects in a new order. Nothing is rebuilt, so install states, events and image caches stay intact (`Core/Modules/CatalogSort.ps1:92-147`).

### 1.12 Scan games flow and spinner

Source: `Core/Modules/Filter.Scan.ps1`, `Core/Modules/Filter.ScanSources.ps1`, `Core/Modules/ScanSpinner.ps1`, `Core/Modules/Filter.Search.ps1`.

**Steps:**

1. **Click** starts the spinner before the scan is scheduled (`Core/Modules/Filter.Scan.ps1:111-115`).
2. **Guard.** A re-entrancy flag stops a second scan. A heartbeat older than 60 s counts as a dead scan, and the flag is cleared (`:82-89`).
3. **Label** changes to "Scanning..." in `#ffcc44`, 14 px. A second magnifier appears on the right. The counter and shimmer are hidden (`:127-146`).
4. **Lock.** Window hit-testing is turned off and all keys and text input are swallowed until the scan ends (`Core/Modules/Filter.ScanSources.ps1:1650-1665`).
5. **Yield.** The scan pumps the dispatcher about every 80 ms, so the window repaints (`Core/Modules/Filter.Scan.ps1:103-108`).
6. **Sources:**
   - Steam: library folders from the registry and `libraryfolders.vdf` (`Core/Modules/Filter.Scan.ps1:186-187`; the VDF read is `Core/Modules/Helpers.ps1:345`).
   - GOG Galaxy: install roots from the registry and default folders (`:197-215`).
   - Epic: default install roots (`:225-237`).
   - Per-title fallback paths and markers, including Xbox build folders (`Core/Modules/Filter.Scan.ps1:838-846`).
   - Each game is checked for its base install and its mod marker (`Core/Modules/Filter.ScanSources.ps1:192`, `:238`).
7. **Result.** The counter is shown, and the separator and VR count are hidden if there are zero VR Ready. The counter's green halo is set to blur 12, opacity 0.22 (a comment there describes a stronger target, 18 and 0.55; the code value is used). The shimmer is allowed, and the STATE pills reveal after 520 ms (`Core/Modules/Filter.Scan.ps1:2004-2070`, `:2250-2271`).
8. **Close.** Closing the window during a scan is deferred until the scan ends, unless the heartbeat is stale (`Core/Modules/Filter.Scan.ps1:2357-2362`).

**Spinner** (`Core/Modules/ScanSpinner.ps1`):
- Visual: a rounded rectangle (radius 6) stroke that carries two lit segments, each 30% of half the perimeter. The dash offset is animated, so the light travels round. Halo: thickness 3.2 x core, opacity 0.40, Gaussian blur radius 9. Band: thickness 2, opacity 0.95, blur radius 2. Colour `#6BF49B`. One loop takes 2.2 s (`:132-201`, `:243`).
- Own STA thread and HostVisual, so the light keeps moving while the UI thread is busy with the scan (`:9-16`, `:93-107`). Resize follows the button's size changes (`:262-274`).
- Windows PowerShell 5.1 only. On PowerShell 7 the spinner is skipped and the scan still runs (`:18-30`).

### 1.13 Persisted settings

Source: `Core/Modules/HubState.ps1`, `Core/Modules/Window.Controls.ps1`, `Core/Modules/Window.Layout.ps1`.

- **Primary store:** `%LOCALAPPDATA%\PCVR Mods Installer Hub\State\HubState.json` (`Core/Modules/HubState.ps1:38-50`).
- **Recovery copy:** `Core/UserData` beside the Hub. A newer generation is promoted back when LocalAppData is writable (`:4-9`, `:53-57`, `:598-617`).
- **Legacy file:** `Core/.hub-settings.json` is still read on first use (`Core/Modules/HubState.ps1:981`).
- **Keys seen in the source** (the defaults shown are the code's defaults):

| Key | Default | Meaning | Lines |
|---|---|---|---|
| `winWidth`, `winHeight`, `winLeft`, `winTop`, `winMaximized` | none | Window geometry | `Core/Modules/Window.Layout.ps1:1611-1617` |
| `sizeLibrary` | `L` | Library tile size | `Core/Modules/Window.Controls.ps1:293-297` |
| `sizeExplore` | `M` | Explore tile size | `:298` |
| `sizeDetail` | `M` | Detail text size | `:299` |
| `scaleList` | `1.0` | List card scale (1.0, 1.5, 2.0) | `:302-308` |
| `catalogSort` | `hub` | ORDER mode | `Core/Modules/CatalogSort.ps1:11` |
| `hubStyle` | `frosted` | Card style | `Core/Modules/Window.Controls.ps1:169-170` |
| `checkOnStartup` | `false` | Scan on Startup | `Core/Modules/Startup.ps1:28-36` |
| `shimmerDisabled` | off | Counter shimmer | `Core/Modules/Filter.Search.ps1:495-498` |
| `bannerListDisabled`, `bannerLibDisabled` | `false` | Banner hidden | `Core/Modules/Filter.Banners.ps1:405-406` |
| `hiddenModders` | `[]` | Standing exclusions | `Core/Modules/Filter.Search.ps1:17-30` |
| `recentlyPlayedHidden`, `recentlyPlayedCollapsed` | `false` | Recently Played display | `Core/Modules/Window.Controls.ps1:690`, `:774`, `:1063` |
| `playHistory` | list | Recently Played games (legacy file) | `Core/.hub-settings.json` |
| `startView` | `LIST` | Start view (`LIST` or `LIBRARY`) | `Core/Modules/Startup.ps1:492`; `Core/Modules/BannerOvFilters.ps1:1509`, `:1527` |
| `desktopShortcut` | off | Shortcut on the desktop | `Core/Modules/Helpers.ps1:3045-3110` |

- **Booleans must be real JSON booleans.** A string "False" is truthy in PowerShell, so the reader parses text explicitly (`Core/Modules/Startup.ps1:23-35`).
- **Writes are batched during a scan** (`Start-HubStateBatch`, `Core/Modules/Filter.Scan.ps1:94`).

### 1.14 Tokens: colour, font, size, radius

**Colour.** All hex values are as written in the source.

| Token | Value | Used for | Lines |
|---|---|---|---|
| window background | `#0f0f12` | window, content base | `Core/Modules/Window.Layout.ps1:12`, `:696` |
| header and filter bar | `#0d0d0f` | header and filter bar | `:24`, `:272` |
| filter divider | `#1a1a22` | filter bar bottom border | `:273` |
| glass base | `#16161a` | search pill, ORDER pill, menu buttons, chips | `:116`, `:156`, `:180`, `:205` |
| glass tint | `#09ffffff` | S / M / L and back button background | `:284`, `:327` |
| glass hairline | `#0fffffff` | inactive pill border | `Core/Modules/Filter.Controls.ps1:60` |
| opaque pill | `#000000` | filter pill background | `Core/Modules/Window.Layout.ps1:349`, `:364` |
| strong border | `#2a2a35` | search and ORDER idle border, banner border | `:182`, `:206`, `:717` |
| button border | `#3a3a48` | library and help buttons, overlay panels | `:118`, `:158` |
| dot grid | `#222230` on `#0f0f12` | content background, 24 px tile, dots radius 0.9 | `:690-705` |
| selected ring | `#ffeeb0`, halo `#80ffeeb0` | active pill ring | `Core/Modules/Filter.Controls.ps1:61`, `:98` |
| text primary | `#ffffff` | titles, active pill text | `Core/Modules/Window.Layout.ps1:61` |
| text inactive | `#c7c7d0` | inactive pill text (set in code) | `Core/Modules/Filter.Controls.ps1:77` |
| text muted | `#aaaaaa` | S / M / L text, Motion and Gamepad default | `Core/Modules/Window.Layout.ps1:289`, `Core/Modules/Window.Controls.ps1:112` |
| text dim | `#6f6f7a` | TYPE and STATE labels | `:337`, `:412` |
| text faint | `#555568` | tagline, version, placeholder, search hint | `:67`, `:72`, `:219`, `:235` |
| text grey | `#7a7a86` | section counts | `:908` |
| brand orange | `#dd6600` | headset glyph, gamepad icon, Explore button, GP kicker, Shuffle | `:41`, `:394`, `:1247` |
| brand blue | `#3a8add` | title glow, ORDER prefix, active ORDER, order dot | `:52`, `:193`, `:1343` |
| motion green | `#44cc66` | motion icon, motion section | `:374`, `:895` |
| gold | `#bfa845` (press `#f0d860`) | Show button border and text | `Core/Modules/Filter.Banners.ps1:223`, `:225` |
| found green | `#5fff8f` | counter numbers, shimmer colour | `:583`, `:536` |
| scan text | `#e8f5ec` | scan button text | `:562` |
| scan border | `#b35fff8f` | scan button 2 px border | `:501` |
| scan fill | `#034ade80` | scan button background | `:502` |
| scanning amber | `#ffcc44` | "Scanning..." and magnifier | `Core/Modules/Filter.Scan.ps1:139`, `:573` |
| ready text | `#fcefb0` | " VR Ready" label | `:607` |
| spinner | `#6BF49B` | neon light | `Core/Modules/ScanSpinner.ps1:243` |
| updates badge | `#1F60A5FA` fill, `#8FB6DD` text | Updates count | `:461`, `:464` |
| discover on | gradient `#f47a1e` to `#5566aa`, fill `#26262e` | library button active | `Core/Modules/DiscoverInit.ps1:560-574` |
| discover off | gradient `#3c4777` to `#ab5515`, fill `#16161a` | library button idle | `Core/Modules/DiscoverInit.ps1:576-595` |
| active S / M / L border | `#5566aa` | S / M / L active | `Core/Modules/Window.Controls.ps1:129` |
| external blue | `#6fa8ff` | External section | `Core/Modules/Window.Layout.ps1:960` |
| recent teal | `#3fb6c8` | Recently Played icon | `:840` |
| recent gold | `#c7a13a` | "to launch in VR" | `:851` |
| hover item | `#1e1e2a` | menu item hover | `Core/Modules/CatalogSort.ps1:310` |
| overlay panel | `#16161a`, border `#3a3a48`, radius 8 | ORDER and help menus | `Core/Modules/Window.Layout.ps1:1326-1327`, `:1436` |
| banner frame | `#0f0f15` | banner background | `:716` |
| banner hover | `#4a4a55` (press `#ff8822`) | banner border states | `Core/Modules/Filter.Banners.ps1:262`, `:268` |
| kicker orange | `#dd6600` | banner kicker (gamepad) | `Core/Modules/OverviewPage.ps1:703` |
| explore labels | `#666677` on `#0d0d12`, border `#22222e` | GENRE and PC POWER labels | `Core/Modules/Window.Layout.ps1:1267-1270` |
| power mode | `#ffaa66` | PC POWER mode label | `:1302` |
| count bubble | `#1e1e28`, border `#2c2c3a`, text `#9a9aae` | genre row count | `Core/Modules/BannerOvFilters.ps1:727-738` |
| title gradient | `#ffffff` to `#d8dee3` | titles | `Core/Modules/Window.BannerEffects.ps1:21-22` |
| update banner | `#1a2e1a`, border `#5fe08a`, glow `#4ade80`, text `#c8ffc8` / `#8bff8b` | update banner | `Core/Modules/Window.Layout.ps1:82-98` |
| help item sub-text | `#6a6a7e` | menu captions | `:1452` |
| discord | `#5865F2` | help menu dot | `:1475` |

**Font.** Segoe UI everywhere (97 uses in the shell XAML and about 130 in code). Exceptions: `Segoe UI Symbol` (3 uses) and one `Consolas` use.

| Element | Size | Weight | Lines |
|---|---|---|---|
| Title | 22 | Bold | `Core/Modules/Window.Layout.ps1:50-61` |
| Version | 10 | Regular | `:66-68` |
| Tagline | 11 | Medium | `:71-73` |
| ORDER prefix | 9 | SemiBold | `:192-193` |
| ORDER label | 11 | Regular | `:195-196` |
| Search text / placeholder / hint | 12 / 11 / 10 | Regular | `:215`, `:219`, `:235` |
| TYPE / STATE labels | 11 | SemiBold | `:337`, `:412` |
| Filter pill | 13 | SemiBold | `:350`, `:376`, `:397`, `:421`, `:431`, `:450` |
| S / M / L | 12 | Bold | `:288` |
| Scan text | 12 | SemiBold | `:561` |
| Scan counter numbers | 13 | ExtraBold | `:582` |
| Scan "Scanning..." | 14 | Regular | `Core/Modules/Filter.Scan.ps1:144` |
| Banner kicker | 10 | SemiBold | `:750` |
| Banner title (list) | 22 (25 / 28 at M / L) | SemiBold | `:754`; `Core/Modules/Window.Controls.ps1:355-356` |
| Banner subtitle | 11 (12 / 13) | Regular | `:759` |
| Banner buttons | 11 (12 / 13) Bold, arrow 13 | Bold | `:774`, `:783` |
| Section title | 13 (14 / 17) | SemiBold | `:899` |
| Section kind | 13 (14 / 17) | Medium | `:905` |
| Section count | 11 (11 / 13) | Medium | `:908-909` |
| Genre row title | 22 | Bold | `Core/Modules/BannerOvFilters.ps1:709-710` |
| Genre row count | 10 | SemiBold | `:736-737` |
| Explore labels | 10 | SemiBold | `Core/Modules/Window.Layout.ps1:1269` |
| Menu headings | 10 | (default) | `:1331`, `:1441` |
| Menu item | 13 (title) / 11 (caption) | Regular | `:1339-1340` |

**Sizes and spacing.**

| Item | Value |
|---|---|
| Window default / minimum | 1120 x 720 / 500 x 400 |
| Header padding | 28,20,28,14 (narrow 14,20,14,14) |
| Filter bar padding | 28,0,28,12 |
| Content padding | 28,20,28,24 |
| Header square buttons | 34 x 34, radius 8, gap 10 |
| ORDER pill | 144 x 34 (narrow 112), radius 6, gap 10 |
| Search column | 165 (narrow 115) |
| Filter pill | padding 15,9; radius 6; gap 7 |
| S / M / L | 30 x 28, gap 5, radius 6 |
| Back button | 24 x 24, radius 6, right margin 18 |
| List banner height | 140 / 156 / 174 (S / M / L) |
| Explore banner height | 184 / 200 / 224 (S / M / L) |
| Banner radius and bottom margin | 8; 22 (list and library), 18 (Explore) |
| Banner button padding | 14,7 (S); 16,8 (M); 18,9 (L); radius 4 |
| Section pill | radius 11, padding 10,6 |
| Section grid margins | top 22, bottom 30 |
| Section divider | 1 px, bottom margin 24 |
| Genre row bottom margin | 26; row title bottom margin 10 |
| Library tile | 220 x 330 / 250 x 375 / 275 x 413 |
| Explore tile | 140 x 210 / 175 x 260 / 215 x 320 |
| List card scale | 1.00 / 1.15 / 1.30 (`Core/Modules/Window.Controls.ps1:141-158`) |
| Detail text | 12 / 14 / 16 px, line height 18 / 21 / 24 (`Core/Modules/Window.Controls.ps1:327-331`) |

**Radius.** 3 for small chips; 4 for banner buttons; 5 for genre labels and genre chips (S, M); 6 for filter pills, back button, S / M / L, ORDER and search pill; 7 for focus rings; 8 for the header buttons, the banner frame and overlay panels; 11 for section pills; 10 for the Updates count badge.

### 1.15 Timings

| What | Value | Source |
|---|---|---|
| Splash redraw tick | 90 ms | `Core/Show-StartupSplash.ps1:494` |
| Splash hint delay, then typing | 1.0 s, then 30 ms per character | `:330`, `:443` |
| Splash estimate | default 12 s, accepted 2 to 15 s | `:52-58` |
| Ready signal | at `ContentRendered` | `Core/Modules/Startup.ps1:326-356` |
| Library warm-up | `ApplicationIdle` priority | `Core/Modules/Startup.ps1:363-370` |
| Startup scan | `Background` priority, after first paint | `Core/Modules/Startup.ps1:376-393` |
| Banner rotation | random 5 to 15 min, re-armed each tick | `Core/Modules/Startup.ps1:623-631` |
| Banner button defer (Show, Explore) | 1200 ms | `Core/Modules/Filter.Banners.ps1:218`, `:231`, `:300`, `:312`, `:357` |
| Banner hover sweep | 700 ms, once per hover | `Core/Modules/Filter.Banners.ps1:183` |
| Banner hover-close | show after 5 s; hide 800 ms after leave | `Core/Modules/BannerOvFilters.ps1:18-21` |
| Banner dot pulse | 1700 ms auto-reverse, forever | `Core/Modules/Window.BannerEffects.ps1:2531-2551` |
| Search example rotation | 2600 ms | `Core/Modules/Filter.Search.ps1:157-158` |
| Search filtering | no debounce | `Core/Modules/Filter.Search.ps1:307` |
| STATE pill reveal after scan | 520 ms | `Core/Modules/Filter.Scan.ps1:2251` |
| VR Ready partner hide | 650 ms | `Core/Modules/Filter.Controls.ps1:839` |
| Scan attention pulse | timer step 400 ms, 3 pulses over about 2.4 s | `Core/Modules/Filter.Controls.ps1:758-765` |
| Scan on Startup toggle hide | 450 ms | `Core/Modules/Startup.ps1:83` |
| Scan yield | about 80 ms | `Core/Modules/Filter.Scan.ps1:103` |
| Scan heartbeat stale | 60 s | `Core/Modules/Filter.Scan.ps1:82-85` |
| Scan spinner loop | 2.2 s | `Core/Modules/ScanSpinner.ps1:243` |
| Counter shimmer sweep | 1.5 s | `Core/Modules/Filter.Search.ps1:442` |
| Counter shimmer pause | random 12 to 60 s | `Core/Modules/Filter.Search.ps1:456` |
| Counter shimmer opt-out chips | after 5 s dwell | `Core/Modules/Filter.Search.ps1:495-498` |
| Recently Played heading overlay | 5 s show; 800 ms hide | `Core/Modules/Window.Controls.ps1:1023-1026` |
| Recently Played tile overlay | 7 s dwell | `Core/Modules/Window.Controls.ps1:929-930` |
| Explore row wrap glyph | 1200 ms | `Core/Modules/BannerOvFilters.ps1:899-900` |
| Header button and menu | no animation (instant) | `Core/Modules/Window.Controls.ps1:1121-1134` |

### 1.16 Keyboard and mouse

The Hub has very few keyboard shortcuts. Most actions are mouse driven.

| Input | Action | Lines |
|---|---|---|
| Typing in search | Live filter | `Core/Modules/Filter.Search.ps1:307` |
| Enter in search, on Explore or Detail | Return to the Library with the query | `Core/Modules/Filter.Search.ps1:346-372` |
| Typing in search while a detail page is open | Close the detail page, show the filtered library | `Core/Modules/Filter.Search.ps1:315-328` |
| Escape, ORDER menu open | Close it | `Core/Modules/CatalogSort.ps1:335-337` |
| Escape, screenshot viewer | Close it | `Core/Modules/DetailView.Content.ps1:1203-1208` |
| Mouse XButton1 (side back) | Back: detail to overview or library; overview to library | `Core/Modules/DiscoverInit.ps1:372-…` |
| Mouse XButton2 (side forward) | Replay the page left by XButton1 | `Core/Modules/DiscoverInit.ps1:365-370` |
| Mouse wheel over ORDER or help menu | Close it | `Core/Modules/CatalogSort.ps1:297-299`; `Core/Modules/Window.Controls.ps1:1163` |
| Middle-button drag on a scroll view | Drag-scroll | `Core/Modules/DiscoverInit.ps1:51`, `:102-127` |
| Any key or text during a scan | Swallowed | `Core/Modules/Filter.ScanSources.ps1:1650-1665` |
| Close (X, Alt+F4) during a scan | Deferred until the scan ends | `Core/Modules/Filter.Scan.ps1:2357-2362` |

### 1.17 Help and feedback panel

Source: `Core/Modules/Window.Layout.ps1:1433-1531`, `Core/Modules/Window.Controls.ps1:1111-1265`.

- Opened by the help button. A transparent scrim (`#01000000`) covers the window; clicking outside, or the wheel, closes it (`:1435`, `Core/Modules/Window.Controls.ps1:1163`).
- Panel: 224 wide, top margin 60, right margin 200, background `#16161a`, radius 8, padding 6 (`:1436-1440`).
- Heading "HELP & FEEDBACK", 10 px.
- Items (13 px label, 11 px caption `#6a6a7e`, 8 px coloured dot, radius 7, padding 8,9):
  - "Suggest a VR mod" (orange dot): opens a short GitHub form.
  - "Logs & report a problem" (`#d8923a`): opens the latest log and a report form.
  - "Join our Discord" (`#5865F2`): invite link.
  - "Switch Hub Style" (`#3a8add`): toggles frosted and classic cards.
  - "Desktop Shortcut" (`#4ac07a`): caption "Currently on - click to turn off" or the opposite, read from state.
- Opening the help panel closes the ORDER menu (`Core/Modules/Window.Controls.ps1:1121-1124`).

---

## 2. AladdinsCastle equivalent

Same layout, same tokens, same behaviour. Changes are tagged **[DIVERGE]** (a deliberate change from the Hub) and **[NEW]** (no Hub equivalent). Field names come from `docs/game-schema.md`. Card details are in `docs/ui/02-game-cards.md`.

### 2.1 Window and header

```
+----------------------------------------------------------------------------------------------+
| (headset) AladdinsCastle [v{APP_VERSION}]               [grid]  [more]  [ORDER Title v]        |
|           3D light gun & racing games in true VR                  [ Search          ]         |
+----------------------------------------------------------------------------------------------+
```

| Element | AladdinsCastle value | Note |
|---|---|---|
| Window title and caption | "AladdinsCastle" | Narrow caption: keep "AladdinsCastle" (short enough). |
| Default size, minimum | 1120 x 720; 500 x 400 | Same as the Hub. Owner may raise the minimum (see 4). |
| Icon | Headset glyph | Same glyph, VR. A lamp or castle mark is an option. |
| Tagline | "3D light gun & racing games in true VR" | Same style. |
| Version pill | `v{APP_VERSION}` | Read from the app version, not a constant in the UI. |
| Library button | "Open library / discover view" | Toggles the portrait grid (2.4). |
| Help button | "Help & feedback" | Panel as 1.17. Item list: see section 4, item 11. |
| ORDER pill | Labels: Title, Year, Manufacturer, Hardware, Recently played | Modes in 2.7. |
| Search pill | "Search" | Syntax in 2.6. |
| Update banner | Same | Shown only if a new release exists. |
| Responsive rules | Same breakpoints (780, 620) | Same header behaviour. |
| Header hover grow | Same factors | Same. |

The Hub's "Switch Hub Style" (frosted and classic) is **[DIVERGE]**: ship frosted only (sibling `docs/ui/02-game-cards.md`, section 2.6). The help menu item stays only if classic is kept.

### 2.2 Filter bar: two rows, plus a Filters drawer

The Hub's bar is one row. Its width is about 1,064 px of content at a 1120 px window. Adding seven facets to it does not fit. So the bar splits in two. Row 1 keeps the Hub's look and order. Row 2 is a facet strip that the Hub does not have. **[DIVERGE]**

```
+----------------------------------------------------------------------------------------------+
| [<]  GENRE  [All] [Light gun] [Racing]      [Filters  2 v]    [Scan my files]      [S][M][L] |  row 1
|      STATE  [To install] [Ready] [Updates]                     IN MY LIBRARY (o)           |  row 2
|      Active: Manufacturer: Namco x   Year 1995-2001 x   Hardware: Sega Model 3 x   Clear all |
+----------------------------------------------------------------------------------------------+
```

**Row 1 (Hub order and chrome):**

| Element | Value |
|---|---|
| Back arrow | As 1.5: grey on Library; white on Detail and Explore. |
| GENRE label | "GENRE" 11 SemiBold `#6f6f7a` (replaces TYPE). |
| Genre pills | "All", "Light gun", "Racing" in the Hub's pill chrome (1.5). Default All. Icon on Light gun and Racing: 14 to 18 px, stroke in the genre colour (2.11 tokens). Active ring cream `#ffeeb0`. |
| Filters button | Pill with the same chrome. Label "Filters". A count badge after the label shows how many facets are active (the Updates badge style, 20 px, radius 10). Opens the drawer below. |
| Scan button | "Scan my files" (replaces "Scan installed games"). Chrome and states as 2.8. |
| S / M / L | Unchanged (1.5). |

**Row 2 (facet strip):**

| Element | Value |
|---|---|
| STATE label and pills | "To install", "Ready", "Updates". Hidden until the first scan, as the Hub. Mapping in 2.2 table below. |
| IN MY LIBRARY | Toggle chip, same style as the Scan on Startup toggle (1.5). On: only games whose media was found. Disabled with a tooltip "Scan my files first" until a scan has run. |
| Active facet chips | One chip per active facet, removable (x). Chip style: the filter pill chrome, smaller (13 px to 12 px, padding 10,6). |
| Clear all | 11 px `#6a6a7e`, right-aligned. Hidden when no facet is active. |

**Filters drawer** (opened from Filters; anchored under the button; same panel chrome as the ORDER menu: background `#16161a`, border `#3a3a48`, radius 8, padding 6; width 380 to 420). **[NEW]**

Sections, top to bottom. Each section label is a GENRE-style box (10 px `#666677` on `#0d0d12`).

| Section | Control | Values (from the vocabulary) |
|---|---|---|
| HARDWARE | Tree with expanders (chevron as the ORDER chevron) | Arcade > Sega (Model 1, Model 2, Model 3, ST-V, NAOMI, NAOMI 2, Lindbergh, ...), Namco (System 11, 12, 22, Super System 22, 23, 246, ...), Konami, Taito, Midway, Atari, PC arcade, Other. Console > Sony (PS1, PS2, PS3), Sega (Saturn, Dreamcast), Nintendo (N64, GameCube, Wii, Wii U, Switch), Microsoft (Xbox, Xbox 360). PC > Windows. Counts in `#7a7a86` 11 px. Source: `data/vocab/hardware.toml`. |
| MANUFACTURER | Multi-select chips | Sega, Namco / Bandai Namco, Konami, Taito, Capcom, Midway, Atari Games, Raw Thrills, Play Mechanix, Global VR, Incredible Technologies, Jaleco, Gaelco, Sammy, Tecmo / Koei Tecmo, Nintendo, Sony, Capcom / Sega, Ecole, Eidos, EA, Activision, Ubisoft, Infogrames / Atari SA, Acclaim, Other. Source: `data/vocab/manufacturers.toml`. |
| YEAR | Range slider (track `#2a2a35`, fill `#dd6600`, thumb 12 px `#ffeeb0`) and decade chips | Range bounds from the data (earliest to latest year). Decade chips: 1970s to 2020s. |
| VR | Multi-select chips | True 3D (`routes.vr.best = true3d`), Theatre (`theatre`), Planned (`true3d-planned`). Colours in 2.11. |
| PLAYERS | Segmented control | 1 / 2+ (`players`). |
| CONTROLS | Multi-select chips | Gun, Wheel, Handlebars, Bike, Ski, Joystick, Yoke, Boat, Other (`controls.type`). |

Footer: "Clear" (left) and "Done" (right) buttons. Filters apply live, as search does. Close by clicking the scrim or pressing Escape.

Rules:
- AND across facets, OR within a facet (for example Namco OR Sega, AND Model 3).
- A facet that is inactive does not filter.
- A malformed entry fails open: it stays visible and the error is logged. The Hub does the same (`Core/Modules/Filter.Controls.ps1:680-685`).

**STATE mapping [DIVERGE]:** the Hub's labels describe mods. AladdinsCastle's describe setups. Use the game-packages states (sibling `docs/ui/02-game-cards.md`, section 6, item 2).

| Hub pill | Meaning in the Hub | AladdinsCastle pill | Meaning here | Filter rule |
|---|---|---|---|---|
| Needs Mod | Base game found, mod not installed | To install | Media found, setup not installed | `status = installed` (game-packages: "Ready to install") |
| VR Ready | Mod installed and ready | Ready | Setup installed and verified | `status in {ready, update}` |
| Updates | Newer mod exists | Updates | Installed setup has a newer upstream release | `status = update` |
| (none) | | (open, see 4) Needs files | Media missing | `status = needs-files` (optional 4th pill) |

### 2.3 Featured banner

| Element | AladdinsCastle value |
|---|---|
| Frame, art, fade, size | As 1.6. Art from `games/<id>/art/banner.png`, then `marquee.png`, then a generated title card in the accent colours (docs/frontend.md section 5). Art is `Stretch=Uniform`, right-aligned. |
| Kicker | "FEATURED GAME  -  " followed by the VR label: TRUE 3D, THEATRE or PLANNED. Dot and kicker colour = `[hub].accent` (fallback: genre colour). **[DIVERGE]** The Hub's kicker shows control type; ours shows VR quality, because the quality is the main decision a player makes. |
| Title | `title`. Word-trim rule as the Hub. |
| Subtitle | Up to 3 subgenre labels from `data/vocab/genres.toml` joined with "  .  ", then the year. Example: "Rail shooter  .  Cover shooter (pedal)  .  1995". **[DIVERGE]** Replaces the Hub's genre whitelist. |
| Wash | `[hub].accent` through the HSV gradient of 1.6. Fallback: genre colour (2.11). |
| Show button | "Show": opens the detail page (spec 03). Same gold chrome and 1200 ms defer. |
| Explore button | "Explore all games >": opens Explore (2.5). Same orange chrome. |
| Hover-close | Same overlay and timings. "Always disable" saves `bannerListDisabled`. |
| Rotation | Every 5 to 15 min, List and Library banners. |
| Featured pool | Games with `routes.vr.best` true3d or theatre, not `hub.featured = false` **[NEW]**, not the current game, 50 / 50 Light gun and Racing **[NEW]** (the Hub's 70 / 30 is by control type; ours is by genre, owner call in 4), and games whose media was found first if any exist (the Hub prefers owned). |
| Random effect | Same effect system. Pool and genre extras are for a separate doc. |

### 2.4 Section headers and grid

Sections follow genre, as the sibling card spec decides (`docs/ui/02-game-cards.md`, section 2.5).

```
+----------------------------------------------------------------------------------------------+
| (bolt) Light guns   (gun) Light gun   14 games                                              |  <- pill; icon and kind in the genre colour
|  [card] [card] [card] [card] [card]                                                          |
| ----------------------------------------------------------------------------------------------- |
| (bolt) Racing       (wheel) Racing    9 games                                               |
|  [card] ...                                                                                  |
+----------------------------------------------------------------------------------------------+
```

- Pill: same chrome as 1.7. Title "Light guns" or "Racing" in white (the Hub's "Custom Installers"). Kind label is the genre ("Light gun" or "Racing") in the genre colour. Count "N games" in `#7a7a86`.
- Order: Recently Played, then Light guns, divider, Racing, divider. Theatre and planned entries stay in their genre section, with the VR badge on the card (sibling spec, section 2.5).
- Library view (portrait grid, 1.9) has no sections; it is one WrapPanel, sorted by ORDER.
- Option (owner call in 4): a "Group by hardware" mode puts one section per hardware family, using the same pill.

### 2.5 Explore page

Same layout as 1.8. Changes:

**Genre buckets** replace the Hub's seven genre rows. Each bucket is a set of subgenres from `data/vocab/genres.toml`. A game joins every bucket its subgenres match **[NEW]**.

| Bucket (row title) | Subgenres | Colour (proposed) |
|---|---|---|
| Rail shooters | rail-shooter | `#e0302a` |
| Cover and sniper | cover-shooter, sniper | `#dd6600` |
| Horror | horror | `#cc3344` |
| Hunting, party and water | hunting, machine-gun, party, water-gun | `#dd9922` |
| Circuit and street | circuit, street | `#f0a020` |
| Rally and off-road | rally, truck | `#88aa44` |
| Kart and futuristic | kart, futuristic, vehicle-combat | `#aa66dd` |
| Bikes, boats and more | motorcycle, bicycle, boat, ski, mission-driving, flying | `#44aadd` |

- Chips: All, then the eight buckets, then "New" (titles added in the last 10.5 days) if kept (4). FREE is dropped (no free arcade titles) **[DIVERGE]**.
- The PC POWER toggle is replaced by a **VR** toggle: chips All (`#888888`), True 3D (`#34d399`), Theatre (`#6fa8ff`), Planned (`#9a9aa8`). The toggle label reads "VR" with the active choice. **[DIVERGE]** Power tiers do not map to arcade setups. A "Your PC" rating is an open question (4).
- Banner: same as 1.8. Kicker "FEATURED PICK", button "View this game" (replaces "View this mod"), and "Shuffle" (picks within the active bucket).

### 2.6 Search

Live, as the Hub. Same matching pipeline, different fields.

**Searched fields:** title, `alt_titles`, `series`, `developer`, manufacturer label, hardware label, subgenre labels, `[hub].blurb`, and `[hub].badges`.

**Syntax:**

| Input | Effect |
|---|---|
| `time crisis` | Text match. |
| `-theatre` | Exclude the VR quality "theatre". |
| `-namco` | Exclude a manufacturer (matched by id and label). |
| `-racing` | Exclude a genre (matched on genre and subgenre labels, not the title). |
| `+name` | Re-include something excluded in the standing list. |
| `gun`, `racing` | Genre keywords (same as the GENRE pills). |
| `true3d`, `theatre`, `planned` | VR quality keywords. |
| `new` | Titles added in the last 10.5 days (exclusive). |
| `roomscale` | Roomscale flag in `[hub].badges`. |

Exclusions never match the title (same rule as the Hub). A malformed term never hides a game (fail open, as the Hub).

**Hint line** under the box, rotating every 2600 ms when focused and empty: "e.g. time crisis", "e.g. daytona", "e.g. namco", "e.g. -theatre", "e.g. sega -racing", "e.g. rally". Same position and size as the Hub's hint (1.4).

**Enter** on Explore or Detail returns to the Library with the query applied (as the Hub).

**Shortcut to focus search [NEW]:** `/` (when no text box has focus) or Ctrl+K. The Hub has none. Optional.

### 2.7 Sort (ORDER)

| Mode (key) | Pill label | Menu caption | Ordering |
|---|---|---|---|
| `title` (default) | Title | "A-Z, series in order" | Title, ascending; within a series, by series order. |
| `year` | Year | "Oldest first" | `year` ascending, then title. |
| `manufacturer` | Manufacturer | "A-Z, then year" | Manufacturer label, then `year`. |
| `hardware` | Hardware | "Arcade, then console, then PC, then year" | `data/vocab/hardware.toml` order (arcade, console, PC), then `year`. |
| `recent` | Recently played | "Last launched first" | Most recent launch first; never-launched last, by title. |

Persisted as `gameSort` (renamed from `catalogSort`). Reorders existing cards; no rebuild. Menu chrome as 1.11. **[DIVERGE]** Captions and modes; the Hub's "VR mod release" and "Added to Hub" are not used. "Added to library" (scan date) can be added as a sixth mode.

### 2.8 Scan my files

Same state machine as 1.12, with these changes:

| Step | Hub | AladdinsCastle |
|---|---|---|
| Idle label | "Scan installed games" | "Scan my files" |
| Running label | "Scanning..." (`#ffcc44`, 14 px) | "Scanning..." with a live count "120 of 214" **[NEW]**. Hashing is slower than the Hub's file checks, so a count is needed. |
| Spinner | Neon light, `#6BF49B`, 2.2 s | Same. Runs in a Rust worker, so it needs no separate thread. |
| Result | counter reads N on PC, then N VR Ready | counter reads N found, then N ready (same colours) |
| Lock during scan | Input swallowed | Not needed: the scan runs off the UI thread. Keep the button disabled and show the count. Closing asks to cancel. **[DIVERGE]** |
| Re-entry guard and heartbeat | PowerShell flag and 60 s heartbeat | Same rule in the worker: one scan at a time, cancel token, progress events. |

**Sources** (read-only; nothing is downloaded or installed):
- User folders for ROM sets, disc images and PC games (set in Settings).
- Automatic: Steam libraries, GOG Galaxy roots, Epic default roots (as the Hub, `Core/Modules/Filter.Scan.ps1:186-237`).
- Emulator installs (for the tools route in docs/frontend.md section 4): listed, not used without confirmation.

**Matching:** by **hash**, not by file name, for ROM sets and disc images (the owner's test set is matched this way, per the owner's test library, kept private). PC games are matched by folder markers and the launch executable.

**Counter button:** the same colours as 1.5. Pre-scan: "Scan my files". Post-scan: "[N] found  |  [M] ready" with the green triangle. Shimmer and the 5 s opt-out chips as the Hub.

**Scan on Startup:** same hover toggle, default off, runs post-paint.

### 2.9 Persisted settings

Storage follows the Hub pattern (1.13) with a new path:
- Primary: `%LOCALAPPDATA%\AladdinsCastle\State\hub-state.json`.
- Recovery copy: a folder beside the app (`UserData`), promoted when the primary is unwritable.
- Unknown keys pass through, as the owner decided (see `docs/workshop.md` and `docs/config-spec.md`).
- Booleans are real JSON booleans.

| Key | Default | Replaces | Note |
|---|---|---|---|
| `winWidth`, `winHeight`, `winLeft`, `winTop`, `winMaximized` | none | same | same rules |
| `sizeLibrary` | `L` | same | |
| `sizeExplore` | `M` | same | |
| `sizeDetail` | `M` | same | |
| `sizeList` | `1.0` | `scaleList` | renamed; same values (1.0, 1.5, 2.0) |
| `gameSort` | `title` | `catalogSort` | renamed; new modes |
| `checkOnStartup` | `false` | same | |
| `shimmerDisabled` | `false` | same | |
| `bannerListDisabled`, `bannerLibDisabled` | `false` | same | |
| `hiddenPublishers` | `[]` | `hiddenModders` | **[DIVERGE]** renamed to match the vocabulary |
| `recentlyPlayed` | `[]` (max 8) | `playHistory` | list of game ids |
| `recentlyPlayedHidden` | `false` | same | |
| `genre` | `all` | none | last GENRE choice **[NEW]** |
| `facets` | `{}` | none | last Filters drawer state **[NEW]** |
| `scanFolders` | `[]` | none | ROM, disc and PC folders **[NEW]**; belongs in the config, decide in 4 |
| `desktopShortcut` | `false` | same | |
| `hubStyle` | (not used) | same | dropped with Classic **[DIVERGE]** |

### 2.10 Tokens added

Same base tokens as 1.14. Additions:

| Token | Value | Use |
|---|---|---|
| genre Light gun | `#e0302a` | GENRE icon, section kind, chip accent (proposal; it matches the schema example accent) |
| genre Racing | `#f0a020` | GENRE icon, section kind, chip accent (proposal) |
| VR True 3D | `#34d399` | VR badge, VR chip, Ready state (same as the Hub's VR Ready green) |
| VR Theatre | `#6fa8ff` | VR badge, VR chip (Hub's External blue) |
| VR Planned | border `#40404e`, text `#9a9aa8` | VR badge (sibling spec) |
| Status Ready | `#34d399` | Ready pill and card state |
| Status To install | `#f59e0b` text accent | optional, sibling spec's "Needs files" amber |
| Scan count | `#5fff8f` | "found" count, as the Hub |

### 2.11 Field mapping (game.toml to shell)

| Shell element | Source field | Rule |
|---|---|---|
| Title, search | `title`, `alt_titles` | Title shown; alt titles searched. |
| Subtitle, subgenre chips | `subgenre` (vocabulary label), `year` | Up to 3 labels, then year. |
| GENRE pills and sections | `genre` | `gun` = Light gun; `racing` = Racing. |
| Explore buckets | `subgenre` | Bucket table in 2.5. |
| MANUFACTURER filter, search | `manufacturer` (vocabulary label), `developer` | Developer searched only. |
| YEAR filter, sort | `year` | Decade chips and range. |
| HARDWARE tree, sort | `hardware` (`family` and `kind` from vocabulary) | Tree in the Filters drawer. |
| VR filter and VR badge | `routes.vr.best` | `true3d`, `theatre`, `none` (no badge), `true3d-planned` = Planned. |
| PLAYERS filter | `players` | 1 or 2+. |
| CONTROLS filter | `controls.type` | Gun, wheel, handlebars, bike, and so on. |
| IN MY LIBRARY, STATE | scan result | Media found and setup status. |
| Accent (banner wash, kicker) | `[hub].accent` | Fallback: genre colour (`#e0302a` gun, `#f0a020` racing). |
| Blurb (Explore subtitle, optional) | `[hub].blurb` | Shown only on Explore's banner. |
| Featured eligibility | `hub.featured` (new, default true) | **[NEW]** replaces the Hub's blocked-title list. |
| Recently played | `recentlyPlayed` (settings) | Game ids. |

### 2.12 Rules carried over as behaviour

These are the Hub's behaviours that the spec keeps. They are rules, not pixels.

1. Filters fail open. A bad entry stays visible (`Core/Modules/Filter.Controls.ps1:680-685`).
2. STATE filters are inactive until a scan has run. They show everything, not nothing (`:652-657`).
3. ORDER and filters reorder or hide existing objects. They never rebuild cards (`Core/Modules/CatalogSort.ps1:92-147`; `Core/Modules/Filter.Controls.ps1:702-715`).
4. A banner's hover-close and rotation only change the banner. The Explore banner has its own Shuffle (`Core/Modules/Window.BannerEffects.ps1:2273-2290`).
5. A scan cannot be closed away mid-run (`Core/Modules/Filter.Scan.ps1:2357-2362`).
6. Persisted flags are read as explicit booleans or strings (`Core/Modules/Startup.ps1:23-35`).

---

## 3. Implementation notes

### 3.1 PowerShell + WPF (keep the Hub's stack)

**Carries over as-is (logic):**
- Filter predicate (`Test-GamePassesFilter`, `Core/Modules/Filter.Controls.ps1:560-686`): rewrite the field reads only. Keep the AND-across, OR-within rule and the fail-open `try`.
- Banner colour wash (`Core/Modules/OverviewPage.ps1:636-694`): the HSV code is pure maths. Feed it `[hub].accent`.
- Responsive header (`Core/Modules/Window.Layout.ps1:1541-1594`) and geometry restore (`:1606-1663`).
- Sort reorders existing objects (`Core/Modules/CatalogSort.ps1:92-147`).
- Splash hand-off: ready flag, estimate file, easing (`Core/Show-StartupSplash.ps1:60-82`; `Core/Modules/Startup.ps1:326-356`).
- State layer: two locations and a recovery copy (`Core/Modules/HubState.ps1`).

**Must rewrite:**
- The XAML (names, sizes, the two filter rows, the Filters drawer). The Hub's XAML is one 1,500-line here-string (`Core/Modules/Window.Layout.ps1:4-1531`).
- The spinner is Windows PowerShell 5.1 only (`Core/Modules/ScanSpinner.ps1:18-30`). A fallback is needed if the stack moves to PowerShell 7.
- Scan logic: AladdinsCastle matches by hash, which is slower. Use a real background runspace and progress events, not a UI pump.

**Risk:** `docs/frontend.md` section 6 flags big PowerShell scripts as hard to maintain. The Hub's own catalogue is one 6,700-line file. Keep the modules small.

### 3.2 Tauri 2 + web UI (if D4 is decided that way)

| Hub mechanism | Web equivalent |
|---|---|
| Window size, min size, centre | `tauri.conf` window: 1120 x 720, minWidth 500, minHeight 400, center true, title "AladdinsCastle". |
| Geometry restore | `tauri-plugin-window-state`, or the same rules in the Rust side (on-screen check). |
| Splash and ready signal | Second borderless window (about 480 x 260, always on top). Main window starts `visible: false`; the page calls a command after two animation frames; Rust shows the main window and closes the splash. Estimate file read from the state folder. |
| Token colours | CSS custom properties on `:root`, one per token in 1.14 and 2.10. |
| Glass pill, border, radius | `background`, `border: 1px solid`, `border-radius`. The selected ring is an inset or outer box-shadow, not a second element. |
| DropShadowEffect (title glow, headset glow) | `filter: drop-shadow(0 0 16px rgba(58,138,221,.45))` on a duplicate text layer, or `text-shadow`. Do not put a drop-shadow filter on opaque pills: the Hub avoids it because the shadow shows as a dark halo on neighbours (`Core/Modules/Filter.Controls.ps1:87-91`). |
| LinearGradientBrush (title, banner fade, wash) | `linear-gradient(...)` with the same stops and direction. Note WPF gradients run in the object's bounding box; CSS percentages match. |
| DrawingBrush dot grid | `background-image: radial-gradient(#222230 0.9px, transparent 1.1px); background-size: 24px 24px;`. Browsers cache this; no hint needed. |
| ScrollViewer | `overflow-y: auto`, a thin 8 px thumb `#2a2a35`. The Hub uses the default scrollbar. |
| WrapPanel (grids) | `display: flex; flex-wrap: wrap; justify-content: center`. |
| Hover scale 1.02 on the banner | `transform: scale(1.02)` on the outer frame only. |
| DispatcherTimer (rotation, hover-close, defers) | `setTimeout` and `setInterval` with the same values (1.15). Clear them on navigation. |
| DoubleAnimation loops (dot pulse, shimmer, spinner) | `@keyframes`. The spinner is an SVG rect with `stroke-dasharray` and an animated `stroke-dashoffset`, plus a blur filter. |
| Scan spinner on its own thread | Not needed: the scan runs in Rust, so the page never freezes. |
| Overlays (ORDER, help) | Absolutely positioned panels over a transparent scrim; Escape handler. |
| Segoe UI | `font-family: "Segoe UI", system-ui, sans-serif`. WebView2 on Windows 11 has it; nothing to bundle. |
| XButton1 and XButton2 | `mousedown` / `mouseup` with `event.button` 3 and 4. Call `preventDefault()` so WebView2 does not navigate. Verify on Windows. |
| Settings | Rust `serde` JSON with atomic write (temp file, then rename) and the recovery copy. |
| Art cache | Rust downloads and caches; the page only sees `asset:` URLs. |
| Breakpoints | `@media (max-width: 779px)` and `(max-width: 619px)`, same rules as 1.3. |

### 3.3 Hub traps to avoid

Each of these is a real bug the Hub's comments describe:

1. **Hover on the image, not the frame.** A scale on a child moves its own hit area, so the cursor leaves and enters in a loop. Put hover on the outer frame (`Core/Modules/Filter.Banners.ps1:246-253`). Same in CSS.
2. **Focusable controls in the search overlay.** A focusable checkbox steals focus on mouse-down and the click is lost (`Core/Modules/Window.Layout.ps1:240-251`). The Filters drawer is not over the search box, but the same rule applies to any popup that is.
3. **Booleans in JSON.** A string "False" is truthy. Parse explicitly (`Core/Modules/Startup.ps1:23-35`).
4. **Full-window repaints.** A tiled background re-rasterises on every scroll pixel unless cached (`Core/Modules/Window.Layout.ps1:684-693`). In CSS, use a single `background-image` on a fixed layer.
5. **Stuck scan.** A dead scan must not lock the window. Use a heartbeat and a timeout (`Core/Modules/Filter.Scan.ps1:82-89`).
6. **Close during scan.** Deferred, not forbidden. A dead scan releases the window (`Core/Modules/Filter.Scan.ps1:2357-2362`).
7. **Ghost timers.** Stop per-banner timers before replacing an effect (`Core/Modules/Window.BannerEffects.ps1:2243-2266`).

---

## 4. Owner decisions and open points

| # | Question | Recommendation |
|---|---|---|
| 1 | Hub technology (D4): Tauri 2 or PowerShell + WPF | Tauri 2: `docs/frontend.md` recommends it. The shell rules here hold for either. |
| 2 | Filters: two-row bar with a drawer (above), or all facets inline on wide windows | Two rows plus drawer. Inline needs about 1,300 px (est.). |
| 3 | STATE vocabulary: game-packages states or docs/frontend.md states | Use game-packages states for the buttons and STATE pills. Reconcile the two docs (sibling spec, section 6, item 2). |
| 4 | Add a fourth STATE pill "Needs files" (media missing)? | Yes. It is the main reason a player cannot launch. |
| 5 | Genre accent colours: `#e0302a` gun, `#f0a020` racing | Accept, or pick other tokens. Changing them is a token edit. |
| 6 | Featured pick: 50 / 50 by genre, or the Hub's 70 / 30 by control type? | 50 / 50 by genre. Owned-first rule stays. |
| 7 | Explore VR toggle replacing PC POWER. Keep a "Your PC" rating? | VR toggle only. A GPU rating can come later for emulator setups. |
| 8 | Section grouping: genre (sibling) or hardware | Genre. "Group by hardware" as an option later. |
| 9 | Scan progress: keep the live "N of M" count? | Yes. Hashing is slow. |
| 10 | Min window size 500 x 400 (Hub) or larger | 720 x 480. The facet strip needs the width. |
| 11 | Keep Help items: Suggest, Logs, Discord, Desktop shortcut. Drop Switch Style. | Keep all but Switch Style. Discord only if a server exists. |
| 12 | "New" chip and the 10.5-day rule | Keep, as the Hub does. |
| 13 | Scan folders: in `hub-state.json` or in the config | Config (per `docs/config-spec.md`), so they can be shared and layered. |

---

## 5. Stale or conflicting facts found

1. **Screenshot is older than the source.** It shows `v0.8.2` and "Scan games". The source has `0.8.7.6` (`Core/VRModHub.ps1:178`) and "Scan installed games" (`Core/Modules/Window.Layout.ps1:560`). This spec uses the source.
2. **Stale comment in the effect pool.** A comment says "1-in-56 pool entry" (`Core/Modules/Window.BannerEffects.ps1:2472`). The pool has 86 entries (`:2296`).
3. **Two status vocabularies** in AladdinsCastle's own docs: `docs/frontend.md` section 1 and `game-packages.md` section 2 (sibling spec, section 6, item 2). This spec follows game-packages for STATE.
4. **Reference paths.** The sibling `docs/ui/02-game-cards.md` uses repo-relative paths. This document uses full absolute paths, per the global instruction. Pick one convention for the `docs/ui/` folder.
