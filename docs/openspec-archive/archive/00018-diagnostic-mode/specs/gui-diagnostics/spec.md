# Specification: Diagnostic Mode & Tools

**Capability ID:** `gui/diagnostics`
**Type:** GUI Feature
**Phase:** 1 (Core Editor)
**Status:** New capability (ADDED)

---

## ADDED Requirements

### Requirement: Command Line Diagnostic Mode

The application SHALL support `--diag` command line parameter to enable diagnostic mode.

**ID:** `gui/diagnostics/cli-mode`
**Priority:** Medium
**Phase:** 1

**Rationale:** Developers need quick access to diagnostic tools without navigating UI.

#### Scenario: User launches with --diag

```
GIVEN the user runs application with --diag parameter
WHEN the application starts
THEN diagnostic mode SHALL be enabled
AND "Diagnostics" menu SHALL be visible in menu bar
AND log SHALL contain "DIAGNOSTIC MODE ENABLED" message
```

---

### Requirement: Advanced Settings Diagnostic Toggle

The application SHALL provide a checkbox in Advanced Settings to enable diagnostic menu at runtime.

**ID:** `gui/diagnostics/settings-toggle`
**Priority:** Medium
**Phase:** 1

**Rationale:** Users working with support may need diagnostic tools without restart.

#### Scenario: User enables diagnostic menu via Settings

```
GIVEN the user opens Edit → Settings → Advanced tab
WHEN the user checks "Enable Diagnostic Menu" checkbox
THEN a confirmation dialog SHALL appear
AND dialog SHALL warn about stability risks
AND if user confirms, diagnostic menu SHALL appear
AND if user cancels, checkbox SHALL revert to unchecked
```

#### Scenario: Checkbox not saved between sessions

```
GIVEN the user enables diagnostic menu via checkbox
WHEN the user restarts application
THEN checkbox SHALL be unchecked (default state)
AND diagnostic menu SHALL NOT be visible
```

---

### Requirement: Diagnostic Menu

The application SHALL provide a "Diagnostics" menu with 18 diagnostic tools organized in 6 categories.

**ID:** `gui/diagnostics/menu`
**Priority:** Medium
**Phase:** 1

**Rationale:** Centralized access to all diagnostic capabilities.

#### Scenario: Diagnostic menu structure

```
GIVEN diagnostic mode is enabled
THEN "Diagnostics" menu SHALL appear in menu bar
AND menu SHALL contain 6 categories separated by dividers
AND categories SHALL be: System, Application State, Core Systems, Python Environment, Performance, Quick Actions
AND menu SHALL NOT use Command Registry (direct QAction)
AND menu items SHALL NOT have keyboard shortcuts
```

---

### Requirement: System Diagnostic Tools

The application SHALL provide 3 system diagnostic tools.

**ID:** `gui/diagnostics/system-tools`
**Priority:** Medium
**Phase:** 1

**Rationale:** Verify Qt installation, OS compatibility, file system access.

#### Scenario: System Info tool

```
GIVEN diagnostic menu is visible
WHEN user selects "System Info"
THEN Log Panel SHALL display:
  - Qt version
  - OS name and version
  - Kernel version
  - CPU architecture
  - Compiler info
  - Application version
  - Build date/time
```

#### Scenario: Qt Environment tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Qt Environment"
THEN Log Panel SHALL display:
  - Available Qt styles
  - Plugin paths
  - DPI scaling info
  - High DPI mode status
```

#### Scenario: File System Check tool

```
GIVEN diagnostic menu is visible
WHEN user selects "File System Check"
THEN Log Panel SHALL display:
  - Application directory path
  - Settings file path and permissions
  - Log file path and permissions
  - Temp directory accessibility
```

---

### Requirement: Application State Diagnostic Tools

The application SHALL provide 3 application state diagnostic tools.

**ID:** `gui/diagnostics/app-state-tools`
**Priority:** Medium
**Phase:** 1

**Rationale:** Debug configuration issues, memory leaks, document management.

#### Scenario: Settings Dump tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Settings Dump"
THEN Log Panel SHALL display complete settings.json content
```

#### Scenario: Memory Stats tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Memory Stats"
THEN Log Panel SHALL display:
  - Process memory usage
  - Heap allocations
  - Cache size
```

#### Scenario: Open Documents Stats tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Open Documents Stats"
THEN Log Panel SHALL display:
  - Number of open documents
  - Document sizes
  - Document states (modified/saved)
```

---

### Requirement: Core Systems Diagnostic Tools

The application SHALL provide 4 core systems diagnostic tools.

**ID:** `gui/diagnostics/core-systems-tools`
**Priority:** High
**Phase:** 1

**Rationale:** Verify Logger, EventBus, PluginManager, CommandRegistry functionality.

#### Scenario: Logger Test tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Logger Test"
THEN Log Panel SHALL display test messages at all 6 log levels:
  - TRACE
  - DEBUG
  - INFO
  - WARN
  - ERROR
  - CRITICAL
```

#### Scenario: Event Bus Test tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Event Bus Test"
THEN application SHALL send test events
AND Log Panel SHALL display event delivery confirmation
```

#### Scenario: Plugin System Check tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Plugin System Check"
THEN Log Panel SHALL display:
  - Plugin directory path
  - List of installed plugins
  - Plugin statuses (loaded/failed)
  - Plugin API version
```

#### Scenario: Command Registry Dump tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Command Registry Dump"
THEN Log Panel SHALL display all registered commands with IDs and shortcuts
```

