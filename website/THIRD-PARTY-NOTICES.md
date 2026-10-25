# Website notices

The website source is project code under GPL-3.0-only. Its original model copies
are CC0 under the source asset notices. `tools/sync_models.py` excludes the
CC BY 4.0 arc-light-gun; that exception is documented in the repository's root
notice and must be retained if the model is added here later.

## Three.js r160

The arcade page (`index.html`) loads Three.js and addons from jsDelivr at version 0.160.0. It is
the one library still fetched from a third-party host (see the [privacy page](privacy.html));
no copy of r160 is vendored in this repository. Upstream:
https://github.com/mrdoob/three.js/tree/r160 . Full upstream MIT notice:

```text
The MIT License

Copyright © 2010-2023 three.js authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## ascii-magic-art

The animated ASCII on the cabinet screens, the flow-page headers and dividers and
the 404 comes from ascii-magic-art (https://github.com/kailashsr38/ascii-magic-art).
`tools/sync_ascii.py` copies its runtime, helper kit and only the listed pieces into
`ascii/`, with the licence in `ascii/LICENSE`. The two photo-based pieces that keep
the Unsplash License (ocean-sunset, starry-ridge) are never copied.

Upstream provenance, as far as the repository shows it: the files were copied once, in
commit 45452d5 (2026-10-22), from a local checkout of the repository above. The checkout's
commit and the package version were not recorded then, and nothing in the copied files
states a version, so no version number is claimed here. From now on `tools/sync_ascii.py`
writes the upstream commit to `ascii/UPSTREAM.txt` when the checkout is a git repository.
The files are unmodified upstream paths: `runtime/{ascii,player,glyphs,canvas}.js`,
`lib/kit.js`, and these 21 pieces under `pieces/`: alien-wave, block-drop, brick-breaker,
circuit-lap-racer, circuit-trace, coin-run-platformer, dino-runner, frog-hop-crossing,
gem-cascade, maze-chase, paddle-tennis, pipe-flyer, pulse-line, rainbow-warp-road,
rock-blaster, snake-autoplay, stars-divider, synthwave-grid, tv-static, vector-space-duel
and wave-divider. Full upstream MIT notice:

```text
MIT License

Copyright (c) 2026 ASCII Magic (ascii-magic.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Fonts (self-hosted)

The pages serve their three font families from `fonts/`; no font provider is contacted.
All three are under the SIL Open Font License 1.1, whose complete text, with each
copyright line, is kept beside the files (`fonts/OFL-*.txt`):

| Family | Files | Version | Upstream |
| --- | --- | --- | --- |
| Cinzel Decorative (Bold, Black) | `CinzelDecorative-Bold.woff2`, `CinzelDecorative-Black.woff2` | 1.002 | https://github.com/google/fonts/tree/main/ofl/cinzeldecorative |
| Press Start 2P | `PressStart2P-Regular.woff2` | 3.000 | https://github.com/google/fonts/tree/main/ofl/pressstart2p |
| Space Grotesk (variable, 300 to 700) | `SpaceGrotesk-Variable.woff2` | 2.000 | https://github.com/google/fonts/tree/main/ofl/spacegrotesk |

The woff2 files are the Google Fonts distribution TTFs, recompressed to woff2 with
fontTools and otherwise unchanged (no glyphs removed, no renaming), so the Reserved Font
Names are not misused. They were copied from another local project that vendors the same
Google Fonts files with their OFL texts, not downloaded for this site.

## Music

`audio/l-von-theme.mp3` is credited to L-VON as "Theme From Aladdin's Castle", the
developer's track, in `website/README.md` and in the music bar. The repository's root
README does not mention it, and the file carries no embedded credit. It is not covered by the
software GPL or model CC0 licences. This notice grants no additional rights to
redistribute the recording. A new repository or independently published website
must document the rights holder's intended music permission before copying it.
