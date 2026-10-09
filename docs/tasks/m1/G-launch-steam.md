# M1 lane G: launch, runtime pinning, Steam/SteamVR library

**Branch:** `m1/g-launch`. **Depends on:** C, E. **Read:** [launch-and-runtime.md](../../launch-and-runtime.md) (normative), wiki `J:/projects/.wiki/topics/steamvr-development/wiki/references/qt6-overlay-and-steam-library.md` and `wiki/references/openxr-runtimes-and-overlays.md`.

## Build (in `hub/src/core/launch/` and `hub/src/core/steam/`)

1. **Pre-flight:** active OpenXR runtime (`XR_RUNTIME_JSON`, then HKLM `ActiveRuntime` on Windows, XDG paths on Linux); SteamVR location from Steam's `libraryfolders.vdf`; SteamVR running (`vrserver.exe`), with start-and-wait if the setup has `runtime = "steamvr"`.
2. **Launch:** child process with `XR_RUNTIME_JSON` pinned for `runtime = "steamvr"`, working dir and args from `[variant.<id>.launch]`, stdout/stderr to `user/logs/<setup>/`. "Playing" state; on exit, record last-played and raise the Hub window; non-zero exit shows the last 40 log lines.
3. **`--launch <game-id>`:** headless pre-flight + launch, for Steam shortcuts (D35).
4. **Steam library writer:** binary VDF parse and write of `userdata/<id>/config/shortcuts.vdf` with **Steam fully closed**, backups (keep 10), atomic write, byte-identical round-trip for entries we don't own, idempotent by AppId, entries tagged as ours, `OpenVR` flag set so they show in the SteamVR library. Grid art from the local art resolver (capsule, hero, logo).
5. **Removal** of our entries on uninstall.

## Tests

VDF round-trip on synthetic files (including unknown fields), AppId calculation, art file naming, runtime detection with fake registry and env.

## Owner-PC verification (ask before writing Steam files)

- Verify the AppId formula and grid filenames by writing **one** test shortcut with Steam closed, then check that Steam shows it with art and that it launches. Record the results in the PR, and correct launch-and-runtime.md where the community sources were wrong.
- Launch both M1 setups from the SteamVR library in the headset (ALVR).

## Acceptance

M1 acceptance steps 4-5 in [README](README.md) pass on the owner's PC.
