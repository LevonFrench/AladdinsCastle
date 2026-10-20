# supermodel-vr: Sega Model 3 in stereoscopic 3D and head-tracked VR

A patch for [Supermodel](https://github.com/trzy/Supermodel), the Sega Model 3 emulator, that draws every game frame once per eye from the game's own 3D data and shows the result in a PC VR headset through OpenXR.

It contains no game content and no built program. You build Supermodel yourself, from its source, with this patch applied, and you supply your own game files.

Supermodel is licensed GPL v3 or later, and so is everything in this folder.

## What it does

| Mode | What you see | How it is drawn |
|---|---|---|
| Screen | A large screen in front of you with the game in real 3D, sized so the scene is at real scale | Parallel cameras with off-axis frusta; two quad layers, one per eye |
| Wide screen | The same with a wider camera on a larger screen | The game's frustum is widened and its culling is turned off |
| Open frame | The same screen, wider than the arcade picture, with the game's world drawn past its 4:3 edges | Supermodel's own `-wide-screen` with a wide window; the patch also widens letterboxed scene views |
| Inside the game | The scene fills your view and the camera follows your head | The headset's own per-eye frusta and eye poses; a projection layer; the game's text and score on a floating panel |

The emulator keeps its own 57.5 Hz timing. The headset is fed from a separate thread at its own rate, and in the head-tracked mode each frame carries the eye poses it was drawn for, so the runtime can re-aim it between game frames.

## State on 13 October 2026

Tried in a headset through SteamVR by the project owner:

- Screen mode: Scud Race, Daytona USA 2, Sega Rally 2.
- Inside the game: Scud Race, Daytona USA 2, Star Wars Trilogy, Fighting Vipers 2, Dirt Devils.

Checked on a monitor only, with a made-up headset pose: Virtua Fighter 3, Sega Rally 2, Harley-Davidson and L.A. Riders, Spikeout, Virtua Striker 2 in the head-tracked mode; supersampling; the sky fill; the controller mapping (which has not been tried with real controllers at all).

Known limits of the head-tracked mode:

- Small views (mirrors, some gauges and maps) are not drawn.
- The view follows the arcade camera, including its tilt and bumps.
- Where a game draws nothing (straight up, far to the sides) the picture is filled with a flat sky colour.
- World scale is assumed to be one game unit per metre. It is a setting.

## Build

1. Get Supermodel's source at commit `8b4de23` (see `upstream-pin.json`) and the libraries its Windows build asks for.
2. Put the OpenXR headers where the project can find them: `Libraries\openxr\include\openxr\`.
3. From the Supermodel source root run `python <this folder>\patch\apply_acvr.py`. Every edit checks its anchor and stops if the source has moved.
4. Build the Visual Studio solution, Release, x64.
5. Put `openxr_loader.dll` beside the built program.

`tools\Build.ps1` does step 4 at low priority.

## Run

Everything is switched by environment variables, so the same program runs unchanged without them.

| Variable | Meaning |
|---|---|
| `ACVR_STEREO=1` | Draw each frame once per eye; both eyes side by side in the window |
| `ACVR_XR=1` | Also send the pictures to the headset (screen mode) |
| `ACVR_VR=1` | With `ACVR_XR`: head-tracked mode |
| `ACVR_EYE_SEP` | Screen mode: eye separation in metres (0.065) |
| `ACVR_WORLD_SCALE` | Game units per metre (1) |
| `ACVR_CONVERGENCE` | Screen mode: distance of the zero-parallax plane in metres |
| `ACVR_XR_DISTANCE`, `ACVR_XR_SIZE`, `ACVR_XR_HEIGHT` | Screen mode: where the screen is and how large |
| `ACVR_HUD_DISTANCE`, `ACVR_HUD_WIDTH` | Head-tracked: the panel that carries the game's 2D layer |
| `ACVR_FOV_SCALE`, `ACVR_NO_CULL=1` | Screen mode: wider camera, culling off |
| `ACVR_WIDE_ALL=0` | With `-wide-screen`: widen only full-screen views, as upstream does. By default letterboxed scene views (Star Wars Trilogy) and sky strips (Virtua Fighter 3) are widened too |
| `ACVR_AIM_X`, `ACVR_AIM_Y` | Analog-stick games: where the game draws its crosshair for each stick value, `value:pixel,...` over its 496x384 picture. The default is Star Wars Trilogy's, measured from its own crosshair |
| `ACVR_AIM_DISTANCE` | Head-tracked: how far ahead the arcade view is met when pointing, metres (2.5) |
| `ACVR_FAKE_CONTROLLERS` | Made-up controllers for unattended checks, used only when no real ones are read: `first-last:control:value;...` (see the top of `acvr_main_block.cpp`) |
| `ACVR_INPUT_SCRIPT` | Hold inputs during given frames, as `first-last:InputId:value;...` |
| `ACVR_HIDDEN=1`, `ACVR_CAPTURE_FRAME`, `ACVR_CAPTURE_DIR`, `ACVR_SAVE_STATE_FRAME` | Unattended runs: hidden window, save frames as pictures and quit, write a save state |
| `ACVR_VR_FAKE=1`, `ACVR_VR_FAKE_YAW` | Head-tracked rendering with a made-up headset, for checks on a monitor |
| `ACVR_DEBUG=1` | On captured frames, list every viewport and save the bottom 2D layer |

Open frame: add `-wide-screen` and give the window the shape you want, for example `-window -res=1920,1080 -ss=2`. The game area stays at the arcade's 4:3 size in the middle; the 3D scene is drawn on both sides of it, and the headset screen takes the window's shape. The 2D layers (text, score) stay in the 4:3 middle. What a game never sends to the board cannot appear: some games leave out objects that are outside their own 4:3 view.

Head-tracked mode wants square pictures and Supermodel's own supersampling: `-window -res=1344,1344 -stretch -ss=2`. Screen mode: `-window -res=1440,1080 -ss=2`.

The Home key recentres. The presenter writes what it does to `acvr-xr.log` in the working folder.

With `ACVR_XR` the game never runs unseen. It closes cleanly, saving NVRAM, in three cases: when the runtime ends the session (SteamVR's "Exit game", a lost runtime), when the presenter cannot start (no runtime, no headset), or when the presenter's thread stops. While the session is starting, or after the runtime stopped it (headset idle, standby), the game waits, paused.

### Controllers

Read through OpenXR and turned into the emulator's own inputs, by cabinet type:

- Driving: hold both grips and turn your hands like a wheel, or use the left thumbstick; right trigger accelerates, left trigger brakes; right thumbstick up and down shifts; A start, B coin, X view change, Y second view button or hand brake.
- Analog stick (Star Wars Trilogy): point the right controller at the picture and the game's crosshair goes where the ray meets it. The ray is met with the screen itself in screen mode and with the arcade view 2.5 m ahead in the head-tracked mode; the stick value is looked up from the game's measured crosshair response, which is not linear. Checked offline against the game's own crosshair at 12 points on Yavin: the largest error was 0.009 of the picture, on the normal and on the open-frame screen. Right trigger fires, right grip is the event button.
- Stick and buttons: left thumbstick, and A, B, X, Y; start on the menu button or a right-stick click, coin on a left-stick click.

The keyboard and gamepad keep working: a controller only overrides an input while it is being used.

Hold Y for 3 seconds to quit; the game saves its NVRAM before the headset presenter stops (the presenter gets at most 3 s to stop). Y keeps its game input while held, so a 3-second hand-brake hold also quits. Controller input expires after 250 ms without a fresh reading, and is dropped when the headset session loses focus or the presenter stops, so nothing stays held.

### The cabinet's own controls in the headset

With `ACVR_CONTROLS=1`, head-tracked mode draws the cabinet's wheel (driving games) or Harley-Davidson's handlebars in front of the player, turning with the steering the game receives, from whatever source: the controllers, a gamepad or the keyboard. The wheel is a procedural solid (a thick rim, three spokes, a plain hub and a short column, every part a closed mesh); the bars are the repository's own `assets/controls/bars-motorcycle.glb`. Both are made once into the format the patch reads:

    python tools/make_wheel_acm.py <Supermodel source>/Assets/acvr/wheel-solid.acm
    python tools/bake_control.py <repo>/assets/controls/bars-motorcycle.glb pivot_steering <Supermodel source>/Assets/acvr/bars-motorcycle.acm

Without `wheel-solid.acm` the older `wheel-three-spoke.acm` (baked from `assets/controls/wheel-three-spoke.glb`) is used. The models are drawn into the supersampled picture with a depth buffer of their own: that picture has none, and without it every model looked hollow (its far side showed through).

The wheel sits about 0.4 m below and 0.45 m ahead of the eyes at the moment of recentring, tilted 25 degrees like the cabinet's, and turns 135 degrees each way; the bars turn 30 degrees each way. The game's picture is drawn first and the control over it, so it is always in front.

| Variable | Meaning |
|---|---|
| `ACVR_CONTROLS=1` | Draw the cabinet control (off by default) |
| `ACVR_CONTROL_POS` | Where it sits relative to the recentred eyes, metres `x,y,z` (for example `0,-0.38,-0.45`) |
| `ACVR_TEST_STEER` | Fix the steering at -1 to 1, for unattended pictures |
| `ACVR_STICK_GRAB=1` | Star Wars Trilogy: holding the right grip takes the flight stick and tilting the hand moves it (25 degrees is full deflection, 2 degree dead zone; sideways roll steers, pulling back aims up). When the grip is not held the controller points 1:1, as without this setting. The event button moves to the right stick click while this is on |
| `ACVR_STICK_MODEL=1` | With `ACVR_STICK_GRAB` and `ACVR_CONTROLS=1`: draw the cabinet stick (`assets/controls/stick-flight.glb`, baked to `Assets/acvr/stick-flight.acm`) tilting with the hand. Off by default |
| `ACVR_STICK_INVERT=1` | With `ACVR_STICK_GRAB`: pushing forward aims up instead |
| `ACVR_TEST_STICK` | With `ACVR_STICK_GRAB`: a stick held at `pitch,roll` degrees, for unattended pictures |

Head-tracked pointing (Star Wars Trilogy): the HUD panel is placed exactly over the arcade view (`ACVR_HUD_MATCH`), the right controller's ray is met with that view at the panel's distance, and a thin red laser with a yellow dot at its end is drawn in the scene (`ACVR_LASER=0` hides them). Checked offline at 12 points against the game's own crosshair in the head-tracked mode: largest error 0.009 of the picture (about 0.4 degrees).

Not yet: hands drawn on the wheel, grabbing the rim itself, the shifter and pedals.

## Getting games past their start-up checks

Found by scripting the test menus with `ACVR_INPUT_SCRIPT`. Frame numbers are from power-on.

- **Scud Race, two-cabinet sets:** stops at "network board not present". Set Link ID to SINGLE in the test menu, Game Assignments. The emulator saves it. Script that does it once:
  `300-312:TestA:1`, eight `ServiceA` presses 40 frames apart from frame 900, `1260-1272:TestA:1`, `1450-1458:ServiceA:1`, `1520-1532:TestA:1`, `1660-1672:TestA:1`, eight `ServiceA` presses from frame 1800, `2160-2172:TestA:1`, four `ServiceA` presses from frame 2300, `2500-2512:TestA:1`.
- **Daytona USA 2, two-cabinet sets:** the same, with three presses on Link ID (MASTER, SLAVE, LIVE, SINGLE) and one more menu row.
- **Star Wars Trilogy:** stops at "failed feedback lever initialize" at every start. Entering and leaving the test menu during boot skips it: `300-312:TestA:1;700-712:TestA:1`.
- **Sega Rally 2 DX (srally2dx):** a clone set. Supermodel opens the parent `srally2.zip` from the same folder as the DX file, and the game wants `-ppc-frequency=100`. Steering, pedals, open frame and the head-tracked mode work as for Sega Rally 2.

## Files

| File | What it is |
|---|---|
| `patch\apply_acvr.py` | Applies everything to a pristine Supermodel tree |
| `patch\acvr_main_block.cpp` | Inserted into `Src\OSD\SDL\Main.cpp`: eye storage, head-tracked camera, HUD picture, sky fill, capture, controller mapping |
| `patch\ACVRXR.cpp` | The OpenXR presenter and controller reader |
| `patch\ACVRStereo.h` | State shared between the frame loop, the renderer and the presenter |
| `tools\Build.ps1` | Builds the patched solution at low priority |
| `tools\Run-Capture.ps1` | One hidden, low-priority run that saves chosen frames as pictures |
| `upstream-pin.json` | The Supermodel commit the patch is written against |

## Where it changes Supermodel

- `CNew3D::CalcViewport`: the eye offset and frustum shift (screen mode), or the headset's frustum and eye matrix (head-tracked), per viewport.
- `CNew3D::RenderFrame`: the second eye does not swap the line-of-sight buffers again.
- The node cull test: bypassed when culling is off.
- `CModel3::RenderFrame`: the frame is drawn once per eye; in head-tracked mode the 2D layers are kept out of the eye pictures.
- `CRender2D::Setup2D`: in head-tracked mode the bottom layer is drawn as a far backdrop.
- `Main.cpp`: start-up, the frame hooks, a hidden-window option and the presenter's lifetime.


## Dependency identity

`upstream-pin.json` now hashes all consumed OpenXR 1.1.43 headers and records
the versioned upstream source reference. The local version macro and content
were checked; no upstream fetch or original SDK-commit attestation is claimed.
The MSVC solution uses SDL2 2.30.11, SDL2_net 2.4.0 and zlib 1.3.1. Each has a
source URL, version-header path and SHA-256 in the pin; SDL2_net also carries
its release revision description. These are the library inputs for the v145,
Release-x64 solution, not an instruction to fetch current rolling libraries.
No emulator patch or build command changed.
