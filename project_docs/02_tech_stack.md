# Technology Stack

> Comprehensive technical stack for Kalahari - C++20 core with Python plugin system

**Status:** ✅ Finalized
**Last Updated:** 2025-10-24

---

## Overview

Kalahari is built as a **C++ core application** with an **embedded Python plugin system**. This architecture provides:

- **Performance:** Native C++ for responsive GUI and core operations
- **Extensibility:** Python plugins for features, importers/exporters
- **Cross-platform:** wxWidgets provides native look & feel on all platforms
- **Modern:** C++20 features, contemporary build tools

---

## Core Stack

### Language & Compiler

**Language:** C++20

**Minimum Compiler Versions:**
- GCC 10+ (Linux)
- Clang 10+ (macOS)
- MSVC 2019+ / Visual Studio 2019+ (Windows)

**Why C++20?**
- Modern features: concepts, ranges, modules
- Excellent compiler support (mature, stable)
- Long-term maintainability (decades of support)
- Performance critical for real-time text editing
- Native platform integration

**Key C++20 Features Used:**
- `std::optional`, `std::variant` for safer code
- Concepts for template constraints (plugin API)
- Ranges for collection manipulation
- `std::format` (or fmt library as fallback)
- Designated initializers

---

### Build System

**Build Tool:** CMake 3.21+

**Why CMake?**
- Industry standard for C++ projects
- Excellent cross-platform support
- vcpkg integration built-in
- Modern CMake practices (targets, properties)
- IDE support (Visual Studio, CLion, VS Code)

**CMake Features Used:**
- Modern target-based approach
- vcpkg toolchain integration
- Platform detection & configuration
- Test integration (CTest + Catch2)
- Install/package rules (CPack)

**Example CMakeLists.txt structure:**
```cmake
cmake_minimum_required(VERSION 3.21)
project(kalahari VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Find packages
find_package(wxWidgets REQUIRED COMPONENTS core base richtext aui stc webview)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# ... more packages

# Kalahari executable
add_executable(kalahari
    src/main.cpp
    src/core/document.cpp
    # ... more files
)

target_link_libraries(kalahari PRIVATE
    wxWidgets::wxWidgets
    nlohmann_json::nlohmann_json
    # ... more libraries
)
```

---

### Package Manager

**Package Manager:** vcpkg (manifest mode)

**Why vcpkg?**
- Best wxWidgets support (critical dependency)
- Cross-platform (Windows, macOS, Linux)
- Reproducible builds (vcpkg.json manifest)
- Binary caching (fast rebuilds)
- CMake integration (seamless)
- Large library ecosystem

**vcpkg.json (manifest):**
```json
{
  "name": "kalahari",
  "version-semver": "1.0.0",
  "dependencies": [
    {
      "name": "wxwidgets",
      "features": ["richtext", "aui", "stc", "webview"]
    },
    "sqlite3",
    "nlohmann-json",
    "fmt",
    "spdlog",
    "pybind11",
    "python3",
    "libzip",
    "pugixml",
    "curl",
    "openssl"
  ]
}
```

**Alternatives Considered:**
- **Conan:** Faster, but worse wxWidgets support
- **Manual:** Full control, but maintenance nightmare
- **Verdict:** vcpkg is optimal for Kalahari

---

### Testing Framework

**Testing:** Catch2 v3

**Why Catch2?**
- BDD style (readable, Given-When-Then)
- Header-only option (easy integration)
- Less boilerplate than Google Test
- Fast compilation
- CMake/CTest integration
- vcpkg available

**Example Test:**
```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/document.h"

using namespace kalahari::core;

TEST_CASE("Document chapter management", "[document]") {
    Document doc("Test Book");

    SECTION("Adding chapters") {
        auto* ch1 = doc.addChapter("Chapter 1");
        REQUIRE(ch1 != nullptr);
        CHECK(ch1->getTitle() == "Chapter 1");
        CHECK(doc.getChapterCount() == 1);
    }

    SECTION("Removing chapters") {
        auto* ch1 = doc.addChapter("Chapter 1");
        auto* ch2 = doc.addChapter("Chapter 2");
        doc.removeChapter(ch1->getId());
        CHECK(doc.getChapterCount() == 1);
        CHECK(doc.getChapter(0)->getTitle() == "Chapter 2");
    }
}
```

**Test Organization:**
```
tests/
├── unit/               # Unit tests (core classes)
├── integration/        # Integration tests (plugins, file I/O)
└── fixtures/           # Test data (sample .klh files)
```

**Coverage Target:** 70%+ for core, 50%+ for plugins

---

## GUI Framework

### wxWidgets

**Version:** wxWidgets 3.2+

