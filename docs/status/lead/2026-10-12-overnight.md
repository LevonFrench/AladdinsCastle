# AladdinsCastle — resumed checkpoint, 12 October 2026

All five lanes resumed. The owner then needed the GPU again: all graphics bookings are cancelled, Dolphin and every owned null-service helper are closed, and original SteamVR settings are restored byte-for-byte. Source work, CPU tests and review continue. Main, the DolphinXR pin and the OpenXR runtime remain unchanged.

**What worked.** The owner played Umbrella's saved train mission in the real headset: the inset window was gone, depth looked right, shots landed at the reticle, motion was clear/smooth and the HUD stayed readable. The published native profile uses scale10 and a4m panel; the sitting used4x/MSAA4/AF16. The Eager-labelled sitting felt equally good, but pinned D3D disables detached pacing: this was not a distinct heartbeat A/B. Time Crisis produced real first-stage left/right desktop pictures. The gun room rendered and tracked in the headset.

**What failed.** Later train gameplay reproduces displaced world bands and a boxed blood effect in both eyes, including at the larger mirror size through the earlier launch route. The early paused scene is clean at both sizes. This narrows the failure to a later gameplay condition; the causal shader is not identified. Whole-mission visual acceptance remains open. The gun points upward under normal controller hold and the fingers miss; fit is not accepted. Time Crisis has no headset connection; a separate desktop pointer candidate is built but untested in play. VPX's null-VR attempt reached renderer initialization but not player startup; the child needed forced closure. No Pinball eye pair or upright Hub compositor proof resulted. Umbrella's menu wallpaper/panel seam remains.

| Goal | Evidence and remaining work |
|---|---|
| 1. Umbrella profile | Scale10/4m published; saved mission headset pass and null eyes exist. Menu texture isolation and every-screen completion remain. |
| 2. Sitting kit | Exact PS5.1 wrapper ran end to end for60seconds under null; both-eye mirror and OpenXR log saved. Launcher returned0, but bounded Dolphin closure required termination (exit-1), and captured bands fail the visual check. Ten owner checks retained; no new sitting requested. |
| 3. Frame rate | Off/Auto/90 native entry prepared; quiet measurements and game-speed evidence pending. Extraction60 primary-source transcription verified; code remains disabled pending gameplay baseline and runtime comparison. |
| 4. Aim | Final gain100/30 profiles prepared; qualitative shot alignment passed. Systematic nine-point sensitivity/Recenter comparison remains. |
| 5–7. Other games | Darkside mission/profile, other-five both-eye boots/profiles and Metroid Trilogy gameplay reference remain. No startup screen is counted as gameplay. |
| 8. Hub/Pinball | Desktop launch/card previously proved. Alias/pre-launch cancellation fixes pass89 launch tests; global Pinball Stop and close-during-startup regressions are being addressed. Actual VR orientation/table eyes remain. |
| 9–10. Guns | Fixed-pose eye capture waits for GPU availability. Lead corrected grip-position/aim-orientation placement, tracking admission and the support solver; nine CPU tests and Guns' independent source review pass. The regression includes a 90-degree grip/aim difference, hand switching and tracking recovery. Physical confirmation remains. Guns is correcting finger contact separately; GPU finish sheet remains. |
| 11. System22 | Real first-stage Time Crisis both-eye desktop output achieved. Physical scale, tracked controls and headset integration are separate outstanding work. |
| 12. Integration | PR91–95 integrated; next1ffa5cf has both complete CI runs passing. The reviewed pose correction is the next publication; its combined CI remains pending. PR96 makes the fallback shortcut opt-in: Lead source review and independent CPU/actual CLI checks pass; its final Windows check precedes merge. |

**Ownership.** Frame preserves the completed comparison and prepares remaining title tests; Viewer owns source work for the fixed-gun and fallback routes; 3D mode owns native profiles and real-board integration; Hub owns launch lifecycle; Guns owns fit and finishes. Lead reviews and integrates. All graphics bookings are cancelled until the owner releases the GPU. No mouse/keyboard takeover.

**Needs the owner later:** actual controller-ray samples for final native nine-point aim and confirmation of corrected held fit. No sitting is requested now. Null captures cannot establish physical comfort or controller fit. Private discs, saves, settings, logs and captures remain outside Git.
