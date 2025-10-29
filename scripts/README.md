# Kalahari Build Scripts

> **Automated build scripts** for cross-platform development (Windows, Linux, macOS)

This directory contains convenience scripts to simplify building, testing, and managing the Kalahari project.

---

## 📋 Available Scripts

### Build Scripts

| Script | Platform | Description |
|--------|----------|-------------|
| **build.sh** | All (auto-detect) | Universal wrapper that detects OS and calls appropriate script |
| **build_linux.sh** | Linux | Full-featured build for Debian/Ubuntu/Fedora |
| **build_macos.sh** | macOS | Build for Intel + Apple Silicon (universal binary support) |
| **build_windows.bat** | Windows | Build for Windows 10/11 with Visual Studio |

### Utility Scripts

| Script | Description |
|--------|-------------|
| **clean.sh** | Remove build artifacts (cross-platform) |
| **test.sh** | Run Catch2 tests via ctest wrapper |
| **setup_dev_env.sh** | Install system dependencies (Ubuntu/Fedora/macOS) |
| **copy_clean.sh** | Copy project without build artifacts (useful for VirtualBox workflows) |

---

## 🚀 Quick Start

### Universal Build (Recommended)

```bash
# Auto-detects your OS and builds
./scripts/build.sh
```

### Platform-Specific Builds

**Linux:**
```bash
./scripts/build_linux.sh              # Debug build
./scripts/build_linux.sh --release    # Release build
./scripts/build_linux.sh --clean      # Clean + rebuild
./scripts/build_linux.sh --test       # Build + run tests
```

**macOS:**
```bash
./scripts/build_macos.sh              # Debug (native arch)
./scripts/build_macos.sh --release    # Release
./scripts/build_macos.sh --universal  # Universal binary (Intel + ARM)
./scripts/build_macos.sh --test       # Build + tests
```

**Windows:**
```powershell
# Open: "x64 Native Tools Command Prompt for VS 2022"
scripts\build_windows.bat              # Debug build
scripts\build_windows.bat Release      # Release build
scripts\build_windows.bat Debug clean  # Clean + rebuild
scripts\build_windows.bat Release test # Release + tests
```

---

## 🛠️ Script Usage Details

### build.sh (Universal)

Auto-detects your operating system and calls the appropriate platform-specific script.

**Usage:**
```bash
./scripts/build.sh [OPTIONS]
```

**Options:**
- `--release, -r` - Build in Release mode (default: Debug)
- `--clean, -c` - Clean build directory before building
- `--test, -t` - Run tests after building
- `--verbose, -v` - Enable verbose build output
- `--help, -h` - Show help message

**Examples:**
```bash
./scripts/build.sh                  # Debug build
./scripts/build.sh --release        # Release build
./scripts/build.sh --clean --test   # Clean + Debug + tests
./scripts/build.sh -r -t            # Release + tests (short flags)
```

---

### build_linux.sh

Full-featured build script for Linux (Debian/Ubuntu/Fedora).

**Features:**
- ✅ Auto-checks prerequisites (CMake, Ninja, GCC/Clang)
- ✅ **Auto-installs missing dependencies** (asks for confirmation)
- ✅ **Auto-detects VirtualBox shared folders** (builds in local storage automatically)
- ✅ Auto-initializes vcpkg submodule
- ✅ Auto-bootstraps vcpkg (first run)
- ✅ Parallel build (uses all CPU cores)
- ✅ Colorized output (green ✓, red ✗)
- ✅ Build time reporting

**Usage:**
```bash
./scripts/build_linux.sh [OPTIONS]
```

**Options:**
- `--release, -r` - Build in Release mode
- `--clean, -c` - Clean build directory first
- `--test, -t` - Run tests after building
- `--verbose, -v` - Verbose build output
- `--force-vbox` - Force VirtualBox workflow (rsync + local build)
- `--force-native` - Force native build (skip VBox detection)
- `--help, -h` - Show help

**Shared Filesystem Support (VirtualBox & WSL):**

The script **automatically detects** shared filesystems with symlink issues:
- **VirtualBox** shared folders (`/media/sf_*`, vboxsf filesystem)
- **WSL** Windows mounts (`/mnt/*`, 9p/drvfs filesystem)

