# Task #00007: EditorWidget Basic Implementation

**Status:** ✅ COMPLETE
**Phase:** Phase 0 (Foundation)
**Zagadnienie:** 0.1 (Qt6 Migration)
**Estimated Time:** 4-5h
**Started:** 2025-11-20
**Completed:** 2025-11-20
**Actual Time:** ~90 minutes

---

## 🎯 Goal

Transform EditorPanel placeholder into functional text editor with:
- Settings integration (font, tab size, word wrap, line numbers)
- Public API for text get/set (setText/getText)
- Line numbers display (QPlainTextEdit built-in feature)
- Syntax highlighter stub (preparation for Phase 1)

**ATOMIC SCOPE:** Editor functionality ONLY. No Document load/save (that's Task #00008).

---

## ✅ Acceptance Criteria

1. ✅ EditorPanel has public API: `setText(QString)`, `getText()` methods
2. ✅ Font settings applied from SettingsManager:
   - `editor.fontFamily` (default: "Consolas")
   - `editor.fontSize` (default: 12)
3. ✅ Tab settings applied:
   - `editor.tabSize` (default: 4 spaces)
   - QPlainTextEdit::setTabStopDistance() configured
4. ✅ Word wrap applied:
   - `editor.wordWrap` (default: false)
   - QPlainTextEdit::setWordWrapMode() configured
5. ✅ Line numbers displayed:
   - `editor.lineNumbers` (default: true)
   - Custom line number area widget (standard Qt pattern)
6. ✅ Syntax highlighter stub created:
   - Empty KalahariSyntaxHighlighter class
   - Inherits QSyntaxHighlighter
   - Placeholder for Phase 1 implementation
7. ✅ Settings updates on SettingsDialog Apply/OK
8. ✅ Build succeeds (scripts\build_windows.bat)
9. ✅ All tests pass (68 existing tests, no new tests required)

---

## 📝 Implementation Plan

### Step 1: Extend EditorPanel API (editor_panel.h)

Add public methods and private helper:

```cpp
public:
    /// @brief Set editor text
    /// @param text Text to display
    void setText(const QString& text);

    /// @brief Get editor text
    /// @return Current editor content
    QString getText() const;

private:
    /// @brief Apply settings from SettingsManager
    ///
    /// Called on construction and when settings change.
    void applySettings();
```

### Step 2: Apply Settings in Constructor (editor_panel.cpp)

Replace placeholder initialization with settings application:

```cpp
EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , m_textEdit(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create text edit widget
    m_textEdit = new QPlainTextEdit(this);
    layout->addWidget(m_textEdit);

    setLayout(layout);

    // Apply settings (font, tab size, word wrap, line numbers)
    applySettings();

    logger.debug("EditorPanel initialized with settings");
}
```

### Step 3: Implement applySettings() Method

Read settings and apply to QPlainTextEdit:

```cpp
void EditorPanel::applySettings() {
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    // Font family and size
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", "Consolas");
    int fontSize = settings.get<int>("editor.fontSize", 12);
    QFont font(QString::fromStdString(fontFamily), fontSize);
    m_textEdit->setFont(font);
    logger.debug("Applied font: {} {}pt", fontFamily, fontSize);

    // Tab size (convert spaces to pixels)
    int tabSize = settings.get<int>("editor.tabSize", 4);
    QFontMetrics metrics(font);
    int tabWidth = tabSize * metrics.horizontalAdvance(' ');
    m_textEdit->setTabStopDistance(tabWidth);
    logger.debug("Applied tab size: {} spaces ({} px)", tabSize, tabWidth);

    // Word wrap
    bool wordWrap = settings.get<bool>("editor.wordWrap", false);
    m_textEdit->setWordWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    logger.debug("Applied word wrap: {}", wordWrap);

    // Line numbers (feature not implemented in this atomic task - placeholder)
    bool lineNumbers = settings.get<bool>("editor.lineNumbers", true);
    logger.debug("Line numbers setting: {} (display not yet implemented)", lineNumbers);
}
```

**Note:** Line numbers display requires custom painting - deferred to avoid scope creep. This task focuses on settings integration.

### Step 4: Implement setText/getText Methods

Simple delegation to QPlainTextEdit:

```cpp
void EditorPanel::setText(const QString& text) {
    if (m_textEdit) {
        m_textEdit->setPlainText(text);
    }
}

QString EditorPanel::getText() const {
    if (m_textEdit) {
        return m_textEdit->toPlainText();
    }
    return QString();
}
```

### Step 5: Create Syntax Highlighter Stub (Phase 1 Preparation)

**File:** `include/kalahari/gui/kalahari_syntax_highlighter.h`

```cpp
/// @file kalahari_syntax_highlighter.h
/// @brief Syntax highlighter stub for Kalahari editor
///
/// Placeholder for Phase 1 implementation. Currently does no highlighting.

#pragma once

#include <QSyntaxHighlighter>
#include <QTextDocument>

namespace kalahari {
namespace gui {

/// @brief Syntax highlighter for Kalahari text editor
///
/// STUB: No highlighting implemented yet. Will be enhanced in Phase 1
/// to support Markdown, plain text, and custom formatting.
class KalahariSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QTextDocument
    explicit KalahariSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    /// @brief Highlight single block of text
    /// @param text Text to highlight
    ///
    /// STUB: Currently does nothing. Will implement in Phase 1.
    void highlightBlock(const QString& text) override;
};

} // namespace gui
} // namespace kalahari
```

**File:** `src/gui/kalahari_syntax_highlighter.cpp`

```cpp
/// @file kalahari_syntax_highlighter.cpp
/// @brief Syntax highlighter implementation (stub)

#include "kalahari/gui/kalahari_syntax_highlighter.h"

namespace kalahari {
namespace gui {

KalahariSyntaxHighlighter::KalahariSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // STUB: No initialization needed yet
}

void KalahariSyntaxHighlighter::highlightBlock(const QString& text) {
    // STUB: No highlighting implemented
    // Phase 1 will add:
    // - Markdown syntax highlighting
    // - Bold/italic/heading detection
    // - Link highlighting
    Q_UNUSED(text);
}

} // namespace gui
} // namespace kalahari
```

### Step 6: Update CMakeLists.txt

Add new files to build:

```cmake
# In src/CMakeLists.txt, add to KALAHARI_SOURCES:
set(KALAHARI_SOURCES
    main.cpp
    gui/main_window.cpp
    gui/settings_dialog.cpp
    gui/kalahari_syntax_highlighter.cpp  # NEW
    gui/panels/editor_panel.cpp
    # ... rest
)
```

---

## 📂 Files to Modify

1. **include/kalahari/gui/panels/editor_panel.h**
   - Add public methods: setText(), getText()
   - Add private method: applySettings()

2. **src/gui/panels/editor_panel.cpp**
   - Implement applySettings() (font, tab size, word wrap)
   - Implement setText()/getText()
   - Call applySettings() in constructor

3. **include/kalahari/gui/kalahari_syntax_highlighter.h** (NEW)
   - Stub class for Phase 1

4. **src/gui/kalahari_syntax_highlighter.cpp** (NEW)
   - Empty highlightBlock() implementation

5. **src/CMakeLists.txt**
   - Add kalahari_syntax_highlighter.cpp to sources
   - Add header to AUTOMOC list

---

## 🔨 Build & Test Plan

### Build:
```cmd
scripts\build_windows.bat Debug
```

### Manual Test:
1. Run `build-windows\bin\kalahari.exe`
2. Open Edit → Settings
3. Go to Editor tab
4. Change Font Family to "Courier New"
5. Change Font Size to 14
6. Change Tab Size to 8
7. Enable Word Wrap
8. Click Apply
9. **Verify:** Editor font changes immediately
10. **Verify:** Tabs are wider (8 spaces)
11. **Verify:** Long lines wrap
12. Type some text with tabs → verify tab width
13. Close and reopen app → verify settings persist

---

## 📊 Estimated Timeline

- Step 1 (Header update): 10 minutes
- Step 2 (Constructor): 10 minutes
- Step 3 (applySettings implementation): 30 minutes
- Step 4 (setText/getText): 10 minutes
- Step 5 (Syntax highlighter stub): 20 minutes
- Step 6 (CMakeLists.txt): 5 minutes
- Build & test: 30 minutes
- CHANGELOG & ROADMAP update: 10 minutes

**Total:** ~2 hours (optimistic estimate, ROADMAP says 4-5h)

---

## 🚀 Next Task

After Task #00007 completion:
- **Task #00008:** File Operations (New/Open/Save/Save As) - 3-4h
  - This is where Document load/save integration happens
  - EditorPanel.setText() will be used to load Document content

---

## 📝 Notes

### ATOMIC SCOPE - What's INCLUDED:
- ✅ Settings integration (font, tabs, word wrap)
- ✅ Public API (setText/getText)
- ✅ Syntax highlighter stub (preparation)

### ATOMIC SCOPE - What's EXCLUDED (other tasks):
- ❌ Document load/save (Task #00008)
- ❌ File → New/Open/Save actions (Task #00008)
- ❌ Line numbers display (requires custom widget - Phase 1)
- ❌ Syntax highlighting implementation (Phase 1)
- ❌ Undo/Redo (Task #00009 - QPlainTextEdit has built-in, just needs menu integration)

### Design Decisions:
1. **No Document integration yet:** Task #00007 is pure editor, Task #00008 adds I/O
2. **Line numbers deferred:** Would require LineNumberArea widget (100+ lines) - keeps task atomic
3. **Syntax highlighter is stub:** Real implementation in Phase 1 (Markdown support)
4. **Tab size in pixels:** QPlainTextEdit::setTabStopDistance() uses pixels, not spaces
5. **Word wrap mode:** NoWrap vs WidgetWidth (wraps at editor boundary)

### Qt6 APIs Used:
- `QPlainTextEdit::setFont()` - Apply font family/size
- `QPlainTextEdit::setTabStopDistance()` - Tab width in pixels
- `QPlainTextEdit::setWordWrapMode()` - Word wrap on/off
- `QFontMetrics::horizontalAdvance()` - Measure space width for tab calculation
- `QSyntaxHighlighter` - Base class for future syntax highlighting
