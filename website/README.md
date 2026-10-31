# Project website

The public site for AC VR Hub. It lives here, in `website/`, and is kept current as the project moves. This page is for whoever maintains it. It covers what each file does, how the arcade walk is put together, where its facts come from, and how to test and publish a change.

The live preview is https://preview.ac-vr-hub.pages.dev (Cloudflare Pages project `ac-vr-hub`, branch `preview`).

## Ground rules

- **Never claim more than the repo does.** Every status on the site comes from [docs/status/current.md](../docs/status/current.md). E3 (accepted in the headset) appears only where `current.md` says the owner accepted it. Anything an agent can prove is E2 or E1. Say "not yet seen in the headset" when that is true.
- **No Aladdin's Castle branding.** No name, logos or photos. The one exception is the owner-approved music title in the player. The room is generated in code; see [legal.md](../docs/legal.md).
- **Only the project's own art.** Guns, hands and controls are the CC0 models from `assets/`. The only third-party art is the MIT ASCII animations. The CC BY `arc-light-gun` is left out, so it needs no credit line on the site.
- **Silent tests.** Add `?mute` to any page during test runs.
- **Run `tools/check_public_privacy.py`** (repo root) before every push.

## Pages

| Page | What it is | Source |
|---|---|---|
| `index.html` | The arcade walk: nine glass-panel sections over a Three.js arcade | Hand-written |
| `how-it-works.html` | Files to headset, the SteamVR panel, VR pinball, and each system's route into OpenXR | Hand-written |
| `games.html` | The game list, one section per system | **Generated** by `tools/make_games.py` from `tools/games_head.html`, the `SYSTEMS` table and `tools/games_foot.html` |
| `notices.html` | Licences and external dependencies | **Generated** by `tools/make_notices.py` from `THIRD-PARTY-NOTICES.md` |
| `privacy.html` | What the site collects (nothing) and which outside hosts it contacts | Hand-written |
| `404.html` | "Game over" with animated TV static | Hand-written |

Never hand-edit a generated page: the next regeneration wipes the edit. Change the source, then rerun the generator.

## Files

| File | Role |
|---|---|
| `arcade.js` | The 3D scene and the walk: room, cabinets, attract screens, exhibits, camera stops, scroll and input, render loop |
| `models.js` | Loads `models/models.json` and the GLBs, restyles materials and attaches grip hands. Shared by `arcade.js` and `bench.js` |
| `bench.js` | The model bench on the Gun models panel: a second WebGL canvas that shows one model at a time |
| `page.js` | Slides `.reveal` panels in as they scroll into view. Kept separate from the 3D scene so the page works without it |
| `music.js` | The music bar on every page (see Music) |
| `ascii-deco.js` | Flow pages: inserts ASCII dividers and sizes each `<ascii-art>` to its box |
| `sprites.js` | Pixel-art SVG icons on How It Works, drawn from character grids |
| `style.css` | All arcade-page styles, plus shared ones (HUD links, buttons, `.state` badges, music bar) |
| `flow.css` | Layout for the flow pages (How It Works, Game List, 404, notices, privacy) |
| `models/` | Copied GLBs and `models.json` (from `tools/sync_models.py`) |
| `ascii/` | The ascii-magic-art runtime and the chosen pieces (from `tools/sync_ascii.py`) |
| `audio/` | The theme track |
| `fonts/` | Self-hosted woff2 fonts with their OFL texts |
| `_headers` | Cloudflare caching rules |
| `tools/` | Generators, sync scripts and the deploy stager. Never published |

## The arcade walk (`index.html` + `arcade.js`)

The page is a column of `section.stop` elements. As you scroll, the camera in the fixed WebGL canvas behind the panels walks a spline through the arcade and stops in front of each section's exhibit.

| Stop | Section | Panels | Exhibit in the open third |
|---|---|---|---|
| 0 | Welcome (hero) | centred | The storefront sign, which animates from ARCADE to AC VR HUB |
| 1 | How you play | right | A gun held in a grip hand, on a turntable |
| 2 | Gun models | left | The project's guns, taking turns on a stand; the model bench sits in the panel |
| 3 | Ghost controls | right | Sit-down racer with ghost wheel, shifter and pedals |
| 4 | Arcade boards | left | Model 3 cabinets and the mounted machine gun on a turntable |
| 5 | Consoles | right | Wii and PS2 kiosks |
| 6 | Open source | left | The prize counter |
| 7 | The Hub | right | Three Hub cabinets |
| 8 | Where it stands | left | The STATUS board |

