# Task #00012: Document Model + JSON Serialization (RTF + 3-Section Structure)

## Context
- **Phase:** Phase 0 Week 8
- **Roadmap Reference:** ROADMAP.md Phase 0, Week 8: Document Model + JSON Serialization
- **Related Docs:**
  - [project_docs/03_architecture.md](../project_docs/03_architecture.md) - Composite pattern principles
  - [project_docs/07_mvp_tasks.md](../project_docs/07_mvp_tasks.md) - Week 8 detailed tasks
- **Dependencies:**
  - vcpkg setup with nlohmann_json and libzip (✅ Complete)
  - Catch2 test framework (✅ Complete)
  - CMake build system (✅ Complete)
  - Task #00011 (PluginArchive RAII pattern for ZIP) (✅ Complete)

## Objective

Implement the **core document model** for Kalahari representing a **complete book structure** (front matter + body + back matter) with **RTF formatting support** and **.klh file format** (ZIP container).

**Key Architectural Decision (Approved):**
- **Pragmatic + Flexible approach (Option C)**
- BookElement with **type as string** (not enum) → extensibility
- **3-section structure:** frontMatter, body (Parts), backMatter
- **RTF format** for content (wxRichTextCtrl native support)
- **Metadata map** for future extensibility

**Success Criteria:**
- ✅ BookElement class (universal, type as string, metadata map)
- ✅ Part class (container for chapters)
- ✅ Book class (3 sections: frontMatter, body, backMatter)
- ✅ Document class (wrapper with project metadata)
- ✅ JSON serialization (toJson/fromJson) for all classes
- ✅ DocumentArchive (RAII ZIP handler, RTF files)
- ✅ .klh save/load with proper structure
- ✅ Unit tests >= 80% coverage
- ✅ No memory leaks (smart pointers, RAII)

## Finalized Architecture (Option C - Approved)

### 1. Class Structure

```cpp
// Universal element with flexible type (NOT abstract base)
class BookElement {
public:
    std::string m_type;           // "title_page", "chapter", "bibliography", custom...
    std::string m_id;             // UUID
    std::string m_title;          // Display title
    std::filesystem::path m_file; // Relative path to RTF (e.g., "frontmatter/title_page.rtf")
    int m_wordCount;              // Cached word count
    std::chrono::system_clock::time_point m_created;
    std::chrono::system_clock::time_point m_modified;
    std::map<std::string, std::string> m_metadata;  // Extensible (custom fields)

    nlohmann::json toJson() const;
    static BookElement fromJson(const nlohmann::json& j);
};

// Part - container for chapters (body structure)
class Part {
public:
    std::string m_id;
    std::string m_title;
    std::vector<std::shared_ptr<BookElement>> m_chapters;  // type="chapter"

    int getWordCount() const;  // Sum of all chapters
    void addChapter(std::shared_ptr<BookElement> chapter);
    void removeChapter(const std::string& chapterId);

    nlohmann::json toJson() const;
    static Part fromJson(const nlohmann::json& j);
};

// Book - 3-section professional structure
class Book {
public:
    std::vector<std::shared_ptr<BookElement>> m_frontMatter;  // title_page, copyright, dedication, preface...
    std::vector<std::shared_ptr<Part>> m_body;                 // Parts → Chapters
    std::vector<std::shared_ptr<BookElement>> m_backMatter;   // epilogue, glossary, bibliography...

    int getWordCount() const;  // Sum of all body chapters

    nlohmann::json toJson() const;
    static Book fromJson(const nlohmann::json& j);
};

// Document - top-level wrapper with project metadata
class Document {
public:
    std::string m_id;        // Project UUID
    std::string m_title;     // Project title
    std::string m_author;    // Author name
    std::string m_language;  // ISO 639-1 (e.g., "en", "pl")
    Book m_book;             // The book structure

    bool save(const std::filesystem::path& path);
    static std::optional<Document> load(const std::filesystem::path& path);

    nlohmann::json toJson() const;
    static Document fromJson(const nlohmann::json& j);
};
```

### 2. Known Types (Phase 0)

