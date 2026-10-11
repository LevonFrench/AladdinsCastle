# Viewer checkpoint: opt-in WGC failure diagnostics

Source base: reviewed next `d3e739a`. Isolated branch
`codex/m2-wgc-failure-diagnostic`. Scope: theatre helper and this lane report.

Capture-only runs can explicitly request a fresh private failure directory.
Failures record parsed bounds and selection/capture-start/first-candidate/
compare-change stage, selected HWND/PID and title-match boolean, client/source/
crop dimensions, frame-pool delivery/recreation/readback counters, unchanged
spatial statistics, RGB comparison outcomes and compositor timestamps. No actual
window titles or operator/library details appear in this report.

At most two immutable selected-client samples are retained: first candidate
(first raw if no candidate exists) and latest raw. They export under distinct
failure-only names with accepted=false metadata. Fresh-directory reservation,
no linked ancestors, directory locking and exclusive file creation protect
previous artifacts. Metadata precedes image export; partial export is still a
failed diagnostic, never accepted proof. The original failure exits 1.

Existing Dolphin/PID/title/window admission, no retarget/fallback, whole-image
candidate threshold, alpha/border exclusion, one-second minimum, RGB-change gate
and accepted PNG/receipt generation are preserved. No per-eye/game/mission/
headset acceptance follows from these diagnostics.

Authored deterministic CPU coverage: stage/counters, no-frame versus recreation/
readback failure, exact spatial threshold boundary, alpha/border parity, unchanged
RGB, stalled/regressing timestamps, immutable bounded retention and independent
JSON parsing. Extended software PNG round-trip coverage checks failure exports
and exclusive-write rejection without changing existing proof images. Relative
diagnostic directories have a pre-admission rejection check.

Verified at source checkpoint: whitespace check, Python wrapper syntax and nine
privacy regressions pass. Pinned-base source comparison also confirms the
accepted PNG writer/receipt block and spatial predicate are preserved.

After the lead released the CPU slot, the first local build stalled before
target compilation and only its verified owned build tree was stopped.
Hosted compilation independently diagnosed a duplicate WinRT catch and encoder
parameter shadowing under /WX; review also identified acquisition exceptions
miscounted as empty polls. A combined repair preserves original HRESULT text/
exit 1, uses distinct acquisition/readback failure states, and renames parameters.
Its elevated build compiled the production helper and software readback target,
then the deterministic fixture's always-throwing mock triggered C4702 under /WX.
The mock now has a runtime-conditioned failure/return path. The lead explicitly
authorized focused continuation at source `2f4558a`; Release compilation passes
with /W4 /WX, two workers and node reuse disabled. All 11 theatre headless tests
pass in 0.33 seconds, including the deterministic diagnostic/JSON fixture and
software PNG round-trip/exclusive-write coverage. No build workers remain; the
CPU slot is released to the lead. Both failed attempts are retained privately.

No game/helper capture, SteamVR, headset, input takeover, media read or system
change in this lane. Runtime diagnostics and the one retry remain unverified
and require parent independent review/scheduling. Warning policy is unchanged.
