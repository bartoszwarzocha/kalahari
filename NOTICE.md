# Kalahari – copyright, licence and additional terms

Kalahari Writer's IDE
Copyright (C) 2025-2026 Bartosz W. Warzocha, BW Digital Systems & Publishing – Bartosz Warzocha
Created by BW Digital Systems & Publishing – https://bwdsp.com.pl/

Kalahari is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License version 3 as published by the Free Software Foundation, with the
additional terms below, which its section 7 permits.

Kalahari is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

The text of the licence is in `LICENSE` (in the packages: `LICENSE.txt` or `LICENSE`); the
program shows it in Help > About Kalahari, on the License tab. It is also at
https://www.gnu.org/licenses/gpl-3.0.html.

All components of Kalahari – the application, the plugin system and all official plugins,
present and future – are free and released under this licence. The source code of Kalahari is
at https://github.com/bartoszwarzocha/kalahari; the source of each release is under its tag
(`v0.3.0-alpha`, ...).

SPDX identifier: `GPL-3.0-only` (each source file names it, with a pointer to this file).

---

## Additional terms (section 7 of the GNU GPL version 3)

These terms apply to the whole of Kalahari, to each of its source files, and to every work based
on it.

### 1. Author attribution – section 7(b)

Every copy of Kalahari, and every work based on it, that is conveyed (distributed) must keep this
author attribution in the Appropriate Legal Notices that it displays – in Kalahari, the About
window (Help > About Kalahari) – unchanged, clearly visible and with its address:

    Kalahari – created by BW Digital Systems & Publishing, https://bwdsp.com.pl/

The attribution may be shown in the language of the user interface. A modified version keeps it
in its own About window or, when it has none, in the place of its user interface that shows its
legal notices; it may add that it is a modified version, and by whom it was modified.

### 2. The names and the logos – section 7(e)

This licence grants no rights under trademark law to the name "Kalahari" or to the logo of
Kalahari, nor to the name "BW Digital Systems & Publishing" or to its logo (the files
`resources/images/bwdsp_logo_black.png` and `resources/images/bwdsp_logo_white.png`). A modified
version, or any other product, may not use them as its name or its logo without the permission
of the copyright holder; it may be distributed under a name of its own. Saying that a work is
based on Kalahari, and naming its authors, as the attribution of term 1 does, is not such a use.

For permissions concerning the names and the logos, contact: bartosz.warzocha@gmail.com

---

## Third-Party Dependencies

Kalahari uses the following third-party libraries and frameworks. Each has its own license, listed below.

### Qt Framework - LGPL v3

Kalahari uses the **Qt 6.4+** framework (builds use Qt 6.9) under the GNU Lesser General Public
License version 3 (LGPL v3). The modules it uses are also available under the GNU GPL version 3.

**LGPL v3 compliance:**
- On Windows and Linux Qt is linked dynamically: users can replace its libraries with
  compatible builds of the same version.
- On macOS vcpkg links Qt statically into the program; the complete source code of Kalahari
  and its build instructions (`BUILDING.md`) let anyone build it again with a changed Qt.
- Qt source code is available from the official Qt website.

**Links:**
- Qt Homepage: https://www.qt.io/
- Qt Downloads: https://download.qt.io/official_releases/qt/
- Qt Licensing: https://www.qt.io/licensing/
- LGPL v3 Full Text: https://www.gnu.org/licenses/lgpl-3.0.html