**Predefined types** (GUI provides icons/labels):
```cpp
// In header file (constants)
namespace kalahari::core {
    // Known front matter types
    inline constexpr const char* TYPE_TITLE_PAGE = "title_page";
    inline constexpr const char* TYPE_COPYRIGHT = "copyright";
    inline constexpr const char* TYPE_DEDICATION = "dedication";
    inline constexpr const char* TYPE_PREFACE = "preface";

    // Body type
    inline constexpr const char* TYPE_CHAPTER = "chapter";

    // Known back matter types
    inline constexpr const char* TYPE_EPILOGUE = "epilogue";
    inline constexpr const char* TYPE_GLOSSARY = "glossary";
    inline constexpr const char* TYPE_BIBLIOGRAPHY = "bibliography";
    inline constexpr const char* TYPE_ABOUT_AUTHOR = "about_author";
}
```

**Unknown types** → displayed as generic "Section" in GUI (Phase 1)

### 3. .klh File Format (ZIP Container)

```
my_novel.klh (ZIP archive)
├── manifest.json                # Document metadata + complete structure
└── content/
    ├── frontmatter/
    │   ├── title_page.rtf      # RTF with formatting
    │   ├── copyright.rtf
    │   ├── dedication.rtf
    │   └── preface.rtf
    ├── body/
    │   ├── part-001/
    │   │   ├── chapter-001.rtf
    │   │   └── chapter-002.rtf
    │   └── part-002/
    │       └── chapter-001.rtf
    └── backmatter/
        ├── epilogue.rtf
        ├── glossary.rtf
        └── bibliography.rtf
```

**Why RTF (not txt):**
- ✅ wxRichTextCtrl **native support** (LoadFile/SaveFile)
- ✅ Formatting: bold, italic, underline, fonts, sizes, colors
- ✅ Styles: headings, quotes, paragraphs
- ✅ Industry standard (MS Word, LibreOffice compatible)
- ✅ Reasonably git-diff friendly
- ✅ Compact (not verbose like HTML)

### 4. JSON Structure (manifest.json)

```json
{
  "version": "1.0.0",
  "document": {
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "title": "My Novel",
    "author": "John Doe",
    "language": "en",
    "created": "2025-10-30T10:00:00Z",
    "modified": "2025-10-30T15:30:00Z"
  },
  "book": {
    "frontMatter": [
      {
        "type": "title_page",
        "id": "front-001",
        "title": "Title Page",
        "file": "content/frontmatter/title_page.rtf",
        "wordCount": 50,
        "created": "2025-10-30T10:00:00Z",
        "modified": "2025-10-30T10:00:00Z",
        "metadata": {}
      },
      {
        "type": "dedication",
        "id": "front-002",
        "title": "For my family",
        "file": "content/frontmatter/dedication.rtf",
        "wordCount": 15,
        "metadata": {}
      }
    ],
    "body": [
      {
        "id": "part-001",
        "title": "Part I: Beginning",
        "chapters": [
          {
            "type": "chapter",
            "id": "chapter-001",
            "title": "Chapter 1: The Adventure Begins",
            "file": "content/body/part-001/chapter-001.rtf",
            "wordCount": 2500,
            "created": "2025-10-30T10:00:00Z",
            "modified": "2025-10-30T12:00:00Z",
            "metadata": {
              "pov": "First Person",
              "scene_location": "London"
            }
          }
        ]
      }
    ],
    "backMatter": [
      {
        "type": "epilogue",
        "id": "back-001",
        "title": "Epilogue",
        "file": "content/backmatter/epilogue.rtf",
        "wordCount": 800,
        "metadata": {}
      },
      {
        "type": "bibliography",
        "id": "back-002",
        "title": "Bibliography",
        "file": "content/backmatter/bibliography.rtf",
        "wordCount": 350,
        "metadata": {}
      }
    ]
  }
}
```

**Flexible metadata map examples:**
- Chapter: `{"pov": "Third Person", "location": "Paris", "timeline": "1942"}`
- Bibliography: `{"citation_style": "APA"}` (Phase 2+: structured)
- Custom section: `{"custom_field": "value"}` (user-defined, plugin-defined)

## Implementation Plan (Checklist)

### Phase 1: Core Classes
- [ ] Create `include/kalahari/core/book_element.h`
  - BookElement with type string, metadata map, RTF path
  - Constructor, getters/setters
  - toJson/fromJson
