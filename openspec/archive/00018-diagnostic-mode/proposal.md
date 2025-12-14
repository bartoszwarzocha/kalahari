# Proposal: Diagnostic Mode & Tools

**Change ID:** `00018-diagnostic-mode`
**Type:** Feature
**Phase:** 1 (Core Editor)
**Estimated Effort:** 3-4 hours
**Task Number:** #00018

---

## Why

Developers and QA engineers need diagnostic tools to:
- Debug application issues
- Verify system state
- Test core systems (Logger, EventBus, Plugins)
- Benchmark performance
- Inspect configuration

Current state: No diagnostic capabilities exist in Qt version.

---

## What Changes

### 1. Command Line Parameter `--diag`

Add `--diag` flag to CmdLineParser:
- Activates diagnostic mode when present
- Logged to console and Log panel
- Enables diagnostic menu in MainWindow

### 2. Advanced Settings Tab

Add new "Advanced" tab to Settings Dialog:
- Checkbox: "Enable Diagnostic Menu"
- Warning label: "⚠️ Warning: Diagnostic tools are for developers only. Some tools may crash the application or consume significant resources."
- Confirmation on enable: Dialog "Are you sure you want to enable diagnostic menu? This exposes advanced debugging tools that may affect application stability."
- NOT saved to settings (runtime only, default: disabled)
- When enabled: Shows diagnostic menu (same as --diag)

### 3. Diagnostic Menu (OUTSIDE Command Registry)

Create "Diagnostics" menu **directly via QAction** (not CommandRegistry):
- Only visible when `--diag` OR Advanced Settings checkbox enabled
- 18 diagnostic tools organized in 6 categories
- All output goes to Log Panel (using Logger)
- NO keyboard shortcuts

### 4. Diagnostic Tools (18 total)

**Category 1: System (3 tools)**
- System Info (Qt version, OS, CPU, RAM, build info)
- Qt Environment (styles, plugin paths, DPI scaling)
- File System Check (permissions, directories, accessibility)

**Category 2: Application State (3 tools)**
- Settings Dump (current settings.json content)
- Memory Stats (allocations, cache usage)
- Open Documents Stats (count, sizes, state)

**Category 3: Core Systems (4 tools)**
- Logger Test (test all log levels: trace/debug/info/warn/error/critical)
- Event Bus Test (send test events, verify delivery)
- Plugin System Check (list plugins, status, paths)
- Command Registry Dump (all registered commands)

**Category 4: Python Environment (4 tools)**
- Python Environment (version, paths, sys.path, site-packages)
- Python Import Test (import kalahari_api, test basic calls)
- Python Memory Test (check interpreter memory usage)
- Embedded Interpreter Status (initialization state, thread safety)

**Category 5: Performance (2 tools)**
- Performance Benchmark (measure I/O, rendering operations)
- Render Stats (FPS, redraw count, widget count)

**Category 6: Quick Actions (2 tools - DEBUG ONLY)**
- Clear Log (wipe Log panel content)
- Force Crash (test exception handling - **Debug builds only**)
- Memory Leak Test (allocate 100MB test data - **Debug builds only**)

---

## Impact

**Affected specs:** `gui/diagnostics` (NEW capability)

**Affected code:**
- Modified: `include/kalahari/core/cmd_line_parser.h` (add isDiagnosticMode())
- Modified: `src/core/cmd_line_parser.cpp` (parse --diag flag)
- Modified: `include/kalahari/gui/main_window.h` (add diagnostic methods, m_diagnosticMode flag)
- Modified: `src/gui/main_window.cpp` (createDiagnosticMenu(), 18 onDiag* slot methods)
- Modified: `include/kalahari/gui/settings_dialog.h` (add Advanced tab, checkbox, m_diagModeCheckbox)
- Modified: `src/gui/settings_dialog.cpp` (create Advanced tab, confirmation dialog)
- Modified: `src/main.cpp` (check --diag, enable diagnostic menu)

**No new files** - all changes in existing codebase

---

## Technical Details

### CmdLineParser Changes

