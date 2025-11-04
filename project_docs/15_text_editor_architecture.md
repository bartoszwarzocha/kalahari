# bwxTextEditor - Complete Architecture Specification

**Purpose:** Comprehensive technical architecture for custom text editor control (Task #00019 + follow-ups)

**Status:** 📝 Design Phase
**Version:** 1.0
**Last Updated:** 2025-11-04
**Related Tasks:** #00019 (MVP), #00020-#00026 (Extended features)

---

## Executive Summary

### Vision: Writer's IDE Text Editor

A custom wxWidgets text editor control designed specifically for book authors, featuring:
- **4 View Modes:** Page View (MS Word-like), Typewriter Mode (immersive), Full View (continuous), Publisher View (manuscript)
- **Advanced Features:** Comments, Footnotes, Citations/Bibliography, Indexes
- **Professional Quality:** Native performance, extensible architecture, writer-focused UX

### Implementation Strategy

**Phased Approach (8 phases, 20 weeks total):**
1. **Phase 1 (MVP):** Full View Mode + basic editing (2-3 weeks) → Enables Kalahari Phase 1
2. **Phases 2-4:** Additional view modes (8 weeks)
3. **Phases 5-8:** Advanced features (8 weeks)

### Architectural Principles

1. **Extensibility** - New view modes and features can be added without breaking existing code
2. **Separation of Concerns** - Document model, rendering, and control logic are independent
3. **Performance** - Efficient for 10K+ word documents, smooth 60 FPS interaction
4. **Writer-Focused** - Features designed for book authors, not generic editing

---

## Architecture Overview

### Three-Layer Architecture (MVC-inspired)

```
┌─────────────────────────────────────────────────────────┐
│                    bwxTextEditor                        │  ← Controller
│                      (wxControl)                        │
│  - Event handling (keyboard, mouse)                    │
│  - View mode switching                                 │
│  - File I/O coordination                               │
└────────────────┬────────────────────────────────────────┘
                 │
        ┌────────┴────────┐
        │                 │
        ▼                 ▼
┌───────────────┐   ┌──────────────────────────────────┐
│bwxTextDocument│   │     ITextRenderer (Strategy)     │  ← View
│   (Model)     │   │  - PageViewRenderer              │
│               │   │  - TypewriterRenderer            │
│ - Text storage│   │  - FullViewRenderer              │
│ - Formatting  │   │  - PublisherRenderer             │
│ - Metadata    │   └──────────────────────────────────┘
│ - Comments    │
│ - Footnotes   │
│ - Citations   │
│ - Index       │
└───────────────┘
```

**Key Design Patterns:**
- **Strategy Pattern:** Swappable renderers for different view modes
- **Command Pattern:** Undo/Redo system
- **Observer Pattern:** Document changes notify UI
- **Composite Pattern:** Document structure (Book → Chapters → Paragraphs)

---

## Component 1: Document Model (bwxTextDocument)

### Purpose

Stores all document data (text, formatting, metadata, advanced features) independently of how it's displayed.

### Core Data Structure Decision

**Chosen:** Gap Buffer (for MVP)

**Rationale:**
- Simple to implement (crucial for 2-3 week timeline)
- Good performance for typical editing patterns (insertion at cursor)
- Cache-friendly (contiguous memory)
- Proven in many editors (Emacs, etc.)

**Future Migration Path:** Can migrate to Rope or Piece Table if profiling shows bottlenecks

**Gap Buffer Concept:**
```
Text: "Hello World"
Buffer: [H][e][l][l][o][ ][_][_][_][_][_][W][o][r][l][d]
                         ↑ Gap ↑
Cursor position: 6
Gap start: 6, Gap end: 11

Insert 'X': Fill gap from start
[H][e][l][l][o][ ][X][_][_][_][_][W][o][r][l][d]
                   ↑ Gap ↑

Delete: Extend gap
[H][e][l][l][o][_][_][_][_][_][_][W][o][r][l][d]
               ↑ Gap expanded ↑
```

### Class Interface

```cpp
namespace bwx_sdk {
namespace gui {

/// Text storage abstraction (allows swapping implementations)
class ITextStorage {
public:
    virtual ~ITextStorage() = default;

    virtual void InsertText(int pos, const wxString& text) = 0;
    virtual void DeleteText(int startPos, int length) = 0;
    virtual wxString GetText(int startPos, int length) const = 0;
    virtual wxString GetAllText() const = 0;
    virtual int GetLength() const = 0;
};

/// Gap Buffer implementation
class GapBufferStorage : public ITextStorage {
public:
    GapBufferStorage(int initialCapacity = 1024);

    void InsertText(int pos, const wxString& text) override;
    void DeleteText(int startPos, int length) override;
    wxString GetText(int startPos, int length) const override;
    wxString GetAllText() const override;
    int GetLength() const override;

private:
    std::vector<wxChar> m_buffer;
    int m_gapStart;
    int m_gapEnd;

    void MoveGap(int pos);
    void ExpandGap(int minSize);
};

/// Complete document model
class bwxTextDocument {
public:
    bwxTextDocument();
    ~bwxTextDocument();

    // ====================================================================
    // TEXT OPERATIONS
    // ====================================================================

    /// Get complete text
    wxString GetText() const;

    /// Set complete text (replaces all)
    void SetText(const wxString& text);

    /// Insert text at position
    void InsertText(int pos, const wxString& text);

    /// Delete text range
    void DeleteText(int startPos, int endPos);

    /// Get text length
    int GetLength() const;

    // ====================================================================
    // FORMATTING
    // ====================================================================

    /// Character-level text format
    struct TextFormat {
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikethrough = false;
        wxString fontName = "Arial";
        int fontSize = 12;
        wxColour textColor = *wxBLACK;
        wxColour backgroundColor = *wxWHITE;

        bool operator==(const TextFormat& other) const;
        bool operator!=(const TextFormat& other) const;
    };

    /// Apply format to range
    void ApplyFormat(int startPos, int endPos, const TextFormat& format);

    /// Get format at position
    TextFormat GetFormatAt(int pos) const;

    /// Clear all formatting in range
    void ClearFormatting(int startPos, int endPos);

    /// Get all format runs (for rendering)
    struct FormatRun {
        int startPos;
        int endPos;
        TextFormat format;
    };
    std::vector<FormatRun> GetFormatRuns(int startPos, int endPos) const;

    // ====================================================================
    // CURSOR & SELECTION
    // ====================================================================

    struct Cursor {
        int position = 0;        // Character position in document
        int line = 0;            // Line number (0-based)
        int column = 0;          // Column in line (0-based)

        bool operator==(const Cursor& other) const {
            return position == other.position;
        }
    };

    Cursor GetCursor() const { return m_cursor; }
    void SetCursor(const Cursor& cursor);
    void SetCursorPosition(int pos);

    struct Selection {
        int startPos = 0;
        int endPos = 0;
        bool active = false;

        int GetLength() const { return endPos - startPos; }
        bool IsEmpty() const { return !active || startPos == endPos; }
    };

    Selection GetSelection() const { return m_selection; }
    void SetSelection(const Selection& selection);
    void SetSelection(int startPos, int endPos);
    void ClearSelection();
    bool HasSelection() const { return m_selection.active; }

    // ====================================================================
    // UNDO/REDO (Command Pattern)
    // ====================================================================

    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    void ClearUndoHistory();
    void SetUndoLimit(int maxCommands); // Default: 100

    // ====================================================================
    // COMMENTS SYSTEM
    // ====================================================================

    struct Comment {
        wxString id;             // Unique identifier
        wxString author;
        wxDateTime timestamp;
        int startPos;
        int endPos;
        wxString content;
        bool resolved = false;
        std::vector<Comment> replies; // Threaded replies

        // Visual hint
        wxColour highlightColor = wxColour(255, 255, 200); // Yellow tint
    };

    void AddComment(const Comment& comment);
    void UpdateComment(const wxString& id, const Comment& comment);
    void DeleteComment(const wxString& id);
    Comment* GetComment(const wxString& id);
    std::vector<Comment> GetComments() const;
    std::vector<Comment> GetCommentsInRange(int startPos, int endPos) const;

    // ====================================================================
    // FOOTNOTES
    // ====================================================================

    enum FootnoteType {
        FOOTNOTE,                // Bottom of page
        ENDNOTE                  // End of chapter/document
    };

    struct Footnote {
        wxString id;             // Unique identifier
        int referencePos;        // Position in text where reference appears
        int number;              // Auto-generated number (1, 2, 3...)
        wxString content;        // Footnote text
        FootnoteType type;
    };

    void AddFootnote(const Footnote& footnote);
    void UpdateFootnote(const wxString& id, const Footnote& footnote);
    void DeleteFootnote(const wxString& id);
    Footnote* GetFootnote(const wxString& id);
    std::vector<Footnote> GetFootnotes() const;
    std::vector<Footnote> GetFootnotesOfType(FootnoteType type) const;

    /// Renumber all footnotes (call after add/delete)
    void RenumberFootnotes();

    // ====================================================================
    // CITATIONS & BIBLIOGRAPHY
    // ====================================================================

    enum CitationType {
        BOOK,
        ARTICLE,
        WEBSITE,
        CONFERENCE,
        THESIS,
        OTHER
    };

    enum CitationStyle {
        APA,                     // American Psychological Association
        MLA,                     // Modern Language Association
        CHICAGO,                 // Chicago Manual of Style
        HARVARD,                 // Harvard referencing
        IEEE,                    // Institute of Electrical and Electronics Engineers
        VANCOUVER                // Vancouver system
    };

    struct Citation {
        wxString id;             // Unique identifier
        CitationType type;

        // Common fields
        wxString author;
        int year;
        wxString title;

        // Book-specific
        wxString publisher;
        wxString edition;
        wxString isbn;

        // Article-specific
        wxString journal;
        int volume;
        int issue;
        wxString pages;
        wxString doi;            // Digital Object Identifier

        // Website-specific
        wxString url;
        wxDateTime accessDate;

        // Generate formatted citation string
        wxString Format(CitationStyle style) const;
    };

    struct InlineCitation {
        int position;            // Where in text
        wxString citationId;     // Reference to Citation
        wxString customText;     // Optional override (e.g., "Smith, 2020, p. 45")
    };

    void AddCitation(const Citation& citation);
    void UpdateCitation(const wxString& id, const Citation& citation);
    void DeleteCitation(const wxString& id);
    Citation* GetCitation(const wxString& id);
    std::vector<Citation> GetCitations() const;

    void AddInlineCitation(const InlineCitation& inlineCitation);
    void DeleteInlineCitation(int position);
    std::vector<InlineCitation> GetInlineCitations() const;

    /// Generate formatted bibliography
    std::vector<wxString> GenerateBibliography(CitationStyle style) const;

    // ====================================================================
    // INDEX
    // ====================================================================

    struct IndexEntry {
        wxString id;             // Unique identifier
        wxString term;           // Index term (e.g., "Photosynthesis")
        std::vector<int> positions; // All positions where term appears
        std::vector<IndexEntry> subEntries; // Hierarchical (e.g., "Types of...")
        wxString seeAlso;        // Cross-reference (e.g., "See also: Chlorophyll")
    };

    void AddIndexEntry(const IndexEntry& entry);
    void UpdateIndexEntry(const wxString& id, const IndexEntry& entry);
    void DeleteIndexEntry(const wxString& id);
    IndexEntry* GetIndexEntry(const wxString& id);
    std::vector<IndexEntry> GetIndexEntries() const;

    /// Generate sorted index with page numbers
    /// (requires ITextRenderer to compute page numbers)
    std::vector<IndexEntry> GenerateIndex() const;

    // ====================================================================
    // METADATA
    // ====================================================================

    struct Metadata {
        wxString title;
        wxString author;
        wxDateTime created;
        wxDateTime modified;
        int wordCount = 0;
        int characterCount = 0;
        int pageCount = 0;       // Computed by renderer
        wxString language = "en";
    };

    Metadata GetMetadata() const { return m_metadata; }
    void SetMetadata(const Metadata& metadata);
    void UpdateWordCount();      // Recalculate word/character counts

    // ====================================================================
    // FILE I/O
    // ====================================================================

    /// Load from .ktxt file (Kalahari Text format - JSON)
    bool LoadFromFile(const wxString& path);

    /// Save to .ktxt file
    bool SaveToFile(const wxString& path);

    /// Export to various formats
    bool ExportToRTF(const wxString& path);
    bool ExportToPDF(const wxString& path);
    bool ExportToHTML(const wxString& path);
    bool ExportToMarkdown(const wxString& path);

    // ====================================================================
    // EVENTS (Observer pattern)
    // ====================================================================

    /// Document change notification
    class IDocumentObserver {
    public:
        virtual ~IDocumentObserver() = default;
        virtual void OnTextChanged() = 0;
        virtual void OnFormattingChanged() = 0;
        virtual void OnCursorMoved() = 0;
        virtual void OnSelectionChanged() = 0;
    };

    void AddObserver(IDocumentObserver* observer);
    void RemoveObserver(IDocumentObserver* observer);

private:
    // Text storage (swappable)
    std::unique_ptr<ITextStorage> m_storage;

    // Formatting storage (position → format map)
    std::map<int, TextFormat> m_formatting;

    // Editor state
    Cursor m_cursor;
    Selection m_selection;

    // Undo/Redo (Command pattern)
    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
    };

    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    int m_undoLimit = 100;

    // Advanced features
    std::map<wxString, Comment> m_comments;
    std::map<wxString, Footnote> m_footnotes;
    std::map<wxString, Citation> m_citations;
    std::vector<InlineCitation> m_inlineCitations;
    std::map<wxString, IndexEntry> m_indexEntries;

    // Metadata
    Metadata m_metadata;

    // Observers
    std::vector<IDocumentObserver*> m_observers;

    // Helper methods
    void NotifyTextChanged();
    void NotifyFormattingChanged();
    void NotifyCursorMoved();
    void NotifySelectionChanged();

    void AddUndoCommand(std::unique_ptr<ICommand> command);
};

} // namespace gui
} // namespace bwx_sdk
```

---

## Component 2: Renderer Strategy (View Layer)

### Purpose

Renders document in different view modes without changing document model.

### Base Renderer Interface

```cpp
namespace bwx_sdk {
namespace gui {

/// Abstract renderer interface (Strategy pattern)
class ITextRenderer {
public:
    virtual ~ITextRenderer() = default;

    // ====================================================================
    // LIFECYCLE
    // ====================================================================

    /// Initialize renderer with document reference
    virtual void Initialize(const bwxTextDocument& document) = 0;

    /// Cleanup resources
    virtual void Cleanup() = 0;

    // ====================================================================
    // RENDERING
    // ====================================================================

    /// Main render method (called from OnPaint)
    virtual void Render(wxGraphicsContext* gc, const wxRect& clientRect) = 0;

    /// Get best size for control
    virtual wxSize GetBestSize() const = 0;

    // ====================================================================
    // LAYOUT QUERIES
    // ====================================================================

    /// Convert screen coordinates to document position
    virtual int HitTest(int x, int y) const = 0;

    /// Get cursor rectangle (for caret drawing)
    virtual wxRect GetCursorRect(int position) const = 0;

    /// Get selection rectangles (for highlight)
    virtual std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const = 0;

    /// Get page count (for pagination)
    virtual int GetPageCount() const = 0;

    /// Get current page at position
    virtual int GetPageAtPosition(int position) const = 0;

    // ====================================================================
    // INTERACTION
    // ====================================================================

    /// Handle resize
    virtual void OnResize(int width, int height) = 0;

    /// Handle scroll
    virtual void OnScroll(int deltaX, int deltaY) = 0;

    /// Handle cursor move (for view-specific behavior)
    virtual void OnCursorMove(int newPosition) = 0;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /// Set antialiasing
    virtual void SetAntialiasing(bool enable) { m_antialiasing = enable; }
    virtual bool GetAntialiasing() const { return m_antialiasing; }

    /// Set zoom level (1.0 = 100%)
    virtual void SetZoom(double zoom) { m_zoom = zoom; }
    virtual double GetZoom() const { return m_zoom; }

protected:
    const bwxTextDocument* m_document = nullptr;
    bool m_antialiasing = true;
    double m_zoom = 1.0;
    wxGraphicsRenderer* m_graphicsRenderer = nullptr;

    /// Helper: Create graphics context with settings
    wxGraphicsContext* CreateGraphicsContext(wxDC& dc);
};

} // namespace gui
} // namespace bwx_sdk
```

### Renderer 1: Full View (MVP - Phase 1)

```cpp
/// Full View Renderer - continuous text, no page boundaries
class FullViewRenderer : public ITextRenderer {
public:
    FullViewRenderer();
    ~FullViewRenderer();

    void Initialize(const bwxTextDocument& document) override;
    void Cleanup() override;

    void Render(wxGraphicsContext* gc, const wxRect& clientRect) override;
    wxSize GetBestSize() const override;

    int HitTest(int x, int y) const override;
    wxRect GetCursorRect(int position) const override;
    std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const override;

    int GetPageCount() const override { return 1; } // No pagination
    int GetPageAtPosition(int position) const override { return 0; }

    void OnResize(int width, int height) override;
    void OnScroll(int deltaX, int deltaY) override;
    void OnCursorMove(int newPosition) override;

    // Configuration
    void SetMargins(int left, int right) { m_marginLeft = left; m_marginRight = right; }
    void SetLineSpacing(double spacing) { m_lineSpacing = spacing; }

private:
    // Layout cache
    struct LayoutLine {
        int startPos;            // Start position in document
        int endPos;              // End position
        int y;                   // Y coordinate (top of line)
        int height;              // Line height
        std::vector<wxRect> charRects; // For hit testing
    };

    std::vector<LayoutLine> m_lines;

    // Settings
    int m_marginLeft = 20;
    int m_marginRight = 20;
    double m_lineSpacing = 1.2;

    // Scroll state
    int m_scrollY = 0;

    // Methods
    void CalculateLayout(int width);
    void RenderLine(wxGraphicsContext* gc, const LayoutLine& line);
    void RenderCursor(wxGraphicsContext* gc);
    void RenderSelection(wxGraphicsContext* gc);
};
```

### Renderer 2: Page View (Phase 2)

```cpp
/// Page View Renderer - MS Word-like with visible page boundaries
class PageViewRenderer : public ITextRenderer {
public:
    PageViewRenderer();
    ~PageViewRenderer();

    void Initialize(const bwxTextDocument& document) override;
    void Cleanup() override;

    void Render(wxGraphicsContext* gc, const wxRect& clientRect) override;
    wxSize GetBestSize() const override;

    int HitTest(int x, int y) const override;
    wxRect GetCursorRect(int position) const override;
    std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const override;

    int GetPageCount() const override { return m_pages.size(); }
    int GetPageAtPosition(int position) const override;

    void OnResize(int width, int height) override;
    void OnScroll(int deltaX, int deltaY) override;
    void OnCursorMove(int newPosition) override;

    // Page configuration
    struct PageSettings {
        int width = 595;         // A4: 595pt (210mm)
        int height = 842;        // A4: 842pt (297mm)
        int marginTop = 72;      // 1 inch = 72pt
        int marginBottom = 72;
        int marginLeft = 72;
        int marginRight = 72;
        wxString headerText;
        wxString footerText;
        bool showPageNumbers = true;
    };

    void SetPageSettings(const PageSettings& settings);
    PageSettings GetPageSettings() const { return m_pageSettings; }

private:
    struct Page {
        int startPos;            // Start position in document
        int endPos;              // End position
        int pageNumber;          // 1-based
        std::vector<LayoutLine> lines;
        wxString header;
        wxString footer;
    };

    std::vector<Page> m_pages;
    PageSettings m_pageSettings;

    // Background color for "desk"
    wxColour m_deskColor = wxColour(200, 200, 200);

    // Scroll state
    int m_scrollY = 0;

    // Methods
    void Paginate();             // Calculate page breaks
    void RenderPage(wxGraphicsContext* gc, const Page& page, int yOffset);
    void RenderPageBoundary(wxGraphicsContext* gc, const wxRect& pageRect);
    void RenderHeader(wxGraphicsContext* gc, const Page& page, const wxRect& pageRect);
    void RenderFooter(wxGraphicsContext* gc, const Page& page, const wxRect& pageRect);
};
```

### Renderer 3: Typewriter Mode (Phase 3)

```cpp
/// Typewriter Renderer - cursor locked in center, text scrolls up
class TypewriterRenderer : public ITextRenderer {
public:
    TypewriterRenderer();
    ~TypewriterRenderer();

    void Initialize(const bwxTextDocument& document) override;
    void Cleanup() override;

    void Render(wxGraphicsContext* gc, const wxRect& clientRect) override;
    wxSize GetBestSize() const override;

    int HitTest(int x, int y) const override;
    wxRect GetCursorRect(int position) const override;
    std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const override;

    int GetPageCount() const override { return 1; }
    int GetPageAtPosition(int position) const override { return 0; }

    void OnResize(int width, int height) override;
    void OnScroll(int deltaX, int deltaY) override;
    void OnCursorMove(int newPosition) override;

    // Typewriter-specific
    void SetCenterLock(bool enable) { m_centerLock = enable; }
    void SetAnimationDuration(int ms) { m_animationDuration = ms; }

private:
    // Cursor always at this Y coordinate
    int m_centerLineY = 0;

    // Scroll animation
    double m_scrollOffset = 0.0;
    double m_targetScrollOffset = 0.0;
    int m_animationDuration = 200; // milliseconds
    wxStopWatch m_animationTimer;
    bool m_animating = false;

    // Settings
    bool m_centerLock = true;
    double m_lineHeight = 20.0;

    // Methods
    void StartScrollAnimation(double targetOffset);
    void UpdateAnimation();
    void RenderCenteredView(wxGraphicsContext* gc);
};
```

### Renderer 4: Publisher View (Phase 4)

```cpp
/// Publisher Renderer - manuscript format (monospace, double-spaced)
class PublisherRenderer : public ITextRenderer {
public:
    PublisherRenderer();
    ~PublisherRenderer();

    void Initialize(const bwxTextDocument& document) override;
    void Cleanup() override;

    void Render(wxGraphicsContext* gc, const wxRect& clientRect) override;
    wxSize GetBestSize() const override;

    int HitTest(int x, int y) const override;
    wxRect GetCursorRect(int position) const override;
    std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const override;

    int GetPageCount() const override { return m_pages.size(); }
    int GetPageAtPosition(int position) const override;

    void OnResize(int width, int height) override;
    void OnScroll(int deltaX, int deltaY) override;
    void OnCursorMove(int newPosition) override;

    // Publisher settings
    struct PublisherSettings {
        wxString fontName = "Courier New";
        int fontSize = 12;
        double lineSpacing = 2.0;    // Double-spaced
        int paragraphIndent = 5;     // Characters (spaces)
        int marginTop = 72;          // 1 inch
        int marginBottom = 72;
        int marginLeft = 72;
        int marginRight = 72;
        wxString authorName;
        wxString manuscriptTitle;
    };

    void SetPublisherSettings(const PublisherSettings& settings);

private:
    PublisherSettings m_settings;

    // Stripped document (no formatting)
    wxString m_strippedText;

    // Page structure
    struct Page {
        int startPos;
        int endPos;
        int pageNumber;
        std::vector<wxString> lines;
    };

    std::vector<Page> m_pages;

    // Methods
    void StripFormatting();
    void Paginate();
    void RenderPage(wxGraphicsContext* gc, const Page& page, int yOffset);
    void RenderManuscriptHeader(wxGraphicsContext* gc, const Page& page);
};
```

---

## Component 3: Main Control (bwxTextEditor)

### Purpose

Coordinates document model and renderer, handles user input, manages view mode switching.

### Class Interface

```cpp
namespace bwx_sdk {
namespace gui {

/// Main text editor control
class bwxTextEditor : public wxControl, public bwxTextDocument::IDocumentObserver {
public:
    /// View modes
    enum ViewMode {
        VIEW_FULL,               // Continuous, no pages (MVP)
        VIEW_PAGE,               // MS Word-like pages
        VIEW_TYPEWRITER,         // Immersive, center-locked cursor
        VIEW_PUBLISHER           // Manuscript format
    };

    // ====================================================================
    // CONSTRUCTION
    // ====================================================================

    bwxTextEditor();
    bwxTextEditor(wxWindow* parent, wxWindowID id,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = 0);
    virtual ~bwxTextEditor();

    bool Create(wxWindow* parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0);

    // ====================================================================
    // VIEW MODE
    // ====================================================================

    void SetViewMode(ViewMode mode);
    ViewMode GetViewMode() const { return m_viewMode; }

    // ====================================================================
    // DOCUMENT ACCESS
    // ====================================================================

    bwxTextDocument& GetDocument() { return m_document; }
    const bwxTextDocument& GetDocument() const { return m_document; }

    // ====================================================================
    // RENDERER ACCESS
    // ====================================================================

    ITextRenderer* GetRenderer() { return m_renderer.get(); }
    const ITextRenderer* GetRenderer() const { return m_renderer.get(); }

    // ====================================================================
    // FILE I/O
    // ====================================================================

    bool LoadFromFile(const wxString& path);
    bool SaveToFile(const wxString& path);

    // ====================================================================
    // EDITING OPERATIONS (Convenience methods)
    // ====================================================================

    wxString GetText() const { return m_document.GetText(); }
    void SetText(const wxString& text) { m_document.SetText(text); }

    void Copy();
    void Cut();
    void Paste();
    void SelectAll();

    void Undo() { m_document.Undo(); }
    void Redo() { m_document.Redo(); }
    bool CanUndo() const { return m_document.CanUndo(); }
    bool CanRedo() const { return m_document.CanRedo(); }

    // ====================================================================
    // CUSTOM EVENTS
    // ====================================================================

    // (Define custom events for text changes, etc.)

protected:
    // ====================================================================
    // wxWidgets overrides
    // ====================================================================

    wxSize DoGetBestSize() const override;

    void Init();

    // Event handlers
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);

    // Drawing
    void PaintNow();
    void DoDraw(wxDC& dc);

    // IDocumentObserver implementation
    void OnTextChanged() override;
    void OnFormattingChanged() override;
    void OnCursorMoved() override;
    void OnSelectionChanged() override;

private:
    // Document model
    bwxTextDocument m_document;

    // Renderer (Strategy pattern)
    ViewMode m_viewMode;
    std::unique_ptr<ITextRenderer> m_renderer;

    // Input state
    bool m_selecting = false;
    int m_selectionStart = 0;

    // Focus state
    bool m_hasFocus = false;

    // Caret (cursor) blink
    wxTimer* m_caretTimer = nullptr;
    bool m_caretVisible = true;

    // Methods
    void SwitchRenderer(ViewMode mode);
    void HandleCharInput(wxChar ch);
    void HandleKeyCommand(int keyCode, bool ctrl, bool shift, bool alt);
    void UpdateCaret();
    void StartCaretBlink();
    void StopCaretBlink();

    wxDECLARE_DYNAMIC_CLASS(bwxTextEditor);
};

} // namespace gui
} // namespace bwx_sdk
```

---

## File Format Specification (.ktxt)

### Format: JSON with Comments

**Extension:** `.ktxt` (Kalahari Text)

**MIME Type:** `application/vnd.kalahari.text+json`

### Complete Example

```json
{
  "version": "1.0",
  "format": "kalahari-text",

  "metadata": {
    "title": "Chapter 1: The Beginning",
    "author": "Author Name",
    "created": "2025-11-04T10:00:00Z",
    "modified": "2025-11-04T14:30:00Z",
    "wordCount": 1523,
    "characterCount": 8945,
    "pageCount": 6,
    "language": "en"
  },

  "content": {
    "text": "The quick brown fox jumps over the lazy dog. This is a sample text with formatting...",

    "formatting": [
      {
        "start": 0,
        "end": 9,
        "bold": true,
        "fontSize": 14
      },
      {
        "start": 20,
        "end": 30,
        "italic": true,
        "textColor": "#FF0000"
      }
    ]
  },

  "comments": [
    {
      "id": "comment-1",
      "author": "Editor",
      "timestamp": "2025-11-04T12:00:00Z",
      "startPos": 15,
      "endPos": 25,
      "content": "This needs clarification. Can you expand on this point?",
      "resolved": false,
      "highlightColor": "#FFFF00",
      "replies": [
        {
          "id": "comment-1-reply-1",
          "author": "Author",
          "timestamp": "2025-11-04T13:00:00Z",
          "content": "Sure, I'll add more details.",
          "resolved": false
        }
      ]
    }
  ],

  "footnotes": [
    {
      "id": "footnote-1",
      "referencePos": 50,
      "number": 1,
      "content": "This is a footnote explaining the term.",
      "type": "footnote"
    },
    {
      "id": "endnote-1",
      "referencePos": 100,
      "number": 1,
      "content": "This is an endnote for additional information.",
      "type": "endnote"
    }
  ],

  "citations": [
    {
      "id": "citation-1",
      "type": "book",
      "author": "Smith, John",
      "year": 2020,
      "title": "The Complete Guide to Writing",
      "publisher": "Publishing House",
      "edition": "2nd",
      "isbn": "978-0-123456-78-9"
    },
    {
      "id": "citation-2",
      "type": "article",
      "author": "Jones, Alice",
      "year": 2019,
      "title": "Modern Writing Techniques",
      "journal": "Journal of Writing Studies",
      "volume": 15,
      "issue": 3,
      "pages": "45-67",
      "doi": "10.1234/jws.2019.15.3.45"
    }
  ],

  "inlineCitations": [
    {
      "position": 120,
      "citationId": "citation-1",
      "customText": null
    },
    {
      "position": 250,
      "citationId": "citation-2",
      "customText": "Jones (2019, p. 50)"
    }
  ],

  "indexEntries": [
    {
      "id": "index-1",
      "term": "Photosynthesis",
      "positions": [300, 450, 780],
      "subEntries": [
        {
          "id": "index-1-1",
          "term": "Light-dependent reactions",
          "positions": [310, 460]
        }
      ],
      "seeAlso": "Chlorophyll"
    }
  ],

  "viewSettings": {
    "lastViewMode": "page",
    "zoom": 1.0,
    "cursorPosition": 150
  }
}
```

### JSON Schema (for validation)

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Kalahari Text Format",
  "type": "object",
  "required": ["version", "format", "metadata", "content"],
  "properties": {
    "version": {
      "type": "string",
      "pattern": "^\\d+\\.\\d+$"
    },
    "format": {
      "type": "string",
      "const": "kalahari-text"
    },
    "metadata": {
      "type": "object",
      "required": ["title", "created"],
      "properties": {
        "title": { "type": "string" },
        "author": { "type": "string" },
        "created": { "type": "string", "format": "date-time" },
        "modified": { "type": "string", "format": "date-time" },
        "wordCount": { "type": "integer", "minimum": 0 },
        "characterCount": { "type": "integer", "minimum": 0 },
        "pageCount": { "type": "integer", "minimum": 0 },
        "language": { "type": "string", "pattern": "^[a-z]{2}$" }
      }
    },
    "content": {
      "type": "object",
      "required": ["text"],
      "properties": {
        "text": { "type": "string" },
        "formatting": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["start", "end"],
            "properties": {
              "start": { "type": "integer", "minimum": 0 },
              "end": { "type": "integer", "minimum": 0 },
              "bold": { "type": "boolean" },
              "italic": { "type": "boolean" },
              "underline": { "type": "boolean" },
              "strikethrough": { "type": "boolean" },
              "fontName": { "type": "string" },
              "fontSize": { "type": "integer", "minimum": 6, "maximum": 72 },
              "textColor": { "type": "string", "pattern": "^#[0-9A-Fa-f]{6}$" },
              "backgroundColor": { "type": "string", "pattern": "^#[0-9A-Fa-f]{6}$" }
            }
          }
        }
      }
    }
  }
}
```

---

## Implementation Roadmap

### Phase 1: MVP (Task #00019, Weeks 10-12, 2-3 weeks)

**Goal:** Functional text editor with Full View mode

**Deliverables:**
1. ✅ `bwxTextDocument` class - Gap Buffer implementation
2. ✅ `FullViewRenderer` class - Basic rendering
3. ✅ `bwxTextEditor` control - Event handling, input
4. ✅ Basic formatting (bold, italic, underline, font, size, color)
5. ✅ Cursor & selection (keyboard + mouse)
6. ✅ Undo/Redo (Command pattern, 100 commands)
7. ✅ Copy/Cut/Paste
8. ✅ Load/Save (.ktxt format)
9. ✅ Word count
10. ✅ Unit tests (text operations, formatting, undo/redo)
11. ✅ Integration into EditorPanel (Kalahari)

**File Structure:**
```
external/bwx_sdk/
├── include/bwx_sdk/bwx_gui/
│   ├── bwx_text_document.h         # Document model
│   ├── bwx_text_renderer.h         # ITextRenderer interface
│   ├── bwx_text_editor.h           # Main control
│   └── renderers/
│       └── full_view_renderer.h    # Full View implementation
├── src/bwx_gui/
│   ├── bwx_text_document.cpp
│   ├── bwx_text_renderer.cpp
│   ├── bwx_text_editor.cpp
│   └── renderers/
│       └── full_view_renderer.cpp
└── tests/
    ├── test_text_document.cpp
    ├── test_text_editor.cpp
    └── test_full_view_renderer.cpp
