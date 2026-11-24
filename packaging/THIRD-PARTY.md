# Third-party components

Kalahari is free software under the MIT License (`LICENSE`). Its packages carry the libraries
below; each keeps its own licence. The texts of the licences are in the `licenses` folder of
the program (Windows and macOS: one file per package of vcpkg the program is built with; Linux:
`share/doc/kalahari/licenses`), and the source code of each library is at the address given.

| Component | Used for | Licence | Source |
|---|---|---|---|
| Qt 6 (Core, Gui, Widgets, Svg, SvgWidgets, Network, Xml, Sql, PrintSupport, image format plugins) | the whole user interface | LGPL 3.0 | https://download.qt.io/official_releases/qt/ |
| Python 3 (interpreter and standard library) | the plugins | PSF License 2.0 | https://www.python.org/downloads/source/ |
| pybind11 | the interface of the plugins | BSD 3-Clause | https://github.com/pybind/pybind11 |
| PDFium (pdfium-binaries) | the preview of PDF files | BSD 3-Clause, Apache 2.0 and the licences of its libraries (`licenses/pdfium`) | https://pdfium.googlesource.com/pdfium/, https://github.com/bblanchon/pdfium-binaries |
| Hunspell | the spelling check | MPL 1.1 / GPL 2.0 / LGPL 2.1 (LGPL 2.1 applies) | https://github.com/hunspell/hunspell |
| spdlog, fmt | the log | MIT | https://github.com/gabime/spdlog, https://github.com/fmtlib/fmt |
| nlohmann/json | settings and book files | MIT | https://github.com/nlohmann/json |
| libzip, zlib | book archives, packages of book types and plugins | BSD 3-Clause, zlib | https://libzip.org/, https://zlib.net/ |
| libcurl | downloads | curl License | https://curl.se/ |
| OpenSSL 3 | signatures of plugins, secure connections | Apache 2.0 | https://www.openssl.org/source/ |
| Microsoft Visual C++ runtime (Windows only: vcruntime140, msvcp140, concrt140) | the program and its libraries | the licence terms of Visual Studio for its distributable code | https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files |
| Polish dictionary (sjp.pl, LibreOffice) | the spelling check | Apache 2.0 (`resources/dictionaries/LICENSE_pl_PL.txt`) | https://sjp.pl/sl/en/ |
| English dictionary (SCOWL) | the spelling check | SCOWL licence (`resources/dictionaries/LICENSE_en_US.txt`) | http://wordlist.aspell.net/ |

Qt and Hunspell are linked as shared libraries on Windows and Linux, so they can be replaced
with other builds of the same version. On macOS vcpkg links them statically into the program;
the complete source code of Kalahari (https://github.com/bartoszwarzocha/kalahari) and its
build instructions (`BUILDING.md`) let anyone build it again with a changed Qt or Hunspell, as
the LGPL requires.

The Linux packages also carry libraries of Ubuntu 24.04 that the program and Qt need; their
licences are those of the Ubuntu packages they come from (`/usr/share/doc/<package>/copyright`
on Ubuntu).
