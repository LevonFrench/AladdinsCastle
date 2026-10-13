# First sitting: repair directions

The first sitting reached the save prompt, with a visible menu and moving aim
pointer. It did not establish mission gameplay or any of the three true-3D
targets. Existing sitting runs stay intact.

Repair order is buttons, facing, sharpness, logs, operator scripts, then Hub
picture and pointer. Lead owns the sitting kit and integration. Guns owns the
shared shell-and-stick model/control schema; Hub consumes its exact generated
native profile in the recipe. Stereo owns a scoped pinned-source facing and
logging patch. All runtime work waits for the owner's desktop turn; no lane has
a separate desktop lease.

Verified causes: Source-only Wii configuration omitted the device and bindings;
internal resolution was left at 1x; the logger's parent directory was absent.
Source also disproves the suspected logger-before-user-path order. A separate
bug silently ignored `-C GFX...`; the parser requires `Graphics...`. Immersive
recenter updates position but not yaw, so the flat-quad yaw code does not fix the
active virtual-screen route. Hub reproduced a reversed vertical texture bound;
its existing Flip Y control affects only input.

The new kit prepares exact shared native bindings, a both-stick recenter chord,
isolated background hotkeys, 4x/8x-filtering settings, and the log directory.
Windows PowerShell 5.1 passes 25 synthetic checks; all three prepare-only modes
pass. The compiled source candidate passed independent review, including 132
anchored ray cases. It keeps a stable yaw basis and queues recenter until valid
poses; conditional wear detection must be reported separately from the
controller fallback. Candidate Desktop/SharpAA preparation also passes in 5.1.
Desktop AA/filtering and real logging are still pending; no candidate runtime
has been executed.

Next sitting, in both Default and Eager heartbeat modes: (1) put on the headset
and check facing; (2) recenter using both sticks; (3) confirm the save prompt and
reach the first mission; (4) turn/tilt and check a stable HUD; (5) look around for
a wide scene without the old border/culling; (6) check the held gun's size, depth,
hand tracking and aim; (7) separate world and hand judder; (8) confirm the run log.
Record pass/fail/not reached for each item. Native held-model integration and
independent native two-player aiming are not established by model/profile tests.

Guns builds the shell and stick, their split/docked/dual hold descriptions and
the controls-page preview against one data source. Unknown game actions remain
questions. Merge shared controls first, then lead/Hub consumption and the scoped
source patch after review. `next` to `main` remains a draft review checkpoint.
