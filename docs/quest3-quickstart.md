# Play now on Quest 3 (before AladdinsCastle exists)

These existing projects already run 3D light gun games in true VR today. AladdinsCastle will launch them in hand-off mode. Until then, here is how to run them directly. Status checked 2026-10-08.

## Time Crisis VR (DR-89): standalone on Quest 3, or PCVR

- Repo: https://github.com/DR-89/time-crisis-vr (MIT port code; experimental; latest v0.8.4, 2026-10-07)
- Built on https://github.com/spacestate1/namco22-decompile
- Effort: **about 15 minutes**.

Standalone (Quest 3):
1. Turn on developer mode for the headset in the Meta Horizon phone app. This needs a free Meta developer account.
2. Connect the Quest by USB-C and allow USB debugging in the headset.
3. Install the APK from the Releases page with SideQuest, or with platform-tools:
   ```bash
   adb install -r TimeCrisisVR-v0.8.4-quest.apk
   ```
4. Open **Time Crisis VR (Experimental)** under **Unknown sources** in the library.
5. Right **A** inserts credits, then either trigger picks your gun hand and starts.

Game content: the README says the release APK bundles the Time Crisis ROM set. The project also documents a ROM-free build that takes your own dump (`timecris.zip`, Time Crisis World TS2 Ver.B, checked by SHA-256). If you want to play only from your own dump, use the ROM-free build route.

PCVR: extract the Windows zip, start Quest Link, Air Link or Virtual Desktop, then run `Play VR.cmd` (or `Play SteamVR.cmd`).

Controls: either trigger fires and selects the hand; either grip is the pedal (leave cover); or switch to physical ducking in options. Hold still and press **X** (left) to recenter and set standing height.

## Virtua Cop 2 VR (VC2VR): PCVR

- Repo: https://github.com/NeuralF/Rea-Virtua-Cop-2-VR (MIT, v1.0 beta)
- Needs: your own **Virtua Cop 2 PC (1997)** install, Windows x64, any OpenXR runtime.
- Effort: about 10 minutes if the PC game already runs. The 1997 installer can be fiddly on modern Windows; the README explains the `PROJECT` folder workaround.
- Copy the mod next to `PPJ2DD.EXE`, start your VR runtime, run `VC2VR.exe`. Two controllers give two guns: player 2 joins with A or X.

## PenguinScreen2: PS2 in VR (Linux PC or Steam Deck)

- Repo: https://github.com/PenguinVRLab/PenguinScreen2 (GPL-3.0, v1.0-rc2)
- Needs a Linux or SteamOS PC, WiVRn on the Quest, your own PS2 BIOS dump and discs.
- All games run on a virtual screen. A few profiled games get true stereo and head tracking. Gun aiming isn't mentioned yet.
- Effort: about an hour, including installing WiVRn.

## Inspired-by games (native Quest Store and SideQuest titles, not ports)

- *Under Cover* (Sigtrap, 2024): Time Crisis-inspired cover shooter, Quest Store.
- *Crisis VRigade* (Sumalab): free on SideQuest, Time Crisis-style.
- *Zombieland VR: Headshot Fever*: House of the Dead-style rail shooter.
