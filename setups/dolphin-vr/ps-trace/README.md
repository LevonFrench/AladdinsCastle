# Pixel-shader trace and skip list

Finds the shader hash of a draw you want gone (a game's black letterbox quads, a splash card, a HUD piece) without the
interactive Shader Hunter, and tests a skip without editing `GameSettingsVR`. Opt-in, offline use with a private
DolphinXR build. The repository holds only original code; the patch tool edits an owned source tree under `.local`.

The hash is the value the Shader Hunter shows and `GameSettingsVR` `[ShaderOverride]` `Type=PS` uses: the CRC32 of the
pipeline's pixel-shader UID data, taken in `VertexManagerBase::Flush` exactly where the hunter takes it.

## Apply

```text
python -B setups/dolphin-vr/ps-trace/apply.py <private-source>                  # clone at the pinned commit
python -B setups/dolphin-vr/ps-trace/apply.py <private-source> --any-revision   # private copy / other revision
```

`<private-source>` contains `Source/Core/VideoCommon/VertexManagerBase.cpp` and is under `.local`. The tool makes three
exact-anchor edits (include, `OnEndFrame`, the line `bool hunter_skip = false;` in `Flush`), writes `ACPsTrace.h`
beside it, keeps LF/CRLF, refuses a missing or non-unique anchor and a half-patched file, and is idempotent. It works
before or after the `instrumentation` delta. It builds and runs nothing.

## Use

| Variable | Meaning |
|---|---|
| `AC_PS_TRACE` | CSV path (parent folder must exist; opened for replacement). Unset: no trace |
| `AC_PS_TRACE_FIRST`, `AC_PS_TRACE_LAST` | emulator frame range, counted from process start (default 0 to 100000) |
| `AC_PS_TRACE_ALL` | `1` also traces perspective draws; default is orthographic (2D) draws only |
| `AC_PS_SKIP` | comma/space list of hex pixel-shader hashes; those draws are skipped |

Columns: `frame,draw,projection,primitive,indices,ps_hash,vs_hash,viewport_x,viewport_y,viewport_w,viewport_h,blend,zmode,gen_mode`
(`projection` 0 perspective, 1 orthographic; hashes and state words in hex). At most 200000 rows. With both variables
unset the code does nothing. Vertices, textures and pixels are never read.

Method: trace a scene that shows the thing (for example a cutscene restored from a savestate), list the 2D draws that
repeat every frame, then bisect `AC_PS_SKIP` over those hashes while checking frames (for example with
`tools/letterbox_check.py --bands`). The surviving hash goes into `GameSettingsVR/<GAMEID>.ini`:

```text
[ShaderOverride_Enable]
$Black bars
[ShaderOverride]
$Black bars
Hash=<8 hex digits, zero padded to 16>
Type=PS
match_mode=exact_hash
handling=skip
```

A pixel shader shared with other 2D elements (a plain vertex-colour quad is typical) hides all of them; check the HUD.

## Checks

`python -B -m unittest -v test_apply` (CPU only): anchor transform, idempotence, refusal cases, CRLF, and, when a C++
compiler is on `PATH`, that the header compiles and parses hash lists. A successful transform is not a compile of
Dolphin; build the patched `VertexManagerBase.cpp` with the matching libraries before relying on it.
