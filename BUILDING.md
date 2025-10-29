# Building Kalahari

> **Cross-platform build instructions** for Windows, Linux, and macOS

**Current Version:** 0.0.1-dev (Phase 0 Week 2)

---

## Quick Start with Build Scripts 🚀

**Easiest way to build Kalahari:**

```bash
# Universal script (auto-detects OS)
./scripts/build.sh

# Or platform-specific:
./scripts/build_linux.sh              # Linux
./scripts/build_macos.sh              # macOS
scripts\build_windows.bat              # Windows
```

**Key features:**
- ✅ Auto-checks prerequisites
- ✅ Auto-initializes vcpkg submodule
- ✅ Auto-bootstraps vcpkg
- ✅ Parallel build (uses all CPU cores)
- ✅ Colorized output
- ✅ Build time reporting

**Common options:**
```bash
./scripts/build.sh --release    # Release build
./scripts/build.sh --clean      # Clean + rebuild
./scripts/build.sh --test       # Build + run tests
./scripts/build.sh --help       # Show all options
```

**For detailed script documentation, see:** [scripts/README.md](scripts/README.md)

**Manual build instructions below** (if you prefer step-by-step or need troubleshooting).

---

## Prerequisites

### All Platforms

**Required:**
- **Git** 2.30+ (for cloning repository and vcpkg)
- **CMake** 3.21+ (build system)
- **Ninja** 1.10+ (recommended build tool) OR Make
- **C++20 Compiler:**
  - GCC 10+ (Linux)
  - Clang 10+ (macOS)
  - MSVC 2019+ / Visual Studio 2019+ (Windows)

**Included (via vcpkg):**
- wxWidgets 3.3.1 (GUI framework)
- spdlog 1.16.0 (logging)
- nlohmann_json 3.12.0 (JSON)
- libzip 1.11.4 (ZIP archives)
- Catch2 3.11.0 (testing)

---

## Windows 10/11 Build Instructions

### Option 1: Visual Studio 2022 (Recommended)

#### 1. Install Prerequisites

**Visual Studio 2022:**
- Download from: https://visualstudio.microsoft.com/downloads/
- Install **Desktop development with C++** workload
- Includes: MSVC compiler, CMake, Ninja, Git

**Verify Installation:**
```powershell
# Open "x64 Native Tools Command Prompt for VS 2022"
cmake --version       # Should show 3.21+
ninja --version       # Should show 1.10+
git --version         # Should show 2.30+
```

#### 2. Clone Repository

```powershell
cd E:\Python\Projekty  # Or your preferred location
git clone https://github.com/bartoszwarzocha/kalahari.git
cd kalahari
git submodule update --init --recursive  # Initialize vcpkg submodule
```

#### 3. Bootstrap vcpkg

```powershell
cd vcpkg
.\bootstrap-vcpkg.bat
cd ..
```

#### 4. Configure with CMake

**Debug Build:**
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja
```

**Release Build:**
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja
```

**First Run:** vcpkg will download and build all dependencies (~10-20 minutes).
**Subsequent Runs:** Dependencies are cached (~1 minute).

#### 5. Build

```powershell
cmake --build build --config Debug
```

**Output:**
- Executable: `build\bin\kalahari.exe`
- Tests: `build\bin\kalahari-tests.exe`

#### 6. Run Application

```powershell
.\build\bin\kalahari.exe
```

**Expected:**
- Window opens with title "Kalahari Writer's IDE"
- Menu bar: File, Edit, View, Help
- Toolbar: New, Open, Save buttons
- Status bar: "Ready" | "Line 0, Col 0" | "00:00:00"

#### 7. Run Tests

```powershell
cd build
ctest -C Debug --output-on-failure
```

**Expected Output:**
```
Test project E:/Python/Projekty/Kalahari/build
    Start 1: kalahari-tests
1/1 Test #1: kalahari-tests ...................   Passed    0.05 sec

100% tests passed, 0 tests failed out of 1
```

---

### Option 2: WSL2 (Windows Subsystem for Linux)

**If you prefer Linux-style build:**
1. Install WSL2: https://docs.microsoft.com/en-us/windows/wsl/install
2. Install Ubuntu 22.04 from Microsoft Store
3. Follow **Linux Mint / Ubuntu** instructions below

