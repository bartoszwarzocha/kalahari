# Kalahari Development Roadmap

> **Writer's IDE** – C++20 + Qt6 desktop application, free and open source (MIT)

**Current Phase:** Phase 1 (Core Editor) – in progress, about half done
**Version:** 0.3.2-alpha
**Next Release:** 0.4.0-alpha (Phase 1 complete)
**Last Updated:** 2026-10-03

This roadmap was rewritten on 2026-10-03 after a review of the actual state of the code.
The previous version overstated progress in some areas (the editor, the menus) and
understated it in others (Find & Replace). Each item below reflects what the code does
today, not what was planned.

---

## Principles

- **Free and open source.** Every component – the application, the plugin system and all
  plugins, present and future – is released under the MIT License. There are no paid
  plugins, marketplace, subscriptions or cloud services. Your work stays on your computer.
- **Working software over wide menus.** A menu command that is not implemented yet is shown
  disabled, with a tooltip saying so. It becomes active once its feature is wired.
- **Measured, not assumed.** Performance work is judged by benchmarks run in CI, and a
  stage is complete only when its tests pass and the application has been checked by hand.

---

## Completed

### Phase 0 – Qt Foundation (2025-11-19 – 2025-11-21)

Migration from wxWidgets to Qt6: main window, settings dialog, command registry, logging,
Python plugin runtime (pybind11), document model and the `.klh` project format.
The wxWidgets code is archived on the `wxwidgets-archive` branch.

### Phase 1 – completed parts

| Area | State |
|---|---|
| Menu structure and keyboard shortcuts (#00030) | Done; commands without a feature are disabled |
| Toolbars, customization and persistence (#00031, #00039) | Done |
| Project file system, export/import archive (#00033) | Done |
| Navigator panel (#00034, #00036) | Done |
| Theme and icon system, ArtProvider (#00027, #00032) | Done; editor colors not yet unified with ThemeManager |
| SQLite project database (#00041) | Done |
| Formatting round-trip in KML (#00044A) | Done |
| Custom text editor (#00042, #00043) | Works, but performance and rendering are not satisfactory – see Stage 1 |

---

## Phase 1 – remaining work (target: 0.4.0-alpha)

### Stage 0 – Foundation

No new features. Make the project measurable and its build reliable.

- [x] Fix macOS CI (runner pinned to `macos-15` with Xcode 16; Qt 6.9.1 does not build with newer Xcode)
- [x] Disable menu commands that have no implementation instead of leaving them silently empty
- [x] Remove marketplace, cloud sync and collaboration from menus and settings
- [x] Rewrite this roadmap and remove the paid-plugin business model from the documentation
- [ ] Register each test case separately in CTest (`catch_discover_tests`)
- [ ] Add a sanitizer job (ASan/UBSan) and coverage reporting to CI
- [ ] Use one minimum Qt version on all platforms and in CMake
- [ ] Run the editor benchmark (150k-word document) in CI with thresholds

### Stage 1 – Editor

Make the editor fast and correct on book-length documents. Detailed plan:
editor review, stages 0–6 (safety net and measurement, quick fixes, a single layout core
built on `QTextDocument`, incremental pagination and one zoom model, missing rendering
features, cleanup, wiring of unconnected features).

- [ ] Round-trip and layout tests for the real `fromKml → toKml` path
- [ ] Quick fixes: loading in one edit block, debounced resize and zoom, cached word count
- [ ] One layout core as the single source of geometry (lazy layout, estimated heights)
- [ ] Incremental pagination in page mode; one zoom model for all view modes
- [ ] Paragraph formatting (spacing, indents, line height, tabs) handled by the layout
- [ ] Spelling and grammar underlines, comments, TODO markers and footnotes rendered from formats
- [ ] Remove the old document model and layout code once the new core is in place
- [ ] Split `book_editor.cpp` (5,400 lines) into smaller classes

**Done when:** benchmark thresholds are met on all platforms and a manual test on a large
document is positive.

### Stage 2 – Wire features that already exist

These are implemented and unit-tested in core, but not reachable from the GUI.

- [ ] Find & Replace: connect `edit.find`, `edit.findReplace`, `edit.findNext/Previous`,
      `edit.findInBook` to `FindReplaceBar` and `SearchPanel`
- [ ] Spell check: create `SpellCheckService`, pass it to the editor, dictionary language setting
- [ ] Grammar check (LanguageTool)
- [ ] Zoom, formatting marks, status bar toggle
- [ ] Accessibility, comments and tags panels, quick insert, snapshots, word frequency
      analysis, text-to-speech, split view

### Stage 3 – Text styles (#00044B–F)

Design: `docs/superpowers/specs/2026-04-10-text-styling-system-design.md`.
Starts after Stage 1, because B and C change `BookEditor`.

- [ ] B – Pending format (toggle bold, then type) and clear formatting (Ctrl+Space)
- [ ] C – Toolbar follows the formatting at the cursor; font size drop-down
- [ ] D – Text and highlight color
- [ ] E – Paragraph styles (Heading 1–3, Body, Quote, Code)
- [ ] F – User-defined styles stored in the project

### Stage 4 – Phase 1 completion

- [ ] Statistics bar and weekly statistics panel
- [ ] Named perspectives (Writer, Editor, Researcher, Planner), save and manage dialogs
- [ ] Export: DOCX and Markdown as a minimum
- [ ] Translations (Qt Linguist `.ts`/`.qm`)
- [ ] Release **0.4.0-alpha**

---

## Phase 2 – Plugins (target: 0.5.0-alpha)

All plugins are free and MIT-licensed. Plugin signing (`PluginSignature`, `TrustedKeys`)
stays: it protects users from tampered code.

- [ ] Plugin Manager dialog: discovery, install, uninstall, enable, disable, settings
- [ ] Extension points: `IExporter`, `IPanelProvider`, `IAssistant`
- [ ] First plugins: statistics (Meerkat), writing goals (Lion), notes and character cards
      (Elephant), quick actions and snippets (Cheetah)
- [ ] Plugin API documentation and templates

## Phase 3 – Writer's features

Delivered as free plugins or core features, in an order decided after Phase 2.

- [ ] Mind maps and timelines (`.kmap`, `.ktl`)
- [ ] Export suite: PDF, EPUB, LaTeX, ICML
- [ ] Import: DOCX, Markdown, plain text, Scrivener
- [ ] Research tools and notes
- [ ] AI assistant integrations (user-provided keys or local models)
- [ ] Tables, lists and images in the editor

## Phase 4 – Release 1.0

- [ ] Full test suite (unit, integration, GUI with QTest, rendering snapshots)
- [ ] User manual and plugin development guide
- [ ] Installers: Windows, macOS (`.dmg`), Linux (`.deb`, Flatpak)
- [ ] Version 1.0.0

No dates are given: the project is developed in spare time, and earlier estimates
proved unreliable.

---

## Related documents

- `CHANGELOG.md` – what changed in each version
- `docs/superpowers/specs/` and `docs/superpowers/plans/` – designs and implementation plans
- `docs/openspec-archive/` – historical task records (#00001–#00045)
- `project_docs/06_roadmap.md` – rules for maintaining this file and the changelog
