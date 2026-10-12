# Compositor exporter candidate — 2026-10-12

The isolated Viewer candidate adds an explicit OpenVR compositor mirror export and a separate own-dashboard show request. It does not substitute the Hub source texture for compositor evidence. Output requires a fresh private catalog-local directory and includes both labelled eye images plus a receipt.

Native MSVC Release compilation completed for the Hub and the three focused test executables. GPU-free CTest passed all three suites; deterministic Qt reports confirm 11 compositor cases, 16 source-capture cases and 29 overlay cases, all with zero failures or skips. Offscreen/software Qt was used. Public privacy and whitespace checks passed. The source-capture suite reports a temporary-directory cleanup warning in its reparse-path fixture; its assertions passed.

The exporter rejects partial/invalid reads, cleanup failures, unsupported formats, non-private or existing output, and mismatched or changing dashboard visibility. Fake mirror tests cover release/unlock paths, eye labels, output privacy and command-mode isolation. No native compositor acquisition, dashboard presentation or physical headset acceptance follows from these tests.

Live gates remain: OpenVR mirror availability under the existing null service; actual mirror format and dimensions; correct content/orientation in both exported eyes; Pinball with dashboard hidden, then the Hub with its own dashboard visible. The accepted format is bounded desktop GL linear RGBA8; sRGB/HDR mirrors are currently refused. Sequential eye reads do not guarantee an atomic stereo frame. A supervisor must bound blocking native calls.

Independent Hub draft review found no source blocker; final review must use the frozen candidate head and these test results. Lead owns runtime booking and integration. Main and the frozen runtime candidates remain outside this change.
