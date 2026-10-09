# 03. Detail page and install flow: the PCVR Mods Installer Hub look, adapted for AladdinsCastle

> **File references:** paths starting `Core/`, `Start PCVR Mods Hub.bat` or `_screenshots/` are relative to [Mr-Nlce/PCVR-Mods-Installer-Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) at commit `a64401f` (MIT). Paths starting `docs/`, `data/`, `games/` are in this repo.

Status: research spec, 2026-10-08. Owner decision D19 (UI look close to the Installer Hub) is in [workshop.md](../workshop.md). D4 (Hub technology) is still open, so section 3 covers WPF and web. Card rules (tile size, card states, hover popup) are in [02-game-cards.md](02-game-cards.md); this file does not repeat them.

**Reference checked:** PCVR-Mods-Installer-Hub, MIT licence, repo root ``. Line numbers are from the clone as it stood on 2026-10-08.

**How references read.** File references are full absolute paths, per the owner's path rule. Inside a row or bullet that names one file, a bare `:NNN` or `:NNN-NNN` refers to that file. Links between the spec documents stay relative so they work inside the repo.

**Method:** read the source named in the brief plus the modules it calls. The Hub was not run. Values come from code unless marked "est.". Labels are marked **PROPOSAL** where we choose, and **OPEN** where the owner must choose.

---

## 0. The rules in brief

1. **One folder per game.** The detail page is the game's `game.toml` + `install.toml` + `README.md` + `art/` shown as one page (the Hub's per-mod detail page, `Core/Modules/DetailView.Page.ps1`).
2. **Order:** hero, title row, meta strip, state line, "What you need" checklist, action row, per-game settings, performance (optional), about and similar games, video, controls, README sections, what it installs, uninstall, quip.
3. **One primary button.** Its label and colour follow the state, with the precedence of [02 section 2.3](02-game-cards.md): planned, then update, then installed, then needs files, then needs emulator, then ready to install.
4. **State is always visible.** The Hub once hid "VR Ready" behind hover; it now shows a pill (`Core/Modules/DetailView.Page.ps1:495-501`). Do the same.
5. **Install runs in the app with the Hub's console grammar:** `--- [n/t] Title ---` headers and `[OK]`, `[!!]`, `[XX]`, `[..]` markers, written to a log file that is the same text.
6. **Any failure stops at that step and opens a recovery panel.** It offers retry, log, folder and "drop a file here". It never offers Exit (`Core/Modules/InstallerRecovery.ps1:97-184`).
7. **Nothing is marked installed until its check passes.** The Hub's Alba installer printed "Installation complete!" after a failed DLL check (section 1.21).
8. **Downloads:** a pinned SHA-256 is required before any alternate source is tried. Last comes a manual drop-in, validated before it replaces anything (`Core/Modules/InstallerSafety.ps1:558-789`).
9. **Remembered folders and files** live in checksummed state under `user/`, never in the program folder. "Clear" asks first.
10. **Uninstall removes only what the install recorded** (an ownership manifest). It previews the list, keeps the user's media, saves and edited files, and never deletes the base game (`Core/Modules/OwnedModFiles.ps1`, `Core/Modules/UninstallGuide.ps1:19-123`).

---

## 1. How the Mod Hub does it

### 1.1 Detail page, top to bottom

```
< Back to library                                              (Page 88-163)
+----------------------------------------------------------------------------+
|                                                                            |  hero, 360 px tall, radius 10
|                      header.jpg, UniformToFill                             |  Page 169-181
|    (over 1040 px wide: frozen 1040 px centred, accent edges, side bands)   |  Page 258-355
+----------------------------------------------------------------------------+
Title 28 px   [VR READY] [FAMILY] [CONTROLS] [ROOMSCALE] [INSTALLED]
              [AUTO-UPDATE] [FREE] [WIP]                                   (Page 479-763)
VR MOD  <mod name>            |  CREATED BY  <author>                      (Page 767-899)
|> Update available   /  Game installed - ready for the VR mod              (Page 901-988)
|> You'll need <game> installed first. Get on Steam below.  (blue box)      (Page 990-1055)
[ Install Mod ] [ Reinstall Mod ] [ Start Depot ] [ Locate Game ]          (Page 1057-3402)
[ + Install <addon> add-on ] [ Get Wabbajack ] [ Mod Page ] [ Open in Steam ] [ VR / Flat ]
PC POWER  <tier>  [####--] YOU marker, GPU and CPU reference              (DetailView.Power.ps1:8-127)
+------------------------------------------+  +----------------------------+
| GAME INFO (1.7 part)                     |  | SIMILAR GAMES (1 part)     |
|  IMPORTANT - ABOUT THIS MOD (notice)     |  |  thumb  Title              |
|  description (Steam text or override)    |  |  thumb  Title              |
|  optional screenshot                     |  |  ...  (4 shown, grows)     |
+------------------------------------------+  +----------------------------+
[video 1] [video 2]                                             (Page 1836-1864)
README sections, in order: About this mod, About, Where to get the game,
What it installs, Requirements (+ Steam Theatre button), Note, middle
sections, then the tail: related, communit, discord, support, donat,
credit, deactivate, uninstall                                 (Page 1866-1969)
[ Steam Theatre ] [ Uninstall Guide v ] [ Uninstall now ]                 (Page 1985-2068)
Discord discussion & support (central registry)                           (Page 2093-2102)
| quip, italic, accent bar                                                (Page 2104-2175)
SUPPORT THE MOD (only with no README and a SupportUrl)                    (Page 2186-2234)
```

Annotations: `Page` means `Core/Modules/DetailView.Page.ps1`. `Power`, `Content`, `Actions` and `Launch` mean the same folder's `DetailView.Power.ps1`, `DetailView.Content.ps1`, `DetailView.Actions.ps1` and `DetailView.Launch.ps1`.

### 1.2 Section table

| # | Section | What it does | Source |
|---|---|---|---|
| 1 | Back button | "Back to library", or "Back to explore" from the Explore page. Outline only. Border `#3a3a48`, hover `#dd6600`, press `#ffcc66`. | `Core/Modules/DetailView.Page.ps1:88-163` |
| 2 | Scroll reset | Scrolls to top immediately and again after layout. | `Core/Modules/DetailView.Page.ps1:61-72` |
| 3 | Hero | 360 px tall, radius 10, background `#0a0a0c`, a card tint from the accent (top alpha 0.30, mid 0.10), hover scale 1.02. Trailers are off: Steam's trailer URLs changed and WPF cannot decode VP8/VP9. "Open in Steam" replaces them. | `Core/Modules/DetailView.Page.ps1:169-181`, `:220`, `:456-461` |
| 4 | Hero image chain | header → fastly header → portrait → primary URL again → hide the image and show the title (36 px bold). The title placeholder is always laid down first. | `Core/Modules/DetailView.Page.ps1:203-453` |
| 5 | Wide-window layout | Above 1040 px the image freezes at 1040 px, centred. Gradient edges use the accent at alpha 150, fading to 0 between stops 0.22 and 0.78. Side bands run an animated banner effect. | `Core/Modules/DetailView.Page.ps1:258-355` |
| 6 | Title row | Title 28 px bold, a white to `#D8DEE3` vertical gradient. Pills follow it (section 1.3). | `Core/Modules/DetailView.Page.ps1:479-763` |
| 7 | Meta strip | Two columns, "VR MOD" and "CREATED BY". Labels 9 px SemiBold `#666677`, values 13 px Medium `#dddddd`, a 1 px rule `#1e1e26`, 24 px gap. Strips a leading "(auto-updates)" from the author. | `Core/Modules/DetailView.Page.ps1:767-899` |
| 8 | Status line | 3 px left bar on a 10 to 16 % tint. "Update available" in `#ffb060`. "Game installed - ready for the VR mod" in `#7ed59a`. Hidden when VR Ready (the title pill says it). | `Core/Modules/DetailView.Page.ps1:901-988` |
| 9 | "You'll need X installed" hint | Blue box (`rgb(37,99,235)`, fill alpha 38, border alpha 110), text `#cfd6e6`, 12 px, line 18. Shown only after a scan, when the base game is missing and the entry is not external or standalone. | `Core/Modules/DetailView.Page.ps1:990-1055` |
| 10 | Action row | Primary and companion buttons (sections 1.5 and 1.6). Margin 0/10/0/18. | `Core/Modules/DetailView.Page.ps1:1057-1068` |
| 11 | Elden Ring strip | Game-specific control strip after the action row (section 1.20). | `Core/Modules/DetailView.Page.ps1:1070-1086` |
| 12 | PC Power | Hardware tier card (section 1.7). | `Core/Modules/DetailView.Page.ps1:1088-1092` |
| 13 | Game Info and Similar Games | Two columns, Game Info 1.7 star : 10 px gap : Similar 1 star. Collapses to one column without info text. | `Core/Modules/DetailView.Page.ps1:1094-1190` |
| 14 | Video strip | After the info row, only for entries with a `VideoUrl`. Two videos sit side by side. | `Core/Modules/DetailView.Page.ps1:1836-1864` |
| 15 | README sections | Parsed from the README, ordered (section 1.10). | `Core/Modules/DetailView.Page.ps1:1866-1969` |
| 16 | Uninstall box | Steam Theatre (if not placed in Requirements), Uninstall Guide, Uninstall now (section 1.11). | `Core/Modules/DetailView.Page.ps1:1985-2068` |
| 17 | Discord block | One central registry block after the README tail (section 1.12). | `Core/Modules/DetailView.Page.ps1:2093-2102` |
| 18 | Quip | Last text block: accent left bar, italic. | `Core/Modules/DetailView.Page.ps1:2104-2175` |
| 19 | Support box | Only with no README and a `SupportUrl`. Label "SUPPORT THE MOD" (9 px caps); default text "If you enjoy this mod, consider supporting the maintainer:". | `Core/Modules/DetailView.Page.ps1:2183-2234` |

### 1.3 Title-row pills

