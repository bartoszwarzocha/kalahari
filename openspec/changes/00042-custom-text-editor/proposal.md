# 00042: Custom Text Editor

## Status
PENDING

## Summary

Custom Text Editor is the HEART of Kalahari Writer's IDE. This is a major undertaking - a rich text editor optimized for long-form creative writing (novels, short stories, books).

**ROADMAP Reference:** Section 1.5 Custom Text Editor

**Current State:** QPlainTextEdit (plain text only) - used as interim solution per ADR-005

**Target:** Full-featured BookEditor class with custom rendering, designed specifically for writers

**Functional Specification:** `project_docs/19_text_editor_functional_spec_pl.md` (v5.0)

**Philosophy:**
> Pisarz, szklanka whisky, zanurzenie w procesie twórczym.
> Zero dystraktorów. Zero wersji mobilnych. Tylko słowa.

## Goal

Create a custom text editor that provides:
1. Rich text editing with KML (Kalahari Markup Language) format
2. Excellent performance with long documents (100k+ words)
3. Writer-focused features (focus mode, typewriter mode, distraction-free writing)
4. Three view modes: Continuous, Page, Typewriter
5. Integration with Kalahari's project system and SQLite database

## Scope

### Included

**Core Editor:**
- Custom QWidget-based editor (BookEditor) with own rendering
- KML format for content storage (XML subset)
- Four view modes: Continuous, Page, Typewriter, Distraction-Free
- Focus Mode with paragraph/sentence dimming
- Virtual scrolling for large documents
- Split View (horizontal/vertical) for working with two fragments
- Tables in KML (`<table>`, `<tr>`, `<td>`, `<th>`)

**Text Editing:**
- Full Unicode support with IME
- Rich text formatting (bold, italic, underline, strikethrough)
- Paragraph styles (Normal, Heading 1-3, Dialog, Quote)
- Character styles (Emphasis, Strong, custom)
- Multi-paragraph selection
- Clipboard: KML, HTML, plain text formats

