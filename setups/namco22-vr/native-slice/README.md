# Native synthetic System 22 slice

This restart uses the pinned upstream renderer: formula-built point packets,
polygon display list, palette, checker tile and mixer go through `ss22_prepare`
once and the resulting native painter queue through `ss22_draw` twice. No game
loader is invoked; no generated game translation or game data is linked. The old setup-local
CPU material/composition renderer is not linked by this target.

Source: [namco22-decompile](https://github.com/spacestate1/namco22-decompile),
MIT, commit `a9f1a0a1f9de62387a7138006fa36933fd477123`. The build-time edits in
`prepare.py` contain attributed upstream anchor fragments, not a vendored
engine. Technique reference: [DR-89 Time Crisis VR](https://github.com/DR-89/time-crisis-vr)
`tools/patch_upstream.py`. Its reconstruction implementation is not copied.
Preserve upstream and SDL notices beside local SDK/source storage.

`source-hashes.json` pins 12 approved generic translation units, the MIT notice, root engine
headers and the native Windows compatibility header (LF-normalized hashes match
the committed pin; Windows CRLF checkout conversion is allowed). The patcher verifies HEAD,
every input hash and unique edit anchors before writing a fresh `.local` overlay.
It never changes the reference checkout. Native units are `geo_hw`, `ss22_gl`,
`eng`, `slave_list`, `tex_bake`, `render_target`, `quad_gl`, `hud_edges`, `post_gl`,
`fog_hw`, `sprite_hw`, `text_hw`. No source glob compiles other native units.
The unused upstream headless-context helper is guarded out only for this target;
it depends on a Windows context utility outside the approved unit list. The
fixture creates a real SDL compatibility context directly. There is no replacement
stub. MinGW PE linking retained unused generic file-loader/capture symbols despite
function-section flags. The fixture invokes none of them. Removing those symbols
is a separate guarded build-boundary decision; no content loading is admitted.

The `acvr_eye` seam consumes the shared column-major matrix/rectangle layout.
It converts captured +Z board view to canonical -Z, applies the supplied eye
transform and projection once, then feeds the native screen-space rasterizer.
The prepared queue and original board depth, sort key, emission tie, palette,
texture and fog inputs remain unchanged. There is no second hardware perspective
divide: the native rasterizer still draws under its orthographic projection.
Each eye clear is scissored to its own rectangle.

This is a deliberately bounded witness, not a registered native backend. It
accepts camera-local X/Y eye translation with identity rotation and scale,
parallel/off-axis GL frusta and wholly visible four-vertex polygons. It rejects
rotation, Z translation, near/guard clipping, direct polygons, partial native
viewports, sprites, nonzero character/text RAM, spot layers and nonidentity
gamma. Eye homogeneous W must equal original board Z, retaining native
projective texture and fog math. It suppresses only the admitted blank text
plane and identity gamma scanout, and does not apply widescreen backdrop copies.
No shared gun depth, aim, world scale calibration, head-turn culling, native HUD,
frame lease callback table, OpenXR, Time Crisis or racer admission is claimed.
The existing backend still reports no production graphics support.

The fixture uses 64 scene units eye separation, labelled as 64 mm at provisional
1000 units/metre. Its frustum is derived from the formula camera focal400 and
640x480 raster; near100/far12000 bound its authored depth range only. Those are
fixture values, not game calibration. Painter tests cover reverse emission on
equal keys and a physically nearer quad forced behind a farther background by
the board sort key. Per-eye disparity is checked against `focal*separation/Z`.
Repeated eyes must leave the native frame count and prepared queue unchanged.

Build with the installed MinGW GCC and the already approved local SDL2 2.32.10
development archive. Never invoke the upstream root/game build scripts. Supply
absolute paths for these caller variables: `SOURCE`, `OVERLAY` (fresh under
`.local`), `SDK`, `BUILD`, `CMAKE`, `NINJA`, `GCC` and `CTEST`.

```powershell
python -B "$SOURCE_ROOT/setups/namco22-vr/native-slice/prepare.py" --source "$SOURCE" --output "$OVERLAY"
& "$CMAKE" -S "$SOURCE_ROOT/setups/namco22-vr/native-slice" -B "$BUILD" -G Ninja "-DCMAKE_MAKE_PROGRAM=$NINJA" "-DCMAKE_C_COMPILER=$GCC" "-DN22_OVERLAY=$OVERLAY" "-DSDL2_ROOT=$SDK" -DCMAKE_BUILD_TYPE=Release
& "$CMAKE" --build "$BUILD" --parallel 2
Copy-Item -LiteralPath "$SDK/bin/SDL2.dll" -Destination "$BUILD/SDL2.dll"
& "$CTEST" --test-dir "$BUILD" --output-on-failure
```

CTest runs `--check` without SDL initialization, window, GPU or game files.
`--draw` is a separately authorized GPU check, not a CI test:

```powershell
& "$BUILD/n22_native_fixture.exe" --draw "$FRESH_LOCAL_PPM"
```

Output must be a new file under private `.local` storage. The title and PPM
comment identify synthetic content. The visible1280x480 window shows left/right
halves for three seconds, then closes its own context and window. Pixel checks
cover equal-key ordering, forced priority and preservation of both backgrounds;
GL errors are fatal. No game pixels or geometry belong in this repository.
