# Dashboard picture and pointer

The dashboard preserves the Qt target's corner ordering with full texture bounds
and target mirroring disabled. The owner-recorded vertical inversion matched the
previous reversed V bounds. This restores the separate picture correction from
the preserved M1 source handoff. Software corner tests check the submission
policy; a new owner sitting must verify the actual headset image.

Picture and pointer overrides are independent and accessible before the panel:

```
aladdinscastle-hub --overlay --window --spike --overlay-picture-flip-y off --overlay-pointer-flip-y on
```

Picture defaults to `off`. Pointer defaults to `on`, retaining OpenVR's
bottom-left to Qt top-left conversion. The desktop diagnostic's compact **Flip Y**
checkbox has the accessible name **Pointer Flip Y** and a pointer-only tooltip; it changes only pointer coordinates; it never changes the picture. Verify
upright corner labels first, then both controllers at all four corners. Toggle
only the pointer setting for cursor comparison. A picture override requires a
restart. Neither option registers a manifest or changes a system runtime.

The private owner kit runs under Windows PowerShell 5.1 and accepts `PictureFlipY`,
`PointerFlipY` and `PrepareOnly`. Prepare-only never starts Hub. Compatibility
checks use a CPU console fixture for argument quoting, output capture, nonzero
exit and bounded cleanup; these do not initialize graphics or SteamVR.
