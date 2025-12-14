# Implementation Tasks: Enhance About Dialog

**Change ID:** `00017-enhance-about-dialog`
**Task Number:** #00017
**Estimated Total Time:** 2-3 hours

---

## Pre-Implementation Checklist

- [x] Proposal reviewed and approved by user
- [x] Read wxWidgets archive implementation (`wxwidgets-archive:src/gui/dialogs/about_dialog.cpp`)
- [x] Read current Qt `onAbout()` implementation (`src/gui/main_window.cpp:724-744`)
- [x] Qt6 documentation reviewed (QDialog, QTabWidget, QTextEdit, QPixmap, QPainter)

---

## Implementation Tasks

### Task 1: Create AboutDialog Header (30 min)

**File:** `include/kalahari/gui/dialogs/about_dialog.h`

- [x] Create file with `#pragma once`
- [x] Add includes: `<QDialog>`, `<QPixmap>`
- [x] Create `kalahari::gui::dialogs` namespace
- [x] Declare `AboutDialog` class (inherits `QDialog`)
- [x] Add constructor: `AboutDialog(QWidget* parent = nullptr)`
- [x] Add private helper methods:
  - `QWidget* createAboutTab()` - Tab 1: Application info + credits
  - `QWidget* createComponentsTab()` - Tab 2: Third-party components list
  - `QWidget* createLicenseTab()` - Tab 3: MIT license text
  - `QPixmap createPlaceholderBanner(int width, int height)` - Banner generation
- [x] Add Doxygen comments for class and methods

**Acceptance:**
- Header compiles without errors
- All methods documented
- Namespace structure correct

---

### Task 2: Implement AboutDialog Constructor (45 min)

**File:** `src/gui/dialogs/about_dialog.cpp`

- [x] Create file with includes
- [x] Implement constructor:
  - [ ] Set window title: `"About Kalahari Writer's IDE"`
  - [ ] Set window flags: `Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint`
  - [ ] Set fixed size: `600×720px`
  - [ ] Create main layout (`QVBoxLayout`)
  - [ ] Create banner (`QLabel` with pixmap from `createPlaceholderBanner(580, 100)`)
  - [ ] Add banner to layout with margins: `10px all sides`
  - [ ] Create `QTabWidget`
  - [ ] Add 3 tabs:
    - `addTab(createAboutTab(), "About")`
    - `addTab(createComponentsTab(), "Third-Party Components")`
    - `addTab(createLicenseTab(), "License")`
  - [ ] Add tab widget to layout (stretch factor 1)
  - [ ] Create Close button (`QPushButton`, text "Close")
  - [ ] Connect Close button to `accept()` slot
  - [ ] Add Close button to layout (right-aligned, 10px margins)
  - [ ] Set layout on dialog

**Acceptance:**
- Dialog opens with correct size
- Banner displays at top
- 3 tabs visible
- Close button functional

---

### Task 3: Implement createAboutTab() (30 min)

**File:** `src/gui/dialogs/about_dialog.cpp`

- [x] Create `QWidget` container
- [x] Create `QVBoxLayout`
- [x] Add application name label:
  - Text: `"Kalahari Writer's IDE 0.3.1-alpha (Qt6)"`
  - Font: 14pt, bold
  - Alignment: center
  - Margins: 10px all sides
- [x] Add platform info label:
  - Text: `"Cross-platform Writer's IDE for Windows, macOS, and Linux"`
  - Alignment: center
  - Margins: 10px left/right/bottom
- [x] Add description label:
  - Text: Multi-line description (app purpose, tech stack, phase info)
  - Word wrap enabled
  - Margins: 10px all sides
  - Stretch factor: 1 (fills remaining space)
- [x] Add Qt version label:
  - Text: `"Built with Qt " + qVersion()`
  - Alignment: center
- [x] Add credits section label:
  - Text: Development team info
  - Small font (8pt)
  - Word wrap enabled
- [x] Add copyright label:
  - Text: `"Copyright (c) 2025 Kalahari Project"`
  - Font: 8pt
  - Alignment: center
  - Margins: 10px all sides
