# Kalahari Development Roadmap

> **Writer's IDE** – C++20 + Qt6 desktop application, free and open source (MIT)

**Current Phase:** Phase 1 (Core Editor) – in progress, Part A released as 0.4.0-alpha
**Version:** 0.4.0-alpha
**Next Release:** 0.4.x-alpha (Phase 1, Part B)
**Last Updated:** 2026-11-13

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
| Theme and icon system, ArtProvider (#00027, #00032) | Done; the editor colors are set per theme |
| SQLite project database (#00041) | Done |
| Formatting round-trip in KML (#00044A) | Done |
| Custom text editor (#00042, #00043) | Rebuilt in editor stages 1–4 – see Part A |

---

## Phase 1 – Core Editor (0.4.0-alpha, then 0.4.x)

0.4.0-alpha is released after Part A – the editor with every part of the full editor –, the
settings rebuild, the translations and the tests (decisions of 2026-10-08 and 2026-11-09).
Text styles and the statistics, planned for Part B, were finished before it and are part of
0.4.0-alpha. DOCX/Markdown export follows in 0.4.x releases, and Phase 1 ends with a test of
the whole program on Windows and Linux. What each version brings is in `CHANGELOG.md`.

### Part A – Editor

- [x] Stage 0 – Foundation: tests on three systems, disabled unimplemented commands,
      this roadmap and the MIT license
- [x] Stage 1 – Speed: a 150k-word chapter opens in a fraction of a second, typing stays smooth
      (one layout core, layout on demand, the old document model removed)
- [x] Stage 2 – Typography (line spacing, paragraph spacing, indents), paste and undo,
      window resize
- [x] Stage 3 – Drag and drop of text, Find & Replace, cursor settings, paragraph alignment
- [x] Stage 4 – Page view (paper size, margins, page numbers), zoom, typewriter scrolling
- [x] Stage 5 – Cleanup: old code removed, unimplemented options disabled, editor settings in
      one place, room made for the features of Stage 6
- [x] Continuous view as one endless page of the page width, with lines broken as on pages
- [x] Comments, TODOs and notes: marks in the text, the frame they are written in (a click on
      a mark opens it, also in the page view), cards under the paragraphs in the continuous
      view, the Annotations panel
- [x] Line or paragraph numbers on the left margin, for drafting only (not printed)
- [x] Stage 6 – Wire the existing features: spelling, grammar (local LanguageTool), versions
      of the texts (snapshots), quick insert, word repetition analysis and the focus modes.
      The tags panel of typed TODO markers is not used: typed markers were dropped on
      2026-10-06 and annotations took their place. Text-to-speech and split view are to be
      discussed again.
- [x] The book: book types, the new project file, the Navigator with the book and the
      Workshop, materials (card, sources, links, lists, the Cover), books of 0.3 converted
- [x] Full editor, parts 1–9 (plan of 2026-10-12): paragraph styles, headings, breaks and
      pasting from other programs; autocorrect and special characters; lists and the Font and
      Paragraph windows; links; tables; images with captions; footnotes and endnotes
- [x] Full editor, part 10: Find in Book, with Replace All in Book
- [ ] Full editor, parts 11–12: splitting a chapter, fields and Go To
- [ ] A window of its own for each book, and one book never open twice; the tabs of the
      texts already go to windows of their own

**Done when:** benchmark results stay good on all platforms and a manual test on a large
document is positive.

### Settings rebuild

- [x] K1 – Quick fixes: Apply/OK without the progress dialog, only changed options saved,
      faster theme switch, a restart note after a language change
- [x] K2 – One list of all settings with their defaults; editor colors per theme; a change
      is saved shortly after it is made, safely
- [x] K3 – Editor settings read from that list
- [x] K4 – A new Settings dialog whose pages are built when first opened
- [x] K5 – All settings in one file: window layout, toolbars and recent books move from
      QSettings to `settings.json`

### Translations

- [x] Qt Linguist pipeline (`.ts`/`.qm`) and language switch
- [x] Polish: menus, main window, panels, dialogs and messages
- [x] Polish: the Settings dialog

### Tests and CI

- [x] Each test case registered separately in CTest (`catch_discover_tests`)
- [x] Sanitizer job (ASan/UBSan), coverage report and clang-tidy on changed files in CI
- [x] KML round-trip tests, including the chapters of the example project
- [x] Package installation in CI that survives a stalled mirror
- [x] One Qt version (6.9) on all platforms, with 6.4 as the minimum in CMake
- [x] Editor benchmark run in CI, with the results in the job summary
- [x] One version number in five files, checked by a test (`scripts/release.py`), and a
      draft GitHub release made from the CHANGELOG when a version tag is pushed

### Release

- [x] Release **0.4.0-alpha**

### Part B – 0.4.x releases

Design of the text styles: `docs/superpowers/specs/2026-04-10-text-styling-system-design.md`.

- [x] Text styles: pending format and clear formatting, a toolbar that follows the cursor,
      text and highlight color, the paragraph styles of the book's type and the writer's own,
      stored in the book – in 0.4.0-alpha
- [x] Statistics bar and weekly statistics panel, with writing goals – in 0.4.0-alpha
- [ ] Export: DOCX and Markdown
- [ ] Test of the whole program at the end of Phase 1, on Windows and Linux

Named perspectives and the default layout of the toolbars and panels moved to the end of the
work, where the layouts of the window are set together (decisions of 2026-10-08 and
2026-11-10).

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
- [x] Tables, lists and images in the editor – done in Phase 1, with the full editor

## Phase 4 – Release 1.0

- [ ] Full test suite (unit, integration, GUI with QTest, rendering snapshots)
- [ ] User manual and plugin development guide
- [ ] Installers: Windows, macOS (`.dmg`), Linux (`.deb`, Flatpak)
- [ ] Default layouts of the toolbars and panels, with named perspectives (Writer, Editor,
      Researcher, Planner)
- [ ] Version 1.0.0

No dates are given: the project is developed in spare time, and earlier estimates
proved unreliable.

---

## Related documents

- `CHANGELOG.md` – what changed in each version
- `docs/superpowers/specs/` and `docs/superpowers/plans/` – designs and implementation plans
- `docs/openspec-archive/` – historical task records (#00001–#00045)
- `project_docs/06_roadmap.md` – rules for maintaining this file and the changelog