| Pill | Text | Look | Shown when | Source |
|---|---|---|---|---|
| VR READY | `VR READY` | Radius 11, border `#4ade9f`, text `#7df3bd`, ● dot, green glow `#34d399` blur 12 opacity 0.55, no fill | State ready | `Core/Modules/DetailView.Page.ps1:495-546` |
| Family | `Get-ModFamily` (uppercase) | Radius 3, 8 px / 10 px SemiBold, fill 18 % accent + 82 % `#16161a`, text 50 % accent + 50 % white | Always | `Core/Modules/DetailView.Page.ps1:549-574` |
| Controls | Motion Controls / Gamepad VR / VR Controller = Gamepad / Motion + Gamepad | Same palette as family | Entry has `Controls` | `Core/Modules/DetailView.Page.ps1:467-473`, `:576-604` |
| ROOMSCALE | `ROOMSCALE` | Same palette | `Roomscale` flag | `Core/Modules/DetailView.Page.ps1:606-634` |
| INSTALLED | `INSTALLED` | Fill `#161d18`, border `#4d8a5e`, text `#88dd99` | Installed, not ready (hidden when ready, because VR READY says the same) | `Core/Modules/DetailView.Page.ps1:636-675` |
| AUTO-UPDATE | `AUTO-UPDATE` | Same palette as family | Mod text or tag marks auto-update | `Core/Modules/DetailView.Page.ps1:677-716` |
| FREE | `FREE` | Fill alpha 20 of `rgb(52,211,153)`, border and text `rgb(52,211,153)`, bold | Free game list | `Core/Modules/DetailView.Page.ps1:718-740` |
| WIP | `WIP` | Fill alpha 20 of `rgb(248,113,113)`, border and text the same, bold | WIP list | `Core/Modules/DetailView.Page.ps1:742-763` |

Hover on each pill: scale 1.08 (`Add-HoverScale`).

### 1.4 Meta strip, state line and hint

- **Meta values** are plain text. The catalog's `Mod` string drops "(auto-updates)" and the GEM marker sits beside the named mod (`Core/Modules/DetailView.Page.ps1:829-853`).
- **Status line** states (`Core/Modules/DetailView.Page.ps1:919-938`):

| State | Primary text | Detail text | Bar and text colour | Tint alpha |
|---|---|---|---|---|
| update | Update available | none | `#ffb060` | 26 |
| installed | Game installed | " - ready for the VR mod" in `#8a8f99` | `#7ed59a` | 26 |
| installed, free standalone | Ready for the VR mod | none | `#7ed59a` | 26 |

- **Hint wording** (`Core/Modules/DetailView.Page.ps1:1043-1046`). The text names the base game, says whether the user must own it, and points to "Get on Steam" when it is missing.

### 1.5 Primary button: states

The primary button is built at `Core/Modules/DetailView.Page.ps1:2241-2397`. Its colours depend on state (lines 2281-2384).

| State | Condition (code) | Label | Fill / border / text | Click (code) |
|---|---|---|---|---|
| Ready | `state = ready` | `Start in VR ▶` (U+25B6), label fixed, no hover swap | `#161d18` / `#5fa873` 1.5 px / `#88dd99`, check icon | `Start-GameInVR` (`:2675-2678`) |
| Update, one mod | `state = update`, not multi-launch | `Update <name>` from `Get-UpdateActionLabel`, fallback "Update Mod" (`Core/Modules/Helpers.ps1:1361-1366`) | `#2563eb` / `#6da3ff` 1.5 px / white | Runs the Bat through `Start-LoggedInstaller` (`:2728-2729`) |
| Update, multi-launch | update with `TwoMods` or `DualMode` | `Start in VR ▶` (green); the update action moves to the companion (blue) | As Ready | Launch (`:2675`) |
| Installed, no mod, or free scanned | `Tag = installed` or `State = free` | `ButtonLabel`, else `Get Installer` (external or itch), else `Install Mod` | As Ready | Section 1.5 click rules |
| Default | Not scanned or not found | Same label rule as above | Accent-tinted slate: fill `0.09 × accent + 6`, border `0.55 × accent + 60`, text `120 + 0.45 × accent` | Same |

- **Hover** (`Core/Modules/DetailView.Page.ps1:2496-2550`). The hover handler is wired only when the Reinstall companion exists, that is for ready and update states (`:2406`). For a dual-mode game it swaps the label to "Start Current ▶" and leaving restores the rest label (`:2522-2546`). Elsewhere the rest label already reads "Start in VR ▶", so there is no visible swap. The update state keeps its label on hover on purpose (code comment at `:2517-2521`). The close delay for the companion popover is 250 ms (`:2559`).
- **Click** (`Core/Modules/DetailView.Page.ps1:2657-2764`), in this order:
  1. Dual mode: "Current" or "Depot" launch (`:2665-2672`).
  2. Ready, or a multi-launch update: `Start-GameInVR` (`:2675-2678`).
  3. No Bat installer: open `DownloadUrl` (not an `api.github.com` URL), then `InfoUrl`, then `Url` (`:2680-2713`). A Steam entry sets a one-game refresh marker first (`:2707-2710`).
  4. Bat present but file missing: an error line, "The configured installer is missing" (`:2716-2719`).
  5. Otherwise run the Bat through `Start-LoggedInstaller`, then poll every 750 ms for exit and refresh only that game (`:2728-2757`).
- **Gloss:** `Add-ButtonGloss -Intensity 0.10` (`:2397`). Button: radius 7, padding 16/10, `MinWidth 162` (`:2250`), 14 px SemiBold, line height 20 (`:2256-2265`).

### 1.6 Companion buttons

Listed in the order they are added to the row (`Core/Modules/DetailView.Page.ps1:2813`, `2849`, `2863`, `2925`, `2945`, `2951`, `3184`, `3220`, `3253`, `3304`, `3390`, `3401`).

| Button | Shown when | Look (fill / border / text) | Click | Source |
|---|---|---|---|---|
| Two-mod choices ("Play X", "Install X", "Install / manage mods", "Install VR mod") | `TwoMods` pages | Installed `#161d18` / `#5fa873` / `#88dd99`; not installed `#16161d` / `#3a3a47` / `#c2cad2` | Alternative installer | `Core/Modules/DetailView.Actions.ps1:632-683`, `Core/Modules/DetailView.Page.ps1:2772-2815` |
| Re-locate Game and Clear | After a user-located folder (marker `user_located`) | Re-locate: `#16161d` / `#50505f` / `#c2cad2`. Clear: `#16161d` / `#8a5560` / `#d2a0ad`, hidden until hover on the pair | Re-locate: folder picker. Clear: confirm, then remove state | `Core/Modules/DetailView.Page.ps1:2823-2850`, `Core/Modules/DetailView.Actions.ps1:66-130` |
| Locate Game | After a scan that did not find the game, for entries with a `ModFile` (or always, when `AlwaysOfferLocate`) | As Re-locate | Folder picker, then Plan A or Plan B (section 1.16) | `Core/Modules/DetailView.Page.ps1:2858-2864`, `Core/Modules/DetailView.Actions.ps1:726-1110` |
| Start Depot | `DualMode` and a depot build is present. Label "Start <DepotButtonLabel>" | `#161d18` / `#3d6e4a` / `#88dd99`, ▶ 13 px | `Start-GameInVR -Mode Depot` | `Core/Modules/DetailView.Page.ps1:2871-2928` |
| Start Legacy | An older pinned build is present | Two-mod style, installed | `Start-GameInVR -Mode LegacyDepot` | `Core/Modules/DetailView.Page.ps1:2933-2947` |
| Reinstall Mod | Ready or update, not external (`Test-ShowDetailReinstallAction`). Label "Reinstall Mod" when ready; "Start in VR ▶" when update (single mod); the update label when multi-launch | Ready `#15151e` / `#5a6aa8` 1.5 / `#dddddd`; update-green `#161d18` / `#5fa873`; multi-launch update `#2563eb` / `#6da3ff` | Ready: run Bat with RequiresAdmin and refresh. Update: start the game | `Core/Modules/DetailView.Page.ps1:2402-2653`, `Core/Modules/DetailView.Actions.ps1:7-13` |
| Add-on | Entry has `AddonInstaller` and `AddonName` (section 1.6a) | Three states (1.6a) | Runs the add-on installer, polls 750 ms | `Core/Modules/DetailView.Page.ps1:2954-3184` |
| Get Wabbajack and mod-link buttons | Wabbajack guide entries | Get Wabbajack: border `#8a72d8`, white text | Opens the link | `Core/Modules/DetailView.Page.ps1:3205-3253` |
| Mod Page | `InfoUrl` or `ModPageUrl`, not `HideModPageButton`. Uses `ModPageUrl` first | Fill `#0e1c21`, border `#4a9ab0` 1.5, text `#b8dde8`, external icon | Opens the URL | `Core/Modules/DetailView.Page.ps1:3257-3304` |
| Get on Steam / Open in Steam | `SteamId`, not `HideSteamButton`. "Get on Steam" when the scan ran and the game is not installed | Get on Steam: `#2563eb` / `#6da3ff` / white. Open in Steam: `#101a30` / `#5078cc` 1.5 / `#b8cdf0` | Sets refresh marker, then `steam://store/<id>` | `Core/Modules/DetailView.Page.ps1:3307-3391` |
| Flat / VR switch | BepInEx, `FlatVR*` or the Luke Ross launcher, and VR-installed. Segmented "VR / Flat": active `#cdb77a` bold, inactive `#767688`, border `#4a7ea0` | Toggle the loader or INI, or rename the proxy DLL, with the game closed | `Core/Modules/DetailView.Actions.ps1:351-400`, `Core/Modules/DetailView.Page.ps1:3393-3402` |
| Steam Theatre | Requirements section, or the uninstall box | Button "Disable Steam Theatre" opens a tooltip mock of SteamVR's Dashboard setting, footer "Set to OFF" | **Shows instructions only. It does not change SteamVR settings** (section 1.21) | `Core/Modules/DetailView.Power.ps1:828-1058` |

**1.6a Add-on states** (`Core/Modules/DetailView.Page.ps1:3095-3126`):

| State | Fill / border | Text | Tooltip |
|---|---|---|---|
| Base not installed | `#181820` / `#2a2a35`, opacity 0.55 | `+ Install <addon> add-on` in `#5b5b6e` | "Install the base <mod> first." |
| Base installed, add-on missing | `#0e2030` / `#5599ee` 1.5 | `+ Install <addon> add-on` in `#7ab5ff` | "Install <addon> add-on on top of <mod>." |
| Add-on installed | `#161d18` / `#5fa873` 1.5 | `<addon> installed` in `#88dd99` | "... is installed. Click to reinstall." |

Add-on detection (`Core/Modules/DetailView.Page.ps1:3040-3080`) looks for a unique payload file first, then an `.installed_path` marker that still points at a valid folder. It does not hash anything.

### 1.7 PC Power card

- **What it is:** a six-step gauge of how much GPU and CPU a VR mod needs. Tiers are LOW, BASIC, SOLID, STRONG, HIGH, EXTREME, each with a reference GPU and CPU (`Core/Modules/Helpers.ps1:1950-1957`).
- **Title to tier:** a hard-coded table in `Get-PowerTier` (`Core/Modules/Helpers.ps1:2159` onward), with the author's notes in comments.
- **Gauge:** six 8 px segments, the active range lit in a blue ramp (`Core/Modules/DetailView.Power.ps1:44-80`, approx.). The header shows the tier name, or "LOW -> HIGH" for a range. A "RECOMMENDED" caption sits under the required tier (`:256`).
- **"YOU" marker:** only after the user consents. The copy is "Reads your installed GPU name once, locally." (`Core/Modules/DetailView.Power.ps1:486`). Badges then say NO MATCH, FITS, CLOSE or STRETCH (`:615-638`). "Always compare" is a toggle.
- **Disclaimer** (`Core/Modules/DetailView.Power.ps1:363`, 11 px `#8a93a3`): VR mods are not tested across many systems. The scale is a rough indicator, and GPU strength is often the most important factor. The user can tick to dismiss.

