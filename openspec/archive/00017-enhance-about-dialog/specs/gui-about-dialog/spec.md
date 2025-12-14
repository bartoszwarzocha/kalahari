# Specification: About Dialog

**Capability ID:** `gui/about-dialog`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** New capability (ADDED)

---

## ADDED Requirements

### Requirement: Custom About Dialog

The application SHALL provide a custom About dialog (QDialog subclass) displaying application information, third-party component attributions, and license text in a tabbed interface.

**ID:** `gui/about-dialog/custom-dialog`
**Priority:** Medium
**Phase:** 0

**Rationale:** Professional applications require comprehensive About dialogs with proper attribution and legal compliance. Generic QMessageBox is insufficient.



#### Scenario: User opens About dialog

```
GIVEN the user is running Kalahari
WHEN the user selects "Help → About Kalahari" from the menu
THEN a custom About dialog SHALL open
AND the dialog SHALL be modal (blocking main window)
AND the dialog SHALL have fixed size 600×720 pixels
AND the dialog SHALL not be resizable
AND the dialog SHALL display a banner at the top
AND the dialog SHALL contain a tabbed interface with 3 tabs
AND the dialog SHALL have a "Close" button at the bottom
```

#### Scenario: User browses dialog tabs

```
GIVEN the About dialog is open
WHEN the user clicks on different tabs
THEN the tab content SHALL switch accordingly
AND the banner SHALL remain visible at the top
AND the dialog size SHALL not change
AND all tab content SHALL be readable without scrolling horizontally
```

#### Scenario: User closes About dialog

```
GIVEN the About dialog is open
WHEN the user clicks the "Close" button
THEN the dialog SHALL close
AND the main window SHALL regain focus

WHEN the user clicks the window close button (X)
THEN the dialog SHALL close
AND the main window SHALL regain focus
```

---

### Requirement: Banner Display

The About dialog SHALL display a banner image at the top, above the tabbed interface, with dimensions 580×100 pixels.

**ID:** `gui/about-dialog/banner`
**Priority:** Medium
**Phase:** 0

**Rationale:** Visual branding element, consistent with wxWidgets version. Placeholder design (black background, white "KALAHARI" text) acceptable for Phase 0.



#### Scenario: Banner renders correctly

```
GIVEN the About dialog is open
THEN a banner image SHALL be displayed at the top
AND the banner SHALL have dimensions 580×100 pixels
AND the banner SHALL have black background (#000000)
AND the banner SHALL display white text "KALAHARI" (#FFFFFF)
AND the text SHALL be centered horizontally and vertically
AND the text SHALL use bold font at 24 points
AND the banner SHALL be visible across all tabs
```

---

### Requirement: Tab 1 - About

The first tab ("About") SHALL display application information including name, version, description, Qt version, credits, and copyright.

**ID:** `gui/about-dialog/tab-about`
**Priority:** Medium
**Phase:** 0

**Rationale:** Users need to know app version, platform support, and development team.



#### Scenario: About tab displays application info

```
GIVEN the About dialog is open
AND the "About" tab is selected
THEN the following information SHALL be displayed:
- Application name: "Kalahari Writer's IDE 0.3.1-alpha (Qt6)"
- Font: 14pt bold, centered
- Platform info: "Cross-platform Writer's IDE for Windows, macOS, and Linux"
- Alignment: centered
- Description: Multi-line text describing app purpose, tech stack, current phase
- Word wrap: enabled
- Qt version: "Built with Qt X.Y.Z" (using qVersion())
- Credits section: Development team information
- Copyright: "Copyright (c) 2025 Kalahari Project"
- Font: 8pt, centered

AND all text SHALL be readable
AND text SHALL not overflow the tab boundaries
```

---

### Requirement: Tab 2 - Third-Party Components

The second tab ("Third-Party Components") SHALL display an attribution list of all third-party libraries used by Kalahari, including component name, description, license, and usage notes.

**ID:** `gui/about-dialog/tab-components`
**Priority:** High
**Phase:** 0

**Rationale:** Legal compliance requires proper attribution of open-source dependencies. Users have right to know what libraries are included.



#### Scenario: Components tab displays attribution list

