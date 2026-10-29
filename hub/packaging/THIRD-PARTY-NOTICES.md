# Portable Hub notices

The Hub is GPL-3.0-only. Its full license is LICENSE-GPL-3.0.txt. The corresponding
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
- Project-local libacvr CPU hand-assembly implementation (MIT): licenses/libacvr-MIT.txt
  Source and header byte receipts: licenses/libacvr-source.txt; application source
  commit: BUILD-INFO.txt. The Hub uses this CPU implementation for metadata
  validation and does not link the OpenXR/GL runtime. Its translation unit also
  defines other CPU helpers; no stripping claim is made here.
- toml++ v3.4.0 (MIT): licenses/tomlplusplus-MIT.txt
  https://github.com/marzer/tomlplusplus/tree/v3.4.0
- nlohmann/json v3.12.0 (MIT): licenses/nlohmann-json-MIT.txt
  https://github.com/nlohmann/json/tree/v3.12.0
- json-schema-validator 2.4.0 (MIT): licenses/json-schema-validator-MIT.txt
  https://github.com/pboettch/json-schema-validator/tree/2.4.0
- VPin Studio's VPS table matcher (MIT, Copyright (c) 2022 syd711), ported to C++ in
  hub/src/core/pinball/VpsMatch.cpp through Visual Pinball Infinite's Rust port:
  licenses/VPinStudio-MIT.txt; https://github.com/syd711/vpin-studio and
  https://github.com/VPinInfinite/VisualPinballInfinite/blob/main/engine/library/src/matcher.rs
- OpenVR v2.15.6 (BSD-3-Clause): licenses/OpenVR-BSD-3-Clause.txt
  https://github.com/ValveSoftware/openvr/tree/v2.15.6
- 7-Zip 25.01 C metadata reader (public domain): licenses/7zip-C-public-domain.txt
  https://github.com/ip7z/7zip/tree/5e96a8279489832924056b1fa82f29d5837c9469/C
  Only the C SDK is linked. Its archive-header decoder does not extract game members.
- libchdr sparse CHD reader (BSD-3-Clause), pinned607694ca0812edfc9cc2030c64634fc2393668de:
  licenses/libchdr-BSD-3-Clause.txt; https://github.com/rtissera/libchdr/tree/607694ca0812edfc9cc2030c64634fc2393668de
  Bundled LZMA26.02 (public domain), miniz3.1.2 (MIT), zstd1.5.7 (BSD-3/GPL),
  dr_flac (its bundled notices) are preserved under licenses/libchdr-*.txt.
  Only sparse identifying sectors are decoded; no disc extraction or full hashing.
- Bundled PS2 serial/title metadata facts: official PCSX2 GameIndex, GPL-3.0 project
  https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/bin/resources/GameIndex.yaml
  Source URL and exact revision are retained on every mapping; no game data is included.

The Microsoft Visual C++ runtime DLLs come only from the official Visual Studio
VC/Redist/MSVC x64 CRT directory, subject to Microsoft's redistribution terms.
They are not copied from Windows/System32. The VS 2022 runtime retains the
msvcp140.dll name; the earlier architecture example msvcp170.dll is incorrect.

The archive backend statically links the following project-local sources;
their unmodified upstream licence texts are preserved with the package:
- libarchive 3.8.9: licenses/libarchive-COPYING.txt
  https://github.com/libarchive/libarchive/tree/v3.8.9
- zlib 1.3.1: licenses/zlib-LICENSE.txt
  https://github.com/madler/zlib/tree/v1.3.1
- XZ Utils liblzma 5.8.1 (0BSD): licenses/xz-COPYING.txt and the full 0BSD text below
  https://github.com/tukaani-project/xz/tree/v5.8.1

The bundled Archivo 2.001 fonts are by the Archivo Project Authors / Omnibus-Type,
under SIL Open Font License 1.1. Their copyright and full terms are embedded in
the Hub at :/fonts/OFL.txt and reproduced below. Source and font-instance changes
are documented in the source tree at hub/resources/fonts/README.md.
https://github.com/Omnibus-Type/Archivo

## XZ Utils liblzma 5.8.1 full licence

The archive backend links liblzma; XZ command-line programs are disabled in this
build. The upstream COPYING file is a component licence map. The additional full
0BSD text from https://github.com/tukaani-project/xz/blob/v5.8.1/COPYING.0BSD
is reproduced here so it travels with this notice in the portable package:

```text
Permission to use, copy, modify, and/or distribute this
software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```

## Optional models

An explicitly staged native gun-room package may include models with separate
asset licences. Preserve the included assets/guns/LICENSE and
assets/hands/LICENSE. The adapted arc-light-gun and its previews are CC BY 4.0:
"Namco Arcade Gun" by Martoscar,
https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be,
https://creativecommons.org/licenses/by/4.0/.
The hotd2-vr conversion re-oriented, quantised and recoloured it and dropped its
textures. Project changes remove lettering, split the slide and trigger, replace
sights, change the coordinate frame, simplify to two LODs and use four materials.
The full change record is data/guns/arc-light-gun.toml. This asset is not CC0.

## Archivo font copyright and full licence

The source file hub/resources/fonts/OFL.txt is reproduced below (trailing whitespace omitted):

```text
Copyright 2020 The Archivo Project Authors (https://github.com/Omnibus-Type/Archivo)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
http://scripts.sil.org/OFL


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded,
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```
