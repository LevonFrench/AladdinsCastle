# Hub grip calibration — source/software checkpoint

Separate follow-up to consumer PR66. The desktop action requires literal JSON
`calibrate_grip: true` in the native lane's matching `gun-room-launch/1` receipt,
after existing path, metadata, identity and executable SHA-256 checks. Older,
missing, false and wrongly typed capabilities keep normal launch and hide the
calibration action. Calibration adds only `--calibrate-grip`; it retains explicit
model/hand, executable/loader and owned stdin. Restart retains the action and
revalidates capability and bytes, ignoring later form edits.

The page explains native edit/save/cancel controls. Native code owns calibration
persistence; Hub adds no calibration-file write or simulated save. Closing the
room preserves the existing cooperative close/EOF and confirmed-exit behavior.
The historical frozen executable and receipt are unchanged.

Verified: capability admission and missing QML action each failed first against
the baseline. The strict MSVC build initially failed in Qt-generated QVariant
instruction-text conversion (C4702); explicit B/right and Y/left text passed the
single diagnosed retry with strict warnings preserved. Nine core/service cases
(11 Qt phases, 1136ms), full strict-warning real-QML software suite (25.69s) and
full integration suite (27.94s) passed. QML checks click the actual calibration
control and shared restart, retaining the original hand/action after form edits.

Heavy slot released. Independent source review closed checkpoint `541b6be`.
The accepted consumer report correction is inherited; the unchanged public
privacy policy passes, with all reviewed Hub source/test bytes preserved.
Hosted verification is pending;
no native room, XR, GPU, physical input, calibration save or headset acceptance
was executed. QML uses an inert service probe; actual HubServices/native runtime
and controller/save behavior remain unproved. Next: lead publication checkpoint,
native lane's matching
capability receipt, then the lead's next task. Parent owns AGENCY/KB and merges.