### Layout and browser zoom

- **Two thirds, alternating.** From 900 CSS px wide, each stop's `.wrap` takes 2/3 of the page width and leaves the other third open for the exhibit. `data-side="right"` puts the panels on the right, `left` on the left. Sides alternate from stop 1. The hero is centred at 2/3.
- **Gutters.** Right-hand panels keep `--gut-r` (110–150 px) clear for the stop dots. Left-hand panels use the smaller `--gut-l`.
- **Zoom.** Widths are percentages, not pixels, so browser zoom keeps the 2/3 split: zooming in makes the text bigger inside the same share of the page. Past about 200% on a 1920 screen the window drops under 900 CSS px. The page then switches to a single centred column, and from 720 px down to the phone layout.
- **Container queries.** `.wrap` is a size container, so grids follow the panel's own width rather than the window's:
  - Under 560 px, card grids drop to one column and the bench stacks.
  - Under 640 px, the four stat tiles go two-up.
- **Portrait windows.** On a phone or narrow portrait window the panels fill the width. The camera aims so the exhibit sits in the top fifth, above the first panel.

### Camera stops

`STOPS` in `arcade.js` has one entry per section, in the same order:

```js
{ pos, look, fov, subject, side }
```

- **`pos`** is where the camera stands.
- **`look`** is the plain look target. It is used when there is no side layout.
- **`subject`** is the point to frame.
- **`side`** must match the section's `data-side`.

`aimStops()` turns the camera so `subject` lands in the middle of the open third, at 2/3 of the way from centre to the open edge. It runs at start and on every resize, including zoom. `PATH` adds waypoints between stops so the walk stays at least 0.5 m clear of cabinets.

To **add, remove or reorder a stop**:
1. Add or move the `section.stop` in `index.html`, with `id="stop-N"` and `data-stop="N"`.
2. Make the matching change in `STOPS`. Section count and `STOPS` length must be equal.
3. Renumber `data-side` so sides still alternate, and set each `STOPS[i].side` to match.
4. Add the stop's position to `PATH` in walk order.
5. Add a dot to the `.stops` nav.
6. Check every stop at 1920×950, 960×475 and phone width.

### Building blocks in `arcade.js`

The file is split by comment rules (`// ----`). In order:
- **helpers:** canvas, texture and material factories, `box()`, `plane()`, `markClickable()`.
- **textures:** mall tiles, carpet, mural, the animated sign, marquees, the token.
- **attract-mode screens:** `screen(draw, w, h)` makes a canvas texture redrawn by a function. `asciiScreen(slug)` plays an ASCII piece. `screenBoard` is the STATUS high-score table.
- **lighting.**
- **mall corridor, storefront and arcade room:** the corridor, storefront and room, from front to back along -z.
- **cabinets:**
  - `upright()`, `sitdown()`, `claw()`, `wheelGame()` and `hoops()` build the cabinets.
  - Pass `stop` to make one clickable; clicking it walks to that stop.
  - Pass `screenTex` to give it a screen.
- **the project's own models:**
  - `realGun()` loads a gun and adds a muzzle flash.
  - `fire()` animates trigger, slide and flash.
  - `turntable()` spins a displayed model.
  - `exhibitHolder(wide, narrow)` gives an exhibit two positions, blended by aspect ratio in `applyLayout()`, so exhibits spread out on wide screens and gather on narrow ones.
  - `whenNear(stop, fn)` loads a stop's models only once the walk gets near it.
- **systems:** `kiosk()` for the console stands.
- **static batching:** merges unmoving meshes to cut draw calls. Put anything that moves in a group with `userData.dynamic = true`.
- **post-processing:** bloom and FXAA.
- **the walk:** `STOPS`, `PATH`, `aimStops()`, scroll progress, picking and hover, `resize()`.
- **loop:** the frame loop, the quality governor and idle handling.

### Status board and screens

`screenBoard` draws the STATUS table in the room from a `rows` array of `[system, text, level, colour]`. Update it with the panels whenever `current.md` changes.

Cabinet attract screens are ASCII pieces assigned through `ascii(slug)` in the cabinet calls. A screen redraws at most 12 times a second, only while the camera is within 9 m of it. At most two screens draw per tick.

## Models and the bench

