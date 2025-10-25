# Development Roadmap

> 18-month journey from foundation to release

**Status:** ✅ Finalized
**Last Updated:** 2025-10-24
**Target Release:** Q2-Q3 2026 (Kalahari 1.0)

---

## Overview

**Total Timeline:** 14-20 months (realistically **18 months**)

**6 Development Phases:**
- **Phase 0:** Foundation (2-3 months)
- **Phase 1:** Core Editor (3-4 months)
- **Phase 2:** Plugin System MVP (2-3 months)
- **Phase 3:** Feature Plugins (3-4 months)
- **Phase 4:** Advanced Plugins (2-3 months)
- **Phase 5:** Polish & Release (2-3 months)

**Strategy:** Build infrastructure first, features second, polish third

---

## Phase 0: Foundation

**Duration:** Weeks 1-8 (2-3 months)
**Focus:** Build infrastructure & plugin system
**Deliverable:** Technical foundation working, plugin system functional

### Core Infrastructure

**CMake + vcpkg Setup:**
- Configure build system for all platforms
- Set up vcpkg manifest (vcpkg.json)
- Integrate with GitHub Actions for CI/CD
- Test builds on Windows, macOS, Linux

**wxWidgets Basic Application:**
- Hello World wxWidgets app
- Main window with menu bar
- Toolbar (structure only)
- Status bar with basic fields
- Window icon, sizing, positioning

**Settings System:**
- JSON-based settings (nlohmann_json)
- Persistent storage (user preferences)
- Settings dialog (basic UI)
- Default settings on first run

**Logging System:**
- spdlog integration
- Log to console + file (rotating logs)
- Log levels (debug, info, warn, error)
- Structured logging (context fields)

### Plugin Architecture

**Python 3.11 Embedding:**
- Embed Python 3.11 in C++ app (pybind11)
- Python interpreter initialization
- Expose C++ API to Python
- Error handling (Python exceptions → C++)

**Plugin Manager:**
- Plugin discovery (scan plugins/ folder)
- Plugin loading/unloading (dynamic)
- Plugin lifecycle (init, activate, deactivate, destroy)
- Plugin dependency resolution

**Extension Points System:**
- Define core interfaces (IExporter, IImporter, IAssistant, etc.)
- Register extension points (C++ → Python bindings)
- Plugin registration mechanism
- Query plugins by extension point

**Event Bus:**
- Async event system (thread-safe)
- Event types (DocumentChanged, ChapterAdded, etc.)
- Subscribe/emit API
- Thread marshalling (background → GUI thread)
- Priority queue

**Plugin Format:**
- .kplugin ZIP handler (libzip)
- plugin.json manifest parsing
- Extract .kplugin to plugins/ directory
- Validate plugin structure

### Document Model

**Core C++ Classes:**
```cpp
class Document;
class Chapter;
class Book;
class Project;
```

- Define data structures
- Implement CRUD operations
- In-memory representation

**JSON Serialization:**
- Document ↔ JSON (nlohmann_json)
- Chapter ↔ JSON
- Validation (schema checking)

**.klh File Format:**
- ZIP container (libzip)
- manifest.json, book.json structure
- Save/Load basic implementation
- Error handling (corrupted files)

### Testing Foundation

**Catch2 Setup:**
- Integrate Catch2 v3
- Unit test framework
- Test runner (CTest)
- Sample tests for Document model

**CI/CD Pipeline:**
- GitHub Actions workflow
- Matrix builds (Windows, macOS, Linux)
- Run tests on every commit
- Build artifacts

### Deliverable Checkpoint

- ✅ Builds on all 3 platforms
- ✅ Python embedding works
- ✅ Can load simple "hello world" Python plugin
- ✅ Event bus functional
- ✅ Document model serializes to JSON
- ✅ Basic tests passing

**Risk Assessment:** Medium (new architecture, learning curve)

---

## Phase 1: Core Editor

**Duration:** Weeks 9-20 (3-4 months)
**Focus:** Rich text editor & project management
**Deliverable:** Functional rich text editor with project management

### Rich Text Editor

**wxRichTextCtrl Integration:**
- Integrate wxRichTextCtrl widget
- Customize appearance (fonts, colors)
- Handle keyboard/mouse events

**Basic Formatting:**
- Bold, italic, underline
- Font size, font family
- Text color, background color
- Alignment (left, center, right, justify)

**Text Styles:**
- Heading 1, 2, 3
- Body text, quotes, code blocks
- Style picker UI (dropdown/toolbar)

