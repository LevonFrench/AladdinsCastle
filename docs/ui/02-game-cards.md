# 02. Game cards: the PCVR Mods Installer Hub look, adapted for AladdinsCastle

> **File references:** paths starting `Core/`, `Start PCVR Mods Hub.bat` or `_screenshots/` are relative to [Mr-Nlce/PCVR-Mods-Installer-Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) at commit `a64401f` (MIT). Paths starting `docs/`, `data/`, `games/` are in this repo.

Status: research spec, 2026-10-08. Owner decision D19 (UI look close to the Installer Hub) is recorded in [workshop.md](../workshop.md). D4 (Hub technology) is still open, so section 4 covers both WPF and web.

**Reference checked:** PCVR-Mods-Installer-Hub at commit `a64401f` (2026-10-06), repo root ``. File:line references below are relative to that root, for example `Core/Modules/CardTile.Frosted.ps1:27`. Screenshot: `_screenshots/hub-main.png`.

**Method:** read the source and the screenshot. The Hub was not run. Pixel heights are layout estimates unless a line of code states the value, and those are marked "est.".

---

## 0. The card rules in brief

1. Card: 175 x 160 px base, 8 px corner, 1 px border, 14 px inner padding, 12 px gap right and bottom.
2. Sizes S / M / L apply a layout scale of 1.00 / 1.15 / 1.30 to the card body. The S / M / L buttons also set a level of 1.0 / 1.5 / 2.0 for headers and the banner.
3. Stack, top to bottom: family pill (left) and info pill (right); title with a docked badge; coloured meta line; grey author line; one-line note; full-width neon button.
4. The accent drives the colour: pill, meta line, neon button and border mix. The card base is near-black, with about 6% accent tint in a flat fill.
5. The button is a dark neon outline with a glow and an accent-coloured label. It is not a solid colour fill.
6. Install states override the colour and the label: amber = game found, mod missing; green = VR Ready; blue = update available.
7. Hover: 600 ms dwell, then the card scales 1.4x and shows its art in a top 80 px strip. Cards without art do not enlarge.
8. Click: the card body opens the detail page. Only the button and the reinstall pill install or launch.
9. Art is never shipped. It is cached on the user's machine, and a generated fallback means no card is ever blank.
10. Sorting and sections move the same card objects. They never rebuild them.

---

## 1. How the Mod Hub does it

### 1.1 Styles

- Two styles, chosen by `$global:hubStyle`: `frosted` (default, `Core/Modules/Window.Controls.ps1:52`) and `classic`.
- `New-GameCard` dispatches on the style (`Core/Modules/CardTile.Classic.ps1:10-16`).
- `CardTile.Frosted.ps1` is one function, `New-GameCardFrosted` (about 2,290 lines). `CardTile.Classic.ps1` has `New-GameCardClassic`. Shared pieces (neon button, state painters, glass sheen, hover spotlights, glow) are in `CardTile.Core.ps1`.
- The screenshot is **Frosted**: neon-outline button, no left accent cap, glass sheen. Copy Frosted. Classic differences are in 1.11.

### 1.2 Size and scale

| S/M/L button | Level (headers, banner) | Card body scale | Card, px (est. for M, L) | Hover popup, px (est.) | Source |
|---|---|---|---|---|---|
| S (default) | 1.0 | 1.00 | 175 x 160 | 245 x 224 | `Window.Controls.ps1:143-144`; default `Helpers.ps1:4` |
| M | 1.5 | 1.15 | 201 x 184 | 282 x 258 | same |
| L | 2.0 | 1.30 | 228 x 208 | 319 x 291 | same |

- The scale is a **`LayoutTransform`**, so it affects layout. The WrapPanel reflows, and switching size does not rebuild cards (`Window.Controls.ps1:141-158`).
- The comment at `Window.Controls.ps1:138` says "1.0/1.2/1.4". The code says 1.15 and 1.3. The code wins.
- The hover scale (1.4x) is a `RenderTransform`, so it does not affect layout and overlaps neighbours. It sits at z-index 1000 (`Helpers.ps1:1162-1165`).
- Cards are built once at base size 1.0 (`CardTile.Frosted.ps1:14-15`). All internal `*$sc` values are base px and the transform scales them.
- Default level is S (`Helpers.ps1:4`). The level is saved as `scaleList`.
- Grid: sections of WPF `WrapPanel`, centred, with cards 12 px apart right and bottom (`Core/Modules/Window.Layout.ps1:913`, `947`, `979`). Dividers are 1 px `#1e1e26` (`Window.Layout.ps1:917`). **No width breakpoints**: the only size-change handlers are in the banner effects. Responsiveness is just wrapping.

### 1.3 Wireframe, S size (175 x 160 px base)

```
+--------------------------------------------------+
| [1] 7D2DVR [2]                     [3] (icon|i)  |  row 0: pills, info pill 22 px tall
|                                                  |
| [4] 7 Days to Die VR                  [5] FREE   |  title 13 px bold, up to 2 lines
| [6] 7DaysVR v4.1.0.227                           |  mod line, accent, 10 px
|     by 7DaysVR Team                              |  author, 9 px, #555568
| [7] Guided Nexus download.                       |  note, 10 px, #777788, one line
| +----------------------------------------------+ |
| |        Install                        [9]    | |  [8] button, 11 px semibold, neon outline
| +----------------------------------------------+ |
+--------------------------------------------------+
```

Callouts:

- [1] Family pill: the catalog `Pill` value (for example `7D2DVR`), else the first word of `Mod`. 8 px SemiBold caps, accent-tinted. `Frosted.ps1:69-108`; `Helpers.ps1:2645-2660` (`Get-ModFamily`).
- [2] Gem diamond, only when `Gem=true` in the catalog. `CardTile.Core.ps1:231-284`.
- [3] Info pill: controls glyph, a 1 px separator, and an italic bold "i". Click opens `InfoUrl` (tooltip "Open mod page"). `Frosted.ps1:1477-1592`.
- [4] Title: 13 px bold, a white to `#a8b0ba` vertical gradient. `Frosted.ps1:223-233`.
- [5] Docked badge in the title row: FREE or WIP. `Frosted.ps1:241-335`.
- [6] Mod line: accent colour, 10 px Medium (9 px if 31+ characters). `Frosted.ps1:423-428`. Author below: 9 px `#555568`, "by <name>" (`Frosted.ps1:490-546`).
- [7] Note line: `Description`, 10 px Medium `#777788`, one line with ellipsis, full text in tooltip. `Frosted.ps1:661-685`.
- [8] Button: the neon outline, label 11 px SemiBold in neon colour. `Frosted.ps1:696-751`, `971`; `CardTile.Core.ps1:515-566`.
- [9] Reinstall pill: 28 px wide, on the right inside the button, only when installed. ↻ glyph. `Frosted.ps1:820-841`.

M and L are the same drawing multiplied by 1.15 and 1.30 (table 1.2). Everything inside scales together.

### 1.4 Element table (Frosted, base px)

| Element | Geometry | Colour / type | Source |
|---|---|---|---|
| Card root | 175 x 160; corner 8; margin right/bottom 12; no card shadow | base `#0c0c10`; border = 22% accent over base | `Frosted.ps1:19-38`; shadow removed `CardTile.Core.ps1:785` |
| Inner grid | padding 14 on all sides; rows Auto / * / Auto | | `Frosted.ps1:57-64` |
| Family pill | corner 3; padding 6 h, 2 v; max width 104 (86 with gem); VRGP and BOTH: 68 (50 with gem) | bg = 18% accent over `#16161a`; text = 50% accent + 50% white; 8 px SemiBold | `Frosted.ps1:70-108` |
| Info pill | height 22; corner 11; margin top 8, right 8; 1 px border `#40404e` | glyph + `#3a3a4a` separator + "i" 12 px bold italic; hover brightens and adds accent glow (blur 10, opacity 0.85) | `Frosted.ps1:1487-1570` |
| Controls glyph | motion 14 px / gamepad 18 px, stroke 1.9, round caps | glyph colour = family text colour | `CardTile.Core.ps1:97-176` |
| Title | 13 px Bold, wraps | white `#ffffff` to `#a8b0ba` vertical gradient | `Frosted.ps1:223-233` |
| Docked badge | corner 2; padding 5 h, 1 v; 1 px border; fill alpha 20 | FREE `#34d399`; WIP `#f87171`; 8 px Bold | `Frosted.ps1:241-335` |
| Mod (meta) line | 10 px Medium (9 px if 31+ chars); margin top 2 | accent colour | `Frosted.ps1:423-428` |
| Author line | 9 px Medium (8 px if author > 28 chars); margin top 2 | `#555568` "by name" | `Frosted.ps1:497-501` |
| Note line | 10 px Medium; NoWrap; ellipsis; margin top 6 (4 with add-on) | `#777788` | `Frosted.ps1:661-685` |
| Button | corner 4; padding 6 v (est. about 27 px tall with the label); 1 px neon border | fill `#0e0e12`; neon glow blur 14 | `Frosted.ps1:696-751`; `Core.ps1:537-550` |
| Button label | 11 px SemiBold, centred, neon colour | | `Frosted.ps1:728-729`; `Core.ps1:562-564` |
| Reinstall pill | width 28; left divider 1 px `#3d6e4a`; glyph ↻ U+21BB 13 px `#88dd99`; opacity 0.75 (1.0 on hover) | | `Frosted.ps1:820-850` |
| Accent cap | 5 px bar, left of the button | **hidden in Frosted**, visible in Classic | `Frosted.ps1:805-813`; `Core.ps1:704-705` |
| Hot zone (body) | top margin 34, bottom margin 41 | transparent; opens detail; arms hover timer | `Frosted.ps1:1651-1662` |
| Pill guard | 50 x 40, top right, z 10 | opens detail | `Frosted.ps1:1735-1758` |
| Top shield | full width x 34, top, z 11 | opens detail | `Frosted.ps1:1773-1793` |

Note: the Frosted button first gets a slate fill and warm-white label (`Frosted.ps1:696-725`). `Add-NeonButtonFx` then overwrites both (`Frosted.ps1:971`; `Core.ps1:537` and `564`). Only the neon survives.

### 1.5 Colour formulas

Use these exact formulas. They are all in code.