When detected, builds in local VM storage (`~/kalahari-build/`) to avoid symlink/file locking issues. Binaries are copied back automatically.

**Workflow:**
```bash
# VirtualBox VM - detection is automatic!
cd /media/sf_E_DRIVE/Python/Projekty/Kalahari
./scripts/build_linux.sh              # Auto-detects VBox, builds in ~/kalahari-build/

# WSL - detection is also automatic!
cd /mnt/e/Python/Projekty/Kalahari
./scripts/build_linux.sh              # Auto-detects WSL, builds in ~/kalahari-build/

# Binaries automatically copied to shared folder:
# → build-linux/bin/kalahari
# → build-linux/bin/kalahari-tests

# Force native build on shared folder (not recommended - symlinks/locking fail):
./scripts/build_linux.sh --force-native
```

**Benefits:**
- ✅ **Zero configuration** - Auto-detects VirtualBox & WSL
- ✅ **Edit in host OS** (Windows) - Changes immediately visible
- ✅ **Fast builds** - No file locking issues
- ✅ **Symlinks work** - Builds in real filesystem (`~/kalahari-build/`)
- ✅ **Binaries accessible** - Automatically copied back to shared folder

**Prerequisites:**
```bash
# OPTIONAL - Script will auto-install if missing!
# But you can pre-install manually:

# Ubuntu/Debian
sudo apt install build-essential cmake ninja-build git pkg-config
sudo apt install libgtk-3-dev libx11-dev libxext-dev  # wxWidgets deps

# Fedora/RHEL
sudo dnf install gcc-c++ cmake ninja-build git pkg-config
sudo dnf install gtk3-devel libX11-devel libXext-devel  # wxWidgets deps

# Or use setup script:
./scripts/setup_dev_env.sh
```

**Note:** The build script will detect missing tools (cmake, ninja-build, g++, git, pkg-config) and offer to install them automatically.

---

### build_macos.sh

Build script for macOS with Intel + Apple Silicon support.

**Features:**
- ✅ Auto-detects architecture (Intel x86_64 vs ARM64)
- ✅ Universal Binary support (Intel + Apple Silicon in one binary)
- ✅ Homebrew integration
- ✅ Xcode Command Line Tools check

**Usage:**
```bash
./scripts/build_macos.sh [OPTIONS]
```

**Options:**
- `--release, -r` - Build in Release mode
- `--clean, -c` - Clean build directory first
- `--test, -t` - Run tests after building
- `--universal, -u` - Build Universal Binary (Intel + ARM)
- `--verbose, -v` - Verbose build output
- `--help, -h` - Show help

**Examples:**
```bash
./scripts/build_macos.sh              # Native architecture (Debug)
./scripts/build_macos.sh --release    # Native (Release)
./scripts/build_macos.sh --universal  # Universal binary (Intel + ARM)
./scripts/build_macos.sh -r -u -t     # Universal Release + tests
```

**Prerequisites:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build tools
brew install cmake ninja pkg-config git