### 1.8 Game Info, Notice and Similar Games

- **Game Info box:** `#13131a` fill, border `#222230`, radius 6, padding 14/10/14/12 (`Core/Modules/DetailView.Page.ps1:1195-1200`). The heading is "Game Info", or "Tool Info" for three tool entries (`:1155-1159`). Heading: 3 px accent bar, text one step above body size, SemiBold `#f0f0f4`. Body: Medium `#c8c8d4`.
- **Description text:** from a hard-coded map keyed by title (`:1104-1154`, about 50 entries). That map overrides the Steam text because some SteamIds point at the wrong product. If Steam text is not yet loaded, the box says so and fills in later (`:1166`, `:1318-1333`).
- **Notice** (catalog `Notice`, optional `NoticeUrl` or `NoticeGameTitle`). Amber box: fill `rgb(224,168,58)` alpha 28, border `rgb(224,168,58)`, radius 5, padding 11/9/11/10. Header "IMPORTANT - ABOUT THIS MOD" 12 px Bold `rgb(240,184,72)`. Body 14 px, line 20, `rgb(220,210,190)`. Link underlined in `rgb(240,184,72)`, which can open another game's detail page (`:1213-1274`).
- **Similar Games:** `Get-SimilarGames -Count 12` (`:1677`). Each row: a 160 x 75 thumbnail (`:1704`), title 13 px Medium with a white-to-`#C2CAD2` sheen, sub-line 9.5 px (control type in `#8a8a9a`, "+ FREE" in `#34D399`). Hover background `#1a1a24` and scale 1.05. Clicking opens that game (`:1681-1789`).
- **Row count:** four rows by default (`:1794`). A resize handler then sets the count to `(box height - 26) / 93`, with a minimum of 4 (`:1812`). The count follows the Game Info height, so the two columns line up.
- **Empty state:** "No similar games found", 12 px Medium `#666677` (`:1824-1829`).

### 1.9 Video strip

- A card with a thumbnail of 132 x 74 and a play button 40 x 28 (`Core/Modules/DetailView.Content.ps1:263`, `:334`). Subtitle: "See it in action on <provider>" (`:366`). The default label is "Watch VR gameplay" (`:221`).
- Only YouTube thumbnails are fetched by ID. Other hosts fall back to the game's header art (`Core/Modules/DetailView.Content.ps1:224-232`).
- Two videos sit side by side with a 14 px gap, each half width (`Core/Modules/DetailView.Page.ps1:1843-1859`).

### 1.10 README rendering

- **Section order** (`Core/Modules/DetailView.Page.ps1:1888-1969`):
  - **Preferred first:** "About this mod", "About", "Where to get the game", "What it installs", "Requirements", "Note".
  - **Skipped:** "How to use" and "More info". The code comment says "How to use" usually tells the user to double-click the Bat, which the Hub makes unnecessary (`:1879-1881`).
  - **Middle:** every other H2, in README order.
  - **Tail, grouped by pattern in this order:** related, communit, discord, support, donat, credit, deactivate, uninstall, deinstall (`:1908`).
  - **Override:** `<!-- hub:keep-order -->` turns sorting off, for READMEs that document two mods in sequence (`:1884-1889`).
- **Parsing** (`Core/Modules/Helpers.ps1:1399`, `Read-GameReadme`): H2 headings become sections. Keys starting with an underscore (`_tagline`, `_quip`, `_baseDir`, `_keepOrder`) are metadata, not sections (`Core/Modules/DetailView.Page.ps1:1915-1948`).
- **Rendering** (`Core/Modules/DetailView.Content.ps1:385-1050`):
  - The section heading comes from the page, so `##` lines inside the body are dropped.
  - Bullets become U+2022.
  - Code fences become monospace chips (`#1c1c24`, radius 4, Consolas, Cascadia Mono, Courier New).
  - A fence holding one launch line, such as `-dx11`, becomes a copy chip. "Copy" turns to "Copied!" on `#16301f` with `#5fcf80` text (`:699-750`).
  - A line starting `>>> ` becomes the quip box.
  - Tables are rendered as rows with `#1c1c24` fill.
  - Images resolve relative to the README folder (`_baseDir`, `:1912-1914`).
  - Bold and inline code render as runs (`Set-TextBlockWithLinks`, `:10-190`).
- **Width:** paragraphs are capped at `0.78 × window width`, clamped to 720 to 1040 px (`Core/Modules/DetailView.Page.ps1:47-57`).
- **Text sizes** (`Core/Modules/Window.Controls.ps1:327-331`): S 12/18, M 14/21, L 16/24 (font / line height). The default is M (`:295`). The S/M/L switch reflows the open page.
- **Local paths and web links** in the README become clickable (`Core/Modules/ReadmeLinks.ps1`). Web links must be http or https with no whitespace (`:2-11`). A path such as `%LOCALAPPDATA%\...` is resolved against known roots (`:69-107`). Path links become active only after a bounded folder index, one render turn after the page is shown (`Core/Modules/DetailView.Page.ps1:3404-3422`). Downloads use the known-folder GUID, not a hard-coded path (`Core/Modules/ReadmeLinks.ps1:221-233`).

### 1.11 Uninstall Guide and Uninstall now

- **Uninstall Guide** is a collapsible button in the uninstall box. Label "Uninstall Guide" with a v chevron that turns to ^ when open. Accent `#cc6655`. Its accessible name changes between "collapsed" and "expanded" (`Core/Modules/DetailView.Actions.ps1:1576-1880`, `:1824`, `:1840`, `:1857-1861`; `Core/Modules/DetailView.Page.ps1:2020`).
- **Guide header** (`Core/Modules/DetailView.Actions.ps1:1665-1671`): "How to safely remove the VR mod", "How to uninstall this Steam VR app" for separate Steam apps, or "How to remove this VR build" for standalone builds.
- **Guide steps** are generated by `Get-SafeUninstallSteps` (`Core/Modules/UninstallGuide.ps1:19-123`). Rules, in order:
  1. A separate Steam app: "Open Steam > Library ... Manage > Uninstall", and nothing else (`:27-33`).
  2. Explicit `UninstallSteps` in the catalog are used, unless they are the old shared-loader boilerplate that could delete other mods' files (`:34-41`).
  3. Otherwise steps are built from the markers (`ModFile`, `ModFileAlt`, `FlatVR*`, probe files) and branch on the mod type: Luke Ross R.E.A.L., BepInEx, MelonLoader, a standalone build, GTA V, NOLF2, RaiManager.
  4. Generic fallback: close the game first, remove only the verified mod files, and never uninstall the base game or delete its folder (last line of the function, near `:121`).
  5. Steam launch options that the mod added must be cleared, and only those (`:58-61`).
- **Numbered steps** are drawn as circles with accent at alpha 60 (`Core/Modules/DetailView.Actions.ps1:1740-1760`).
- **Uninstall now** runs the publisher's own uninstaller with `Wait = true` and refreshes that one game afterwards (`Core/Modules/DetailView.Actions.ps1:1367-1396`). It appears only when the entry names an uninstaller and that file is really on disk (`Core/Modules/DetailView.Page.ps1:2024-2026`; button at `Core/Modules/DetailView.Actions.ps1:1397-1575`). The Hub does not confirm first; the uninstaller's own UI does.
- **Two-mod uninstall:** one chooser. For two removable mods, Yes removes A, No removes B, and Cancel changes nothing. For one, OK and Cancel (`Core/Modules/DetailView.Actions.ps1:1287-1365`).

### 1.12 Quip, Discord and Support

- **Quip** (`Core/Modules/DetailView.Page.ps1:2104-2175`): the catalog `Quip` wins over a README `_quip` section. The box has fill `#16161e`, a 3 px accent left bar, radius 6, text 14 px SemiBold italic in the accent colour. Hover: fill `#2b2b36`, scale 1.03 across and 1.10 down, accent glow blur 18 opacity 0.7.
- **Discord block** (`Core/Modules/DetailView.Page.ps1:2097-2102`). Built from one central registry keyed by the stable catalog Id, not the title or array position (`Core/Modules/DiscordChannels.ps1:1-30`). Heading "Discord discussion & support". Some channels require joining the Flat2VR server first (`:308-309`; the builder is at `:284-300`).
- **Support** (section 1 table row 19).

### 1.13 Launch: Start in VR

`Start-GameInVR` (`Core/Modules/DetailView.Launch.ps1:94-839`). The steps, in order:

1. **Recently played.** Records the title first, keeps the latest 8, and rebuilds the row (`:99-118`).
2. **Hub window.** Minimises the Hub before launching, so the game gets focus. "We minimise rather than close" (`:575-585`).
3. **Launch wrapper.** If the entry has `HubLaunchScript`, the Hub runs its own controller first (`:585-605`).
4. **Direct launch first.** Current, Depot or Legacy exe from the detected folder (`:190-290`).
5. **Steam fallback.** `steam://rungameid/<id>` only if no direct route worked and the entry does not set `NeverSteamLaunch` (`:826-833`).
6. **Folder gone.** A message "<title> install folder no longer exists", the state is cleared, and a full rescan runs only if the user had run one before (`:785-806`).
7. **Never a silent flat start.** With `NeverSteamLaunch`, the Hub says "Cannot start in VR" and does not fall back (`:811-822`).
8. **No SteamVR or OpenXR check.** A search of `Core/` for `vrserver` and `vrmonitor` finds nothing. The only SteamVR text is a catalog description, "SteamVR must be running" (`Core/Modules/Catalog.ps1:39`). The Hub relies on the game's own launcher and on Steam.

Failures go through `Write-HubActionFailure` (`Core/Modules/Helpers.ps1:3403-3440`). It writes `hub-errors.log` under the runtime logs folder, then shows a modal unless `-Quiet` is set.

### 1.14 Installer console UX

The console is the per-game Bat's own window. It runs `<Game>-core.ps1` (or the wrapper `Core/Run-Installer.ps1`) and writes its output to `Core\Logs\<Title>-<timestamp>.log` (`Core/Run-Installer.ps1:1-5`, `Core/Modules/Helpers.ps1:3478` `Start-LoggedInstaller`).

