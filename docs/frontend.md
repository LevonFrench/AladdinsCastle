# Front end: the Hub

AladdinsCastle's front end is a **desktop app on Windows**: a library, an installer and a launcher. There is no VR lobby. You pick a game on the monitor (or from the SteamVR library), and it opens straight into VR.

Model: [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) (MIT, PowerShell). It's a portable app that finds your games, downloads each VR mod from its original source, runs a guided per-game setup, updates mods, and launches them. AladdinsCastle does the same for 3D light gun and racing games, but installs **our own setups**: a backend, its config, controls and calibration, made for that game.

## 1. What the Hub does

| Function | Detail |
|---|---|
| **Library** | A grid of games with art (marquee, flyer, box art, screenshots), filterable by Gun / Racing, platform and status. |
| **Status per game** | The states in [game-packages.md](game-packages.md) §2 (Needs your files · Needs an emulator · Ready to install · Installed · Update available · Coming soon), plus the VR quality badge: **True 3D** or **Theatre**. |
| **Detect** | Scans folders you choose for ROM sets and disc images by hash. Finds PC ports (Steam, GOG, plain folders). Finds installed VR runtimes (SteamVR, Meta Link, Virtual Desktop). |
| **Install** | Runs the game's **recipe** (see §3): downloads backends and tools from their original sources only, checks hashes, extracts, writes config, sets up controls. Never downloads game content. |
| **Install to Quest** | For setups that run standalone (e.g. DR-89 Time Crisis VR): sideload the APK over USB with adb. The Hub checks developer mode and USB debugging and walks you through them if they're off. |
| **Launch** | Checks the OpenXR runtime is up, then starts the backend straight into VR. |
| **SteamVR library entries** | Optionally adds each installed game to Steam as a non-Steam shortcut with art, so you can launch from inside the headset with no lobby. |
| **Update** | Checks upstream releases (GitHub API, cached and rate-limited like the Installer Hub's version cache) and offers updates. |
| **Settings** | Paths, VR runtime, comfort defaults, handedness and height, gun angle, bindings. All written to plain files. |
| **Diagnostics** | Collects logs and configs into a zip for bug reports, uploading nothing automatically (as DR-89's diagnostic helper does). |

## 2. In-headset UI (no lobby)

- Each VR setup has the same **pause overlay**: resume, recenter, comfort, laser on/off, gun angle, cover mode, quit to desktop. It comes from the shared VR runtime library (see [architecture.md](architecture.md)), so every game behaves the same.
- To switch games without taking the headset off: the **SteamVR library** (via the shortcuts above) or the overlay's "Back to Hub" action, which returns to the desktop Hub on the virtual desktop. **OPEN**: is a flat in-VR game list panel wanted later?

## 3. Recipes: how a setup is installed

Each game's install steps live in its own `games/<id>/install.toml`, one block per **variant** (see [game-packages.md](game-packages.md) §4). Steps are data first, with a script only as an escape hatch. The Installer Hub uses one PowerShell script per game; we use declarative TOML so installs are easy to read, review and share.

Example: the VC2VR variant of [`games/vcop2/install.toml`](../games/vcop2/install.toml):

```toml
[variant.vc2vr]
title    = "VC2VR by NeuralF (true 3D, PCVR)"
quality  = "true3d"
status   = "wip"
needs    = { media = ["vcop2-pc"], tools = [] }
installed_when = "${media.vcop2-pc.dir}/VC2VR.exe"

[[variant.vc2vr.step]]
do      = "github-release"              # original source only, pinned tag
repo    = "NeuralF/Rea-Virtua-Cop-2-VR"
asset   = "VC2VR-*.zip"
version = "v1.0-beta"
to      = "${download_dir}"

[[variant.vc2vr.step]]
do = "extract"
to = "${media.vcop2-pc.dir}"

[[variant.vc2vr.step]]
do      = "shortcut"
targets = ["steam"]
exe     = "${media.vcop2-pc.dir}/VC2VR.exe"
```

A release that bundles game files is never downloaded by the Hub. Such variants use `locate-package` instead (see `games/timecris/install.toml`).

Step kinds: `github-release`, `download` (URL + sha256), `locate-package` (the user points to a file they obtained), `extract`, `copy`, `copy-media` (copies or links the user's own checked files into place), `require-media`, `write-config` (ini/cfg/toml/json/yaml), `patch-text`, `registry`, `adb-install`, `shortcut`, `run` (script or exe, with a confirmation prompt), and `uninstall` steps that reverse the install. Unknown step kinds stop the install with a clear message. They are never skipped silently.

## 4. Emulators and tools: install, locate or search

Many setups need an emulator or helper (MAME, Model 2 Emulator, Supermodel, PCSX2, RPCS3, Dolphin, Flycast, DuckStation, TeknoParrot, DemulShooter, vJoy, HidHide, adb). The Hub offers all three routes for every one of them. *(Owner decision 2026-10-08.)*

| Route | What happens |
|---|---|
| **Install it for me** | Download from the official release page only (GitHub releases or the project's own site), check the hash, put it in the AladdinsCastle `emulators/<id>/` folder, and record the version for updates. Never a mirror, never with game content. |
| **I already have it: here it is** | You pick the folder or exe. The Hub verifies it (exe name, version string or file hash) and uses it in place, never modifying your install. Per-emulator settings our setups need go into a separate profile or config dir where the emulator supports one. |
| **Search for it** | Scan likely places: Program Files, `%LOCALAPPDATA%`, Scoop and winget install paths, Steam (e.g. Dolphin, RetroArch), common emulator folders on every drive, and front ends' known paths (LaunchBox, RetroBat, Playnite). Results are listed with version for you to confirm. Nothing is used without confirmation. |

Each emulator or helper is described by a `data/emulators/<id>.toml` manifest (schema: [emulator-manifests.md](emulator-manifests.md)):

```toml
id       = "pcsx2"
name     = "PCSX2"
upstream = "https://github.com/PCSX2/pcsx2"
license  = "GPL-3.0"

[install]
do      = "github-release"
repo    = "PCSX2/pcsx2"
asset   = "pcsx2-*-windows-x64-Qt.7z"
version = "latest"               # or a pinned, tested version

[locate]
exe       = ["pcsx2-qt.exe", "pcsx2-qtx64*.exe"]
version   = { from = "file-version" }
min       = "2.0.0"

[search]
paths     = ["${ProgramFiles}/PCSX2", "${LOCALAPPDATA}/Programs/PCSX2", "${scoop}/apps/pcsx2/current", "*:/Emulators/PCSX2*"]
```

If a found version is older than a setup needs, the Hub says so and offers to install a side-by-side copy rather than upgrade the user's own install.

The same three routes apply to **game files**: point the Hub at your ROM, ISO and PC-game folders, or let it search. Matches are confirmed by hash.

## 5. Art and metadata

1. **We ship no third-party art.** Packs may include only art they have the right to distribute.
2. **Scraping happens on the user's machine** with the user's own accounts where a service needs them (ScreenScraper and others). Scraped art goes to `user/art/<id>/` (private, gitignored), never into `games/<id>/art/`. Each asset gets a `*.source.toml` sidecar recording where it came from.
3. **Fallback art** is generated: title on a marquee template in the maker's colours. Every game looks good on day one.

## 6. Technology: Qt 6 / QML (decided 2026-10-08)

| Option | Verdict |
|---|---|
| **Qt 6 / QML** | **Chosen.** The same QML UI runs as a desktop window and renders offscreen (`QQuickRenderControl`) into a SteamVR dashboard overlay, exactly how OpenVR Advanced Settings works. Native on Windows and Linux x86_64/ARM64 (Steam Frame). GPU-rendered cards, glows and animations match the Mod Hub look. LGPL Qt is compatible with GPL-3.0. |
| Tauri 2 | Dropped. WebView2 / WebKitGTK cannot render offscreen into a VR texture; CEF off-screen rendering for Tauri is an open PR. |
| Godot 4 | Not chosen for the Hub. Good at render-to-texture, but a SteamVR dashboard overlay needs an unverified plugin. Still a candidate for in-VR panels. |
| .NET WPF / PowerShell + WPF | Ruled out: Windows-only, and WPF has no supported path into a D3D11/Vulkan texture. |

Zero-code VR until our overlay exists: Desktop+ or SteamVR's Desktop view mirrors the Hub window.

How the Installer Hub is built: `Start PCVR Mods Hub.bat` → `Show-StartupSplash.ps1` → `VRModHub.ps1` (WPF window, about 40 modules in `Core/Modules/`), then background `Update-Hub.ps1` and `Prefetch-Versions.ps1`. Downloads via `Invoke-WebRequest`; extraction via bundled `7z.exe` or `Expand-Archive`; GitHub releases API with a version cache; DepotDownloader for Steam depots; settings in JSON.

Lessons to copy: portable folder (no installer), splash then background update checks, self-updater, version cache to stay under GitHub rate limits, Steam/GOG/Epic detection, safe-download helpers with a manual fallback when automatic steps fail. What we change: the catalog and recipes are data files (TOML), not code.
