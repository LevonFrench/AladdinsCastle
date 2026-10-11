# R3 grip coverage follow-on

This separate follow-on to PR50 reviews 22 rest-pose assemblies / 44 CPU
images: both primary hands for the seven proposal models and the stick;
shell/heavy support holds with both primary hands; and shell plus docked stick
with both primary hands. Each has a full view and grip crop. Eight family
sheets, overview and input/image-hash manifest live in
`assets/hands/preview/grips/`. Imported LOD0 selection is deterministic.
No accepted gun or hand GLB changes, mesh mirroring or extra hand variants.

Baseline images exposed heavy left-primary using the wrong handle and shell
support using an upright wrist against a sloped rear handle. Assembly-only
corrections select the heavy gun's opposite handle and read the shell's
canonical dock rotation. Baselines remain in private receipts. Five affected
cases were rerendered; four support cases then received one framing-only
retry because rear view requires negative Blender Y and the shell crop needed
the off-hand side. No further renders are planned in this phase.

The consumer contract is explicit: `primary_mount_by_hand` chooses the gun
mounting node independently of glove anatomy; apply its inverse transform
to place that node at the controller. Heavy right-primary uses `grip`,
left-primary uses `grip_two`. `support_by_primary` chooses the opposite node.
The old `support_grip` retains the right-primary compatibility default and
must not override that selector. Positive thumb scale is unchanged.

Checks pass for complete proposal/hand coverage, actual exported nodes,
mirrored heavy-handle selection, unit quaternions, unchanged inputs outside
selective rerenders and image hashes. Corrections were inspected. All owned
render processes closed; no GPU or runtime launch.

These are static CPU fit views. Fine finger/frame intersections, pressure,
comfort, controller calibration, tracking, mounted yaw/pitch motion and
headset appearance remain unaccepted. Occluded parts remain a review
limitation; the sitting must verify physical fit.