- **Base:** `#0c0c10` in Frosted (`Frosted.ps1:27`); `#16161a` in Classic (`Classic.ps1:45`).
- **Catalog `Color` is not used by the card.** Nothing in the card or filter code reads `$game.Color`. The base is hard-coded, and only `Accent` feeds the tint.
- **Flat tint (Frosted):** alpha `a = (TopAlpha + MidAlpha) / 2`. For the default tint, `TopAlpha 0.10` and `MidAlpha 0.02`, so `a = 0.06`. If `a < 0.09` and `lum < 0.45`, add `(0.45 - lum) * 0.30` (lifts dark reds). `lum = (0.299 R + 0.587 G + 0.114 B) / 255`. Blend `tint * a + base * (1 - a)`. (`Helpers.ps1:2587-2630`.)
- **Border:** `0.22 * accent + 0.78 * base` (`Frosted.ps1:29-37`).
- **Family pill:** fill `0.18 * accent + 0.82 * #16161a`; text `0.5 * accent + 0.5 * #ffffff` (`Frosted.ps1:76-89`).
- **Neon (button outline, glow and label):** `GetGlowColor(accent)` lifts dark accents until perceived luminance is at least 140 (`Helpers.ps1:1513-1527`). The neon is that colour times 0.90 (`Core.ps1:518-523`). Glow: `DropShadowEffect` blur `14 * scale`, shadow depth 0, opacity `0.72 - 0.22 * lum`, floored at 0.42 (`Core.ps1:543-549`). Fill `#0e0e12` (`Core.ps1:537`).
- **Title gradient:** `#ffffff` to `#a8b0ba` top to bottom (`Frosted.ps1:230-231`).
- **Neutral colours:** note `#777788`; author `#555568`; long-title author `#888899` (`Frosted.ps1:609`); separator `#444455` (`Frosted.ps1:591`); info-pill border `#40404e`; info separator `#3a3a4a`; divider `#1e1e26` (`Window.Layout.ps1:917`).
- **Badge colours:** FREE `#34d399` (`Frosted.ps1:246-247`); WIP `#f87171` (`Frosted.ps1:280-281`); add-on and improvement tag border `#5599ee`, text `#7ab5ff` (`Frosted.ps1:438-449`).
- **Click pulse:** `#4ade80`, blur 18, 1200 ms total (`CardTile.Core.ps1:383`, `425`, `441-442`).

### 1.6 Text rules

- **Title:** if its measured width is at most 145 px, it is a **short title**: separate mod line and author line. Otherwise it is a **long title** (wraps to 2 lines), and the mod and author merge into one NoWrap line, "Mod . by Author", at 9 px (`Frosted.ps1:373-374`, `568-611`).
- **Author dropped:** on a long title, if the merged line overflows, the author is removed from the tile entirely. It stays on the detail page (`Frosted.ps1:388-402`).
- **Auto-update marker:** the text "(auto-update)" stays where the catalog wrote it. The marker takes the accent colour, and the name stays grey (`Frosted.ps1:515-540`). The marker is text, not a badge.
- Title-specific fixes exist: non-breaking-space line breaks and hard-coded title widths (`Frosted.ps1:141-221`, `412`). Do not copy these. Keep the measured-width rule.

### 1.7 Badges and icons

| Badge / icon | Catalog source | Where it shows | Look | Source |
|---|---|---|---|---|
| Family pill | `Pill` (else first word of `Mod`, else "External" or "Mod") | top left | 8 px caps, accent tint | `Helpers.ps1:2645-2660` |
| Gem | `Gem=true` | next to family pill | faceted diamond, `#71d7ff` stroke | `Core.ps1:231-284`; `Catalog.ps1:68` |
| Controls glyph | `Controls`: MC (motion), GP (gamepad), VRGP (motion, "=", gamepad), BOTH (gamepad + motion) | info pill, left | stroke glyph, family text colour | `Core.ps1:97-176` |
| FREE | `free` tag, derived in `Catalog.ps1`; `FREE_GAME_TITLES` | title row, right | green pill; button shows "FREE" label | `Core.ps1:179-186`; `Frosted.ps1:241-270`, `767-798` |
| WIP | hard-coded list `WIP_GAME_TITLES` | title row, right | red pill | `Core.ps1:195-225` (hard-coded list) |
| Add-on / improvement | `AddonInstaller`, `ImprovementTag` | under mod line | blue outlined tag | `Frosted.ps1:435-484` |
| ROOMSCALE | `Roomscale=true` | **not on the card**: search term and detail-page pill only | | `Filter.Controls.ps1:646-647`; `DetailView.Page.ps1:606-609` |
| AUTO-UPDATE | "(auto-update)" in `Mod` or `Author` | inline text | accent text | `Frosted.ps1:515-540` |
| Quip | `Quip` | **detail page only** | | `DetailView.Page.ps1:2107-2114` |
| Trailer | `VideoUrl` | **not played**: hover trailer disabled | | `Helpers.ps1:1239-1241` |

### 1.8 Button states and labels

Precedence (highest first) is set by the scan painters (`Filter.Scan.ps1:1794`, `1842`, `1895`): **update > VR ready > installed > not installed**.

| Hub state | Condition (est. from code) | Rest label | Hover label | Neon / fill | Card tint and border | Source |
|---|---|---|---|---|---|---|
| Not installed | game not found, or no VR mod | **Install** | Install | neon = accent | flat tint 0.06, accent border | `Frosted.ps1:744`; `Filter.Scan.ps1:1878` |
| Bat missing | `Bat` file not on disk | **Not found** | Not found | accent, opacity 0.4 | | `Frosted.ps1:749-750` |
| External | `isExternal` | `ButtonLabel`, else "Open in Steam", "Open on itch.io", "Get Installer" | same | accent | | `Frosted.ps1:735-742` |
| Installed, no mod | game found (`$installed`) and `ModFile` absent | **Install** in gold `#cdb77a`, bold | Install | neon amber `#f59e0b`; border `#5c4420` | flat tint amber, alpha 0.105 | `Filter.Scan.ps1:728-790`, `1895`; `CardTile.Core.ps1:689-702` |
| VR Ready | `ModFile` present and no update needed (a missing `ModRequiredFile` or a legacy file triggers Update instead) | **VR Ready** in gold, bold | **Start in VR** (label swap) | neon green `#34d399`; sheen `#227052` to `#104230`, alpha 20 | radial top glow of `#46a05a`; border `#1d2e22` | `Core.ps1:632-688`; `Frosted.ps1:1120-1135`, `2238-2255` |
| Update available | newer upstream version, or a legacy file present, or a required file missing (`Filter.Scan.ps1:1700-1730`) | **↓ Update** (arrow 1.3x), label `#b9ccf4` | same | neon blue `#2563eb`; fill alpha 40 gradient | flat tint blue alpha 0.135; glow blur 16, opacity 0.55 | `Core.ps1:581-631`; `Helpers.ps1:1361-1366` |
| DualMode | two installs (Current + Depot) | split: "▶ Current" / "▶ Depot" on hover | same | | | `Frosted.ps1:889-1058` |
| Free game | `free` state after Check Installed | "FREE" label before "Install" | | accent glow label | accent border | `Frosted.ps1:767-798`; `Filter.Scan.ps1:1962` |