# Or use setup script:
./scripts/setup_dev_env.sh
```

---

### build_windows.bat

Build script for Windows 10/11 with Visual Studio.

**Features:**
- ✅ Auto-detects Visual Studio toolchain
- ✅ Supports Ninja or MSBuild
- ✅ Auto-bootstraps vcpkg

**Usage:**
```powershell
scripts\build_windows.bat [BUILD_TYPE] [OPTIONS]
```

**Arguments:**
- `Debug` - Debug build (default)
- `Release` - Release build
- `clean` - Clean build directory first
- `test` - Run tests after building
- `--help, -h` - Show help

**Examples:**
```powershell
scripts\build_windows.bat              # Debug build
scripts\build_windows.bat Release      # Release build
scripts\build_windows.bat Debug clean  # Clean + Debug
scripts\build_windows.bat Release test # Release + tests
```

**Prerequisites:**
- Visual Studio 2019+ with "Desktop development with C++" workload
- Git for Windows

**Important:** Run from "x64 Native Tools Command Prompt for VS 2022"

---

### clean.sh

Remove build artifacts to force clean rebuild.

**Usage:**
```bash
./scripts/clean.sh [OPTIONS]
```

**Options:**
- `--full, -f` - Also remove `vcpkg_installed/` cache (requires re-download)
- `--help, -h` - Show help

**Examples:**
```bash
./scripts/clean.sh        # Remove build/ directory
./scripts/clean.sh --full # Remove build/ + vcpkg cache
```

**Note:** Full clean requires re-downloading all dependencies (~15-30 minutes on first rebuild).

---

### test.sh

Convenient wrapper for running Catch2 tests via ctest.

**Usage:**
```bash
./scripts/test.sh [OPTIONS]
```

**Options:**
- `--verbose, -v` - Show detailed test output
- `--filter PATTERN, -f` - Run only tests matching pattern
- `--help, -h` - Show help

**Examples:**
```bash
./scripts/test.sh                    # Run all tests
./scripts/test.sh --verbose          # Verbose output
./scripts/test.sh --filter "Thread*" # Run threading tests only
./scripts/test.sh -f "GUI*" -v       # GUI tests with verbose output
```

**Direct ctest usage:**
```bash
cd build
ctest --output-on-failure            # Run all tests
ctest -V                             # Very verbose
ctest -R "TestName"                  # Filter by regex
```

---

### setup_dev_env.sh

Install system dependencies required to build Kalahari.

**Supported platforms:**
- Ubuntu 20.04+ / Debian 11+
- Fedora 36+ / RHEL 8+
- macOS 11+

**Usage:**
```bash
./scripts/setup_dev_env.sh [OPTIONS]
```

**Options:**
- `--dry-run, -n` - Show commands without executing
- `--help, -h` - Show help

**Examples:**
```bash
./scripts/setup_dev_env.sh           # Install dependencies
./scripts/setup_dev_env.sh --dry-run # Preview what would be installed
```

**What it installs:**

**Ubuntu/Debian:**
- Build tools: `build-essential`, `cmake`, `ninja-build`, `git`, `pkg-config`
- wxWidgets deps: `libgtk-3-dev`, `libx11-dev`, `libxext-dev`, etc.

**Fedora/RHEL:**
- Build tools: `gcc-c++`, `cmake`, `ninja-build`, `git`, `pkg-config`
- wxWidgets deps: `gtk3-devel`, `libX11-devel`, `libXext-devel`, etc.

**macOS:**
- Homebrew packages: `cmake`, `ninja`, `pkg-config`, `git`
- Xcode Command Line Tools (prompts if not installed)

---

### copy_clean.sh

Copy Kalahari project to another directory without build artifacts and temporary files.

**Use cases:**
- ✅ Copy from VirtualBox shared folder to local Linux directory (avoids file locking issues)
- ✅ Create clean project snapshot for distribution
- ✅ Migrate project to faster storage (NVMe, SSD)
- ✅ Prepare project for archiving/backup

**Usage:**
```bash
./scripts/copy_clean.sh <destination_directory>
```

**Options:**
- `<destination>` - Target directory (required)

**Examples:**
```bash
# Copy to home directory (recommended for VirtualBox)
./scripts/copy_clean.sh ~/Kalahari

# Copy to /tmp for quick testing
./scripts/copy_clean.sh /tmp/kalahari-clean

# Copy to different partition
./scripts/copy_clean.sh /mnt/nvme/Kalahari
```

**What it excludes:**
- ❌ Build directories (`build-*/`, `build/`)
- ❌ vcpkg packages/buildtrees/downloads/installed
- ❌ Git objects and logs (keeps .git metadata for submodules)
- ❌ Compiled binaries (`.o`, `.obj`, `.exe`, `.dll`, `.so`, `.a`)
- ❌ Temporary files (`.log`, `.tmp`, `.bak`, `.swp`, `*~`)
- ❌ IDE cache (`.DS_Store`, `__pycache__`, `node_modules`)
- ❌ Screenshots (`*.jpg`, `*.png`, `Schowek_*`)

**What it keeps:**
- ✅ All source code (`src/`, `include/`, `tests/`)
- ✅ CMakeLists.txt and build scripts
- ✅ Documentation (`*.md`, `project_docs/`)
- ✅ vcpkg manifest and git submodule metadata
- ✅ Project configuration files

**Performance:**
- Uses `rsync` if available (fast, incremental)
- Falls back to `cp` with exclusions (slower)
- Shows size comparison and file counts

**Typical workflow (VirtualBox):**
```bash
# Option 1: Auto-detection (RECOMMENDED)
cd /media/sf_E_DRIVE/Python/Projekty/Kalahari
./scripts/build_linux.sh       # Auto-detects VBox, builds in ~/kalahari-build/