---

## Linux Mint / Ubuntu 22.04+ Build Instructions

### 1. Install Prerequisites

```bash
# Update package lists
sudo apt update

# Install build tools
sudo apt install -y build-essential cmake ninja-build git pkg-config

# Install wxWidgets dependencies (GTK3, X11 libraries)
sudo apt install -y libgtk-3-dev libx11-dev libxext-dev libxrandr-dev \
    libxrender-dev libxi-dev libxfixes-dev libxtst-dev libglu1-mesa-dev \
    libpng-dev libjpeg-dev libtiff-dev libwebp-dev libcurl4-openssl-dev \
    libnotify-dev libsecret-1-dev libsdl2-dev liblzma-dev libbz2-dev \
    libzip-dev zlib1g-dev

# Verify installation
cmake --version       # Should show 3.21+
ninja --version       # Should show 1.10+
g++ --version         # Should show 10+
```

### 2. Clone Repository

```bash
cd ~/Projects  # Or your preferred location
git clone https://github.com/bartoszwarzocha/kalahari.git
cd kalahari
git submodule update --init --recursive  # Initialize vcpkg submodule
```

### 3. Bootstrap vcpkg

```bash
cd vcpkg
./bootstrap-vcpkg.sh
cd ..
```

### 4. Configure with CMake

**Debug Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -G Ninja
```

**Release Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -G Ninja
```

**First Run:** vcpkg will download and build dependencies (~15-30 minutes on Linux).
**Note:** Linux builds take longer due to GTK3 compilation.

**Troubleshooting:**
- If vcpkg fails with "libxcrypt" error → already fixed in recent commits
- If missing dependencies → check error message for package name, install with `apt install`

### 5. Build

```bash
cmake --build build --config Debug
```

**Output:**
- Executable: `build/bin/kalahari`
- Tests: `build/bin/kalahari-tests`

### 6. Run Application

```bash
./build/bin/kalahari
```

**Expected:**
- Window opens with GTK3 native look & feel
- Menu bar: File, Edit, View, Help
- Toolbar: New, Open, Save buttons (GTK3 icons)
- Status bar: "Ready" | "Line 0, Col 0" | "00:00:00"

**If window doesn't open:**
```bash
# Check for errors
./build/bin/kalahari 2>&1 | less

# Verify display
echo $DISPLAY  # Should show :0 or :1

# Check logs (if spdlog writes to file)
cat ~/.local/share/kalahari/logs/kalahari.log
```

### 7. Run Tests

```bash
cd build
ctest --output-on-failure
```

**Expected Output:**
```
Test project /home/username/Projects/kalahari/build
    Start 1: kalahari-tests
1/1 Test #1: kalahari-tests ...................   Passed    0.12 sec

100% tests passed, 0 tests failed out of 1
```

---

## macOS Build Instructions (Intel + Apple Silicon)

### 1. Install Prerequisites

**Homebrew:**
```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build tools
brew install cmake ninja pkg-config git

# Verify installation
cmake --version       # Should show 3.21+
ninja --version       # Should show 1.10+
git --version         # Should show 2.30+
```

**Xcode Command Line Tools:**
```bash
xcode-select --install
```

### 2. Clone Repository

```bash
cd ~/Projects  # Or your preferred location
git clone https://github.com/bartoszwarzocha/kalahari.git
cd kalahari
git submodule update --init --recursive
```

### 3. Bootstrap vcpkg

```bash
cd vcpkg
./bootstrap-vcpkg.sh
cd ..
```

### 4. Configure with CMake

**Debug Build (Intel):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -G Ninja
```

**Debug Build (Apple Silicon - M1/M2/M3):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -G Ninja
```

