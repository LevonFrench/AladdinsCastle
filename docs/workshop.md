# Workshop: open decisions

Decisions that shape the project. Each has a recommendation. When one is settled, record the answer and the date here and update the docs it affects.

| # | Decision | Options | Recommendation | Status |
|---|---|---|---|---|
| D1 | Default experience | True 3D vs virtual screen | True 3D first, theatre as fallback | **Decided 2026-10-08 (owner)** |
| D2 | Genres | Gun only vs gun + racing | Gun + racing, with racing controls as grabbable ghost controls and analog triggers as pedals | **Decided 2026-10-08 (owner)** |
| D3 | Licence | GPL-3.0 / MIT | GPL-3.0 | **Decided 2026-10-08 (owner)** |
| D4 | Hall engine | Godot 4 / Unity / custom C++ | Godot 4 + C++ GDExtension | Open |
| D5 | Config format | TOML / YAML / JSON | TOML canonical, JSON accepted. Possibly YAML for PenguinScreen2 interop. | Open |
| D6 | Compositing model | Hall owns the XR session and composites backend eyes (ACBP) / each backend owns the headset | ACBP for our backends; hand-off only for third-party VR apps | Open |
| D7 | First ACBP backend | namco22-vr / supermodel-vr / pcsx2-vr | namco22-vr: MIT, C source, covers gun + racing, and already proven in VR by DR-89 | Open |
| D8 | First racing slice | Rave Racer / Ace Driver / Scud Race / Daytona | Rave Racer (decomp, playable, 2-position shifter, online play upstream) | Open |
| D9 | PC graphics API | Vulkan / D3D11 / D3D12 | Pick after an interop test against Quest Link + VDXR + SteamVR | Open |
| D10 | Light gun device strategy for theatre | Emulator APIs / vJoy / absolute mouse / custom VHF driver | Emulator APIs first; no kernel driver in v1 | Open |
| D11 | Relationship with DR-89 and namco22-decompile | Fork / contribute upstream / depend on releases | Depend as a submodule, contribute upstream, keep the VR host in our repo; contact the authors | Open |
| D12 | Linux / SteamOS | v1 / later | Later; keep code portable | Open |
| D13 | Name / trademark | Keep "AladdinsCastle" / rename before store release | Keep for the open-source project; clear the trademark before any store release | Open |
| D14 | Attract mode | Videos / live backend / still art | Videos when the user has them, then still art; live backend as an option | Open |

## Questions for the owner

1. After playing DR-89's Time Crisis VR: grip cover or physical ducking as default? Laser on or off?
2. For racing: seated only, or standing as well? Should the ghost controls be fully visible, or only appear when a hand comes near?
3. Do you want online play in scope eventually? namco22-decompile already has netplay for Rave Racer and Cyber Sled.
4. Should the hall have a theme (Aladdin's Castle 1980s mall arcade) or a neutral modern room, with theming done in packs?
