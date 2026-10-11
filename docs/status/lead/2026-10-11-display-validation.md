# Both-eye validation — 2026-10-11

The runtime previously validated the right-eye draw record and required depth only after drawing the left eye. It now validates and stamps both records before either backend or gun draw. Error codes, frame ownership, zero-layer failure cleanup and the public ABI are unchanged.

The new right-eye regression failed on the original runtime. Eight malformed-record/depth cases and a valid control now pass, as does the configured-gun case that requires zero world and gun draws on a malformed right record. All eight local CPU CTests passed; independent review reran 8,448 runtime checks and found no actionable issue. Tests use synthetic records with XR and GL disabled.

This does not preflight backend resources or prevent a backend from failing during its second draw. The optional whole-display backend callback and borrowed-context adapter remain separate work. Hosted checks must pass before integration; no GPU, game or headset acceptance is claimed.