# Option 2: Copy to local directory (for independent work)
cd /media/sf_E_DRIVE/Python/Projekty/Kalahari
./scripts/copy_clean.sh ~/Kalahari  # Copy entire project
cd ~/Kalahari
./scripts/build_linux.sh  # Build from local copy
```

**Note:** `build_linux.sh` auto-detects VirtualBox and handles everything automatically. Use `copy_clean.sh` only if you need a completely independent copy.

**Note:** Destination directory will be removed if it exists (asks for confirmation first).

---

## 📂 Build Output

After successful build, binaries are located in:

```
build/
├── bin/
│   ├── kalahari          # Main application (Linux/macOS)
│   ├── kalahari.exe      # Main application (Windows)
│   ├── kalahari-tests    # Test executable (Linux/macOS)
│   └── kalahari-tests.exe # Test executable (Windows)
└── ...
```

**Run application:**
```bash
# Linux/macOS
./build/bin/kalahari

# Windows
build\bin\kalahari.exe
```

---

## 🐛 Troubleshooting

### "CMake not found" / "Ninja not found"
**Solution (Linux):** The build script will automatically offer to install missing tools. Just run `./scripts/build_linux.sh` and confirm installation prompts.

**Manual installation:**
```bash
# Linux
sudo apt install cmake ninja-build  # Ubuntu/Debian
sudo dnf install cmake ninja-build  # Fedora

# macOS
brew install cmake ninja

# Windows: Use Visual Studio installer
```

### "vcpkg bootstrap failed"
**Solution:**
1. Check internet connection (vcpkg needs to download tools)
2. Try manual bootstrap:
   ```bash
   cd vcpkg
   ./bootstrap-vcpkg.sh  # Linux/macOS
   .\bootstrap-vcpkg.bat # Windows
   ```

### "Build fails with missing libraries (Linux)"
**Solution:** Install wxWidgets system dependencies:
```bash
./scripts/setup_dev_env.sh  # Recommended
# Or manually:
sudo apt install libgtk-3-dev libx11-dev libxext-dev  # Ubuntu
sudo dnf install gtk3-devel libX11-devel libXext-devel  # Fedora
```

### "Application doesn't start (Linux)"
**Solution:** Check display variable and logs:
```bash
echo $DISPLAY  # Should show :0 or :1
./build/bin/kalahari 2>&1 | less  # Check for errors
cat ~/.local/share/kalahari/logs/kalahari.log  # Check logs
```

### First build takes very long (10-30 minutes)
**Expected:** vcpkg downloads and compiles dependencies (wxWidgets, spdlog, etc.)
- **First build:** 10-30 minutes (depends on CPU cores and internet speed)
- **Subsequent builds:** 1-2 minutes (dependencies cached in `vcpkg_installed/`)

---

## 📚 Additional Resources

- **Full build instructions:** [BUILDING.md](../BUILDING.md)
- **Project overview:** [CLAUDE.md](../CLAUDE.md)
- **Roadmap:** [ROADMAP.md](../ROADMAP.md)
- **Contributing:** [CONTRIBUTING.md](../CONTRIBUTING.md) *(when available)*

---

## ❓ Questions?

**Issue tracker:** https://github.com/bartoszwarzocha/kalahari/issues

**Documentation:** See [BUILDING.md](../BUILDING.md) for detailed manual build instructions

---

**Last Updated:** 2025-10-27 (Auto-detection for VirtualBox & WSL in build_linux.sh - zero configuration!)