```
GIVEN the About dialog is open
AND the "Third-Party Components" tab is selected
THEN the tab SHALL display a read-only text area
AND the text area SHALL have word wrap enabled
AND the text area SHALL be scrollable if content exceeds visible height

AND the following components SHALL be listed:
1. Qt6 6.5.0+ (LGPL v3 / Commercial)
2. nlohmann_json (MIT License)
3. spdlog (MIT License)
4. libzip (BSD 3-Clause License)
5. Catch2 (Boost Software License 1.0)
6. pybind11 (BSD 3-Clause License)
7. Python 3.11 (Python Software Foundation License)
8. vcpkg (MIT License)

AND for each component, the following SHALL be displayed:
- Component name + version (if applicable)
- Official website or GitHub repository URL
- Brief description (1 line)
- License name
- Usage notes (optional)

AND components SHALL be separated by blank lines for readability
```

---

### Requirement: Tab 3 - License

The third tab ("License") SHALL display the full MIT License text applicable to Kalahari core application, plus a trademark notice.

**ID:** `gui/about-dialog/tab-license`
**Priority:** High
**Phase:** 0

**Rationale:** Legal compliance requires displaying full license terms. Open-source users have right to view license.



#### Scenario: License tab displays MIT License

```
GIVEN the About dialog is open
AND the "License" tab is selected
THEN the tab SHALL display a read-only text area
AND the text area SHALL have word wrap enabled
AND the text area SHALL be scrollable if content exceeds visible height

AND the following SHALL be displayed:
- Header: "MIT License"
- Copyright line: "Copyright (c) 2025 Kalahari Project"
- Full MIT License text (standard template):
  - Permission is hereby granted...
  - THE SOFTWARE IS PROVIDED "AS IS"...
  - (complete standard MIT License)
- Trademark notice: "Note: The 'Kalahari' name and branding are trademarked."

AND all text SHALL be readable without horizontal scrolling
```

---

### Requirement: Modal Behavior

The About dialog SHALL be modal, blocking interaction with the main window until closed.

**ID:** `gui/about-dialog/modal`
**Priority:** Medium
**Phase:** 0

**Rationale:** Standard behavior for About dialogs. Prevents user confusion from having multiple About windows open.



#### Scenario: Dialog blocks main window

```
GIVEN the About dialog is open
WHEN the user attempts to interact with the main window
THEN the interaction SHALL be blocked
AND the About dialog SHALL remain in focus

WHEN the user closes the About dialog
THEN the main window SHALL regain focus
AND the user SHALL be able to interact with the main window again
```

---

### Requirement: Fixed Size

The About dialog SHALL have a fixed size of 600×720 pixels and SHALL not be resizable by the user.

**ID:** `gui/about-dialog/fixed-size`
**Priority:** Low
**Phase:** 0

**Rationale:** Content is designed for specific dimensions. Resizing could break layout or cause visual artifacts.



#### Scenario: Dialog has fixed size

```
GIVEN the About dialog is open
THEN the dialog SHALL have width 600 pixels
AND the dialog SHALL have height 720 pixels
AND the user SHALL NOT be able to resize the dialog
AND the maximize button SHALL NOT be present
AND only the close button SHALL be present in the title bar
```

---

### Requirement: Qt6 Widgets Implementation

The About dialog SHALL be implemented using Qt6 Widgets (QDialog, QTabWidget, QLabel, QTextEdit, QPushButton, QVBoxLayout).

**ID:** `gui/about-dialog/qt6-implementation`
**Priority:** Medium
**Phase:** 0

**Rationale:** Consistency with project tech stack (Qt6). Native Qt6 widgets ensure proper platform integration and DPI scaling.

#### Scenario: Implementation uses Qt6 widgets

```
GIVEN the AboutDialog class is implemented
THEN the class SHALL inherit from QDialog
AND the banner SHALL use QLabel::setPixmap()
AND the tabs SHALL use QTabWidget
AND the tab content SHALL use QWidget containers
AND the text areas SHALL use QTextEdit (read-only mode)
AND the Close button SHALL use QPushButton
AND the layout SHALL use QVBoxLayout

AND automatic DPI scaling SHALL be enabled (Qt6 default)
AND no manual DPI calculations SHALL be required
```

