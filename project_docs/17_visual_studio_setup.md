# Visual Studio 2026 Setup Guide

**Status:** ✅ Ready for use
**Created:** 2025-11-13
**VS Version:** Visual Studio 2026 (v144 toolset)

---

## Overview

Kalahari supports **Visual Studio 2026** through **CMakePresets.json** for native CMake integration. This provides full IDE experience:
- ✅ Native project navigation
- ✅ IntelliSense with full context (wxWidgets, Python, C++20)
- ✅ Integrated debugger (breakpoints, watch, call stack)
- ✅ Build/Run directly from IDE (F5, Ctrl+Shift+B)
- ✅ Solution Explorer with CMake targets

**No manual .sln generation needed!** VS 2026 opens CMake projects directly.

---

## Quick Start

### 1. Open Project in Visual Studio 2026

**Option A: File → Open → Folder**
1. Launch Visual Studio 2026
2. File → Open → Folder
3. Navigate to `Kalahari/` directory
4. Select folder → Open
5. VS will detect CMakeLists.txt and CMakePresets.json automatically

**Option B: File → Open → CMake**
1. Launch Visual Studio 2026
2. File → Open → CMake
3. Navigate to `Kalahari/CMakeLists.txt`
4. Open

### 2. Select Preset

After opening, Visual Studio will prompt to select a preset:
- **windows-debug** - Debug build (recommended for development)
- **windows-release** - Release build (optimized)
- **windows-relwithdebinfo** - Release with debug symbols

**Or manually:** Toolbar → Configuration dropdown → "Manage Configurations" → Select preset

### 3. Configure Project

1. VS will start CMake configuration automatically
2. vcpkg will download/build dependencies (first time: ~20-30 minutes)
3. Wait for "CMake generation finished" message
4. Solution Explorer will populate with targets

### 4. Build

**Option A: Build All**
- Menu: Build → Build All
- Shortcut: Ctrl+Shift+B

**Option B: Build Specific Target**
- Right-click target in Solution Explorer → Build
- Available targets: kalahari, kalahari_core, kalahari-tests

### 5. Run/Debug

**Run without debugging:**
- Select `kalahari.exe` in toolbar dropdown
- Press Ctrl+F5 (or Debug → Start Without Debugging)

**Debug:**
- Set breakpoints in code (F9)
- Press F5 (or Debug → Start Debugging)
- Use Watch, Call Stack, Locals windows

---

## CMakePresets.json Configuration

### Windows Presets

**Configure presets:**
- `windows-debug` - Debug build, v144 toolset, x64
- `windows-release` - Release build, v144 toolset, x64
- `windows-relwithdebinfo` - Release with debug info, v144 toolset, x64

**Key settings:**
- Generator: `Visual Studio 17 2026`
- Toolset: `v144` (latest, C++20 full support)
- Architecture: `x64`
- Binary directory: `build-vs2026/`
- vcpkg triplet: `x64-windows`

### Cross-Platform Support

CMakePresets.json includes presets for:
- **Linux:** `linux-debug`, `linux-release` (Ninja + GCC/Clang)
- **macOS:** `macos-debug`, `macos-release` (Ninja + Clang)

These presets work in:
- Visual Studio Code (with CMake Tools extension)
- CLion
- Command line (`cmake --preset linux-debug`)

---

## Toolset Information

**v144 Toolset (Visual Studio 2026):**
- C++20 full support (concepts, modules, ranges)
- C++23 partial support
- Improved IntelliSense performance (50% faster)
- Better error messages
- Native vcpkg integration

**Compatibility:**
- ✅ wxWidgets 3.3.0+
- ✅ Python 3.11 (pybind11)
- ✅ All vcpkg dependencies
- ✅ C++20 standard library

---

## GitHub CI/CD Compatibility

**GitHub Actions workflow remains unchanged:**
- CI uses `windows-latest` (currently Windows Server 2022 + VS 2022)
- Generator: Ninja (not Visual Studio)
- Toolchain: Default MSVC from `ilammy/msvc-dev-cmd@v1`

**Why it works:**
- CMakePresets.json uses **conditional presets** (`hostSystemName` check)
- Local development: VS 2026 with v144 toolset
- CI/CD: Ninja with default MSVC
- Both produce compatible binaries (x64-windows, C++20)

**No CI/CD changes needed!** ✅

---

## Troubleshooting

### "CMake generation failed"

**Cause:** vcpkg dependencies not downloaded yet
**Solution:** Wait for vcpkg to finish (check Output → CMake window)

### "Toolset v144 not found"

**Cause:** Visual Studio 2026 not fully installed
**Solution:**
1. Launch Visual Studio Installer
2. Modify installation
3. Ensure "Desktop development with C++" workload installed
4. Check "MSVC v144 - VS 2026 C++ x64/x86 build tools"

### "Cannot find Python3"

**Cause:** vcpkg Python not built yet
**Solution:**
1. Delete `build-vs2026/` folder
2. Reconfigure (VS will rebuild vcpkg cache)

### "IntelliSense errors but build succeeds"

**Cause:** IntelliSense cache out of sync
**Solution:**
1. Close VS
2. Delete `.vs/` folder
3. Reopen project
4. Wait for IntelliSense to rebuild (~2-3 minutes)

---

## Tips & Tricks

### Performance

**Speed up builds:**
1. Use Debug preset for development (faster compilation)
2. Enable parallel builds: Tools → Options → Projects and Solutions → Build and Run → "maximum number of parallel project builds" = 4-8
3. Use SSD for `build-vs2026/` directory

**Speed up IntelliSense:**
1. Tools → Options → Text Editor → C/C++ → Advanced
2. Set "Max Cached Translation Units" = 8
3. Enable "Faster Project Load" = True

### Debugging

**Visualize wxWidgets objects:**
1. Tools → Options → Debugging → Native → "Enable natvis diagnostics"
2. wxWidgets provides `.natvis` files for wxString, wxArrayString, etc.

**Python debugging (mixed mode):**
1. Enable "Python/Native Debugging" in project properties
2. Set breakpoints in both C++ and Python code
3. F5 to debug across language boundary

### Code Navigation

**Quick shortcuts:**
- F12 - Go to definition
- Ctrl+F12 - Go to implementation
- Shift+F12 - Find all references
- Ctrl+T - Go to all (files, symbols, types)
- Alt+G - Go to line

---

## Alternative: Classic .sln Approach

If you prefer traditional Visual Studio workflow with Solution Explorer:

**Generate .sln manually:**
```cmd
cd Kalahari
cmake -G "Visual Studio 17 2026" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -B build-vs2026-sln
```

**Open generated solution:**
- File → Open → Project/Solution
- Navigate to `build-vs2026-sln/Kalahari.sln`
- Open

**Note:** .sln files are gitignored (regenerated locally)

---

## VS 2026 New Features Used

**AI-Native IDE:**
- Code completion with GitHub Copilot integration
- AI-powered refactoring suggestions
- Intelligent code search

**Performance Improvements:**
- 50% reduction in UI hang times
- 40% faster solution load
- Real-time IntelliSense updates

**C++ Features:**
- Native C++20 modules support
- Improved template error messages
- Better constexpr debugging

---

## References

- [Visual Studio 2026 Release Notes](https://learn.microsoft.com/en-us/visualstudio/releases/2026/release-notes)
- [CMakePresets.json Schema](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [vcpkg Integration in VS](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/msbuild-integration)

---

**Document Version:** 1.0
**Last Updated:** 2025-11-13
