# Playing Pinball from Hub

Open **Pinball**, choose your table folder and let the scan finish. Hub scans
that folder's tables in place and keeps the result in its own data, so the tab
shows your tables at once the next time; use **Scan** after adding or changing
tables. Use **ROM folder**, **Media folder** and
**Database** if you have those local resources. Select a table, then use
**Locate VPX** to select your installed VPX executable.

Every table has two buttons, on its tile and on its page: **Launch on desktop**
and **Launch in headset**. Each does exactly what it says, every time; nothing is
remembered. Both need a table with no scan error or known missing ROM, while no
other Hub session is active. **Launch in headset** also needs the table marked
VR-ready (detected metadata or your **Mark VR-ready** choice; it does not prove
the table's VR room works) and SteamVR already running: until then it stays off,
and the page says why. **Launch on desktop** never needs SteamVR and never starts
it. Hub uses VPX's normal settings and score saving; it does not select your
OpenXR runtime or start it.

## How the two launches work

Hub does this without changing your VPX settings: it gives VPX a settings file of
its own for that table (`-TableIni`), which VPX layers over your normal settings.
For the headset it holds `[PlayerVR] AskToTurnOn = 0` (VR on); for the desktop
`AskToTurnOn = 2` (VR off), so VPX never starts OpenXR. VPX shows the table in its
desktop view, as your own settings set it. The file lives in Hub's `user/pinball/` folder. The first time, it
starts as a copy of the table's own settings file if there is one beside the
table; that file is only read. Changes you then make to that table inside VPX are
saved in Hub's file, so they apply to launches from Hub.

For the headset, have it connected and SteamVR running first. If VPX cannot
reach the headset it asks on the PC monitor whether to retry; answering No plays
on the monitor. Use the VPX build with OpenXR (the BGFX build of 10.8.1).

## Gamepad

An Xbox-style gamepad (one Windows sees as an XInput controller) plays
the table with either launch:

| Gamepad | Table |
|---|---|
| LB / RB, or LT / RT | Left / right flipper |
| Right stick, pull down and let go | Plunger (analog) |
| A (hold) | Plunger button |
| Start | Start game |
| Back / View | Add coin |
| Left stick left, right, up | Nudge |
| Left / right stick click | Magna-save, where the table has it |
| D-pad | Visual Pinball's menus |
| Start and Back together | Exit the table |

VPX keeps gamepad mappings in its main settings file, which a table settings file
cannot change, and names each gamepad by its SDL identity. So at each start Hub
asks the SDL library beside VPX (`SDL364.dll`, only read and only for the list of
gamepads, with VPX's own defaults) which gamepads are connected. When there is
one, Hub writes `user/pinball/VPinballX.ini`: a copy of your own VPX settings
(`%APPDATA%\VPinballX\10.8\VPinballX.ini`, or the one beside VPX) with this
layout added to `[Input]`, and starts VPX with `-Ini` pointing at it. Your keyboard
and VR controller bindings stay; VPX's own gamepad layout is turned off for that
gamepad so it does not replace this one. Your own settings file is only read.
The copy is made again at every start, so settings changed inside VPX during a
Hub launch are not kept; change them in VPX started on its own. With no gamepad
connected, Hub passes no `-Ini` and VPX reads your settings as usual.

When you launch a table, the now-playing card keeps that table's title and
launch route even if you navigate to another page. The status shows preparation.
If you change or move a scanned table, use **Scan** again before playing.

Use the global **Stop** control to cancel during preparation. Hub waits for any
preparation already underway to finish safely. If the child is starting, Hub
keeps the close request until its own window can receive it. The card remains
while Hub waits; another table cannot start during that time.

Once the table is running, **Exit game** or global **Stop** requests normal VPX
closure. Repeated clicks do not start another close. Hub waits for VPX to exit
without a forced-stop countdown. If clean close is unavailable on your platform,
the card explains this and you must close the table in VPX.

Closing Hub while a table is active on Windows also requests normal table close
and waits. On a platform without that close support, exit the table in VPX first.

**Restart** first requests normal close, then launches the retained table and
executable again only after a successful exit. Global **Stop** cancels a pending
restart. A failed exit leaves an error instead of replaying. The card clears when
the session ends; recent-play history is saved in Hub settings when launch starts.

Hub has no recentre control; use the game's own. For scan and
launch implementation details, see the [software boundary](PINBALL_SOFTWARE.md).
