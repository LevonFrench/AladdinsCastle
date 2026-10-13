# Draft DolphinXR title baselines

These five **DRAFT** upstream-format profiles preserve eight explicit VR values
from observed title or start screens in both eyes. They are untuned comparison
seeds for review, with no shader, texture or element overrides.

| Profile | Region | Observed title picture |
|---|---|---|
| [R2VE01.ini](R2VE01.ini) | USA | Readable title; displaced strips and overlapping layers |
| [RHOP8P.ini](RHOP8P.ini) | Europe | Readable title and start prompt |
| [RZJE69.ini](RZJE69.ini) | USA | Very small, dark start panel |
| [RZPP01.ini](RZPP01.ini) | Europe | Readable title panel and room background |
| [SQDE8P.ini](SQDE8P.ini) | USA | Readable title and start prompt |

Each seed retains the observed `VirtualScreen=True`, screen size/distance1.5,
zero HUD thickness, disabled anchors, nonflat rendering and disabled CPU culling.
It leaves ordinary shipped game settings inherited, including their CPU, copy and
texture-cache requirements. The values do not establish a fix for the two visible
faults or a performance recommendation.

This directory is outside `GameSettingsVR`, the directory used by create-only
profile seeding. Adding these drafts does not install them or alter existing
profiles, pins or installers. Effective per-game admission, usable gameplay,
comfortable world scale, other scenes and physical aiming remain to be checked
before a reviewed profile can move into active seeding.

The [source proposal report](../../../../docs/status/framerate/2026-10-12-title-profile-drafts.md)
describes the shared values and source limits. Both-eye title visibility does not
establish quiet frame rate or unchanged game speed.
