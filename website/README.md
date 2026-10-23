# Project website

The public site for AC VR Hub. It lives here, in `website/`, and is updated as the project progresses: pages are `index.html` (the arcade walk), `how-it-works.html` and `games.html`.

The arcade page is a scroll-driven walk from a mall corridor into an arcade, built with Three.js (loaded from jsDelivr, pinned to 0.160.0). The arcade is the backdrop: each section is a grid of info panels over a dimmed scene, and the camera walks to the matching spot as you scroll. Nine sections: welcome (the sign animates ARCADE to AC VR HUB), how you play, gun models, ghost controls, arcade boards, consoles, open source, the Hub, and where it stands.

The guns, grip hands and ghost controls in the scene, and the interactive model bench, are the project's own CC0 models: `tools/sync_models.py` copies them from `assets/` into `models/` with their tints and moving parts from `data/guns/`. The room itself (cabinets, sign, carpet, neon) is generated in code in `arcade.js`, with no photos, logos or third-party art ([legal.md](../docs/legal.md)).

## Music

`music.js` plays `audio/l-von-theme.mp3` (L-VON, "Theme From Aladdin's Castle", the developer's own track, encoded at 192 kbps) on every page. A first visit starts 45 seconds in with a 6-second fade once the visitor clicks or presses a key (browsers block sound before that); the track then loops from 0:00. The bar at the bottom has play/pause, stop (back to 0:45), the title (it turns into a seek bar on hover; on phones the seek bar shows in its place), the position and a volume knob that starts at -6 dB (drag up or down, scroll, or arrow keys; double-click resets). The position carries across pages in the same tab, and a stop or pause keeps it quiet until the visitor presses Play. Cloudflare Pages ignores range requests, so the track is fetched whole and played from a local copy, which is what lets it seek. The track is not covered by the repository's software or model licences. Add `?mute` to any page for silent test runs.

## Keeping it current

- Models: `python website/tools/sync_models.py` after the gun, hand or control assets change.
- Game List: edit `SYSTEMS` in `website/tools/make_games.py` from `docs/status/current.md` and the Hub's game tables, then `python website/tools/make_games.py website`.
- Arcade and How It Works text: `index.html` and `how-it-works.html`, from the same status page. Never claim more than the repo does: E3 only where `current.md` says the owner accepted it.
- Bump the `?v=` tag on the CSS and JS links when those files change, so browsers do not mix old and new files. The models carry their own tag: `VER` in `models.js` and the `models.js?v=` imports in `arcade.js` and `bench.js` (keep those two imports identical, or the page loads two copies of the module).

## Preview

ES modules need a web server, so `file://` will not work:

```bash
python -m http.server 8765 --bind 127.0.0.1 --directory website
```

Then open http://127.0.0.1:8765. Add `?debug` to see draw-call counts in `window.__acvr`. Deep links such as `#stop-3` land on that stop.

## Controls

Scroll, the stop dots on the right, or the arrow keys walk the path. Mouse drag looks around. Clicking a cabinet walks to its stop. `prefers-reduced-motion` turns off head bob and idle animation. Without WebGL or JavaScript the panels stay readable as a plain page; the model bench starts only when it scrolls into view.

## Editing

- Panel text: `index.html`, one `section.stop` per camera stop (sections and `STOPS` must stay the same length).
- Camera stops: `STOPS` and `PATH` in `arcade.js`. Each stop has a camera position and a look target. `PATH` adds waypoints that keep the walk clear of cabinets.
- Status board in the scene: `screenBoard` in `arcade.js`, from [current.md](../docs/status/current.md).

## Publishing

The preview is the Cloudflare Pages project `ac-vr-hub`, branch `preview` (https://preview.ac-vr-hub.pages.dev). Deploy the folder as it is; there is no build step. `_headers` sets caching (models a day, CSS and JS an hour) and `404.html` answers missing paths with a real 404:

```bash
wrangler pages deploy website --project-name ac-vr-hub --branch preview
```

Run `tools/check_public_privacy.py` before pushing. When the Game List or How It Works statuses change in the repo (setup READMEs, lead notes), update the matching cards so the site never claims more than the repo does.
