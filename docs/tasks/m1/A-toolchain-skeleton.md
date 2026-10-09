# M1 lane A: toolchain, `hub/` skeleton, CI

**Branch:** `m1/a-skeleton`. **Depends on:** nothing. **Read:** [README](README.md), [hub-architecture.md](../../hub-architecture.md) §3, §9-§11, [legal.md](../../legal.md).

## 1. Toolchain on the owner's PC (ask the owner before installing anything)

Propose this list to the owner, then install what they approve:
- Visual Studio 2022 Build Tools with "Desktop development with C++" (MSVC v143, Windows SDK)
- CMake ≥ 3.28, Ninja
- Qt **6.8 LTS** for MSVC 2022 64-bit: Qt Quick, Qt Quick Controls, Qt Shader Tools, Qt Network, Qt Concurrent (online installer or `aqtinstall`)
- Git (present), Python 3.12 (present)

Write the exact versions installed to `docs/dev-setup.md` (no owner paths: use `<QtDir>`-style placeholders).

## 2. Repository skeleton

```
hub/
  CMakeLists.txt            # top level: hubcore, aladdinscastle-hub, hubtool, hub-tests
  cmake/                    # deps: toml++, nlohmann/json, json-schema-validator, openvr (FetchContent, pinned tags)
  src/core/                 # hubcore (static lib): empty module stubs from hub-architecture §4
  src/app/main.cpp          # parses modes: default | --overlay | --launch <id>
  qml/HubRoot.qml           # placeholder window: title + game count
  resources/theme.toml      # copied from docs/ui/theme.toml at configure time
  tests/                    # Qt Test target, one smoke test
tools/                      # existing Python tools stay here
```

- C++20, warnings as errors on our code only.
- Dependencies via CMake FetchContent at **pinned tags**: toml++ v3.4.0, nlohmann/json v3.12.0, json-schema-validator 2.4.0, OpenVR v2.15.6 (headers + `openvr_api` lib/dll).
- `aladdinscastle-hub` runs and shows "AladdinsCastle: 413 games" by counting `games/*/game.toml` (proves the data path).

## 3. CI (GitHub Actions)

- `ci.yml`: on push/PR.
  - Windows (MSVC 2022 + Qt 6.8 via `jurplel/install-qt-action`): configure, build, run `hub-tests`.
  - Linux x86_64 (Ubuntu 24.04 + Qt 6.8): same.
  - Python job: `python tools/validate_catalog.py` must exit 0; TOML parse of `data/**`, `games/**`, `docs/ui/theme.toml`.
- Artefact: the Windows portable folder (`windeployqt` output + VC++ runtime DLLs, per D33) zipped as a CI artefact.

## 4. Acceptance

- Local build on the owner's PC succeeds; the window opens and shows 413 games.
- CI is green on both OSes; the portable zip artefact runs on the owner's PC.
- `docs/dev-setup.md` lets a new developer build from scratch.