- [ ] Create `src/core/book_element.cpp`
- [ ] Create `include/kalahari/core/part.h`
  - Part with vector of chapters
  - addChapter, removeChapter, getWordCount
  - toJson/fromJson
- [ ] Create `src/core/part.cpp`
- [ ] Create `include/kalahari/core/book.h`
  - Book with 3 vectors: frontMatter, body, backMatter
  - Helper methods for adding elements
  - getWordCount (sum body only)
  - toJson/fromJson
- [ ] Create `src/core/book.cpp`
- [ ] Add to `src/CMakeLists.txt`

### Phase 2: Document Wrapper
- [ ] Create `include/kalahari/core/document.h`
  - Document with id, title, author, language, Book
  - save/load method signatures (stub for now)
  - toJson/fromJson
- [ ] Create `src/core/document.cpp`
- [ ] UUID generation helper (simple: timestamp + random)
- [ ] ISO 8601 timestamp helpers (std::chrono → string)

### Phase 3: JSON Serialization (nlohmann_json)
- [ ] Implement BookElement::toJson()
- [ ] Implement BookElement::fromJson() (static factory)
- [ ] Implement Part::toJson() (with chapters array)
- [ ] Implement Part::fromJson()
- [ ] Implement Book::toJson() (3 sections)
- [ ] Implement Book::fromJson()
- [ ] Implement Document::toJson() (manifest structure)
- [ ] Implement Document::fromJson()
- [ ] Handle missing optional fields gracefully (use defaults)

### Phase 4: DocumentArchive (ZIP Handler)
- [ ] Create `include/kalahari/core/document_archive.h`
  - RAII wrapper for .klh ZIP (similar to PluginArchive)
  - save(Document, path) - create ZIP, write manifest.json, write RTF files
  - load(path) → optional<Document> - extract, parse manifest, return Document
- [ ] Create `src/core/document_archive.cpp`
  - Use libzip (zip_open, zip_add, zip_fopen)
  - RAII cleanup (destructor closes ZIP handle)
  - Platform-specific temp directory for extraction (if needed in Phase 1)
- [ ] Implement Document::save() using DocumentArchive
- [ ] Implement Document::load() using DocumentArchive

### Phase 5: Constants & Utilities
- [ ] Create `include/kalahari/core/book_constants.h`
  - Known type constants (TYPE_TITLE_PAGE, etc.)
  - Helper: getDisplayName(type) → user-friendly name
- [ ] Word count helper (count words in RTF - Phase 1 may need wxRichTextCtrl)
  - For Phase 0: stub returning 0 or length/5 estimate

### Phase 6: Unit Tests (Catch2)
- [ ] Create `tests/core/test_book_element.cpp`
  - Test construction, getters/setters
  - Test toJson/fromJson round-trip
  - Test metadata map extensibility
- [ ] Create `tests/core/test_part.cpp`
  - Test addChapter/removeChapter
  - Test getWordCount aggregation
  - Test toJson/fromJson with chapters
- [ ] Create `tests/core/test_book.cpp`
  - Test 3-section structure
  - Test adding to each section
  - Test getWordCount (body only)
  - Test toJson/fromJson full structure
- [ ] Create `tests/core/test_document.cpp`
  - Test Document creation
  - Test metadata (id, title, author)
  - Test toJson/fromJson
- [ ] Create `tests/core/test_document_archive.cpp`
  - Test save: create .klh, verify ZIP structure
  - Test load: extract, parse, verify data
  - Test round-trip: save → load → compare
  - Test edge cases: empty book, missing files, corrupted JSON
- [ ] Update `tests/CMakeLists.txt`
- [ ] Aim for >= 80% code coverage

