# Pinball isolation guidance

The read-only pinned-tag audit found three unresolved launch conditions:
preference setup can write before command parsing, scores/user data can be
stored beside tables, and script/ROM/plugin outputs are not confined. Ini,
TableIni, PrefPath, CacheMode and per-plugin settings cover only particular
outputs. No safe all-sinks future launch plan is available.

The Hub detail page and diagnostic JSON now share plain-language help for
those conditions. Desktop/VR plans remain launchAllowed=false, even for a
VR-ready table classified as needing no ROM. Scan and now-playing behavior
are unchanged. Developer docs cite the immutable VPX source and correct the
FileLocator links to src/core.

Verification pending independent source/build-plan review and a CPU slot:
Python fixture syntax, diff checks and the public privacy scan pass. No Hub
binary or C++ build has run at this head. Synthetic plan checks require
all blocker reasons, unchanged fixture bytes and no session files; the real
QML scene checks matching help in VR/desktop. No PowerShell is changed. No VPX
launch/build, collection access, GPU, download or registry operation occurred.

Independent review precedes publication. Portable PR71 at frozen bf29239 passed
all six final hosted checks and is integrated; its separate private runner is
parked after two failed attempts, with
untested diagnosis repairs preserved. Parent owns integration and acceptance.