```cpp
// include/kalahari/core/cmd_line_parser.h
class CmdLineParser {
public:
    bool isDiagnosticMode() const { return m_diagnosticMode; }

private:
    bool m_diagnosticMode = false;
};

// src/core/cmd_line_parser.cpp
void CmdLineParser::parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--diag") {
            m_diagnosticMode = true;
        }
        // ... existing parsing
    }
}
```

### MainWindow Changes

```cpp
// include/kalahari/gui/main_window.h
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    void enableDiagnosticMode();
    void disableDiagnosticMode();

private slots:
    void onDiagModeChanged(bool enabled); // Called from SettingsDialog

private:
    void createDiagnosticMenu();
    void removeDiagnosticMenu();

    // 18 diagnostic slot methods
    void onDiagSystemInfo();
    void onDiagQtEnvironment();
    void onDiagFileSystemCheck();
    void onDiagSettingsDump();
    void onDiagMemoryStats();
    void onDiagOpenDocsStats();
    void onDiagLoggerTest();
    void onDiagEventBusTest();
    void onDiagPluginCheck();
    void onDiagCommandRegistryDump();
    void onDiagPythonEnvironment();
    void onDiagPythonImportTest();
    void onDiagPythonMemoryTest();
    void onDiagEmbeddedInterpreterStatus();
    void onDiagPerformanceBenchmark();
    void onDiagRenderStats();
    void onDiagClearLog();
#ifdef _DEBUG
    void onDiagForceCrash();
    void onDiagMemoryLeakTest();
#endif

    bool m_diagnosticMode = false;
    QMenu* m_diagnosticMenu = nullptr;
};
```

### Menu Creation (OUTSIDE Command Registry)

```cpp
void MainWindow::createDiagnosticMenu() {
    if (m_diagnosticMenu) return; // Already created

    m_diagnosticMenu = menuBar()->addMenu(tr("&Diagnostics"));

    // Category 1: System
    m_diagnosticMenu->addAction(tr("System Info"), this, &MainWindow::onDiagSystemInfo);
    m_diagnosticMenu->addAction(tr("Qt Environment"), this, &MainWindow::onDiagQtEnvironment);
    m_diagnosticMenu->addAction(tr("File System Check"), this, &MainWindow::onDiagFileSystemCheck);

    m_diagnosticMenu->addSeparator();

    // Category 2: Application State
    m_diagnosticMenu->addAction(tr("Settings Dump"), this, &MainWindow::onDiagSettingsDump);
    m_diagnosticMenu->addAction(tr("Memory Stats"), this, &MainWindow::onDiagMemoryStats);
    m_diagnosticMenu->addAction(tr("Open Documents Stats"), this, &MainWindow::onDiagOpenDocsStats);

    m_diagnosticMenu->addSeparator();

    // Category 3: Core Systems
    m_diagnosticMenu->addAction(tr("Logger Test"), this, &MainWindow::onDiagLoggerTest);
    m_diagnosticMenu->addAction(tr("Event Bus Test"), this, &MainWindow::onDiagEventBusTest);
    m_diagnosticMenu->addAction(tr("Plugin System Check"), this, &MainWindow::onDiagPluginCheck);
    m_diagnosticMenu->addAction(tr("Command Registry Dump"), this, &MainWindow::onDiagCommandRegistryDump);

    m_diagnosticMenu->addSeparator();

    // Category 4: Python Environment
    m_diagnosticMenu->addAction(tr("Python Environment"), this, &MainWindow::onDiagPythonEnvironment);
    m_diagnosticMenu->addAction(tr("Python Import Test"), this, &MainWindow::onDiagPythonImportTest);
    m_diagnosticMenu->addAction(tr("Python Memory Test"), this, &MainWindow::onDiagPythonMemoryTest);
    m_diagnosticMenu->addAction(tr("Embedded Interpreter Status"), this, &MainWindow::onDiagEmbeddedInterpreterStatus);

    m_diagnosticMenu->addSeparator();

    // Category 5: Performance
    m_diagnosticMenu->addAction(tr("Performance Benchmark"), this, &MainWindow::onDiagPerformanceBenchmark);
    m_diagnosticMenu->addAction(tr("Render Stats"), this, &MainWindow::onDiagRenderStats);

    m_diagnosticMenu->addSeparator();

    // Category 6: Quick Actions (Debug only)
    m_diagnosticMenu->addAction(tr("Clear Log"), this, &MainWindow::onDiagClearLog);
#ifdef _DEBUG
    m_diagnosticMenu->addAction(tr("Force Crash (Debug)"), this, &MainWindow::onDiagForceCrash);
    m_diagnosticMenu->addAction(tr("Memory Leak Test (Debug)"), this, &MainWindow::onDiagMemoryLeakTest);
#endif
}
```

