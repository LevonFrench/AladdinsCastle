# Time Crisis private stereo prototype: source review

Scope: Lead's private patch generator against the original upstream engine source. Read-only; no extracted data, game files, build, render, or runtime examined.

## Finding: require an actual offscreen target

The initial stereo host checked only the dimensions returned by `rt_begin`. The original render-target helper draws directly when requested and drawable dimensions match, and falls back to direct drawing if its FBO is unavailable. At a 640 by 480 drawable size, the dimension checks therefore passed even though no FBO was active. `rt_end_rect_keep` then did nothing, while each native scene draw cleared the default framebuffer. The second eye replaced the first rather than presenting both halves. Reported to Lead as P2 before build/runtime.

Lead corrected both the private source and its patch generator by adding `rt_is_offscreen()` and requiring it in both capture and presentation guards. Stereo reread those exact guards and the helper returning actual active state. This resolves the identified silent direct-draw fallback in source; no runtime reproduction or acceptance is claimed. No other blocker identified within the bounded review.

## Other reviewed behavior

For the configured 1280 by 480 window with an available 640 by 480 FBO, sequential reuse is coherent: each eye redraw clears its target, the first blit clears the window, and the second preserves the first half. Gamma copies from and draws into the bound target; it does not force the default framebuffer.

The eye offset is subtracted from the already transformed view translation before native backface tests, clipping and projection. Native zoom and viewport centers remain in use, with no toe-in rotation. The list walker reconstructs local camera state on each call and resets per-object lighting state. Z, sort priorities, native sprites, text, fog, fade and gamma remain on the existing compositor path.

The retained polygon-word callback reads live board RAM rather than a deep snapshot. The inspected single-thread path prepares video, captures if requested, then enters host presentation before returning to emulation; there is no emulation tick between the two eye draws. That supports this synchronous diagnostic, but does not establish immutable-frame ownership for a future asynchronous renderer or headset loop.

The prototype's early stereo return bypasses the normal host menu overlay, gun picture rectangle update, screenshot-hotkey path and draw timing. Rewalking also accumulates native geometry diagnostic counters beyond one preparation. These are diagnostic limitations to retain in receipts, rather than claims of complete host integration.

No physical scale, aim calibration, HMD behavior, libacvr integration or game acceptance is established by this review.