Other behaviour:

- Install detection: the game is found (Steam, GOG, folder), then `ModFile` on disk decides VR Ready (`Filter.Scan.ps1:728-790`). After an installer runs, a 750 ms timer checks for the marker and refreshes the card (`Frosted.ps1:2142-2160`, `2199-2215`). Post-install state is written by `Filter.InstallRefresh.ps1:174-210`.
- The light sweep on the default button is a 700 ms diagonal gradient, stopped for VR Ready (`Frosted.ps1:1233-1300`).
- Reinstall pill: visible on VR Ready and Update; hover lights the glyph.

### 1.9 Hover popup

There is **no separate popup element**. The popup in the screenshot is the hovered card, enlarged in place. The top strip covers the title and the mod and author lines; the note line and the button stay visible below it. Screenshot check: the enlarged Black Mesa card is about 242 px wide and the S card about 173 px, which is a ratio of about 1.4 (approximate, read from the 1001 px capture).

| Stage | Behaviour | Source |
|---|---|---|
| Dwell | 600 ms (`HoverDelayMs`). Cancelled if the pointer moved onto the Install button, to avoid shifting a click | `Helpers.ps1:91`; `Frosted.ps1:1669-1691` |
| Before preview | card lifts to 1.06x (Frosted only), with ember sparks; accent spotlights on enter | lift and sparks `Frosted.ps1:1804-1835`; spotlights `Frosted.ps1:1902-1940`, `Core.ps1:843` |
| Preview | card `RenderTransform` 1.4x about its centre; z-index 1000; drop shadow blur 24, depth 6, opacity 0.6 | `Helpers.ps1:1162-1172` |
| Art strip | `Image`, UniformToFill, 175 x 80 px (card coords), clipped to corner radius 7; bottom fade 18 px to `rgba(22,22,26,0.86)` | `Helpers.ps1:1067-1092` |
| Hidden during preview | family pill, info pill, mod and author lines, add-on tag | `Helpers.ps1:1135-1149` |
| Gem | repositioned below the strip, sparkle pulse 900 ms | `Frosted.ps1:1593-1603`; `Helpers.ps1:1150-1160` |
| Button | the card's own button, scaled with the card | `Frosted.ps1:1612-1636` |
| Video | disabled | `Helpers.ps1:1239-1241` |
| Scroll | hover effects suppressed while scrolling, re-armed 120 ms after the last scroll event | `Helpers.ps1:98`, `138-185` |

Art fallback chain on hover: cached header, then the Fastly CDN header, then the portrait, then one retry of the primary URL (`Helpers.ps1:1199-1234`). Cards with no image source get no preview (`Frosted.ps1:1616-1638`).

### 1.10 Click targets

| Zone | Action | Source |
|---|---|---|
| Install button | run installer (Bat), or Start in VR if ready, or Update | `Frosted.ps1:2089-2110`, `2142` |
| Reinstall pill | reinstall, or start if pill action is Play | `Frosted.ps1:2170-2200` |
| Info pill | open `InfoUrl` | `Frosted.ps1:1573-1577` |
| Top 34 px strip, pill guard | open detail page (with pulse) | `Frosted.ps1:1728-1793` |
| Body (hot zone) | green pulse 1200 ms, then detail page (deferred one dispatcher tick so the pulse renders) | `Frosted.ps1:1697-1722`; `Core.ps1:375-494` |
| Installed card, card level | `Start-GameInVR` | `Frosted.ps1:1985-1991`, `2098-2106` |

### 1.11 Classic style: differences

| Aspect | Frosted (default) | Classic |
|---|---|---|
| Base colour | `#0c0c10`, flat fill | `#16161a`, top-to-base gradient (tint 0.10, 0.02 at 60%) (`Classic.ps1:45`; `Helpers.ps1:2587`) |
| Button | neon outline, glow, cap hidden | slate fill (`accent * 0.18 + 10` per channel, `Classic.ps1:681-697`), 5 px accent cap (`Classic.ps1:787`), no halo (`Classic.ps1:932`) |
| Glass sheen and bevel | yes (`Core.ps1:773-841`) | no |
| 1.06 lift | yes | no |
| State repaint | `Sync-FrostedCardState` then neon (`Core.ps1:717`) | flat painters in `Filter.Scan.ps1` |

### 1.12 Sorting and grouping

- Three sort modes: `hub` (curated order), `release` (mod release date), `added` (date added to Hub) (`Core/Modules/CatalogSort.ps1:10`, `15-22`, `47-79`). The default label "Alphabetical" belongs to `hub` mode, but the order is the curated `CatalogOrder`.
- Sorting reorders the existing card objects (`Set-CatalogPanelOrder`, `CatalogSort.ps1:92-147`): children are detached and re-added. Install state, handlers and caches survive.
- Sections: Motion Controls (`ownGames`, `Catalog.ps1:4`), Gamepad (`ownGamesGP`, `Catalog.ps1:4438`), External (`externalGames`, `Catalog.ps1:5923`). Each section has a pill header (coloured icon, title, count) (`Window.Layout.ps1:891-979`).

### 1.13 Performance tricks