`python website/tools/sync_models.py` (repo root):
- Copies the GLBs listed in its `GUNS`, `CONTROLS` and `HANDS` tables from `assets/` into `models/`.
- Writes `models/models.json`. Each entry has the model's name, family, hold, length, default tint and moving parts (from `data/guns/*.toml`). It also has the grip-hand placement, copied unchanged from `assets/hands/assembly.json`, which puts the index finger on the trigger.
- Records the catalog size.

To add a model, add its id to the right table, rerun the script, then bump `VER` (see Cache tags). The bench lists every non-hand entry in `models.json` automatically.

`models.js` restyles materials in one of three styles:
- `solid`: the default.
- `runtime`: Lambert, close to how the hosts light the guns.
- `ghost`: translucent controls.

Tints come from the data files, not the GLB. `loadModel(id, { hands: true })` attaches the right grip hand at the model's `grip` node.

`bench.js` starts only when its panel comes within 300 px of the viewport. You drag to turn the model, and Fire runs its trigger and recoil parts. If WebGL fails, the bench hides its canvas and keeps the text.

## Where the facts come from

| Site text | Source | How to update |
|---|---|---|
| System cards, "Accepted in the headset", "Still open", the hero counts | `docs/status/current.md` | Edit `index.html` |
| "From the workshop" log on the scoreboard | Merged PRs and the "Merged since" table in `current.md` | Edit `index.html`. Keep it to about six recent items, dated by merge date, marked offline-only unless the headset saw it |
| Game List | `current.md` and the Hub's game tables (`hub/src/core/*/*.cpp`) | Edit `SYSTEMS` in `tools/make_games.py` (and the pinball text in `tools/games_foot.html`), then `python website/tools/make_games.py website` |
| How It Works routes and notes | `current.md` and each setup README under `setups/` | Edit `how-it-works.html` |
| STATUS board in the scene | `current.md` | `screenBoard` rows in `arcade.js` |
| Controls wording | `docs/controls.md` and `hub/src/core/launch/UniversalControls.h` | Use the same U1–U4 sentence everywhere |
| Licences | `THIRD-PARTY-NOTICES.md` | Edit it, then `python website/tools/make_notices.py website` |

Status badges use `<span class="state ok|wip|no">`:
- green `ok` for E3;
- amber `wip` for E2 or anything with an open problem;
- grey `no` for E1.

When `current.md` changes, search the site for each number and claim it touches: counts, percentages, dates, "accepted". A stale number can survive in four places: `index.html`, `how-it-works.html`, `make_games.py` and `screenBoard`.

## ASCII art

The cabinet attract screens, the header art and dividers on How It Works and the Game List, and the 404's TV static are animated ASCII pieces from [ascii-magic-art](https://github.com/kailashsr38/ascii-magic-art). It is MIT-licensed, and the licence is in `ascii/LICENSE`.

`python website/tools/sync_ascii.py <path to an ascii-magic-art checkout>` copies the runtime, the helper kit and only the pieces listed in that script into `ascii/`. It never copies the two photo pieces (`ocean-sunset`, `starry-ridge`), which keep the Unsplash License.

On the flow pages, `ascii-deco.js`:
- inserts a divider before every section after the first;
- sets each `<ascii-art data-cols data-max>` font size from its box width, in half-pixel steps, so small resizes do not restart a piece.

The EQ in the music bar is the site's own. It draws eight bands of the playing track, read through a Web Audio analyser, in block characters.

## Music

