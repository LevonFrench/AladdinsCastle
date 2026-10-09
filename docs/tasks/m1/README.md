# M1 execution plan (for Codex)

**Milestone M1:** Hub skeleton + third-party setups ([roadmap](../../roadmap.md)).
**Done when:** on a clean Windows PC, the Hub installs **DR-89 Time Crisis VR (PCVR)** and **VC2VR**, and both launch from the Hub and from the SteamVR library. Quest sideloading is **out of M1**.

Codex executes the lanes below. Each lane has its own brief, branch and worktree, and ends in a pull request.

## Ground rules (all lanes)

1. **Read first:** [README](../../../README.md), [workshop.md](../../workshop.md) (decisions), [hub-architecture.md](../../hub-architecture.md), plus the docs named in each brief. Specs win over guesses; when a spec is ambiguous, pick the simplest option, write it down in the PR, and carry on.
2. **One lane = one branch + one worktree:**
   `git -C J:/projects/games/aladdinscastle worktree add J:/projects/games/aladdinscastle-m1-<lane> -b m1/<lane> main`
   Rebase on `main` before opening the PR. Never push to `main` directly.
3. **Privacy:** the owner's local paths (ROM folders, emulator installs, Steam) are in `J:\projects\games\aladdinscastle\AGENCY.md` (gitignored, main checkout only). Never commit them, never commit ROM/disc file names from the owner's library, never commit game content. Local test output goes to `.local/` (gitignored).
4. **Licences:** our code is GPL-3.0; `libacvr/` is MIT. Third-party code only as dependencies with compatible licences (toml++ MIT, nlohmann/json MIT, json-schema-validator MIT, OpenVR BSD-3, Qt LGPL-3.0 dynamically linked). Never copy DuckStation (CC BY-NC-ND) or Cannonball (non-commercial) code.
5. **System changes need the owner's OK:** installing toolchains, writing Steam's `shortcuts.vdf`, changing the OpenXR runtime. Ask first; say exactly what will change.
6. **Tests:** every lane adds tests that run in CI (lane A sets CI up). No test may need game content: use synthetic fixtures.
7. **Report:** each PR description lists what was done, what was verified and how, deviations from the spec, and open questions.

## Execution defaults (locked for M1)

These close the open build decisions with the recommended option. The owner can override any of them.

| ID | Default |
|---|---|
| D6 | Each VR setup is one process (engine + libacvr); the Hub launches it |
| D23 | One executable `aladdinscastle-hub` with modes: desktop (default), `--overlay`, `--launch <game-id>` (headless pre-flight + launch) |
| D24 | Hub renders with OpenGL (overlay texture = GL texture) |
| D25 | Qt **6.8 LTS**, re-checked after spike S1 |
| D29 | All Hub state in the portable folder (`user/`), not `%LOCALAPPDATA%` |
| D31 | UI tokens loaded in C++ from `docs/ui/theme.toml` (copied to `hub/resources/`) |
| D32 | Python `tools/validate_catalog.py` in CI; the C++ loader validates in warn mode |
| D33 | Ship the VC++ runtime DLLs in the portable folder |
| D35 | Steam shortcuts target the Hub: `aladdinscastle-hub --launch <game-id>` |
| D40 | Art: local only (folders beside the ROMs, RetroArch thumbnails, PCSX2 covers, `user/art`), then generated fallback |

## Lanes and order

```
A toolchain + skeleton + CI ──┬─► B spike: SteamVR overlay (S1-S3)
                              ├─► C catalog core + models ──┬─► D QML UI
                              │                             └─► F scanner + art resolver
                              ├─► E install engine (needs C's loader for recipes)
                              └─► G launch + Steam shortcuts (needs C, E for state)
Integration: D + E + F + G on main → M1 acceptance run on the owner's PC
```

| Lane | Brief | Depends on | Branch |
|---|---|---|---|
| A | [A-toolchain-skeleton.md](A-toolchain-skeleton.md) | none | `m1/a-skeleton` |
| B | [B-spike-overlay.md](B-spike-overlay.md) | A | `m1/b-overlay-spike` |
| C | [C-catalog-core.md](C-catalog-core.md) | A | `m1/c-catalog` |
| D | [D-qml-ui.md](D-qml-ui.md) | C | `m1/d-ui` |
| E | [E-install-engine.md](E-install-engine.md) | A, C | `m1/e-install` |
| F | [F-scanner-art.md](F-scanner-art.md) | C | `m1/f-scanner` |
| G | [G-launch-steam.md](G-launch-steam.md) | C, E | `m1/g-launch` |

B, C and (after C) D, E, F can run in parallel. G comes last.

## M1 acceptance (owner's PC, after all lanes merge)

1. A fresh portable build starts; the library shows 413 games with fallback or local art, all filters work, and the grid stays smooth (S2 numbers recorded).
2. "Find my files" points at the owner's folders. Time Crisis and Virtua Cop 2 PC turn **Ready**, matched by hash or exe.
3. Install Time Crisis VR (PCVR) via `locate-package` from a ROM-free build, and VC2VR via `github-release`. Both reach **Installed** with verify passing; uninstall leaves nothing behind (journal).
4. Start in VR from the Hub (SteamVR via ALVR): both run, and the Hub regains focus on exit.
5. With Steam closed, add the SteamVR library entries; both appear with art and launch from the headset through `--launch`.
6. The Hub shows as a SteamVR dashboard tab (`--overlay`), clickable with the laser (from spike B; full overlay polish is M2).
