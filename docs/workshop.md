# Workshop: open decisions

When a decision is settled, record it here with the date and update the docs it affects.

| # | Decision | Options | Recommendation | Status |
|---|---|---|---|---|
| D1 | Default experience | True 3D vs virtual screen | True 3D first, theatre as fallback | **Decided 2026-10-08 (owner)** |
| D2 | Genres | Gun only vs gun + racing | Gun + racing; ghost controls, triggers as pedals | **Decided 2026-10-08 (owner)** |
| D3 | Licence | GPL-3.0 / MIT | GPL-3.0 | **Decided 2026-10-08 (owner)** |
| D15 | Front end shape | VR lobby / desktop installer-launcher | Desktop Hub modelled on PCVR Mods Installer Hub, installing our own setups; no VR lobby | **Decided 2026-10-08 (owner)** |
| D4 | Hub technology | Tauri 2 / .NET (WPF, Avalonia) / PowerShell + batch / Godot desktop | Tauri 2 (portable exe, web UI for the art library, Rust for installer work) | Open |
| D5 | Config format | TOML / YAML / JSON | TOML canonical, JSON accepted; maybe YAML for PenguinScreen2 interop | Open |
| D6 | Process model for our setups | Engine + libacvr in one process / separate VR process with shared textures | One process (as DR-89 does); shared-memory seqlock only where a hook DLL is unavoidable | Open |
| D7 | First setup of our own | namco22-vr / supermodel-vr / pcsx2-vr | namco22-vr: MIT, C, covers gun + racing, proven in VR by DR-89 | Open |
| D8 | First racing setup | Rave Racer / Ace Driver / Scud Race / Daytona | Rave Racer | Open |
| D9 | VR graphics API | Vulkan / D3D11 / OpenGL | Test against Quest Link, VDXR and SteamVR first. DR-89 uses OpenGL; Vulkan needed for Quest standalone. | Open |
| D10 | Light gun device strategy for theatre | Emulator APIs / vJoy / absolute mouse / custom VHF driver | Emulator APIs first; no kernel driver in v1 | Open |
| D11 | Relationship with DR-89 and namco22-decompile | Fork / contribute upstream / install their releases | Install their releases via recipes now; build namco22-vr as guarded patches on upstream; contact the authors | Open |
| D12 | Linux / SteamOS | v1 / later | Later; keep code portable | Open |
| D13 | Name / trademark | Keep "AladdinsCastle" / rename before store release | Keep for open source; clear before any store release | Open |
| D16 | In-VR game switching | SteamVR library shortcuts only / plus a flat in-VR list panel | SteamVR shortcuts in v1; revisit later | Open |

## Questions for the owner

1. After playing DR-89's Time Crisis VR: grip cover or physical ducking as default? Laser on or off?
2. Racing: seated only, or standing as well? Should the ghost controls be always visible, or appear when a hand comes near?
3. Online play eventually? namco22-decompile already has netplay for Rave Racer and Cyber Sled.
4. Should the Hub also install the emulators themselves (from official sources), or expect them already installed?
