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

```bash
# Clone with submodules (includes vcpkg)
git clone --recursive https://github.com/bartoszwarzocha/kalahari.git
cd kalahari

# Bootstrap vcpkg
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# OR
bootstrap-vcpkg.bat   # Windows

# Configure and build
cd ..
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Run
./build/bin/kalahari
```

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

**Phase 0: Foundation** (Weeks 1-8)
✅ Week 1: Project setup, CMake, vcpkg, CI/CD
⏳ Week 2: wxWidgets basic window
⏳ Week 3-4: Python embedding (pybind11)
⏳ Week 5-6: Plugin Manager core
⏳ Week 7-8: Extension Points & Event Bus

[View full roadmap →](ROADMAP.md)

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
**Current Phase:** Phase 0 (Foundation) - Week 1 Complete ✅

**Ecosystem Roadmap:**
- **Kalahari** - Main writing environment (this project) 🚧
- **Serengeti** - Collaborative writing (future)
- **Okavango** - Research & knowledge management (future)
- **Victoria** - Cloud sync & storage (future)
- **Zambezi** - Publishing toolkit (future)