**Universal Binary (Intel + Apple Silicon):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -G Ninja
```

### 5. Build

```bash
cmake --build build --config Debug
```

### 6. Run Application

```bash
./build/bin/kalahari
```

**Expected:**
- Window opens with native macOS look & feel
- Menu bar: Kalahari, File, Edit, View, Help (macOS standard)
- Toolbar: macOS-style icons
- Keyboard shortcuts: Cmd+Q (quit), Cmd+N (new), etc.

### 7. Run Tests

```bash
cd build
ctest --output-on-failure
```

---

## Manual Testing Checklist

### Task #00001: Basic GUI Window

**Test 1: Window Opens**
- [ ] Application starts without crashes
- [ ] Window displays with title "Kalahari Writer's IDE"
- [ ] Window size is reasonable (default 1024x768 or larger)

**Test 2: Menu Bar**
- [ ] File menu: New, Open, Save, Save As, Exit
- [ ] Edit menu: Undo, Redo
- [ ] View menu: (placeholder)
- [ ] Help menu: About
- [ ] Click "Help → About" → dialog shows with "Kalahari Writer's IDE v0.0.1"

**Test 3: Toolbar**
- [ ] Toolbar visible with icons
- [ ] New button present (document icon)
- [ ] Open button present (folder icon)
- [ ] Save button present (floppy disk icon)
- [ ] Click buttons → status bar shows action

**Test 4: Status Bar**
- [ ] Status bar visible at bottom
- [ ] 3 panes: Status text | Position | Time
- [ ] Status text shows "Ready" on startup
- [ ] Position shows "Line 0, Col 0" (placeholder)
- [ ] Time shows "00:00:00" (placeholder)

**Test 5: i18n Structure**
- [ ] File → Exit shows localized text (English or Polish)
- [ ] If you change system language → restart app → UI updates (Phase 1 feature)

---

### Task #00002: Threading Infrastructure

**Test 1: Thread Limiting (Semaphore)**
1. Click **File → Open** 5 times rapidly (within 1 second)
2. **Expected:**
   - First 4 clicks: Status bar shows "Loading file..."
   - 5th click: Dialog appears: "Busy - please wait for current tasks to complete"
3. Wait 10 seconds (all tasks complete)
4. Click **File → Open** again → should work (status bar updates)

**Test 2: Success Case**
1. Click **File → Open** once
2. **Expected:**
   - Status bar immediately shows "Loading file..."
   - After 2 seconds: Status bar shows "Loaded: example.klh"
   - No crashes or freezes

**Test 3: Cleanup (Graceful Shutdown)**
1. Click **File → Open** 2 times (start 2 background tasks)
2. Immediately close window (X button or File → Exit)
3. **Expected:**
   - Window closes within 5 seconds (waits for tasks)
   - No crashes or "thread terminated" errors
   - If tasks don't finish in 5 seconds → forced shutdown with log error

**Test 4: Log Verification**
1. Run application from terminal/command prompt
2. Click **File → Open** once
3. **Expected console output:**
   ```
   [debug] Submitting background task (thread pool: 1/4)
   [debug] <... 2 seconds later ...>
   ```
4. Check logs show thread activity (if spdlog writes to file)

**Test 5: Stress Test**
1. Click **File → Open** 10 times rapidly
2. Immediately click **File → Exit**
3. **Expected:**
   - Application closes gracefully
   - No "use after free" or segmentation faults
   - Logs may show "Timeout waiting for N background tasks"

---

### Task #00003: Settings System (To be tested after implementation)

**Test 1: Default Settings (First Run)**
1. Delete settings file:
   - Windows: Delete `%APPDATA%\Kalahari\settings.json`
   - Linux: Delete `~/.config/kalahari/settings.json`
2. Start application
3. **Expected:**
   - Window opens with default size (1280x800)
   - Window position (100, 100)
   - settings.json created with defaults

**Test 2: Window State Persistence**
1. Start application
2. Resize window to 800x600
3. Move window to position (200, 200)
4. Close application
5. Restart application
6. **Expected:**
   - Window opens at 800x600
   - Window position (200, 200)
   - settings.json contains updated values

**Test 3: Maximized State**
1. Start application
2. Maximize window (Windows: Win+Up, Linux: Double-click title bar)
3. Close application
4. Restart application
5. **Expected:**
   - Window opens maximized
   - settings.json contains `"maximized": true`

**Test 4: Corrupted Settings**
1. Open settings.json in text editor
2. Delete half the file (corrupt JSON)
3. Save file
4. Start application
5. **Expected:**
   - Application starts normally with defaults
   - Console shows warning: "Settings file corrupted, using defaults"
   - Old settings.json backed up (optional: `settings.json.bak`)

**Test 5: Missing Directory**
1. Delete entire config directory:
   - Windows: Delete `%APPDATA%\Kalahari\`
   - Linux: Delete `~/.config/kalahari/`
2. Start application
3. **Expected:**
   - Directory created automatically
   - settings.json created with defaults
   - No errors in console

---

## Troubleshooting

### Build Errors

**Error: "Could not find wxWidgets"**
- **Solution:** vcpkg didn't install correctly. Run:
  ```bash
  cd vcpkg
  ./vcpkg install wxwidgets[core,debug-support,fonts,sound]:x64-linux
  ```

**Error: "libxcrypt.so not found" (Linux)**
- **Solution:** Already fixed in latest commits. Pull latest code:
  ```bash
  git pull origin main
  git submodule update --init --recursive
  ```

**Error: "Permission denied" (Linux/macOS)**
- **Solution:** Make bootstrap script executable:
  ```bash
  chmod +x vcpkg/bootstrap-vcpkg.sh
  ```

**Error: CMake can't find Ninja**
- **Solution (Windows):** Use "x64 Native Tools Command Prompt for VS 2022"
- **Solution (Linux/macOS):** Install ninja:
  ```bash
  # Linux
  sudo apt install ninja-build

  # macOS
  brew install ninja
  ```

### Runtime Errors

**Application doesn't start (no window)**
- Check console output for errors
- Verify wxWidgets installed correctly: `vcpkg list | grep wxwidgets`
- Check logs (if enabled)

**Window appears but is blank**
- Expected in Phase 0 (no editor yet)
- Main panel shows "Editor placeholder - Phase 1"

**Crashes on close**
- Check if background threads are stuck
- Verify Task #00002 implementation has destructor cleanup

---

## CI/CD Verification

**After pushing to GitHub, verify builds pass:**

### Option 1: GitHub Web UI
1. Go to: https://github.com/bartoszwarzocha/kalahari/actions
2. Check latest workflow run
3. Verify all 3 platforms: ✅ macOS, ✅ Windows, ✅ Linux

### Option 2: Command Line (gh CLI)
```bash
# Install GitHub CLI
# Windows: winget install GitHub.cli
# Linux: sudo apt install gh
# macOS: brew install gh