```

**Estimated LOC:** ~2,500-3,000 lines
- Document model: ~800 lines
- Full View Renderer: ~600 lines
- Main control: ~800 lines
- Tests: ~300 lines

**Critical Path:**
1. Day 1-2: Document model (text storage, cursor, selection)
2. Day 3-4: Undo/Redo system (Command pattern)
3. Day 5-6: Formatting system (TextFormat, FormatRun)
4. Day 7-8: Full View Renderer (layout, rendering)
5. Day 9-10: Main control (event handling, input)
6. Day 11-12: File I/O (.ktxt), integration, testing

**Risk Mitigation:**
- Gap Buffer is well-understood (many references)
- Full View is simplest renderer (no pagination)
- Command pattern is standard (Undo/Redo)
- Focus on core features only (no advanced features yet)

### Phase 2: Page View Mode (Task #00020, Weeks 13-15, 3 weeks)

**Goal:** MS Word-like page view

**Deliverables:**
1. ✅ `PageViewRenderer` class
2. ✅ Pagination engine (automatic page breaks)
3. ✅ Headers/Footers rendering
4. ✅ Page numbers
5. ✅ Configurable margins
6. ✅ Manual page breaks
7. ✅ Multi-page view (vertical stack)

**Estimated LOC:** +2,000 lines

### Phase 3: Typewriter Mode (Task #00021, Weeks 16-18, 3 weeks)

**Goal:** Immersive writing mode

**Deliverables:**
1. ✅ `TypewriterRenderer` class
2. ✅ Center-lock cursor
3. ✅ Reverse scroll animation
4. ✅ Smooth line transition (200ms)
5. ✅ Minimalist UI integration

**Estimated LOC:** +1,500 lines

### Phase 4: Publisher View (Task #00022, Weeks 19-20, 2 weeks)

**Goal:** Manuscript format

**Deliverables:**
1. ✅ `PublisherRenderer` class
2. ✅ Format stripping
3. ✅ Monospace layout (Courier New, 12pt)
4. ✅ Double spacing
5. ✅ Publisher-standard margins
6. ✅ Manuscript header (author/page)

**Estimated LOC:** +1,000 lines

### Phase 5: Comments System (Task #00023, Weeks 21-23, 3 weeks)

**Goal:** Collaborative editing

**Deliverables:**
1. ✅ Comment data structure (in document model)
2. ✅ Inline indicators (highlighting)
3. ✅ Comment sidebar UI
4. ✅ Threading (replies)
5. ✅ Authorship & timestamps
6. ✅ Resolution tracking

**Estimated LOC:** +2,000 lines

### Phase 6: Footnotes (Task #00024, Weeks 24-25, 2 weeks)

**Deliverables:**
1. ✅ Footnote data structure
2. ✅ Automatic numbering
3. ✅ Footnote/Endnote placement (renderer integration)
4. ✅ Reference links (click to jump)
5. ✅ Separator line

**Estimated LOC:** +1,000 lines

### Phase 7: Bibliography (Task #00025, Weeks 26-27, 2 weeks)

**Deliverables:**
1. ✅ Citation data structure
2. ✅ Citation manager UI (add/edit/delete)
3. ✅ Inline citations
4. ✅ Auto-generated bibliography
5. ✅ Multiple citation styles (APA, MLA, Chicago)

**Estimated LOC:** +1,500 lines

### Phase 8: Indexes (Task #00026, Weeks 28-29, 2 weeks)

**Deliverables:**
1. ✅ Index entry marking UI
2. ✅ Auto-generated index (alphabetical)
3. ✅ Cross-references (See, See also)
4. ✅ Hierarchical entries
5. ✅ Page number generation

**Estimated LOC:** +1,000 lines

### Total Estimates

**Full Implementation:**
- **Duration:** 20 weeks (5 months)
- **LOC:** ~12,000-15,000 lines
- **Phases:** 8 tasks (#00019 - #00026)
- **Team:** 1 developer (with AI assistance)

---

## Testing Strategy

### Unit Testing (Catch2 v3)

**Document Model Tests:**
- Text operations (insert, delete, get)
- Gap Buffer mechanics (gap move, expand)
- Formatting (apply, get, clear)
- Cursor operations (move, position)
- Selection operations (set, clear, extend)
- Undo/Redo (command execution, stack limits)
- Comments (add, update, delete, threading)
- Footnotes (add, renumber, delete)
- Citations (add, format generation)
- Index (add, generate sorted)

**Renderer Tests:**
- Layout calculation (line breaks, word wrap)
- Hit testing (screen to document position)
- Cursor rectangle calculation
- Selection rectangle calculation
- Pagination (page breaks, page count)

**Main Control Tests:**
- Event handling (keyboard input, mouse clicks)
- View mode switching
- File I/O (save, load, roundtrip)
- Clipboard operations (copy, cut, paste)

### Integration Testing

**End-to-End Workflows:**
1. Create document → Type text → Apply formatting → Save → Load → Verify
2. Multi-line editing → Undo → Redo → Verify state
3. Add comments → Thread replies → Resolve → Export
4. Add citations → Generate bibliography → Verify formatting
5. Mark index entries → Generate index → Verify page numbers

### Performance Testing

**Benchmarks:**
- 10,000 word document: Keystroke latency <100ms
- 50 page document: Pagination <500ms
- 100 comments: Rendering <60 FPS
- File save/load: <2 seconds for 100 pages

**Profiling:**
- Identify hotspots (rendering loops, layout calculation)
- Optimize critical paths
- Memory profiling (check for leaks)

### Manual Testing

**Cross-Platform:**
- Linux (Ubuntu 22.04, GCC 10+)
- macOS (macOS 14, Clang 10+)
- Windows (Windows 11, MSVC 2019+)

**User Scenarios:**
1. Write 1000-word chapter with formatting
2. Switch between all 4 view modes
3. Add 10 comments, 5 footnotes, 3 citations
4. Generate bibliography and index
5. Export to PDF, RTF, HTML

---

## Open Questions & Future Considerations

### Question 1: Text Storage Migration

**When to migrate from Gap Buffer to Rope/Piece Table?**
- Profile with 50K+ word documents
- If insert/delete >100ms, consider Rope
- If undo/redo memory >100MB, consider Piece Table

### Question 2: Collaborative Editing

**Real-time collaboration (like Google Docs)?**
- Requires: Operational Transformation (OT) or CRDTs
- Out of scope for MVP (Phase 1-8)
- Consider for Phase 9+ (Cloud features)

### Question 3: Spell Checking

**Built-in or plugin?**
- Likely plugin (leverages plugin system)
- Use Hunspell library (open source, 100+ languages)
- Phase 9+ consideration

### Question 4: Track Changes

**MS Word-style revision tracking?**
- Complex feature (separate from comments)
- Requires: Change history, accept/reject UI
- Phase 9+ consideration

### Question 5: Export Quality

**Professional-quality PDF export?**
- Use wxPrintout + wxPdfDocument
- Or: Generate LaTeX → compile to PDF
- Phase 8+ after all view modes stable

---

## Architectural Decisions Log

### ADR-001: Text Storage (Gap Buffer)

**Date:** 2025-11-04
**Status:** Accepted for MVP

**Context:** Need efficient text storage for editing

**Decision:** Use Gap Buffer for MVP

**Rationale:**
- Simple implementation (critical for timeline)
- Good performance for typical editing (insertion at cursor)
- Well-understood algorithm
- Can migrate later if profiling shows issues

**Alternatives Considered:**
- Rope: More complex, better for very large documents
- Piece Table: Optimal for undo/redo, more complex

**Consequences:**
- MVP can be delivered on time
- Performance acceptable for <50K words
- Future migration possible if needed

### ADR-002: Rendering Strategy (Strategy Pattern)

**Date:** 2025-11-04
**Status:** Accepted

**Context:** Need 4 different view modes

**Decision:** Strategy Pattern with swappable renderers

**Rationale:**
- Clean separation of document model and view
- Easy to add new view modes
- Each renderer optimized for its use case
- Standard OO design pattern

**Alternatives Considered:**
- Single renderer with mode flags: Too complex, hard to maintain
- Separate controls: Code duplication, hard to switch

**Consequences:**
- Each view mode is independent module
- Adding new view mode doesn't break existing
- Testing each renderer in isolation

### ADR-003: File Format (Custom JSON)

**Date:** 2025-11-04
**Status:** Accepted

**Context:** Need format for advanced features (comments, footnotes, etc.)

**Decision:** Custom JSON format (.ktxt)

**Rationale:**
- RTF insufficient for metadata (comments, citations, index)
- JSON human-readable (debugging, version control)
- Easy to parse (nlohmann_json library)
- Extensible (add new fields without breaking old files)

**Alternatives Considered:**
- RTF: Standard, but limited metadata support
- XML: Verbose, harder to read
- Binary: Fast, but not human-readable

**Consequences:**
- Need import/export for RTF, PDF, etc.
- File size slightly larger than binary
- Version migration easier (JSON is flexible)

### ADR-004: Build Location (bwx_sdk)

**Date:** 2025-11-04
**Status:** Accepted

**Context:** Where to implement custom control

**Decision:** Build in bwx_sdk first, then integrate to Kalahari

**Rationale:**
- Follows "Need → Solution" integration strategy
- Reusable beyond Kalahari
- Standalone testing possible
- Clean separation of concerns

**Alternatives Considered:**
- Build directly in Kalahari: Faster short-term, but not reusable

**Consequences:**
- Git submodule workflow (push to bwx_sdk → update Kalahari)
- Slightly longer development cycle (commit to 2 repos)
- Better code organization long-term

---

## Summary

**Complete architecture defined for:**
- Document model (text storage, formatting, metadata, advanced features)
- 4 rendering modes (Full, Page, Typewriter, Publisher)
- Main control (event handling, input, view switching)
- File format (.ktxt JSON)
- Testing strategy (unit, integration, performance, manual)

**Ready for implementation:**
- Phase 1 (MVP) can start immediately
- All interfaces defined
- Implementation roadmap clear (20 weeks)
- File structure planned

**Next Step:** Update Task #00019 with MVP specification (Phase 1 only)

---

**Document Status:** ✅ Complete
**Next Review:** After Phase 1 implementation (Week 12)
**Maintainer:** Kalahari Core Team
