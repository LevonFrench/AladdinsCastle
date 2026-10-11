# Desktop gun-room launch contract

The desktop Help menu opens a gun-room page. The operator selects an authored
gun-model ID, explicitly selects the primary left or right hand, and selects the
reviewed executable. An existing loader can be selected for this child only;
otherwise the native provider's configured/system loader route is used. The Hub
does not install a loader, alter the active XR runtime or configure Steam.

Admission requires Windows, an absolute local data root, the canonical product
name, a model whose complete hand/mount metadata passes the shared native CPU
decoder (including safe asset paths, supported nodes and normalized rotations), its
local gun descriptor/asset, and existing absolute executable/loader paths.
Linked ancestors and linked receipts are refused. Metadata reads are bounded;
at most 128 model entries are considered. Gun meshes are not decoded by this consumer. The provider owns placement and
full asset/node validation.

The executable must have a sibling `<executable>.launch.json` with schema
`gun-room-launch/1`, `control_stdin: true`, a lowercase 40-character
`source_commit` and a lowercase SHA-256 matching the executable bytes. This
local build receipt is supplied by the native lane; the Hub never generates or
modifies it. It prevents accidental admission of an older binary without the
close contract. It is not hardware acceptance. Click and restart revalidate it.

An optional literal JSON boolean `calibrate_grip: true` admits the separate
**Calibrate grip in VR** action after the same identity, metadata, path and
executable-hash checks. Missing, false, string, numeric or container values keep
normal launch available and do not expose calibration. The native lane supplies
this capability only for its matching reviewed build; selecting it does not
replace the historical pinned executable or change its receipt.

The retained child receives separate arguments:
`--root <root> --xr --model <id> --hand <left|right> --control-stdin`.
There is no shell, capture request, angle-file write or game/backend claim.
Visible Hub and room names consume the canonical product definition.
Calibration adds only `--calibrate-grip`; it keeps the explicitly selected model,
hand, executable, loader and owned stdin pipe. The retained card distinguishes
the action, and restart revalidates both its capability and binary bytes rather
than substituting a normal launch or using later form edits.

The native room owns calibration editing and persistence. Keep the trigger
released, start with B (right) or Y (left) released, then hold it with a centred
stick for 0.8 seconds and release to edit. Stick X adjusts yaw; Y adjusts pitch.
Hold with the stick centred for 1.2 seconds and release to save an actual change;
a short tap cancels. Focus, tracking or required-action loss cancels; unsupported
interaction profiles cannot save. Native room exit cancels unsaved changes.
Saved per-model/per-hand calibration is applied by later normal native launches.
The Hub neither writes calibration files nor simulates a successful save.

Exit writes exactly `close\n` to the owned managed stdin pipe, then schedules
EOF. [Qt's write-channel contract](https://doc.qt.io/qt-6/qprocess.html#closeWriteChannel)
drains queued bytes before closing. The native loop polls this pipe between XR
ticks and destroys the runtime before normal exit. The shared card remains
closing until QProcess reports exit. Restart retains the same model/hand and
is offered only after a successful close. Recentre remains unavailable. Hub
shutdown cancels pending restart and waits for the child; even exceptional
teardown retains it through orderly exit rather than invoking QProcess's
destructive live-child teardown. A blocked XR call can keep shutdown pending;
there is no forced-stop timer or false exit report.

CPU tests use generated metadata and our own synthetic pipe child. The actual
QML scene test exercises desktop navigation, explicit hand selection and the
shared card with an inert service probe. No test launches the native room or
loads OpenXR. Source/CPU and software-scene evidence remain separate from
actual-eye, tracked-hand, input-response and owner-headset acceptance.
