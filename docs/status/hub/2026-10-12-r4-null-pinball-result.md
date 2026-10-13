# Hub: bounded null-runtime Pinball result

The selected cached table entered the installed VPX OpenXR initialization path,
and the retained Hub scene displayed the correct now-playing card. The run did
not complete native player startup or produce a compositor eye pair. VR table
presentation and upright Hub dashboard presentation remain unproved.

## Evidence

- Producer source: `3042e7ab41ed55cb76d3ed104176e97cb414578b` (PR 86,
  already integrated). Separate compositor exporter:
  `b75f4a253225c8ad3db309d8f300448f1dbe4314` (PR 91).
- Lead assigned the graphics booking after verifying preceding consumers were
  closed. The existing null service was verified before this launch.
- One previously desktop-proved cached table was launched with the private
  VR-mode settings copy. There was no collection scan or alternate launcher.
- Native current-run logs confirm SteamVR OpenXR initialization and renderer
  creation. They stop at environment-map radiance computation; neither
  `Startup done` nor `Player closed` was observed. CPU consumption continued,
  so this is incomplete initialization, not evidence that the process was idle.
- The actual compositor exporter refused its hidden-dashboard guard with
  `Dashboard requirement hidden refused: observed visible=true.` It exited
  with code 3 and produced no eye images. Dashboard visibility is an observed
  condition; it does not establish the cause of the initialization stall.
- Hub's own window image and receipt confirm the retained now-playing title,
  running state and close/restart controls. This is UI evidence, not VR pixels.

## Closure and limits

Hub's normal exit timer requested cooperative child closure. A subsequent
ordinary close-window request was accepted, but native closure did not complete
within the lead's finite cleanup window. Only the verified owned VPX child was
then terminated. Hub observed child exit code -1, cleared its busy/now-playing
state, and exited without forced producer termination. Both owned processes
were verified absent at 14:23:08 UTC. This was an abnormal child exit, not a
successful score/settings shutdown.

The installed VPX settings file remained byte-for-byte unchanged. Private
settings and native-owned cache evidence were preserved. Hub did not write into
the collection or change the shared null service. The cleanup direction allowed
no replacement launch or new capture, so the shown-Hub dashboard stage was not
run. No dashboard-hiding workaround, input takeover, headset/controller test,
or source-image substitute was used.

Private run, native log, card, exporter and closure receipts remain local and
are identified in the lead handoff. Graphics booking was released to the lead.
Next bounded work requires a lead assignment: establish an approved hidden
dashboard condition for scene capture, or separately book the shown-Hub
compositor/orientation check.
