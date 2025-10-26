# Task #00001: wxWidgets Basic Window

## Context
- **Phase:** Phase 0 Week 2
- **Roadmap Reference:** project_docs/07_mvp_tasks.md (Week 2)
- **Related Docs:**
  - project_docs/08_gui_design.md (GUI architecture)
  - project_docs/03_architecture.md (MVP pattern)
  - .claude/skills/kalahari-wxwidgets/README.md (wxWidgets patterns)
- **Dependencies:**
  - Task #00000 (Fix Linux CI Build Failure) - ✅ COMPLETED (2025-10-26)
  - Week 1 deliverables (CMake, vcpkg, CI/CD) - ✅ COMPLETED

## Objective
Create basic wxWidgets application window with menu bar, status bar, toolbar, and logging.
This establishes the GUI foundation for Kalahari Writer's IDE.

**Goal:** Display native wxWidgets window with functional UI chrome (menus, toolbar, status bar).

## Proposed Approach

### 1. Architecture Overview

**MVP Pattern Structure:**
```
src/
├── main.cpp                    # wxApp entry point
├── gui/
│   ├── kalahari_app.h/cpp     # wxApp-derived application class
│   └── main_window.h/cpp      # wxFrame-derived main window
└── core/
    └── logger.h/cpp           # spdlog wrapper (singleton)
```

**Design Decisions:**
- Use **wxFrame** for main window (standard top-level window)
- Use **wxAUI** for future docking (setup infrastructure now, use in Week 3+)
- Use **event tables** (`wxBEGIN_EVENT_TABLE`) for menu/toolbar events (type-safe, compile-time)
- Use **spdlog** for logging (already in vcpkg.json)
- Follow **kalahari-wxwidgets** skill conventions (m_ prefix, On prefix, camelCase)

### 2. Step-by-Step Implementation

#### Step 1: Setup spdlog Logger Singleton (30 min)
**File:** `src/core/logger.h`, `src/core/logger.cpp`

```cpp
// logger.h
namespace kalahari::core {
class Logger {
public:
    static Logger& getInstance();
    void init(const std::string& logFilePath);

    template<typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args);
    // ... info, warn, error

private:
    Logger() = default;
    std::shared_ptr<spdlog::logger> m_logger;
};
}

// Usage:
// Logger::getInstance().info("Application started");
```

**Why:**
- Singleton pattern (global access, lazy initialization)
- spdlog already in vcpkg dependencies
- Thread-safe (spdlog handles internally)
- Needed before GUI for startup logging

#### Step 2: Create KalahariApp class (45 min)
**File:** `src/gui/kalahari_app.h`, `src/gui/kalahari_app.cpp`

```cpp
class KalahariApp : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;

private:
    void initializeLogging();
    void showSplashScreen();  // Placeholder for future
};

// In kalahari_app.cpp
bool KalahariApp::OnInit() {
    // 1. Initialize logging
    initializeLogging();
    Logger::getInstance().info("Kalahari starting...");

    // 2. Set app name for wxConfig
    SetAppName("Kalahari");
    SetVendorName("Kalahari Project");

    // 3. Create main window
    MainWindow* window = new MainWindow();
    window->Show(true);

    Logger::getInstance().info("Main window created");
    return true;
}
```

**Why:**
- wxApp manages event loop, initialization, cleanup
- SetAppName/SetVendorName for config file paths
- Logging initialized before GUI (catch early errors)

#### Step 3: Create MainWindow class (90 min)
**File:** `src/gui/main_window.h`, `src/gui/main_window.cpp`

```cpp
class MainWindow : public wxFrame {
public:
    MainWindow();
    ~MainWindow() override;

private:
    // UI Components
    wxMenuBar* m_menuBar;
    wxToolBar* m_toolBar;
    wxStatusBar* m_statusBar;
    wxPanel* m_mainPanel;  // Placeholder for future editor

    // Event handlers
    void onFileNew(wxCommandEvent& event);
    void onFileOpen(wxCommandEvent& event);
    void onFileSave(wxCommandEvent& event);
    void onFileExit(wxCommandEvent& event);
    void onHelpAbout(wxCommandEvent& event);
    void onClose(wxCloseEvent& event);

    // Helper methods
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void setupMainPanel();

    wxDECLARE_EVENT_TABLE();
};
```

**Components:**

