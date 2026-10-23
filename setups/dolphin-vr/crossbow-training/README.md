# Link's Crossbow Training — mono theatre candidate

European disc ID RZPP01. The local disc header was read without copying the disc. This package contains input/configuration files only.

E2 offline observations reached Ordon Target Practice using a locally registered built-in Link avatar. The initial open-frame configuration entered a timed round but drew a large blank panel over the targets. That configuration failed visual review and is not the supplied presentation.

The supplied fallback uses FlatScreen=True and DisableCPUCull=False. Its single-image output shows the world, target boards, score HUD and a decreasing round timer, with zero FIFO errors in the bounded observation. Label it **Theatre candidate (mono)**. It does not provide per-eye world depth. Headset panel placement, pointer projection, physical scale and pacing remain unverified. A sampled fire input did not establish a successful hit or calibrated aim.

`AC Crossbow Candidate.ini` is a Wiimote profile. `RZPP01-controls.ini` supplies the per-game selector; `RZPP01-vr.ini` is the mono candidate configuration. Their destination names are respectively `Config/Profiles/Wiimote/AC Crossbow Candidate.ini`, `GameSettings/RZPP01.ini` and `GameSettingsVR/RZPP01.ini` beneath a dedicated User folder. They are not installed automatically. Keep other regions separate until tested.

The profile maps right A/B/trigger/stick click to Wii A/1/B/2; right stick to the D-pad; left stick/X/trigger to Nunchuk stick/C/Z. Plus is left-stick click, Minus left Y, and Home a 0.5-second Y hold. Holding Y briefly emits Minus before Home. MotionPlus is off; neither system button is mapped.

`menu-observation.txt` is the original bounded menu replay. Eight of nine cursor positions were identified in the open-frame menu observations; this does not calibrate theatre pointing. `level-one-observation.txt` follows the learned route using a test user with a registered local player; it is not a fresh-user setup script. Saves are not included. The game offers built-in avatars and an Adjust alignment menu. No numeric 1.5% accuracy result or aim trim was established.

Use a separately reviewed synthetic probe for offline checks. Game media, saves, screenshots and private launcher paths are excluded. This package is a limited offline candidate, not whole-game or headset acceptance.
