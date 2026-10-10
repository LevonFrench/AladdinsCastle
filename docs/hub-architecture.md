# Hub architecture: Qt 6 / QML (draft 1)

Status: draft for discussion, 2026-10-08. Technology is decided (D4: Qt 6 / QML, see [workshop.md](workshop.md)). The Hub's process model, rendering path and packaging below are proposals, with open items listed in section 14.

Tags used in this document:
- **Verified**: backed by a source in section 16 (Qt docs, Qt source, OpenVR header, or a shipping project).
- **Design**: our proposal, not yet tested.
- **UNVERIFIED**: no source found yet. Each one has a spike in section 13.
- **OPEN**: needs an owner decision. Numbered in section 14.

The Hub is the desktop app described in [frontend.md](frontend.md). There is no VR lobby (D15). Each game is a VR setup launched from the Hub (see [architecture.md](architecture.md)).

## 1. Scope and constraints

| Constraint | Source | Effect on the Hub |
|---|---|---|
| Desktop app, no VR lobby | D15 | Desktop window is the primary UI. VR presence is an overlay (section 7). |
| Platform order: Steam Frame, then Quest 3 over PC, then Quest 3 native | D21 | Windows x64 first. Linux x86_64 and Linux ARM64 must build (D12). |
| Qt 6 / QML, one UI for window and overlay | D4 | Section 6 and 7. |
| SteamVR dashboard overlay later | D22 | Desktop+ covers the zero-code step until then. |
| Look close to PCVR Mods Installer Hub | D19, [docs/ui/](ui/) | Views map to the UI specs (section 6). |
| Install from original sources only, never game content | [frontend.md](frontend.md) sections 1 and 4 | Install engine has no game-file download step. |
| One folder per game, TOML catalog | D18, [config-spec.md](config-spec.md) | Catalog loader and layered merge (section 4). |
| GPL-3.0 | D3 | Licensing summary in section 12. |

Scale: 413 game folders exist today in `games/`. The grid target is 400+ cards.

Non-goals: no third-party art in the repo, no ROM or BIOS content, no kernel driver, no in-VR lobby.

## 2. Process model and threads

| Context | Owns | Runs |
|---|---|---|
| GUI thread | QML engine, all `QObject` models, the desktop `QQuickWindow`, the overlay `QQuickRenderControl` and its `QQuickWindow`, OpenVR overlay calls | UI, rendering, OpenVR overlay events and texture submission. |
| Worker pool (`QThreadPool`) | Catalog and TOML parsing, media hashing, downloads, archive extraction, update checks, art decoding | Results return to the GUI thread through queued signals. Workers never touch QML objects. |
| Process runner (`QProcess`) | Installers, `adb`, emulators, launched setups | stdout and stderr become console events. |

Decisions in this model:
- **Single-threaded rendering first.** Qt's own single-threaded example and OpenVR Advanced Settings both render on the GUI thread, throttled by a timer (Verified: section 16). Qt docs say a render thread needs the GUI thread blocked during `sync()`. Add a render thread only if measurements require it.
- **One graphics API per process.** `setGraphicsApi` can only change when no `QQuickWindow` or render control exists (Verified, Qt docs). The desktop window and the overlay share one API. Proposed: OpenGL (see OPEN D24).
- **Quit handling.** On `VREvent_Quit`, call `IVRSystem::AcknowledgeQuit_Exiting()` before exiting (Verified, OpenVR header lifecycle notes).

## 3. Executables and build targets

| Target | Type | Contents |
|---|---|---|
| `hubcore` | static library | C++ core (section 4). Links QtCore, QtNetwork, QtConcurrent and toml++. No QML. |
| `aladdinscastle-hub` | executable | Desktop shell (`DesktopShell.qml`). Modes: default (desktop), `--overlay` (dashboard overlay only, no window). A "both" mode is behind a flag. Design, following the single-process layout of OpenVR Advanced Settings as read in its source (not run-tested). |
| `hubtool` | CLI | `validate`, `explain`, `hash-check`, `pack` (architecture.md section 6). Python `tools/validate_catalog.py` already covers validation (OPEN D32). |
| `hub-tests` | Qt Test | Core tests plus a generator for synthetic game fixtures (no real game content). |

