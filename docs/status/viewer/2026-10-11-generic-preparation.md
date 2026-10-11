# Viewer: explicit disc and per-game profile preparation

Generic private recipes now select a six-character Wii `game_id`, generated
`game_slug` and exact canonical DSU profile/hash. A fresh User contains only that
game's selector; wrong IDs, profile slugs, hashes or native input roles fail
before output creation. Old recipes without an ID retain the historical pair,
and all old profile/receipt/calibration identifiers remain unchanged. There is
no embedded owner-title list, collection scan or filename-based title inference.

The bounded reader checks ISO or single-disc WBFS Wii headers. WBFS copies are
read after the validated host sector, rather than assuming offset 512. Magic,
ID, declared metadata bounds and single-slot rules match the sitting contract.
The hash covers exactly 32 selected header bytes, not payload/block-map integrity.
Prepared receipts retain input/config fingerprints and reject subsequent changes.

Twelve new synthetic tests pass, covering ISO/WBFS offsets, malformed/multi-disc
containers, wrong slugs/roles, same-size changed headers, changed config bytes,
selected-game manifests and both DSU profile roles. The first full local run
had three existing tests fail at the known intermittent Windows staging rename;
two passed their targeted retry, while one still failed. A benign rename check
passed, cause remains unproven, and no production retry workaround was added.
The full local suite is therefore not claimed green; hosted checks remain a gate.

No actual disc/library data was read, no game/desktop/headset run
occurred, and no collection, install, system/runtime or Steam change was made.
PR49's safety-only SBS proof and PR52's identity consumer remain separate frozen
checkpoints. Menu/mission, depth, physical input and headset acceptance stay open.
