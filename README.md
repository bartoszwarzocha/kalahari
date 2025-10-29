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

**Pre-Alpha** - Phase 0 (Foundation)

**Current Version:** 0.0.1-dev
**Target Release:** Kalahari 1.0 (Q2-Q3 2026)

---

## 🎯 Vision

Kalahari is a comprehensive **Writer's IDE** designed to eliminate technical and organizational barriers in book writing. It combines:
- Native desktop performance (C++20 + wxWidgets)
- Powerful plugin system (Python 3.11)
- Intelligent graphical assistant (8 animal personalities)
- Professional writing tools (Character banks, Timeline, Analytics)

**Part of the African Ecosystem:**
Kalahari • Serengeti • Okavango • Victoria • Zambezi

---

## 🛠️ Tech Stack

- **Language:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- **GUI:** wxWidgets 3.3.0+ (native cross-platform, dark mode support)
- **Build:** CMake 3.21+ with vcpkg (manifest mode)
- **Plugins:** Python 3.11 (embedded) + pybind11
- **Testing:** Catch2 v3 (BDD style)

**Core Libraries:**
- **spdlog** - Fast structured logging
- **nlohmann_json** - Modern C++ JSON
- **libzip** - Project file compression (.klh format)

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
- ✅ Compile all dependencies (first time: 15-30 min)
- ✅ Run unit tests
- ✅ Copy binaries to `build*/bin/`

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
# wxWidgets development libraries
sudo apt-get install libwxgtk-3.2-dev      # Ubuntu/Debian
# OR
sudo dnf install wxGTK-devel                # Fedora
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
- [Development Roadmap](ROADMAP.md) - 18-month development plan
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

**Phase 0: Foundation** (Weeks 1-8) - **Week 3-4 Complete ✅**

| Week | Component | Status | Details |
|------|-----------|--------|---------|
| 1 | Project Setup, CMake, vcpkg, CI/CD | ✅ Complete | Full infrastructure |
| 2 | wxWidgets GUI, Logging, Threading | ✅ Complete | Main window with menu/toolbar/status bar |
| 3 | Settings System | ✅ Complete | JSON persistence + Dialog UI (Task #00003) |
| 3 | Build Automation Scripts | ✅ Complete | Cross-platform build scripts (Task #00004) |
| 2 | Python 3.11 Embedding | ✅ Complete | Embedded interpreter + stdlib detection (Task #00005) |
| **3-4** | **Plugin Foundation** | **✅ Complete** | **PluginManager + pybind11 (Task #00009)** |
| 5-6 | Extension Points + Event Bus | ⏳ Planned | ExtensionPointRegistry + EventBus (Task #00010) |
| 6 | .kplugin Format Handler | ⏳ Planned | Plugin discovery, loading, unloading (Task #00011) |
| 7-8 | Document Model | ⏳ Planned | Document/Chapter/Book classes (Task #00012) |

**Current Phase Progress:** 52% Complete (4.5 / 8 weeks)
- ✅ 42+ files created
- ✅ 50+ unit tests passing
- ✅ All platforms building (Windows, macOS, Linux)

[View detailed roadmap →](ROADMAP.md) | [Phase 0 Strategic Plan →](STRATEGIC_PLAN_PHASE0.md)

---

## 💼 Business Model

**Open Core + Premium Plugins + Cloud Services**

- **Core:** MIT License (open source, GitHub public)
- **Premium Plugins:** 5 paid plugins ($14-39, $79 bundle)
  - AI Assistant Pro, Advanced Analytics, Export Suite, Research Pro, Collaboration Pack
- **Cloud:** Subscription ($5-10/month) - Cloud Sync Pro

[Learn more →](project_docs/05_business_model.md)

---

## 🤝 Contributing

Kalahari is in **Phase 0 (Foundation)**. Contributions welcome after 1.0 release.

For now, please:
- ⭐ Star the repository
- 📣 Share with writer friends
- 🐛 Report bugs via [Issues](https://github.com/bartoszwarzocha/kalahari/issues)

---

## 📄 License

**MIT License** - See [LICENSE](LICENSE) for details.

Copyright (c) 2025 Kalahari Project

Core application is open source. Premium plugins and cloud services available separately.

---

## 🦁 About

Created with passion for writers who deserve better tools.

**Project Start:** October 2025
**Current Phase:** Phase 0 (Foundation) - Week 3-4 Complete ✅ (Plugin Foundation)

**Ecosystem Roadmap:**
- **Kalahari** - Main writing environment (this project) 🚧
- **Serengeti** - Collaborative writing (future)
- **Okavango** - Research & knowledge management (future)
- **Victoria** - Cloud sync & storage (future)
- **Zambezi** - Publishing toolkit (future)
