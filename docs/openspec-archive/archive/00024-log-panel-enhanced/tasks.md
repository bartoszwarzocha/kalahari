# Tasks: Enhanced Log Panel (Theme-Aware, Mode-Restricted)

**Status:** DEPLOYED

**Created:** 2025-11-27
**Completed:** 2025-11-27

## Summary

Enhance LogPanel from placeholder to full-featured diagnostic log viewer with:
- Real-time spdlog integration via custom Qt sink
- Colored output per log level (theme-aware)
- Ring buffer for memory management
- Mode-based visibility (normal vs --diag/--dev)
- Vertical toolbar (Options, Open Folder, Copy, Clear)

## Phase 1: Core Infrastructure

- [x] Create `LogPanelSink` class (spdlog custom sink)
  - Inherits from `spdlog::sinks::base_sink<std::mutex>`
  - Emits Qt signal `logMessage(int level, QString message)`
  - Thread-safe (spdlog calls from any thread)

- [x] Create `LogEntry` struct for ring buffer
  ```cpp
  struct LogEntry {
      int level;
      QString message;
  };
  ```

- [x] Implement ring buffer in LogPanel
  - `std::deque<LogEntry> m_logBuffer`
  - `size_t m_maxBufferSize = 500`
  - Auto-trim oldest when full

- [x] Register sink in main.cpp
  - Create LogPanelSink before MainWindow
  - Add to Logger sinks
  - Pass sink pointer to MainWindow/LogPanel

## Phase 2: Enhanced UI

- [x] Replace QPlainTextEdit with QTextEdit
  - QTextEdit supports rich text (colors)
  - Read-only mode

- [x] Implement `appendLog(level, message)`
  - Format with timestamp
  - Apply color based on level
  - Handle ring buffer overflow
  - Skip UI update when panel hidden (optimization)

- [x] Add vertical toolbar
  - QToolBar with Qt::Vertical orientation
  - 4 buttons: Options, Open Folder, Copy, Clear
  - Use ArtProvider for icons

- [x] Implement toolbar actions
  - `onOptions()` - Open Settings Dialog (via signal)
  - `onOpenLogFolder()` - Platform-specific file explorer
  - `onCopyToClipboard()` - Copy buffer to clipboard
  - `onClearLog()` - Clear display and buffer

## Phase 3: Theme Integration

- [x] Define log color schemes
  - Light theme colors (dark text)
  - Dark theme colors (light text)
  - Stored in Theme struct (theme.log.*)

- [x] Connect to ThemeManager::themeChanged
  - Update colors on theme switch
  - Call `rebuildDisplay()`

- [x] Implement `rebuildDisplay()`
  - Clear QTextEdit
  - Re-append all buffer entries with new colors
  - Use single setHtml() for performance

## Phase 4: Mode-Based Visibility

- [x] Parse command-line flags in main.cpp
  - Detect `--diag` and `--dev` flags
  - Store in application-wide flag

- [x] Pass mode to MainWindow
  - Via enableDiagnosticMode() / enableDevMode()
  - Store as member variable

- [x] Set initial panel visibility
  - Normal mode: `m_logDock->hide()`
  - Diag/Dev mode: `m_logDock->show()`

- [x] Implement log level filtering
  - Normal mode: Filter out TRACE, DEBUG
  - Diag/Dev mode: Show all levels
  - Filter in `shouldDisplayLevel()` based on mode

## Phase 5: Settings UI

- [x] Add "Log" page to Settings Dialog
  - Under Advanced category
  - Buffer size spinner (1-1000)

- [x] Implement settings persistence
  - Save to SettingsManager (log.bufferSize)
  - Apply via MainWindow::onApplySettings()

## Performance Optimizations (Added)

- [x] ArtProvider batch mode
  - Prevents multiple resourcesChanged emissions during settings apply
  - Coalesces ~11 signals into 1

- [x] LogPanel visibility optimization
  - Skip UI updates when panel hidden
  - Rebuild display on showEvent()

## Files to Create

| File | Purpose |
|------|---------|
| `include/kalahari/core/log_panel_sink.h` | spdlog Qt sink header |
| `src/core/log_panel_sink.cpp` | spdlog Qt sink implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `include/kalahari/gui/panels/log_panel.h` | Full interface |
| `src/gui/panels/log_panel.cpp` | Full implementation |
| `src/main.cpp` | Add sink, pass mode flags |
| `src/gui/main_window.h` | Mode flag member |
| `src/gui/main_window.cpp` | Mode-based visibility |
| `src/CMakeLists.txt` | Add new source files |

## Dependencies

- OpenSpec #00022 (Theme System) - for theme colors
- OpenSpec #00026 (ArtProvider) - for toolbar icons

## Estimated Effort

- Phase 1: 2-3 hours (core infrastructure)
- Phase 2: 2-3 hours (UI enhancement)
- Phase 3: 1-2 hours (theme integration)
- Phase 4: 1 hour (mode-based visibility)
- Phase 5: 1-2 hours (settings UI, optional)

**Total:** 7-11 hours

## Reference Implementation

wxWidgets version archived at:
- `git show wxwidgets-archive:include/kalahari/gui/panels/log_panel.h`
- `git show wxwidgets-archive:src/gui/panels/log_panel.cpp`
