# Game catalog and routes

Every game gets the best route available: **True 3D** (ACBP backend or an existing VR port), **Hand-off** (third-party VR app), or **Theatre** (virtual screen). Full lists with emulator status are in [`data/`](data/):

- [Arcade 3D light gun games](data/arcade-gun-games.md): about 60 titles, with MAME set, status flags, TeknoParrot profile and DemulShooter support.
- [Console light gun games](data/console-gun-games.md): PS1/PS2/PS3, Saturn, Dreamcast, Wii, Xbox, Switch, PC.
- [Arcade 3D racing games](data/arcade-racing-games.md): about 60 titles, with MAME, Supermodel, TeknoParrot and decomp routes.
- [Emulator light gun input hooks](data/emulator-gun-input.md): how each emulator accepts gun position, from source.

Status as of 2026-10-08. `?` = unverified.

## Light gun: true-3D routes

| Game | Original hardware | True-3D route | Status |
|---|---|---|---|
| Time Crisis (1995) | Namco Super System 22 | DR-89 Time Crisis VR (hand-off), then **namco22-vr** (ours) | Playable in VR today (experimental) |
| Virtua Cop 2 (1995) | Sega Model 2 | VC2VR on the 1997 PC port (hand-off) | Playable in VR today (beta) |
| Virtua Cop (1994) | Sega Model 2 | VC2VR-style renderer intercept on the PC port | Research |
| The House of the Dead 1/2/3 | Model 2 / NAOMI / Chihiro | Renderer intercept on the official PC ports, VC2VR-style | Research |
| The Lost World, Ocean Hunter | Sega Model 3 | **supermodel-vr** (ours) | Research. Both NOT_WORKING in MAME; check Supermodel status |
| Time Crisis 2/3, Crisis Zone, Vampire Night, Ninja Assault, Virtua Cop Elite Edition (PS2) | PS2 / System 246 | **pcsx2-vr** (PenguinScreen2 approach + GunCon 2 aim) | Research |
| House of the Dead 4 / EX, Let's Go Jungle, Rambo, Ghost Squad Evolution | Sega Lindbergh (x86 Linux) | lindbergh-loader GL interposer | Research |
| Time Crisis 4, Razing Storm, Deadstorm Pirates (PS3 versions) | PS3 / System 357 | RPCS3 (GunCon 3 emulated); stereo seam unknown | Theatre first |

## Racing: true-3D routes

| Game | Original hardware | True-3D route | Controls (ghost set) |
|---|---|---|---|
| **Rave Racer** (1995) | Namco System 22 | **namco22-vr**, playable natively upstream (online up to 8) | Wheel, 2-position shifter, pedals |
| **Ace Driver** (1994) | Namco System 22 | **namco22-vr** (force feedback upstream) | Wheel, shifter, pedals |
| **Dirt Dash** (1995) | Namco System 22 | **namco22-vr** | Wheel, shifter?, pedals |
| **Prop Cycle** (1996) | Namco Super System 22 | **namco22-vr** (playable start to finish upstream) | Bicycle: `pump` pedalling + handlebars |
| Tokyo Wars (1996) | Namco Super System 22 | namco22-vr (not a racer: a tank battle) | Tank controls |
| Scud Race, Daytona USA 2, Sega Rally 2, Le Mans 24, Harley-Davidson, Dirt Devils, Emergency Call Ambulance | Sega Model 3 | **supermodel-vr** | Wheel / handlebars, shifter, pedals |
| Daytona USA, Sega Rally, Virtua Racing, Indy 500, Manx TT | Sega Model 1/2 | Research: PC ports, Daytona XBLA recomp, MAME TGP path | Wheel, 4-speed H / shifter, pedals; Manx TT: bike + lean |
| OutRun 2 / 2006 | Chihiro / PC | OutRun2006Tweaks hook or decomp (research) | Wheel, 2-position shifter, pedals |
| Initial D, Sega Race TV, Hummer, OutRun 2 SP SDX | Lindbergh | lindbergh-loader GL interposer (research) | Wheel, sequential shifter, pedals |
| Ridge Racer 1/2, Alpine Racer 1/2, Cyber Cycles | Namco System 22 | Not in namco22-decompile yet; MAME theatre. Upstream may add them. | Wheel; skis (`lean_body`); bike |

## Everything else: theatre

Model 2 Emulator, MAME, TeknoParrot (Lindbergh, RingEdge, Type X, ES3), Flycast, Dolphin, RPCS3 and DuckStation games run on a virtual screen with motion-controller aim through the input bridge (see [architecture.md](architecture.md) §4) until a stereo route exists.

## First slice

1. **Time Crisis**: hand-off to DR-89 first, then namco22-vr.
2. **Rave Racer**: namco22-vr with ghost wheel, shifter and trigger pedals.

Both run on the same MIT-licensed engine and share a projection model, so one backend covers both the gun and the racing pillar.