OPEN D23: one executable with modes, or two executables sharing `hubcore`. Recommendation: one executable with modes.

## 4. C++ core modules (`hubcore`)

| Module | Responsibility | Notes |
|---|---|---|
| Catalog loader and layered config | Loads built-in, then packs (by priority), then `user/overrides`, deep-merging per [config-spec.md](config-spec.md) section 8. Produces `GameRecord`s. | toml++ (MIT, C++17, header-only by default) parses TOML. Merge rules are hand-written over `toml::table`. Verified: toml++ licence and API (section 16). |
| Schema validation | Validates TOML files against `schemas/*.json` (JSON Schema draft 7) in warn mode. Unknown keys pass through. | nlohmann/json-schema-validator (MIT, 2.4.0, draft 7). Needs a TOML-to-JSON converter. Design. |
| Filter and sort engine | Lives in the Qt model layer (section 5). | Rules from [docs/ui/01-shell-and-filters.md](ui/01-shell-and-filters.md) section 2.12: filters fail open, STATE filters inactive before a scan, no card rebuilds. |
| Media scanner | Scans user folders, matches files by hash against known sets. Progress and cancel. Never downloads. | Interface below. Hash source is OPEN (config-spec section 10). |
| Install engine | Runs `install.toml` steps in order, from a registry keyed by step kind. Unknown kinds stop the install with a message. Writes the console event stream and a log file. | Step kinds: [frontend.md](frontend.md) section 3. Console events and log path: [docs/ui/03-detail-and-install-flow.md](ui/03-detail-and-install-flow.md) section 2.8. Optional-step skip is OPEN (03 section 2.17). |
| Update checker | Upstream release checks through the GitHub API, cached and rate-limited. | [frontend.md](frontend.md) section 1; 03 section 1.17 (version cache). |
| Launcher and runtime manager | Detects an active OpenXR runtime (SteamVR, Meta Link, Virtual Desktop), starts the setup, tracks its process, and shows "Playing". | 03 sections 2.5 and 2.11. Restoring the Hub window after a launch is a proposal there. |
| Settings and profiles | UI state, user profiles, bindings. Unknown keys preserved. | [config-spec.md](config-spec.md) section 7. Storage location is OPEN D29. |
| Art cache | Resolves art from `games/<id>/art/` (redistributable) and `user/art/<id>/` (scraped). Generates fallback art. Serves through the image provider. | [frontend.md](frontend.md) section 5. |
| Overlay host (overlay builds only) | OpenVR overlay lifecycle, texture submission, input. | Section 7. |
| Steam shortcuts | Writes SteamVR library entries for installed setups. | `shortcut` step; [frontend.md](frontend.md) section 1. |

Interface sketches (illustrative, not final):

```cpp
// Media scanner. Runs on a worker. Emits results on the GUI thread via queued signals.
struct ScanOptions { bool hashFiles = true; int maxDepth = 6; };
class IMediaScanner {
public:
    virtual ~IMediaScanner() = default;
    virtual void scan(const QStringList &folders, const ScanOptions &opts,
                      std::atomic_bool &cancel) = 0;
};

// Install step. One implementation per step kind in the registry.
class IInstallStep {
public:
    virtual ~IInstallStep() = default;
    virtual QString kind() const = 0;          // "github-release", "extract", ...
    virtual bool run(InstallContext &ctx, ConsoleSink &console) = 0;
    virtual bool undo(InstallContext &ctx, ConsoleSink &console) = 0;  // for uninstall
};
```

Catalog fields the GUI needs are listed in section 5. Field names follow [game-schema.md](game-schema.md).

