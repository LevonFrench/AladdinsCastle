# Website notices

The website source is project code under GPL-3.0-only. Its original model copies
are CC0 under the source asset notices. `tools/sync_models.py` excludes the
CC BY 4.0 arc-light-gun; that exception is documented in the repository's root
notice and must be retained if the model is added here later.

## Three.js r160

The site loads Three.js and addons from jsDelivr at version 0.160.0. Upstream:
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

## ascii-magic-art 1.0.2

The animated ASCII on the cabinet screens, the flow-page headers and dividers and
the 404 comes from ascii-magic-art (https://github.com/kailashsr38/ascii-magic-art).
`tools/sync_ascii.py` copies its runtime, helper kit and only the listed pieces into
`ascii/`, with the licence in `ascii/LICENSE`. The two photo-based pieces that keep
the Unsplash License (ocean-sunset, starry-ridge) are never copied. Full upstream
MIT notice:

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

## Remote fonts

The pages request Cinzel Decorative, Press Start 2P and Space Grotesk through
Google Fonts. They are external requests, not bundled font files. Family sources
and their SIL Open Font License notices are maintained at:

- https://github.com/google/fonts/tree/main/ofl/cinzeldecorative
- https://github.com/google/fonts/tree/main/ofl/pressstart2p
- https://github.com/google/fonts/tree/main/ofl/spacegrotesk

The served font bytes are not locked by this repository. If self-hosting them,
record the exact family revision and retain its copyright and complete OFL text.

## Music

`audio/l-von-theme.mp3` is credited in the project README to L-VON as
"Theme From Aladdin's Castle", the developer's track. It is not covered by the
software GPL or model CC0 licences. This notice grants no additional rights to
redistribute the recording. A new repository or independently published website
must document the rights holder's intended music permission before copying it.
