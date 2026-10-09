# Front end: the Hub

AladdinsCastle's front end is a **desktop app on Windows**: a library, an installer and a launcher. There is no VR lobby. You pick a game on the monitor (or from the SteamVR library), and it opens straight into VR.

Model: [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) (MIT, PowerShell). It's a portable app that finds your games, downloads each VR mod from its original source, runs a guided per-game setup, updates mods, and launches them. AladdinsCastle does the same for 3D light gun and racing games, but installs **our own setups**: a backend, its config, controls and calibration, made for that game.

## 1. What the Hub does

| Function | Detail |
|---|---|
| **Library** | A grid of games with art (marquee, flyer, box art, screenshots), filterable by Gun / Racing, platform and status. |
| **Status per game** | `Ready` · `Needs your files` · `Not installed` · `Update available` · `Unsupported`, plus the VR quality badge: **True 3D** or **Theatre**. |
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

Each game has a recipe: a file of steps, data first, with a script only as an escape hatch. The Installer Hub uses one PowerShell script per game. We use declarative TOML so recipes are easy to read, review and share, and allow a script step for the odd cases.

```toml
# recipes/timecris-dr89.toml
id      = "timecris-dr89"
game    = "timecris"
title   = "Time Crisis - DR-89 Time Crisis VR (PCVR)"
quality = "true3d"

[[step]]
do       = "github-release"          # download from the original source only
repo     = "DR-89/time-crisis-vr"
asset    = "TimeCrisisVR-*-windows-x64.zip"
version  = "latest"                  # or a pinned tag
to       = "${setup_dir}"

[[step]]
do    = "require-media"              # the user's own files; checked, never downloaded
file  = "timecris.zip"
check = "hashpack:namco22/timecris"

[[step]]
do   = "write-config"
file = "${setup_dir}/quest-options.cfg"
set  = { physical_crouch = "${profile.cover == 'duck' ? 1 : 0}", left_handed = "${profile.left_handed}" }

[[step]]
do      = "shortcut"
targets = ["desktop", "steam"]
exe     = "${setup_dir}/Play SteamVR.cmd"
art     = "game"
```

Step kinds: `github-release`, `download` (URL + sha256), `extract`, `copy`, `require-media`, `write-config` (ini/cfg/toml/json/yaml), `patch-text`, `registry`, `adb-install`, `shortcut`, `run` (script or exe, with a confirmation prompt), and `uninstall` steps that reverse the install. Unknown step kinds stop the install with a clear message. They are never skipped silently.

## 4. Emulators and tools: install, locate or search

Many setups need an emulator or helper (MAME, Model 2 Emulator, Supermodel, PCSX2, RPCS3, Dolphin, Flycast, DuckStation, TeknoParrot, DemulShooter, vJoy, HidHide, adb). The Hub offers all three routes for every one of them. *(Owner decision 2026-10-08.)*

| Route | What happens |
|---|---|
| **Install it for me** | Download from the official release page only (GitHub releases or the project's own site), check the hash, put it in the AladdinsCastle `tools/` folder, and record the version for updates. Never a mirror, never with game content. |
| **I already have it: here it is** | You pick the folder or exe. The Hub verifies it (exe name, version string or file hash) and uses it in place, never modifying your install. Per-emulator settings our setups need go into a separate profile or config dir where the emulator supports one. |
| **Search for it** | Scan likely places: Program Files, `%LOCALAPPDATA%`, Scoop and winget install paths, Steam (e.g. Dolphin, RetroArch), common emulator folders on every drive, and front ends' known paths (LaunchBox, RetroBat, Playnite). Results are listed with version for you to confirm. Nothing is used without confirmation. |

Each tool is described by a `tools/<id>.toml` file:

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
2. **Scraping happens on the user's machine** with the user's own accounts where a service needs them (ScreenScraper and others). Each asset gets a `*.source.toml` sidecar recording where it came from.
3. **Fallback art** is generated: title on a marquee template in the maker's colours. Every game looks good on day one.

## 6. Technology (OPEN, recommendation)

| Option | For | Against |
|---|---|---|
| **Tauri 2 (Rust + web UI) (recommended)** | Small portable exe; rich art grid with web tech; Rust for downloads, hashing, adb, file work | Two languages |
| .NET (WPF or Avalonia) | Native Windows; easy registry, Steam and shortcut work | Heavier runtime; UI theming takes more effort |
| PowerShell + WPF (exactly what the Installer Hub uses) | Nothing to install (Windows PowerShell 5.1 + WPF ship with Windows); proven at 300+ games with card tiles, banners, detail view, filters | Big scripts get hard to maintain (their catalog is a 6,700-line PowerShell file); slow cold start (they need a splash screen); Windows-only |
| Godot 4 (desktop) | Same engine as a possible future VR UI | Not a natural fit for file/installer work |

How the Installer Hub is built: `Start PCVR Mods Hub.bat` → `Show-StartupSplash.ps1` → `VRModHub.ps1` (WPF window, about 40 modules in `Core/Modules/`), then background `Update-Hub.ps1` and `Prefetch-Versions.ps1`. Downloads via `Invoke-WebRequest`; extraction via bundled `7z.exe` or `Expand-Archive`; GitHub releases API with a version cache; DepotDownloader for Steam depots; settings in JSON.

Lessons to copy: portable folder (no installer), splash then background update checks, self-updater, version cache to stay under GitHub rate limits, Steam/GOG/Epic detection, safe-download helpers with a manual fallback when automatic steps fail. What we change: the catalog and recipes are data files (TOML), not code.