## 5. Models exposed to QML

| Model | Base class | Content |
|---|---|---|
| `GameListModel` | `QAbstractListModel` | One row per game. Roles for every filter and display field: `id`, `title`, `altTitles`, `genre`, `subgenres`, `year`, `manufacturer`, `developer`, `hardwareFamily`, `hardwareKind`, `hardwareLabel`, `graphics`, `players`, `controlsType`, `vrQuality` (`true3d`, `theatre`, `none`, `planned`), `badges`, `pill`, `accent`, `colour`, `blurb`, `featuredEligible`, `installState`, `installedVariants`, `recentlyPlayedRank`. Source: [game-schema.md](game-schema.md) sections 2 and 3, 01 section 2.11. |
| `FilterSortModel` | `QSortFilterProxyModel` subclass | Facets (genre, manufacturer, year range, graphics, hardware tree, VR quality, players, controls, in library, state), search over title, alt titles and developer, and ORDER (title, year, manufacturer, hardware, recently played). Reorders and hides rows only. |
| `SectionModel` | Proxy per genre | Light guns and Racing sections ([02 section 2.5](ui/02-game-cards.md)). |
| `VariantModel` | `QAbstractListModel` | Variant chips with quality and status dot ([03 section 2.4](ui/03-detail-and-install-flow.md)). |
| `RequirementModel` | `QAbstractListModel` | "What you need" rows: media, tools, headset, runtime, each with a status enum ([03 section 2.5](ui/03-detail-and-install-flow.md)). |
| `ConsoleModel` | `QAbstractListModel` | Install events: `step`, `ok`, `warn`, `fail`, `work`, `detail`, `prompt`, `done` ([03 section 2.8](ui/03-detail-and-install-flow.md)). |
| `ScanModel`, `UpdateModel`, `SettingsModel` | `QAbstractListModel` | Scan progress and results, update notices, per-game and global settings. |
| `Theme` | `QObject` singleton | Colour, font, size and radius tokens, loaded in C++ from a token file. QML never parses TOML. Source: [02 section 4.3](ui/02-game-cards.md) (proposal). |
| Image provider | `QQuickAsyncImageProvider` | Serves `image://art/<gameId>/<kind>`. Caching is automatic. Verified: class exists in qtdeclarative (section 16). |

Model rules:
- Models are owned and updated on the GUI thread only.
- Sorting and filtering change `FilterSortModel` state. They never rebuild delegates (01 section 2.12, Mod Hub rule).
- Unknown field values pass filters ("fail open", 01 section 2.12).

## 6. QML layer

| File | Role |
|---|---|
| `hub/qml/Hub/HubRoot.qml` | Root **`Item`** (not `Window`). Header, filter bar, banner, grid, detail stack. Shared by both presentations. Verified: Qt's RHI example says the root must be a `QQuickItem`, and `Window` elements are not supported in the scene. |
| `hub/qml/Desktop/DesktopShell.qml` | `Window { HubRoot { anchors.fill: parent } }`. The only file with a `Window`. |
| `hub/qml/Hub/Header.qml`, `FilterBar.qml`, `FeaturedBanner.qml`, `ExplorePage.qml`, `SearchPanel.qml`, `ScanSpinner.qml`, `HelpPanel.qml` | [01 shell and filters](ui/01-shell-and-filters.md). |
| `hub/qml/Hub/CardGrid.qml`, `GameCard.qml` | [02 game cards](ui/02-game-cards.md). `GridView` with a minimal delegate. |
| `hub/qml/Hub/DetailPage.qml`, `Console.qml`, `ReadmeView.qml`, `SettingsPage.qml` | [03 detail and install flow](ui/03-detail-and-install-flow.md). |
| `hub/qml/Common/` | Token bindings, icons (Qt Quick Shapes for controls glyphs, benchmark first). |

