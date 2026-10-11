# Two-scene paired proof: source design delta

Source-only implementation, uncompiled and unexecuted until independent review.
Only the frozen markers (pose0,pose1,pose0 replay) and clipped CZ scene are admitted.
Production `n22_native`, `begin_eye` and public graphics retain gate0. A separate
default-OFF proof target uses the same twelve pinned units and approved cached SDK.
Explicit proof build and controller run flags do not grant runtime authorization.

The native process owns one context/window and immutable formula quad/resource
storage. It acquires two fixture snapshot leases, increments its actual native
fixture counter once per acquisition, stages both eyes, primes native cache/LUT
resources and then locks texture lookup to cache hits. It records actual policy,
palette generation and occupied cache entries before/after draw. No point/ROM
loader, game step, OpenXR, alternative rasterizer or additional native unit.

The setup-local Python controller hashes actual exported resource bytes and fills
the snapshot root's explicitly UNHASHED diagnostic holes before acknowledging a
draw. Those raw files are never called completed snapshot artifacts. Native code
compares its retained geometry/resources with the exported originals before and
after draw and rejects drift. Controller rechecks the same bytes and actual cache
audit before writing complete receipts. Hashing uses standard-library SHA-256;
no new library or invented GL handle is needed.

One complete GL_BACK readback follows both eyes and error checks; row export is
flipped once. The controller independently checks frozen pixels/centroids, hashes
full/eye RGB, and acknowledges only a complete validated pair for one swap.
Final manifest follows native normal cleanup/exit. Producer target correlation
and swap-requested remain distinct from compositor delivery (unverified). Maximum
four pairs in this implementation; total process deadline15s, no automatic retry.
No Viewer admission or WGC protocol is implemented.

CPU fixtures first exercise hashing, wire/correlation validation, image oracle
negative cases and transaction ordering. They use synthetic bytes and do not
pretend to execute C/GL. Guarded patch preparation is checked from approved source
without compilation. Actual C compilation, resource upload/cache behavior and
pixels remain separate later gates. The lead owns integration and run slots;
Stereo owns only this native-slice code and status report.