**Why wxWidgets?**
- **Native look & feel** on all platforms
- **Mature & stable** (25+ years of development)
- **Cross-platform** (Windows, macOS, Linux)
- **Rich widgets** (rich text control, AUI docking)
- **Active community**
- **MIT-like license** (compatible with our MIT core)

**Key wxWidgets Components:**
- `wxFrame` - Main application window
- `wxAUI` - Advanced User Interface (dockable panels)
- `wxRichTextCtrl` - Rich text editor widget
- `wxStyledTextCtrl` - Syntax highlighting (for Markdown mode)
- `wxTreeCtrl` - Project navigator
- `wxNotebook`, `wxPanel` - UI containers
- `wxMenuBar`, `wxToolBar`, `wxStatusBar` - Standard UI elements

**wxAUI (Docking System):**
- Dockable panels (project navigator, assistant, stats)
- Perspectives (saveable layouts)
- Drag-and-drop panel repositioning
- Floating panels

**Example wxAUI Setup:**
```cpp
#include <wx/aui/aui.h>

class MainFrame : public wxFrame {
    wxAuiManager m_auiMgr;

public:
    MainFrame() {
        m_auiMgr.SetManagedWindow(this);

        // Add panels
        m_auiMgr.AddPane(new ProjectNavigator(this),
            wxAuiPaneInfo().Name("navigator").Caption("Project")
            .Left().MinSize(200, -1).BestSize(250, -1));

        m_auiMgr.AddPane(new EditorPanel(this),
            wxAuiPaneInfo().Name("editor").CenterPane());

        m_auiMgr.AddPane(new AssistantPanel(this),
            wxAuiPaneInfo().Name("assistant").Caption("Assistant")
            .Right().Float().BestSize(300, 400));

        m_auiMgr.Update();
    }

    ~MainFrame() {
        m_auiMgr.UnInit();
    }
};
```

---

## Core C++ Libraries

### JSON - nlohmann_json

**Purpose:** JSON parsing & serialization (project files, settings, plugin manifests)

**Why nlohmann_json?**
- Modern C++ API (STL-like)
- Header-only (easy integration)
- Excellent documentation
- Wide adoption

**Example:**
```cpp
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Serialize
json j;
j["title"] = "My Novel";
j["author"] = "John Doe";
j["chapters"] = json::array();

std::ofstream file("book.json");
file << j.dump(2);  // Pretty-print with 2-space indent

// Deserialize
std::ifstream file("book.json");
json j = json::parse(file);
std::string title = j["title"];
```

---

### Logging - spdlog

**Purpose:** Fast, structured logging

**Why spdlog?**
- Extremely fast (header-only, compiled options)
- Multiple sinks (console, file, rotating files)
- Log levels (trace, debug, info, warn, error, critical)
- Thread-safe
- Formatted logging (fmt-like syntax)

**Example:**
```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// Setup logger
auto logger = spdlog::basic_logger_mt("kalahari", "logs/kalahari.log");
spdlog::set_default_logger(logger);
spdlog::set_level(spdlog::level::debug);

// Usage
spdlog::info("Application started");
spdlog::debug("Loading project: {}", projectPath);
spdlog::warn("Auto-save took {}ms", duration);
spdlog::error("Failed to load plugin: {}", pluginName);
```

---

### Compression - libzip

**Purpose:** ZIP archive handling (for .klh project files and .kplugin packages)

**Why libzip?**
- Cross-platform
- Clean C API (easy C++ wrapper)
- Read & write support
- Random access to archive contents

**.klh File Format:**
```
book.klh (ZIP archive)
├── manifest.json           # Book metadata
├── book.json              # Structure (chapters, sections)
├── chapters/
│   ├── ch_001.rtf         # Chapter 1 content (RTF)
│   ├── ch_002.rtf         # Chapter 2 content
│   └── ...
├── resources/
│   ├── images/
│   └── sources/
└── settings.json          # User preferences
```

**Example:**
```cpp
#include <zip.h>

// Create .klh file
zip_t* archive = zip_open("book.klh", ZIP_CREATE | ZIP_TRUNCATE, nullptr);
zip_source_t* src = zip_source_buffer(archive, jsonData.c_str(), jsonData.size(), 0);
zip_file_add(archive, "book.json", src, ZIP_FL_ENC_UTF_8);
zip_close(archive);

// Read .klh file
zip_t* archive = zip_open("book.klh", ZIP_RDONLY, nullptr);
zip_stat_t stat;
zip_stat(archive, "book.json", 0, &stat);
char* buffer = new char[stat.size];
zip_file_t* file = zip_fopen(archive, "book.json", 0);
zip_fread(file, buffer, stat.size);
zip_fclose(file);
zip_close(archive);
```