---

### Requirement: Python Environment Diagnostic Tools

The application SHALL provide 4 Python environment diagnostic tools.

**ID:** `gui/diagnostics/python-tools`
**Priority:** High
**Phase:** 1

**Rationale:** Debug Python embedding issues, verify kalahari_api module.

#### Scenario: Python Environment tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Python Environment"
THEN Log Panel SHALL display:
  - Python version
  - Python home path
  - sys.path contents
  - site-packages location
  - Embedded interpreter status
```

#### Scenario: Python Import Test tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Python Import Test"
THEN application SHALL attempt to import kalahari_api module
AND Log Panel SHALL display import result (success/failure)
AND if successful, SHALL test basic API calls
```

#### Scenario: Python Memory Test tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Python Memory Test"
THEN Log Panel SHALL display:
  - Python interpreter memory usage
  - Number of Python objects alive
  - GC statistics
```

#### Scenario: Embedded Interpreter Status tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Embedded Interpreter Status"
THEN Log Panel SHALL display:
  - Interpreter initialization state
  - Thread safety mode
  - GIL status
  - Active Python threads
```

---

### Requirement: Performance Diagnostic Tools

The application SHALL provide 2 performance diagnostic tools.

**ID:** `gui/diagnostics/performance-tools`
**Priority:** Low
**Phase:** 1

**Rationale:** Identify performance bottlenecks during development.

#### Scenario: Performance Benchmark tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Performance Benchmark"
THEN application SHALL measure:
  - File I/O speed (read/write 1MB test file)
  - JSON parsing speed
  - Qt widget creation time
AND Log Panel SHALL display benchmark results in ms
```

#### Scenario: Render Stats tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Render Stats"
THEN Log Panel SHALL display:
  - Current FPS (if measurable)
  - Number of widgets in main window
  - Redraw count since startup
```

---

### Requirement: Quick Action Diagnostic Tools

The application SHALL provide diagnostic quick actions, with some restricted to Debug builds only.

**ID:** `gui/diagnostics/quick-actions`
**Priority:** Low
**Phase:** 1

**Rationale:** Utility actions for development and testing.

#### Scenario: Clear Log tool

```
GIVEN diagnostic menu is visible
WHEN user selects "Clear Log"
THEN Log Panel content SHALL be cleared
```

#### Scenario: Force Crash tool (Debug only)

```
GIVEN diagnostic menu is visible
AND application is built in Debug mode
WHEN user selects "Force Crash (Debug)"
THEN confirmation dialog SHALL appear
AND if user confirms, application SHALL throw exception and crash
```

#### Scenario: Force Crash not available in Release

```
GIVEN diagnostic menu is visible
AND application is built in Release mode
THEN "Force Crash" menu item SHALL NOT be visible
```

#### Scenario: Memory Leak Test tool (Debug only)

```
GIVEN diagnostic menu is visible
AND application is built in Debug mode
WHEN user selects "Memory Leak Test (Debug)"
THEN confirmation dialog SHALL appear
AND if user confirms, application SHALL allocate 100MB of test data
AND Log Panel SHALL display allocation result
```

---

### Requirement: Diagnostic Output Format

All diagnostic tools SHALL output to Log Panel using Logger with consistent formatting.

**ID:** `gui/diagnostics/output-format`
**Priority:** Medium
**Phase:** 1

**Rationale:** Readable, searchable diagnostic output.

#### Scenario: Output uses separators

```
GIVEN any diagnostic tool is executed
THEN output SHALL start with "========================================"
AND output SHALL contain tool name
AND output SHALL end with "========================================"
```

#### Scenario: Output includes timestamps

```
GIVEN any diagnostic tool is executed
THEN each log entry SHALL include timestamp (via Logger)
```

---

## Technical Notes

### Implementation Files

**Modified files:**
- `include/kalahari/core/cmd_line_parser.h` (~5 new lines)
- `src/core/cmd_line_parser.cpp` (~10 new lines)
- `include/kalahari/gui/main_window.h` (~25 new lines)
- `src/gui/main_window.cpp` (~500 new lines - 18 diagnostic methods)
- `include/kalahari/gui/settings_dialog.h` (~5 new lines)
- `src/gui/settings_dialog.cpp` (~50 new lines - Advanced tab)
- `src/main.cpp` (~5 new lines)

**Total LOC:** ~600 new lines

### Dependencies

- No new third-party dependencies
- Uses existing Logger, SettingsManager, PluginManager, EventBus, CommandRegistry
- Uses Qt6::Core (QSysInfo, QStorageInfo)
- Uses Python3 (PyRun_SimpleString, Py_GetVersion)

### Testing Strategy

**Phase 1:** Manual testing only
- Test --diag flag
- Test Advanced Settings checkbox
- Test all 18 diagnostic tools
- Verify Debug-only tools
- Verify output format

**Phase 2+:** Consider automated tests
- Unit tests for diagnostic methods
- Integration tests for menu creation

---

## Future Enhancements (Post-Phase 1)

### Phase 2

- [ ] Export diagnostic results to file
- [ ] Copy diagnostic results to clipboard
- [ ] Diagnostic history (save previous runs)

### Phase 3+

- [ ] Remote diagnostics (web interface for support)
- [ ] Custom diagnostic scripts (user-defined)
- [ ] Automatic diagnostic on crash

---

**Specification Version:** 1.0
**Status:** ⏳ Awaiting implementation
**Phase:** 1 (Core Editor)
**Change ID:** `00018-diagnostic-mode`