| Element | Convention | Source |
|---|---|---|
| Launcher | `title <Game> Installer`, then `color 0B` (bright aqua on black). Most launchers use it. | `Core/AlbaVR/START_INSTALLER.bat:1-4` |
| Header | `Clear-Host`, a 60-character rule in magenta, the game name in the title line, then a sub-line. Twenty installers use magenta. | `Core/AlbaVR/AlbaVR-core.ps1:19` |
| Step header | `--- [n/t] Title ---` in cyan, preceded by a blank line. The count is fixed per installer (for example 1/4 to 4/4). | `Core/AlbaVR/AlbaVR-core.ps1:20`, `:46`, `:116`, `:170` |
| OK | ` [OK] text` in green | `Core/AlbaVR/AlbaVR-core.ps1:21` |
| Warning | ` [!!] text` in yellow. The install continues. | `Core/AlbaVR/AlbaVR-core.ps1:22` |
| Failure | ` [XX] text` in red. The step stops or falls back. | `Core/AlbaVR/AlbaVR-core.ps1:23` |
| Working | ` [..] text` in gray | `Core/AlbaVR/AlbaVR-core.ps1:24` |
| Prompt | ` >>> text ` in black on yellow, then `Read-Host`. Default "Press Enter to continue...". | `Core/AlbaVR/AlbaVR-core.ps1:25`. About 170 installers define it. |
| Finish | A green "Installation complete!" block between magenta rules, with a "Launch with Start in VR" hint and the platform. Ends with `Pause-User "Press Enter to exit."`. | `Core/AlbaVR/AlbaVR-core.ps1:182` onward |

**Marker counts** across the installers: `[OK]` 326, `[!!]` 218, `[..]` 162, `[XX]` 147. Step counts vary by installer, from two to four.

**Shape of a typical installer** (Alba, 4 steps): locate the game, choose a platform (menu 1/2), download and extract, then a desktop shortcut and summary.

**Known weakness:** the Alba installer prints "AlbaVR.dll not found - check ... manually", and still ends with "Installation complete!" (`Core/AlbaVR/AlbaVR-core.ps1` steps 3 and finish).

### 1.15 Downloads and fallbacks

- **Progress** (`Core/Modules/InstallerSafety.ps1:505-557`): `HttpWebRequest`, user agent `PCVR-Mods-Hub`, connect timeout 30 s, read/write 120 s, 128 KB buffer. Progress updates every 250 ms: "X MB of Y MB (pct%) - rate MB/s".
- **Ladder** (`Invoke-SafeDownload`, `Core/Modules/InstallerSafety.ps1:558-789`):
  1. The primary URL. The file is checked before it may replace anything (`Test-DownloadedPayload`, `:381-453`, plus zip anchors `:454-504`).
  2. Alternate URLs, **only if an expected SHA-256 is pinned** (`:629`).
  3. GitHub release API lookup for the matching asset, 10 s timeout (`:697`; success messages `:725` and `:740`).
  4. Manual drop-in (section 1.16 and the prompt below).
- **Verification** (`:600-630`): a pinned SHA-256 is checked (`:606`), and the file is staged, then moved over the target only after the check passes.
- **Zip safety** (`Test-ZipArchiveEntrySafety`, `:834-872`): entries are validated before extraction.
- **Manual prompt** (`Invoke-InstallerFallback`, `Core/Modules/InstallerSafety.ps1:119-380`):
  - Banner: "Manual step needed: <action>", yellow rule.
  - "What happened?" One plain sentence. For downloads: "the automated download of X is currently not possible" (`:159-164`).
  - "What to do" (`:232-270`): (1) open the page after the user presses Enter, with the URL shown; (2) drag the file onto the window or paste its path; (3) choose [R]etry, [S]kip, or [O]pen (`:276-285`).
  - A dropped file is copied to a staging name, checked by a validator, and only then moved over the target. A failed file says "The existing file was kept" (`:295-309`).
  - "Please answer ..." lists only the options on offer (`:370`).
- **Depot path menu** (`Resolve-DepotPath`, `Core/Modules/InstallerSafety.ps1:2270-2292`): two options, "open Steam Console and re-run the command" (copied to the clipboard), or "paste the depot path". Empty input or three failed attempts return to the menu. The comment says: "The error surface does not end the installer."

### 1.16 Locate, remembered folder and Clear

- **Remembered folder order** (`Get-GameFolderInteractive`, `Core/Modules/InstallerSafety.ps1:2198-2290`):
  1. A remembered path from the checksummed state (`Get-PCVRRememberedGameFolder`, `:1868-1876`; `Save-HubRememberedGameFolder`, `:1885-1925`). Printed as "Using the remembered install folder".
  2. A path handed over by the recovery screen.
  3. A prompt. "Couldn't auto-detect the install folder for: <game>", with examples and a help link. It accepts "S" to skip deliberately.
  4. If the folder lacks the probe file: "Is this really the install folder? [Y]es accept anyway / [N]o try again".
  5. On accept, the folder is saved with `-UserSelected`.
- **State** is `user_located` and `installed_path`, mirrored under LocalAppData so a Hub update does not forget a custom location (`Core/Modules/HubState.ps1` around `:886`; `Core/Modules/DetailView.Actions.ps1:66-130`).
- **Locate Game** (`Core/Modules/DetailView.Actions.ps1:726-1110`). Tooltip: "Choose its folder. The Hub checks known VR mod files first, then the game executable." Plan A: the VR mod is found, so the game is VR Ready. Plan B: only the game is found, so ask "Link this folder anyway so <game> shows as installed?" The success messages say "It will show as installed. Install the VR mod to make it VR Ready."
- **Exe picker** (`Core/Modules/DetailView.Actions.ps1:15-65`): a list dialog titled "Select the game exe for <title>", with the question "Which file launches the game?".
- **Clear** (`Core/Modules/DetailView.Actions.ps1:66-130`). Tooltip "Forget the located folder for this game." Confirm: "Forget the saved location for <title>? It will go back to 'not found' until you locate it again." Then all state is removed and read back to make sure, and the page says "<title> location cleared."

### 1.17 Update checks and the version cache