---

### Database - SQLite3

**Purpose:** Metadata, search index, statistics (Phase 2+)

**Why SQLite3?**
- Embedded (no server needed)
- Cross-platform
- ACID transactions
- Fast full-text search (FTS5)
- Public domain license

**Not in MVP:** JSON is sufficient for MVP. SQLite added in Phase 2 for:
- Full-text search across all chapters
- Advanced analytics (word count trends, writing speed)
- Character/location mention tracking

---

## Python Plugin System

### Embedded Python

**Version:** Python 3.11 (embedded)

**Why Python 3.11?**
- **Stable:** Mature, excellent library support
- **Fast:** 10-60% faster than 3.10
- **Not too new:** 3.12 still has library compatibility issues
- **Embedded:** Bundled with Kalahari (zero user friction)

**Deployment:**
```
kalahari/
├── kalahari.exe                # Main executable (C++)
├── python311/                  # Embedded Python runtime
│   ├── python311.dll
│   ├── python311.zip           # Standard library
│   ├── Lib/
│   └── DLLs/
└── plugins/                    # Plugin directory
    ├── docx_exporter/
    │   ├── plugin.json
    │   ├── plugin.py
    │   └── requirements.txt
    └── assistant/
```

**Why Embedded (not system Python)?**
- ✅ **Zero dependencies** - User doesn't need Python installed
- ✅ **Version control** - We control exact Python version
- ✅ **Reliability** - No "Python not found" errors
- ✅ **Professional** - Seamless user experience (like Blender, FreeCAD)

---

### pybind11

**Purpose:** C++ ↔ Python binding

**Why pybind11?**
- Modern C++11+ API
- Header-only (easy integration)
- Automatic type conversions
- STL container support
- Excellent documentation

**Example C++ → Python Binding:**
```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Expose C++ class to Python
PYBIND11_MODULE(kalahari_core, m) {
    py::class_<Document>(m, "Document")
        .def(py::init<const std::string&>())
        .def("get_title", &Document::getTitle)
        .def("set_title", &Document::setTitle)
        .def("add_chapter", &Document::addChapter,
             py::return_value_policy::reference)
        .def("get_chapters", &Document::getChapters);

    py::class_<Chapter>(m, "Chapter")
        .def_property("title", &Chapter::getTitle, &Chapter::setTitle)
        .def_property_readonly("word_count", &Chapter::getWordCount);
}
```

**Python Plugin Using C++ API:**
```python
import kalahari_core

class DOCXExporter:
    def export(self, doc: kalahari_core.Document, path: str) -> bool:
        from docx import Document as DocxDocument
        docx = DocxDocument()

        for chapter in doc.get_chapters():
            docx.add_heading(chapter.title, level=1)
            docx.add_paragraph(chapter.get_content())

        docx.save(path)
        return True
```

---

### Plugin Python Libraries

**Import/Export:**
- `python-docx` - Microsoft Word (.docx)
- `reportlab` - PDF generation
- `ebooklib` - EPUB e-books
- `markdown` - Markdown parsing
- `beautifulsoup4` - HTML parsing

**AI/NLP:**
- `openai` - OpenAI API (GPT models)
- `anthropic` - Anthropic API (Claude)
- `spacy` - NLP toolkit (local models)
- `langchain` - LLM orchestration

**OCR:**
- `pytesseract` - Tesseract OCR wrapper

**Language Tools:**
- `language-tool-python` - Grammar/spell checking
- `hunspell` - Spell checking dictionaries

**Visualization:**
- `matplotlib` - Charts, graphs
- `plotly` - Interactive charts
- `networkx` - Graph visualization (character relationships)

**Data Science:**
- `numpy` - Numerical computing
- `pandas` - Data analysis (statistics plugin)

---

## Additional Libraries

### XML/HTML - pugixml

**Purpose:** RTF parsing, HTML export/import

**Why pugixml?**
- Fast, lightweight
- XPath support
- Easy to use C++ API

---

### HTTP Client - libcurl + OpenSSL

**Purpose:** Cloud sync, AI API calls (from plugins)

**Why libcurl?**
- Industry standard
- HTTPS support (with OpenSSL)
- Cross-platform

**Used by:**
- Cloud Sync Pro plugin (Dropbox/GDrive API)
- AI Assistant plugin (OpenAI/Claude API)
- Research Pro plugin (web scraping)

---

## Internationalization (i18n)

**System:** wxWidgets wxLocale + GNU gettext

**Formats:**
- `.po` - Portable Object (human-editable translations)
- `.mo` - Machine Object (compiled translations)

