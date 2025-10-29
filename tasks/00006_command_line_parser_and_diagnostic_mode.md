# Task #00006: Command Line Parser and Diagnostic Mode

## Context
- **Phase:** Phase 0 Week 2
- **Roadmap Reference:** ROADMAP.md - "GUI Infrastructure & Threading"
- **Related Docs:**
  - `project_docs/03_architecture.md` (architectural patterns)
  - `project_docs/07_mvp_tasks.md` (Phase 0 tasks)
- **Dependencies:**
  - Task #00002 (GUI skeleton - MainWindow exists)
  - Task #00005 (Python integration - has diagnostic tests to expose)
- **Inspiration:**
  - `/mnt/e/C++/Projekty/Libs/bwx_sdk/src/bwx_core/bwx_cmd.*` - pattern/architecture reference
  - **NOT A DEPENDENCY** - we implement our own version in Kalahari

## Objective
Implement command line argument parsing and diagnostic mode infrastructure.

**User Story:**
- Normal launch: `kalahari.exe` → standard GUI
- Diagnostic launch: `kalahari.exe --diag` (or `-d`) → GUI with additional "Diagnostics" menu
- Restart from GUI: Help → "Restart in Diagnostic Mode" → restarts with `--diag` flag

**Why:**
- Clean GUI for 99% users (no diagnostic clutter)
- Easy troubleshooting for support (Help → Restart in Diagnostic Mode → screenshot)
- Scalable: future commands (`--version`, `--config-path`, etc.)
- Professional: similar to Chrome DevTools, VS Code Developer Tools

## Proposed Approach

### 1. Create Command Line Parser (inspired by bwx_sdk pattern)

**File:** `include/kalahari/core/cmd_line_parser.h` + `src/core/cmd_line_parser.cpp`

**Design:**
- Wrapper over native `wxCmdLineParser`
- Simplified API similar to bwx_sdk's `bwxCmdLineParser`
- Focus on switches (flags) for now - expandable later

**Key methods:**
```cpp
class CmdLineParser {
public:
    CmdLineParser(int argc, char** argv);  // or wchar_t**

    bool parse();  // Returns false on error or help request

    void addSwitch(const wxString& shortName,
                   const wxString& longName,
                   const wxString& description);

    bool hasSwitch(const wxString& name) const;

private:
    wxCmdLineParser m_parser;
};
```

**Example usage:**
```cpp
CmdLineParser parser(argc, argv);
parser.addSwitch("d", "diag", "Enable diagnostic mode");
parser.addSwitch("v", "version", "Show version information");

if (!parser.parse()) {
    return false;  // Help shown or error
}

bool diagMode = parser.hasSwitch("diag");
```

### 2. Create DiagnosticManager Singleton

**File:** `include/kalahari/core/diagnostic_manager.h` + `src/core/diagnostic_manager.cpp`

