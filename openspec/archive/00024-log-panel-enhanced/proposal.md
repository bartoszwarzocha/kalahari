# Change: Enhanced Log Panel (Theme-Aware, Mode-Restricted)

## Why

### Problem 1: Placeholder Implementation

**Current State:**
```cpp
// log_panel.cpp - just a placeholder
m_logEdit = new QPlainTextEdit(this);
m_logEdit->appendPlainText(tr("[INFO] Log panel initialized"));
m_logEdit->appendPlainText(tr("[INFO] Full log integration comes in Phase 1"));
```

**Problems:**
- No real-time log integration with spdlog
- No colored output per log level
- No ring buffer (memory unbounded)
- No toolbar (Options, Copy, Clear, Open Folder)

### Problem 2: No Theme Integration

Log panel uses hardcoded colors:
- No integration with ThemeManager
- No per-theme color schemes (light/dark)
- No settings for background/text colors

### Problem 3: No Mode-Based Visibility

**Current:** Panel visibility is manual (user toggles View menu)

**Required:**
- Normal mode: Panel HIDDEN by default, shows only INFO+ logs
- `--diag` mode: Panel SHOWN by default, shows ALL logs (trace, debug, info, warn, error)
- `--dev` mode: Same as --diag

## What Changes

### 1. Real-Time Log Integration (spdlog Custom Sink)

```
┌─────────────────────────────────────────────────────────────────┐
│                     spdlog Architecture                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Logger::getInstance()                                           │
│       │                                                          │
│       ├──► File Sink (kalahari.log)                             │
│       │                                                          │
│       └──► LogPanelSink (NEW - Qt signal-based)                 │
│                 │                                                │
│                 └──► emit logMessage(level, message)            │
│                           │                                      │
│                           ▼                                      │
│                      LogPanel::appendLog(level, message)        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2. Colored Output Per Log Level

| Level | Light Theme | Dark Theme |
|-------|-------------|------------|
| TRACE | Gray (#808080) | Gray (#808080) |
| DEBUG | Blue (#0066CC) | Cyan (#66CCFF) |
| INFO | Black (#000000) | White (#FFFFFF) |
| WARN | Orange (#CC6600) | Yellow (#FFCC00) |
| ERROR | Red (#CC0000) | Red (#FF4444) |
| CRITICAL | White on Red BG | White on Red BG |

### 3. Ring Buffer (Memory Management)

```cpp
struct LogEntry {
    spdlog::level::level_enum level;
    QString message;
    QDateTime timestamp;
};

std::deque<LogEntry> m_logBuffer;  // Ring buffer
size_t m_maxBufferSize = 500;      // Configurable 1-1000
```

### 4. Mode-Based Behavior

```
┌─────────────────────────────────────────────────────────────────┐
│                     Mode-Based Behavior                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Normal Mode (no flags):                                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ • LogPanel HIDDEN by default                            │    │
│  │ • Shows only: INFO, WARN, ERROR, CRITICAL               │    │
│  │ • User can show via View menu                           │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Diagnostic Mode (--diag or --dev):                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ • LogPanel SHOWN by default                             │    │
│  │ • Shows ALL: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL  │    │
│  │ • Full debugging capability                             │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5. Toolbar (Vertical, Right Side)

```
┌────────────────────────────────────────────────────────────┐
│                                                      │ ⚙ │ │
│  [INFO] Application started                          │ 📁│ │
│  [DEBUG] Loading settings...                         │ 📋│ │
│  [WARN] Config file not found                        │ 🗑 │ │
│  [ERROR] Connection failed                           └───┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Toolbar buttons:
⚙  Options      - Open Settings Dialog (Advanced/Log page)
📁 Open Folder  - Open log directory in file explorer
📋 Copy         - Copy log buffer to clipboard
🗑  Clear        - Clear log display
```

### 6. Settings Integration

**New settings in SettingsManager:**
```json
{
  "log": {
    "bufferSize": 500,
    "fontSize": 11,
    "showTimestamp": true,
    "lightTheme": {
      "background": "#FFFFFF",
      "trace": "#808080",
      "debug": "#0066CC",
      "info": "#000000",
      "warn": "#CC6600",
      "error": "#CC0000"
    },
    "darkTheme": {
      "background": "#1E1E1E",
      "trace": "#808080",
      "debug": "#66CCFF",
      "info": "#FFFFFF",
      "warn": "#FFCC00",
      "error": "#FF4444"
    }
  }
}
```

## Implementation Plan

### Phase 1: Core Infrastructure
- [ ] Create LogPanelSink (spdlog custom sink with Qt signals)
- [ ] Implement ring buffer with LogEntry struct
- [ ] Connect sink to Logger in main.cpp

### Phase 2: Enhanced UI
- [ ] Replace QPlainTextEdit with QTextEdit (rich text for colors)
- [ ] Implement appendLog(level, message) with color formatting
- [ ] Add vertical toolbar (Options, Open Folder, Copy, Clear)

### Phase 3: Theme Integration
- [ ] Define per-theme color schemes in ThemeManager or SettingsManager
- [ ] Connect to ThemeManager::themeChanged signal
- [ ] Implement rebuildDisplay() for theme switches

### Phase 4: Mode-Based Visibility
- [ ] Read --diag/--dev flags in main.cpp
- [ ] Pass mode to MainWindow constructor
- [ ] Set panel visibility based on mode
- [ ] Implement log level filtering based on mode

### Phase 5: Settings UI
- [ ] Add "Log" page to Settings Dialog (Advanced section)
- [ ] Buffer size spinner (1-1000)
- [ ] Font size selector
- [ ] Timestamp toggle
- [ ] Color pickers for each level (optional, Phase 2+)

## Files to Create/Modify

### Create
| File | Purpose |
|------|---------|
| `include/kalahari/core/log_panel_sink.h` | spdlog custom sink header |
| `src/core/log_panel_sink.cpp` | spdlog sink implementation |

### Modify
| File | Changes |
|------|---------|
| `include/kalahari/gui/panels/log_panel.h` | Full interface (ring buffer, toolbar, colors) |
| `src/gui/panels/log_panel.cpp` | Full implementation |
| `src/main.cpp` | Add LogPanelSink to logger, pass mode flags |
| `src/gui/main_window.cpp` | Mode-based panel visibility |
| `src/gui/settings_dialog.cpp` | Add Log settings page |

## Success Criteria

- [ ] Real-time logs appear in panel as they're generated
- [ ] Each log level has distinct color (theme-aware)
- [ ] Ring buffer prevents memory growth (default 500 lines)
- [ ] Normal mode: panel hidden, INFO+ only
- [ ] Diag/Dev mode: panel shown, all levels
- [ ] Toolbar works (Options, Open Folder, Copy, Clear)
- [ ] Theme switch updates log colors immediately

## Reference

**wxWidgets Implementation (archived):**
- `wxwidgets-archive:include/kalahari/gui/panels/log_panel.h`
- `wxwidgets-archive:src/gui/panels/log_panel.cpp`

Key patterns to port:
- Ring buffer with `std::deque<wxString>`
- `appendLog()` with overflow handling
- `rebuildDisplay()` for settings changes
- Platform-specific Open Folder (explorer/open/xdg-open)
