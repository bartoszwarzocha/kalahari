# Task #00016: Complete Menu Design (Phase 0-5)

**Date:** 2025-11-21
**Status:** 🟡 AWAITING REVIEW (redesigned based on roadmap analysis)
**Type:** Design / Planning
**Estimated Time:** 120 minutes
**Phase:** 0 (Foundation)

---

## 🎯 Objective

Design comprehensive menu structure for Kalahari Writer's IDE by **deducing requirements from ROADMAP.md and project_docs/**, covering all phases (0-5), following user's architectural guidance.

**Key Design Principle (from user):**
> "Eksport powinien znaleźć się w menu File, z zastrzeżeniem, że menu eksport powinno być dynamicznie rozbudowywane przez pluginy. Czyli np. kilka formatów stałych, separator i wtedy dodatkowe formaty z pluginów."

---

## 📚 Analysis Sources

Analyzed the following documents to deduce menu requirements:
- **ROADMAP.md** - Feature roadmap across 5 phases
- **project_docs/01_overview.md** - Vision: "Writer's IDE", not just word processor
- **project_docs/03_architecture.md** - MVP pattern, Command Registry, plugin architecture
- **project_docs/04_plugin_system.md** - Extension Points (IExporter, IPanelProvider, ICommandProvider, IAssistant)
- **project_docs/05_business_model.md** - Free vs Premium feature tiers
- **project_docs/08_gui_design.md** - Panel catalog, perspectives, focus modes
- **Serena memory:** `kalahari_project_vs_document_architecture.md` - PROJECT-first paradigm

---

## 🧠 Key Insights from Analysis

### 1. Kalahari's Unique Identity
- **"Writer's IDE"** - Not Scrivener clone, not Word clone, not generic editor
- **Components:** Editor + Project Management + Research + AI Assistant + Analytics + Export + Plugins
- **Philosophy:** "Everything in one place, but not in your face"
- **Writer-centric language:** "Chapter" not "File", "Book" not "Project"

### 2. PROJECT-First Architecture (Critical!)
**From Serena memory:**
```
HIERARCHY:
Project (Book .klh file)
  ├─ Structure (Front Matter, Body, Back Matter → Parts → Chapters)
  ├─ Objects (Characters, Locations, Events, Notes, Mind Maps)
  ├─ Research (PDF references, web clips, annotations)
  └─ Metadata (Author, Genre, Language, Statistics)
```

**4 Operational Modes:**
1. **New Project** - Wizard for book creation (title, genre, structure)
2. **Open Project** - Shows summary (chapters, pages, stats, last session)
3. **New Object** - Create in-project objects (Chapter, Character, Location, etc.)
4. **Import File** - Add external reference (PDF, DOCX)

### 3. Plugin Extensibility Pattern
**From project_docs/04_plugin_system.md:**
- **Extension Points:** IExporter, IPanelProvider, ICommandProvider, IAssistant
- **Dynamic Menu Building:** CommandRegistry + MenuBuilder
- **Pattern:** Built-in items + Separator + Plugin-added items

**Example (from user):**
```
Export →
  DOCX (built-in)
  PDF (built-in)
  ───────────────
  EPUB (Export Suite plugin)
  LaTeX (Export Suite plugin)
```

### 4. Free vs Premium Features
**From project_docs/05_business_model.md:**

**FREE Tier:**
- Core editor, basic statistics
- DOCX, Markdown, PDF export
- Lion Assistant (grammar/style)

**PREMIUM Plugins ($79 bundle):**
- **AI Assistant Pro** ($19) - 3 additional animals
- **Analytics** ($14) - Advanced writing metrics
- **Export Suite** ($29) - EPUB, MOBI, InDesign, LaTeX
- **Research Pro** ($39) - Character/Location banks, Timeline, Mind Map
- **Collaboration** ($29) - Real-time co-editing, comments

**CLOUD SaaS** ($5-10/month):
- Cross-device sync
- Cloud backups
- Web access

