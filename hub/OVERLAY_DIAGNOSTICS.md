# Overlay diagnostics and correctness

The `--spike` scene has four off-centre hit targets on its 1280 × 800 canvas,
a marker for the dispatched Qt position, and a long synthetic list with visible
`contentY`. All content is synthetic. Raw observations are grouped by cursor ID:
mouse coordinates, separate smooth/discrete counts and deltas, cached position,
Qt event/position/buttons, receipt/press timestamps, owner and fixed
rejection/cancellation reasons. Raw counts and dispatched Qt counts are separate.
Typed text never enters these rows or a log.

## Overlay-only input contract

Only a finite in-panel mouse move establishes position authority. Button packet
coordinates, even divergent/nonfinite ones, do not supply it. Raw moves outside
[0,width) × [0,height) invalidate before the diagnostic mapper clamps anything.
Each cursor owns its last valid move and fractional wheel remainder; wheel events
use that cursor's position. Both smooth and discrete factors remain ×120, with
the existing ±100 delta clamp. At most 64 admitted IDs are retained for a
translator lifetime; over-budget IDs never become a late gesture. Diagnostic
rows retain 16 IDs and count additional observations as dropped.

The first admitted down owns the entire accepted button chord. Other cursors may
refresh cached moves but cannot steer, release or scroll the held gesture.
Rejected/canceled held chords remain suppressed until their physical ups; no
press or scroll is queued for replay. Unowned ups do not generate Qt releases.

Freshness is receiver/visibility/focus epoch validity, with no elapsed-age
expiry. A stationary controller can keep a valid position indefinitely. Hide,
focus loss, receiver/orientation changes and owner/target loss invalidate it.
Inactive packets cannot establish a new valid position or activate anything;
ups still acknowledge suppressed buttons. Genuine show/focus resume (including
host visibility and Qt window transitions) plus a new move permits admission.
The host sets visibility before draining queued packets. Its external visibility
gate is separate from event visibility: queued Shown cannot override a false
host sample, and an unchanged true host sample cannot erase a queued Hidden.
A genuine host-visible transition restores admission only after a fresh move.

One injected monotonic receipt sample per input packet stamps Qt events in
milliseconds. External lifecycle samples also advance the same high-water mark.
Regressing/negative packet clocks cancel and reject input until the clock catches
up; cleanup timestamps never regress. OpenVR `eventAgeSeconds` is not treated as
an occurrence clock. Press point/time are observable snapshots, not a freeze or
slop policy. Production freeze is zero; Qt's native drag/slop behavior, desktop
routing and all scaling/filter settings remain unchanged.

## Cancellation and lifetime

The overlay uses its own Qt mouse device, separate from the native desktop
mouse. Cancellation immediately clears logical ownership, invalidates caches,
and suppresses held buttons. Public Qt point-grab cleanup precedes off-panel
release/move/leave; an ordinary release at the pressed control is never used.

QObject target/window teardown cannot receive synthetic input safely. These
paths defer Qt cleanup while invalidating logical state immediately. Cleanup
holds weak receiver/device references and a generation guard, and is flushed
before any subsequent packet/rebind. Old queued callbacks cannot affect a later
gesture. Tests cover surviving and replaced receivers, target/window destruction,
shutdown, and native desktop presses held during synthetic-device cancellation.

## Text and acceptance

Focus alone never opens SteamVR's keyboard. The explicit button asks an already
initialized host; it is disabled in desktop presentation. All 39 on-panel keys
and Done remain usable. Text exists only on screen/in process memory until reset
or process exit. The shared `OverlayKeyboard` keeps existing target/token/UTF-8
session guards. Software tests supply a fake adapter and initialize no XR runtime
or renderer graphics context.

Real-QML checks cover four corners and every key/Done under both Y settings,
independent contexts, matching/stale/wrong/closed sessions, Unicode/backspace,
two separate scroll lists, native drag, and cancellation/recovery across Button,
MouseArea, TapHandler and Flickable. They establish source/software behavior.
Actual packet ordering, orientation, readability and controller feel still need
an approved headset sitting. ×2880, nonzero freeze/slop/drag thresholds, speed,
dead-zone and One Euro filtering remain separate candidates; no saved WIP was
applied. Shared hardware instructions and acceptance belong to the lead.