### Advanced Settings Tab

```cpp
// In SettingsDialog::SettingsDialog()
QWidget* advancedTab = new QWidget();
QVBoxLayout* advancedLayout = new QVBoxLayout(advancedTab);

// Warning label
QLabel* warningLabel = new QLabel(tr("⚠️ Warning: Diagnostic tools are for developers only.\n"
    "Some tools may crash the application or consume significant resources."));
warningLabel->setWordWrap(true);
warningLabel->setStyleSheet("QLabel { color: #ff6600; }");
advancedLayout->addWidget(warningLabel);

// Diagnostic mode checkbox
m_diagModeCheckbox = new QCheckBox(tr("Enable Diagnostic Menu"), this);
m_diagModeCheckbox->setChecked(false); // Always start disabled
connect(m_diagModeCheckbox, &QCheckBox::toggled, this, &SettingsDialog::onDiagModeCheckboxToggled);
advancedLayout->addWidget(m_diagModeCheckbox);

advancedLayout->addStretch();

m_tabWidget->addTab(advancedTab, tr("Advanced"));
```

```cpp
void SettingsDialog::onDiagModeCheckboxToggled(bool checked) {
    if (checked) {
        // Show confirmation dialog
        QMessageBox::StandardButton reply = QMessageBox::warning(this,
            tr("Enable Diagnostic Menu"),
            tr("Are you sure you want to enable diagnostic menu?\n\n"
               "This exposes advanced debugging tools that may affect application stability."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::No) {
            m_diagModeCheckbox->setChecked(false); // Revert
            return;
        }

        // Emit signal to MainWindow
        emit diagnosticModeChanged(true);
    } else {
        emit diagnosticModeChanged(false);
    }
}
```

### Example Tool: System Info

```cpp
void MainWindow::onDiagSystemInfo() {
    auto& logger = core::Logger::getInstance();

    logger.info("========================================");
    logger.info("SYSTEM INFO");
    logger.info("========================================");
    logger.info("Qt Version: {}", qVersion());
    logger.info("OS: {}", QSysInfo::prettyProductName().toStdString());
    logger.info("Kernel: {}", QSysInfo::kernelVersion().toStdString());
    logger.info("CPU Architecture: {}", QSysInfo::currentCpuArchitecture().toStdString());
    logger.info("Build ABI: {}", QSysInfo::buildAbi().toStdString());
    logger.info("App Version: {}", QApplication::applicationVersion().toStdString());
    logger.info("Build Date: {} {}", __DATE__, __TIME__);

#ifdef _MSC_VER
    logger.info("Compiler: MSVC {}", _MSC_VER);
#elif defined(__GNUC__)
    logger.info("Compiler: GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
    logger.info("Compiler: Clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif

    logger.info("========================================");
}
```

### Example Tool: Logger Test

```cpp
void MainWindow::onDiagLoggerTest() {
    auto& logger = core::Logger::getInstance();

    logger.info("=== LOGGER TEST START ===");
    logger.trace("This is TRACE level (most verbose)");
    logger.debug("This is DEBUG level");
    logger.info("This is INFO level");
    logger.warn("This is WARN level");
    logger.error("This is ERROR level");
    logger.critical("This is CRITICAL level (most severe)");
    logger.info("=== LOGGER TEST END ===");
}
```

---

## Benefits

1. **Development Speed** - Instant access to system state without debugger
2. **QA Testing** - Verify core systems work correctly
3. **Bug Reports** - Users can include diagnostic output
4. **Performance Monitoring** - Identify bottlenecks
5. **Configuration Debugging** - See actual settings vs expected
6. **Feature Parity** - Restores wxWidgets diagnostic capabilities

