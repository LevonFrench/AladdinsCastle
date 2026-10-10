# Hub in VR: how the menu is controlled

Status: 2026-10-10, design. The SteamVR dashboard integration works: the Hub shows up as a dashboard tab at the right size and can be resized. Controlling it does not feel right yet. In the first headset session some clicks missed, the thumbstick did not scroll and letter keys failed. This page sets the control scheme and lists what to change, from research into how SteamVR overlays receive input and what working overlay apps do (hub wiki topic `steamvr-development`, article `vr-menu-controls`).

The problem analysis below refers to `codex/m1-integration` at `5921903`, the earlier owner-tested build. The control scheme is a target, not a claim of implemented or headset-verified behavior. On 2026-10-10, diagnostic source `9038515` in [PR28](https://github.com/LevonFrench/AladdinsCastle/pull/28) passed independent build and five software suites (43.47 s); hosted CI/integration and headset acceptance remain separate.

## 1. What the evidence says about the three problems

| Seen in the headset | What the code does | What working overlays do | Change |
|---|---|---|---|
| **Some clicks work, some don't** | `hub/src/overlay/OverlayInput.cpp` sends press and release at the coordinates carried by the button event. Nothing raises the drag threshold, so a press that moves a few pixels inside the scrolling grid becomes a drag and the click is lost. | Valve's own Qt overlay sample sends press and release at the **last mouse-move position**. Studies of ray pointing find the trigger pull itself shifts the pointer (upward in about 73% of trials). | Use the last move position for button events. Hold the pointer still for about 100 ms around a press. Raise the drag threshold to about 40 px while in the overlay. Filter pointer jitter (One Euro). |
| **The stick doesn't scroll** | Requests smooth scroll events and multiplies their deltas by 120 before building the wheel event. | OpenVR Advanced Settings (Qt and QML, like us) multiplies smooth scroll deltas by **2880** (360 × 8; Qt's wheel unit is an eighth of a degree). Our factor is 24 times smaller, so the list barely moves. | Keep smooth scroll events and scale **those** by 2880, with a speed setting and a dead zone. A discrete scroll event is one notch and stays at 120. Log which kind the runtime actually sends before tuning. |
| **Numbers work, letters don't** | Not explained by the research. | Advanced Settings reads typed text with `GetKeyboardText`; Valve's sample and Desktop+ treat the keyboard as a session tied to one field. | Needs a test that presses every key through the real hit geometry, for both the SteamVR keyboard and any on-panel keys. See §4. |

These are evidence-backed causes, not proven ones: no packet trace from the headset exists yet, and the thresholds and timings below are targets to tune, not measured settings. The Y direction of pointer coordinates also differs between the three reference apps, so it must be checked on the real runtime with off-centre targets.

## 2. The control scheme

One scheme, always on, usable seated with one hand.

| Action | Control |
|---|---|
| Point | Either controller's laser (SteamVR's dashboard pointer) |
| Select | Trigger |
| Scroll | Thumbstick up/down under the laser; or hold the trigger and drag the list; or the page buttons at the edge of every list |
| Back | The Back button at the top left of every page (and SteamVR's own dashboard button to leave) |
| Text | A keyboard button beside each editable field opens SteamVR's keyboard |
| Feedback | Hover highlight, pressed state, a haptic tick on hover and on press |

Rules:

1. **Clicks land where you aimed.** Button events use the last move position; the pointer is frozen for about 100 ms from trigger-down; movement under the drag threshold between press and release is still a click.
2. **Drag threshold about 40 px** in overlay mode (a few pixels on the desktop). Above it, a press on a list becomes a scroll drag.
3. **Stick scrolling** uses smooth scroll events × 2880 × the user's speed setting, with a dead zone, and can be turned off (a resting thumb must not scroll). Discrete events, if the runtime sends them instead, count as one notch (120) each. The scroll goes to the list under the controller that scrolled.
4. **Targets:** at least 48 px high with 8 px gaps on the 1280 × 800 overlay canvas, hit areas larger than the visuals, and hit areas at the panel edge extended outward. (Platform guidance: about 22 mm, or 2.5 to 3 degrees, per target and 12 mm between targets.)
5. **Typing is rare.** Filters, an A to Z jump strip and recent searches come before the keyboard.
6. **Losing focus never clicks.** On focus loss or hide, held buttons are released without activating anything and the pointer is parked off the panel.
7. **Hover, press and release are always visible**, and the laser's dot sits on the panel, not in front of it.
8. **Controller buttons** (A/B/X/Y, grips) are not relied on. Reading them from a dashboard overlay is an experimental SteamVR feature that needs a developer setting. If it proves reliable, B = Back and grips = previous/next tab come later.

Desktop mode keeps mouse, keyboard and gamepad as they are.

## 3. A diagnostic page

The reviewed PR28 overlay test scene (`--spike`) now supplies the following observations. Use `--overlay --window --spike` only for a separately approved headset run; see [the procedure](../spikes/s1-s3-overlay.md).

- Four off-centre targets with individual hit counts, for comparing X/Y direction and scale.
- The last pointer position submitted to Qt, drawn as a marker with raw cursor ID.
- Bounded per-cursor move/press/release and separate smooth/discrete counts, raw deltas and Qt angle deltas.
- A long list with visible `contentY`, for checking actual movement.
- A transient text field, explicit **Open SteamVR keyboard** button, and all 39 on-panel keys plus Done. Focusing the field does not request the keyboard.

No typed text is logged. Software tests cover actual hit geometry under both Y settings and matching/stale/wrong/closed keyboard sessions without runtime initialization. The observations retain existing button-event coordinates, global pointer/buttons/remainder and ×120 scroll conversion. They do not implement last-move authority, per-cursor routing, press freeze, new scroll scale, drag threshold or cancellation fixes. Invalid packets and synthetic focus/hide releases are outside the counters. Headset packet evidence still decides feel tuning.

## 4. Acceptance in the headset

1. All four corner targets hit first time, with either controller.
2. Ten cards clicked in a row inside the scrolling grid: ten opens, no accidental scrolls.
3. The stick scrolls the library top to bottom in a few seconds; a resting thumb does not scroll.
4. Trigger-drag scrolls the list and never opens a card.
5. Every letter, digit, space and backspace reaches the search field; Done closes the keyboard; the search runs.
6. Pressing a button and moving off the panel before release does not activate it.
7. Text on the smallest card size is readable without leaning in.

## 5. What the owner reported (2026-10-10)

- The letters that failed were the **keys drawn on the panel**, not SteamVR's keyboard.
- Controllers: stock Meta Quest Touch controllers (through ALVR and SteamVR).
- Failed clicks were **everywhere**: only some of the top bar worked, and only some of everything else.

Because the failures were not limited to the scrolling grid, pointer position and press/release stability come first, the drag threshold second:

1. Button events at the last move position (the build tested used the position in the button event).
2. Freeze the pointer from trigger-down, and treat a release within the click slop of the press as a click on the pressed control, even if the laser has drifted off it. A trigger pull that shifts the laser by a degree or two moves it about as far as a 44 px button is tall.
3. Bigger targets on the panel keyboard and the top bar (§2 rule 4).
4. Then the drag threshold for lists.

The diagnostic page (§3) decides between these before anything is tuned.
