# M1 lane D: QML UI (Mod Hub style)

**Branch:** `m1/d-ui`. **Depends on:** C. **Read:** [ui/01-shell-and-filters.md](../../ui/01-shell-and-filters.md), [ui/02-game-cards.md](../../ui/02-game-cards.md), [ui/03-detail-and-install-flow.md](../../ui/03-detail-and-install-flow.md), [ui/04-qml-components.md](../../ui/04-qml-components.md) (the build map), [ui/05-models.md](../../ui/05-models.md), [ui/theme.toml](../../ui/theme.toml). Reference look: PCVR Mods Installer Hub screenshot and source (MIT), local clone in `<repo>/_refs/PCVR-Mods-Installer-Hub/`.

## Build (in `hub/qml/`)

Follow 04-qml-components.md component by component:
- **Shell:** header (logo, title, version pill, search, menu), filter bar (genre and graphics chips, STATE, Scan my files, S/M/L, Filters drawer for manufacturer, year, hardware tree, VR, players, controls, in-library), featured banner, section headers with counts.
- **GameCard:** pill, title, coloured meta line (manufacturer · hardware · year), "by developer", blurb, badges, state-driven main button, hover enlarge 1.4× after 600 ms with the art strip, Frosted neon-outline style, S/M/L scales.
- **DetailPage:** sections in 03 §2.3 order: hero, meta, state, "What you need" checklist, variant picker, action row, per-game settings, controls table (from the control set), README (Markdown), what it installs, uninstall.
- **InstallConsole + RecoveryPanel:** console grammar from 03 (`--- [n/t] ---`, `[OK]`/`[!!]`/`[XX]`/`[..]`), driven by lane E's event stream (mock it until E lands).
- **Theme** singleton from `theme.toml`; no hard-coded colours.
- **VR overlay mode** sizing from 04 §9 (min 40 px targets, no hover-only actions) behind a property, ready for lane B's overlay.

## Performance (spike S2)

Measure with all 413 games: scroll fps, memory, delegate creation time, MultiEffect cost. Record in `docs/spikes/s2-grid.md`. Budget: 60 fps on the owner's PC while scrolling; if missed, apply the 04 fallbacks (cache layers, cheaper glow).

## Acceptance

The library looks like the Mod Hub reference (side-by-side screenshot in the PR, using fallback/local art only, no third-party art committed); all filters and sorts work; detail pages render for every game, including "No setup yet" ones; S2 numbers recorded.