### 5. Phase-Based Implementation
**From ROADMAP.md:**
- **Phase 0 (COMPLETE):** Basic editor, panels, command registry
- **Phase 1 (Current):** Rich text, project management, search, .klh format
- **Phase 2:** Plugin System MVP, 4 AI assistants (Lion, Meerkat, Elephant, Cheetah)
- **Phase 3:** Premium plugins, marketplace
- **Phase 4:** Research tools, collaboration, cloud sync
- **Phase 5:** Polish, i18n, packaging

---

## 📝 Redesigned Menu Structure

### Design Decisions:
1. **8 menus** (not 15!) - Simpler, more focused
2. **Export in FILE menu** (per user's example) - Dynamic plugin extension
3. **No separate menus** for Characters/Locations/Timeline - They are **panels**, not menus
4. **TOOLS menu** for utilities - Statistics, Focus modes, Plugins
5. **ASSISTANT menu** for AI - Dynamic, plugin-extensible (4 free + 4 premium)
6. **Writer-centric language** - "Book" not "Project", "Chapter" not "Document"

---

## 🗂️ 1. FILE Menu (PROJECT Operations + Export)

**Philosophy:** PROJECT is the base unit. Everything starts with Book/Project.

```
FILE
├─ New Book...                    (Ctrl+Shift+N)  [Phase 1: Wizard]
├─ Open Book...                   (Ctrl+Shift+O)  [Phase 1: .klh file]
├─ Recent Books →                                  [Phase 1: Last 10]
│  ├─ My Novel.klh
│  ├─ Thesis Draft.klh
│  └─ Clear Recent
├─ Close Book                     (Ctrl+Shift+W)  [Phase 1]
├─ ─────────────────────────
├─ Save                           (Ctrl+S)        [Phase 0: ✅ Works]
├─ Save As...                     (Ctrl+Shift+S)  [Phase 1]
├─ Save All                                        [Phase 1: All open tabs]
├─ ─────────────────────────
├─ Import →                                        [Phase 4]
│  ├─ DOCX Document...
│  ├─ PDF Reference...
│  ├─ Plain Text...
│  └─ Scrivener Project...
├─ Export →                                        [Phase 2+: Plugin-extensible!]
│  ├─ DOCX                        (Ctrl+Shift+E)  [Phase 2: Free]
│  ├─ PDF                                          [Phase 2: Free]
│  ├─ Markdown                                     [Phase 2: Free]
│  ├─ ─────────────────────────   [Separator]
│  ├─ [Plugin formats below...]                    [Phase 3+: Premium]
│  │  ├─ EPUB (Export Suite)
│  │  ├─ MOBI (Export Suite)
│  │  ├─ InDesign ICML (Export Suite)
│  │  └─ LaTeX (Export Suite)
│  └─ Export Settings...                           [Phase 3: Templates]
├─ ─────────────────────────
├─ Book Properties...                              [Phase 1: Metadata editor]
├─ ─────────────────────────
├─ Exit                           (Alt+F4)        [Phase 0: ✅ Works]
```

**Phase 0 Status:**
- ⚠️ **File > New/Open** - Currently work on documents (WRONG! Should be removed or disabled)
- ✅ **File > Save/Save As** - Work, but context unclear
- ✅ **File > Exit** - Works

**Phase 1 Corrections:**
- Remove/disable "New" and "Open" (they violate PROJECT-first paradigm)
- Add "New Book..." and "Open Book..." with proper wizards
- Rename "Save" → context remains "Save" but operates on current project

---

## ✏️ 2. EDIT Menu (Standard Operations)

```
EDIT
├─ Undo                           (Ctrl+Z)        [Phase 0: ✅ Works]
├─ Redo                           (Ctrl+Y)        [Phase 0: ✅ Works]
├─ ─────────────────────────
├─ Cut                            (Ctrl+X)        [Phase 0: ✅ Works]
├─ Copy                           (Ctrl+C)        [Phase 0: ✅ Works]
├─ Paste                          (Ctrl+V)        [Phase 0: ✅ Works]
├─ Paste Special...                                [Phase 1: Unformatted]
├─ Delete                         (Del)           [Phase 0: ✅ Works]
├─ ─────────────────────────
├─ Select All                     (Ctrl+A)        [Phase 0: ✅ Works]
├─ Select Word                    (Ctrl+D)        [Phase 1]
├─ Select Paragraph                                [Phase 1]
├─ ─────────────────────────
├─ Find...                        (Ctrl+F)        [Phase 1: Search panel]
├─ Find Next                      (F3)            [Phase 1]
├─ Find Previous                  (Shift+F3)      [Phase 1]
├─ Replace...                     (Ctrl+H)        [Phase 1]
├─ Find in Book...                (Ctrl+Shift+F)  [Phase 1: All chapters]
├─ ─────────────────────────
├─ Preferences...                 (Ctrl+,)        [Phase 0: ✅ Settings Dialog]
```

**Phase 0 Status:** All basic commands work ✅

---

## ➕ 3. INSERT Menu (Content Creation)

**Philosophy:** Create in-book objects (chapters, scenes, footnotes, etc.)

```
INSERT
├─ New Chapter...                 (Ctrl+Alt+C)    [Phase 1: In current Part]
├─ New Scene...                   (Ctrl+Alt+S)    [Phase 1: Scene break]
├─ Chapter Break                  (Ctrl+Enter)    [Phase 1]
├─ Scene Break                    (Ctrl+Shift+Enter) [Phase 1]
├─ ─────────────────────────
├─ Image...                                        [Phase 1: Inline image]
├─ Table...                                        [Phase 1: Insert table]
├─ Link...                        (Ctrl+K)        [Phase 1: Hyperlink]
├─ ─────────────────────────
├─ Footnote                       (Ctrl+Alt+F)    [Phase 1]
├─ Endnote                        (Ctrl+Alt+E)    [Phase 1]
├─ Comment                        (Ctrl+Alt+M)    [Phase 1: Inline comment]
├─ Annotation                                      [Phase 1: Margin note]
├─ ─────────────────────────
├─ Special Character...                            [Phase 1: Character map]
├─ Date & Time                                     [Phase 1: Current datetime]
├─ Field...                                        [Phase 1: Dynamic fields]
```

**Phase 0 Status:** No INSERT menu yet (Phase 1 feature)

---

## 🎨 4. FORMAT Menu (Rich Text Formatting)

```
FORMAT
├─ Font...                                         [Phase 1: Font dialog]
├─ Paragraph...                                    [Phase 1: Spacing, alignment]
├─ ─────────────────────────
├─ Text Style →                                    [Phase 1: Paragraph styles]
│  ├─ Heading 1               (Ctrl+1)
│  ├─ Heading 2               (Ctrl+2)
│  ├─ Heading 3               (Ctrl+3)
│  ├─ Body Text               (Ctrl+0)
│  ├─ Quote
│  └─ Code
├─ ─────────────────────────
├─ Bold                           (Ctrl+B)        [Phase 1]
├─ Italic                         (Ctrl+I)        [Phase 1]
├─ Underline                      (Ctrl+U)        [Phase 1]
├─ Strikethrough                                   [Phase 1]
├─ ─────────────────────────
├─ Align Left                     (Ctrl+L)        [Phase 1]
├─ Align Center                   (Ctrl+E)        [Phase 1]
├─ Align Right                    (Ctrl+R)        [Phase 1]
├─ Justify                        (Ctrl+J)        [Phase 1]
├─ ─────────────────────────
├─ Increase Indent                (Tab)           [Phase 1]
├─ Decrease Indent                (Shift+Tab)     [Phase 1]
├─ ─────────────────────────
├─ Bullets                                         [Phase 1]
├─ Numbering                                       [Phase 1]
├─ ─────────────────────────
├─ Clear Formatting               (Ctrl+Space)    [Phase 1: Remove all styles]
```

**Phase 0 Status:** No FORMAT menu yet (Phase 1 feature)

---

## 🔧 5. TOOLS Menu (Utilities + Plugins + Statistics)

**Philosophy:** All utilities, statistics, plugins, focus modes, backups

```
TOOLS
├─ Statistics →                                    [Phase 1: Free feature]
│  ├─ Current Chapter Stats                        [Word/char count, reading time]
│  ├─ Book Statistics...                           [Full project stats]
│  └─ Writing Session History                      [Daily progress]
├─ Advanced Analytics...                           [Phase 3: Analytics plugin $14]
├─ ─────────────────────────
├─ Spellchecker                   (F7)            [Phase 1]
├─ Grammar Check                                   [Phase 2: Lion Assistant]
├─ Readability Score                               [Phase 3: Analytics plugin]
├─ ─────────────────────────
├─ Focus Mode →                                    [Phase 1: Distraction-free]
│  ├─ Normal                  (Esc)               [Default view]
│  ├─ Focused                 (F11)               [Hide panels]
│  └─ Distraction-Free        (Ctrl+F11)          [Fullscreen, no UI]
├─ ─────────────────────────
├─ Backup Now                                      [Phase 1: Manual backup]
├─ Auto-Save Settings...                           [Phase 1: Interval config]
├─ Version History...                              [Phase 1: Snapshots]
├─ ─────────────────────────
├─ Plugins →                                       [Phase 2: Plugin management]
│  ├─ Plugin Manager...                            [Install/uninstall]
│  ├─ Browse Marketplace...                        [Phase 3: Plugin store]
│  ├─ Check for Updates...                         [Phase 3]
│  ├─ ─────────────────────────
│  ├─ [Plugin-added commands...]                   [Dynamic section]
│  └─ Reload Plugins                               [Dev mode]
├─ ─────────────────────────
├─ Challenges & Badges...                          [Phase 2: Gamification]
├─ Writing Goals...                                [Phase 1: Daily/weekly targets]
├─ ─────────────────────────
├─ Cloud Sync...                                   [Phase 4: Cloud SaaS $5-10/mo]
├─ Collaboration...                                [Phase 4: Collaboration plugin $29]
```

**Phase 0 Status:** No TOOLS menu yet (Phase 1+ feature)

---

## 🦁 6. ASSISTANT Menu (AI Assistants - Plugin-Extensible!)

**Philosophy:** 4 free AI assistants (Phase 2) + 4 premium (Phase 3). Dynamic, plugin-added.

```
ASSISTANT
├─ Ask Assistant...               (Ctrl+Shift+A)  [Phase 2: Quick query]
├─ ─────────────────────────
├─ Switch Assistant →                              [Phase 2: FREE animals]
│  ├─ 🦁 Lion (Grammar & Style)    (Ctrl+Alt+1)   [Phase 2: Free]
│  ├─ 🦝 Meerkat (Research Helper) (Ctrl+Alt+2)   [Phase 2: Free]
│  ├─ 🐘 Elephant (Plot Analysis)  (Ctrl+Alt+3)   [Phase 2: Free]
│  ├─ 🐆 Cheetah (Speed Writing)   (Ctrl+Alt+4)   [Phase 2: Free]
│  ├─ ─────────────────────────    [Separator]
│  ├─ [Premium assistants below...]                [Phase 3: AI Pro $19]
│  │  ├─ 🦊 Fox (Character Arc)
│  │  ├─ 🦉 Owl (World-Building)
│  │  ├─ 🐆 Leopard (Pacing)
│  │  └─ 🐃 Buffalo (Research Deep-Dive)
├─ ─────────────────────────
├─ Assistant Actions →                             [Phase 2: Context-aware]
│  ├─ Check Grammar                                [Lion: Current paragraph]
│  ├─ Improve Style                                [Lion: Suggestions]
│  ├─ Analyze Plot                                 [Elephant: Chapter arc]
│  ├─ Research Topic...                            [Meerkat: Web search]
│  └─ Speed Draft Mode                             [Cheetah: Dictation/flow]
├─ ─────────────────────────
├─ Assistant Settings...                           [Phase 2: Tone, verbosity]
```

**Phase 0 Status:** No ASSISTANT menu yet (Phase 2 feature)

**Key Design:** This menu demonstrates plugin extensibility perfectly!
- 4 built-in free assistants (Phase 2)
- Separator
- 4 premium assistants added by AI Assistant Pro plugin (Phase 3)

---

## 👁️ 7. VIEW Menu (Panels + Perspectives + UI)

```
VIEW
├─ Panels →                                        [Phase 0+: Dockable panels]
│  ├─ Navigator                  (Ctrl+1)         [Phase 0: ✅ Works]
│  ├─ Properties                 (Ctrl+2)         [Phase 0: ✅ Works]
│  ├─ Statistics                 (Ctrl+3)         [Phase 1]
│  ├─ Assistant                  (Ctrl+4)         [Phase 2: ✅ Works (stub)]
│  ├─ Research                   (Ctrl+5)         [Phase 4: Research Pro $39]
│  ├─ Timeline                   (Ctrl+6)         [Phase 4: Research Pro $39]
│  ├─ Search                     (Ctrl+F)         [Phase 1: ✅ Works (stub)]
│  ├─ Log                        (Ctrl+L)         [Phase 0: ✅ Works]
│  ├─ ─────────────────────────
│  └─ [Plugin-added panels...]                     [Phase 2+: IPanelProvider]
├─ ─────────────────────────
├─ Perspectives →                                  [Phase 1: Workspace presets]
│  ├─ Writer                                       [Focused: Editor + Assistant]
│  ├─ Editor                                       [Analytical: Editor + Stats]
│  ├─ Researcher                                   [Research-heavy layout]
│  ├─ Planner                                      [Structure: Navigator + Timeline]
│  ├─ ─────────────────────────
│  ├─ Save Current Perspective...
│  └─ Manage Perspectives...
├─ ─────────────────────────
├─ Toolbars →                                      [Phase 1]
│  ├─ Standard Toolbar
│  ├─ Format Toolbar
│  ├─ Quick Access Toolbar
│  ├─ ─────────────────────────
│  └─ Customize Toolbars...
├─ ─────────────────────────
├─ Show Info Bar                                   [Phase 1: Top banner]
├─ Show Status Bar                                 [Phase 0: ✅ Works]
├─ Show Formatting Marks                           [Phase 1: ¶ ¬ →]
├─ ─────────────────────────
├─ Zoom In                        (Ctrl++)        [Phase 1]
├─ Zoom Out                       (Ctrl+-)        [Phase 1]
├─ Reset Zoom                     (Ctrl+0)        [Phase 1: 100%]
├─ ─────────────────────────
├─ Full Screen                    (F11)           [Phase 1]
├─ ─────────────────────────
├─ Reset Layout                   (Ctrl+Shift+R)  [Phase 0: ✅ Works]
```

**Phase 0 Status:**
- ✅ Panel toggles work (Navigator, Properties, Log, Search, Assistant)
- ✅ Reset Layout works
- ❌ Perspectives not implemented yet (Phase 1)

---

## ❓ 8. HELP Menu

```
HELP
├─ Kalahari Help                  (F1)            [Phase 5: User manual]
├─ Getting Started Tutorial                        [Phase 5: Interactive]
├─ Video Tutorials                                 [Phase 5: YouTube links]
├─ ─────────────────────────
├─ Keyboard Shortcuts             (Ctrl+/)        [Phase 1: Reference card]
├─ Tips & Tricks                                   [Phase 5]
├─ What's New                                      [Phase 3: Release notes]
├─ ─────────────────────────
├─ Report a Bug...                                 [Phase 3: GitHub issue]
├─ Suggest a Feature...                            [Phase 3: Feedback form]
├─ Community Forum                                 [Phase 3: Discussion link]
├─ ─────────────────────────
├─ Check for Updates...                            [Phase 3: Auto-updater]
├─ ─────────────────────────
├─ About Kalahari                                  [Phase 0: ✅ Works]
├─ About Qt                                        [Phase 0: ✅ Works]
└─ Licenses                                        [Phase 5: Third-party credits]
```

**Phase 0 Status:**
- ✅ About Kalahari works
- ✅ About Qt works
- ❌ Help documentation not implemented yet (Phase 5)

---

## 📊 Why This Design is Better

### 1. Follows User's Guidance
✅ **Export in FILE menu** (not separate EXPORT menu)
✅ **Plugin-extensible pattern** (built-in + separator + plugin items)
✅ **Deduced from roadmap** (not copied from competitors)

### 2. Simpler Structure
- **8 menus** (not 15!) - More focused, less overwhelming
- **No separate menus** for Characters/Locations/Timeline → They are **panels** (in VIEW menu)
- **TOOLS consolidates** utilities (statistics, plugins, focus modes, backups)

### 3. Writer-Centric Language
- "New Book" (not "New Project")
- "Chapter" / "Scene" (not "Document" / "Section")
- "Book Properties" (not "Project Settings")

### 4. Clear Free vs Premium Boundaries
- **FILE > Export →** DOCX/PDF/Markdown (free) + separator + EPUB/MOBI (premium)
- **ASSISTANT →** 4 free animals + separator + 4 premium animals
- **VIEW > Panels →** Basic panels (free) + separator + Research/Timeline (premium)

### 5. Phase-Based Growth
- **Phase 0:** FILE (basic), EDIT, VIEW, HELP ✅
- **Phase 1:** +INSERT, +FORMAT, enhanced FILE/VIEW
- **Phase 2:** +TOOLS (plugins), +ASSISTANT (4 animals)
- **Phase 3:** Premium features (Export Suite, AI Pro, Analytics)
- **Phase 4:** Research tools (Character Bank, Timeline, Cloud Sync)
- **Phase 5:** Polish (Help documentation, i18n)

### 6. Plugin Extensibility (3 Examples)
**Example 1: Export (IExporter extension point)**
```
FILE > Export →
  DOCX (core)
  PDF (core)
  ───────────────
  EPUB (Export Suite plugin)
  LaTeX (Export Suite plugin)
```

**Example 2: Assistants (IAssistant extension point)**
```
ASSISTANT > Switch Assistant →
  Lion (core)
  Meerkat (core)
  ───────────────
  Fox (AI Pro plugin)
  Owl (AI Pro plugin)
```

**Example 3: Panels (IPanelProvider extension point)**
```
VIEW > Panels →
  Navigator (core)
  Properties (core)
  ───────────────
  Custom Panel (third-party plugin)
```

---

## 📋 Implementation Status Matrix

### Phase 0 (COMPLETE ✅)
| Menu | Command | Status | Notes |
|------|---------|--------|-------|
| FILE | Save | ✅ Works | Context: current project |
| FILE | Save As | ✅ Works | |
| FILE | Exit | ✅ Works | |
| EDIT | Undo/Redo | ✅ Works | |
| EDIT | Cut/Copy/Paste | ✅ Works | |
| EDIT | Select All | ✅ Works | |
| EDIT | Preferences | ✅ Works | Settings Dialog |
| VIEW | Panel toggles | ✅ Works | Navigator, Properties, Log, Search, Assistant |
| VIEW | Reset Layout | ✅ Works | |
| HELP | About | ✅ Works | |
| HELP | About Qt | ✅ Works | |

**Phase 0 Issues to Fix:**
- ⚠️ **FILE > New/Open** - Should be removed or disabled (violate PROJECT-first paradigm)
- 🔜 Add "Coming in Phase 1" stubs for removed commands

### Phase 1 (IN PROGRESS)
| Feature | Status | Priority |
|---------|--------|----------|
| FILE > New Book | ❌ TODO | 🔴 CRITICAL |
| FILE > Open Book | ❌ TODO | 🔴 CRITICAL |
| FILE > Recent Books | ❌ TODO | 🟡 HIGH |
| INSERT menu (complete) | ❌ TODO | 🔴 CRITICAL |
| FORMAT menu (complete) | ❌ TODO | 🔴 CRITICAL |
| EDIT > Find/Replace | ❌ TODO | 🔴 CRITICAL |
| VIEW > Perspectives | ❌ TODO | 🟡 MEDIUM |
| VIEW > Zoom | ❌ TODO | 🟢 LOW |

### Phase 2 (Plugin System)
| Feature | Status | Priority |
|---------|--------|----------|
| TOOLS menu (complete) | ❌ TODO | 🔴 CRITICAL |
| ASSISTANT menu (complete) | ❌ TODO | 🔴 CRITICAL |
| 4 AI assistants (Lion, Meerkat, Elephant, Cheetah) | ❌ TODO | 🔴 CRITICAL |
| Plugin Manager | ❌ TODO | 🔴 CRITICAL |

### Phase 3-5 (Premium + Polish)
All premium features and polish (Help docs, i18n, etc.): ❌ TODO

---

## 🎯 Acceptance Criteria

**For this design task:**
- [x] Analyzed ROADMAP.md and project_docs/ thoroughly
- [x] Deduced menu structure from Kalahari's vision (not copied from competitors)
- [x] Followed user's Export example (plugin-extensible pattern)
- [x] Simplified from 15 → 8 menus
- [x] Documented all Phase 0-5 features
- [x] Clear Free vs Premium boundaries
- [ ] **User approval received** ⬅️ NEXT STEP

**Next steps after approval:**
1. User reviews and comments on this design
2. Incorporate user feedback
3. Create final approved version
4. Update ROADMAP.md with menu implementation tasks
5. Create Task #00017: Fix Phase 0 Menu Issues

---

## 🔗 Related Files

- `src/gui/main_window.cpp` - Menu creation (createMenus)
- `src/gui/command_registry.cpp` - Command registration (registerCommands)
- `src/gui/menu_builder.cpp` - Dynamic menu builder
- `include/kalahari/gui/command.h` - Command structure (IconSet, KeyboardShortcut)
- `.serena/memories/kalahari_project_vs_document_architecture.md` - PROJECT-first rules

---

## 📝 Notes for User Review

**Key questions for user:**
1. Is 8-menu structure better than 15-menu structure? (FILE, EDIT, INSERT, FORMAT, TOOLS, ASSISTANT, VIEW, HELP)
2. Is Export placement in FILE menu correct? (following your example)
3. Should Characters/Locations/Timeline be **panels** (not menus)?
4. Any missing commands that are critical for Kalahari's vision?
5. Are Free vs Premium boundaries clear in menu structure?

**Changes from rejected design:**
- ❌ Removed separate EXPORT menu → merged into FILE menu
- ❌ Removed separate PROJECT menu → merged into FILE menu
- ❌ Removed separate CHARACTERS/LOCATIONS/TIMELINE menus → they are panels (VIEW menu)
- ❌ Removed separate WINDOW menu → tab management in VIEW or keyboard shortcuts
- ❌ Removed separate RESEARCH menu → merged into TOOLS menu
- ✅ Added TOOLS menu → consolidates utilities, plugins, statistics
- ✅ Clearer plugin extensibility pattern (separator + dynamic section)
- ✅ Simpler, more focused structure (8 menus vs 15)

---

**End of Task #00016 - Redesigned Menu Structure**
**Status:** 🟡 AWAITING USER REVIEW AND FEEDBACK
