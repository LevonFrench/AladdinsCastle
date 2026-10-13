# Fixed authoring poses: independent source review

Scope: Lead's capture-only gun-room change, read-only. No edits, build, OpenXR run, GPU capture or controller verification performed by Stereo.

The initial host implementation allowed live hands and input through whenever head tracking was invalid, while the receipt still identified fixed authoring poses. Lead corrected this during review: fixed mode now returns an error when head flags are invalid, and capture also requires initialized fixed hands. The corrected source closes the identified fallback and receipt mismatch.

Lead also requires actual gun-overlay draws in both eyes before fixed-mode capture; Stereo reread the resulting gate. This prevents a visible but unfocused room-only frame from satisfying a fixed-gun capture.

No remaining blocker identified within this small review. The quaternion rotation and right/left offsets agree with the existing right-first hand convention. Initialization validates the pose and preserves output on rejected input. Actual head and eye views remain OpenXR-derived. Controller buttons, trigger, reload and calibration samples are neutralized; the stdin channel accepts only cooperative close. CLI gating requires XR capture and rejects grip calibration. Receipts identify authoring poses and leave controller tracking verification false.

Reviewed the helper, host integration, CLI gates, receipt fields and synthetic test source. The synthetic tests cover translation, yaw rotation and rejected input preservation, but do not establish host tracking-loss behavior at runtime. Lead owns build and capture validation.
