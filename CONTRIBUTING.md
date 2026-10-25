# Contributing

The project has a working development Hub and emulator integrations, but no 0.01 release. Reproducible bug reports, source-backed catalog corrections and design feedback are welcome.

- **Discuss a design doc:** open an issue that names the doc and section (e.g. `controls.md §2.1`).
- **Fix the catalog:** the tables in `docs/data/` were collected automatically and contain `?` cells and probably errors. Pull requests with a source link for each change are welcome.
- **Settle an open decision:** see [docs/workshop.md](docs/workshop.md).

## Rules

1. **No game content.** No ROMs, BIOS files, disc images, game binaries, or links to them, in code, issues or pull requests. These will be removed.
2. **No third-party art** unless its licence allows redistribution. State the licence and source.
3. **Respect upstream licences.** Don't copy code from files whose licence is incompatible with GPL-3.0 (e.g. CC-BY-NC-ND).
4. Cite sources for claims about emulator or game status. Distinguish source checks, offline tests and headset acceptance.
5. Add an SPDX header to new source files using that file's actual licence. Project-owned GPL code uses `SPDX-License-Identifier: GPL-3.0-only` with the language's comment syntax. Preserve MIT, BSD, upstream GPL and asset exceptions; a header is not permission to relicense someone else's work. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Agents (Claude Code and Codex)

AI agents work on this repository in lanes (one branch, one worktree, one deliverable). The rules are in [docs/agents/](docs/agents/README.md):

- Start from the [lane contract](docs/agents/lane-contract.md): a stated budget (rounds, workers, wall time), the files the lane owns, and what it must ask the owner.
- Run builds, renders, emulators and measurements through the [heavy-run lease](docs/agents/heavy-lease.md) (`python tools/heavy_lease.py run ...`), so only one heavy job runs on the machine at a time.
- Run `python tools/check_public_privacy.py` before every push. The optional [pre-push guard](docs/agents/pre-push.md) does it and refuses `main`.
- Push the lane branch and open a draft pull request. An agent does not merge, does not push to `main` and does not use `git push --no-verify`.
- Report what was not done and what was only checked offline (E1/E2); only the owner's headset session makes E3.