**a) Menu Bar (File, Edit, View, Help):**
```cpp
void MainWindow::createMenuBar() {
    m_menuBar = new wxMenuBar();

    // File menu
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, _("&New\tCtrl+N"), _("Create new document"));
    fileMenu->Append(wxID_OPEN, _("&Open...\tCtrl+O"), _("Open document"));
    fileMenu->Append(wxID_SAVE, _("&Save\tCtrl+S"), _("Save document"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, _("E&xit\tAlt+F4"), _("Exit Kalahari"));
    m_menuBar->Append(fileMenu, _("&File"));

    // Edit menu (stubs)
    wxMenu* editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO, _("&Undo\tCtrl+Z"));
    editMenu->Append(wxID_REDO, _("&Redo\tCtrl+Y"));
    m_menuBar->Append(editMenu, _("&Edit"));

    // View menu (stubs)
    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(wxID_ANY, _("&Fullscreen\tF11"));
    m_menuBar->Append(viewMenu, _("&View"));

    // Help menu
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, _("&About Kalahari"));
    m_menuBar->Append(helpMenu, _("&Help"));

    SetMenuBar(m_menuBar);
}
```

**b) Toolbar (icons placeholders):**
```cpp
void MainWindow::createToolBar() {
    m_toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);

    // Use stock icons for now (platform-native)
    m_toolBar->AddTool(wxID_NEW, _("New"),
        wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR));
    m_toolBar->AddTool(wxID_OPEN, _("Open"),
        wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR));
    m_toolBar->AddTool(wxID_SAVE, _("Save"),
        wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR));

    m_toolBar->Realize();
}
```

**c) Status Bar:**
```cpp
void MainWindow::createStatusBar() {
    m_statusBar = CreateStatusBar(3);  // 3 panes

    // Set pane widths (status text | position | clock)
    int widths[3] = { -1, 150, 150 };
    m_statusBar->SetStatusWidths(3, widths);

    m_statusBar->SetStatusText(_("Ready"), 0);
    m_statusBar->SetStatusText(_("Line 1, Col 1"), 1);
}
```

**d) Main Panel (placeholder):**
```cpp
void MainWindow::setupMainPanel() {
    m_mainPanel = new wxPanel(this, wxID_ANY);

    // Placeholder text (editor will be added in Phase 1)
    wxStaticText* text = new wxStaticText(m_mainPanel, wxID_ANY,
        _("Editor will be implemented in Phase 1\n\nPhase 0 Week 2: GUI Infrastructure"),
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);

    wxFont font(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    text->SetFont(font);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer();
    sizer->Add(text, 0, wxALIGN_CENTER | wxALL, 20);
    sizer->AddStretchSpacer();

    m_mainPanel->SetSizer(sizer);
}
```

**e) Event Handlers (stubs):**
```cpp
void MainWindow::onFileNew(wxCommandEvent& event) {
    Logger::getInstance().info("File -> New clicked");
    wxMessageBox(_("New document (stub)"), _("Info"), wxOK | wxICON_INFORMATION);
}

void MainWindow::onFileExit(wxCommandEvent& event) {
    Logger::getInstance().info("File -> Exit clicked");
    Close(true);
}

void MainWindow::onClose(wxCloseEvent& event) {
    Logger::getInstance().info("Window closing");
    event.Skip();  // Allow default close behavior
}
```

**f) Event Table:**
```cpp
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_MENU(wxID_NEW, MainWindow::onFileNew)
    EVT_MENU(wxID_OPEN, MainWindow::onFileOpen)
    EVT_MENU(wxID_SAVE, MainWindow::onFileSave)
    EVT_MENU(wxID_EXIT, MainWindow::onFileExit)
    EVT_MENU(wxID_ABOUT, MainWindow::onHelpAbout)
    EVT_CLOSE(MainWindow::onClose)
wxEND_EVENT_TABLE()
```

#### Step 4: Update main.cpp (15 min)
**File:** `src/main.cpp`

```cpp
#include "gui/kalahari_app.h"

// Macro creates WinMain() on Windows, main() on Unix
wxIMPLEMENT_APP(kalahari::gui::KalahariApp);
```

**Why:**
- wxIMPLEMENT_APP handles platform-specific entry points
- Replaces current console-based main()

#### Step 5: Update CMakeLists.txt (30 min)
**File:** `src/CMakeLists.txt`

```cmake
# Add new source files
target_sources(kalahari PRIVATE
    main.cpp
    core/logger.cpp
    gui/kalahari_app.cpp
    gui/main_window.cpp
)

# Link wxWidgets (already in vcpkg)
find_package(wxWidgets REQUIRED COMPONENTS core base)
target_link_libraries(kalahari PRIVATE ${wxWidgets_LIBRARIES})
target_include_directories(kalahari PRIVATE ${wxWidgets_INCLUDE_DIRS})

# Link spdlog
find_package(spdlog REQUIRED)
target_link_libraries(kalahari PRIVATE spdlog::spdlog)

# Windows: Set subsystem to GUI (not console)
if(WIN32)
    set_target_properties(kalahari PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
```

