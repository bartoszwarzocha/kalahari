<div align="center">
  <img src="project_docs/kalahari_logo.png" alt="Kalahari Logo" width="50%">

  # Kalahari - Writer's IDE

  > **Advanced writing environment for book authors**

  [![LINUX](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-linux.yml)
  [![WINDOWS](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-windows.yml)
  [![MACOS](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/bartoszwarzocha/kalahari/actions/workflows/ci-macos.yml)

  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
  [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
  [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://github.com/bartoszwarzocha/kalahari)
</div>

---

## 🚧 Status

**Phase 0 Complete ✅** | **Phase 1 IN PROGRESS** (Core Editor, about half done)

**Current Version:** 0.3.0-alpha
**Next Release:** 0.4.0-alpha (Phase 1 complete)
**Last Updated:** 2026-10-03

---

## 🎯 Vision

Kalahari is a comprehensive **Writer's IDE** designed to eliminate technical and organizational barriers in book writing. It combines:
- Native desktop performance (C++20 + Qt6)
- Powerful plugin system (Python 3.11)
- Intelligent graphical assistant (8 animal personalities)
- Professional writing tools (Character banks, Timeline, Analytics)

**Part of the African Ecosystem:**
Kalahari • Okavango • Zambezi

---

## 🛠️ Tech Stack

- **Language:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- **GUI:** Qt6 6.4+, builds use 6.9 (Widgets, native cross-platform, automatic DPI scaling)
- **Build:** CMake 3.21+ with vcpkg (manifest mode)
- **Plugins:** Python 3.11 (embedded) + pybind11
- **Testing:** Catch2 v3 (BDD style)

**Core Libraries:**
- **Qt6** - Cross-platform GUI framework (LGPL v3, dynamically linked)
- **spdlog** - Fast structured logging (MIT)
- **nlohmann_json** - Modern C++ JSON (MIT)
- **libzip** - Project file compression (.klh format, BSD)
- **Hunspell** - Spell checking library (LGPL/MPL)
- **OpenSSL** - Plugin signature verification (Apache 2.0)
- **Catch2** - Unit testing framework (BSL-1.0)
- **pybind11** - C++/Python bindings (BSD)

---

## 🚀 Quick Start

### Automated Build (Recommended)

We provide platform-specific build scripts for easy setup:

**Linux/macOS:**
```bash
git clone --recursive https://github.com/bartoszwarzocha/kalahari.git
cd kalahari
./scripts/build_linux.sh    # Or build_macos.sh for macOS
```

**Windows:**
```cmd
git clone --recursive https://github.com/bartoszwarzocha/kalahari.git
cd kalahari
scripts\build_windows.bat
```

The build script will:
- ✅ Bootstrap vcpkg (automatic)
- ✅ Configure CMake with correct toolchain
- ✅ Compile all dependencies (first time: 15-30 min, subsequent: 1-3 min with binary cache)
- ✅ Run unit tests (623 test cases, 4,216 assertions)
- ✅ Copy binaries to `build*/bin/`

**Performance Note:** CI/CD builds optimized with vcpkg binary cache (~92% faster on Linux!)

**Run the application:**
```bash
# Linux/macOS
./build-linux/bin/kalahari

# Windows
.\build-windows\bin\kalahari.exe
```

### Manual Build

If you prefer manual control:

```bash
# Clone with submodules
git clone --recursive https://github.com/bartoszwarzocha/kalahari.git
cd kalahari

# Bootstrap vcpkg (one-time only)
cd vcpkg
./bootstrap-vcpkg.sh      # Linux/macOS
# OR
.\bootstrap-vcpkg.bat     # Windows
cd ..

# Configure CMake with vcpkg toolchain
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --parallel 4

# Run
./build/bin/kalahari

# Run tests
cd build
ctest --output-on-failure
```

### Platform Requirements

**Common (All Platforms):**
- CMake 3.21+
- C++ compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- Python 3.11 (will be installed by vcpkg)

**Linux (Debian/Ubuntu/Fedora):**
```bash
# Qt6 and Hunspell development libraries (for system builds)
sudo apt-get install qt6-base-dev libhunspell-dev  # Ubuntu/Debian
# OR
sudo dnf install qt6-qtbase-devel hunspell-devel   # Fedora
```

**macOS:**
- Xcode Command Line Tools (automatic installation)

**Windows:**
- Visual Studio 2019+ (Build Tools sufficient)
- Windows 10+ or Windows Server 2016+

### Diagnostic Mode

Test Python embedding and plugin system:

```bash
./build-linux/bin/kalahari --diag
```

Then open **Diagnostics** menu to verify:
- ✅ Python interpreter initialization
- ✅ kalahari_api module loading
- ✅ Logger bindings working

---

## 📖 Documentation

- [Project Overview](project_docs/01_overview.md) - Vision, goals, target audience
- [Tech Stack](project_docs/02_tech_stack.md) - Complete technical details
- [Architecture](project_docs/03_architecture.md) - System design patterns
- [Plugin System](project_docs/04_plugin_system.md) - Plugin API specification
- [KML Format](docs/kml_format.md) - Chapter text format as the editor reads and writes it
- [Development Roadmap](ROADMAP.md) - Current state and next stages
- [Master Project File](CLAUDE.md) - Complete project documentation

**Full Documentation Index:** [project_docs/README.md](project_docs/README.md)

---

## 🦁 Graphical Assistant

Kalahari features an intelligent writing assistant with **8 animal personalities**:

**MVP (Phase 1):**
- 🦁 **Lion** (default) - Authoritative mentor
- 🐭 **Meerkat** - Friendly helper
- 🐘 **Elephant** - Wise advisor
- 🐆 **Cheetah** - Fast & focused

**Phase 2:**
- 🦒 Giraffe, 🦬 Buffalo, 🦜 Parrot, 🦎 Chameleon

Each assistant has unique personality and communication style, helping you stay motivated and productive.

---

## 🗺️ Development Status

**Phase 0: Qt Foundation** - **100% COMPLETE ✅** (2025-11-21)

### ✅ Phase 0 Achievements:

**Qt6 Migration (2025-11-19 Decision):**
- ✅ Migrated from wxWidgets to Qt6 for superior quality
- ✅ Automatic DPI scaling, QSS theming, QPalette integration
- ✅ wxWidgets code archived (wxwidgets-archive branch, v0.2.0-alpha-wxwidgets tag)

**Core Infrastructure:**
- ✅ CMake build system (all platforms)
- ✅ vcpkg integration (manifest mode)
- ✅ QMainWindow application (menu/toolbar/statusbar)
- ✅ Settings system (JSON persistence + QDialog UI)
- ✅ Logging system (spdlog - structured, multi-level)
- ✅ Theme system (ThemeManager, Light/Dark themes, per-theme colors)
- ✅ Icon system (ArtProvider, SVG with color placeholders)
- ✅ Build automation scripts (cross-platform)
- ✅ CI/CD pipelines (GitHub Actions - Linux/macOS/Windows)

**Plugin Architecture:**
- ✅ Python 3.11 embedding + pybind11
- ✅ Plugin Manager (discovery, loading, lifecycle)
- ✅ Extension Points system (IExporter, IPanelProvider, IAssistant)
- ✅ Event Bus (async, thread-safe, GUI-aware)
- ✅ .kplugin format handler (ZIP with manifest.json)

**Document Model:**
- ✅ Core classes (BookElement, Part, Book, Document)
- ✅ JSON serialization (nlohmann_json)
- ✅ .klh file format (ZIP container)

**Testing:**
- ✅ 623 test cases, 4,216 assertions
- ✅ 100% passing on all platforms (Linux, macOS, Windows)
- ✅ Zero compiler warnings

### Phase 1: Core Editor - IN PROGRESS

**Focus:** Custom text editor + project navigation

**Completed Features:**
- ✅ Menu System Review & Cleanup (OpenSpec #00030)
- ✅ Toolbar System with customization UI (OpenSpec #00031)
- ✅ Project File System with .klh support (OpenSpec #00033)
- ✅ Navigator Panel with drag & drop (OpenSpec #00034, #00036)
- ✅ Theme & Icon System (ArtProvider, KalahariStyle)

**In Progress:**
- Custom Text Editor (KML model, rich formatting) - performance and rendering work
- Find & Replace, Spell Check - implemented in core, not yet wired to the GUI
- Text styles (paragraph styles, colors, user styles)
- Statistics (word count in status bar works; statistics panel planned)

Menu commands that are not implemented yet are shown disabled.

[View detailed roadmap ->](ROADMAP.md)

---

## 💚 Free and Open Source

Kalahari is free software. Every component – the application, the plugin system and all plugins, present and future – is released under the **MIT License**. There are no paid plugins, subscriptions or cloud services; your work stays on your computer.

---

## 🔧 Git Hooks

Kalahari uses a pre-commit hook to enforce code quality standards and prevent common anti-patterns from being committed.

### What It Checks

The hook scans staged `.cpp` and `.h` files for prohibited patterns:

| Pattern | Reason | Correct Alternative |
|---------|--------|---------------------|
| `QIcon(` | Hardcoded icon paths | `core::ArtProvider::getInstance().getIcon()` |
| `QColor(` | Hardcoded colors | `core::ArtProvider::getInstance().getPrimaryColor()` |
| `qDebug` | Debug statements | `core::Logger::getInstance().debug()` |
| `std::cout` | Console output | `core::Logger::getInstance().info()` |

### Installation

```bash
# Linux/macOS
./scripts/install-hooks.sh

# Windows (Git Bash)
bash scripts/install-hooks.sh
```

### Check Status

```bash
./scripts/install-hooks.sh --check
```

### Bypassing the Hook

In exceptional cases, you can skip the hook:

```bash
git commit --no-verify -m "message"
```

**Warning:** Bypassing the hook is discouraged. CI/CD will still fail if prohibited patterns are present.

---

## 🤝 Contributing

Kalahari is in **Phase 1 (Core Editor)** active development. Contributions welcome after 1.0 release.

For now, please:
- Star the repository
- Share with writer friends
- Report bugs via [Issues](https://github.com/bartoszwarzocha/kalahari/issues)

---

## 📄 License

**MIT License** - See [LICENSE](LICENSE) for complete license text and third-party dependencies.

Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

**Key Points:**
- ✅ Kalahari application code and all plugins: **MIT License** (permissive, open source)
- ✅ Qt6 Framework: **LGPL v3** (dynamically linked for compliance)
- ✅ All other dependencies: MIT/BSD/permissive licenses
- ✅ Commercial use: **Allowed** (LGPL v3 compliant via dynamic linking)

**Redistribution:** If distributing binaries, include Qt dynamic libraries (.dll, .so, .dylib) and inform users they can replace them. See [LICENSE](LICENSE) for full LGPL v3 requirements.

---

## 🦁 About

Created with passion for writers who deserve better tools.

**Project Start:** October 2025
**Qt6 Migration:** November 2025
**Current Phase:** Phase 0 Complete ✅ | Phase 1 IN PROGRESS

**Ecosystem Roadmap:**
- **Kalahari** - Main writing environment (this project) - Phase 1 IN PROGRESS
- **Okavango** - Research & knowledge management (future)
- **Zambezi** - Publishing toolkit (future)
