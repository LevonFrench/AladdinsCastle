# Wii discovery and launch

The Hub-owned Wii disc index maps the public regional IDs for Umbrella Chronicles
and Darkside Chronicles to their catalog titles, separately from the generated
PCSX2 serial index. Matching uses the inspected disc header and Wii hardware;
filenames are not identity. This checkpoint supports the raw ISO images supplied
for these two games; it does not add compressed RVZ/WBFS decoding.

Ordinary Dolphin remains a flat route. Its launch plan prepends an explicit
`--user` pointing to a per-game Hub-owned User directory and uses an owned working
directory. A manifest cannot replace that user argument. The original executable,
portable marker and existing User directory remain intact; no config or NAND is
copied. Dolphin's public command parser defines this option:
https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/UICommon/CommandLineParse.cpp

The lead owns authored DolphinXR recipes/catalog edits. The staged contract is
`launch.exe` within the managed installation, `launch.cwd`, explicit owned
`--user`, `--exec` with the verified disc, and `launch.runtime`. Admission must
verify the installed recipe/version and executable identity before invoking the
runtime. Experimental true3d metadata does not establish target acceptance.
Flat, stereo theatre and true3D remain distinct. Exact owner paths and disc
observations, recipe dry-plans and captures are private under `.local/`; no disc or
Nintendo system file enters the repository.