**Writer Features:**
- Real-time spell checking (Hunspell)
- Grammar checking (LanguageTool, Phase 2)
- Word frequency analysis (overused words detection)
- Text-to-Speech (QTextToSpeech)
- Word count and statistics
- Inline comments (collapsible sub-paragraphs)
- TODO/FIX/CHECK tags
- Quick Insert (@character, #location references)
- Snapshots (restore points)

**Undo/Redo:**
- QUndoStack integration
- Command merging for typing
- Full operation history

### Excluded

- AI writing assistance (future plugin)
- Real-time collaboration (future)
- Advanced export formats (Export Suite plugin)
- Encryption (handled at project level)
- **Mobile version** (by design - desktop focus for immersive writing)
- Cloud sync (desktop-first philosophy)

## Acceptance Criteria

### Phase 1 (Core)
- [ ] BookEditor renders KML content correctly
- [ ] All four view modes work (Continuous, Page, Typewriter, Distraction-Free)
- [ ] Focus Mode dims inactive paragraphs
- [ ] Split View works (horizontal and vertical)
- [ ] Tables render correctly in KML
- [ ] Performance: 60 FPS scrolling with 100k word document
- [ ] Spell checking works in background without UI lag
- [ ] Undo/Redo works for all operations
- [ ] IME input works correctly (tested with CJK input)
- [ ] Clipboard copy/paste preserves formatting
- [ ] Integration with ProjectDatabase (statistics, styles)
- [ ] Unit tests pass (>80% coverage)
- [ ] Manual testing complete

### Phase 2 (Advanced)
- [ ] Grammar checking works (LanguageTool integration)
- [ ] Word frequency analysis panel works
- [ ] Text-to-Speech reads selected text with highlighting

## Technical Design

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    BookEditor (QWidget)                      │
│                    ═══════════════════                       │
│  Main widget - handles input, scrolling, rendering          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              KmlDocument (Model)                       │  │
│  │  - Tree of paragraphs and inline elements             │  │
│  │  - CRUD operations on content                         │  │
│  │  - Signals: contentChanged, paragraphModified         │  │
│  └───────────────────────────────────────────────────────┘  │
│                          │                                   │
│                          ▼                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           LayoutManager (Layout Engine)                │  │
│  │  - ParagraphLayout for each paragraph (QTextLayout)   │  │
│  │  - Lazy layout calculation (viewport + buffer)        │  │
│  │  - Height estimation for non-laid-out paragraphs      │  │
│  └───────────────────────────────────────────────────────┘  │
│                          │                                   │
│                          ▼                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            Renderer (View)                             │  │
│  │  - QPainter-based rendering                           │  │
│  │  - View modes: Continuous, Page, Typewriter           │  │
│  │  - Focus Mode dimming                                 │  │
│  │  - Comment rendering (collapsed/expanded)             │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │ CursorManager │  │ SelectionMgr  │  │ UndoManager     │  │
│  │ - Position    │  │ - Range       │  │ - QUndoStack    │  │
│  │ - Navigation  │  │ - Rendering   │  │ - KmlCommands   │  │
│  └───────────────┘  └───────────────┘  └─────────────────┘  │
│                                                              │
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │ SpellChecker  │  │ StyleResolver │  │ StatsCollector  │  │
│  │ - Hunspell    │  │ - Cache       │  │ - Batched saves │  │
│  │ - Background  │  │ - Inheritance │  │ - Signals       │  │
│  └───────────────┘  └───────────────┘  └─────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Core Classes

#### KmlDocument (Model)

```cpp
namespace kalahari::editor {

class KmlDocument : public QObject {
    Q_OBJECT
public:
    // Loading/Saving
    bool loadFromKml(const QString& kml);
    QString toKml() const;

    // Paragraph access
    int paragraphCount() const;
    KmlParagraph* paragraph(int index);
    const KmlParagraph* paragraph(int index) const;

    // Content modification
    void insertText(const CursorPosition& pos, const QString& text);
    void deleteText(const CursorPosition& start, const CursorPosition& end);
    void applyStyle(const SelectionRange& range, const QString& styleId);
    void insertElement(const CursorPosition& pos, KmlElement* element);

    // Plain text extraction (for search, spellcheck)
    QString plainText() const;
    QString plainTextForParagraph(int index) const;

signals:
    void contentChanged();
    void paragraphModified(int index);
    void paragraphInserted(int index);
    void paragraphRemoved(int index);

private:
    QVector<std::unique_ptr<KmlParagraph>> m_paragraphs;
    QString m_language;  // document language
};

} // namespace kalahari::editor
```

#### CursorPosition & SelectionRange

```cpp
namespace kalahari::editor {

struct CursorPosition {
    int paragraphIndex = 0;
    int elementIndex = 0;   // 0 = main text, >0 = inline element
    int offset = 0;         // character offset within element

    bool operator==(const CursorPosition& other) const;
    bool operator<(const CursorPosition& other) const;
};

struct SelectionRange {
    CursorPosition start;
    CursorPosition end;

    bool isEmpty() const { return start == end; }
    bool isMultiParagraph() const { return start.paragraphIndex != end.paragraphIndex; }

    // Iterate through selected paragraphs
    template<typename Func>
    void forEachParagraph(Func&& fn) const {
        for (int p = start.paragraphIndex; p <= end.paragraphIndex; p++) {
            int startOff = (p == start.paragraphIndex) ? start.offset : 0;
            int endOff = (p == end.paragraphIndex) ? end.offset : -1;
            fn(p, startOff, endOff);
        }
    }
};

} // namespace kalahari::editor
```

#### ParagraphLayout (QTextLayout wrapper)

```cpp
namespace kalahari::editor {

class ParagraphLayout {
public:
    void setText(const QString& text);
    void setFont(const QFont& font);
    void setFormats(const QVector<QTextLayout::FormatRange>& formats);

    // Layout calculation
    int doLayout(int width);
    bool isDirty() const { return m_dirty; }
    void invalidate() { m_dirty = true; }

    // Geometry
    int height() const { return m_height; }
    int lineCount() const;
    QRectF lineRect(int lineIndex) const;

    // Hit testing
    int positionAt(const QPointF& point) const;
    QRectF cursorRect(int position) const;

    // Drawing
    void draw(QPainter* painter, const QPointF& position);

private:
    QTextLayout m_layout;
    bool m_dirty = true;
    int m_height = 0;
};

} // namespace kalahari::editor
```

#### VirtualScrollManager

```cpp
namespace kalahari::editor {

class VirtualScrollManager {
public:
    static constexpr int BUFFER_PARAGRAPHS = 10;
    static constexpr int ESTIMATED_LINE_HEIGHT = 20;

    void setDocument(KmlDocument* doc);
    void setViewport(int top, int height);

    // Visible range (includes buffer)
    QPair<int, int> visibleRange() const;

    // Height management
    void updateParagraphHeight(int index, int height);
    int totalHeight() const;
    int paragraphY(int index) const;

    // Scroll position
    int scrollOffset() const { return m_scrollOffset; }
    void setScrollOffset(int offset);

    // Find paragraph at Y coordinate
    int paragraphAtY(int y) const;

private:
    struct ParagraphInfo {
        int y = 0;              // calculated Y position
        int height = -1;        // -1 = not calculated, use estimate
        bool heightKnown = false;
    };

    KmlDocument* m_document = nullptr;
    QVector<ParagraphInfo> m_paragraphs;
    int m_viewportTop = 0;
    int m_viewportHeight = 0;
    int m_scrollOffset = 0;
};

} // namespace kalahari::editor
```

#### Undo/Redo Commands

```cpp
namespace kalahari::editor {

// Base command
class KmlCommand : public QUndoCommand {
protected:
    KmlDocument* m_document;
    CursorPosition m_cursorBefore;
    CursorPosition m_cursorAfter;
};

// Text insertion (with merge support for typing)
class InsertTextCommand : public KmlCommand {
public:
    InsertTextCommand(KmlDocument* doc, const CursorPosition& pos, const QString& text);

    void redo() override;
    void undo() override;

    // Merge consecutive typing
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return CommandId::InsertText; }

private:
    CursorPosition m_position;
    QString m_text;
};

// Text deletion
class DeleteTextCommand : public KmlCommand {
public:
    DeleteTextCommand(KmlDocument* doc, const SelectionRange& range);

    void redo() override;
    void undo() override;

private:
    SelectionRange m_range;
    QString m_deletedKml;  // for undo
};

// Style application
class ApplyStyleCommand : public KmlCommand {
public:
    ApplyStyleCommand(KmlDocument* doc, const SelectionRange& range, const QString& styleId);

    void redo() override;
    void undo() override;

private:
    SelectionRange m_range;
    QString m_newStyleId;
    QVector<QString> m_oldStyleIds;  // per-paragraph old styles
};

} // namespace kalahari::editor
```

#### SpellCheckService

```cpp
namespace kalahari::editor {

class SpellCheckService : public QObject {
    Q_OBJECT
public:
    explicit SpellCheckService(QObject* parent = nullptr);

    void setDocument(KmlDocument* doc);
    void setLanguage(const QString& lang);

    // Mark paragraph as needing check
    void markDirty(int paragraphIndex);

    // Get misspelled words for paragraph
    QVector<SpellError> errorsForParagraph(int index) const;

    // Dictionary operations
    void addToUserDictionary(const QString& word);
    void ignoreWord(const QString& word);
    QStringList suggestions(const QString& word) const;

signals:
    void paragraphChecked(int index);

private slots:
    void onDebounceTimeout();
    void onCheckComplete(int paragraphIndex, QVector<SpellError> errors);

private:
    void checkParagraphsAsync();

    KmlDocument* m_document = nullptr;
    Hunspell* m_hunspell = nullptr;
    QTimer m_debounceTimer;  // 300ms
    QSet<int> m_dirtyParagraphs;
    QHash<QString, bool> m_wordCache;
    QMap<int, QVector<SpellError>> m_errors;

    static constexpr int DEBOUNCE_MS = 300;
};

struct SpellError {
    int start;      // offset in paragraph
    int length;
    QString word;
};

} // namespace kalahari::editor
```

#### Clipboard Handler

```cpp
namespace kalahari::editor {

class ClipboardHandler {
public:
    // Copy selection to clipboard (multiple formats)
    static void copy(KmlDocument* doc, const SelectionRange& selection);

    // Paste from clipboard
    static QString paste();  // returns KML or converted content

    // Format detection
    static bool hasKml();
    static bool hasHtml();
    static bool hasText();

private:
    static constexpr const char* KML_MIME_TYPE = "application/x-kalahari-kml";

    static QString kmlToHtml(const QString& kml);
    static QString htmlToKml(const QString& html);
    static QString textToKml(const QString& text);
};

} // namespace kalahari::editor
```

#### IME Support

```cpp
// In BookEditor class:
void BookEditor::inputMethodEvent(QInputMethodEvent* event) {
    // Handle composition (preedit)
    if (!event->preeditString().isEmpty()) {
        m_compositionText = event->preeditString();
        m_compositionStart = m_cursor.position();
        m_compositionFormats = event->attributes();
        update();
    } else {
        m_compositionText.clear();
    }

    // Handle commit
    if (!event->commitString().isEmpty()) {
        m_compositionText.clear();
        insertText(event->commitString());
    }

    event->accept();
}

QVariant BookEditor::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query) {
        case Qt::ImEnabled:
            return true;
        case Qt::ImCursorRectangle:
            return cursorRect();
        case Qt::ImFont:
            return currentFont();
        case Qt::ImCursorPosition:
            return m_cursor.offset();
        case Qt::ImSurroundingText:
            return currentParagraphText();
        case Qt::ImCurrentSelection:
            return selectedText();
        default:
            return QVariant();
    }
}
```

#### StyleResolver

```cpp
namespace kalahari::editor {

class StyleResolver {
public:
    void setDatabase(ProjectDatabase* db);

    // Resolve style with inheritance
    ResolvedStyle resolve(const QString& styleId);

    // Invalidate cache (on style edit)
    void invalidateCache();
    void invalidateStyle(const QString& styleId);

private:
    ProjectDatabase* m_db = nullptr;
    QHash<QString, ResolvedStyle> m_cache;

    ResolvedStyle resolveRecursive(const QString& styleId, QSet<QString>& visited);
};

struct ResolvedStyle {
    QString fontFamily;
    int fontSize = 12;
    QFont::Weight fontWeight = QFont::Normal;
    bool italic = false;
    bool underline = false;
    Qt::Alignment alignment = Qt::AlignLeft;
    int marginTop = 0;
    int marginBottom = 0;
    int textIndent = 0;
    // ... etc

    void merge(const QJsonObject& properties);
    QFont toFont() const;
    QTextCharFormat toCharFormat() const;
    QTextBlockFormat toBlockFormat() const;
};

} // namespace kalahari::editor
```

#### StatisticsCollector

```cpp
namespace kalahari::editor {

class StatisticsCollector : public QObject {
    Q_OBJECT
public:
    explicit StatisticsCollector(QObject* parent = nullptr);

    void setDocument(KmlDocument* doc);
    void setDatabase(ProjectDatabase* db);

    // Current stats
    int wordCount() const;
    int characterCount() const;
    int paragraphCount() const;

public slots:
    void onTextChanged(int wordsAdded, int wordsDeleted);
    void flush();  // save to database

signals:
    void statsUpdated(int words, int chars, int paragraphs);

private:
    void startSession();
    void endSession();

    KmlDocument* m_document = nullptr;
    ProjectDatabase* m_db = nullptr;
    QTimer m_flushTimer;  // 5 minutes

    // Current hour stats
    SessionStats m_currentHour;
    QDateTime m_sessionStart;

    static constexpr int FLUSH_INTERVAL_MS = 5 * 60 * 1000;
};

} // namespace kalahari::editor
```

### New Files

**Headers:**
- `include/kalahari/editor/book_editor.h`
- `include/kalahari/editor/kml_document.h`
- `include/kalahari/editor/kml_paragraph.h`
- `include/kalahari/editor/kml_element.h`
- `include/kalahari/editor/cursor_position.h`
- `include/kalahari/editor/selection_range.h`
- `include/kalahari/editor/paragraph_layout.h`
- `include/kalahari/editor/virtual_scroll_manager.h`
- `include/kalahari/editor/kml_commands.h`
- `include/kalahari/editor/spell_check_service.h`
- `include/kalahari/editor/clipboard_handler.h`
- `include/kalahari/editor/style_resolver.h`
- `include/kalahari/editor/statistics_collector.h`
- `include/kalahari/editor/view_modes.h`

**Sources:**
- `src/editor/book_editor.cpp`
- `src/editor/kml_document.cpp`
- `src/editor/kml_paragraph.cpp`
- `src/editor/kml_element.cpp`
- `src/editor/paragraph_layout.cpp`
- `src/editor/virtual_scroll_manager.cpp`
- `src/editor/kml_commands.cpp`
- `src/editor/spell_check_service.cpp`
- `src/editor/clipboard_handler.cpp`
- `src/editor/style_resolver.cpp`
- `src/editor/statistics_collector.cpp`

**Tests:**
- `tests/editor/kml_document_test.cpp`
- `tests/editor/book_editor_test.cpp`
- `tests/editor/spell_check_test.cpp`
- `tests/editor/undo_redo_test.cpp`

### Files to Modify

- `src/gui/editor_panel.cpp` - integrate BookEditor
- `include/kalahari/gui/editor_panel.h` - add BookEditor member
- `CMakeLists.txt` - add editor module

## Dependencies

- **Depends on:**
  - Project File System (1.2) - COMPLETE
  - Navigator Panel (1.4) - COMPLETE
  - **OpenSpec #00041: SQLite Project Database** - PENDING (required for metadata/statistics/styles)

- **Required by:**
  - Statistics Module (1.7) - word count source
  - Search & Replace (1.6) - text search in editor
  - Spell Checking (Phase 2) - integrated in this OpenSpec

## Implementation Phases

### Phase 1: Core Editor
- KmlDocument and KmlParagraph
- BookEditor with basic rendering (Continuous mode only)
- Cursor and selection
- Basic text input
- Table element support in KML

### Phase 2: Layout & Scrolling
- ParagraphLayout with QTextLayout
- VirtualScrollManager
- Smooth scrolling
- Table layout

### Phase 3: Editing Operations
- Undo/Redo with QUndoStack
- Clipboard (copy/paste/cut)
- IME support

### Phase 4: View Modes
- Page mode
- Typewriter mode
- Focus Mode dimming
- **Distraction-Free Mode** (fullscreen, no UI)
- **Split View** (horizontal/vertical)

### Phase 5: Advanced Features (Part 1)
- Spell checking (Hunspell)
- Style resolver with inheritance
- Statistics collector
- Comments rendering

### Phase 6: Advanced Features (Part 2)
- **Grammar checking** (LanguageTool integration)
- **Word frequency analysis** (panel + highlighting)
- **Text-to-Speech** (QTextToSpeech)

### Phase 7: Integration & Polish
- ProjectDatabase integration
- EditorPanel integration
- Testing and polish
- Performance optimization

## Notes

- This is a MAJOR feature - expect multi-phase implementation
- Design phase completed - see `project_docs/19_text_editor_functional_spec_pl.md`
- Hybrid approach: own rendering + Qt components (QTextLayout, QUndoStack, etc.)
- Lazy layout: full KML in memory, layout only for visible paragraphs

## References

- ROADMAP.md Section 1.5
- **project_docs/19_text_editor_functional_spec_pl.md** - Functional specification v4.1
- project_docs/15_text_editor_architecture.md (existing ADR-005)
- Qt Documentation: QTextLayout, QUndoStack, QInputMethodEvent
- Hunspell Documentation: https://hunspell.github.io/
