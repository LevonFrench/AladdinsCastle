# Hub development setup (M1 lane A)

M1 builds the desktop skeleton and its CLI. Overlay rendering, emulator installs,
game launch, scanning and Steam shortcuts remain later lanes. Build from a checkout
containing the catalog; no game content is required.

## Pinned toolchain

The owner's approved isolated Windows toolchain was installed and inspected on
2026-10-08. Use placeholders below for your own installation, never local library paths.

| Component | Version |
|---|---|
| Visual Studio 2022 Community | 17.14.41 |
| MSVC v143 toolset | 14.44.35207 |
| MSVC compiler | 19.44.35229.0 |
| Windows SDK | 10.0.26100.0 |
| Official VC++ x64 redistribution files | 14.44.35112 |
| Qt shared MSVC 2022 x64 | 6.8.3 |
| Python | 3.12.10 |
| aqtinstall | 3.3.0 |
| CMake | 3.31.10 (project minimum 3.28) |
| Ninja | 1.13.0 |

Git must be available. Linux CI uses Ubuntu 24.04, its GCC toolchain, and the
Qt 6.8.3 `linux_gcc_64` binary distribution. Linux desktop/ARM64 runtime
acceptance is separate from CI compilation; the M1 OpenVR import is x86_64 only.

## Install on a new Windows developer machine

Install Visual Studio 2022 with the Desktop development with C++ workload and
a Windows SDK. Toolchain installation is a developer action; the build scripts
do not install Visual Studio, activate licenses or change accounts.

A local Python environment can hold the pinned build tools:

```powershell
python -m venv <ToolsDir>/python
& <ToolsDir>/python/Scripts/python.exe -m pip install aqtinstall==3.3.0 cmake==3.31.10 ninja==1.13.0
& <ToolsDir>/python/Scripts/python.exe -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O <ToolsDir>/Qt -m qtshadertools
```

`<QtDir>` is the resulting `<ToolsDir>/Qt/6.8.3/msvc2022_64` directory.
Qt's base desktop archive supplies Quick, Quick Controls, Network, Concurrent and
OpenGL. Shader Tools is the additional module. CMake requires shared Qt 6.8.3
exactly, so a future version change must be an explicit pin update.

## Configure, build and test

Open an **x64 Native Tools Command Prompt for VS 2022** and start PowerShell
inside it, or use Visual Studio's developer PowerShell. Enter the repository
root (`<RepoRoot>`) and set the tools and Qt paths for this shell:

```powershell
$env:Path = "<ToolsDir>/python/Scripts;<QtDir>/bin;$env:Path"
cmake -S hub -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<QtDir> -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python tools/validate_catalog.py
& ./build/bin/hubtool.exe count
& ./build/bin/aladdinscastle-hub.exe
```

On Linux, put the Qt `bin` directory on PATH, use the same CMake commands with
`-DCMAKE_PREFIX_PATH=<QtDir>`, then run `./build/bin/hubtool count`.
Ubuntu needs Ninja, OpenGL development headers, xkbcommon and the xcb cursor
runtime (CI installs these). Qt Test uses synthetic temporary catalog folders;
it never requires ROMs, BIOS files, emulator installs, Steam or SteamVR.

Build outputs remain under `build/`. Targets are `hubcore` (static),
`aladdinscastle-hub`, `hubtool`, and `hub-tests`. Our targets use C++20
and warnings as errors. Dependencies are marked as system code, and never
inherit those warning options. Qt is shared; OpenVR uses upstream headers plus
the pinned prebuilt shared runtime. No OpenVR initialization is implemented in A.

The QML module is `AladdinsCastle.Hub` with `DesktopShell` and `HubRoot`.
The app loads it through `QQmlApplicationEngine::loadFromModule`.
CMake copies `docs/ui/theme.toml` into **build/resources/theme.toml** at
configure time and embeds it as `qrc:/resources/theme.toml`. This deliberately
keeps the generated theme copy out of the source tree instead of the brief's
`hub/resources/` location.

The app resolves catalog roots from `--data-root <folder>`, its executable
folder, and checkout ancestors. It does not bake the build machine's repository
path into the executable. A portable package has `games/` and `data/`
beside its binaries and keeps user state in `user/`.

## Dependency pins

FetchContent clones only these tags on first configure; an absent tag fails
configuration. Public tag references were verified on 2026-10-08:

| Dependency | Tag | Tag reference commit |
|---|---|---|
| [toml++](https://github.com/marzer/tomlplusplus/tree/v3.4.0) | v3.4.0 | 30172438cee64926dc41fdd9c11fb3ba5b2ba9de |
| [nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0) | v3.12.0 | 65ee68451d8eb2b5f3a30b410476ab83deb3289b |
| [json-schema-validator](https://github.com/pboettch/json-schema-validator/tree/2.4.0) | 2.4.0 | 55b49c221b41c8369342d4d23e52d9f31119c848 |
| [OpenVR](https://github.com/ValveSoftware/openvr/tree/v2.15.6) | v2.15.6 | 41bc3825fd35b04047610c86fee26fb33b017b29 |

Tag pins follow the lane brief. These recorded tag references are receipts,
rather than a claim that upstream tags are immutable. Cached FetchContent
sources can be reused for offline rebuilds after one successful configure.
Dependency license texts are copied to `build/licenses/`.

## Windows portable package

After a Release build, run from the same x64 developer shell:

```powershell
./hub/packaging/portable-windows.ps1 -BuildDir build -QtDir <QtDir> -OutputDir <NewPortableFolder>
```

The output folder must not exist. The script uses `windeployqt`, includes
the CLI and OpenVR DLL, copies catalog/config metadata, creates `user/`,
and emits `<NewPortableFolder>.zip`. Optional `schemas/` and `recipes/`
are copied only when present. No local user folders or game art are staged.

VC++ DLLs come exclusively from the official VS `VC/Redist/MSVC/<version>/x64/Microsoft.VC*.CRT`
folder. The developer shell supplies `VCToolsRedistDir`; alternatively pass
`-VsRedistDir <OfficialRedistVersionDir>`. The script fails if the official
CRT directory or mandatory DLLs are missing. It never takes DLLs from System32.
VS 2022's DLL is **msvcp140.dll**, correcting the old architecture example's
msvcp170.dll. See [Microsoft's redistribution guidance](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute?view=msvc-170).

Notices include project GPL-3.0, dependency license texts, Qt LGPL-3.0, available
Qt installation notices, Qt SBOMs when present and exact Qt source links.
Read `hub/packaging/THIRD-PARTY-NOTICES.md` for the public-release notice audit.

## CI and acceptance

GitHub Actions runs Python catalog validation and parses every TOML file in
`data/`, `games/`, plus `docs/ui/theme.toml`. Windows 2022 and Ubuntu
24.04 jobs configure, build and run CTest. Windows also builds the portable ZIP,
runs the packaged CLI against its bundled catalog and uploads the ZIP.

The workflow's presence is not a green CI receipt. A fresh portable ZIP running
on the owner's PC, Linux runtime acceptance and remote CI results must be recorded
after those checks actually occur. The 413-game count comes from catalog folders,
not a hard-coded display number.

Qt build and deployment references:
[qt_add_qml_module](https://doc.qt.io/qt-6.8/qt-add-qml-module.html),
[windeployqt](https://doc.qt.io/qt-6.8/windows-deployment.html).