### Phase 7: Documentation & Integration
- [ ] Doxygen comments for all public APIs
- [ ] Code examples in headers
- [ ] Update CHANGELOG.md (Task #00012 entry)
- [ ] Update ROADMAP.md (mark Week 8 complete)
- [ ] Build and test on all platforms (CI/CD)

## Why This Approach (Option C Rationale)

**✅ Professional structure:**
- 3-section book anatomy (frontMatter/body/backMatter) = industry standard
- Supports all common book elements (title page, copyright, chapters, epilogue, bibliography)

**✅ RTF format:**
- wxRichTextCtrl native support (no parsing needed)
- Formatting from day 1 (bold, italic, styles)
- Won't require rewrite in Phase 1

**✅ Flexible type system:**
- String type (not enum) = extensible without recompilation
- Users can add custom sections (e.g., "character_notes", "world_map")
- Plugins can define new types (Phase 2+)

**✅ Metadata map:**
- Future-proof extensibility
- Plugins can add custom fields without modifying core classes
- GUI can store view-specific data (collapsed state, etc.)

**✅ Git-friendly:**
- Separate RTF files = better diffs
- manifest.json is human-readable
- ZIP can be extracted for inspection

**✅ Lazy loading ready:**
- RTF paths (not inline content) = load on-demand in Phase 1
- Metadata eager-loaded (structure, word counts)

**Alternatives considered:**
- **txt format:** ❌ No formatting, would require rewrite
- **HTML format:** ❌ Verbose, overkill, harder to edit
- **Markdown:** ❌ Limited formatting, not native in wxRichTextCtrl
- **Enum for types:** ❌ Not extensible, requires recompilation for new types
- **Single JSON file:** ❌ Doesn't scale to large books, no lazy loading
- **SQLite database:** ❌ Overkill for Phase 0, deferred to Phase 2 for full-text search

## Risks & Open Questions

**Q:** How to generate UUIDs without external library?
- **A:** Simple approach: timestamp (milliseconds) + 4 random hex digits. Collision-resistant for single-user app.
  ```cpp
  std::string generateId() {
      auto now = std::chrono::system_clock::now().time_since_epoch();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
      std::random_device rd;
      return std::to_string(ms) + "-" + std::to_string(rd() % 10000);
  }
  ```

**Q:** How to count words in RTF without wxRichTextCtrl?
- **A:** Phase 0 stub: return 0 or estimate (content.length() / 5). Phase 1: use wxRichTextCtrl::GetNumberOfLines() or similar.

**Q:** Should Part be mandatory or optional?
- **A:** Optional. If user doesn't want parts, create 1 default part with no title. GUI can hide/show based on number of parts.

**Risk:** libzip write operations might differ from read (PluginArchive only reads)
- **Mitigation:** Check libzip docs for zip_add_file, test early

**Risk:** RTF might not be portable across platforms (line endings, encoding)
- **Mitigation:** wxRichTextCtrl handles this. Test on Windows/macOS/Linux early.

**Q:** Should word count be stored in JSON or calculated on load?
- **A:** Store in JSON (cached). Recalculate on content save. Phase 0: stub values OK.

## Status
- **Created:** 2025-10-30
- **Architecture Approved:** 2025-10-30 (User approved Option C)
- **Ready for Implementation:** ✅ YES
- **Started:**
- **Completed:**

## Implementation Notes
(Will be filled during implementation)

## Verification
- [ ] Code compiles on Windows (CI)
- [ ] Code compiles on macOS (CI)
- [ ] Code compiles on Linux (local + CI)
- [ ] Tests pass >= 80% coverage
- [ ] No memory leaks (smart pointers verified)
- [ ] Round-trip test: Document → save → load → verify identical
- [ ] Edge cases: empty book, missing sections, corrupted manifest
- [ ] .klh is valid ZIP (can unzip manually)
- [ ] manifest.json is valid JSON (can parse with jq)
- [ ] RTF files are valid (can open in WordPad/TextEdit)
- [ ] Documentation complete (Doxygen)
- [ ] CHANGELOG.md updated
- [ ] CI/CD passes

## Related Tasks
- **Depends on:**
  - Task #00001 (CMake + vcpkg setup) ✅
  - Task #00009 (nlohmann_json in vcpkg) ✅
  - Task #00011 (PluginArchive RAII ZIP pattern) ✅
- **Blocks:**
  - Phase 1 Week 9-10: wxRichTextCtrl Integration (needs Document model + RTF)
  - Phase 1 Week 11-12: Project Navigator Panel (needs Book tree structure)
  - Phase 1 Week 13-14: Save/Load in GUI (needs .klh handler)
- **Related:**
  - Task #00008 (Settings system - similar JSON pattern)
  - Task #00011 (Plugin Archive - similar RAII ZIP pattern)

## Post-Completion Review
(To be filled after completion)

**Estimated Time:** 5 days (per 07_mvp_tasks.md)
**Actual Time:** TBD

---

## Quick Reference: Code Skeleton

### BookElement
```cpp
class BookElement {
public:
    BookElement(std::string type, std::string id, std::string title, std::filesystem::path file);

    const std::string& getType() const { return m_type; }
    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::filesystem::path& getFile() const { return m_file; }
    int getWordCount() const { return m_wordCount; }

    void setTitle(const std::string& title);
    void setFile(const std::filesystem::path& file);
    void setWordCount(int count);
    void setMetadata(const std::string& key, const std::string& value);
    std::optional<std::string> getMetadata(const std::string& key) const;

    nlohmann::json toJson() const;
    static BookElement fromJson(const nlohmann::json& j);

private:
    std::string m_type;           // "chapter", "title_page", etc.
    std::string m_id;             // UUID
    std::string m_title;          // Display title
    std::filesystem::path m_file; // Relative path to RTF
    int m_wordCount = 0;
    std::chrono::system_clock::time_point m_created;
    std::chrono::system_clock::time_point m_modified;
    std::map<std::string, std::string> m_metadata;
};
```

### Part
```cpp
class Part {
public:
    Part(std::string id, std::string title);

    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::vector<std::shared_ptr<BookElement>>& getChapters() const { return m_chapters; }

    void setTitle(const std::string& title);
    void addChapter(std::shared_ptr<BookElement> chapter);
    void removeChapter(const std::string& chapterId);
    std::shared_ptr<BookElement> getChapter(const std::string& chapterId) const;

    int getWordCount() const;  // Sum of all chapters

    nlohmann::json toJson() const;
    static Part fromJson(const nlohmann::json& j);

private:
    std::string m_id;
    std::string m_title;
    std::vector<std::shared_ptr<BookElement>> m_chapters;
};
```

### Book
```cpp
class Book {
public:
    Book() = default;

    std::vector<std::shared_ptr<BookElement>>& getFrontMatter() { return m_frontMatter; }
    std::vector<std::shared_ptr<Part>>& getBody() { return m_body; }
    std::vector<std::shared_ptr<BookElement>>& getBackMatter() { return m_backMatter; }

    void addFrontMatter(std::shared_ptr<BookElement> element);
    void addPart(std::shared_ptr<Part> part);
    void addBackMatter(std::shared_ptr<BookElement> element);

    int getWordCount() const;  // Sum body only (not front/back matter)

    nlohmann::json toJson() const;
    static Book fromJson(const nlohmann::json& j);

private:
    std::vector<std::shared_ptr<BookElement>> m_frontMatter;
    std::vector<std::shared_ptr<Part>> m_body;
    std::vector<std::shared_ptr<BookElement>> m_backMatter;
};
```

### Document
```cpp
class Document {
public:
    Document(std::string title, std::string author, std::string language = "en");

    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::string& getAuthor() const { return m_author; }
    const std::string& getLanguage() const { return m_language; }

    Book& getBook() { return m_book; }
    const Book& getBook() const { return m_book; }

    void setTitle(const std::string& title);
    void setAuthor(const std::string& author);
    void setLanguage(const std::string& language);

    bool save(const std::filesystem::path& path);
    static std::optional<Document> load(const std::filesystem::path& path);

    nlohmann::json toJson() const;
    static Document fromJson(const nlohmann::json& j);

private:
    std::string m_id;       // UUID
    std::string m_title;    // Project title
    std::string m_author;   // Author name
    std::string m_language; // ISO 639-1 (en, pl, etc.)
    Book m_book;            // The book structure
    std::chrono::system_clock::time_point m_created;
    std::chrono::system_clock::time_point m_modified;
};
```

### DocumentArchive (RAII ZIP handler)
```cpp
class DocumentArchive {
public:
    static bool save(const Document& doc, const std::filesystem::path& klhPath);
    static std::optional<Document> load(const std::filesystem::path& klhPath);

private:
    static bool writeManifest(zip_t* archive, const nlohmann::json& manifest);
    static bool writeRTFFiles(zip_t* archive, const Document& doc);
    static std::optional<nlohmann::json> readManifest(zip_t* archive);
};
```
