# Draft title profile proposals

Five authorized title checks produced directly inspected start or title pictures
in both eyes. Five **DRAFT** per-game VR profile seeds preserve the settings used
for those pictures. They add no shader, texture or element overrides and remain
outside active profile seeding.

| Catalog game | Observed title picture | Profile decision |
|---|---|---|
| wii-dead-space-extraction | Very small, dark start panel | Preserve baseline as a comparison seed; scale fix unresolved |
| wii-gunblade-ny-la-machineguns | Readable title and start prompt | Preserve baseline; no justified override |
| wii-house-of-the-dead-overkill | Readable title and start prompt | Preserve baseline; no justified override |
| wii-links-crossbow-training | Readable title panel with a room background | Preserve baseline; no justified override |
| wii-sin-and-punishment-star-successor | Readable title with displaced strips and overlapping layers | Preserve baseline as a comparison seed; routing fix unresolved |

The shared proposal uses the upstream per-game section, rather than copying the
global GFX.ini section name:

```ini
[Graphics.VR]
FlatScreen = False
VirtualScreen = True
DisableCPUCull = True
ScreenDistance = 1.5
ScreenSize = 1.5
HudThickness = 0.0
EnableControllerAnchor = False
EnableCameraAnchor = False
```

These values were present during the observed title checks. Moving them into a
per-game profile still needs an effective-settings readback during a later booked
run. The drafts preserve inherited ordinary game settings, including existing
CPU, copy and texture-cache settings. They do not introduce codes, clock changes,
new world scale, or unidentified draw signatures.

The pinned [profile parser](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/ConfigManager.cpp#L134)
merges shipped profiles before User profiles and applies supported keys to the
current run. These proposals remain in
[data/profiles/dolphinxr/drafts](../../../data/profiles/dolphinxr/drafts/README.md),
pending Lead and 3D review, outside active User and Sys directories. No installer,
pin or create-only seeding behavior changes.

Native screen size and distance control the shared plane; they do not identify the
cause of a small title panel. Pane and clipping switches depend on the actual
projection and viewport classification, which the title pictures do not provide.
The pinned [automatic EFB-effect route](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/VertexManagerBase.cpp#L1744)
requires `VirtualScreen=True`; it cannot change routing with that switch off.
None of these source facts justifies adding an unidentified draw override to a
draft profile.

Title visibility does not establish playable-scene quality, comfortable world
scale, physical aiming, or performance. Quiet Off/Auto/90 and the separate code
comparison remain queued for their own authorized stretch.
