# Proposal: Enhance About Dialog

**Change ID:** `00017-enhance-about-dialog`
**Type:** Enhancement
**Phase:** 0 (Qt Foundation)
**Estimated Effort:** 2-3 hours
**Task Number:** #00017

---

## Why

Current About dialog is a simple QMessageBox with basic HTML text - insufficient for a professional application. The wxWidgets version had a rich About dialog with custom banner, tabbed interface (4 tabs), detailed component attributions, and full MIT license text. The Qt version needs feature parity with improved design.

---

## What Changes

- Add `AboutDialog` class (QDialog subclass) with custom UI
- Create 3-tab interface: About, Third-Party Components, License
- Add 580×100px banner image at top (placeholder: black bg, white text)
- Replace `MainWindow::onAbout()` to use custom dialog instead of QMessageBox
- Update CMakeLists.txt with new source files

---

## Impact

**Affected specs:** `gui/about-dialog` (NEW capability)

**Affected code:**
- New: `include/kalahari/gui/dialogs/about_dialog.h`
- New: `src/gui/dialogs/about_dialog.cpp`
- Modified: `src/gui/main_window.cpp` (onAbout method)
- Modified: `src/CMakeLists.txt`

---

## Problem Statement

Current About dialog (Task #00011) is a simple `QMessageBox::about()` with basic HTML text. This is insufficient for a professional application. The wxWidgets version had a rich About dialog with:
- Custom banner at top (580x100px)
- Tabbed interface (4 tabs: Main, Third-Party, License, Credits)
- Detailed component attributions
- Full MIT license text

The Qt version needs feature parity with improved design.

---

## Proposed Solution

Create custom `AboutDialog` (QDialog subclass) with Qt6 widgets, matching wxWidgets functionality:

### UI Components
1. **Banner** - Top banner image (580x100px) with "KALAHARI" placeholder text
   - Black background, white text, centered
   - Common across all tabs (above QTabWidget)

2. **QTabWidget** - 3 tabs (user requirement: simplified from wxWidgets 4 tabs)
   - **Tab 1: "About"** - Application info + credits (merged from wxWidgets "Kalahari" + "Credits")
   - **Tab 2: "Third-Party Components"** - Attribution list (QTextEdit, read-only)
   - **Tab 3: "License"** - Full MIT license text (QTextEdit, read-only)

3. **Close button** - Bottom-aligned, QPushButton

### Window Properties
- **Size:** 600×720px (fixed, non-resizable)
- **Style:** `QDialog::setWindowFlags(Qt::Dialog | Qt::MSHBmoveWindowHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint)`
- **Modal:** `dialog->exec()` (blocking)

### Content

**Tab 1: About**
- Application name: "Kalahari Writer's IDE 0.3.1-alpha (Qt6)"
- Version, license (MIT), copyright (© 2025)
- Description: "Advanced writing environment for book authors"
- Qt version: `qVersion()`
- Credits section (development team info)

**Tab 2: Third-Party Components**
List of all dependencies with licenses:
- Qt6 6.5.0+ (LGPL v3)
- nlohmann_json (MIT)
- spdlog (MIT)
- libzip (BSD 3-Clause)
- Catch2 (Boost Software License 1.0)
- pybind11 (BSD 3-Clause)
- Python 3.11 (PSF License)
- vcpkg (MIT)

**Tab 3: License**
- Full MIT License text
- Trademark notice: "Kalahari" name trademarked

---

## Benefits

1. **Professional appearance** - Custom dialog vs generic QMessageBox
2. **Complete attribution** - Proper third-party component credits
3. **Legal compliance** - Full license text displayed
4. **Feature parity** - Matches wxWidgets version functionality
5. **Reusable pattern** - Template for other custom dialogs (Phase 1+)

---

## Alternatives Considered

### Alternative 1: Keep QMessageBox::about()
**Pros:** Simple, minimal code
**Cons:** Unprofessional, no third-party attributions, no license text
**Verdict:** ❌ Rejected - insufficient for production app

### Alternative 2: QDialog with single page (no tabs)
**Pros:** Simpler implementation
**Cons:** Too much content for single page, poor UX
**Verdict:** ❌ Rejected - poor information architecture

### Alternative 3: Web view (QWebEngineView)
**Pros:** Rich HTML formatting
**Cons:** Heavy dependency (QtWebEngine), overkill for static content
**Verdict:** ❌ Rejected - unnecessary complexity

---

## Implementation Impact

### New Files
- `include/kalahari/gui/dialogs/about_dialog.h` (~80 lines)
- `src/gui/dialogs/about_dialog.cpp` (~350 lines)

### Modified Files
- `src/gui/main_window.cpp` - Replace `onAbout()` implementation (lines 724-744)
- `src/CMakeLists.txt` - Add new source files

### Dependencies
- No new dependencies (Qt6 Widgets already included)

### Testing
- Manual testing (open dialog, verify tabs, check content)
- No automated tests (GUI dialog, Phase 0)

---

## Migration from wxWidgets

### Mapping: wxWidgets → Qt6

| wxWidgets | Qt6 | Notes |
|-----------|-----|-------|
| `wxDialog` | `QDialog` | Base class |
| `wxNotebook` | `QTabWidget` | Tabbed interface |
| `wxPanel` | `QWidget` | Tab content containers |
| `wxBoxSizer` | `QVBoxLayout` | Vertical layout |
| `wxStaticBitmap` | `QLabel::setPixmap()` | Banner image |
| `wxStaticText` | `QLabel` | Static text |
| `wxTextCtrl` (multiline, read-only) | `QTextEdit` (read-only) | Long text display |
| `wxButton` | `QPushButton` | Close button |
| `wxBitmap` + `wxMemoryDC` | `QPixmap` + `QPainter` | Banner creation |

### Preserved from wxWidgets
- Dialog size: 600×720px
- Banner size: 580×100px
- Tab structure (simplified: 4 → 3 tabs)
- Content layout and organization
- Placeholder banner design (black bg, white "KALAHARI" text)

### Qt6 Improvements
- Automatic DPI scaling (no manual code)
- QSS styling support (future: theme-aware colors)
- Signal/slot connection (vs wxWidgets Bind)
- Simpler layout API (addWidget vs Add with flags)

---

## Risks & Mitigation

### Risk 1: Banner image placeholder
**Issue:** Placeholder "KALAHARI" text may look unpolished
**Mitigation:** Acceptable for Phase 0, design proper banner in Phase 1+ (branding task)

### Risk 2: Component list maintenance
**Issue:** Third-party list may become outdated
**Mitigation:** Document in code comments to update when dependencies change

### Risk 3: License text changes
**Issue:** If license changes (e.g., add contributors), need to update
**Mitigation:** MIT license stable, trademark notice clear

---

## Success Criteria

- [ ] AboutDialog opens from Help → About Kalahari
- [ ] Banner displays at top (580×100px, black bg, white text)
- [ ] 3 tabs visible and functional
- [ ] Tab 1 (About): Application info + credits displayed correctly
- [ ] Tab 2 (Components): All dependencies listed with licenses
- [ ] Tab 3 (License): Full MIT license text readable
- [ ] Close button closes dialog
- [ ] Dialog is modal (blocks main window)
- [ ] Dialog size 600×720px (fixed, non-resizable)
- [ ] Build succeeds on Windows (MSVC)
- [ ] Manual testing confirms all content correct

---

## Follow-up Tasks

**Phase 1:**
- [ ] Design professional banner image (replace placeholder)
- [ ] Add clickable links (GitHub, website)
- [ ] Theme-aware colors (QSS integration)

**Phase 2+:**
- [ ] Auto-detect component versions from vcpkg manifest
- [ ] Check for updates button
- [ ] Animated banner (subtle logo animation)

---

## References

- **wxWidgets archive:** `wxwidgets-archive:src/gui/dialogs/about_dialog.cpp`
- **Current Qt code:** `src/gui/main_window.cpp:724-744` (`onAbout()`)
- **Qt6 Documentation:** [QDialog](https://doc.qt.io/qt-6/qdialog.html), [QTabWidget](https://doc.qt.io/qt-6/qtabwidget.html), [QTextEdit](https://doc.qt.io/qt-6/qtextedit.html)
- **ROADMAP.md:** Phase 0, Week 4 (Task #00011 - About Dialog, completed with basic version)

---

**Status:** ⏳ Awaiting approval
**Next Step:** User reviews proposal → Approval → Implementation
**Estimated Timeline:** 2-3 hours implementation + testing
