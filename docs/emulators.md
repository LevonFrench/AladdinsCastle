# Which emulators first

Status 2026-10-08. Ranked for **true 3D** (we can draw the game's own scene per eye) and for **theatre** (flat window plus motion-controller aim).

## Scoring

| Criterion | Why it matters |
|---|---|
| Open source + Windows | We have to add a VR renderer. Closed emulators can only be theatre. |
| Renderer seam | Is there a clean place to get the projection and draw per eye? |
| Gun **and** racing coverage | One integration should serve both pillars. |
| Games actually working | No point in VR for games that don't boot. |
| Gun input hook | Can we feed the aim point directly? |

## True-3D order

| # | Target | Platform | Light gun games | Racing games | Seam | Effort |
|---|---|---|---|---|---|---|
| **0** | **namco22-decompile** (engine, not an emulator) | Namco System 22/21 | Time Crisis | Rave Racer, Ace Driver, Dirt Dash, Prop Cycle (+ Tokyo Wars, Cyber Sled) | Host API + `ss22_prepare`/`ss22_draw` split; DR-89 already did it | **Low-medium.** Do it first. |
| **1** | **Supermodel** | Sega Model 3 | The Lost World, Ocean Hunter, L.A. Machineguns | Scud Race (+Plus), Daytona USA 2 (+Power Edition), Sega Rally 2, Le Mans 24, Harley-Davidson, Dirt Devils, Emergency Call Ambulance | OpenGL New3D: projection in `CNew3D::CalcViewport`, transforms in `InitMatrixStack`; built-in light gun axes, off-screen reload, Raw Input 2-player | **Medium.** First real emulator. |
| 2 | **PCSX2** (with PenguinScreen2's approach) | PS2 / System 246 | Time Crisis 2, 3, Crisis Zone, Vampire Night, Ninja Assault, Virtua Cop Elite Edition (+ arcade System 246 versions) | Ridge Racer V, OutRun 2006, Gran Turismo 3/4, Burnout... | GS vertex-shader disparity + camera RAM writes per game; GunCon 2 reads an absolute pointer | Medium-high; per-game profiles. PenguinScreen2 is Linux-only today. |
| 3 | **MAME** (Model 1/2 path) or **sm2-emu** | Sega Model 1/2 | Virtua Cop, Virtua Cop 2, Gunblade NY (working in MAME) | Virtua Racing, Daytona USA (working in MAME), Sega Rally | Geometrizer output → unproject reconstruction (VC2VR technique) | High; research first. Theatre right away. |
| 4 | **Dolphin** | GameCube / Wii (+ Triforce) | Ghost Squad, HotD 2&3 Return, HotD Overkill, RE Chronicles | F-Zero GX / AX, Mario Kart | Free-look camera, existing stereo output | High; the old VR fork shows it's possible. |
| 5 | **Flycast** | NAOMI / Dreamcast | HotD 2, Confidential Mission, Death Crimson OX | Crazy Taxi, F355 | Open renderer | High. |
| — | lindbergh-loader | Sega Lindbergh | HotD 4, Let's Go Jungle, Rambo, Ghost Squad Evolution | Initial D 4/5, OutRun 2 SP SDX, Sega Race TV | OpenGL interposer (Linux) | Research |
| — | RPCS3 | PS3 / System 357 | Time Crisis 4, Razing Storm, Deadstorm Pirates | — | Vulkan; unknown | Theatre only for now |
| — | Model 2 Emulator, Demul, TeknoParrot | closed | many | many | none | Theatre + DemulShooter only |

## Theatre order (flat window + aim), in parallel

1. **Supermodel**: same install serves theatre now and true 3D later.
2. **MAME**: Virtua Cop 1/2, Gunblade NY, Time Crisis, Daytona, Virtua Racing are marked working. Lua `ioport` writes give a direct aim hook.
3. **PCSX2**: GunCon 2 reads the absolute host pointer, so place the cursor from the aim ray.
4. **TeknoParrot + DemulShooter**: biggest list of later arcade gun games (Lindbergh, RingEdge, Raw Thrills), all closed source.

## Recommendation

**namco22-decompile first, then Supermodel.** Between them they cover gun and racing on two platforms, both open source, both with a clean renderer seam, both on Windows. That proves libacvr against a decompiled engine and a real emulator before taking on PS2.