---
## Technical Notes
### Implementation Files
**New files:**
- `include/kalahari/gui/dialogs/about_dialog.h` (~80 lines)
- `src/gui/dialogs/about_dialog.cpp` (~350 lines)
**Modified files:**
- `src/gui/main_window.cpp` (replace `onAbout()` implementation)
- `src/CMakeLists.txt` (add new source files)
### Dependencies
- Qt6::Widgets (already included in project)
- Qt6::Gui (QPainter for banner generation)
- No new third-party dependencies
### Testing Strategy
**Phase 0:** Manual testing only
- Open dialog from menu
- Verify banner displays
- Switch between tabs
- Check content correctness
- Test Close button
- Verify modal behavior
**Phase 1+:** Consider automated tests
- QTest for dialog creation
- Tab switching simulation
- Content validation tests
---
## Future Enhancements (Post-Phase 0)
### Phase 1
- [ ] Design professional banner image (replace placeholder)
- [ ] Add clickable hyperlinks (GitHub, website)
- [ ] Theme-aware colors (QSS integration)
- [ ] Animated banner (subtle logo animation)
### Phase 2+
- [ ] Auto-detect component versions from vcpkg manifest
- [ ] "Check for Updates" button
- [ ] Contributor list (if project grows)
- [ ] Translation support (i18n for About dialog)
---
## Compliance & Legal
### Open Source Attribution
**Requirement:** All third-party open-source components MUST be attributed in the About dialog
**Covered by:** Requirement 4 (Tab 2 - Third-Party Components)
**Components listed:**
1. Qt6 6.5.0+ (LGPL v3)
2. nlohmann_json (MIT)
3. spdlog (MIT)
4. libzip (BSD 3-Clause)
5. Catch2 (Boost Software License 1.0)
6. pybind11 (BSD 3-Clause)
7. Python 3.11 (PSF License)
8. vcpkg (MIT)
**Maintenance:** Update list when dependencies change (document in code comments)
### License Display
**Requirement:** MIT License terms MUST be displayed in full
**Covered by:** Requirement 5 (Tab 3 - License)
**Content:** Full MIT License text + trademark notice
### Trademark Notice
**Requirement:** "Kalahari" trademark MUST be disclosed
**Covered by:** Requirement 5 (Tab 3 - License)
**Notice:** "Note: The 'Kalahari' name and branding are trademarked."
---
## Migration from wxWidgets
### Mapping: wxWidgets → Qt6
| wxWidgets Component | Qt6 Equivalent | Notes |
|---------------------|----------------|-------|
| `wxDialog` | `QDialog` | Base class |
| `wxNotebook` | `QTabWidget` | Tabbed interface |
| `wxPanel` | `QWidget` | Tab content containers |
| `wxBoxSizer` | `QVBoxLayout` | Vertical layout |
| `wxStaticBitmap` | `QLabel::setPixmap()` | Banner image display |
| `wxStaticText` | `QLabel` | Static text labels |
| `wxTextCtrl` (multiline, read-only) | `QTextEdit` (read-only) | Long text display |
| `wxButton` | `QPushButton` | Close button |
| `wxBitmap` + `wxMemoryDC` | `QPixmap` + `QPainter` | Banner generation |
| `wxFont` | `QFont` | Font styling |
| `Bind(wxEVT_BUTTON, ...)` | `connect(button, &QPushButton::clicked, ...)` | Event handling |
### Preserved Design Elements
- Dialog size: 600×720px
- Banner size: 580×100px
- Banner design: Black bg, white "KALAHARI" text, centered
- Tab structure (simplified: 4 → 3 tabs per user requirement)
- Content organization and text
- Modal behavior
- Fixed size (non-resizable)
### Qt6 Improvements
- Automatic DPI scaling (no manual code needed)
- Signal/slot type safety (compile-time checking)
- Simpler layout API (addWidget vs Add with complex flags)
- QSS styling support (future theme integration)
- Better font rendering (platform-native text)
---
**Specification Version:** 1.0
**Status:** ⏳ Awaiting approval
**Phase:** 0 (Qt Foundation)
**Change ID:** `00017-enhance-about-dialog`



#### Scenario: Implementation uses Qt6 widgets

```
GIVEN the AboutDialog class is implemented
THEN the class SHALL inherit from QDialog
AND the banner SHALL use QLabel::setPixmap()
AND the tabs SHALL use QTabWidget
AND the tab content SHALL use QWidget containers
AND the text areas SHALL use QTextEdit (read-only mode)
AND the Close button SHALL use QPushButton
AND the layout SHALL use QVBoxLayout

AND automatic DPI scaling SHALL be enabled (Qt6 default)
AND no manual DPI calculations SHALL be required
```

---

## Design Decisions

### Decision 1: 3 tabs instead of 4 (wxWidgets had 4)

**Context:** wxWidgets version had 4 tabs: Kalahari, Third-Party, License, Credits

**Decision:** Merge "Kalahari" and "Credits" tabs into single "About" tab (3 tabs total)



























