- [x] Set layout on widget, return widget

**Acceptance:**
- Tab 1 displays all content
- Text properly aligned and formatted
- No content overflow

---

### Task 4: Implement createComponentsTab() (30 min)

**File:** `src/gui/dialogs/about_dialog.cpp`

- [x] Create `QWidget` container
- [x] Create `QVBoxLayout`
- [x] Create `QTextEdit` (read-only, word wrap enabled)
- [x] Build component list text:
  - Header: "Kalahari uses the following third-party components:"
  - For each component (Qt6, nlohmann_json, spdlog, libzip, Catch2, pybind11, Python 3.11, vcpkg):
    - Component name + version
    - Description line
    - License line
    - Usage notes (if applicable)
    - Empty line separator
- [x] Set text to QTextEdit
- [x] Add QTextEdit to layout (stretch factor 1, 10px margins)
- [x] Set layout on widget, return widget

**Component List Content:**
```
Qt6 6.5.0+ (www.qt.io)
  Cross-platform GUI framework
  License: LGPL v3 / Commercial

nlohmann_json (github.com/nlohmann/json)
  JSON for Modern C++
  License: MIT License

spdlog (github.com/gabime/spdlog)
  Fast C++ logging library
  License: MIT License

libzip (libzip.org)
  C library for zip archives
  License: BSD 3-Clause License

Catch2 (github.com/catchorg/Catch2)
  Modern C++ test framework
  License: Boost Software License 1.0

pybind11 (github.com/pybind/pybind11)
  C++/Python interoperability
  License: BSD 3-Clause License

Python 3.11 (www.python.org)
  Embedded Python interpreter
  License: Python Software Foundation License

vcpkg (github.com/microsoft/vcpkg)
  C++ package manager
  License: MIT License
```

**Acceptance:**
- Tab 2 displays all components
- Text readable and properly formatted
- Scrollable if content exceeds visible area

---

### Task 5: Implement createLicenseTab() (20 min)

**File:** `src/gui/dialogs/about_dialog.cpp`

- [x] Create `QWidget` container
- [x] Create `QVBoxLayout`
- [x] Create `QTextEdit` (read-only, word wrap enabled)
- [x] Build license text:
  - "MIT License" header
  - Copyright line: "Copyright (c) 2025 Kalahari Project"
  - Full MIT License text (standard template)
  - Trademark notice: "Note: The 'Kalahari' name and branding are trademarked."
- [x] Set text to QTextEdit
- [x] Add QTextEdit to layout (stretch factor 1, 10px margins)
- [x] Set layout on widget, return widget

**Acceptance:**
- Tab 3 displays full MIT license
- Trademark notice visible
- Text readable and properly formatted

---

### Task 6: Implement createPlaceholderBanner() (25 min)

**File:** `src/gui/dialogs/about_dialog.cpp`

- [x] Add includes: `<QPainter>`, `<QFont>`
- [x] Create `QPixmap` with specified width and height
- [x] Create `QPainter` on pixmap
- [x] Fill background with black: `painter.fillRect(pixmap.rect(), Qt::black)`
- [x] Set text color to white: `painter.setPen(Qt::white)`
- [x] Set font: 24pt, bold
- [x] Calculate text position (centered):
  - `textRect = painter.fontMetrics().boundingRect("KALAHARI")`
  - `x = (width - textRect.width()) / 2`
  - `y = (height + textRect.height()) / 2`
- [x] Draw text: `painter.drawText(x, y, "KALAHARI")`
- [x] End painting
- [x] Return pixmap

**Acceptance:**
- Banner pixmap created with correct size
- Black background, white text
- Text centered horizontally and vertically

---

### Task 7: Update MainWindow::onAbout() (15 min)

**File:** `src/gui/main_window.cpp`

- [x] Add include: `#include "kalahari/gui/dialogs/about_dialog.h"`
- [x] Replace `onAbout()` implementation (lines 724-744):
  - Remove `QString aboutText` and `QMessageBox::about()` code
  - Create `AboutDialog` instance: `AboutDialog dialog(this);`
  - Show modal: `dialog.exec();`
  - Keep logging statements (info before/after)

