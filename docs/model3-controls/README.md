# Model 3 cabinet controls

Original cabinet models, per-game VR control sets and light/dark vector choice pictures for every Model 3 title currently in the catalog.

- [Cabinet families and game differences](families.md)
- [Step 1: Scud Race / Daytona USA 2](step-1.md)
- [Step 2: rally, sequential racers and motorcycle](step-2.md)
- [Step 3: pistol and mounted gun families](step-3.md)
- [Step 4: ski and handcar cabinets](step-4.md)
- [Step 5: emulator handoff](emulator-handoff.md)

Per-game native cabinet source lives in `data/controls/cabinets/games/`; shared choices in its parent directory. Existing generic gun mappings are untouched. Source authoring uses the existing legacy cabinet element/output/extractor and hold tables, with per-game defaults, full range/direction and exact Supermodel input names. Model motions/label anchors are in `data/controls/models/`; original GLBs/scripts/cards in `assets/controls/`. Scripts/reference maths are GPL-3.0-only; built assets and vector art CC0-1.0.

The final meshes were inspected in CPU renders. First lighting overexposure and floating pedal treads were corrected, as was a preview-only reused gun material tint. Six wheel/gate/pedal mathematics tests pass. No validators, supervisors, fixtures, downloads, installs, screen captures, games or headset runs were used. Renders remain outside Git. The optional SVG-to-PNG converter CairoSVG was unavailable; native SVG choice pictures are supplied. No flight-stick or ordinary arcade-stick family is assigned because none is present in this catalog.

This delivers source models, control data and pictures. The live emulator still needs to consume those data and draw/drive the controls; this task does not claim rendered hands, controller feel or gameplay acceptance.