---

## Alternatives Considered

### Alternative 1: Integrate with Command Registry

**Pros:** Consistent with other commands
**Cons:**
- Command Registry is for user-facing commands
- Diagnostic tools are dev/QA only
- Pollutes main command namespace
**Verdict:** ❌ Rejected - User requirement: "POZA systemem komend"

### Alternative 2: Separate Diagnostic Window

**Pros:** Dedicated UI for diagnostics
**Cons:**
- More complex implementation
- Log Panel already exists and works well
- Extra window clutters workspace
**Verdict:** ❌ Rejected - Log Panel sufficient

### Alternative 3: Always Show Diagnostic Menu

**Pros:** No --diag flag needed
**Cons:**
- Confuses end users
- Exposes dev tools in production
- No security isolation
**Verdict:** ❌ Rejected - --diag flag is cleaner

---

## Testing Strategy

### Manual Testing

For each diagnostic tool:
1. Launch with `kalahari.exe --diag`
2. Verify "Diagnostics" menu visible
3. Click each menu item
4. Verify output appears in Log Panel
5. Verify output is correct and useful

### Edge Cases

- **No --diag flag:** Diagnostic menu NOT visible
- **--diag with other flags:** Both work correctly
- **Force Crash:** Application crashes as expected (test exception handling)
- **Memory Leak Test:** Memory increases, then released on exit

### Platform Testing

- Windows: All tools work
- Linux (WSL): All tools work
- macOS: All tools work (future)

---

## Success Criteria

- [ ] `--diag` flag parsed correctly by CmdLineParser
- [ ] Diagnostic menu only visible when `--diag` active
- [ ] Diagnostic menu NOT visible without `--diag`
- [ ] All 15 diagnostic tools implemented
- [ ] All tools output to Log Panel (via Logger)
- [ ] Menu organized with separators between categories
- [ ] Keyboard shortcuts work (Ctrl+Shift+I, etc.)
- [ ] System Info shows correct Qt/OS/compiler info
- [ ] Logger Test shows all 6 log levels
- [ ] Clear Log actually clears Log Panel
- [ ] Force Crash crashes application (Debug mode)
- [ ] No integration with Command Registry (as required)
- [ ] Build successful on Windows
- [ ] No compiler warnings

---

## Follow-up Tasks (Phase 2+)

**Phase 2:**
- [ ] Add "Export Diagnostics" (save to file)
- [ ] Add "Copy Diagnostics" (clipboard)
- [ ] Add "Email Diagnostics" (send to support)

**Phase 3:**
- [ ] Remote diagnostics (web interface)
- [ ] Diagnostic history (save previous runs)
- [ ] Custom diagnostic scripts (user-defined)

---

## Risks & Mitigation

### Risk 1: Force Crash Accidentally Triggered

**Probability:** Low
**Impact:** High (data loss)
**Mitigation:**
- Show confirmation dialog: "This will crash the application. Unsaved data will be lost. Continue?"
- Only available in Debug builds (#ifdef _DEBUG)

### Risk 2: Memory Leak Test Causes OOM

**Probability:** Low
**Impact:** Medium (application crash)
**Mitigation:**
- Allocate only 100MB (safe on modern systems)
- Show warning: "This will allocate 100MB of memory. Continue?"

### Risk 3: Diagnostic Output Floods Log Panel

**Probability:** Medium
**Impact:** Low (hard to read)
**Mitigation:**
- Use separators (===) before/after each diagnostic
- Add timestamps to each log entry (Logger already does this)
- Provide "Clear Log" tool

---

## References

- **wxWidgets archive:** Diagnostic mode implementation (check old codebase)
- **Qt Documentation:** QSysInfo (https://doc.qt.io/qt-6/qsysinfo.html)
- **CLAUDE.md:** Diagnostic tools best practices
- **Phase 1 Roadmap:** Task #00018 slot

---

**Status:** ✅ APPROVED - Ready to implement
**Completed Planning:** 2025-11-22
**Implementation Started:** 2025-11-22
**Estimated Timeline:** ~4 hours implementation
