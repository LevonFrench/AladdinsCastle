# Desktop gun-room launch contract

The desktop Help menu opens a gun-room page. The operator selects an authored
gun-model ID, explicitly selects the primary left or right hand, and selects the
reviewed executable. An existing loader can be selected for this child only;
otherwise the native provider's configured/system loader route is used. The Hub
does not install a loader, alter the active XR runtime or configure Steam.

Admission requires Windows, an absolute local data root, the canonical product
name, a model present in the hand assembly with both primary selectors, its
local gun descriptor/asset, and existing absolute executable/loader paths.
Linked ancestors and linked receipts are refused. Metadata reads are bounded;
gun meshes are not decoded by this consumer. The provider owns placement and
full asset/node validation.

The executable must have a sibling `<executable>.launch.json` with schema
`gun-room-launch/1`, `control_stdin: true`, a lowercase 40-character
`source_commit` and a lowercase SHA-256 matching the executable bytes. This
local build receipt is supplied by the native lane; the Hub never generates or
modifies it. It prevents accidental admission of an older binary without the
close contract. It is not hardware acceptance. Click and restart revalidate it.

The retained child receives separate arguments:
`--root <root> --xr --model <id> --hand <left|right> --control-stdin`.
There is no shell, capture request, angle-file write or game/backend claim.
Visible Hub and room names consume the canonical product definition.

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
