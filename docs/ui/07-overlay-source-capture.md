# One-frame overlay source capture

This opt-in diagnostic reads the renderer's existing texture after its submit call. Normal Hub mode performs no readback or output work. Run it only in a coordinated GPU/Viewer session; it does not start or configure a new VR service.

Use `--overlay --overlay-capture-dir <absolute fresh directory>` with an output directory beneath the selected catalog's `.local`. The ordinary `--data-root`, `--overlay-picture-flip-y on|off`, `--overlay-pointer-flip-y on|off` and bounded `--quit-after-ms` options still apply. The overlay must become visible and render a frame; shutdown reports when no capture was attempted.

The first attempted frame writes `texture.png` and `receipt.json` without overwriting an existing target. Capture mode adds passive TL 1/magenta, TR 2/green, BL 3/blue and BR 4/orange corner markers to the actual Hub or diagnostic scene. The PNG preserves source pixels, with exactly one conversion from bottom-first GL rows to top-first PNG rows. No picture or pointer correction is applied to the PNG.

The readback requires the initialized renderer's current desktop GL3 context and a linear, unsigned normalized RGBA8 attachment. It restores read-FBO/read-buffer, pixel-pack buffer and pack layout on all exits, leaving draw and texture bindings untouched. It performs one attempt per process, including failures, and can stall; it provides no timing/performance result.

The receipt records dimensions/format/row conversion, render-target mirror, actual picture bounds and effective pointer flip separately. A hidden-overlay submit can return true without calling `SetOverlayTexture`; `submission_api_called`, `submission_api_result` and `submission_api_succeeded` distinguish that no-op. `compositor_display_verified` remains false. Inspect the actual compositor/dashboard and pointer behavior independently before calling the menu upright or accepted.
