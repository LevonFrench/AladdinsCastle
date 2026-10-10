# Proposed SDL2 SDK and generic-engine compilation scope

Approval is pending. No download, extraction, installation, content generator or
game/graphics execution is authorized by this document.

## Exact official archive

Use **SDL2 2.32.10**, the baseline pinned by `build-windows.sh` in inspected
engine commit `a9f1a0a1f9de62387a7138006fa36933fd477123`.
The [official release](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10)
and its public GitHub release metadata identify:

- Archive: `SDL2-devel-2.32.10-mingw.tar.gz`.
- [Official asset](https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz).
- Size: **13,892,870 bytes**.
- Published SHA-256: `83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2`.
- Selected SDK subtree: `SDL2-2.32.10/x86_64-w64-mingw32`.

Metadata was verified on 2026-10-10 through the official public release API; the
archive itself was not fetched or inspected. The version is a recipe pin, not a
proved universal engine API constraint: native CMake asks for `sdl2` without a
version bound. Other SDL2 versions and static-library compatibility with the
installed UCRT64 compiler have not been established. Do not substitute SDL3.

Proposed bounded owner request: fetch this one official archive, verify its size
and digest, and extract only into ignored checkout-local SDK storage, preserving
included notices. No global MSYS2/package installation, PATH change, DLL launch,
Mesa download, graphics run or game-file access. Initial use is **headers for
compile-only objects**; executable linking/CRT/DLL decisions are a later gate.

## Explicit generic translation-unit allowlist

Compile only these 15 engine units, using the four guarded overlay files where
applicable. No glob, root/game CMake target or engine build/download script:

```text
engine/ss22_run.c
engine/ss22_video.c
engine/geo_hw.c
engine/ss22_gl.c
engine/eng.c
engine/slave_list.c
engine/tex_bake.c
engine/render_target.c
engine/quad_gl.c
engine/hud_edges.c
engine/post_gl.c
engine/fog_hw.c
engine/sprite_hw.c
engine/text_hw.c
engine/frame_rule.c
```

This is the generic scheduler/video bridge plus shared renderer/layers from
`engine/engine.cmake`. Include roots are `engine/`, `engine/snd/`, `engine/c25/`,
`tools/c25oracle/` and `include/`, plus the approved SDK and setup hook headers.
Use the upstream Windows compatibility header and preserve the required GCC
`-fno-strict-aliasing -fwrapv` flags. Build objects/static archive only, not a game
executable; unresolved game/DSP entry symbols are expected at this stage.

Exclude generated game translations, all BIOS/program/wave/texture/point data,
`engine/c25/c71_bios.c`, game-specific directories, extracted content, captures,
content tools and loaders. Source pin/file hashes remain mandatory; any unexpected
include into a prohibited area stops the work for lead clarification.

Installed MinGW UCRT64 GCC/G++, zlib and OpenGL development files were found;
SDL2 headers/libraries were absent from that prefix and the known upstream SDK
cache. No fake SDL header or dependency implementation will be introduced.