**Qt Components Used:**
- Qt6::Core - Core non-GUI functionality
- Qt6::Widgets - Widget classes for GUI
- Qt6::Gui - Base GUI classes (QPainter, QImage, etc.)
- Qt6::Svg and Qt6::SvgWidgets - SVG icons, pictures and previews
- Qt6::Network - Downloads of icons and materials, the grammar server
- Qt6::Xml - SVG icon conversion
- Qt6::Sql - The project database (SQLite)
- Qt6::PrintSupport - The print of the book (on Windows also its private API, for the window
  of the printer's driver)
- Qt6::TextToSpeech - Only when the module is present at build time
- Qt Image Formats plugins (TIFF, WebP) - Pictures of the books, on vcpkg builds

**Note:** If you redistribute Kalahari, include the Qt libraries it is built with and inform
users they can replace them. See LGPL v3 for full requirements.

---

### spdlog - MIT License

**Homepage:** https://github.com/gabime/spdlog
**License:** MIT License
**Usage:** Logging system

Copyright (c) 2016 Gabi Melman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

### nlohmann_json - MIT License

**Homepage:** https://github.com/nlohmann/json
**License:** MIT License
**Usage:** JSON serialization/deserialization

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

### libzip - BSD 3-Clause License

**Homepage:** https://libzip.org/
**License:** BSD 3-Clause License
**Usage:** .klh file format (ZIP container)

Copyright (c) 1999-2023 Dieter Baron and Thomas Klausner

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

---

### Catch2 - BSL-1.0 License

**Homepage:** https://github.com/catchorg/Catch2
**License:** Boost Software License 1.0
**Usage:** Unit testing framework

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

---

### Python 3.11 - PSF License

**Homepage:** https://www.python.org/
**License:** Python Software Foundation License
**Usage:** Plugin system (embedded interpreter)

Copyright (c) 2001-2023 Python Software Foundation. All rights reserved.

Python is licensed under the PSF License Agreement, which is GPL-compatible
but not copyleft. See full license at:
https://docs.python.org/3/license.html

---

### pybind11 - BSD 3-Clause License

**Homepage:** https://github.com/pybind/pybind11
**License:** BSD 3-Clause License
**Usage:** C++/Python bindings

Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

---

### OpenSSL - Apache License 2.0

**Homepage:** https://www.openssl.org/
**License:** Apache License 2.0 (OpenSSL 3.0 and later)
**Usage:** Verification of plugin signatures (Ed25519) and SHA-256 hashes; linked dynamically

---

### Hunspell - MPL 1.1 / GPL 2.0 or later / LGPL 2.1 or later

**Homepage:** https://hunspell.github.io/
**License:** Mozilla Public License 1.1, GNU GPL 2.0 or later or GNU LGPL 2.1 or later
(tri-license); Kalahari uses it under the **LGPL 2.1 or later**, compatible with the GPL v3
(the MPL 1.1 is not)
**Usage:** Spell checking

---

### Spelling dictionaries

**Location:** `resources/dictionaries/` (licenses and sources in its `README.md`)
**Usage:** Polish and English (US) spell checking

- **Polish (pl_PL)** - word list of the sjp.pl project, copyright Marek Futrega, from
  the LibreOffice dictionaries. Offered under GPL, LGPL, MPL, Apache 2.0 or Creative
  Commons ShareAlike; Kalahari uses it under the **Apache License 2.0**
  (`LICENSE_pl_PL.txt`). Converted from ISO 8859-2 to UTF-8, otherwise unchanged.
- **English (en_US)** - SCOWL by Kevin Atkinson and contributors, under the SCOWL
  license, a permissive BSD-like license (`LICENSE_en_US.txt`). Unchanged.

---

### Material Icons - Apache License 2.0

**Location:** `resources/icons/` (the filled, outlined, rounded and twotone styles)
**Homepage:** https://github.com/google/material-design-icons
**License:** Apache License 2.0 - Material Icons by Google
**Usage:** Icons of the menus, toolbars and panels

The SVG files are changed: their colors are placeholders that the program fills with
the colors of the theme (`scripts/convert_all_icons.py`).

---

### PDFium - BSD 3-Clause / Apache 2.0

**Homepage:** https://pdfium.googlesource.com/pdfium/ (the PDF library of Chromium)
**Build:** pdfium-binaries by Benoit Blanchon (MIT License),
https://github.com/bblanchon/pdfium-binaries – a shared library built by GitHub Actions
from the official sources, without JavaScript (V8) and XFA; the release, its version and
the SHA-512 of each archive are pinned in `vcpkg-ports/pdfium/release.cmake`, and the CI
checks the signed record of each build (`scripts/verify_pdfium.cmake`)
**License:** BSD 3-Clause License (Copyright 2014 The PDFium Authors) and Apache License 2.0;
linked dynamically
**Usage:** The preview of PDF files, the first page of a PDF on the card of its material and
"Fill In from File"

PDFium contains these libraries, each under its own permissive license:

| Library | License |
|---------|---------|
| Abseil | Apache 2.0 |
| Anti-Grain Geometry 2.3 | AGG license (permissive) |
| Dragonbox | Apache 2.0 with LLVM Exceptions or Boost Software License 1.0 |
| fast_float | MIT |
| FreeType | FreeType Project License |
| HarfBuzz | "Old MIT" |
| Highway | Apache 2.0 |
| ICU | Unicode License V3 |
| Little CMS | MIT |
| libjpeg-turbo | IJG License, BSD 3-Clause and zlib |
| OpenJPEG | BSD 2-Clause |
| libpng | PNG Reference Library License version 2 |
| LLVM libc | Apache 2.0 with LLVM Exceptions |
| simdutf | MIT |
| zlib | zlib License |

The full texts come with the library: `share/pdfium/copyright` of the vcpkg port, or
`share/pdfium/LICENSE` and `share/pdfium/licenses/` of a build without vcpkg. They go with
every distribution of the library.

---

## Summary of Licenses

| Dependency | License | Type | Compatible with GPL 3.0 |
|------------|---------|------|-------------------------|
| Kalahari (Application) | GPL 3.0 with additional terms (section 7) | Copyleft | – |
| Qt 6.4+ | LGPL v3 (also GPL v3) | Weak Copyleft | ✅ |
| spdlog | MIT | Permissive | ✅ |
| nlohmann_json | MIT | Permissive | ✅ |
| libzip | BSD 3-Clause | Permissive | ✅ |
| Catch2 (tests only) | BSL-1.0 | Permissive | ✅ |
| Python 3 | PSF | Permissive | ✅ |
| pybind11 | BSD 3-Clause | Permissive | ✅ |
| OpenSSL | Apache 2.0 | Permissive | ✅ (GPL 3.0, not 2.0) |
| Hunspell | LGPL 2.1 or later (chosen of MPL 1.1 / GPL 2.0+ / LGPL 2.1+) | Weak Copyleft | ✅ |
| PDFium (with the libraries it contains) | BSD 3-Clause / Apache 2.0 (and permissive licenses of its libraries) | Permissive | ✅ |
| Polish dictionary (pl_PL) | Apache 2.0 (chosen of five) | Permissive | ✅ |
| English dictionary (en_US) | SCOWL (BSD-like) | Permissive | ✅ |
| Material Icons | Apache 2.0 | Permissive | ✅ |

**All dependencies are compatible with the GNU GPL version 3.** The parts under the Apache
License 2.0 (OpenSSL, PDFium, Material Icons, the Polish dictionary) are compatible with version
3 of the GPL only, not with version 2.

---

## Important Notes for Redistributors

### If you distribute Kalahari binaries:

1. **Include `LICENSE` (the GNU GPL version 3) and this file** in your distribution.

2. **Provide the source code** (section 6 of the GPL): the source of the version of Kalahari you
   distribute, or a clear pointer to where it is (its tag at
   https://github.com/bartoszwarzocha/kalahari), and the source of Qt and Hunspell or where it
   is (see above).

3. **Keep the author attribution** of the About window (additional term 1).

4. **Include the Qt libraries** (.dll on Windows, .so on Linux) and inform users they can replace
   them (LGPL v3). With the preview of PDF files, also the PDFium library (pdfium.dll,
   libpdfium.so or libpdfium.dylib) and the texts of its licenses (see PDFium above).

### If you modify Kalahari:

- Your modified version is a work based on Kalahari: when you distribute it, it is under the GNU
  GPL version 3 with the additional terms above, and its source code goes with it.
- Mark your changes (section 5(a) of the GPL) and keep the attribution of term 1.
- Distribute it under a name of its own (term 2).
- If you modify **Qt itself**, you must release Qt modifications under LGPL v3.

---

## Compliance Verification

To check how Qt is linked:

```bash
# Linux - Check shared libraries
ldd ./bin/kalahari | grep Qt

# macOS - Check libraries (vcpkg links Qt statically: no Qt in the list)
otool -L ./bin/kalahari | grep Qt

# Windows - Check DLLs (PowerShell)
Get-ChildItem -Path . -Recurse -Filter "Qt6*.dll"
```

---

**Last Updated:** 2026-11-25
**Kalahari Version:** 0.4.0-alpha
**Document Version:** 2.0
