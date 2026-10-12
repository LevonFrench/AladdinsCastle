# Playing Pinball from Hub

Open **Pinball**, choose your table folder and let the scan finish. Hub scans
that folder's tables in place. Use **ROM folder**, **Media folder** and
**Database** if you have those local resources. Select a table, then use
**Locate VPX** to select your installed VPX executable.

**Play** is available for a table marked VR-ready with no scan error or known
missing ROM, while no other Hub session is active. VR-ready describes detected
metadata or your **Mark VR-ready** choice; it does not prove the table's VR room
works. Configure VR in your chosen VPX installation. Hub uses VPX's normal
settings and score saving; it does not select your OpenXR runtime.

When you choose **Play**, the now-playing card keeps that table's title and
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

**Recentre** is unavailable in Hub; use the game's recentre control. For scan and
launch implementation details, see the [software boundary](PINBALL_SOFTWARE.md).