**Undo/Redo:**
- Command pattern implementation
- Undo stack (configurable depth)
- Redo after undo
- Keyboard shortcuts (Ctrl+Z, Ctrl+Y)

**Live Word Count:**
- Word count on text change
- Display in status bar
- Character count, line count
- Session word count

### Project Management

**Project Navigator Panel:**
- wxTreeCtrl (tree view)
- Display book structure (parts → chapters → scenes)
- Icons for different node types
- Context menu (right-click)

**Chapter Management:**
- Add chapter (dialog for title)
- Delete chapter (with confirmation)
- Rename chapter (in-place or dialog)
- Move chapter (drag & drop or up/down buttons)
- Chapter metadata (created, modified, word count)

**Save/Load (.klh files):**
- Save Project → .klh file (ZIP + JSON)
- Load .klh → Project (parse JSON, extract chapters)
- Save progress indicator (for large projects)
- Load error handling (invalid format, missing files)

**Auto-save System:**
- Timer-based auto-save (every N minutes, configurable)
- Save to temp location first (atomic write)
- Only save if changes detected
- User notification (status bar: "Auto-saved at HH:MM")

**Backup System:**
- Snapshots on every manual save
- Keep last N backups (configurable, default 5)
- Backup file naming: `project-YYYY-MM-DD-HHMMSS.klh.bak`
- Restore from backup UI (File → Restore Backup)

### UI Enhancements

**wxAUI Docking System:**
- Configure 6 panels:
  - Project Navigator (left, 250px width)
  - Editor (center, takes remaining space)
  - Preview/Inspector (right, 300px)
  - Assistant (bottom-right, floating, 300x400)
  - Statistics (bottom, 200px height)
  - Output/Log (bottom, hidden by default)
- Dockable, resizable, can float
- Perspective system (save/load layouts)

**3 Focus Modes:**
- **Normal:** All panels visible
- **Focused:** Only editor + project navigator
- **Distraction-free:** Only editor, fullscreen

**Perspectives:**
- Default (all panels)
- Writer (editor + project nav + assistant)
- Editor (editor + project nav + stats)
- Research (all panels visible)
- Save/load perspective via View menu

**Toolbar Actions:**
- New project, Open, Save, Save As
- Undo, Redo
- Bold, Italic, Underline
- Styles dropdown
- Focus mode toggle

### Deliverable Checkpoint

- ✅ Can create new book project
- ✅ Can add/delete/rename chapters
- ✅ Can type formatted text in editor
- ✅ Undo/redo works
- ✅ Can save project to .klh
- ✅ Can load project from .klh
- ✅ Auto-save functional
- ✅ Backups created on save

**Risk Assessment:** Low (well-understood wxWidgets APIs)

---

## Phase 2: Plugin System MVP

**Duration:** Weeks 21-30 (2-3 months)
**Focus:** 4 working plugins (prove plugin system)
**Deliverable:** Plugin system proven with 4 working plugins

### Plugin 1: DOCX Exporter

**Features:**
- Export Document → .docx file
- Basic formatting preservation (bold, italic, headings)
- Chapter as headings (H1)
- Sections as H2
- Use python-docx library

**Implementation:**
```python
class DOCXExporter(IExporter):
    def export(self, doc: Document, path: str) -> bool:
        from docx import Document as DocxDocument
        docx = DocxDocument()
        for chapter in doc.chapters:
            docx.add_heading(chapter.title, level=1)
            docx.add_paragraph(chapter.content)
        docx.save(path)
        return True
```

**UI:**
- File → Export → DOCX
- File picker dialog

### Plugin 2: Markdown Tools

**Features:**
- Import .md → Document (parse Markdown)
- Export Document → .md (convert to Markdown)
- Markdown preview (HTML rendering)

**Implementation:**
- Use `markdown` library (Python)
- Parse headings → chapters
- Handle bold, italic, lists

**UI:**
- File → Import → Markdown
- File → Export → Markdown
- View → Markdown Preview (toggle)

### Plugin 3: Basic Statistics

**Features:**
- Word count, character count
- Reading time estimation (250 words/min)
- Session statistics (words this session, time elapsed)
- Simple charts (matplotlib bar chart)

**Implementation:**
```python
class BasicStatistics(IPanel):
    def analyze(self, doc: Document):
        total_words = sum(ch.word_count for ch in doc.chapters)
        reading_time = total_words / 250  # minutes
        self.display_stats(total_words, reading_time)
        self.draw_chart()  # matplotlib chart
```

