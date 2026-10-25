# Project website

The public site for AC VR Hub. It lives here, in `website/`, and is updated as the project progresses: pages are `index.html` (the arcade walk), `how-it-works.html`, `games.html`, `privacy.html` and `notices.html` (generated from `THIRD-PARTY-NOTICES.md` by `tools/make_notices.py`).

The arcade page is a scroll-driven walk from a mall corridor into an arcade, built with Three.js (loaded from jsDelivr, pinned to 0.160.0; the one library still fetched from a third party, because no r160 copy exists in this repo to vendor). The arcade is the backdrop: each section is a grid of info panels over a dimmed scene, and the camera walks to the matching spot as you scroll. Nine sections: welcome (the sign animates ARCADE to AC VR HUB), how you play, gun models, ghost controls, arcade boards, consoles, open source, the Hub, and where it stands.

The guns, grip hands and ghost controls in the scene, and the interactive model bench, are the project's own CC0 models: `tools/sync_models.py` copies them from `assets/` into `models/` with their tints and moving parts from `data/guns/`. The shared hand pose in `assets/hands/assembly.json` rests the middle finger on the trigger, so the sync lowers each pistol's hand until the index finger sits on the trigger blade, measured from the meshes. The room itself (cabinets, sign, carpet, neon) is generated in code in `arcade.js`, with no photos or logos ([legal.md](../docs/legal.md)); the only third-party art is the MIT ASCII animations described below.

## ASCII art

The cabinet attract screens in the arcade, the header art and section dividers on How It Works and the Game List, and the 404's TV static are animated ASCII pieces from [ascii-magic-art](https://github.com/kailashsr38/ascii-magic-art) (MIT; its licence is in `ascii/LICENSE`). `python website/tools/sync_ascii.py <path to an ascii-magic-art checkout>` copies the runtime and only the pieces listed in that script into `ascii/`; never the two photo pieces, which keep the Unsplash License. In the arcade, `asciiScreen()` in `arcade.js` draws a piece into a cabinet's screen canvas while the camera is near it, at most two screens per tick. On the flow pages, `ascii-deco.js` adds the dividers and sizes each `<ascii-art>` to its box. The EQ in the music bar is the site's own: eight bands of the playing track from a Web Audio analyser, drawn in block characters.

See [website licences and external dependencies](THIRD-PARTY-NOTICES.md) for Three.js, the self-hosted fonts, models and the separate music rights, and `privacy.html` for what the site collects (nothing) and which outside hosts it still contacts. The fonts (Cinzel Decorative, Press Start 2P, Space Grotesk, all OFL) are served from `fonts/` as woff2, with their licence texts beside them.

## Music

`music.js` plays `audio/l-von-theme.mp3` (L-VON, "Theme From Aladdin's Castle", the developer's own track, encoded at 192 kbps) on every page. The 7 MB file is not fetched until the visitor presses Play, clicks, or presses Enter or Space (browsers block sound before a gesture; Tab and other keys never start it), so nothing is downloaded on idle. It plays from 0:00 (the track fades itself in) and loops. If the download outlasts the gesture (iOS), the next click or Enter/Space plays it from the local copy; a failed fetch keeps the bar and Play retries. The bar at the bottom has play/pause, stop (back to 0:00), the title (it turns into a seek bar on hover; on phones the seek bar shows in its place), the position and a volume knob that starts at -6 dB (drag up or down, scroll, or arrow keys; double-click resets). The position carries across pages in the same tab, and a stop or pause keeps it quiet until the visitor presses Play. Cloudflare Pages ignores range requests, so the track is fetched whole and played from a local copy, which is what lets it seek. The track is not covered by the repository's software or model licences. Add `?mute` to any page for silent test runs (the output is cut after the EQ's analyser, so the EQ still moves).

## Keeping it current

- Models: `python website/tools/sync_models.py` after the gun, hand or control assets change.
- Game List: edit `SYSTEMS` in `website/tools/make_games.py` from `docs/status/current.md` and the Hub's game tables, then `python website/tools/make_games.py website`.
- Arcade and How It Works text: `index.html` and `how-it-works.html`, from the same status page. Never claim more than the repo does: E3 only where `current.md` says the owner accepted it.
- Bump the `?v=` tag on the CSS and JS links when those files change, so browsers do not mix old and new files. The models carry their own tag: `VER` in `models.js` and the `models.js?v=` imports in `arcade.js` and `bench.js` (keep those two imports identical, or the page loads two copies of the module). The ASCII module imports in `arcade.js` and `ascii-deco.js` carry the same tag style; the files inside `ascii/` import each other untagged (they are upstream files), which `_headers` covers.

## Preview

ES modules need a web server, so `file://` will not work:

```bash
python -m http.server 8765 --bind 127.0.0.1 --directory website
```

Then open http://127.0.0.1:8765. Add `?debug` to see draw-call counts in `window.__acvr`. Deep links such as `#stop-3` land on that stop.

## Controls

Scroll, the stop dots on the right, or the arrow keys walk the path. Mouse drag looks around. Clicking a cabinet walks to its stop. `prefers-reduced-motion` turns off head bob and idle animation. The 3D loop and the cabinet screens stop while the tab is hidden, drop to 30 fps after 3 seconds without input (2 fps under reduced motion), and the flow pages' ASCII pieces play only on screen. Without WebGL or JavaScript the panels stay readable as a plain page; the model bench starts only when it scrolls into view.

## Editing

- Panel text: `index.html`, one `section.stop` per camera stop (sections and `STOPS` must stay the same length).
- Camera stops: `STOPS` and `PATH` in `arcade.js`. Each stop has a camera position and a look target. `PATH` adds waypoints that keep the walk clear of cabinets.
- Status board in the scene: `screenBoard` in `arcade.js`, from [current.md](../docs/status/current.md).

## Publishing

The preview is the Cloudflare Pages project `ac-vr-hub`, branch `preview` (https://preview.ac-vr-hub.pages.dev). There is no build step, but deploy a staged copy, not `website/` itself: Pages publishes every file in the folder, which would put `tools/` and every `.md` on the public site. `tools/stage_deploy.py` copies the site without them. `_headers` sets caching (models a day, CSS and JS an hour, fonts a week, `/ascii/*` revalidated because upstream files import each other without a `?v=`) and `404.html` answers missing paths with a real 404:

```bash
python website/tools/stage_deploy.py <new folder outside website/>
wrangler pages deploy <that folder> --project-name ac-vr-hub --branch preview
```

Run `tools/check_public_privacy.py` before pushing. When the Game List or How It Works statuses change in the repo (setup READMEs, lead notes), update the matching cards so the site never claims more than the repo does.