# Authenticate
gh auth login

# Check workflow status
gh run list --limit 5

# View specific run
gh run view <run-id>
```

### Option 3: Local CI Simulation
```bash
# Install act (local GitHub Actions runner)
# https://github.com/nektos/act

# Run Linux workflow locally
act -j build-linux

# Note: Requires Docker
```

**Expected CI Results:**
- ✅ **macOS:** Build time ~1-5 minutes (cached), ~10-15 minutes (clean)
- ✅ **Windows:** Build time ~3-6 minutes (cached), ~15-20 minutes (clean)
- ✅ **Linux:** Build time ~4-8 minutes (cached), ~20-30 minutes (clean)

---

## Development Workflow

**Typical development cycle:**

1. **Pull latest changes:**
   ```bash
   git pull origin main
   git submodule update --init --recursive
   ```

2. **Create feature branch:**
   ```bash
   git checkout -b feature/my-feature
   ```

3. **Make changes** (edit code)

4. **Build and test:**
   ```bash
   cmake --build build --config Debug
   ./build/bin/kalahari        # Manual test
   cd build && ctest           # Automated tests
   ```

5. **Commit changes:**
   ```bash
   git add .
   git commit -m "feat: Add feature X"
   ```

6. **Push and verify CI:**
   ```bash
   git push origin feature/my-feature
   # Check GitHub Actions
   ```

7. **Merge to main** (after CI passes)

---

## Next Steps

- **Phase 0 Week 3:** Implement Task #00003 (Settings System)
- **Phase 0 Week 3-4:** Python Embedding (pybind11)
- **Phase 0 Week 5-6:** Plugin Manager
- **Phase 1:** Rich text editor (wxRichTextCtrl)

---

**Questions?** See [CLAUDE.md](CLAUDE.md) for project overview or ask in GitHub Issues.

**Last Updated:** 2025-10-26