**UI:**
- Statistics panel (docked at bottom)
- Live updates on document change
- Charts: words per chapter (bar chart)

### Plugin 4: Assistant Lion

**Features:**
- Graphical assistant panel
- Lion animal (6 moods: neutral, happy, proud, worried, thinking, sleeping)
- Speech bubbles with messages
- Basic triggers:
  - Break reminder (every 90 minutes)
  - Goal reached (daily word count)
  - Encouragement (random, low frequency)

**Implementation:**
```python
class AssistantLion(IAssistant):
    def __init__(self):
        self.animal = "lion"
        self.moods = ["neutral", "happy", "proud", "worried", "thinking", "sleeping"]

    def on_event(self, event: Event):
        if event.type == EventType.USER_IDLE_90MIN:
            self.show_message("Even a king needs rest. Stand up and stretch.", "worried")
        elif event.type == EventType.DAILY_GOAL_REACHED:
            self.show_message("Completed your goal. Well done!", "proud")
```

**UI:**
- Assistant panel (docked bottom-right or floating)
- Lion head image (changes based on mood)
- Speech bubble (HTML, fades after 10 seconds)

### Plugin Management UI

**Features:**
- Plugin list (enabled/disabled plugins)
- Enable/disable plugin (checkbox)
- Configure plugin (settings button → plugin settings dialog)
- Install plugin (.kplugin drag & drop or file picker)
- Uninstall plugin (with confirmation)

**UI:**
- Tools → Plugin Manager
- List view: [Icon] Plugin Name | Version | Status | [Settings] [Disable/Enable]
- Install button → file picker for .kplugin

### Deliverable Checkpoint

- ✅ All 4 plugins working
- ✅ Can install .kplugin file
- ✅ Can enable/disable plugins
- ✅ Plugins communicate via event bus
- ✅ No crashes when plugin errors occur

**Risk Assessment:** Medium (new code, Python/C++ interop)

---

## Phase 3: Feature Plugins

**Duration:** Weeks 31-44 (3-4 months)
**Focus:** Advanced plugins (some premium)
**Deliverable:** Rich plugin ecosystem, premium plugins working

### Free Plugins

**PDF Exporter:**
- Export to PDF (reportlab)
- Basic formatting (fonts, bold, italic)
- Table of contents (bookmarks)

**TXT/RTF Import/Export:**
- Plain text import/export
- RTF import (parse RTF)
- RTF export (native format)

**Spell Checker:**
- Hunspell integration
- Underline misspelled words (red squiggly)
- Right-click suggestions
- Custom dictionary (user words)

**Themes:**
- Dark theme
- Savanna theme (warm colors)
- Midnight theme (high contrast)
- Theme switcher UI (View → Themes)

### Premium Plugin: AI Assistant Pro

**Price:** $19-29

