# Studio review brief: AC VR Hub website

**Mode:** review only. Reviewers write only their own report file in this folder. No edits to the site. Fixes go to a later pass ("The Agency").

**Owner outcome:** a public project website for AC VR Hub (repo: AladdinsCastle, an open-source desktop Hub that installs and launches true 3D VR setups for arcade and console light gun and racing games). The site simulates a walk through a 1990s mall arcade. The owner wants it to evoke the old mall-arcade look (red sign band with checker border, teal/white checker pillars, tile floor, triangle murals, neon carpet, brass tokens) **without using any real arcade chain's name, logo or photos**.

**Owner decisions already made (do not challenge unless evidence shows harm):**
- Title is "AC VR Hub". The storefront sign animates ARCADE → AC → AC VR HUB (captures `sign-t1..t3`).
- Arcade cabinets carry **blank multicoloured marquees**: no words, no logos. Cabinet screens show wordless attract loops.
- Ghost wheel lies back at hand height, not upright facing the aisle.
- **One gun only** at the gun stop (it cycles through five lookalike models, one at a time). The owner said the earlier five-gun line-up was too crowded.
- Stops: 0 mall, 1 motion controls, 2 gun models, 3 racing ghost controls, 4 Sega Model 3, 5 Wii, 6 PS2, 7 prize counter, 8 the Hub, 9 scoreboard.
- Gun and controller models are lookalikes, never copies; no logos (repo `docs/guns.md` §2.2).

**Platform:** browser, Three.js 0.160.0 (ES modules from jsDelivr), WebGL2, UnrealBloomPass + OutputPass, ACES tone mapping, physically based lights. No engine or DCC app is involved: Unity/Unreal/Blender roles are used as **expert lenses** on a hand-coded real-time scene. State plainly where an engine-specific technique does not translate to Three.js.

**Files (absolute):**
- Scene: `J:\projects\games\aladdinscastle\.local\worktrees\website\site\arcade.js` (~1260 lines; all geometry, canvas textures, lights, camera path `STOPS`/`PATH`, exhibits, loop)
- Page: `...\site\index.html`, `...\site\style.css`
- Pipeline diagrams page: `...\site\how-it-works.html`, `...\site\pipeline.css`, `...\site\sprites.js`
- Captures (1280×800 viewport, shown at 800×500): `J:\projects\games\aladdinscastle\.local\worktrees\website\docs\reviews\2026-10-17-studio-experts\captures\` — `stop0-mall.jpg` … `stop7-prize.jpg` are current; `stop8-hub-older.jpg`, `stop9-scoreboard-older.jpg`, `phone-mall-older.jpg` predate the blank-marquee change (marquees then had words); `how-top.jpg`, `how-world4.jpg` show the diagram page.
- Live preview (if a reviewer can fetch it): http://localhost:8765 — do **not** drive the desktop or take window captures; read source and the captures instead.

**Execution limits:** source/static review plus the captures above. No GPU profiling or device testing has been done. Label every claim as observed (in source or a capture), inferred, or hypothesis.

**Report contract:** follow `C:\Users\hotgh\.claude\skills\studio-experts\references\work-contract.md` ("Review report"). Findings ranked by impact, each with severity, the exact `arcade.js` line or capture, the visible effect, and a targeted remedy that fits Three.js. 500–1,500 words. No padding; say so if an area is fine.

**Safety overlay:** read `C:\Users\hotgh\.claude\skills\studio-experts\references\compatibility.md` first. No installs, no servers, no publishing, no account or licence access.