- **No virtualisation.** Cards are plain `WrapPanel` children (`Window.Layout.ps1:913`). Rebuilding everything is avoided by design.
- **Preview visuals built lazily** on first hover (`Helpers.ps1:1053-1101`, comment at 1048-1052: about 1 s saved at startup).
- **Neon button cached**: `CacheMode = BitmapCache` (`Core.ps1:551-554`), so scrolling moves a bitmap.
- **Hover effects paused during scroll** (`Helpers.ps1:98`, `138-185`).
- **Image warm-up:** one background runspace. The UI thread only checks `File.Exists` to build the list (`Helpers.ps1:594-626`). Runs at ApplicationIdle after the window appears (`Startup.ps1:407-412`). Overview prewarm at Background priority (`Startup.ps1:656-657`).
- **Pixel snapping:** `UseLayoutRounding` on the card root to avoid blurry text (`Frosted.ps1:13`).
- **Glow on the sibling, not the text:** a `DropShadowEffect` rasterises its whole subtree and softens text. The Hub puts glow on the button border and text on a sibling (`Frosted.ps1:959-974`; `Core.ps1:659-661`).
- **Not a performance feature:** `PrewarmWorker.ps1` is a separate process for online update-version checks (`.gh_version_cache`, `.web_version_cache`). It does not load art (`PrewarmWorker.ps1:1-20`).

---

## 2. AladdinsCastle equivalent

Same geometry, same colour logic, same S / M / L. The content comes from [game-schema.md](../game-schema.md) and [game-packages.md](../game-packages.md). Divergences are tagged **[DIVERGE]** (a deliberate change from the Hub) and **[NEW]** (no Hub equivalent).

### 2.1 Layout

```
+--------------------------------------------------+
| [TIMECRIS]               [gun|2P|  i  ]          |  row 0: [hub].pill | controls glyph, 2P, info
|                                                  |
| Time Crisis                    [TRUE 3D]         |  title + quality badge, right-docked
| Namco · Super System 22 · 1995                   |  coloured line: manufacturer · hardware · year
|     by Namco                                     |  developer (hidden if same as manufacturer)
| Duck behind cover for real, then pop up.         |  [hub].blurb, one line
| +----------------------------------------------+ |
| |                 Ready                        | |  button; hover: "Start in VR ▶"
| +----------------------------------------------+ |
+--------------------------------------------------+
```

Dimensions, pills, glow, neon and hover values are all as in section 1. Only the content changes.

### 2.2 Field mapping

| Hub element | AladdinsCastle source | Rule | Status |
|---|---|---|---|
| Family pill | `[hub].pill` (game-schema section 2) | uppercase, 12 characters max. Fallback: genre label (`GUN`, `RACING`) | same, [NEW] fallback |
| Gem | none | off by default. Optional: `variant.status = "stable"` and `routes.vr.best = "true3d"` | [NEW], optional |
| Controls glyph | `controls.type` | gun → gun glyph; wheel → wheel glyph; handlebars or bike → bike glyph; joystick, yoke, ski, boat, other → generic glyph (not yet drawn) | [NEW] glyphs |
| 2P chip | `players >= 2` | small "2P" text in the info pill, after the glyph | [NEW] |
| Info "i" | none (no info URL field) | opens the **in-app detail page**. Option: add `[hub].info_url` later | [DIVERGE] |
| Title | `title` | same. `alt_titles` not shown on card | same |
| FREE badge | none | not used (no free arcade titles) | [DIVERGE] dropped |
| WIP badge | `variant.status = "wip"` | red pill, as Hub's WIP. Not a hard-coded list | [DIVERGE] data-driven |
| Quality badge | `routes.vr.best` | `true3d` → TRUE 3D (green, FREE style); `theatre` → THEATRE (blue, add-on style); `none` → no badge; planned → PLANNED (grey `#40404e` border, text `#9a9aa8`) | [NEW] |
| Coloured meta line | `manufacturer` label, `hardware` label, `year` | replaces Hub's mod and version line. Priority to drop when too wide: manufacturer, then keep only hardware and year, then ellipsis. Needs short labels (section 6) | [DIVERGE] |
| "by …" | `developer` | shown only when it differs from the manufacturer label | same, [DIVERGE] rule |
| Note line | `[hub].blurb` | one line, ellipsis, full text in tooltip | same |
| ROOMSCALE, SEATED, QUEST | `[hub].badges` (`roomscale`, `quest-standalone`) | search and detail page only, as Hub does for ROOMSCALE. Option: one chip after the family pill (QUEST > ROOMSCALE > SEATED) | [DIVERGE], owner call (section 5) |
| Auto-update text | variant update check | no text; the Update state covers it | [DIVERGE] |
| Accent | `[hub].accent` | neon, pill text, meta line, border mix, all as Hub | same |
| Base colour | `[hub].colour` | **recommended**: use as card base in place of `#0c0c10`. Hub ignores its `Color` field, but our schema defines `colour` as the card background tint | [DIVERGE], owner call (section 5) |
| Add-on tag | none | not used in v1 | [DIVERGE] |

### 2.3 Button states

Precedence (highest first): **planned and nothing installed > update > installed > needs files > needs emulator > ready to install**.

