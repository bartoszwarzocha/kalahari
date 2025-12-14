# 00035: KChapter Document Format

## Status
IN_PROGRESS

## Goal
Replace RTF chapter storage with a new .kchapter format (JSON + HTML) that supports rich metadata, future extensibility for document templates, and disaster recovery via plaintext backup.

## Context

### Current State
- Chapters stored as .rtf files in `content/` directory
- ProjectManager uses `loadChapterContent()` / `saveChapterContent()` for RTF I/O
- EditorPanel uses QTextEdit with `setHtml()` / `toHtml()` for display
- No metadata per-chapter (word count, status, notes stored separately)

### Problems with RTF
1. RTF parsing is fragile and format-dependent
2. No structured metadata support
3. No plaintext backup for disaster recovery
4. Hard to extend for future features (annotations, comments)
5. External tools may corrupt formatting

### Design Decision (ADR)
After analysis of format options (RTF+JSON, HTML+JSON, custom binary), HTML+JSON was chosen:
- QTextDocument natively supports HTML (toHtml/setHtml)
- JSON provides structured metadata
- Plaintext backup included for disaster recovery
- Extensible for future features (templates, annotations)

## Architecture

### File Format: .kchapter

```json
{
  "kalahari": {
    "version": "1.0",
    "type": "chapter"
  },
  "content": {
    "html": "<p>Chapter content with <b>formatting</b>...</p>",
    "plainText": "Chapter content with formatting..."
  },
  "statistics": {
    "wordCount": 0,
    "characterCount": 0,
    "paragraphCount": 0,
    "lastModified": "2025-01-15T10:30:00Z"
  },
  "metadata": {
    "title": "Chapter Title",
    "status": "draft",
    "notes": "",
    "color": null
  },
  "annotations": {
    "comments": [],
    "highlights": []
  }
}
```

### File Structure Changes

```
my-book.klh/
├── manifest.json          (unchanged)
├── content/
│   ├── chapter-001.kchapter  (NEW - replaces .rtf)
│   ├── chapter-002.kchapter
│   └── ...
└── .kalahari/             (cache, unchanged)
```

### Class Changes

#### ChapterDocument (NEW)
```cpp
// include/kalahari/core/chapter_document.h
namespace core {

class ChapterDocument {
public:
    // Version & type
    static constexpr const char* FORMAT_VERSION = "1.0";
    static constexpr const char* FORMAT_TYPE = "chapter";

    // Content
    QString html() const;
    QString plainText() const;
    void setHtml(const QString& html);
    void setPlainText(const QString& text);

    // Statistics (auto-calculated on setHtml)
    int wordCount() const;
    int characterCount() const;
    int paragraphCount() const;
    QDateTime lastModified() const;

    // Metadata
    QString title() const;
    void setTitle(const QString& title);
    QString status() const;
    void setStatus(const QString& status);
    QString notes() const;
    void setNotes(const QString& notes);

    // I/O
    static std::optional<ChapterDocument> load(const QString& path);
    bool save(const QString& path) const;

    // Conversion
    static ChapterDocument fromRtf(const QString& rtfContent);
    QJsonObject toJson() const;
    static ChapterDocument fromJson(const QJsonObject& json);

private:
    QString m_html;
    QString m_plainText;
    int m_wordCount = 0;
    int m_characterCount = 0;
    int m_paragraphCount = 0;
    QDateTime m_lastModified;
    QString m_title;
    QString m_status = "draft";
    QString m_notes;
    QColor m_color;
};

} // namespace core
```

#### ProjectManager Changes
```cpp
// Modified methods
QString loadChapterContent(const QString& elementId);  // Now loads .kchapter
bool saveChapterContent(const QString& elementId, const QString& htmlContent);
ChapterDocument* getChapterDocument(const QString& elementId);

// New methods
bool migrateRtfToKchapter(const QString& elementId);  // Migration helper
void migrateAllChapters();  // Batch migration
```

### Migration Strategy

**Automatic migration on open:**
1. When opening project, check each chapter file extension
2. If .rtf exists and .kchapter doesn't, migrate automatically
3. Keep .rtf as backup (renamed to .rtf.bak)
4. Log migration status

**Manual migration command:**
- File > Tools > Migrate to KChapter Format

## Scope

### Phase 1 (This OpenSpec)
- [x] ChapterDocument class with JSON serialization
- [ ] ProjectManager .kchapter I/O
- [ ] Automatic RTF→KChapter migration
- [ ] EditorPanel integration
- [ ] Statistics auto-calculation

### Phase 2 (Future - Document Templates)
- [ ] Template system for different book types
- [ ] Paragraph styles (novel, screenplay, etc.)
- [ ] Style picker in toolbar

### Phase 3 (Future - Annotations)
- [ ] Comments system
- [ ] Highlights with colors
- [ ] Revision tracking

### Excluded
- Custom binary format (rejected - not human-readable)
- DOCX as internal format (rejected - too complex)
- Scene-level granularity (deferred to Custom Editor)

## Acceptance Criteria

- [ ] New chapters created as .kchapter files
- [ ] Existing RTF chapters migrated automatically on open
- [ ] Content preserved during migration (HTML roundtrip)
- [ ] Statistics (word count, etc.) calculated and stored
- [ ] Metadata (status, notes) saved per-chapter
- [ ] Plaintext backup included in .kchapter
- [ ] Build passes, tests pass
- [ ] Manual test: edit chapter, save, reopen - content preserved

## Technical Notes

### HTML Sanitization
QTextDocument produces verbose HTML. Consider:
- Stripping Qt-specific tags on save
- Normalizing font-family to generic fonts
- Removing inline styles where possible

### Word Count Algorithm
```cpp
int calculateWordCount(const QString& plainText) {
    // Split on whitespace, count non-empty tokens
    return plainText.split(QRegularExpression("\\s+"),
                           Qt::SkipEmptyParts).count();
}
```

### Backward Compatibility
- .klh projects with .rtf files still open (migration on load)
- No changes to manifest.json structure
- Version field allows future format evolution

## Files to Create

| File | Purpose |
|------|---------|
| `include/kalahari/core/chapter_document.h` | ChapterDocument class |
| `src/core/chapter_document.cpp` | Implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `include/kalahari/core/project_manager.h` | Add .kchapter methods |
| `src/core/project_manager.cpp` | .kchapter I/O, migration |
| `src/gui/panels/editor_panel.cpp` | Use ChapterDocument |
| `src/CMakeLists.txt` | Add new source file |

## Implementation Order

1. Create ChapterDocument class with JSON I/O
2. Update ProjectManager to use .kchapter
3. Add migration logic (RTF → KChapter)
4. Update EditorPanel to use ChapterDocument
5. Test with example project

## References

- ROADMAP.md Section 1.5 Custom Text Editor
- ADR: project_docs/15_text_editor_architecture.md
- Qt Documentation: QTextDocument, toHtml/setHtml
