# Overlay diagnostics and Wii sitting

Run `aladdinscastle-hub --overlay --window --spike` only in the lead-coordinated
owner sitting. Desktop-only `--spike` uses the same page without a VR session.
Four off-centre targets and the yellow Qt marker expose Y orientation. The
Keyboard/Cards switch leaves enough space to test both the list and all 39 keys.
Live controls select Flip Y, press freeze0–500ms, click slop0–200px, drag
threshold0–200px and smooth-scroll scale120/2880. Initial overlay values are
100ms/40px/40px/2880, the design defaults; they are not headset-calibrated.
Changing a setting cancels the current click. Values are transient, not saved.

The last20 numeric events show raw packet/Qt types, positions, rejection reason,
press position, release/move position, drift and separate smooth/discrete deltas.
No keyboard payload is recorded. The long card list exposes contentY. Keyboard
focus alone never requests SteamVR text; the explicit button uses the existing
session/token/UTF-8 guards. All39 panel keys and Done are tested by hit geometry
under both Y settings. Desktop mouse routing remains native.

## Input behavior

Only finite in-panel moves establish position; button coordinates are ignored.
The production dashboard does not enable the multiple-cursor overlay flag and
uses one logical pointer/left trigger. Raw cursor IDs remain observable; the
flag-enabled protocol path has explicit test opt-in. A valid move restores focus
only while both host and event visibility gates are open. A repeated down after
a missing up cancels the old Qt grab before beginning one new click. Hidden
packets cannot rearm the pointer or activate controls.

The pointer stays at its press point during the freeze interval. After that,
movement remains withheld until it exceeds the drag threshold; crossing it feeds
the actual QML drag. A nondrag release within click slop returns to the press
point; a nondrag release outside slop cancels. Smooth packets use the live scale;
discrete packets always use120. Fractional wheel units and the ±100 raw delta
clamp remain. Freeze/slop work only in the synthetic overlay path.

Receiver/visibility epochs have no elapsed-position expiry. One monotonic receipt
sample per packet stamps Qt events; external samples advance the same high-water
mark. Regressing clocks cancel/reject until they catch up. OpenVR eventAgeSeconds
is not an occurrence clock. Receiver/target destruction uses weak references and
queued cleanup generations; cancellation clears the overlay's dedicated mouse
device grab and releases off-panel, separate from the native desktop device.

## Acceptance

Software checks cover real QML Button/MouseArea/TapHandler/Flickable cancellation,
press drift versus drag, focus/dropped-up recovery, smooth/discrete scaling,
keyboard geometry and independent hidden/visible scenes. A desktop capture has
been inspected. Actual Y direction, packet ordering, controller feel, readability,
ten consecutive card selections and every key remain for the owner sitting.
Speed/dead-zone and pointer filtering are not part of this checkpoint.
