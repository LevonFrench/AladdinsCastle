# Portable Hub notices

The Hub is GPL-3.0. Its full license is LICENSE-GPL-3.0.txt. The corresponding
application source is https://github.com/LevonFrench/AladdinsCastle; binary release
publishers must identify the exact source commit and preserve access to that source.

Qt 6.8.3 is dynamically linked; its libraries remain replaceable. The applicable
LGPL-3.0 and GPL-3.0 texts are included in licenses/Qt-LGPL-3.0.txt and
LICENSE-GPL-3.0.txt. Replacing Qt may require compatible Qt plugins too.
The application does not restrict reverse engineering needed to debug modifications
to those libraries. No game content or third-party game art is bundled.

Qt's exact source, including module licenses and third-party attribution files:
- https://download.qt.io/archive/qt/6.8/6.8.3/single/
- https://github.com/qt/qtbase/tree/v6.8.3
- https://github.com/qt/qtdeclarative/tree/v6.8.3
- https://github.com/qt/qtshadertools/tree/v6.8.3
- https://doc.qt.io/qt-6.8/licenses-used-in-qt.html

The portable script also preserves license directories provided by the installed
Qt distribution and its bundled SBOMs with component metadata. These M1 artifacts are development builds. Before a public binary
release, audit the modules and plugins actually deployed by windeployqt, include
their complete bundled third-party notices and record the source commit.

Pinned additional dependencies and their preserved license texts:
- toml++ v3.4.0 (MIT): licenses/tomlplusplus-MIT.txt
  https://github.com/marzer/tomlplusplus/tree/v3.4.0
- nlohmann/json v3.12.0 (MIT): licenses/nlohmann-json-MIT.txt
  https://github.com/nlohmann/json/tree/v3.12.0
- json-schema-validator 2.4.0 (MIT): licenses/json-schema-validator-MIT.txt
  https://github.com/pboettch/json-schema-validator/tree/2.4.0
- OpenVR v2.15.6 (BSD-3-Clause): licenses/OpenVR-BSD-3-Clause.txt
  https://github.com/ValveSoftware/openvr/tree/v2.15.6

The Microsoft Visual C++ runtime DLLs come only from the official Visual Studio
VC/Redist/MSVC x64 CRT directory, subject to Microsoft's redistribution terms.
They are not copied from Windows/System32. The VS 2022 runtime retains the
msvcp140.dll name; the earlier architecture example msvcp170.dll is incorrect.