**Supported Languages (MVP):**
- 🇬🇧 English (primary)
- 🇵🇱 Polish (secondary)

**Supported Languages (Post-MVP):**
- 🇩🇪 German, 🇷🇺 Russian, 🇫🇷 French, 🇪🇸 Spanish

**Implementation:** See [09_i18n.md](09_i18n.md) for detailed i18n architecture.

---

## Platform-Specific

### Windows

- **Compiler:** MSVC 2019+ (Visual Studio 2019+)
- **Installer:** NSIS (Nullsoft Scriptable Install System)
- **Code Signing:** Authenticode (signtool.exe)

### macOS

- **Compiler:** Clang 10+ (Xcode Command Line Tools)
- **Installer:** DMG (Disk Image)
- **Code Signing:** Apple Developer Certificate (codesign)
- **Architectures:** Intel (x86_64) + Apple Silicon (ARM64) - Universal Binary

### Linux

- **Compiler:** GCC 10+ or Clang 10+
- **Package:** AppImage (universal, no installation needed)
- **Alternative:** Snap, Flatpak (optional, community-maintained)
- **Dependencies:** Installed via system package manager

---

## Development Tools

### IDE / Editors

**Recommended:**
- Visual Studio 2019+ (Windows)
- CLion (cross-platform, excellent CMake support)
- VS Code (with C++, CMake extensions)

**Configuration Files:**
- `.clang-format` - Code formatting (LLVM style)
- `.clang-tidy` - Static analysis
- `.editorconfig` - Editor settings

---

### Version Control

**VCS:** Git

**Hosting:** GitHub (public repository)

**Branching Strategy:**
- `main` - Stable releases
- `develop` - Integration branch
- `feature/*` - Feature branches
- `bugfix/*` - Bug fixes

---

### CI/CD

**Platform:** GitHub Actions

**Matrix Builds:**
```yaml
strategy:
  matrix:
    os: [windows-latest, macos-latest, ubuntu-latest]
    include:
      - os: windows-latest
        triplet: x64-windows
      - os: macos-latest
        triplet: x64-osx
      - os: ubuntu-latest
        triplet: x64-linux
```

**Pipelines:**
- Build (all platforms)
- Test (unit + integration)
- Package (installers)
- Release (automated GitHub releases)

---

## Summary: Complete Stack

```
┌─────────────────────────────────────────────────────────┐
│                   KALAHARI APPLICATION                   │
├─────────────────────────────────────────────────────────┤
│  C++ Core (C++20)                                       │
│  ├─ GUI: wxWidgets 3.2+ (wxAUI)                        │
│  ├─ JSON: nlohmann_json                                │
│  ├─ Logging: spdlog                                    │
│  ├─ Compression: libzip (.klh files)                   │
│  ├─ Database: SQLite3 (Phase 2+)                       │
│  └─ Build: CMake 3.21+ + vcpkg                         │
├─────────────────────────────────────────────────────────┤
│  Plugin System                                          │
│  ├─ Embedded Python 3.11                               │
│  ├─ Binding: pybind11                                  │
│  └─ Format: .kplugin (ZIP)                             │
├─────────────────────────────────────────────────────────┤
│  Python Plugins                                         │
│  ├─ Import/Export: python-docx, reportlab, ebooklib   │
│  ├─ AI/NLP: openai, anthropic, spacy, langchain       │
│  ├─ OCR: pytesseract                                   │
│  ├─ Visualization: matplotlib, plotly, networkx       │
│  └─ Language: language-tool-python, hunspell          │
├─────────────────────────────────────────────────────────┤
│  Testing & Quality                                      │
│  ├─ Unit Tests: Catch2 v3                             │
│  ├─ Coverage: 70%+ (core), 50%+ (plugins)             │
│  └─ Static Analysis: clang-tidy                        │
├─────────────────────────────────────────────────────────┤
│  Platforms (all in MVP)                                 │
│  ├─ Windows 10/11 (MSVC 2019+, NSIS installer)        │
│  ├─ macOS 11+ (Clang, DMG, Universal Binary)          │
│  └─ Linux (GCC/Clang, AppImage)                        │
├─────────────────────────────────────────────────────────┤
│  CI/CD                                                  │
│  └─ GitHub Actions (matrix builds, automated release)  │
└─────────────────────────────────────────────────────────┘
```

---

## Next Steps

- **[03_architecture.md](03_architecture.md)** - How these technologies are organized
- **[04_plugin_system.md](04_plugin_system.md)** - Detailed plugin API design
- **[07_mvp_tasks.md](07_mvp_tasks.md)** - Implementation roadmap

---

**Version:** 1.0
**Status:** ✅ Finalized
**Last Updated:** 2025-10-24
