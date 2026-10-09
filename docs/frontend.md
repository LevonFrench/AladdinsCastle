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

## 4. Art and metadata

1. **We ship no third-party art.** Packs may include only art they have the right to distribute.
2. **Scraping happens on the user's machine** with the user's own accounts where a service needs them (ScreenScraper and others). Each asset gets a `*.source.toml` sidecar recording where it came from.
3. **Fallback art** is generated: title on a marquee template in the maker's colours. Every game looks good on day one.

## 5. Technology (OPEN, recommendation)

| Option | For | Against |
|---|---|---|
| **Tauri 2 (Rust + web UI) (recommended)** | Small portable exe; rich art grid with web tech; Rust for downloads, hashing, adb, file work | Two languages |
| .NET (WPF or Avalonia) | Native Windows; easy registry, Steam and shortcut work | Heavier runtime; UI theming takes more effort |
| PowerShell + batch (as the Installer Hub) | Zero install, proven model | No real GUI for an art library |
| Godot 4 (desktop) | Same engine as a possible future VR UI | Not a natural fit for file/installer work |

The Installer Hub's lessons to copy: portable folder (no installer needed), built-in self-updater, version cache to stay under GitHub rate limits, Steam/GOG/Epic library detection, safe-download helpers with a manual fallback when automatic steps fail.