#### Step 6: Add i18n placeholders (15 min)
**Files:** `locales/en/kalahari.pot` (template), `locales/pl/kalahari.po` (Polish)

```pot
# kalahari.pot (English template)
msgid ""
msgstr ""
"Content-Type: text/plain; charset=UTF-8\n"

msgid "&File"
msgstr ""

msgid "&New\tCtrl+N"
msgstr ""

# ... all _("...") strings
```

```po
# pl/kalahari.po (Polish translation)
msgid "&File"
msgstr "&Plik"

msgid "&New\tCtrl+N"
msgstr "&Nowy\tCtrl+N"
```

**Note:** Full i18n implementation in Phase 1, this is just structure setup.

### 3. Testing Strategy

**Manual Testing:**
1. Build and run on all 3 platforms (Windows, macOS, Linux)
2. Verify window displays with native look & feel
3. Click all menu items (check MessageBox stubs appear)
4. Click toolbar buttons (check MessageBox stubs appear)
5. Check status bar displays correctly
6. Resize window (check layout adapts)
7. Close window (check logs show proper shutdown)
8. Check log file created in appropriate location

**Log Verification:**
```
[info] Kalahari starting...
[info] Main window created
[info] File -> New clicked
[info] Window closing
```

**Platform-Specific Checks:**
- **Windows:** Check .exe icon, native Windows theme
- **macOS:** Check app bundle, macOS menu bar integration
- **Linux:** Check GTK3 theme, window decorations

**No Unit Tests Yet:**
- This is GUI code (hard to unit test wxWidgets)
- Testing strategy: Manual + CI smoke test (app starts, no crashes)
- Unit testing framework added in Week 3-4 (Python embedding)

### 4. Context7 Usage

**BEFORE coding, fetch wxWidgets docs:**

```
Step 1: resolve-library-id("wxWidgets")
Step 2: get-library-docs(id, topic="wxFrame menu and toolbar")
Step 3: get-library-docs(id, topic="wxStatusBar")
Step 4: get-library-docs(id, topic="wxApp initialization")
```

This ensures we use **wxWidgets 3.3.0+ current API** (not outdated knowledge).

## Implementation Plan (Checklist)

### Prerequisites
- [x] Week 1 complete (CMake, vcpkg, CI/CD working)
- [x] kalahari-wxwidgets skill reviewed
- [ ] Context7 docs fetched (wxFrame, wxApp, wxMenuBar, wxToolBar, wxStatusBar)

### Implementation Steps
- [ ] **Step 1:** Create Logger singleton (logger.h, logger.cpp)
  - [ ] Header with getInstance(), debug/info/warn/error methods
  - [ ] Implementation with spdlog initialization
  - [ ] Test: Log message appears in console/file
- [ ] **Step 2:** Create KalahariApp class (kalahari_app.h, kalahari_app.cpp)
  - [ ] Header with OnInit(), OnExit() overrides
  - [ ] Implementation: initializeLogging(), create MainWindow
  - [ ] Test: App starts, logger initialized
- [ ] **Step 3:** Create MainWindow class (main_window.h, main_window.cpp)
  - [ ] Header with member variables, event handlers
  - [ ] Implementation: Constructor with createMenuBar/ToolBar/StatusBar
  - [ ] createMenuBar() - File, Edit, View, Help menus
  - [ ] createToolBar() - New, Open, Save buttons
  - [ ] createStatusBar() - 3-pane status bar
  - [ ] setupMainPanel() - Placeholder text panel
  - [ ] Event handlers (onFileNew, onFileExit, onHelpAbout, onClose)
  - [ ] Event table (wxBEGIN_EVENT_TABLE)
  - [ ] Test: Window displays, menus/toolbar clickable
- [ ] **Step 4:** Update main.cpp
  - [ ] Replace console main() with wxIMPLEMENT_APP
  - [ ] Test: App launches as GUI (no console on Windows)
- [ ] **Step 5:** Update CMakeLists.txt
  - [ ] Add new source files (logger, kalahari_app, main_window)
  - [ ] Link wxWidgets, spdlog
  - [ ] Set WIN32_EXECUTABLE on Windows
  - [ ] Test: CMake configures, builds on all platforms
- [ ] **Step 6:** Add i18n structure
  - [ ] Create locales/en/kalahari.pot (template)
  - [ ] Create locales/pl/kalahari.po (Polish)
  - [ ] Extract strings with xgettext (manual for now)
  - [ ] Test: Strings compile to .mo files

### Verification
- [ ] Code compiles on all platforms (CI passes)
- [ ] Window displays with native theme (Windows/macOS/Linux)
- [ ] All menu items clickable (stubs show MessageBox)
- [ ] All toolbar buttons clickable (stubs show MessageBox)
- [ ] Status bar shows text in 3 panes
- [ ] Window resizable (layout adapts)
- [ ] Logs written to file (check log output)
- [ ] No memory leaks (valgrind on Linux, if available)
- [ ] No crashes on startup/shutdown