| Our state (game-packages section 2) | Rest label | Hover label | Neon colour | Card tint and border | Click action |
|---|---|---|---|---|---|
| Coming soon (variant `planned`, not installed) | **Coming soon** | same | neutral grey `#8a93a6` | flat base, no glow | open the roadmap item on the detail page; no install |
| Update available | **↓ Update** | same | blue `#2563eb` | blue tint 0.135, glow 16 / 0.55 | run the variant's update recipe |
| Installed (VR ready) | **Ready** (gold `#cdb77a`, bold) | **Start in VR ▶** (lit gold) | green `#34d399` | green top glow `#46a05a`, border `#1d2e22` | launch straight into VR |
| Needs your files (media missing) | **Find my files** | same | amber `#f59e0b` (Hub's "action pending" colour) | amber tint 0.105, border `#5c4420` | open the locator (point or search, frontend.md section 4) |
| Needs an emulator | **Install emulator** | same | info blue `#7ab5ff` | flat base | open the three-route emulator dialog (install, locate, search) |
| Ready to install | **Install** | same | accent | flat tint 0.06 | run the recipe (`install.toml`); confirm `run` steps |

Notes:

- **[DIVERGE]** Rest label "Ready" with the hover swap to "Start in VR ▶" copies the Hub's "VR Ready" pattern. The alternative is to show "Start in VR ▶" at rest; owner call.
- If two variants are installed, hover shows the Hub's split ("▶ <variant A>" | "▶ <variant B>"), labels sized by length as Frosted does (`Frosted.ps1:934`, `950`). Variants come from `install.toml`.
- Reinstall ↻ pill (28 px) appears on Ready and Update states, tooltip "Reinstall or repair".
- Card body click: pulse, then detail page, as in 1.10.

### 2.4 Hover popup and art

- Same dwell (600 ms), 1.4x, shadow, and 80 px strip. Hidden elements: family pill, info pill, meta and author lines, badges.
- Image chain: `banner.png` → `marquee.png` (centre-cropped to the strip ratio) → `flyer.jpg` → generated banner (section 3). Never blank.
- Unlike the Hub, a card without art still shows the generated banner, so every card gets the popup.

### 2.5 Sections and sorting

- Sections by genre instead of controls: **Light guns** (`genre = "gun"`) and **Racing** (`genre = "racing"`), each with the Hub header pill, icon, title and count. Dividers as Hub. Theatre-only entries stay in the same sections, marked by the quality badge.
- Default sort: **title** (alphabetical), matching the Hub's "Alphabetical" label. Other modes: year, manufacturer, hardware, recently played (game-schema section 3).
- Sorting and filtering reorder existing cards (`CatalogSort.ps1:92-147` pattern). No rebuild.

### 2.6 Style

- Ship **Frosted only** in v1. Classic is a Hub toggle and adds work for no gain.
- Keep the Hub's pixel values in a shared token file (section 4.3), so a later Classic or theme change is a data edit.

---

## 3. Art pipeline

### 3.1 What the Hub does

- Art is keyed by `SteamId` and resolved to a CDN URL: `header.jpg` from Akamai, with a Fastly fallback. `library_600x900.jpg` for portraits (`Helpers.ps1:220-238`).
- Manual overrides: `HeaderUrl` and `PortraitUrl`, relative to the Core folder (`Helpers.ps1:978-1003`).
- Disk cache under the runtime root, `Cache/Images/steam_<id>_<kind>.jpg` (`Helpers.ps1:474-535`; `HubState.ps1:85-118`).
- Downloads: `WebClient` to a `.tmp`, then moved into place only on success (`Helpers.ps1:542-583`). A half-written file is never a cache hit.
- Background warm-up as in 1.13.

### 3.2 What replaces Steam for arcade and console games

| Question | Decision |
|---|---|
| Key | the game `id` (for example `timecris`, `ps2-time-crisis-2`). Steam IDs are not used |
| PC releases the user owns | optional `steam_app_id` in `game.toml`. If set, use the Hub's header and portrait CDN chain, cache-first. Record the source in the sidecar |
| Source order | (1) user-scraped art (ScreenScraper and others, from the user's own machine and account); (2) user-supplied files; (3) generated fallback. We ship **no** third-party art (legal.md) |
| Where scraped art goes | **[DIVERGE]** `user/art/<id>/` (gitignored). Not `games/<id>/art/`: config-spec.md section 2 puts scraped art there, but `.gitignore` does not exclude it, and the repo is public GPL. See section 6 |
| Sidecar | `<name>.source.toml` beside each asset: `url`, `scraper`, `retrieved`, `licence_note`, `generated = true|false` (frontend.md section 5) |
| Writing | `.tmp` then move, as `Save-ImageToCache` (`Helpers.ps1:555-567`) |
| Warm-up | at ApplicationIdle; the UI thread checks file existence only (`Helpers.ps1:599-626`) |

### 3.3 Sizes

| Asset | Used for | Recommended | Minimum | Aspect | Notes |
|---|---|---|---|---|---|
| `banner.png` | hover strip (175 x 80 card px); detail hero | 920 x 430 | 460 x 215 | about 2.14:1 | the strip is 2.19:1, UniformToFill, so keep the subject central. Steam's header.jpg is 460 x 215 (Steam's published size; the repo does not state it) |
| `portrait.png` | detail page; library view later | 600 x 900 | 300 x 450 | 2:3 | same ratio as Steam `library_600x900` |
| `marquee.png` | detail page; source for the banner crop | original, at least 1024 wide | 512 wide | original | the generator crops it to banner ratio |
| `flyer.jpg` | detail page only | original | none | original | not on the card |
| generated banner | fallback for hover | 920 x 430 | n/a | 2.14:1 | rendered from `title`, `manufacturer`, `year`, `[hub].colour` and `[hub].accent` |
| generated portrait | fallback for detail | 600 x 900 | n/a | 2:3 | same inputs |

Sizes are recommendations for a 2x display. The Hub has no tile art at rest, so **no tile.png is needed for the card** (game-packages lists it; it is only useful for a future library view).

### 3.4 Hub behaviours not carried over

- The hover video (disabled in the Hub anyway).
- Title-only placeholders: the Hub's `$null` fallback is a text placeholder. We render a generated marquee instead.
- Hard-coded `Assets\*_header.jpg` names.

---

## 4. Implementation notes

### 4.1 WPF (Windows PowerShell 5.1 or .NET WPF)

- Port the Hub's tree nearly line for line: `Border` > `Grid` (Auto / * / Auto) > `DockPanel` for the title row, `WrapPanel` for sections.
- S / M / L: `LayoutTransform` on each card. Hover: `RenderTransform` 1.4x and `Panel.ZIndex` 1000. Do not use `RenderTransform` for S / M / L, because it does not reflow.
- Drop shadows go on the button border and the info pill only, never on the card or its text (the Hub's blurry-title lesson).
- `UseLayoutRounding = $true` on the card root.
- Images: `BitmapImage` with `CacheOption = OnLoad`, plus a `DownloadFailed` chain like `Helpers.ps1:1199-1234`.
- Build the hover preview visual on first hover, not at startup (`Ensure-CardPreviewVisual`).
- Neon button: `CacheMode = BitmapCache`.

### 4.2 Web (Tauri 2 with HTML, CSS and a small framework, or React)

- Layout: CSS grid or flex-wrap with `justify-content: center`. Size by setting `width` and `height` from a CSS variable `--s` (1, 1.15, 1.3). Do **not** use `transform` for S / M / L, because it does not reflow.
- Hover: `transform: scale(1.4)`, `z-index`, `box-shadow` (blur 24, offset 6, alpha 0.6). Set `transition: none` to match the Hub's instant change, or 120 ms ease-out if preferred.
- Title gradient: `background: linear-gradient(...)` with `background-clip: text`. Title clamp: `-webkit-line-clamp: 2`. Other lines: `white-space: nowrap; text-overflow: ellipsis`.
- Art: `object-fit: cover`. Load the banner only on hover intent (the rest state has no art, so lazy loading does nothing useful).
- Local files in Tauri: the asset protocol with a scope limited to `user/art/`.
- Virtualise the grid only above about 200 cards. The Hub's 250-plus cards have no virtualisation and work, so this is far off.

### 4.3 Shared tokens (proposal)

One file, read by both implementations, so the numbers live in one place:

```toml
# ui/theme/cards.toml (proposal). Read by the WPF and web builds alike.

[card]
width = 175
height = 160
radius = 8
pad = 14
gap = 12
border_px = 1
base_default = "#0c0c10"          # Hub value. Recommended: [hub].colour when present
tint_alpha = 0.06                 # (0.10 + 0.02) / 2, flat fill
tint_lift_lum = 0.45
tint_lift_factor = 0.30
tint_lift_below = 0.09
border_mix_accent = 0.22

[scale]
hover_scale = 1.4
hover_lift = 1.06
hover_delay_ms = 600
scroll_quiet_ms = 120
body = { S = 1.00, M = 1.15, L = 1.30 }
level = { S = 1.0, M = 1.5, L = 2.0 }

[type]
pill = 8
title = 13
meta = 10
meta_long = 9
author = 9
note = 10
button = 11

[button]
radius = 4
pad_v = 6
fill = "#0e0e12"
glow_blur = 14
glow_opacity = 0.72
glow_per_lum = 0.22
glow_min = 0.42
neon_dim = 0.90
lift_min_lum = 140

[info_pill]
height = 22
radius = 11
border = "#40404e"
separator = "#3a3a4a"

[preview]
strip_h = 80
strip_radius = 7
fade_h = 18
fade = "#16161adc"
shadow_blur = 24
shadow_depth = 6
shadow_opacity = 0.6
z = 1000

[state.installed]
label = "Install"
neon = "#f59e0b"
border = "#5c4420"
label_fg = "#cdb77a"
tint = 0.105

[state.ready]
label = "Ready"
hover = "Start in VR"
neon = "#34d399"
border = "#1d2e22"
glow = "#46a05a"
label_fg = "#cdb77a"

[state.update]
label = "Update"
neon = "#2563eb"
label_fg = "#b9ccf4"
tint = 0.135
glow_blur = 16
glow_opacity = 0.55

[state.planned]
label = "Coming soon"
neon = "#8a93a6"

[state.needs_files]
label = "Find my files"
neon = "#f59e0b"

[state.needs_tool]
label = "Install emulator"
neon = "#7ab5ff"

[text]
title_top = "#ffffff"
title_bottom = "#a8b0ba"
note = "#777788"
author = "#555568"
author_long = "#888899"
separator = "#444455"
divider = "#1e1e26"
reinstall = "#88dd99"

[badge]
free = "#34d399"
wip = "#f87171"
addon_border = "#5599ee"
addon_fg = "#7ab5ff"
click_pulse = "#4ade80"
planned_border = "#40404e"
planned_fg = "#9a9aa8"
```

### 4.4 Hub traps to avoid

- Per-title hard-coding: non-breaking-space line breaks (`Frosted.ps1:141-221`), title-specific widths (`412`), the WIP title list (`Core.ps1:195-225`), and the family-by-regex rules (`Helpers.ps1:2645-2660`). Use catalog data instead.
- The `DropShadowEffect` on a text parent (see 1.13).
- Hit-zone layering. The Hub uses overlapping zones (hot zone, pill guard, top shield) to route clicks. Keep the behaviour, but document it once as a table, as in 1.10.
- Stale comments: the 1.2 / 1.15 scale note at `Window.Controls.ps1:138`.

---

## 5. Owner decisions needed

| # | Question | Recommendation |
|---|---|---|
| 1 | Card base: `[hub].colour` (per game) or fixed `#0c0c10` (exact Hub)? | `[hub].colour`; the tint formula is unchanged, so the look stays Hub-like |
| 2 | Info "i": opens the detail page, or an external info link? | detail page; add `[hub].info_url` later if wanted |
| 3 | ROOMSCALE, SEATED, QUEST: filter and detail only, or one chip on the card? | filter and detail only (Hub precedent) |
| 4 | Gem marker: keep as "stable True 3D", or drop? | drop for v1 |
| 5 | Motion extras (sparks, spotlights, light sweep): keep, or add a "reduce motion" setting? | keep, with the setting |
| 6 | Button style: Frosted neon (screenshot) or Classic slate with cap? | Frosted neon |
| 7 | Rest label "Ready" with hover "Start in VR ▶", or "Start in VR ▶" always? | Ready with hover swap (Hub pattern) |
| 8 | Scraped art: `user/art/<id>/` (gitignored) or `games/<id>/art/`? | `user/art/<id>/`; update `.gitignore` either way |
| 9 | Meta line: manufacturer · hardware · year, or hardware · year with manufacturer on the detail page? | full line with the drop order in 2.2 |

---

## 6. Schema drift and gaps to fix

1. **Where the card fields live.** `game-schema.md` section 2 puts `pill`, `colour`, `accent`, `blurb`, `badges` in `[hub]`. `game-packages.md` section 3 puts `colour`, `accent`, `blurb`, `badges` at top level. `games/timecris/game.toml` uses `[hub]`. Pick `[hub]` and update game-packages section 3.
2. **Two status sets.** `frontend.md` section 1 lists Ready, Needs your files, Not installed, Update available, Unsupported. `game-packages.md` section 2 lists Needs your files, Needs an emulator, Ready to install, Installed, Update available, Coming soon. This spec uses the game-packages set for buttons and the frontend words for status text. Reconcile the two docs.
3. **Badge vocabulary.** game-packages section 2 lists TRUE 3D, THEATRE, GUN, WHEEL, HANDLEBARS, BIKE, ROOMSCALE, SEATED, 2 PLAYERS, WIP, QUEST STANDALONE. game-schema `[hub].badges` uses `roomscale` and `quest-standalone`. Derive GUN / WHEEL / BIKE from `controls.type`, and TRUE 3D / THEATRE from `routes.vr.best`, so they are not stored twice.
4. **Short labels for the meta line.** Add `short = "..."` to `data/vocab/hardware.toml` and `data/vocab/manufacturers.toml`. For example "Namco / Bandai Namco" is long; `Namco` is the short form. Current hardware labels run to "Namco Super System 22" style lengths.
5. **Missing fields:** `info_url` (optional), `steam_app_id` (optional, PC ports), `variant.status` read by the card (`stable`, `wip`, `planned`, from install.toml).
6. **Art path.** config-spec.md section 2 says `games/<id>/art/` holds "user-scraped or pack art". legal.md says the project ships no third-party art. Keep only pack art (rights owned) in the repo. Put scraped art in `user/art/<id>/` and gitignore it.
7. **Icon set.** The Hub's glyph geometry is in `CardTile.Core.ps1:110-114`. Draw GUN, WHEEL, BIKE and a generic fallback on the same 24-unit grid with stroke 1.9 and round caps, so they match. No ready-made path data exists for them.

---

## 7. Corrections to the brief

Checked against the source. These differ from the first description of the Hub:

- **Install button:** not a solid coloured fill. The Frosted default is a dark neon outline with a glow (matches the screenshot). Solid tints appear only in the Update and VR Ready states, at low alpha (`Core.ps1:620-626`, `677-681`). Classic uses a dark slate fill with a 5 px accent cap.
- **Hover popup:** not a separate element. It is the same card scaled 1.4x. The art sits in a top 80 px strip. The "big Install button" is the card's own button.
- **PrewarmWorker.ps1:** online update-version checks, not art loading (`PrewarmWorker.ps1:1-20`). Art warm-up is `Helpers.ps1:594`.
- **OwnedModFiles.ps1:** install-ownership manifests (`.pcvrhub_*_ownership.csv`), not UI.
- **Catalog `Color`:** not read by the card. The base is hard-coded.
- **Roomscale:** not drawn on the card. Search and detail page only.
- **Quip:** detail page only.
- **Scale comment:** the code is 1.15 and 1.3, not 1.2 and 1.4.

---

## 8. Source index (repo root `_refs/PCVR-Mods-Installer-Hub/`)

| Area | File | Lines |
|---|---|---|
| Style dispatch | `Core/Modules/CardTile.Classic.ps1` | 10-16 |
| Frosted card builder | `Core/Modules/CardTile.Frosted.ps1` | 1-2291 (main at 19-1811) |
| Classic card builder | `Core/Modules/CardTile.Classic.ps1` | 18-2142 |
| Shared icons, gem, glow, neon, states | `Core/Modules/CardTile.Core.ps1` | 97-963 |
| Colour, tint, family, art, hover | `Core/Modules/Helpers.ps1` | 91-98, 103-185, 220-238, 474-626, 978-1244, 1361, 1498-1527, 1891, 2587-2660 |
| Scale, style switch | `Core/Modules/Window.Controls.ps1` | 52-53, 136-158, 169-187, 257-275, 302 |
| Panels and dividers | `Core/Modules/Window.Layout.ps1` | 891-979 |
| Catalog sections | `Core/Modules/Catalog.ps1` | 4, 4438, 5923 |
| Sorting | `Core/Modules/CatalogSort.ps1` | 10-22, 47-147 |
| Install state scan | `Core/Modules/Filter.Scan.ps1` | 728-790, 1794-1962, 2093-2135 |
| Post-install state | `Core/Modules/Filter.InstallRefresh.ps1` | 174-223 |
| Startup warm-up | `Core/Modules/Startup.ps1` | 407-412, 476, 656-657 |
| Search and detail | `Core/Modules/Filter.Controls.ps1`, `DetailView.Page.ps1` | 646-647; 606-609, 2107-2114 |
| Image cache root | `Core/Modules/HubState.ps1` | 85-118 |
| Update-check worker (not art) | `Core/Modules/PrewarmWorker.ps1` | 1-20 |
