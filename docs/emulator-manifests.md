# Emulator and tool manifests

Status: 2026-10-08, design data. Manifests live one per tool in `data/emulators/<id>.toml`. Nothing here is code yet. The Hub reads these files to install, locate, search for, configure and launch the emulators and helpers that game setups need. This file defines the schema, the platform matrix, the licence gates and how game routes map to manifests.

Decision (owner, 2026-10-08): manifests live in `data/emulators/`, not `tools/`. `docs/frontend.md` section 4 still names `tools/<id>.toml`; that line should be updated to match (not changed in this pass).

The three routes from `docs/frontend.md` section 4 apply to every tool: **Install it for me** (official source only), **I already have it** (point at a folder or exe), **Search for it** (scan known places, user confirms).

## 1. Rules that every manifest follows

1. Official sources only. `github-release` uses the project's own GitHub repo. `download` uses the project's own site or the vendor's URL. `manual` shows help text and sends the user to the official page. Mirrors, third-party archives and forks are never used for install.
2. Never game content. ROMs, discs, BIOS, firmware and game files are never fetched, copied or linked. Those belong to `media` in `games/<id>/game.toml`.
3. Pin versions. A tag is pinned and moved only after testing. A rolling tag (for example DuckStation's `latest`) is recorded by publish time and checksum, never by tag name.
4. `?` means not verified. It is a string value, never a guess. A required field that is `?` blocks automatic install and leaves the manual route.
5. Verified and unverified are separate. A platform listed in `platforms` has a build whose name or upstream page we checked. A platform listed in `unverified_platforms` has a build or claim that is not fully checked. The Hub does not offer unverified platforms as ready.
6. Upstream first. Command-line switches and config paths are taken from the project's source or docs. Batocera and RetroBat code is cited only as observed behaviour and is marked as such.
7. Every manifest must parse as TOML. Check with: `python -c "import tomllib,glob;[tomllib.load(open(f,'rb')) for f in glob.glob('data/emulators/*.toml')];print('ok')"`

## 2. Schema

### 2.1 Top-level fields

| Field | Type | Required | Meaning |
|---|---|---|---|
| `id` | string | yes | Equal to the file stem. Used as the join key from game routes. |
| `name` | string | yes | Display name. |
| `kind` | enum | yes | `emulator`, `input-bridge`, `driver`, `sdk` or `vr-tool`. |
| `upstream` | URL or `?` | yes | Official project page or repo. |
| `homepage` | URL | no | Store or site page when different from upstream. |
| `license` | SPDX id or descriptive string | yes | Verified from the licence file where possible. `none` means no licence published. `?` means unresolved. |
| `license_note` | string | no | Special terms, the source of the verification, and any caveat. |
| `redistribution` | enum | yes | `download-from-upstream-only` (the Hub downloads the unmodified upstream build), `manual-only` (the user supplies or picks it, no automatic fetch), or `not-redistributable` (locate and search only). |
| `gate` | string | no | If present, automatic install is blocked until the owner resolves it. Text says why. |
| `platforms` | array | yes | Platform keys with a verified build. May be empty. |
| `unverified_platforms` | array | yes | Platform keys with a build or claim not fully verified. May be empty. |

Platform keys: `windows-x64`, `windows-x86`, `windows-arm64`, `linux-x64`, `linux-arm64`. Every key in `platforms` or `unverified_platforms` needs a matching `[install.<key>]` block, except where no install block is useful (for example RPCS3 on Linux, where no GitHub binary exists and no block is written).

### 2.2 `[install.<platform>]`

| Field | Meaning |
|---|---|
| `do` | `github-release`, `download` or `manual`. |
| `repo` | `owner/name` for `github-release`. |
| `tag` | Release tag to pin. Use `latest` only when the upstream tag itself is rolling, and say so in `note`. |
| `asset` | Exact name or glob of the asset to download. |
| `checksum_asset` | Asset that carries the checksum (for example `SHA256SUMS`), if the release has one. |
| `url` | Direct URL for `download`, or the official page for `manual`. |
| `sha256` | Hex digest, or `?`. A `?` means the Hub records the digest at first download and shows it (proposed policy; see section 6). |
| `version` | Version string the Hub records for updates. |
| `archive` | `zip`, `7z`, `tar.gz`, `none`, or `?`. |
| `run_installer` | `true` if the asset is an installer the Hub runs with a confirmation prompt. |
| `elevation` | `admin` if the installer needs elevation (UAC prompt). |
| `help` | Text shown for `manual` installs. |
| `note` | Verification status and caveats. |

### 2.3 `[portable]`

| Field | Meaning |
|---|---|
| `marker` | A file name the emulator checks beside its exe (for example `portable.txt`), `env:NAME` (an environment variable), `cli:<switch>` (a command-line switch), `none`, or `?`. |
| `note` | Source of the claim and any caveat. |

### 2.4 `[locate]`

| Field | Meaning |
|---|---|
| `exe` | Array of file names or globs, relative to the install folder. |
| `version` | Inline table: `{ from = "file-version" }`, `{ from = "command:<cmd>" }`, or `{ from = "?" }`. |
| `min` | Minimum version the setup needs, or `?` until tested. |
| `note` | Caveats, such as an unverified exe name. |

### 2.5 `[search]`

`paths` is an array of folders to scan in the search route. Recognised variables: `${ProgramFiles}`, `${ProgramFiles(x86)}`, `${LOCALAPPDATA}`, `${HOME}`, `${scoop}`. A pattern of the form `*:/Folder*` means that folder on any drive. Search paths are heuristics, not verified install locations. Results are listed with version and are used only after the user confirms.

### 2.6 `[launch]`

| Field | Meaning |
|---|---|
| `args` | Array of arguments. Placeholders are filled by the setup recipe (see 2.9). |
| `gun_args` | Optional extra arguments added for gun-type games only. |
| `cwd` | Working directory. Usually `${tool_dir}`. |
| `exits_on_game_end` | `true`, `false`, `"n/a"` (for overlays and bridges) or `?`. |
| `note` | Source and caveat for the flags. |
| `[launch.env]` | Optional environment variables set for the child process. |

`[launch]` is required for `emulator` and `vr-tool` kinds. It is optional for `driver` and `sdk` kinds.

### 2.7 `[config]`

`files` is an array of inline tables: `{ path, format, note }`. `format` is `ini`, `xml`, `yaml`, `toml`, `json`, `cfg` or `other`. Paths are relative to the install folder (or the emulator's own config folder when it is redirected by the portable marker). The Hub writes gun and wheel keys here through the layered-override rules in `docs/config-spec.md`.

### 2.8 `[input.gun]` and `[input.wheel]`

| Field | Meaning |
|---|---|
| `method` | How a VR aim point or wheel input reaches the emulator. Examples: `absolute-host-pointer`, `mouse-device-binding`, `lua-field`, `in-process-only`, `rawinput-device-index`, `cursor-binding`, `demulshooter-bridge`, `vjoy-axes`, `not-applicable`, `none-observed`, `?`. |
| `max_guns` | Number of independent guns the method supports, or `?`. `0` for `not-applicable`. |
| `notes` | Key names, file locations, and source lines. Keys are marked as upstream, Batocera-observed or RetroBat-observed. |

### 2.9 Placeholders

Placeholders are resolved by the Hub and the setup recipe, not by the manifest. Known names: `${tool_dir}` (the tool's folder under `tools/`), `${rom.set}`, `${rom.zip}`, `${rom.basename}`, `${media.disc}`, `${media.iso}`, `${media.file}`, `${media.dir}`, `${media.eboot}`, `${media.xbe_or_xiso}`, `${bios.dir}`, `${profile.name}`, `${apk.path}`, `${display.width}`, `${display.height}`, `${logs}`. Recipes add their own names; the manifest uses only these.

### 2.10 `[meta]`

| Field | Meaning |
|---|---|
| `sources` | Public URLs only (no private or local paths). |
| `verified` | What was checked this pass, and how. |
| `unverified` | What was not checked. Mirrors the `?` values in the file. |
| `checked` | Date of the check, `YYYY-MM-DD`. |
| `confidence` | `high`, `medium` or `low`: how well the entry is sourced. |

## 3. Platform matrix

Verified means the build name or upstream page was checked on 2026-10-08. Unverified means a build is claimed or present but its architecture or run status is not checked. A dash means no build is listed.

| Tool (`id`) | windows-x64 | windows-arm64 | windows-x86 | linux-x64 | linux-arm64 | Notes |
|---|---|---|---|---|---|---|
| mame | verified | verified | - | source package only | - | `mame0289lx.zip` is source, not a binary |
| supermodel | unverified | - | - | unverified (tar.gz) | - | Architecture not in the asset names |
| model2emu | - | - | unverified | - | - | No official source; 32-bit per community |
| pcsx2 | verified | - | - | verified (AppImage) | - | ARM64 PS2 path is armsx2 |
| armsx2 | - | - | - | - | verified (AppImage in zip) | PCSX2 fork for ARM64 Linux |
| duckstation | verified | verified | - | verified (AppImage) | verified (AppImage) | Only confirmed ARM64 Linux AppImage among the core emulators |
| rpcs3 | verified | - | - | not on GitHub | - | Linux builds not verified |
| dolphin | unverified | - | - | unverified | unverified | Download channel not read (403); AArch64 JIT upstream, no ARM64 build confirmed |
| flycast | verified | - | - | verified (AppImage x86-64) | - | No ARM64 Linux build |
| redream | verified | - | - | verified (tar.gz x86-64) | - | Raspberry Pi build is aarch32, not arm64 |
| mednafen | verified | - | verified | source tarball only | - | Official site hosts win32 and win64 zips |
| xemu | verified | verified | - | verified (AppImage) | verified (AppImage) | Both ARM64 builds named in the release |
| cxbx-reloaded | unverified | - | - | - | - | CI-only build; no arch in the name |
| teknoparrot | verified (UI) | - | - | verified (UI zip; running not verified) | - | Core not in any public repo |
| demulshooter | verified | - | - | - | - | Windows x64 projects in repo |
| vjoy | unverified | - | - | - | - | Windows 10 and 11 per README |
| hidhide | verified | - | - | - | - | Windows 10 or later |
| adb | unverified | - | - | unverified | - | Page names no architecture; no aarch64 build listed |
| desktopplus | unverified | - | - | - | - | Windows 8.1 or later per README |
| hotd2-vr | verified (zip, not yet tested in a headset upstream) | - | - | - | - | Quest APK installed by game recipes; no Linux build |

Steam Frame relevance. The owner's target order is Steam Frame first, streamed through SteamVR, then standalone SteamOS ARM64. Two observations follow from the matrix. First, for the standalone ARM64 stage the verified native ARM64 Linux builds are DuckStation, xemu and ARMSX2 only. Every other emulator is x86-64 on Linux or has no confirmed Linux build. Second, Windows ARM64 builds exist for MAME, DuckStation and xemu, but the streamed stage's host is a Windows or Linux PC, not the headset. This matrix does not decide which stage runs which tool; it records what exists.

## 4. Licence gates

| Tool | Licence (verified) | Gate and rule for the Hub |
|---|---|---|
| DuckStation | CC BY-NC-ND 4.0 (LICENSE read) | Not open source. Download the unmodified upstream build only. Never bundle, repackage, patch or derive from it. Do not copy `lightgun_controller.cpp` (also CC BY-NC-ND) into GPL code. Owner review before release. |
| Model 2 Emulator | None published; closed source | `not-redistributable`. Locate and search only. No Batocera mirror and no third-party archive. |
| DemulShooter | None: repo has no LICENSE file | No licence grant, so no redistribution and no modification. Install is blocked by a `gate` until the owner decides. |
| TeknoParrot (UI) | GPL-3.0 (teknogods/TeknoParrotUI) | The UI is installed from its releases. The emulator core is not in a public repo and has no published licence. The Hub never fetches cores or game files. |
| Redream | Not published (site says free to use) | `gate` until the owner reviews the terms. Checksum is not published on the site, so the Hub records it at first download. |
| Dolphin | GPL-2.0-or-later (COPYING) | Install is `manual` until the download channel is verified. |
| RPCS3 | Source GPL-2.0 (LICENSE read). The binaries repo has no licence in GitHub metadata. | Download the upstream archive only; never repackage. |
| Mednafen | GPL-2.0 (mirror COPYING read). Official site repo metadata disagrees with an earlier note. | Confirm licence from the source tarball COPYING before release. |
| MAME | GPL-2.0 (COPYING read); some files under other licences | ROMs and BIOS are never fetched. |
| xemu | GPL-2.0 (COPYING); QEMU-style LICENSE notice | Firmware is separately licensed. The Hub does not ship or fetch firmware. |
| Cxbx-Reloaded | GPL-2.0 | Only CI builds exist. Owner confirms the pinned CI tag. |
| Supermodel | GPL-3.0 (Docs/LICENSE.txt read) | No root LICENSE file, so GitHub reports none. |
| PCSX2, ARMSX2 | GPL-3.0 | None beyond the normal rules. |
| Flycast | GPL-2.0 | None beyond the normal rules. |
| vJoy, HidHide | MIT | None beyond the normal rules. Driver installs need admin and are system-wide, not portable. |
| Desktop+ | GPL-3.0 | Single maintainer. Free on Steam (app 1494460). |
| hotd2-vr | GPL-2.0-or-later (release builds GPL-3.0, CC BY 4.0 gun model) | Third-party VR port (D44). Pre-release test builds; pin each tag. Hands and staff models it makes from the user's game stay on the user's machine. |
| adb (platform-tools) | Google's proprietary platform-tools licence | Download from Google's URLs only. No redistribution. |

## 5. How game routes map to manifests

The route keys are the `[routes]` keys in `games/<id>/game.toml` (see `docs/game-schema.md` section 2). Each key that names an emulator joins to `data/emulators/<id>.toml`. Counts below come from parsing the 413 `games/*/game.toml` files on 2026-10-08. A game can have several route keys.

| Route key | Games using it | Manifest | Status |
|---|---|---|---|
| `mame` | 256 | `mame.toml` | Written |
| `teknoparrot` | 168 | `teknoparrot.toml` | Written (UI route; core not redistributable) |
| `pcsx2` | 26 | `pcsx2.toml` | Written (ARM64 Linux candidate: `armsx2.toml`) |
| `duckstation` | 23 | `duckstation.toml` | Written |
| `dolphin` | 19 | `dolphin.toml` | Written (manual install) |
| `flycast` | 16 | `flycast.toml` | Written |
| `lindbergh-loader` | 13 | none | Not written |
| `rpcs3` | 12 | `rpcs3.toml` | Written |
| `supermodel` | 11 | `supermodel.toml` | Written |
| `mednafen` | 11 | `mednafen.toml` | Written |
| `yabasanshiro` | 11 | none | Not written |
| `native` | 9 | not an emulator | Project-native route; no manifest |
| `demul` | 7 | none | Closed; not written |
| `xemu` | 5 | `xemu.toml` | Written |
| `yuzu-like` | 4 | none | Not in the schema's route list (see note) |
| `xenia` | 4 | none | Not written |
| `cxbx` | 3 | `cxbx-reloaded.toml` | Written (route key is `cxbx`, file is `cxbx-reloaded`) |
| `model2emu` | 1 | `model2emu.toml` | Written (locate only) |
| `redream` | 0 | `redream.toml` | Written (no game uses it yet) |
| `demulshooter` | 0 | `demulshooter.toml` | Written as an input bridge, not a route |
| `mupen64plus`, `ares`, `cemu`, `ryujinx` | 0 | none | Listed in the schema; not written |

Supporting manifests with no route key: `vjoy.toml` and `hidhide.toml` (wheel and pedal input stacks), `adb.toml` (Install to Quest), `desktopplus.toml` (SteamVR overlay), `hotd2-vr.toml` (third-party true-3D Flycast fork, used by game recipes, D44), `armsx2.toml` (ARM64 Linux PS2, candidate for the `pcsx2` route).

Note on `yuzu-like`. It appears in four game files but is not in the route list in `docs/game-schema.md` section 2. The owner should either add it to the schema or retire it. No manifest is written for it.

Resolution order for a game route: the Hub reads `routes.<key>`, then `data/emulators/<manifest>.toml` for the emulator, then the install block for the current platform. If the platform has no verified block, the Hub offers the manual route.

## 6. Open items and owner decisions

1. DemulShooter has no licence. Decide whether the Hub may download and run it automatically, or only locate it.
2. DuckStation's CC BY-NC-ND terms. Confirm that downloading the unmodified upstream build for the user is acceptable, or make it `manual-only`.
3. Redream's licence terms are not published. Decide whether to gate it.
4. Checksum policy for `download` installs with `sha256 = "?"` (adb, Redream, Mednafen). Proposed: record the digest at first download, show it to the user, and pin it on the next update. The alternative is manual-only until a published digest exists.
5. Dolphin's Windows and Linux download channel was not read (403). Read it before enabling `github-release` or `download`.
6. Verify the unverified platforms (Supermodel, Dolphin, cxbx-reloaded, vJoy, DesktopPlus, adb, TeknoParrot on Linux) before offering them.
7. Mednafen's licence needs confirmation from the source tarball COPYING.
8. `docs/frontend.md` section 4 still says `tools/<id>.toml`. Update it to `data/emulators/<id>.toml`.
9. The gun and wheel `method` values are research-level. Confirm each against a running build before a setup depends on it.

## Gate decisions (2026-10-08, D41)

- `gate = "consent-install"`: the Hub downloads only the official upstream release, and only after the user confirms a screen showing the manifest's `consent` text. Used for **DemulShooter** (no published licence, memory hooks, antivirus false positives).
- `gate = "locate-only"`: the Hub never downloads it; the user points to their own install. Used for **Redream** (closed freeware with no licence text; Flycast is the default for Dreamcast/NAOMI) and **Model 2 Emulator** (closed, no official download).
- DuckStation keeps `download-from-upstream-only` (CC BY-NC-ND: unmodified upstream downloads are fine; no bundling, no derivatives).