## Risks & Open Questions

### Risks
- **Risk:** wxWidgets 3.3.0 not available in vcpkg baseline
  - **Mitigation:** vcpkg.json already specifies `"version>=": "3.3.1"`, CI passed with this version
- **Risk:** Platform-specific compilation issues (especially Linux GTK3)
  - **Mitigation:** CI already tested wxWidgets deps (Week 1), should work
- **Risk:** spdlog not linking correctly
  - **Mitigation:** spdlog already in vcpkg.json, just need find_package()

### Open Questions
- **Q:** Should we use wxLog* or spdlog for logging?
  - **A:** Use spdlog (more powerful, async, structured, already in stack)
- **Q:** Should we add wxAUI setup now or later?
  - **A:** Add basic infrastructure (m_auiManager member) now, activate in Week 3+ when adding dockable panels
- **Q:** Should we use stock icons or custom icons for toolbar?
  - **A:** Stock icons for Week 2 (wxArtProvider), custom icons in Phase 1 (branding)
- **Q:** Where should log files be stored?
  - **A:** Use wxStandardPaths::Get().GetUserDataDir() + "/logs/kalahari.log" (platform-agnostic)

## Status
- **Created:** 2025-10-26
- **Approved:** ✅ 2025-10-26 (by User - "Zatwierdzam task 00001")
- **Started:** 2025-10-26
- **Completed:** ✅ 2025-10-26

## Implementation Notes

### Compilation Fixes Applied
- **Multi-line comment warning:** Removed trailing backslashes from Windows path comments in `kalahari_app.cpp:66-68`
- **Unused parameter warnings:** Added `[[maybe_unused]]` attribute to all event handler parameters in `main_window.cpp` (8 event handlers)

### .gitignore Fix
- **Issue:** Pattern `core` (for Unix core dumps) was ignoring `src/core/` directory
- **Fix:** Changed `core` to `/core` (only matches at repository root)
- **Impact:** Logger files (`src/core/logger.h`, `src/core/logger.cpp`) are now properly tracked

### Build Statistics
- **Local Linux Build:**
  - Executable size: 166 MB (Debug build with symbols)
  - Build time: ~3 minutes (after vcpkg cache warmed)
  - wxWidgets 3.3.1 statically linked with GTK3 backend
- **CI Build Times:**
  - macOS: 1m 9s
  - Windows: 4m 19s
  - Linux: 4m 36s

### Implementation Decisions
- **Logging paths:** Platform-specific via `wxStandardPaths::GetUserDataDir()`
  - Windows: `C:\Users\<user>\AppData\Roaming\Kalahari Project\Kalahari\logs\`
  - macOS: `~/Library/Application Support/Kalahari/logs/`
  - Linux: `~/.config/kalahari/logs/`
- **Event handlers:** Used event table (`wxBEGIN_EVENT_TABLE`) for compile-time safety over `Bind()` for basic events
- **Stock icons:** Used `wxArtProvider` for toolbar icons (platform-native appearance)
- **i18n:** All UI strings wrapped with `_()` macro for Phase 1 wxLocale integration

## Verification Results

### Local Build (Linux)
✅ **PASSED**
- CMake configuration: Success (1014.8s including vcpkg dependency installation)
- Ninja build: Success (3 compilation units + linking)
- Executable created: `build/bin/kalahari` (166 MB Debug)
- No compilation warnings/errors
- Symbols verified: wxWidgets and GTK3 statically linked

### CI/CD Matrix Builds
✅ **ALL PLATFORMS PASSED**

| Platform | Build Type | Status | Time | Run ID |
|----------|-----------|--------|------|---------|
| macOS | Debug + Release | ✅ SUCCESS | 1m 9s | 18821426899 |
| Windows | Debug + Release | ✅ SUCCESS | 4m 19s | 18821426901 |
| Linux | Debug + Release | ✅ SUCCESS | 4m 36s | 18821426892 |

**Commit:** `733ad81` - "feat: Implement basic wxWidgets GUI window (Phase 0 Week 2)"

### Code Quality
- ✅ No compiler warnings (-Wall -Wextra -Wpedantic -Werror)
- ✅ C++20 standard compliance verified
- ✅ All platforms use consistent code (no platform-specific hacks)
- ✅ Modern C++: `[[maybe_unused]]`, smart pointers, RAII patterns

---

**Task Complexity:** Medium (GUI foundation, new APIs, multi-platform testing)
**Estimated Time:** 5 days (40 hours)
**Blocking:** None (Week 1 complete)
**Blocked By:** None