**Old code (remove):**
```cpp
QString aboutText = tr(
    "<h2>Kalahari Writer's IDE</h2>"
    // ... HTML content ...
).arg(qVersion());

QMessageBox::about(this, tr("About Kalahari"), aboutText);
```

**New code:**
```cpp
dialogs::AboutDialog dialog(this);
dialog.exec();
```

**Acceptance:**
- Help → About Kalahari opens custom dialog
- Dialog is modal (blocks main window)
- No compile errors

---

### Task 8: Update CMakeLists.txt (10 min)

**File:** `src/CMakeLists.txt`

- [x] Add header to kalahari sources:
  - `include/kalahari/gui/dialogs/about_dialog.h`
- [x] Add source to kalahari sources:
  - `src/gui/dialogs/about_dialog.cpp`
- [x] Verify alphabetical order maintained
- [x] Run CMake configure to check for errors

**Acceptance:**
- CMakeLists.txt updated
- CMake configure succeeds
- Build system recognizes new files

---

### Task 9: Build and Test (15 min)

- [x] Clean build: `cmake --build build-windows --config Debug --clean-first`
- [x] Verify build succeeds (no errors)
- [x] Run application: `build-windows\bin\kalahari.exe`
- [x] Open Help → About Kalahari
- [x] Verify:
  - Dialog opens and is modal
  - Banner displays (black bg, white "KALAHARI" text, centered)
  - Tab 1 (About): Application info + credits correct
  - Tab 2 (Components): All 8 dependencies listed
  - Tab 3 (License): Full MIT license + trademark notice
  - Close button closes dialog
  - Dialog size 600×720px (verify visually)
- [x] Check console logs (spdlog output)
- [x] Test on Windows (primary platform for Phase 0)

**Acceptance:**
- Application builds without errors
- About dialog functions correctly
- All content displays as expected
- No crashes or warnings

---

### Task 10: Commit Changes (10 min)

- [x] Stage files:
  - `include/kalahari/gui/dialogs/about_dialog.h`
  - `src/gui/dialogs/about_dialog.cpp`
  - `src/gui/main_window.cpp`
  - `src/CMakeLists.txt`
- [x] Commit with message:
  ```
  feat(gui): Enhance About dialog with tabs and banner (Task #00017)

  Replaced simple QMessageBox with custom AboutDialog (QDialog):
  - Banner at top (580×100px placeholder: black bg, white "KALAHARI" text)
  - 3 tabs: About, Third-Party Components, License
  - Tab 1: Application info + credits
  - Tab 2: 8 third-party components with licenses
  - Tab 3: Full MIT license text + trademark notice
  - Close button (modal dialog)
  - Fixed size 600×720px

  Migrated from wxWidgets AboutDialog (wxwidgets-archive).
  Feature parity with improved Qt6 implementation.

  🤖 Generated with [Claude Code](https://claude.com/claude-code)

  Co-Authored-By: Claude <noreply@anthropic.com>
  ```
- [x] Verify commit created successfully

---

## Post-Implementation Checklist

- [x] All tasks marked as complete
- [x] Build succeeds on Windows
- [x] Manual testing passed
- [x] Commit pushed to repository
- [x] Task #00017 marked as DONE in tasks/00017_enhance_about_dialog.md
- [x] CHANGELOG.md updated ([Unreleased] section)
- [x] OpenSpec change archived (after deployment)

---

## Notes

- **Platform focus:** Phase 0 targets Windows primarily, macOS/Linux in Phase 1
- **Design placeholder:** Banner is temporary (Phase 1: professional design)
- **Component list:** Update when dependencies change (document in code comments)
- **Testing:** Manual only (Phase 0 - no automated GUI tests yet)
- **Qt migration:** Direct port from wxWidgets, preserving layout and content

---

**Total Estimated Time:** 2-3 hours (165 minutes for tasks + 30 min buffer)
**Phase:** 0 (Qt Foundation)
**Priority:** Medium (polish, professional appearance)
