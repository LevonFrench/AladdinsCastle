# Studio review brief: AC VR Hub website refresh (2026-10-22)

**Mode:** review only. Reviewers write only their own report in this folder. No site edits. Fixes go to The Agency afterwards.

**Owner outcome:** bring the public site up to date with the project, use the project's actual gun and control models, and put more info panels in front of the arcade so the arcade becomes a background to the panels.

**What changed since the last review** (`docs/reviews/2026-10-17-studio-experts/`, whose fixes are already in):
- `website/index.html` + `website/style.css`: nine sections, each a grid of glass panels (`.panel`, `backdrop-filter: blur(10px)`) over the fixed Three.js canvas. A `.scrim` darkens the scene (lighter in the mall). The camera still walks between nine stops as you scroll (`STOPS`/`PATH` in `website/arcade.js`).
- `website/models.js`: loads the project's own CC0 GLBs from `website/models/` (copied from `assets/` by `website/tools/sync_models.py`; tints and motion tables from `data/guns/*.toml` into `website/models/models.json`). Re-materials them: named materials body/accent/dark/glass become `MeshStandardMaterial` with the data tints, or an additive cyan "ghost" `MeshBasicMaterial`. Hides LOD1 nodes. Attaches a grip hand at the gun's `grip` node from `assets/hands/assembly.json`. Drives trigger and recoil nodes.
- `website/arcade.js`: the scene's guns (stop 1 hands with guns, stop 2 cycling gun, Model 3 heavy mounted gun, Wii/PS2 kiosk guns) and the racers' ghost wheel/shifter/pedals now use those models, loaded lazily via `requestIdleCallback` after the first frame (`later()`), scaled up 2.4–3x for visibility.
- `website/bench.js`: a second WebGL renderer inside the "Model bench" panel (stop 2), started by IntersectionObserver; models at real size, drag to turn, Fire drives trigger/slide.
- `website/page.js`: panels fade/slide in on intersection.

**The models' native lighting:** `assets/guns/runtime-lighting.json` (ambient 0.35 + one directional 0.65 in camera space, no specular, no tone mapping). The web scene uses three.js PBR with ACES tone mapping, bloom threshold 1.1 and a RoomEnvironment envMap.

**Owner decisions to hold:** title AC VR Hub; ARCADE → AC → AC VR HUB sign; blank multicoloured marquees; one gun at the gun stop; honest status wording (statuses follow `docs/status/current.md`, E1/E2/E3); no real arcade chain's name, logos or photos; the arcade is a backdrop to the panels.

**Captures** (`captures/`, 1280×800 viewport shown at 800×500 unless named `p*` = 375×812 phone): `d0-welcome` … `d8-where-it-stands` are the current build at each section; `d2-bench` the model bench; `scene-stop1-before-panels.jpg` and `scene-stop3-ghost-before-panels.jpg` show the same scene before the panel layout (what the panels now cover); `models-testbench.jpg` shows six models under neutral lighting.

**Files (relative to the repository root):** `website/index.html`, `website/style.css`, `website/arcade.js`, `website/models.js`, `website/bench.js`, `website/page.js`, `website/models/models.json`, `website/tools/sync_models.py`. Live preview: https://preview.ac-vr-hub.pages.dev (fetch as text if useful; do not drive a browser or capture windows).

**Limits:** source/static review plus captures. No GPU profiling or device test. Label claims observed, inferred or hypothesis.

**Report contract:** `studio-experts/references/work-contract.md` ("Review report"): ranked findings with severity, exact file:line or capture, visible effect, a concrete Three.js/CSS remedy. 500–1,500 words. Read `studio-experts/references/compatibility.md` first (no installs, servers, publishing or account access).