**Features:**
- 4 animals in MVP (Lion, Meerkat, Elephant, Cheetah)
- Advanced personality system (different messages, tones)
- Flow state detection (analyze typing speed, don't interrupt)
- Smart triggers (context-aware, e.g., "stuck on chapter 3 for 2 hours")
- AI-powered suggestions (OpenAI/Claude API integration)

**Implementation:**
- Personality JSON files (messages per animal)
- Flow detection algorithm (typing speed + pause analysis)
- API integration (OpenAI/Claude)

### Premium Plugin: Advanced Analytics

**Price:** $14-19

**Features:**
- Timeline visualization (plotly interactive chart)
- Character mention tracking (who appears where)
- Pacing analysis (% action, dialogue, description per chapter)
- Reading level (Flesch-Kincaid score)
- Sentiment analysis (per chapter, trend line)
- Productivity trends (words/day over time, chart)

**Implementation:**
- Use NLP libraries (spaCy for character extraction)
- matplotlib/plotly for charts
- Store analytics data in JSON

### License Verification

**For premium plugins:**
- License key system (local validation)
- License stored in settings (encrypted)
- Graceful degradation (trial mode if no license)

### Deliverable Checkpoint

- ✅ 4 free plugins + 2 premium plugins working
- ✅ Premium plugins verify license
- ✅ UI polished, themes working
- ✅ Performance acceptable (no lag with plugins)

**Risk Assessment:** Low (building on proven plugin system)

---

## Phase 4: Advanced Plugins

**Duration:** Weeks 45-56 (2-3 months)
**Focus:** Professional features
**Deliverable:** Professional writer's toolkit complete

### Premium Plugin: Export Suite

**Price:** $24-34

**Features:**
- EPUB (ebooklib) - e-book publishing ready
- Advanced DOCX (templates, styles, publisher-ready)
- HTML export (website-ready, custom CSS)
- Export templates (Kindle Direct Publishing, IngramSpark formats)

### Premium Plugin: Research Pro

**Price:** $19-24

**Features:**
- OCR for scanned documents (pytesseract + Tesseract)
- Source management (organize PDFs, images, notes)
- Citation tools (footnotes, bibliography)
- Web clipping (save web pages for research)

### Free Features (Core/Plugins)

**Character Bank:**
- Character cards (name, photo, traits, backstory)
- CRUD operations (create, edit, delete)
- Display in panel (right sidebar)

**Location Bank:**
- Location cards (name, map/image, description)
- Similar to character bank

**Notes System:**
- "Yellow sticky" notes
- Attach to chapters or global
- Display in panel

**Writer's Calendar:**
- Set daily/weekly goals
- Track deadlines (manuscript due date)
- Visualize progress (calendar heatmap)

### Deliverable Checkpoint

- ✅ 3 premium plugins (5 total)
- ✅ Character/location banks functional
- ✅ Notes system working
- ✅ Calendar with goals

**Risk Assessment:** Low (incremental features)

---

## Phase 5: Polish & Release

**Duration:** Weeks 57-68 (2-3 months)
**Focus:** Testing, documentation, packaging
**Deliverable:** Kalahari 1.0 - Public Release 🎉

### Testing Marathon

**Unit Tests:**
- Catch2 tests for core (Document, Chapter, Project)
- Target: 70%+ coverage
- Fix failing tests

**Integration Tests:**
- Plugin loading/unloading
- Event bus under load
- File I/O (.klh, .kplugin)

**Beta Testing:**
- Recruit 20-30 beta testers
- Beta announcement (Reddit, writing forums)
- Collect feedback (Google Form, GitHub Issues)
- Prioritize & fix bugs

**Bug Fixing:**
- Triage bugs (critical → high → medium → low)
- Fix critical/high bugs
- Document known issues (KNOWN_ISSUES.md)

### Documentation

**User Manual:**
- Getting Started guide
- Feature walkthroughs (with screenshots)
- FAQ
- Troubleshooting
- English + Polish (MVP requirement)

**Plugin API Docs:**
- For plugin developers
- API reference (classes, methods, events)
- Tutorials (create your first plugin)
- Examples (sample plugins in repo)

**Video Tutorials:**
- 5-10 minute screencasts
- YouTube channel
- Topics: First project, using assistant, exporting, plugins

### Packaging & Installers

**Windows (NSIS):**
- Create installer (setup.exe)
- Bundle embedded Python
- Bundle plugins (4 free official plugins)
- Desktop shortcut, Start menu entry
- Uninstaller
- Code signing (Authenticode certificate)

**macOS (DMG):**
- Create disk image (.dmg)
- Universal Binary (Intel + Apple Silicon)
- Bundle embedded Python
- Application bundle (.app)
- Drag-to-Applications UX
- Code signing (Apple Developer certificate)
- Notarization (Apple)

**Linux (AppImage):**
- Create AppImage (universal, no install needed)
- Bundle all dependencies
- Test on Ubuntu 22.04, Fedora 38
- Alternative: Create .deb, .rpm (optional, community)

### Launch Preparation

**Website:**
- kalahari.app domain
- Landing page (product overview, screenshots, download)
- Documentation site (user manual, API docs)
- Blog (announcement posts)

**GitHub Release:**
- Tag v1.0.0
- Release notes (full changelog)
- Binaries attached (Windows, macOS, Linux)
- Source tarball

**Social Media & Marketing:**
- Reddit posts (r/writing, r/selfpublish, r/opensource)
- Hacker News submission
- ProductHunt launch
- Twitter/X announcement
- Writing community forums

**Community Setup:**
- GitHub Discussions enabled
- Discord server (optional)
- Community guidelines (CODE_OF_CONDUCT.md)

### Deliverable: Kalahari 1.0 🎉

- ✅ Stable release on GitHub
- ✅ Installers for all 3 platforms
- ✅ Documentation (EN + PL)
- ✅ Website live
- ✅ Beta testers happy
- ✅ Zero critical bugs

**Risk Assessment:** Low (mostly logistics)

---

## Post-1.0: Expansion

### Phase 6: Cloud Sync (3-6 months post-1.0)

**Focus:** Cloud Sync Pro subscription service

**Features:**
- Cloud synchronization (Dropbox/GDrive integration)
- Unlimited cloud storage
- Mobile companion apps (iOS/Android - initial version)
- End-to-end encryption
- Premium support (24h email response)

**Business:**
- Subscription: $5-10/month or $50-100/year
- Backend infrastructure (AWS/GCP)
- Payment processing (Stripe)

### Phase 7: Collaboration (6-12 months post-1.0)

**Focus:** Collaboration Pack premium plugin

**Features:**
- Beta-reader mode (commenting without editing)
- Editor mode (track changes, accept/reject)
- Version comparison (git-like diffs)
- Shared notes & annotations
- Real-time writing sprints (online co-writing)

**Price:** $29-39 one-time

### Phase 8: Ecosystem (12-18 months post-1.0)

**Focus:** Expand the Kalahari ecosystem

**Projects:**
- **Plugin Marketplace** - Own platform (kalahari.app/marketplace)
- **Serengeti** - Standalone collaborative writing tool
- **Okavango** - Research & knowledge management app
- **Victoria** - Advanced cloud sync features
- **Zambezi** - Publishing toolkit (formatting, distribution)

**Template Marketplace:**
- Book templates (novel structure, non-fiction outlines)
- Export templates (publisher-specific)
- Revenue share (70/30 with creators)

---

## Milestones Summary

| Phase | Duration | Deliverable | Target Date |
|-------|----------|-------------|-------------|
| **Phase 0** | Weeks 1-8 | Technical foundation | Month 3 |
| **Phase 1** | Weeks 9-20 | Core editor | Month 7 |
| **Phase 2** | Weeks 21-30 | Plugin system MVP | Month 10 |
| **Phase 3** | Weeks 31-44 | Feature plugins | Month 14 |
| **Phase 4** | Weeks 45-56 | Advanced plugins | Month 17 |
| **Phase 5** | Weeks 57-68 | **1.0 Release** 🎉 | **Month 18** |
| **Phase 6** | Post-1.0 (+3-6mo) | Cloud Sync Pro | Month 24 |
| **Phase 7** | Post-1.0 (+6-12mo) | Collaboration | Month 30 |
| **Phase 8** | Post-1.0 (+12-18mo) | Ecosystem | Month 36 |

---

## Risk Management

### High Risk Items

**Python/C++ Integration:**
- **Risk:** Crashes, memory leaks, performance issues
- **Mitigation:** Extensive testing, error handling, memory profiling
- **Fallback:** Sandbox plugins (separate processes)

**Cross-Platform Consistency:**
- **Risk:** Bugs on one platform, differences in UX
- **Mitigation:** CI/CD testing on all platforms, early beta testing
- **Fallback:** Delay platform if issues found, release for stable platforms first

### Medium Risk Items

**Plugin Marketplace (Own Platform):**
- **Risk:** Development time, infrastructure costs
- **Mitigation:** Start with GitHub, delay marketplace to post-1.0
- **Fallback:** Keep GitHub-based distribution

**AI API Costs:**
- **Risk:** OpenAI/Claude API expensive for users
- **Mitigation:** User provides own API key (no cost to project)
- **Fallback:** Local models (Ollama) as alternative

### Low Risk Items

**wxWidgets Integration:**
- **Risk:** Learning curve, documentation gaps
- **Mitigation:** Well-documented library, large community
- **Fallback:** None needed (proven technology)

---

## Success Metrics

### Phase 0-2 (Foundation)
- ✅ Builds on all platforms without errors
- ✅ Plugin system loads/unloads plugins reliably
- ✅ No memory leaks (Valgrind, sanitizers)

### Phase 3-4 (Features)
- ✅ Premium plugins functional, license system works
- ✅ Performance: Large documents (100k words) handle smoothly
- ✅ Stability: No crashes in 8-hour writing sessions

### Phase 5 (Release)
- ✅ 20+ beta testers report "ready for release"
- ✅ Zero critical bugs, <5 high-priority bugs
- ✅ 1,000+ downloads in first month
- ✅ 4.5+ star average (user reviews)

---

## Next Steps

- **[07_mvp_tasks.md](07_mvp_tasks.md)** - Detailed week-by-week tasks
- **[03_architecture.md](03_architecture.md)** - System design
- **[04_plugin_system.md](04_plugin_system.md)** - Plugin API specification

---

**Version:** 1.0
**Status:** ✅ Finalized
**Last Updated:** 2025-10-24
