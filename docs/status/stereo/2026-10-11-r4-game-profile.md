# R4 native game profile — 2026-10-11

## Done and verified

Published candidate: [native profile](https://github.com/LevonFrench/AladdinsCastle/blob/codex/r4-native-game-profiles/data/profiles/dolphinxr/GameSettingsVR/RBUP08.ini), with [installation/evidence notes](https://github.com/LevonFrench/AladdinsCastle/blob/codex/r4-native-game-profiles/data/profiles/dolphinxr/README.md). Reserved Lead candidate copied unchanged; seven enabled native PS shader-family blocks match the observed hashes and family signatures. Native format version 2 matches an actual DolphinXR hunter save. Public privacy and whitespace checks pass. No assets, saves, captures, game binaries or library paths are included.

The ordered fixed-scene experiment found `VirtualScreen=False` removes the inner window and duplicated bands. Native Screen overrides for labels, digits, health fill, weapon/grenade icons and both reticle components preserve the continuous world while assembling a smaller HUD panel. This was visually checked in both-eye desktop mirrors at 1x using the null session. Private experiment evidence remains private; no runtime was initiated by Stereo.

The source supports explicit Screen overrides independently of the automatic Virtual Screen setting: [VertexManagerBase](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/VertexManagerBase.cpp), [GeometryShaderManager](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/GeometryShaderManager.cpp). Native profile persistence: [ShaderHunter](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/ShaderHunter.cpp).

## Not verified / next

4x was prepared but not run. Later scenes, other regions, real headset motion, native controller tracking, aim and performance remain unverified. The measured DSU ranges use the same Cursor/EmulatePoint yaw, pitch and signed vertical-offset settings as native Source 3, but native additionally supplies physical controller-to-screen distance. That prevents treating the desktop calibration as native headset acceptance; controls remain separate from this rendering profile.

Owner has paused all GPU/desktop/VR runtime. Source and CPU work continue; no heavy build overlaps Lead's occupied CPU slot. Darkside is next for source/profile preparation, with runtime deferred until GPU availability and a shared service slot. Lead owns integration and the project Agency brief.