- **Where the cache lives:** LocalAppData, not `Core\`. The code says "volatile update state belongs in LocalAppData", so a release ZIP never ships a user's position (`Core/Modules/Filter.ScanSources.ps1:1603-1607`). Cache files named `.gh_version_cache`, `.web_version_cache` and `.gh_check_rotation` (`Core/Modules/Filter.ScanSources.ps1:1171`, `:1463`, `:1608`).
- **Format:** JSON keyed by `owner/repo[#pre]` or by URL, with `checked` (UTC round-trip) and `tag` or `ver` (`.gh_version_cache` in the clone).
- **TTL by source** (hours):

| Source | Function | TTL | Source ref |
|---|---|---|---|
| Thunderstore package | `Get-ThunderstorePackageCached` | 1 | `Core/Modules/Filter.ScanSources.ps1:874` |
| Codeberg latest tag | `Get-CodebergLatestTagCached` | 6 | `:1052` |
| GitHub latest tag | `Get-GithubLatestTagCached` | 6 | `:1167` |
| GitHub latest commit | `Get-GithubLatestCommitCached` | 6 | `:1366` |
| Web version | `Get-WebVersionCached` | 6 | `:1459` |

- **Stale fallback:** when the network check fails, the last known tag is used (`:1196-1205`).
- **Circuit breaker:** after the first failure in one scan, no more network calls are made in that scan (`:1201`; the same rule for web sources at `:1481`).
- **Deadline:** a scan-wide soft deadline stops network probing (`$global:PrewarmDeadline`, `:1623`). Repositories not reached keep their last value and get an early slot next time, through the rotation file (`:1603-1625`).
- **Update state** in the library: a blue card (`#2563eb`) with a blue glow ring (`Core/Modules/Filter.Scan.ps1:1773-1775`).
- **Update badge on the tile:** orange `#cc6600`, text "UPDATE" (`Core/Modules/DetailView.Tiles.ps1:488`). That disagrees with the blue card. See section 1.21.
- **Startup order** (`Start PCVR Mods Hub.bat`): the splash starts the Hub first; only after the Hub is visible do `Core/Update-Hub.ps1 -Silent` and `Core/Prefetch-Versions.ps1` start, detached and minimised. The bat says "MUST NOT use start /b".

### 1.18 Hub self-update

- `Core/Update-Hub.ps1` checks GitHub silently and writes a marker for the live banner (`:1-20`).
- **Failure:** rolls back the partial download, then relaunches the Hub. The marker stays, so the banner offers a retry. No message box on that path (`:470-500`).
- **Success:** relaunches the Hub. A message box appears only if the relaunch fails: "Update installed successfully, but the Hub could not be relaunched automatically" (`:436-441`).

### 1.19 Uninstall safety and ownership

- **Ownership manifest** (`Core/Modules/OwnedModFiles.ps1`): when a payload is installed, a CSV `.pcvrhub_<id>_ownership.csv` lists exactly the files the package copied, and backups go to `.pcvrhub_<id>_backup`. Uninstall removes only manifest files and restores backups. A file the user has edited is kept, and the install fails closed on it (`Install-OwnedModPayload` parameter comments, around `:11-30`; `Uninstall-OwnedModPayload`, `:141-193`).
- **Shared loaders are not owned:** BepInEx or MelonLoader installed outside the helper are never removed. The Hub cites this as the reason.
- **Standalone builds** (`StandaloneVR`): the guide says to back up saves, settings, dumps and ROMs first, check the folder is the dedicated copy, and never delete the original game (`Core/Modules/UninstallGuide.ps1`, standalone branch).

### 1.20 Game-specific extras

- **Elden Ring save strip** (`Core/Modules/EldenRingSaveUI.ps1:407-443`, `New-EldenRingSettingsControl`). A row in a `#13131a` box, border `#343440`, radius 7, padding 12/7. Captions "VR MODE" and "EDIT" in 10 px bold `#737382`, then mini actions: "Hotbite 3D", "ERVR Full 3D", "Hotbite config", "ERVR config". Each action has a tooltip. Dialogs use the Hub's own message wording, for example "Close Elden Ring completely before changing its save set." It is the pattern to copy: an extra control strip, placed after the action row, never a separate page.
- **Game-specific descriptions and notices** live in code (`Core/Modules/DetailView.Page.ps1:1104-1154`). Keep them in data (section 1.21).
- **Power tiers per title** live in code (`Core/Modules/Helpers.ps1:2159` onward). Keep them in data.
- **Exe picker** (section 1.16).

### 1.21 Gaps and drift in the Hub (do not copy)

1. **No runtime check before launch.** Section 1.13 step 8. We add one (section 2.11).
2. **The Steam Theatre button only shows instructions.** `Core/Modules/DetailView.Power.ps1:828-1058` builds a tooltip mock of a SteamVR setting. Nothing writes SteamVR's settings: `vrsettings` does not appear in `Core/`. The AlbaVR README says the installer "turns off Desktop Game Theatre for you" (`Core/AlbaVR/README_AlbaVR.md`, "How to use"), but `Core/AlbaVR/AlbaVR-core.ps1` contains no such code. Keep the README honest, and keep Theatre as instructions unless the owner decides otherwise.
3. **Installed checks use file presence.** The Hub tests `ModFile` exists (`Core/Modules/DetailView.Page.ps1:3030-3033`), not its hash or version. We check media by hash and tools by exe name and version (section 2.5).
4. **Installs can say "complete" after a failed check** (section 1.14, Alba). We do not (section 2.9).
5. **"Skip" can leave a partial install** (`Core/Modules/InstallerSafety.ps1:279`, "install may be incomplete"; Alba's skip message "questionable result"). We allow skip for optional steps only, and a skipped install is not "Installed" (section 2.9).
6. **Update colour disagrees across the UI:** tile `#cc6600` (`Core/Modules/DetailView.Tiles.ps1:488`), library card blue `#2563eb` (`Core/Modules/Filter.Scan.ps1:1773`), detail status text amber `#ffb060` (`Core/Modules/DetailView.Page.ps1:920-925`). We use one blue for update (section 2.7), matching 02.
7. **Labels differ:** tile "Install", detail "Install Mod", external "Get Installer". We use one set (section 2.7).
8. **Data in code:** per-title descriptions (`Core/Modules/DetailView.Page.ps1:1104-1154`), the tier table (`Core/Modules/Helpers.ps1:2159`), and per-title uninstall branches (`Core/Modules/UninstallGuide.ps1`). We put all of it in `game.toml` and `install.toml`.
9. **README instructions can conflict with the buttons.** The Hub hides "How to use" for this reason (`Core/Modules/DetailView.Page.ps1:1879-1890`). Our READMEs should not tell users to run files by hand, except as a fallback.
10. **Hover-only information** was removed from the Hub (`Core/Modules/DetailView.Page.ps1:495-501`, `:2427-2430`). Keep state visible at rest.

---

## 2. AladdinsCastle equivalent

### 2.1 Mapping

Per-game file names below (`game.toml`, `install.toml`, `README.md`, `art/`, `setup/`) sit in each game's folder, `games/<id>/`. The `user/`, `installed/` and `tools/` folders sit in the install root (see [config-spec.md](../config-spec.md) section 2).

| Hub element | AladdinsCastle equivalent | Data |
|---|---|---|
| Card state pill and button | Game state (game-packages section 2): Needs your files, Needs an emulator, Ready to install, Installed, Update available, Coming soon | `install.toml` variants, media and tools |
| Title pills (VR READY, INSTALLED, FREE, WIP) | Quality badge (TRUE 3D, THEATRE, PLANNED) first, then the state pill, then `[hub].badges` and genre | `routes.vr.best`, variant `quality` and `status`, `[hub].badges` |
| VR MOD and CREATED BY | MANUFACTURER, HARDWARE, YEAR. DEVELOPER only if different | `manufacturer`, `hardware` (vocab label), `year`, `developer` |
| Get on Steam hint | "What you need" checklist | `needs`, `[[media]]`, `installed_when` |
| Install Mod and the installer Bat | Install (declarative steps, in-app console) | `install.toml` steps |
| Locate Game, Re-locate, Clear | Find my files, Forget this folder (with confirm) | State under `user/` |
| Start Depot, Start Legacy, Two-mod choices | Variant picker and "Start <variant>" | `install.toml` variants |
| Reinstall Mod | Reinstall | Same steps over the install |
| Update | Update to <version> | Pinned tag, with cached upstream check |
| Uninstall Guide and Uninstall now | Uninstall (previewed, manifest-based), plus Quest uninstall | `installed/<game>/<variant>/` manifest |
| PC Power | Performance card (OPEN) | Optional `perf` in variant |
| Flat / VR switch | None. Theatre is a separate variant, not a toggle (true 3D is the default, theatre is fallback only; [workshop.md](../workshop.md) D1) | Variant `quality = "theatre"` |
| Mod Page | Upstream page | `upstream` in variant |
| Open in Steam | Only for PC releases on Steam. OPEN: add `store.steam_appid` | Optional |
| Steam Theatre tip | SteamVR Dashboard tip, instructions only | Static text |
| Game Info, Notice | About, and "IMPORTANT - ABOUT THIS SETUP" | README, `notice` (PROPOSAL, new field) |
| Similar Games | Similar games (section 2.13) | Catalog fields |
| Video | Video | `video` (game-packages section 3) |
| README | README (section 2.3) | `README.md` |
| Discord block | Community links, from the upstream README only | Optional |
| Quip | Optional quip | `quip` (PROPOSAL; `[hub].blurb` stays the tile line) |
| SUPPORT THE MOD | SUPPORT THE AUTHOR, only with a link the upstream publishes | `support_url` in variant (PROPOSAL) |

### 2.2 Detail page wireframe: Time Crisis

```
< Back to library
+----------------------------------------------------------------------------+
|                                                                            |  hero, 360 px, art/banner.png
|                                                                            |  fallback: marquee.png, then a generated card
+----------------------------------------------------------------------------+
Time Crisis   [TRUE 3D]  [LIGHT GUN]  [COVER]  [ROOMSCALE]  [NEEDS YOUR FILES]
MANUFACTURER Namco  |  HARDWARE Namco Super System 22  |  YEAR 1995  (|  DEVELOPER ...)

VARIANT   ( DR-89 PCVR  * default )  ( DR-89 Quest 3 )  ( Our true 3D - planned )  ( Theatre - fallback )

| amber bar   Needs your files: timecris.zip not found. We never download it.

 WHAT YOU NEED
   Game files    timecris.zip  MAME 0.271+ "World TS2 Ver.B"     Missing     [Find my files] [Search drives]
   VR runtime    OpenXR runtime active: SteamVR                   Found       [Open runtime setup]

 [ Find my files ]  [ Upstream page ]                                         (primary + companions)

 SETTINGS FOR THIS GAME   Cover [ Grip | Duck ]  Laser [ On | Off ]  Gun angle [-60 ... +60]  Hand [ Right | Left ]

 PERFORMANCE   SOLID   (optional, OPEN)

+-----------------------------------------+  +------------------------------+
| ABOUT                                   |  | SIMILAR GAMES                |
|  blurb, then README About               |  |  thumb  Title     GUN  ...   |
|  [IMPORTANT - ABOUT THIS SETUP] notice  |  |  thumb  Title                |
+-----------------------------------------+  +------------------------------+

 [VIDEO]  Watch VR gameplay  -  See it in action on YouTube

 CONTROLS   (generated from the control set, section 2.3)
 WHAT IT INSTALLS   (name, version, author, licence, link)
 README   About / Requirements / Settings & hotkeys / Troubleshooting / Credits
 UNINSTALL  (only when installed; previews the list first)
 quip (optional)
```

When installed, the primary button becomes `Start in VR ▶`, the state pill becomes INSTALLED, and the checklist collapses to one line of green "Found" items.

### 2.3 Section spec

| # | Section | Rule |
|---|---|---|
| 1 | Hero | `art/banner.png`, then `art/marquee.png` centre-cropped, then a generated title card in `colour`/`accent`. Never blank (frontend.md section 5). Above 1040 px wide: frozen width, centred, accent edges, as the Hub (`Core/Modules/DetailView.Page.ps1:258-355`). |
| 2 | Title row | Title 28 px bold, gradient as the Hub. Quality badge first (2.4), then the state pill, then `[hub].badges` in vocabulary order: GUN, WHEEL, HANDLEBARS, BIKE, ROOMSCALE, SEATED, 2 PLAYERS, WIP, QUEST STANDALONE. |
| 3 | Meta strip | MANUFACTURER, HARDWARE (from `data/vocab/hardware.toml` label), YEAR. DEVELOPER only if it differs from the manufacturer. Same 9 px and 13 px type as the Hub. |
| 4 | Variant picker | Section 2.4. |
| 5 | State line | One sentence in the state's colour with a 3 px bar and an alpha tint (section 2.7). |
| 6 | What you need | Section 2.5. Visible in every state until everything is green. Collapses to a line once installed. |
| 7 | Action row | Section 2.6. |
| 8 | Settings strip | Per-game settings (section 2.10). Only fields that the variant supports are shown. |
| 9 | Performance | Optional card (section 2.15). |
| 10 | About and Notice | The README "About" section, with `blurb` as the first line. The notice box uses the Hub's amber (section 1.8). |
| 11 | Similar games | Section 2.13. |
| 12 | Video | `video`, as in section 1.9. |
| 13 | Controls | A table generated from the control set (`setup/controls.toml` for ghost controls, `setup/gun.toml` for guns, both per [config-spec.md](../config-spec.md) section 5). Columns: action, input, notes. Pause overlay controls from [controls.md](../controls.md) section 3 appear as a footnote on every setup. |
| 14 | README sections | Same parser and order rules as the Hub (section 1.10), with our nine-section template ([game-packages.md](../game-packages.md) section 5) as the recommended order. |
| 15 | What it installs | One row per component: name, pinned version, author, licence, upstream link, role (port code, engine, tool). Taken from `upstream`, `license` and `meta.sources`. |
| 16 | Uninstall | Only when installed (section 2.12). |
| 17 | Quip | Optional, last. |

**Controls table, gun setups** (generated, example for Time Crisis): fire = either trigger; take cover = duck (physical) or grip (grip mode); insert credits = A (right hand); laser = B (right hand); pause = left menu; recenter and set standing height = X (left hand). These come from the README and [controls.md](../controls.md) section 1.2. The generator reads the same data, so the README and the page cannot disagree.

**Controls table, racing setups** (example for Rave Racer, from `setup/controls.toml`): steering wheel with range 270 degrees, two-position shifter (low, high), accelerator and brake from the triggers with a dead zone of 0.04, view-change button. Rows come from the `[[element]]` list.

### 2.4 Variant picker

- **Which variants show:** all variants in `install.toml`, in this order: installed, stable, wip, planned. Planned variants are shown greyed and disabled.
- **Default selection:** the installed variant if any, else `default` from `install.toml` (the Hub picks the best available, frontend.md section 2).
- **Each chip** shows the short variant name, the quality tag (TRUE 3D or THEATRE), and a status dot: installed = green, ready = grey outline, wip = amber, planned = dashed grey.
- **Switching** re-renders the checklist, buttons and variant settings. Variants install side by side in `installed/<game>/<variant>/`, so two can be installed at once (the Hub's "Play X" pattern).
- **Quality on the card** uses the best available variant's `routes.vr.best` ([02 section 2.3](02-game-cards.md)). The detail page shows each variant's own quality, so the two can differ for a planned variant. That is intended.

### 2.5 What you need

Four kinds of row. Each row shows a status word and buttons. A row that is not needed for the chosen variant is hidden, not greyed.

| Row | Status words | Buttons | Rule |
|---|---|---|---|
| Game files (one row per `[[media]]`) | **Found** (hash matches the set), **Found, not verified** (name matches, hash not checked yet), **Missing**, **Wrong version** (hash of another revision) | **Find my files** (file or folder picker, or drop), **Search drives** | Hash checks use the set's known hashes (OPEN: hashpack or MAME software-list XML, [config-spec.md](../config-spec.md) section 10). Search lists candidates with name, size and hash result, and uses none without a click. A rejected file says which set it did not match and never replaces an existing file. |
| Tools (one row per `needs.tools` entry) | **Found** (exe name and version match), **Found, older** (older than the pin), **Missing** | **Install emulator** (or "Install adb") opens the three-route dialog; **Locate**, **Search** | Routes as [frontend.md](../frontend.md) section 4: install from the official release only, with a pinned SHA-256 and into `tools/<id>/`; locate by picking an exe, verified by name and version; search standard install paths. Older found tools get a side-by-side copy, never an upgrade of the user's own install. |
| Headset (Quest variants, `needs.headset = "developer-mode"`) | **Connected** (`adb devices` state `device`, model shown), **Accept the prompt in the headset** (state `unauthorized`), **Not connected** (no entry) | **How to enable developer mode** (opens in-app steps), **Check again** | Developer mode itself cannot be read over adb, so the page says "inferred from the connection". |
| VR runtime (all PCVR variants) | **Found: <runtime>** (SteamVR, Meta Link or Virtual Desktop as the active OpenXR runtime), **Not found** | **Open runtime setup** | The Hub detects these runtimes ([frontend.md](../frontend.md) section 1). |

Rules that apply to all rows:
- Install stays disabled until every required row is green (media, tools and runtime for PCVR; headset for Quest).
- Each row is re-checked when the window regains focus, and on "Check again". No background scanning of the whole PC.

### 2.6 Button matrix

The primary button is chosen by precedence, as in [02 section 2.3](02-game-cards.md): **planned > update > installed > needs files > needs emulator > ready to install**. Companion buttons are listed in the order shown.

| State (variant) | Primary button | Companions (in order) | Hidden |
|---|---|---|---|
| Coming soon (planned) | **Coming soon** (grey `#8a93a6`, disabled, opens the roadmap item) | Upstream page | Install, Start, Reinstall, Uninstall |
| Update available | **Update to <version>** (blue `#2563eb`) | **Start in VR ▶** (green), Reinstall, What's new (upstream release notes), Uninstall | Install |
| Installed (PCVR) | **Start in VR ▶** (green, 1.5 px) | Reinstall (`#5a6aa8`), Open folder, Settings, Uninstall (`#cc6655`), Upstream page, Open in Steam (if any) | Install |
| Installed (Quest) | **Start in VR ▶** when the headset is connected; **Connect your Quest** when not | Reinstall on Quest, Uninstall from Quest, Open folder | Install to Quest (until uninstalled) |
| Needs your files | **Find my files** (amber `#f59e0b`) | Search drives, Upstream page | Install, Start |
| Needs an emulator | **Install emulator** (info blue `#7ab5ff`) | Locate, Search | Install, Start |
| Ready to install | **Install** (accent neon, tint 0.06, as 02) | Upstream page | Start, Reinstall |
| Ready to install (Quest) | **Install to Quest** (accent neon) when connected; **Connect your Quest** when not | Upstream page | Start |
| Installing | Progress button, "Installing 3 of 5" (disabled) | **Cancel** (only between steps; see 2.8) | Everything else |
| Last install failed | **Retry install** (border amber `#e0b060`) | Open log, Open install folder, Discard partial | Start |

Rules:
- **Hiding, not disabling**, except for the planned state, where the disabled primary explains itself.
- Companion buttons use the Hub's Reinstall and Uninstall look, as in section 2.7.

### 2.7 Labels and colour tokens

**Labels.** The Hub uses Title Case on buttons ("Install Mod", "Get on Steam"). We keep that. Button labels are exactly as in the table above, with `▶` (U+25B6) after Start.

**Tokens** (taken from the Hub, unless marked PROPOSAL). Define them once in the theme:

| Token | Value | Use | Hub source |
|---|---|---|---|
| `--page` | `#0f0f12` | Window background | `Core/Modules/Window.Layout.ps1:12` |
| `--header` | `#0d0d0f` | Header strip | `Core/Modules/Window.Layout.ps1:24` |
| `--card` | `#16161a` | Card base, pill fills | `Core/Modules/DetailView.Page.ps1:550` |
| `--box` | `#13131a` | Game Info, similar, uninstall boxes | `:1197` |
| `--box-line` | `#222230` | Box borders | `:1199` |
| `--rule` | `#1e1e26` | Meta strip rule and separators | `:800`, `:867` |
| `--label` | `#666677` | 9 px labels | `:813` |
| `--value` | `#dddddd` | Meta values | `:823` |
| `--body` | `#c8c8d4` | Description text | `:1313` |
| `--heading` | `#f0f0f4` | Section headings | `:1291` |
| `--muted` | `#8a8f99` | Secondary text | `:917` |
| `--ready-fill` | `#161d18` | Installed and ready buttons | `:2293` |
| `--ready-line` | `#5fa873` | Ready borders | `:2295` |
| `--ready-text` | `#88dd99` | Ready text | `:2296` |
| `--installed-line` | `#4d8a5e` | INSTALLED pill border | `:665` |
| `--update-blue` | `#2563eb` (line `#6da3ff`) | Update button, Get on Steam | `:2310-2312` |
| `--amber` | `#ffb060` | Update status text | `:924` |
| `--notice` | `rgb(224,168,58)` (text `rgb(240,184,72)`) | Notice box | `:1216-1227` |
| `--reinstall-line` | `#5a6aa8` | Reinstall border | `:2423` |
| `--uninstall` | `#cc6655` | Uninstall Guide and Uninstall | `Page:2020` |
| `--info` | `#0e1c21` / `#4a9ab0` / `#b8dde8` | Mod Page button (fill / line / text) | `:3270-3283` |
| `--steam-line` | `#5078cc` (fill `#101a30`, text `#b8cdf0`) | Open in Steam | `:3352-3358` |
| `--gold` | `#cdb77a` | Active mode (Flat / VR) | `Core/Modules/DetailView.Actions.ps1:351-375` |
| `--free` | `rgb(52,211,153)` | FREE pill (02 uses `#34d399`) | `:728-735` |
| `--wip` | `rgb(248,113,113)` | WIP pill | `:751-758` |
| `--quality-true3d` | `#34d399` | TRUE 3D badge (as 02 section 2.2) | PROPOSAL, matches 02 |
| `--quality-theatre` | `#7ab5ff` | THEATRE badge (as 02) | PROPOSAL, matches 02 |
| `--quality-planned` | border `#40404e`, text `#9a9aa8` | PLANNED badge (as 02) | PROPOSAL, matches 02 |
| `--needs-files` | `#f59e0b` | Needs your files (as 02) | PROPOSAL, matches 02 |
| `--console-ok` | `#5fcf80` | `[OK]` and copy confirmation | `Core/Modules/DetailView.Content.ps1:735-750` |
| `--console-warn` | `#e0b060` | `[!!]` | Uninstall UAC note colour, `Core/Modules/DetailView.Actions.ps1:1483` |
| `--console-fail` | `#f87171` | `[XX]` | WIP red |
| `--console-work` | `#8a8f99` | `[..]` | muted |
| `--console-step` | `#6ec6e6` | `--- [n/t]` headers | PROPOSAL (the Hub uses console cyan) |
| `--console-bg` | `#0c0c10` | Console panel | PROPOSAL |
| `--mono` | `Consolas, Cascadia Mono, Courier New` | Console, code, paths | `Core/Modules/DetailView.Content.ps1:109` |

**Card and detail agree on state colours** (02 section 2.3): Needs your files amber, Needs an emulator info blue, Update blue, Installed green, Ready to install accent, Coming soon grey.

**Detail page difference from the card:** the card's installed button shows "Ready" at rest, and the detail page shows "Start in VR ▶" at rest with the INSTALLED pill in the title row. The Hub's detail page makes the same choice (`Core/Modules/DetailView.Page.ps1:2285-2288`). The card choice is left to the owner (02 DIVERGE).

**Accent use:** the accent colours the title-row pills, the Game Info bar, the hero edges, the quip and the default install button. It never colours state.

### 2.8 Install in the app: the console

**Where it appears.** The console replaces the "What you need" card while an install runs. When the install ends it stays below the action row as "Last install", collapsible, until the next action.

**Layout.**
- Monospace, `--mono`, 13 px at M (scales with S/M/L), line height 18, `--console-bg`, border `--box-line`, radius 6, maximum height 360 px, auto-scroll to the newest line, a "Copy log" button.
- Opening block: a rule of 60 characters (or a 1 px line) in the accent, the variant title, and "AladdinsCastle installer".

**Lines.** One event type each, rendered as in the Hub's console:

| Event | Format | Colour |
|---|---|---|
| step | `--- [2/4] Extract ---` | `--console-step` |
| ok | ` [OK] Extracted 412 files to installed/timecris/dr89-pcvr/` | `--console-ok` |
| warn | ` [!!] 3 files differ from the package; kept your copies (list below)` | `--console-warn` |
| fail | ` [XX] timecris.zip is not the set Time Crisis World TS2 Ver.B` | `--console-fail` |
| work | ` [..] Checking the archive layout...` | `--console-work` |
| detail | Two-space indent, paths and URLs, e.g. `  From: <url> [GitHub release]` | `--muted` |
| prompt | ` >>> Press Continue to start the install ` as a yellow pill (`#facc15` fill, `#111` text) with a Continue button; Enter also continues | Yellow pill |
| done | Closing rule and "Install finished" in `--ready-text`, with the next action | `--ready-text` |

**Log.** Every line is also written to `user/logs/<game>-<variant>-<timestamp>.log`, with the same text. The Hub writes its installer output to `Core\Logs\<Title>-<timestamp>.log` for the same reason: a bug report can attach it (`Core/Run-Installer.ps1:1-5`).

**Steps.** The steps come from `install.toml` in order, with a preflight before the console opens (section 2.5). Each step's header is `--- [n/t] Title ---`. The count `t` is the number of steps in the variant. Preflight is not counted.

**Step kinds and their console lines** (PROPOSAL, from [frontend.md](../frontend.md) section 3):

| Step kind | Header | Typical lines |
|---|---|---|
| `locate-package` | Find the <package> | ok: name and folder. fail: not found, with "Find my files" |
| `github-release` | Download <name> <version> | detail: `From: <url> [GitHub release]`. ok: "Downloaded (SHA-256 verified)". warn: "Source failed" with the next source. Manual drop-in on failure |
| `download` | Download <name> | As above, with the pinned SHA-256 |
| `extract` | Extract | ok: file count. warn: files that differ and were kept. fail: unsafe archive entry |
| `copy-media` | Set up your <media> | ok: "matches the set (hash ok)". ok: "Linked ..." or "Copied ..." (copy or link, whichever the variant asks for) |
| `write-config` | Write <file> | ok: "Wrote N settings". Unrelated lines are kept (the Hub's INI helper keeps comments and other settings, `Core/Modules/DetailView.Actions.ps1:131-180`) |
| `shortcut` | Create shortcuts | ok: each shortcut. warn: "could not create" (not fatal, as the Hub's Alba) |
| `adb-install` | Install on the headset | detail: the `adb` command. ok: "Installed on Quest 3". fail: the adb message, with the log |
| `run` | Run <tool> | prompt first: "This runs <tool>. Continue?" (frontend.md section 3) |
| verify | Check the install | ok: `installed_when` is true. fail: not verified, and the install is not marked installed |

**Sample console: DR-89 PCVR, with one warning.**

```
============================================================
  Time Crisis - Time Crisis VR by DR-89 (PCVR)
  AladdinsCastle installer
============================================================

--- [1/4] Find the VR build ---
 [OK] TimeCrisisVR-1.2-windows-x64-norom.zip in Downloads (no game files inside)

--- [2/4] Extract ---
 [OK] Extracted 412 files to installed/timecris/dr89-pcvr/
 [!!] 3 files differ from the package; kept your copies
      settings.ini, quest-options.cfg, controls.ini

--- [3/4] Set up your Time Crisis files ---
 [OK] timecris.zip matches the set (hash ok)
 [OK] Linked roms/timecris.zip (no second copy written)

--- [4/4] Write settings and shortcut ---
 [OK] Wrote quest-options.cfg (cover: duck, laser: on, gun angle: -10)
 [!!] Desktop shortcut not created: file in use. Retry from Settings.

 >>> Installed with 1 warning. [Start in VR ▶]
```

Note the verify step: the sample passes `installed_when`, so it is marked **Installed**. A warning does not block it, but the state line says "Installed, with 1 warning".

**Cancel.** Allowed only between steps. Cancelling runs the uninstall rules for what the install has written so far (section 2.12).

**Quest sample.**

```
--- [1/3] Check the headset ---
 [OK] Quest 3 connected (adb state: device)
--- [2/3] Install on the headset ---
 [..] adb install TimeCrisisVR-quest.apk
 [XX] The headset refused the install. See the log for the reason.
      Usually: not enough free storage, or an APK built for another device.
 >>> Install stopped at step 2 of 3. Nothing was marked installed.
```

### 2.9 Failure, manual fallback and recovery

**Download ladder** (PROPOSAL, inheriting the Hub's order):
1. The pinned URL, with HTTPS, a user agent, 30 s connect and 120 s read timeouts. Progress as "X MB of Y MB (pct%) - rate MB/s" every 250 ms.
2. Alternate URLs, only when a SHA-256 is pinned. Otherwise they are not tried.
3. A GitHub release lookup for the matching asset, 10 s timeout.
4. Manual drop-in.

Each file is verified by type, size and SHA-256 before it may replace a file. The staged copy replaces the target only after the check passes (Hub: `Core/Modules/InstallerSafety.ps1:381-453, 790-833`).

**Manual step panel** (PROPOSAL, from `Invoke-InstallerFallback`):
- Banner "Manual step needed: Download <name>", amber rule.
- "What happened?" one sentence: "The automatic download of <name> failed: <reason>."
- "What to do":
  1. **Open the download page** (button; the page opens after the click, not on its own, as the Hub does).
  2. **Drop the file here**, or paste its full path.
  3. **[Retry]**, **[Skip]** (only for optional steps), **[Open folder]**.
- A dropped file is staged and validated (type, size, SHA-256). If it fails: "That file is not <name>. The existing file was kept."

**Folder not found** (`Get-GameFolderInteractive` pattern):
- "Couldn't find <game> automatically." Examples of common folders. [Browse] and a path box.
- If the folder lacks the probe file: "This folder doesn't contain <probe>. Use it anyway?" [Yes] [Pick again].
- On accept, the folder is remembered (section 2.5 and 2.12).

**Recovery panel** (PROPOSAL, from `Invoke-PCVRUniversalRecovery`, `Core/Modules/InstallerRecovery.ps1:97-184`). Shown on any failure that the step did not handle:
- Heading: "INSTALL STOPPED AT STEP 3 OF 4 - nothing was marked installed".
- The failure message in one or two sentences.
- Buttons: **[Retry this step]**, **[Retry from start]**, **[Open log]**, **[Open install folder]**, **[Open Downloads]**, **[Clear handover]** (when a path was handed over).
- A drop zone: "Drag any downloaded file, game folder or archive here, or paste its path. It is handed to the next retry."
- **No Exit button.** The panel stays until the install succeeds or the user closes the window. The Hub's rule: "There is deliberately no error-screen Exit/Q action."

**Partial installs:**
- Work files stay in a staging folder until the verify step passes.
- A failed run rolls back only the files in the manifest (section 2.12).
- A failed update keeps the previous version in place (section 2.12).

**Skip** (PROPOSAL): allowed only for steps marked `optional = true` in `install.toml`. A skipped optional step makes the result "Installed, with skipped steps" in amber. It is never shown as plain "Installed". The Hub's skip message "install may be incomplete" is the right warning; the state should say so too.

### 2.10 Settings for this game

Shown as a strip under the action row (the Elden Ring pattern, section 1.20). For Time Crisis:

| Setting | Control | Stored in | Applied by |
|---|---|---|---|
| Cover | Segmented: Grip / Duck | `user/profiles/<player>.toml`, per-game override | Re-running `write-config` (`physical_crouch`) |
| Laser | On / Off | Same | `write-config` (`laser_enabled`) |
| Gun angle | Slider, -60 to +60 degrees (the range in [controls.md](../controls.md) section 1.1) | Same | `write-config` (`gun_pitch`) |
| Hand | Right / Left | Profile (`left_handed`) | `write-config` |

- **Save** writes the profile and runs only the `write-config` step. No reinstall.
- **Status line:** "Saved. Applies the next time you start." A changed setting shows a dot until the next start.
- Profiles and bindings: [config-spec.md](../config-spec.md) sections 7 and 8.

### 2.11 Launch: Start in VR

1. **Runtime check** (new; the Hub has none, section 1.21). Look for an active OpenXR runtime, and for SteamVR or Meta Link processes as a hint. If nothing is running: "No VR runtime is running. Start SteamVR (or Meta Link) first." [Start SteamVR] [Start anyway].
2. **Quest variants.** `adb devices` must show `device`. Otherwise "Connect your Quest with the USB cable, or start Air Link" (the Quest is still connected over Link in PCVR).
3. **Recently played.** Recorded first, the latest 8 kept (`Core/Modules/DetailView.Launch.ps1:99-118`).
4. **Hub window.** Minimised, so the game takes focus. Restored when the game exits (PROPOSAL). The Hub restores its window only on failure paths (`Core/Modules/DetailView.Launch.ps1:595-600`, `:751`, `:787`), so after a normal launch the user has to find it again.
5. **Start.** The variant's `launch.exe` in `launch.cwd` from `install.toml`. For Quest: start the activity over adb. Track the process and show "Playing <title>" while it runs.
6. **Missing exe.** "The install folder for <title> no longer exists. The install has been cleared. Run Install again." The variant's state goes back to Ready to install (Hub `Core/Modules/DetailView.Launch.ps1:785-806`).
7. **No silent fallback.** If the variant cannot start, say so. Never start a flat game by accident (Hub `:811-822`).
8. **Steam library entry.** For PCVR variants with `shortcut.targets = ["steam"]`, the library shortcut lets the user start from SteamVR ([frontend.md](../frontend.md) section 1; [workshop.md](../workshop.md) D16).

### 2.12 Update, reinstall and uninstall

**Update check** (PROPOSAL, inheriting the Hub's cache):
- Only for variants with a GitHub `upstream`. TTL 6 h, stored in `user/cache/`, never in the program folder (Hub `Core/Modules/Filter.ScanSources.ps1:1603-1607`).
- On network failure, the last known value is used. One failure stops further network calls in that scan (circuit breaker). A scan-wide deadline stops probing (Hub `Core/Modules/Filter.ScanSources.ps1:1201`, `:1623`).
- Pinned variants offer an update when the recipe's pin changes. `latest` variants offer one as soon as upstream publishes a newer tag. Either way the update is never silent. Text: "Update available: v1.3.0 (you have v1.2.0)".

**Update procedure:**
1. Install the new version into a staging folder next to the current one.
2. Verify it (`installed_when` and the manifest).
3. Swap the folders, and write the new manifest.
4. On any failure, keep the current version and show the recovery panel (section 2.9). Nothing is lost, as the Hub's self-update rolls back its partial download (`Core/Update-Hub.ps1:470-500`).
5. User files, saves and profiles are never touched.

**Reinstall:** the same install steps over the existing install. The manifest is compared first. Files the user edited are kept, and the panel says so (the Hub's fail-closed rule, `Core/Modules/OwnedModFiles.ps1`).

**Uninstall** (PROPOSAL, from `Core/Modules/OwnedModFiles.ps1` and the Hub's guide rules):
1. **Preview first.** Dialog: "Remove 412 files installed by Time Crisis VR (DR-89)? Your timecris.zip, saves, profiles and any file you edited are kept." Show the count and three examples. Buttons: [Remove] [Cancel]. The Hub's rule is "Cancel changes nothing".
2. **Remove only manifest files**, then restore the backups the manifest lists. The manifest is `installed/<game>/<variant>/.ownership.csv` (our name for `.pcvrhub_<id>_ownership.csv`).
3. **Keep edited files** and list them at the end.
4. **Shortcuts we created** (Steam shortcut, desktop `.lnk`) are removed only when their target still points at our install.
5. **Never delete** the base game, the user's media (`timecris.zip`), `user/`, saves or profiles.
6. **Quest.** `adb uninstall <package>` after confirmation, with a choice to keep or delete the app's data on the headset.
7. **Stop the game first.** If it is running, ask the user to close it. Do not kill it.

**Why not the Hub's guide text.** The Hub's uninstall guide is a generated list of steps, shown for the user to follow by hand. Our uninstall acts on a recorded list. A hand-written remove list, as in `games/vcop2/install.toml` ("verify the list against the release zip"), is the weak form. Replace it with the manifest.

### 2.13 Similar games (PROPOSAL)

Candidates: every other entry in the catalog. Score:

| Signal | Points |
|---|---|
| Same `series` | +6 |
| Same hardware `family` (from `data/vocab/hardware.toml`, e.g. `namco`) | +4 |
| Same `manufacturer` | +2 |
| Each shared `subgenre` | +2 (max 4) |
| Same `controls.type` | +2 |
| Year within 3 | +2; within 8 | +1 |

Ties: title, alphabetical. Show four rows, with a "Show more" to 12, as the Hub's count follows the box height (`Core/Modules/DetailView.Page.ps1:1673-1676`, `:1794`, `:1812`). Each row: thumbnail 160 x 75 (from `art/tile.png` centre-cropped), title 13 px Medium, sub-line with control type, quality badge and state in one line. Click opens that game. Empty state: "No similar games found". The weights are a starting point and need a test run on the real catalog.

### 2.14 What it installs and credits

- One row per component, with name, pinned version, author, licence, upstream link and role. Example for Time Crisis, from [games/timecris/README.md](../../games/timecris/README.md):
  - Time Crisis VR by DR-89, MIT port code, built on namco22-decompile.
  - namco22-decompile by spacestate1, MIT.
  - "The Hub doesn't download DR-89's release packages because they ship game files." The page shows this sentence once, in the Requirements area, because it explains why the user supplies a ROM-free build.
- **Never link game content.** Links are to upstream projects and to official stores. No ROM or ISO download sites, ever ([legal.md](../legal.md)).
- Support link: only if upstream itself publishes one (the Hub's "SUPPORT THE MOD" box, `Core/Modules/DetailView.Page.ps1:2186-2234`).

### 2.15 Performance card (OPEN)

The Hub's PC Power card (section 1.7) is useful to a VR player, and our true 3D setups are heavy. We propose a compact version:
- An author-set tier per variant in `install.toml` (`perf = "SOLID"`), using the Hub's six labels. The tier lives in data, not in code.
- A disclaimer adapted in our own words: VR performance depends on many factors, and the tier is a rough indicator.
- Optional local GPU read, with the consent line "Reads your installed GPU name once, locally." Off by default.
- We do not copy the Hub's per-title tier table or its GPU reference list. They are the Hub's own data.

### 2.16 Drift in our own docs (fix while writing the UI)

1. **State vocabulary.** [frontend.md](../frontend.md) section 1 lists `Ready`, `Needs your files`, `Not installed`, `Update available`, `Unsupported`. [game-packages.md](../game-packages.md) section 2 lists `Needs your files`, `Needs an emulator`, `Ready to install`, `Installed`, `Update available`, `Coming soon`. Use the game-packages list, and fix frontend.md.
2. **`install.toml` form.** [game-packages.md](../game-packages.md) section 4 shows an inline `steps = [ {...} ]` array. The real file [games/timecris/install.toml](../../games/timecris/install.toml) uses `[[variant.<id>.step]]` tables. Pick the tables form (long steps read better), and fix game-packages.md.
3. **`[[media]]` form.** [game-packages.md](../game-packages.md) section 3 shows `[media]` as a table with inline keys. [game-schema.md](../game-schema.md) section 2 and the real `game.toml` use `[[media]]`. Pick `[[media]]`.
4. **README template.** [game-packages.md](../game-packages.md) section 5 lists nine sections. The real [games/timecris/README.md](../../games/timecris/README.md) uses Features, How to use, Controls, What it installs, Requirements, Troubleshooting, Credits. The UI accepts any H2 headings. Keep the nine as the recommended order.
5. **Uninstall list.** [games/vcop2/install.toml](../../games/vcop2/install.toml) hard-codes `remove = [...]` "verify the list against the release zip". Replace it with the manifest (section 2.12).
6. **Install Quest wording.** [frontend.md](../frontend.md) section 1 says the Hub "checks developer mode and USB debugging". Developer mode cannot be read over adb. Say "inferred from the connection" (section 2.5).
7. **Control-set path.** [config-spec.md](../config-spec.md) section 2 puts ghost controls and gun settings under `setup/` (`setup/controls.toml`, `gun.toml`, `comfort.toml`), while section 5 names control sets `controls/<id>.toml`. The real Rave Racer entry uses `setup/controls.toml`. Pick `setup/` and fix section 5.

### 2.17 Owner questions (OPEN)

1. **Skip:** allow it for optional steps only, with an amber "with skipped steps" result? (PROPOSAL: yes.)
2. **Performance card:** add the author-set tier card? (PROPOSAL: yes, later than v1.)
3. **Steam Theatre:** instructions only, as the Hub does? Or write the SteamVR setting with consent? (PROPOSAL: instructions only in v1. Changing SteamVR's own settings is a system change, and needs explicit consent.)
4. **Card "Ready" label:** rest label "Ready" with a hover swap (02), or "Start in VR ▶" at rest (the Hub's detail page)? (02 DIVERGE.)
5. **Quest launch:** from the Hub only, or also from the headset's library? ([frontend.md](../frontend.md) section 2 OPEN.)
6. **Hash database:** hashpack or MAME software-list XML? ([config-spec.md](../config-spec.md) section 10.)
7. **Notice and quip fields:** add `notice` to variants and `quip` to `game.toml`? (PROPOSAL: yes; both free text, so no vocabulary change needed.)
8. **Store link:** add `store.steam_appid` for PC releases on Steam? (Needed for "Open in Steam".)
9. **Similar games weights:** approve the table in section 2.13, or revise after a test run?

---

## 3. Implementation notes: WPF or web

Section 4 of [02-game-cards.md](02-game-cards.md) covers the card-level differences. These notes cover the detail page and the flow.

**Common to both:**
- **Model first.** Build one `DetailModel` from `game.toml`, `install.toml`, the manifest and the state. The view only renders it. The Hub builds the page imperatively with about 3,600 lines of XAML-in-code (`Core/Modules/DetailView.Page.ps1`). Do not port that. Port the rules in sections 1.5 and 2.6.
- **Data, not code.** No per-title strings, colours or tiers in code (section 1.21, items 8 and 10).
- **One event stream** for installs. The installer emits typed events (`step`, `ok`, `warn`, `fail`, `work`, `detail`, `prompt`, `done`). The console renders them and the log file stores them. Do not keep two versions of the text.
- **Await, do not poll.** The Hub polls every 750 ms for an installer's exit (`Core/Modules/DetailView.Page.ps1:2745-2757`). Await the child process instead.
- **Prompts** are a promise. The console awaits the user's answer. Enter continues where the Hub uses Read-Host, and Escape cancels only where the step allows.
- **State** lives under `user/`, as TOML with a checksum. Never in the program folder, so a Hub update or a fresh download cannot erase it (section 1.16).
- **Copy:** the copy chips and "Copy log" use the platform clipboard (`Core/Modules/DetailView.Content.ps1:699-750`).

**WPF (if D4 picks PowerShell + WPF):**
- Reuse the Hub's visual constants (section 2.7) and the text-size table (S 12/18, M 14/21, L 16/24).
- Build the page from a template per state, not from one 3,600-line function.
- Use `Set-TextBlockWithLinks`-style inline runs for bold and code in the README, as the Hub does.
- Keep hover effects to `RenderTransform` scale, as the Hub does, so the layout does not shift.

**Web (if D4 picks Tauri 2):**
- Hero: `object-fit: cover` up to 1040 px. Above that, `object-fit: contain` centred, with two side panels filled by an accent-tinted gradient, which matches the Hub's frozen-width look.
- Hover: `transform: scale()` only (hero 1.02, pills 1.08, similar rows 1.05, quip 1.03 across and 1.10 down). Honour `prefers-reduced-motion`. The Hub has no such check; we add it (PROPOSAL).
- Console: a virtualised list, so long installs stay smooth. Events arrive from the Rust side over the app's event channel.
- Accessibility: `aria-expanded` on the guide and log toggles, as the Hub sets accessible names on Uninstall Guide (`Core/Modules/DetailView.Actions.ps1:1840-1861`).
- Drop zones use the platform's file-drop event. Test with a path that has spaces and a drive letter, because the Hub's recovery code normalises those (`Core/Modules/InstallerRecovery.ps1:11-30`).

---

## Appendix: source list

Hub files (absolute, under ``):
- `Core/Modules/DetailView.ps1`, `Core/Modules/DetailView.Page.ps1`, `Core/Modules/DetailView.Content.ps1`, `Core/Modules/DetailView.Tiles.ps1`, `Core/Modules/DetailView.Actions.ps1`, `Core/Modules/DetailView.Launch.ps1`, `Core/Modules/DetailView.Power.ps1`
- `Core/Modules/UninstallGuide.ps1`, `Core/Modules/ReadmeLinks.ps1`, `Core/Modules/DiscordChannels.ps1`, `Core/Modules/EldenRingSaveUI.ps1`
- `Core/Modules/InstallerFoundation.ps1`, `Core/Modules/InstallerRecovery.ps1`, `Core/Modules/InstallerSafety.ps1`, `Core/Modules/OwnedModFiles.ps1`, `Core/Modules/Helpers.ps1`, `Core/Modules/HubState.ps1`, `Core/Modules/Window.Controls.ps1`, `Core/Modules/Window.Layout.ps1`, `Core/Modules/Filter.ScanSources.ps1`, `Core/Modules/Filter.Scan.ps1`, `Core/Modules/Catalog.ps1`
- `Core/Run-Installer.ps1`, `Core/Update-Hub.ps1`, `Core/Utils/GameDetection.ps1`
- `Core/AlbaVR/AlbaVR-core.ps1`, `Core/AlbaVR/README_AlbaVR.md`, `Core/AlbaVR/START_INSTALLER.bat`
- `Core/QuestZDoomShared/QuestZDoomShared.ps1` (shared engine install pattern, noted; no Quest sideload flow in the Hub)
- `Start PCVR Mods Hub.bat`, `_screenshots/hub-main.png` (overall style)

AladdinsCastle files (paths in this repo):
- `docs/game-packages.md`, `docs/game-schema.md`, `docs/frontend.md`, `docs/controls.md`, `docs/config-spec.md`, `docs/workshop.md`, `docs/architecture.md`
- `docs/ui/02-game-cards.md` (sibling; card rules and state colours)
- `games/timecris/game.toml`, `games/timecris/install.toml`, `games/timecris/README.md`, `games/raverace/game.toml`, `games/raverace/install.toml`, `games/raverace/setup/controls.toml`, `games/vcop2/install.toml`
- `data/vocab/hardware.toml`, `data/vocab/genres.toml`
- `docs/legal.md` (no game content), `docs/workshop.md` D1 (true 3D default)
