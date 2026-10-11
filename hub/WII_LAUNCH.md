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

The lead owns authored DolphinXR recipes/catalog edits. The Hub accepts a
non-planned authored variant with a pinned `launch.exe_sha256`, string executable,
argument list and explicit runtime (`none`, `openxr` or `steamvr`). The executable
and working directory must stay inside the managed installation. Admission checks
the exact installed version, recipe hash, ownership receipt and current executable
hash, plus only that variant's required media/tools. Missing required files refuse
launch; stale unrelated tools cannot block it. One explicit `--user` must point to
the installation's existing `User` directory. No ordinary portable User or NAND is
copied into the authored route.

The real detail page exposes install/play/retry for this contract. A fresh Hub
restores verified authored receipts and selects the installed variant; the default
launcher uses it. Unsupported legacy authored recipes retain their refusal gate.
`hubtool plan-launch <game> <variant>` with `--data-root`, `--install-root` and
`--bindings` checks admission and prints the launch plan without starting a runtime.
Steam shortcut support remains limited to generated routes.

Recipes may seed configuration with `write-config only_if_missing = true`.
Fresh files are created normally; existing files skip the config merge and old seed
ownership is relinquished, so repair/update cleanup and uninstall retain them.
The generic update path still stages existing user files; this is not a
`preserve` glob implementation. Omitting the flag retains the existing merge
behavior. The flag requires an explicit boolean.

Both lead Wii recipes dry-plan successfully, and their launch preflight refuses
until the exact owned setup has actually been installed. Synthetic software tests
cover positive admission, identity/version/ownership refusal, missing requirements,
restart selection, real QML install/play requests and config retention. These
checks do not establish an installed DolphinXR launch or headset acceptance.
Experimental true3d metadata does not establish target acceptance. Flat, stereo
theatre and true3D remain distinct. Exact owner paths and disc observations,
recipe dry-plans and captures stay private under `.local/`; no disc or Nintendo
system file enters the repository.