Shared-QML rules:
- Design: keep popups in-scene (Qt Quick Controls `Popup`), so they render into the overlay texture as well. UNVERIFIED for the overlay path; check in S3.
- Native file dialogs and OS message boxes exist only in desktop mode. In overlay mode, "Find my files" shows "Do this on the desktop" (Design, OPEN D30).

Performance rules (from Qt's guide, Verified: https://doc.qt.io/qt-6/qtquick-performance.html and https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html):
- `GameCard` delegate: few elements, anchors instead of bindings, no clipping inside the delegate, no `ShaderEffect` or `MultiEffect` per card. Hover glow comes from pre-rendered art and a border.
- `MultiEffect` (Qt Quick Effects) only on the banner and the hover popup. Its blur and shadow are the heaviest effects, and the source must not be animated.
- Art through the async image provider, with `sourceSize` equal to the card size. Glow and shadow are baked into the art.
- Use `cacheBuffer` and item reuse only after measuring.
- Set `visible: false` on fully covered items.
- Target: 413 cards with smooth scrolling on the owner's PC and on the Linux test box. UNVERIFIED: no Qt benchmark exists for this case. Spike M1.2.

## 7. Dual presentation: desktop window and SteamVR dashboard overlay

Same QML, two presentations, one process (default when `--overlay` is not given and the overlay is enabled):

```
                   HubRoot.qml (one file, two instances)
                 /                                      \
   DesktopShell.qml (Window)              OverlayHost (QQuickRenderControl)
   QQuickWindow shown                     QQuickWindow never shown
   native mouse and keyboard              setRenderTarget(fromOpenGLTexture(texId))
                                          render on GUI thread, throttled timer
                                          SetOverlayTexture(TextureType_OpenGL, texId)
                                          overlay events -> QMouseEvent, QWheelEvent
```

Overlay host steps (proposed):
1. `VR_Init(&err, VRApplication_Overlay)`. A background application does not keep SteamVR running (Verified: OpenVR overlay notes).
2. `CreateDashboardOverlay(key, name, &main, &thumb)`. Set width. `SetOverlayInputMethod(main, VROverlayInputMethod_Mouse)`. `SetOverlayMouseScale(main, root item size)`. Thumbnail with `SetOverlayFromFile`.
3. Create the GL context and texture `texId` in the same context. `setGraphicsApi(OpenGL)` before the first window. `setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(ctx))`. `initialize()`. `setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(texId, size))`. (Verified: Qt's `rendercontrol_opengl` example uses this sequence.)
4. Load `HubRoot.qml` as a component. Set its parent to the render-control window's `contentItem()`. Set its size.
5. Each frame, while `IsOverlayVisible(main)`: `polishItems()`, `beginFrame()`, `sync()`, `render()`, `endFrame()`, `glFlush()`, then `SetOverlayTexture(main, {texId, TextureType_OpenGL, ColorSpace_Auto})`. (The `glFlush` is required by the OpenVR Advanced Settings code: Verified.)
6. Input: loop `PollNextOverlayEvent`. `VREvent_MouseMove` becomes `QMouseEvent(MouseMove)`. Button down and up become press and release (right maps to `Qt::RightButton`, others to left). `VREvent_ScrollSmooth` becomes `QWheelEvent`. Deliver each with `QCoreApplication::sendEvent(overlayWindow, ...)`. Verified: Qt's docs say mouse and key events go this way. The Linux Y-coordinate flip is UNVERIFIED (OpenVR Advanced Settings flips Y only on Linux; test on hardware).
7. Keyboard: `ShowKeyboardForOverlay` when a text field gets focus. `KeyboardCharInput` becomes a `QKeyEvent` carrying the text, sent to the focused item. `KeyboardDone` clears focus and `HideKeyboard` is called. Focus uses `forceActiveFocus()` (Verified: Qt docs). The end-to-end path is UNVERIFIED.
8. Lifecycle: on `VREvent_Quit`, `AcknowledgeQuit_Exiting()`. Do not call `SetDashboardOverlaySceneProcess` if the tab must work while a game runs (Verified: OpenVR overlay notes). Install with `AddApplicationManifest`. Auto-launch only after `IsDashboardOverlay_Bool` is true.
9. `SetOverlayTexture` may only be called by the overlay's creator or the process set with `SetOverlayRenderingPid`. A single-process design satisfies this (Verified: openvr.h).

Unverified items for the spike (M1 S1 to S3, section 13):
- Two `QQuickWindow`s (one shown, one offscreen) in one process sharing one `QQmlEngine` and one `QRhi`. Not documented in the fetched Qt pages.
- The SteamVR compositor accepts a Qt-created GL texture on Windows, on Linux x86_64, and on Steam Frame (streamed and standalone).
- Keyboard path (step 7).
- Y coordinate on Linux (step 6).
- Overlay refresh rate. OpenVR Advanced Settings throttles render calls with a 5 ms single-shot timer (Verified).

Alternatives considered:

| Option | Status | Use |
|---|---|---|
| Desktop+ (zero code, GPL-3.0, Windows only) | Prior research; repo [DesktopPlus](https://github.com/elvissteinjr/DesktopPlus) | Interim answer for "Hub in VR" (D22). |
| Window capture overlay | Windows only; focus problems | Not chosen. |
| CEF off-screen | Large runtime | Not chosen. |
| ImGui | Cheap per frame; full UI rewrite | Not chosen. |
| Tauri 2 / WebView2 | Cannot render offscreen ([frontend.md](frontend.md) section 6) | Dropped (D4). |
| Vulkan submission | `fromVulkanImage` + `VRVulkanTextureData_t`; needs `QVulkanInstance` and the extension lists from `IVRCompositor::GetVulkanInstanceExtensionsRequired` and `GetVulkanDeviceExtensionsRequired` (openvr.h, Verified) | Later option (OPEN D24). |
| D3D11 submission | `fromD3D11Texture` (Qt 6.4) + `TextureType_DirectX` | Windows only. Fallback. |

Steam Frame: when streamed from the PC, SteamVR and the overlay run on the PC, so the overlay path is the same. Standalone SteamOS on the Frame: whether a dashboard overlay exists there is UNVERIFIED (see [architecture.md](architecture.md) section 5).

## 8. Input routing summary

| Surface | Input source | Delivered as |
|---|---|---|
| Desktop window | Mouse, keyboard, wheel from the OS | Native Qt events on the shown `QQuickWindow` |
| Overlay | Laser or controller mouse from SteamVR | `VREvent_Mouse*` translated to `QMouseEvent` and `QWheelEvent` |
| Overlay text | SteamVR keyboard | `KeyboardCharInput` translated to `QKeyEvent` (UNVERIFIED) |
| Running setup | Controllers via libacvr | Not the Hub's concern; the setup is its own process |

## 9. Platforms and build

| Target | Qt | Compiler | Graphics | Notes |
|---|---|---|---|---|
| Windows x64 (owner PC, SteamVR with ALVR to a PCVR headset) | Qt 6 for MSVC 2022 x64 (Qt Online Installer) | MSVC | OpenGL (proposed) | Primary development target. |
| Linux x86_64 (Bazzite test box) | Qt 6 from the Online Installer or a distro package | GCC or Clang | OpenGL | Linux Hub and libacvr builds (architecture.md section 5). |
| Linux ARM64 (Steam Frame, SteamOS on Arm) | Qt 6 Online Installer for `linux_arm64` (from 6.7, with caveats), or a source build | GCC | OpenGL | See the caveats in section 10. Needs an ARM64 machine for testing. |

Verified facts behind the table ([Qt Linux platform page](https://doc.qt.io/qt-6/linux.html), Qt 6.12 labelled):
- arm64 is listed for Ubuntu 24.04 (GCC 13.x), Debian 11.6 (GCC 10) and Debian 12 (GCC 12).
- Arm desktops are a special case: Qt's reference platform is a Raspberry Pi 5 (8 GB) on Ubuntu 24.04.
- Qt 6.10 and later need glibc 2.34 or newer. Official binaries are built on Ubuntu 24.04 (glibc 2.39). An older host must build from source.

Build system: CMake with Qt's CMake support (`qt_add_qml_module`). Dependencies (toml++, json-schema-validator, nlohmann/json) through FetchContent, vcpkg or Conan (OPEN: choose one).

OPEN D25: pin the Qt minor version after spike S1. The fetched docs are labelled 6.12. The LTS choice was not checked.

## 10. Packaging

| Target | Method | Precedent or source |
|---|---|---|
| Windows x64, portable folder | `windeployqt --dir <out> --qmldir qml --no-translations` (plus `--release`), then the app. Keep `qt.conf` if the Qt build needs it. | Verified: [windeployqt docs](https://doc.qt.io/qt-6/windows-deployment.html). OpenVR Advanced Settings does the same in its `.pro` (Verified). |
| Windows, VC++ runtime | Redistributable must match the compiler (VS 2022: `vcruntime140.dll`, `msvcp170.dll`). Do not copy loose runtime DLLs from the dev PC. | Verified: windeployqt docs. Ship or require install is OPEN D33. |
| Linux x86_64, AppImage | `linuxdeploy` with `linuxdeploy-plugin-qt` (`--plugin qt`). | Verified: both x86_64 assets exist on the `continuous` tag. OpenVR Advanced Settings ships an AppImage (Verified). `continuous` is a moving tag, so pin by checksum (Design). |
| Linux ARM64, AppImage | Same tools, `-aarch64` assets. Build on Ubuntu 24.04 arm64 to match Qt's glibc floor. | Verified: aarch64 assets exist on the `continuous` tag. Build host not available yet. |
| Flatpak | Option for later | UNVERIFIED: no current Flathub Qt 6 aarch64 listing found. Sandbox limits on game folders, `adb` and emulators (Design). OPEN D28. |
| Steam Linux Runtime | Do not rely on it for Qt | Repo README and docs have no Qt mention (Verified). Secondary sources say SLR 4.0 removed Qt (UNVERIFIED against Valve's own notes). Bundle Qt. |

Portable layout: [config-spec.md](config-spec.md) section 2 keeps everything inside the install folder. The UI spec stores UI state under `%LOCALAPPDATA%` (see OPEN D29).

## 11. Repo layout under `hub/` (planned)

```
hub/
  CMakeLists.txt
  src/core/        catalog/ config/ filter/ scan/ install/ update/ launch/ settings/ art/
  src/overlay/     OpenVR overlay host (overlay mode only)
  src/app/         main.cpp, mode selection, DesktopShell wiring
  qml/Hub/         HubRoot.qml and views
  qml/Desktop/     DesktopShell.qml
  qml/Common/      theme bindings, icons
  tests/           Qt Test, synthetic fixture generator (no real game content)
  packaging/       windeployqt script, AppImage recipe, Flatpak manifest (later)
```

Alongside (already planned in [architecture.md](architecture.md) section 6): `schemas/` (JSON Schema, draft 7), `tools/`, `data/`, `games/`, `recipes/`.

## 12. Licensing (summary, not legal advice)

- The Hub is GPL-3.0 (D3).
- Qt is LGPL-3.0 for open-source use. Verified: [Qt licensing page](https://doc.qt.io/qt-6/licensing.html). Some Qt modules are GPL-only: Qt Virtual Keyboard, Qt Lottie Animation, Qt Quick Timeline, Qt Quick 3D and others. Avoid them unless needed.
- Ship Qt as shared libraries. This matches LGPL-3.0 section 4(d)(1) (Verified: [LGPL-3.0 text](https://www.gnu.org/licenses/lgpl-3.0.html)). Static linking of Qt would need section 4(d)(0) and is not recommended.
- Distribution: include the GPL and LGPL texts, Qt's notices, and a way to get the Qt source. Record the Qt version per release.
- Dependencies: toml++ (MIT), json-schema-validator (MIT), nlohmann/json (MIT), OpenVR (BSD-3-Clause).
- Get a lawyer's review before the first public binary.

## 13. Milestone 1 build plan

Maps to [roadmap.md](roadmap.md) M1. M1's "done when" is: from a clean PC the Hub installs and launches the first third-party setups, from the Hub and from the SteamVR library. The overlay is not in M1's "done when".

| Step | Content | Exit check |
|---|---|---|
| M1.0 Skeleton | CMake, `hubcore`, desktop shell, `HubRoot` with a `GameListModel` over a synthetic fixture of 413 entries | Runs on Windows and on the Linux x86_64 test box |
| M1.1 Catalog | Loader, layered merge, schema validation in warn mode, `hubtool validate` and `explain` | Loads all 413 `game.toml` files. `explain` prints the source of each value. |
| M1.2 Library | `GameCard`, `CardGrid`, `FilterBar` (two rows), search, ORDER | Benchmark: frame rate and memory with 413 cards. Filters and ORDER reorder without rebuilding cards. |
| M1.3 Detail | Variant picker, `RequirementModel`, media scanner with hashing | Scan works on fixture folders. Find my files works. |
| M1.4 Install engine | `github-release`, `require-media`, `extract`, `write-config`, `shortcut`, `adb-install`, console | First third-party setup installs into a clean folder. |
| M1.5 Launch | Runtime check, start, "Playing" state, SteamVR shortcuts | Launches from the Hub and from the SteamVR library. |
| M1.6 Packaging | Portable Windows folder; AppImage (x86_64) | Runs from a folder copy on a clean Windows PC. |

Spikes (run in M1 with owner hardware, or moved to M2 if the owner prefers):

| Spike | Question | Pass condition | If it fails |
|---|---|---|---|
| S1 Overlay | Does a Qt GL texture show in a SteamVR dashboard tab on the owner's PC (ALVR, PCVR headset)? | Tab visible, pixels correct, clicks land | Use Desktop+ (zero code) and re-plan D22. |
| S2 Keyboard | Does `ShowKeyboardForOverlay` type into a QML `TextInput`? | Typed text appears in the field | Use an in-scene keyboard. |
| S3 Two presentations | One `HubRoot` in a window and in the overlay, one process | No crash; GPU cost measured | Use the window and the Desktop+ overlay in two processes. |
| S4 Linux ARM64 | Does the Qt arm64 build start on Ubuntu 24.04 arm64? | App starts and renders | Build from source on the target OS. |

OPEN D26: the timing of Linux ARM64 builds. [workshop.md](workshop.md) D12 says "early", but [roadmap.md](roadmap.md) M8 puts the Frame builds last. The owner needs to choose.

## 14. Open decisions

Numbers continue from [workshop.md](workshop.md) (last is D22). Record each one there when decided.

| ID | Question | Options | Recommendation |
|---|---|---|---|
| D23 | Executable shape | One executable with modes / two executables | One executable with modes |
| D24 | Graphics API for the Hub's Qt Quick | OpenGL / Vulkan | OpenGL first (Qt example and shipping precedent). Vulkan only if needed. |
| D25 | Qt version pin | Latest / LTS | Decide after S1 |
| D26 | Linux ARM64 timing | Early (D12) / last (roadmap M8) | Owner |
| D27 | Qt source on arm64 | Online Installer / distro / source build | Online Installer if S4 passes; source build if the glibc floor blocks it |
| D28 | Flatpak | Yes / no | No for v1. AppImage first. |
| D29 | Hub state location | `%LOCALAPPDATA%` (01 section 2.9) / portable folder (config-spec section 2) | Portable folder, with `%LOCALAPPDATA%` as a fallback. Owner. |
| D30 | Native dialogs in VR mode | Hide the action / "Do this on the desktop" | "Do this on the desktop" |
| D31 | UI token file | TOML loaded in C++ / generated QML | TOML loaded in C++ into `Theme` |
| D32 | Validation tooling | Python `tools/validate_catalog.py` only / plus C++ runtime warn mode | Both: Python in CI, C++ at runtime |
| D33 | VC++ runtime for the portable folder | Ship files / require install | Owner |

## 15. Stale statements to fix (not edited here)

These files conflict with D4 (Qt 6 / QML, decided 2026-10-08). They are listed for a later edit. No files were changed for this item.

- `AGENCY.md` line 18: "cross-platform Hub (Tauri)". Replace with Qt 6 / QML.
- `AGENCY.md` line 66: "Decide D4 (Hub technology)". D4 is decided.
- [docs/ui/01-shell-and-filters.md](ui/01-shell-and-filters.md) section 3.1 (PowerShell + WPF), section 3.2 (Tauri 2), section 4 item 1 ("D4 still open").
- [docs/ui/02-game-cards.md](ui/02-game-cards.md) sections 4.1 (WPF) and 4.2 (Tauri). The token proposal in 4.3 still applies.
- [docs/ui/03-detail-and-install-flow.md](ui/03-detail-and-install-flow.md) section 3 (WPF or web), including the Tauri branch.
- [docs/frontend.md](frontend.md) section 6, the table row for Qt: "Native on Windows and Linux x86_64/ARM64" needs the Arm desktop caveat (section 9 of this document).

## 16. Sources

Qt documentation (doc.qt.io/qt-6, pages labelled Qt 6.12):
- https://doc.qt.io/qt-6/qquickrendercontrol.html
- https://doc.qt.io/qt-6/qquickrendertarget.html
- https://doc.qt.io/qt-6/qquickgraphicsdevice.html
- https://doc.qt.io/qt-6/qquickwindow.html
- https://doc.qt.io/qt-6/qrhitexture.html
- https://doc.qt.io/qt-6/qtquick-rendercontrol-rendercontrol-rhi-example.html
- https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html
- https://doc.qt.io/qt-6/qtquick-performance.html
- https://doc.qt.io/qt-6/qquickimageprovider.html
- https://doc.qt.io/qt-6/linux.html
- https://doc.qt.io/qt-6/windows-deployment.html
- https://doc.qt.io/qt-6/licensing.html

Qt source (qtdeclarative, default branch `dev`):
- https://github.com/qt/qtdeclarative/tree/dev/examples/quick/rendercontrol (`rendercontrol_opengl`, `rendercontrol_d3d11`, `rendercontrol_rhi`)
- https://github.com/qt/qtdeclarative/tree/dev/tests/auto/quick/qquickrendercontrol (Vulkan path in tests)

OpenVR:
- https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h (`Texture_t`, `ETextureType`, `VRVulkanTextureData_t`, `IVRCompositor` Vulkan extension calls, `IVROverlay_028`, `IVRCompositor_029`, `IVRSystem_026`)
- https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview

Shipping precedent:
- https://github.com/OpenVR-Advanced-Settings/OpenVR-AdvancedSettings (Qt overlay, `.pro` with windeployqt, AppImage release 5.8.17)
- https://github.com/elvissteinjr/DesktopPlus (zero-code option)

Licences and libraries:
- https://www.gnu.org/licenses/lgpl-3.0.html (sections 3 and 4)
- https://github.com/marzer/tomlplusplus (MIT)
- https://github.com/pboettch/json-schema-validator (MIT)
- https://github.com/nlohmann/json (MIT)

Packaging:
- https://github.com/linuxdeploy/linuxdeploy and https://github.com/linuxdeploy/linuxdeploy-plugin-qt (assets on the `continuous` tag)

Steam runtime:
- https://github.com/ValveSoftware/steam-runtime (README and `doc/`; no Qt mention)
