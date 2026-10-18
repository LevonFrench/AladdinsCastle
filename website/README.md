# Project website

The public site for AC VR Hub. It lives here, in `website/`, and is updated as the project progresses: pages are `index.html` (the arcade walk), `how-it-works.html` and `games.html`.

A scroll-driven walk from a mall corridor into an arcade, built with Three.js (loaded from jsDelivr, pinned to 0.160.0). Ten stops, one per card in `index.html`: mall (the sign animates ARCADE to AC VR HUB), motion controls, the gun model, racing ghost controls, Model 3, Wii, PS2, prize counter, the Hub and the roadmap scoreboard. Exhibits spread out on wide screens and gather on narrow ones (`exhibitHolder` / `applyLayout` in `arcade.js`).

Everything in the scene is generated in code (`arcade.js`): boxes, canvas textures and emissive neon. No photos, logos or third-party art ([legal.md](../docs/legal.md)). The storefront takes its look from 1990s mall arcades: red sign band with a checker border, teal-and-white checker pillars, tile floor, geometric murals and neon carpet.

## Preview

ES modules need a web server, so `file://` will not work:

```bash
python -m http.server 8765 --bind 127.0.0.1 --directory website
```

Then open http://127.0.0.1:8765. Deep links such as `#stop-3` land on that stop.

## Controls

Scroll, the stop dots on the right, or the arrow keys walk the path. Mouse drag looks around. Clicking a cabinet walks to its stop. `prefers-reduced-motion` turns off head bob and idle animation. Without WebGL or JavaScript the cards stay readable as a plain page.

## Editing

- Card text: `index.html`. Keep it in step with the README status.
- Camera stops: `STOPS` and `PATH` in `arcade.js`. Each stop has a camera position and a look target. `PATH` adds waypoints that keep the walk clear of cabinets.
- Roadmap board: `screenBoard` in `arcade.js`, from [roadmap.md](../docs/roadmap.md).

## Publishing

The preview is the Cloudflare Pages project `ac-vr-hub`, branch `preview` (https://preview.ac-vr-hub.pages.dev). Deploy the folder as it is; there is no build step:

```bash
wrangler pages deploy website --project-name ac-vr-hub --branch preview
```

Run `tools/check_public_privacy.py` before pushing. When the Game List or How It Works statuses change in the repo (setup READMEs, lead notes), update the matching cards so the site never claims more than the repo does.
