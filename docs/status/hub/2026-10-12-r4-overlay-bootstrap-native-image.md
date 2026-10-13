# Hub: initial overlay texture and native image diagnostic

Source candidate on `codex/hub-native-overlay-image`, based on `next`. Runtime acceptance remains open.

## Change

The corrected-resource diagnostic initialized GL and requested its own dashboard, but never rendered a frame. The host required native overlay visibility to render, while the runtime independently refused to submit a hidden overlay's first texture. The offscreen QML root also started hidden. These source gates explain why that run could not reach texture capture; the installed runtime's underlying visibility dependency remains unproved.

After successful runtime and renderer initialization, the host now requests one actual QML frame, even while the native overlay remains hidden. The native submission gate permits that first texture. A failed submission stops the host; a hidden no-op cannot count as successful bootstrap. After successful submission, normal hidden rendering suppression resumes. Input and keyboard handling still follow actual native visibility. Picture bounds, render-target mirroring and pointer conventions are unchanged.

The existing opt-in `--overlay-capture-dir` now calls `GetOverlayTextureBounds` and `GetOverlayImageData` on the owned overlay only after an actual successful texture submission. It retains `texture.png` as the Qt source image and writes `native.png` separately when native acquisition succeeds. `receipt.json` records API return codes, dimensions, buffer size, bounds before/after, submitted OpenGL texture type and separate evidence status. Native errors never substitute Qt pixels. Size is limited to 4096 per dimension and 16 MiB RGBA; changed dimensions or bounds reject the image. Native API rows remain unchanged because the SDK does not specify their origin. This is source-image evidence, not compositor-eye or headset acceptance.

## Validation

The native diagnostic cases first failed against the previous Qt-only behavior: 2 pass / 4 fail including fixture setup/cleanup. Both cold-start policy cases first failed against visibility-only gates: 2 pass / 2 fail including setup/cleanup.

After implementation, the existing offline Windows build succeeded and all three actual CTest suites passed in 14.64 seconds:

- Overlay synthetic: 32 pass, 0 fail.
- Source/native capture CPU: 32 pass, 0 fail; native API errors, invalid/oversized dimensions, changed dimensions and bounds, unchanged native rows, separate PNG provenance and no retry.
- Real QML scene: 51 pass, 0 fail; the bootstrap-renderable scene rejects hidden clicks, then accepts them when visibility returns.

Tests used synthetic fixtures and offscreen/software Qt. No SteamVR initialization or GPU validation of the new source was performed. A premature local test invocation while compilation was still active used stale binaries and the wrong temporary root; it is excluded from regression evidence. Completed-build results above used the project temporary directory.

## Runtime checkpoint, separate from this source candidate

The existing reviewed diagnostic executable, with corrected adjacent resources, initialized and requested its dashboard but produced no rendered source frame. No unchanged compositor-mirror retry was made. Upright display and native image acquisition remain unverified.

The authorized same-table native Pinball comparison reached `Startup done` with VR off and closed normally. VR on reached the radiance entry but neither completion nor startup, and required owned forced closure after normal close failed. CPU activity does not prove radiance progress; the source wait-loop explanation remains a hypothesis without a live stack. No paired Pinball eyes or gameplay acceptance resulted.

The VR-on observation ran 96.4 seconds against a planned 90-second observation. Cleanup follow-up was delayed after the normal-close grace; final absence was observed 291.8 seconds after launch. This supervision error is preserved in the private receipt. Future admitted probes must perform timed close and any exact-owned-process cleanup in one executing bounded operation. All Hub consumers were closed and the graphics slot released. Private game paths, settings and receipts remain outside tracked files.

## Next

Independent exact-head review and hosted checks precede integration. A new explicit runtime booking is required to check actual first submission/native API support and then compositor orientation. No automatic flip or runtime restart is proposed. Ask Lead for the next CPU item while runtime validation waits.
