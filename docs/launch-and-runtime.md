# Launch and runtime

Status: design draft, 2026-10-08. Covers how the Hub starts a VR setup, how games appear in the Steam and SteamVR libraries, and what the user sees when a step fails. Related: [frontend.md](frontend.md) §1 and §3, [architecture.md](architecture.md) §5, [config-spec.md](config-spec.md), [workshop.md](workshop.md) D16, D21 and D22.

Evidence tags:

- **[V]** verified from primary text or code, read 2026-10-08.
- **[C]** community or secondary source. A starting point, to be tested.
- **[U]** unverified. Test on the owner's PC before the Hub depends on it.

## 1. Rules

1. The Hub never changes the Windows default OpenXR runtime. It pins a runtime for one launch by setting `XR_RUNTIME_JSON` in that child process's environment only. [V: the loader reads it before the registry. DR-89 does the same in `pc/Start-SteamVR.ps1`, upstream https://github.com/DR-89/time-crisis-vr]
2. SteamVR is the default runtime for PC play. It is the path for Steam Frame streaming and for Quest over PC (ALVR, Link, Steam Link), and the only one with OpenVR overlays for the Hub (see [architecture.md](architecture.md) §5 and D22). [V: architecture.md §5; OpenVR overlays exist only in SteamVR's compositor]
3. Steam's `shortcuts.vdf` is written only with Steam closed, after a backup, and only entries the Hub owns are changed. [C, Valve wiki quoted in §5.1]
4. Every launch writes a log, and every error dialog links to it.

## 2. Pre-flight checks

Run before anything starts. No side effects.

| Check | How | Result |
|---|---|---|
| Setup installed | `installed_when` from the recipe ([frontend.md](frontend.md) §3) | Otherwise "Not installed" |
| Runtime plan | `runtime` in `[launch.<platform>]`: `steamvr` (PC default) or `any` | Pinned path, or leave the Windows default alone |
| SteamVR manifest | `HKCU\Software\Valve\Steam\SteamPath`, then `steamapps\common\SteamVR\steamxr_win64.json` in each library listed in `steamapps\libraryfolders.vdf`. DR-89's launcher checks only the main folder. [V] | Path, or "SteamVR not found" |
| Default runtime | HKLM `SOFTWARE\Khronos\OpenXR\1`, value `ActiveRuntime`. [V] | Logged only |
| Inherited override | `XR_RUNTIME_JSON` already set in the Hub's environment. It overrides the registry for every OpenXR app. [V] | Notice if it points elsewhere |
| Linux runtime | `$XDG_CONFIG_HOME/openxr/1/active_runtime.json` (default `~/.config`), then each `XDG_CONFIG_DIRS` entry (default `/etc/xdg`), then `/etc`. [V, loader source] | Path, or "No OpenXR runtime configured" |
| Already running | Lock file per setup holding the PID | "Already in VR"; focus the window |
| Streaming bridge | ALVR or Virtual Desktop process present. [C] | Log line only |

The headset is not checked in pre-flight. The reliable signal is the runtime's answer after launch (`XR_ERROR_FORM_FACTOR_UNAVAILABLE`, see §8). [V, DR-89 PCVR.md, upstream repo in Sources]

## 3. Launch sequence

1. **Ensure SteamVR** (only when `runtime = steamvr`).
   - If `vrserver.exe` is running, continue. [C: process name]
   - Otherwise start SteamVR. Steam app `250820` is SteamVR. [C] Use `steam://run/250820` once Steam is running, or start Steam first. Forum posts disagree on `run` versus `rungameid` for this id. Test both (§9).
   - Wait up to 60 s (proposed) for `vrserver.exe`. Show "Starting SteamVR" with Cancel. A timeout gives the §8 error.
   - SteamVR may start with no headset attached. Continue; the setup reports it.
2. **Start the setup** as a child process.
   - Program: the setup's own exe from `[launch]`. For DR-89 that is `TimeCrisisVR.exe`, not the generated `Play SteamVR.cmd` (open decision 2).
   - Environment: a copy of the Hub's environment, plus `XR_RUNTIME_JSON` set to the pinned path when `runtime = steamvr`. In Qt: `QProcess::setProcessEnvironment`. [design]
   - Working directory: the setup folder. Arguments from `[launch]`.
   - stdout and stderr go to `user/logs/<setup-id>/<timestamp>.log`. The setup's own log stays too (DR-89 writes `timecris.log` beside the exe). [V, DR-89 PCVR.md]
3. **While running:** the Hub shows "In VR: <title>" with elapsed time. The setup owns the OpenXR session ([architecture.md](architecture.md) §1). The Hub does not open OpenXR or OpenVR while a setup runs. [design]
4. **Exit**
   - Code 0: normal end. Update last-played in the Hub's own data. Steam's file is updated at the next library write.
   - Non-zero or crash: show "The setup stopped unexpectedly" with the last 40 log lines (§8).
5. **Return:** raise the Hub window. In the headset, the setup's pause menu offers "Back to Hub" ([frontend.md](frontend.md) §2). That path ends the setup the same way a normal exit does, so the Hub must handle both.
6. **SteamVR** is left running by default. Stopping it is open decision 3. No supported shutdown route has been found. [U]

Launches from the SteamVR library (§5) start the shortcut target directly. Steps 1 and 2 run only when that target is the Hub (open decision 1).

## 4. Setup launch config

Extends `[launch.<platform>]` in [config-spec.md](config-spec.md) §4 and the `[variant.<id>.launch]` block in [game-packages.md](game-packages.md):

```toml
[launch.windows-x64]
exe     = "${install_dir}/TimeCrisisVR.exe"   # the setup's own exe (open decision 2)
cwd     = "${install_dir}"
args    = []
runtime = "steamvr"                           # "steamvr" or "any"
```

`runtime = "any"` leaves the Windows default alone. Use it for setups that do not work with SteamVR.

## 5. Steam library entries

Goal: each installed setup appears in the Steam library, and with the VR flag in the SteamVR library, with art, and starts from the headset with no lobby (D16; roadmap M1 "done when").

### 5.1 Safety rules

- **Steam fully closed.** The Valve developer wiki says Steam must be shut down before a shortcut is added. [C, Valve developer wiki, Steam Library Shortcuts] The Hub checks for `steam.exe`, shows "Close Steam, then Retry", and never ends Steam without an explicit click. [design]
- **Account.** Enumerate `userdata\<id>\config` folders, skipping the anonymous account. If there is more than one, ask which to write. [design]
- **Backup first.** Copy `shortcuts.vdf` to `user/backups/steam/<account>/shortcuts-<timestamp>.vdf` and keep the last 10. Back up any grid file about to be replaced. [design; community reports Steam may delete a malformed file]
- **Parse before writing.** If parsing fails, change nothing, keep the backup, and show the error. [design]
- **Atomic write.** Write a temp file in the same folder, then rename it over `shortcuts.vdf`. [design]
- **Preserve what we do not own.** Every other entry and field is kept byte for byte. Test: parse and write an unchanged file and diff the bytes. [C]

### 5.2 Fields written

| Field | Value |
|---|---|
| `appid` | §5.3 |
| `AppName` | Setup title from the catalog |
| `Exe` | Quoted launch target: the Hub's launcher or the setup exe (open decision 1) |
| `StartDir` | Quoted install folder |
| `LaunchOptions` | Launcher arguments, e.g. `--launch <setup-id>` when the target is the Hub |
| `icon` | Hub-generated icon path, optional |
| `ShortcutPath` | empty |
| `IsHidden` | 0 |
| `AllowDesktopConfig`, `AllowOverlay` | 1 [C] |
| `OpenVR` | 1 for VR variants, 0 for flat variants (all three M1 launches). [C: most likely the "Include in VR Library" flag; owner test pending] |
| `Devkit`, `DevkitGameID`, `DevkitOverrideAppID` | 0, empty, 0 [C] |
| `LastPlayTime` | 0, or the last-played Unix time [C] |
| `tags` | Contains `AladdinsCastle` as the ownership marker. Array or nested object: sources disagree (test) |

Integer-like fields (`IsHidden`, `OpenVR`, and the rest) must match the type Steam writes. Confirm against a file Steam itself saved.

### 5.3 AppId and idempotency

- Implemented formula: `appid = CRC32(Exe + AppName) | 0x80000000`, using UTF-8 bytes, the stored quoted `Exe`, reflected polynomial 0xEDB88320, and no terminating NUL. [V: Steam ROM Manager primary `generate-app-id.ts`, read 2026-10-09; owner-PC Steam verification pending] The 64-bit launch ID is `(uint64(appid) << 32) | 0x02000000`.
- Key: `appid`. A matching entry is updated; a missing one is appended.
- Only entries tagged `AladdinsCastle` are ever updated or removed.
- Retain the AppId already stored in a matching owned game entry when renaming a setup or moving the Hub. Ownership requires both `AladdinsCastle` and `AladdinsCastle:<game-id>` tags. Refuse any AppId collision with a different entry.

### 5.4 Art

Folder: `userdata\<account>\config\grid\`. File names use the unsigned decimal appid. [C]

| File | Target size | Basis |
|---|---|---|
| `<appid>.png` | 920 x 430 (horizontal) | Steamworks library header. [V size, applied to shortcuts by assumption] |
| `<appid>p.png` | 600 x 900 (vertical) | Steamworks library capsule. [V size] |
| `<appid>_hero.png` | 1920 x 620 or 3840 x 1240 (test which Steam shows) | Steamworks library hero. [V size] |
| `<appid>_logo.png` | 1280 px wide, transparent PNG | Steamworks library logo. [V] |

- Art comes from the Hub's fallback generator ([frontend.md](frontend.md) §5, item 3) or from the user's own art in `user/art/<id>/`. Never from game-owned or third-party files. [frontend.md §5]
- Art is written only after `shortcuts.vdf` succeeds. An art failure is not fatal; the shortcut still works with Steam's default art.
- Overwritten files are backed up first.
- No `_icon` grid file name was found [U]. The `icon` field is used instead.

### 5.5 Removal

On uninstall, remove the tagged entry whose appid matches and the four art files for that appid, after backup. Touch nothing else. [design]

### 5.6 Behaviour to test on the owner's PC

- Changes appear after a Steam restart. [C]
- The entry shows in the SteamVR library with `OpenVR` set. [C]
- The entry starts from the headset, streamed, and the setup reports the runtime it used.
- Steam Cloud does not overwrite `shortcuts.vdf` from another PC. [U]

## 6. SteamVR app manifests (alternative)

| | Steam shortcut with `OpenVR` | `.vrmanifest` via `IVRApplications` |
|---|---|---|
| Needs Steam | Yes, closed while writing | No |
| Shows in SteamVR library | [C] with the flag | [U] |
| Start | Steam starts the exe | `LaunchApplication` from a running OpenVR app, scene apps only [V] |
| Needs OpenVR code in the Hub | No | Yes: the Hub runs as an OpenVR app |
| Schema | Valve wiki field list, plus community | No official schema; community examples only [C] |
| Art | Grid files | `image_path` [C] |

**Recommendation:** ship Steam shortcuts first (roadmap M1). Spike manifests once the Hub overlay exists (D22, later), because a scene handoff from the overlay to a setup is the case manifests solve. [V] The OpenVR header says `LaunchApplication` is invalid when the target is a dashboard overlay. It does not say whether an overlay may call it. [U]

A spike registers each setup with `AddApplicationManifest(absolutePath, false)`, which needs an absolute path. Temporary manifests (`bTemporary = true`) are not loaded automatically. [V, OpenVR header] The spike must confirm whether registrations persist after the registering process exits, and where they are stored. [U]

## 7. Steam Frame and Linux

### 7.1 Steam Frame, streamed from a PC (first priority, D21)

- The Hub, the setups and SteamVR run on the PC. The Frame is the display. The pre-flight and launch in §2 and §3 apply unchanged.
- Pairing: the headset dashboard lists the PC, and selecting it starts SteamVR on the PC. VR games need SteamVR running on the PC first. [C: Steamworks setup page, via a secondary summary]
- Whether a PC non-Steam shortcut with `OpenVR` appears in the Frame library: [U]. Valve's documented route covers only uploaded builds, which appear under `Library > Non-Steam > Devkit Game`. [V]

### 7.2 Steam Frame, standalone SteamOS on Arm (second, D21)

- Upload route in Steamworks: Name, Local Folder, Start Command, Runtime. Runtime is Android for APKs, Steam Linux Runtime 3.0 ARM64 for Linux ARM64 binaries, or Proton for Windows x86. [V]
- **No on-device OpenXR runtime is documented.** Until one is confirmed, a standalone setup shows "Standalone VR is not available on this device yet" and offers streamed mode (§8). [V, documentation gap]
- Graphics: SteamVR on the Frame accepts only GL and Vulkan. Anything that must run there uses Vulkan or GL. [V, Khronos OpenXR-Inventory]
- Windows x86 setups run through Proton and FEX. Valve's overhead figure is "a few percent"; a secondary source says 10 to 20 percent. APK setups run through Lepton. [V, Valve docs]

### 7.3 Linux PC (SteamOS-like test box)

- Runtime lookup follows §2. Pinning with `XR_RUNTIME_JSON` works the same as on Windows. [V]
- SteamVR's Linux process layout differs: `vrmonitor` under `linux64`, `vrcompositor-launcher`. [C]
- Steam's `shortcuts.vdf` on Linux is expected under `~/.local/share/Steam/userdata/<id>/config/`. [U] Flatpak Steam keeps its data elsewhere. [U] Detect the install type first; the `FlatpakAppID` field is relevant here. [C]
- Proton is needed only for Windows executables. Native Linux setups run directly. [design]

## 8. Error states

| Condition | Detected by | Message (short form) | Actions |
|---|---|---|---|
| SteamVR not installed | Manifest not found in any library | "SteamVR was not found. Install it from Steam, or point the Hub at `steamxr_win64.json`." | Browse; open Steam |
| Inherited override | `XR_RUNTIME_JSON` set, different from the pin | "An OpenXR override is set in this session. This launch uses the runtime chosen for this setup." | Details |
| Steam open during library write | `steam.exe` running | "Close Steam to update its library. Nothing has changed." | Retry |
| `shortcuts.vdf` unreadable | Parse error | "Steam's library file could not be read. No changes were made." | Open backup folder |
| SteamVR did not start in 60 s | `vrserver.exe` not seen | "SteamVR did not start. Check that Steam can start it." | Retry; open log |
| No headset | `XR_ERROR_FORM_FACTOR_UNAVAILABLE` in the setup log | "No headset is connected to the selected runtime. Start your headset (Link, Air Link, ALVR, Virtual Desktop or Steam Frame) and retry." | Retry |
| Graphics not supported | Setup log (DR-89: OpenGL support or a GPU mismatch) | "The runtime does not support this setup's graphics API, or it runs on a different GPU." | Details |
| Setup already running | Lock file | "This setup is already running." | Focus |
| Setup crashed | Non-zero exit | "The setup stopped unexpectedly." Last 40 log lines. | Open log; diagnostics zip |
| No Linux runtime configured | §2 lookup finds nothing | "No OpenXR runtime is configured on this PC." | Setup help |
| Standalone runtime unknown | §7.2 | "Standalone VR is not available on this device yet. Use streamed mode." | Learn more |
| Art missing | File not found | Silent: the generated fallback art is used. | None |

Errors appear in the desktop Hub and the log. Once the overlay exists, they can also appear in the headset. [design]

## 9. Test plan (owner's PC, Steam closed unless noted)

1. Create one shortcut in the Steam GUI with "Include in VR Library" ticked. Record its stored appid and the shape of `tags`. Read the file with Steam closed, then restart Steam.
2. Compare candidate appid formulas (quoted and unquoted exe) with the stored value from step 1.
3. Set art in the GUI for that shortcut. Record the filenames Steam keeps and their pixel sizes.
4. Check that the entry appears in the SteamVR library and starts from the headset.
5. Start SteamVR with `steam://run/250820`, then with `steam://rungameid/250820`. Record which works.
6. Build `steam://rungameid/<64-bit>` from step 1's appid and test it with Steam running.
7. Watch `vrserver.exe` appear and disappear. Record the OpenVR init error codes with the headset off.
8. Round-trip a copy of `shortcuts.vdf` through a parser and writer, and diff the bytes.

## 10. Open decisions

1. **Shortcut target.** Point the shortcut at the Hub (`hub.exe --launch <id>`), so pre-flight runs even from the SteamVR library, or at the setup exe (simpler, but no pre-flight from the library). **Recommendation: the Hub**, with a headless launch mode that can show errors in the headset or as a notification.
2. **Setup exe in config.** Replace `exe = "${setup_dir}/Play SteamVR.cmd"` with the real exe and a runtime pin, so the Hub owns the environment and the log. **Recommendation: yes.** The `.cmd` stays for manual use. This changes [config-spec.md](config-spec.md) §4 and the launch blocks in [game-packages.md](game-packages.md).
3. **Stop SteamVR on exit** when the Hub started it. **Default: off.** No supported shutdown route found.
4. **Account selection** when more than one Steam account exists.
5. **Flatpak and other Linux Steam layouts.** In v1, or later?
6. **Manifests as well as shortcuts.** Phase 2, after the Hub overlay ([workshop.md](workshop.md) D22).
7. **`tags` shape and `OpenVR` type.** Settle from a real file (§9, step 1).
8. **Frame library visibility** of PC shortcuts with `OpenVR`. Test on Frame hardware.
9. **Hero size** for shortcuts: 1920 x 620 or 3840 x 1240.

## Sources

- Valve developer wiki, Steam Library Shortcuts: https://developer.valvesoftware.com/wiki/Steam_Library_Shortcuts
- Steamworks, Steam Frame loadgames: https://partner.steamgames.com/doc/steamframe/loadgames
- Steamworks, library assets: https://partner.steamgames.com/doc/store/assets/libraryassets
- Khronos OpenXR-SDK-Source, loader `manifest_file.cpp`: https://github.com/KhronosGroup/OpenXR-SDK-Source
- Khronos OpenXR-Inventory: https://github.com/KhronosGroup/OpenXR-Inventory
- OpenVR headers: https://github.com/ValveSoftware/openvr
- ValvePython vdf (binary VDF reference): https://github.com/ValvePython/vdf
- DR-89 Time Crisis VR: https://github.com/DR-89/time-crisis-vr