`music.js` plays `audio/l-von-theme.mp3` (L-VON, "Theme From Aladdin's Castle", the developer's own track, 192 kbps) on every page. The track is not covered by the repository's software or model licences.

- **Loading:** the 7 MB file is not fetched until the visitor presses Play, clicks, or presses Enter or Space. Browsers block sound before a gesture, and Tab and other keys never start it.
- **Playback:** it plays from 0:00, since the track fades itself in, and loops.
- **Seeking:** Cloudflare Pages ignores range requests, so the track is fetched whole and played from a local blob, which is what makes seeking work.
- **Retries:** a failed fetch keeps the bar, and Play retries it.
- **Controls:** the bar has:
  - play/pause, and stop (back to 0:00);
  - the title, which becomes a seek bar on hover (on phones the seek bar takes its place);
  - the position;
  - a volume knob that starts at −6 dB. Drag up or down, scroll, or use the arrow keys; double-click resets it. The level is kept in `localStorage`.
- **Across pages:** the position carries across pages in the same tab, through `sessionStorage`. A stop or pause keeps it quiet until the visitor presses Play.
- **Muting:** `?mute` cuts the output after the EQ's analyser, so the EQ still moves during silent tests.

## Performance and accessibility

- **Library:** Three.js 0.160.0 comes from jsDelivr through the import map. It is the one library still fetched from a third party.
- **Loading:** models stream in after the first frame and per stop (`whenNear`), so the page never waits on them.
- **Quality governor:** it holds about 60 fps. When frames run long it steps down resolution, then bloom, and never steps back up within a visit.
- **Idle and hidden:** the loop and attract screens stop while the tab is hidden or the canvas is off screen. They drop to 30 fps after 3 s without input, or 2 fps under `prefers-reduced-motion`.
- **Reduced motion:** turns off head bob, turntables and idle animation; screens draw a still.
- **Without WebGL or JavaScript:** the panels stay readable as a plain page.
- **Keyboard:** the arrow keys and `p`/`n` move between stops, and the music knob is a slider.

## Cache tags

Browsers cache CSS and JS for an hour (`_headers`), so every changed file needs a new `?v=` tag on its links. Otherwise visitors get a mix of old and new files. Use the date with a letter, e.g. `20261027a`.

| When you change | Bump |
|---|---|
| `style.css` | `style.css?v=` in every HTML page and in `tools/games_head.html`, then regenerate `games.html` |
| `flow.css` | `flow.css?v=` in the flow pages and `tools/games_head.html` |
| `arcade.js` | `arcade.js?v=` in `index.html`, twice: the `modulepreload` link and the script tag |
| `music.js`, `page.js`, `bench.js`, `ascii-deco.js` | Their tags in the pages that load them |
| `models/` or `models.js` | `VER` in `models.js` **and** the `models.js?v=` imports in `arcade.js` and `bench.js`. Keep those two imports identical, or the page loads two copies of the module |

The files inside `ascii/` import each other untagged, since they are upstream files. `_headers` makes browsers revalidate them instead.

## Testing a change

ES modules need a web server; `file://` will not work. From the repo root:

```bash
python -m http.server 8777 --bind 127.0.0.1 --directory website
```

Then open http://127.0.0.1:8777/?mute.
- The server caches pages, so add a throwaway query (`&r=2`) after editing HTML.
- `#stop-3` deep-links to a stop.
- `?debug` exposes `window.__acvr` (renderer, scene, bloom, pixel ratio, walk progress) and `window.__music`.

Check at least these sizes. Browser zoom changes the CSS width, so they double as zoom checks:

| Size | Stands for |
|---|---|
| 3840×1900 | 50% zoom on 1920 |
| 1920×950 | 100% |
| 1280×800 | laptop / 150% zoom |
| 960×475 | 200% zoom |
| 375×812 | phone |

At each size:
- There is no horizontal scroll. Check `document.documentElement.scrollWidth <= innerWidth`.
- The panels sit in 2/3 on alternating sides.
- Each stop's exhibit is in the open third. Use the stop dots to jump.
- The music bar does not cover content you need to reach.

Also open `games.html`, `how-it-works.html` and `404.html`, and check the console for errors.

## Publishing

There is no build step, but deploy a **staged copy**, not `website/` itself. Pages publishes every file in the folder it is given, which would put `tools/` and every `.md` on the public site. `tools/stage_deploy.py` copies the site without them and checks that nothing slipped through.

```bash
python tools/check_public_privacy.py
python website/tools/stage_deploy.py <new folder outside website/>
wrangler pages deploy <that folder> --project-name ac-vr-hub --branch preview
```

On the developer's PC, run wrangler with `CLOUDFLARE_API_TOKEN` unset so it uses the logged-in account.

Before you deploy:
- Deploy the `preview` branch only from `main`, or from a branch that already contains every open website change. Each deploy replaces the whole site, so deploying a branch that lacks another PR's content removes that content from the preview.
- Deploy a work-in-progress branch to its own alias instead, e.g. `--branch claude-my-change`.

`_headers` sets caching: models and audio a day, CSS and JS an hour, fonts a week, `/ascii/*` revalidated. `404.html` answers missing paths with a real 404.

Licences and external hosts are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) (published as `notices.html`) and `privacy.html`. The fonts are Cinzel Decorative, Press Start 2P and Space Grotesk, all OFL. They are served from `fonts/` with their licence texts beside them.
