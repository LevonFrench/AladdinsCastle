# Gun-room grip calibration checkpoint

Controller-operated calibration is an opt-in `--xr --calibrate-grip` room mode. The selected hand uses a centered B/Y hold to enter, stick X/Y to adjust yaw/pitch, a longer centered hold and release to save, and a short tap to cancel. Focus, tracking, action activity, trigger release, and fresh matching controller/tracking samples gate edits and saves. A bounded, model-and-hand-specific JSON stores only validated pitch/yaw; a failed save restores the committed angle. The old CLI angle file and explicit degree options retain their behavior.

The additive libacvr contract has a size/version-tagged controller sample and gun-angle update. The sample getter is available only from a provider that supports calibration samples; the angle setter works for any loaded gun model. Calls from backend callbacks are rejected, including pause and destroy paths. The native room displays selected-hand angles and edit/save state in an eye-size-relative lower central panel.

Evidence: Windows x64 Visual Studio Release configure and build succeeded with the already-local dependencies. Headless CTest passed 14/14, including ABI, recorded runtime, pure state/layout, persistence, CLI, math, and existing model/control/GL-mock checks. The built `acvr_gun_room.exe` SHA-256 is `6BB828BB9BD0E2CE4D339D9BF698DC25DA0400568B3696B6A8565392F9C9DEEF`.

No XR session, GPU render, real controller binding, stereo headset readability, live grip fit, or persisted native reload was tested in this checkpoint. Those remain separate acceptance gates. The binary used for earlier Hub integration is historical and is not replaced by this build.