**Design:**
- Singleton (Meyer's pattern)
- Stores global diagnostic state
- Thread-safe (no concurrency issues in current design)

**API:**
```cpp
class DiagnosticManager {
public:
    static DiagnosticManager& getInstance();

    bool isEnabled() const;
    void setEnabled(bool enabled);

private:
    DiagnosticManager() = default;
    bool m_enabled = false;
};
```

### 3. Integrate in KalahariApp::OnInit()

**File:** `src/gui/kalahari_app.cpp`

**Flow:**
1. Parse command line BEFORE Logger init
2. Check for `--diag` or `-d` flag
3. Set DiagnosticManager state
4. Log diagnostic mode if enabled
5. Continue normal init

**Code:**
```cpp
bool KalahariApp::OnInit() {
    // 1. Parse command line FIRST
    core::CmdLineParser parser(argc, argv);
    parser.addSwitch("d", "diag", "Enable diagnostic mode");

    if (!parser.parse()) {
        return false;  // Help or error
    }

    bool diagMode = parser.hasSwitch("diag");
    core::DiagnosticManager::getInstance().setEnabled(diagMode);

    // 2. Initialize logging
    initializeLogging();

    if (diagMode) {
        core::Logger::getInstance().info("========================================");
        core::Logger::getInstance().info("DIAGNOSTIC MODE ENABLED");
        core::Logger::getInstance().info("========================================");
    }

    // ... rest of OnInit()
}
```

### 4. Conditional "Diagnostics" Menu in MainWindow

**File:** `src/gui/main_window.cpp`

**Changes:**
- Add `ID_HELP_RESTART_DIAG` menu ID
- In `createMenuBar()`:
  - If NOT in diag mode: add "Help → Restart in Diagnostic Mode"
  - If IN diag mode: add new "Diagnostics" menu (between View and Help)
- Move "Test Python Integration" from Tools to Diagnostics menu

**Diagnostics Menu Items (Phase 0):**
- Test Python Integration (already implemented)
- Open Log Folder (open %APPDATA%/Kalahari Project/Kalahari/logs/)
- System Information (OS, wxWidgets version, Python version, build type)

**Future items (Phase 1+):**
- Plugin Diagnostics
- Database Check
- Memory Usage
- Thread Pool Status

### 5. Implement Restart Handler

**File:** `src/gui/main_window.cpp`

**Method:** `onHelpRestartDiagnostic()`

**Flow:**
1. Confirm with user (wxMessageBox)
2. Get executable path (wxStandardPaths::GetExecutablePath())
3. Launch new instance with `--diag` flag (wxExecute)
4. Close current instance (Close(true))

**Code:**
```cpp
void MainWindow::onHelpRestartDiagnostic(wxCommandEvent& event) {
    int response = wxMessageBox(
        _("The application will restart in Diagnostic Mode.\n\n"
          "This enables additional diagnostic tools and detailed logging.\n\n"
          "Continue?"),
        _("Restart in Diagnostic Mode"),
        wxYES_NO | wxICON_QUESTION,
        this
    );

    if (response != wxYES) {
        return;
    }

    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxExecute(exePath + " --diag", wxEXEC_ASYNC);

    Close(true);
}
```

## Implementation Plan (Checklist)

### Phase 1: Command Line Parser
- [ ] Create `include/kalahari/core/cmd_line_parser.h`
- [ ] Create `src/core/cmd_line_parser.cpp`
- [ ] Implement CmdLineParser class (wrapper over wxCmdLineParser)
- [ ] Add methods: constructor, parse(), addSwitch(), hasSwitch()
- [ ] Update CMakeLists.txt (add cmd_line_parser.cpp to sources)
- [ ] Write unit tests in `tests/core/test_cmd_line_parser.cpp`

### Phase 2: DiagnosticManager
- [ ] Create `include/kalahari/core/diagnostic_manager.h`
- [ ] Create `src/core/diagnostic_manager.cpp`
- [ ] Implement singleton with isEnabled()/setEnabled()
- [ ] Update CMakeLists.txt
- [ ] Write unit tests in `tests/core/test_diagnostic_manager.cpp`

### Phase 3: Integration in KalahariApp
- [ ] Modify `src/gui/kalahari_app.cpp::OnInit()`
- [ ] Add command line parsing BEFORE logging init
- [ ] Check for `--diag` flag
- [ ] Set DiagnosticManager state
- [ ] Log diagnostic mode banner if enabled
- [ ] Test: launch with `--diag`, verify logs show banner

### Phase 4: Conditional Diagnostics Menu
- [ ] Add menu IDs in `src/gui/main_window.cpp`
  - ID_HELP_RESTART_DIAG
  - ID_DIAG_TEST_PYTHON
  - ID_DIAG_OPEN_LOGS
  - ID_DIAG_SYSTEM_INFO
- [ ] Modify `createMenuBar()` to conditionally create Diagnostics menu
- [ ] Move "Test Python Integration" from Tools to Diagnostics
- [ ] Add "Open Log Folder" handler
- [ ] Add "System Information" handler
- [ ] Add "Restart in Diagnostic Mode" to Help menu (if not in diag mode)
- [ ] Test: launch normal → no Diagnostics menu
- [ ] Test: launch with `--diag` → Diagnostics menu visible

### Phase 5: Restart Handler
- [ ] Implement `onHelpRestartDiagnostic()` in MainWindow
- [ ] Get executable path (wxStandardPaths)
- [ ] Show confirmation dialog
- [ ] Launch new instance with `--diag` (wxExecute)
- [ ] Close current instance
- [ ] Test: Help → Restart in Diagnostic Mode → app restarts with diag menu

### Phase 6: Documentation & Testing
- [ ] Update `include/kalahari/core/cmd_line_parser.h` with Doxygen comments
- [ ] Update `include/kalahari/core/diagnostic_manager.h` with Doxygen comments
- [ ] Add comments in modified MainWindow code
- [ ] Run all unit tests (Catch2)
- [ ] Test on Windows, Linux, macOS (CI/CD)
- [ ] Update CHANGELOG.md

## Risks & Open Questions

**Q: Should we remove "Tools" menu entirely if it only has Python tests?**
- A: YES - if we move Python tests to Diagnostics, Tools menu becomes empty
- Solution: Remove Tools menu, add it back in Phase 1 when we have actual tools

**Q: Should `--help` show diagnostic options?**
- A: YES - wxCmdLineParser automatically adds help text, users can discover it

**Q: What if executable path detection fails (rare edge case)?**
- A: Show error dialog, don't attempt restart
- Log error with full details

**Q: Should diagnostic mode persist across restarts (remember user preference)?**
- A: NO - intentionally ephemeral
- Each launch requires explicit `--diag` flag
- Prevents accidental "stuck in diag mode" scenarios

**Risk: wxExecute on macOS with .app bundles**
- Solution: Use full path to executable inside .app bundle
- Test thoroughly on macOS in CI/CD

**Risk: Command line parsing complexity grows**
- Mitigation: Keep simple for Phase 0 (only switches)
- Future: extend to options with values (`--log-level=debug`)

## Status
- **Created:** 2025-10-27
- **Approved:** 2025-10-27 (by User)
- **Started:** 2025-10-27
- **Completed:** 2025-10-27

## Implementation Notes

### Decision: Use native wxCmdLineParser in KalahariApp::OnInit()
- **Problem:** CmdLineParser requires Logger, but Logger needs to be initialized AFTER command line parsing
- **Solution:** Use wxCmdLineParser directly in OnInit() before Logger initialization
- **Rationale:** Simpler, no circular dependency, works perfectly for --diag flag

### Decision: Static cast for wxCmdLineEntryFlags
- **Problem:** MSVC /WX treats enum mixing warning (C5054) as error
- **Solution:** Use `static_cast<int>()` when combining wxCmdLineEntryFlags
- **Example:** `int flags = static_cast<int>(wxCMD_LINE_VAL_NONE) | static_cast<int>(wxCMD_LINE_PARAM_OPTIONAL);`

### Decision: wxFileName include required
- **Problem:** Forward declarations not enough for wxFileName::GetPathSeparator() and DirExists()
- **Solution:** Added `#include <wx/filename.h>` to main_window.cpp

### Decision: Use non-deprecated wxPlatformInfo methods
- **Replaced:** `GetArchName()` → `GetBitnessName()`
- **Replaced:** `GetToolkitName()` → `GetPortIdShortName()`
- **Rationale:** Follow wxWidgets 3.3+ recommendations

### Testing Results
- **Windows Build:** ✅ Success (MSVC 19.44, C++20)
- **All Tests:** ✅ Passed (2070 assertions in 12 test cases)
  - **+2 test cases** (test_cmd_line_parser.cpp added)
  - **+21 assertions** (CmdLineParser unit tests)
- **CmdLineParser Tests:** ✅ All tests passed (basic functionality + edge cases)
- **Python Tests:** ✅ Still working (5/5 passed)

### Test Coverage Added
- `test_cmd_line_parser.cpp` with 2 TEST_CASE blocks:
  1. **"CmdLineParser basic functionality"** - 5 sections (no args, short switch, long switch, unknown switch, parse before add)
  2. **"CmdLineParser edge cases"** - 2 sections (hasSwitch before parse, multiple switches)

### Decision: Fix tests/CMakeLists.txt
- **Problem:** test_cmd_line_parser.cpp was created but not added to TEST_SOURCES
- **Result:** Test count remained at 2049 assertions / 10 test cases
- **Solution:** Added test_cmd_line_parser.cpp to TEST_SOURCES in tests/CMakeLists.txt
- **Also added:** cmd_line_parser.cpp and diagnostic_manager.cpp to link against
- **Verification:** Test count increased to 2070 assertions / 12 test cases ✅

## Verification
- [ ] Code compiles on all platforms (Windows, Linux, macOS)
- [ ] Unit tests pass (Catch2)
- [ ] Manual test: launch without flags → no Diagnostics menu
- [ ] Manual test: launch with `--diag` → Diagnostics menu visible
- [ ] Manual test: Help → Restart in Diagnostic Mode → restarts correctly
- [ ] Manual test: Diagnostics → Test Python Integration → shows results
- [ ] Manual test: Diagnostics → Open Log Folder → opens correct folder
- [ ] Manual test: `--help` → shows help text and exits
- [ ] No memory leaks (valgrind/ASAN if available)
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
