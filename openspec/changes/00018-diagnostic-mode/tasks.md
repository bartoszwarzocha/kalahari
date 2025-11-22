# Tasks: Diagnostic Mode & Tools

**Change ID:** `00018-diagnostic-mode`

---

## Task List

### 1. Update CmdLineParser (--diag flag)
- Add `isDiagnosticMode()` method
- Parse `--diag` flag in parse()
- **Time:** 15 min
- **Status:** ⏳ Pending

### 2. Add Advanced Settings tab
- Create Advanced tab in SettingsDialog
- Add warning label (orange text)
- Add "Enable Diagnostic Menu" checkbox
- Add confirmation dialog on enable
- Connect signal to MainWindow
- **Time:** 30 min
- **Status:** ⏳ Pending

### 3. Update MainWindow for diagnostic mode
- Add `enableDiagnosticMode()`, `disableDiagnosticMode()`
- Add `onDiagModeChanged(bool)` slot
- Add `createDiagnosticMenu()`, `removeDiagnosticMenu()`
- Add `m_diagnosticMode` flag
- Add `m_diagnosticMenu` pointer
- **Time:** 20 min
- **Status:** ⏳ Pending

### 4. Create Diagnostic Menu (18 tools)
- **Category 1: System** (3 tools: System Info, Qt Environment, File System Check)
- **Category 2: Application State** (3 tools: Settings Dump, Memory Stats, Open Docs Stats)
- **Category 3: Core Systems** (4 tools: Logger Test, Event Bus Test, Plugin Check, Command Registry Dump)
- **Category 4: Python** (4 tools: Python Env, Import Test, Memory Test, Interpreter Status)
- **Category 5: Performance** (2 tools: Benchmark, Render Stats)
- **Category 6: Quick Actions** (2 tools: Clear Log, Force Crash/Memory Leak - Debug only)
- **Time:** 45 min
- **Status:** ⏳ Pending

### 5. Implement diagnostic tools (18 methods)
- onDiagSystemInfo()
- onDiagQtEnvironment()
- onDiagFileSystemCheck()
- onDiagSettingsDump()
- onDiagMemoryStats()
- onDiagOpenDocsStats()
- onDiagLoggerTest()
- onDiagEventBusTest()
- onDiagPluginCheck()
- onDiagCommandRegistryDump()
- onDiagPythonEnvironment()
- onDiagPythonImportTest()
- onDiagPythonMemoryTest()
- onDiagEmbeddedInterpreterStatus()
- onDiagPerformanceBenchmark()
- onDiagRenderStats()
- onDiagClearLog()
- onDiagForceCrash() [#ifdef _DEBUG]
- onDiagMemoryLeakTest() [#ifdef _DEBUG]
- **Time:** 90 min
- **Status:** ⏳ Pending

### 6. Update main.cpp
- Check cmdLine.isDiagnosticMode()
- Call mainWindow.enableDiagnosticMode()
- Log diagnostic mode status
- **Time:** 10 min
- **Status:** ⏳ Pending

### 7. Build and test
- Build with scripts/build_windows.bat
- Test --diag flag
- Test Advanced Settings checkbox
- Test all 18 diagnostic tools
- Verify output in Log Panel
- Verify Debug-only tools (#ifdef _DEBUG)
- **Time:** 45 min
- **Status:** ⏳ Pending

### 8. Update documentation
- Update CHANGELOG.md
- **Time:** 10 min
- **Status:** ⏳ Pending

### 9. Commit changes
- Git commit with proper message
- **Time:** 5 min
- **Status:** ⏳ Pending

---

**Total Estimated Time:** ~4 hours
