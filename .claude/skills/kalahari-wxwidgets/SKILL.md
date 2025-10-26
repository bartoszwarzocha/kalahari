---
name: kalahari-wxwidgets
description: wxWidgets C++20 desktop development expert for Kalahari Writer's IDE
---

# Kalahari wxWidgets Expert Skill

## Purpose

This skill provides specialized knowledge for wxWidgets 3.3.0+ desktop development in the context of **Kalahari Writer's IDE**. It ensures consistent, correct, and efficient use of wxWidgets APIs, patterns, and best practices specific to this project.

## When to Activate

Claude should use this skill when:
- ✅ Writing wxWidgets GUI code (windows, panels, controls)
- ✅ Implementing wxAUI docking system
- ✅ Working with wxRichTextCtrl (rich text editor)
- ✅ Creating custom wxWidgets controls
- ✅ Implementing wxWidgets event handlers
- ✅ Debugging wxWidgets-specific issues
- ✅ Optimizing wxWidgets performance

## Core Competencies

### 1. Kalahari wxWidgets Stack

**Versions & Configuration:**
- wxWidgets 3.3.0+ (dark mode support from 3.3.0)
- Build: CMake 3.21+ with vcpkg manifest mode
- Platforms: Windows (MSVC 2019+), macOS (Clang 10+), Linux (GCC 10+)
- Unicode: Always use `wxString` (UTF-8), never `char*`

**Key Components:**
- **wxAUI:** Advanced User Interface (dockable panels, perspectives)
- **wxRichTextCtrl:** Main text editor (supports formatting, styles)
- **wxStyledTextCtrl:** Alternative for code/plain text (not used in MVP)
- **wxTreeCtrl:** Project navigator (book structure tree)
- **wxNotebook/wxAuiNotebook:** Tabbed interfaces

### 2. MVP (Model-View-Presenter) Pattern

**Kalahari Architecture:**
```cpp
// Model: Pure data, no GUI dependencies
class Document {
    // Business logic, data storage
};

// View: wxWidgets GUI, thin wrapper
class DocumentView : public wxPanel {
    wxRichTextCtrl* m_editor;
    // Only GUI code, delegates to Presenter
};

// Presenter: Logic bridge
class DocumentPresenter {
    Document* m_model;
    DocumentView* m_view;
    // Coordinates Model ↔ View
};
```

**Rules:**
- Views never access Models directly
- Presenters handle all business logic
- Models are GUI-agnostic (can be unit tested without wxWidgets)

### 3. Common Patterns & Templates

#### Main Application Window
```cpp
class MainWindow : public wxFrame {
public:
    MainWindow();

private:
    // wxAUI manager for docking
    wxAuiManager m_auiManager;

    // Panels
    wxPanel* m_projectNavigator;
    wxRichTextCtrl* m_editor;
    wxPanel* m_assistant;

    // Event handlers
    void OnFileNew(wxCommandEvent& event);
    void OnFileOpen(wxCommandEvent& event);
    void OnFileSave(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

// Event table
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_MENU(wxID_NEW, MainWindow::OnFileNew)
    EVT_MENU(wxID_OPEN, MainWindow::OnFileOpen)
    EVT_MENU(wxID_SAVE, MainWindow::OnFileSave)
wxEND_EVENT_TABLE()
```

#### wxAUI Docking Setup
```cpp
void MainWindow::SetupAUI() {
    m_auiManager.SetManagedWindow(this);

    // Add panes
    m_auiManager.AddPane(m_projectNavigator,
        wxAuiPaneInfo()
            .Name("ProjectNavigator")
            .Caption("Project")
            .Left()
            .MinSize(200, -1)
            .BestSize(250, -1)
            .CloseButton(false)
    );

    m_auiManager.AddPane(m_editor,
        wxAuiPaneInfo()
            .Name("Editor")
            .CenterPane()
            .PaneBorder(false)
    );

    m_auiManager.Update();
}
```

#### wxRichTextCtrl Setup
```cpp
void EditorView::SetupEditor() {
    m_editor = new wxRichTextCtrl(this, wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxDefaultSize,
        wxRE_MULTILINE | wxRE_READONLY  // or wxRE_WRITABLE
    );

    // Set font
    wxFont font(12, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL,
                wxFONTWEIGHT_NORMAL, false, "Consolas");
    m_editor->SetFont(font);

    // Enable undo/redo
    m_editor->GetBuffer().SubmitAction(new wxRichTextAction());
}
```

### 4. Event Handling

**Best Practices:**
- Use `Bind()` for dynamic binding, `wxEVT_*` for table-based
- Always check `event.Skip()` for propagation
- Use `CallAfter()` for GUI updates from worker threads

```cpp
// Dynamic binding (preferred for new code)
m_button->Bind(wxEVT_BUTTON, &MyClass::OnButton, this);

// Thread-safe GUI update
wxTheApp->CallAfter([this]() {
    m_statusBar->SetStatusText("Updated from worker thread");
});
```

### 5. i18n/l10n Integration

**wxLocale + gettext:**
```cpp
// In Application::OnInit()
m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
m_locale->AddCatalogLookupPathPrefix("locales");
m_locale->AddCatalog("kalahari");

// In code - use _() macro
wxString msg = _("Welcome to Kalahari");
wxString fmt = wxString::Format(_("File %s saved"), filename);
```

**Extraction:**
```bash
# Extract strings for translation
xgettext --keyword=_ -o kalahari.pot src/*.cpp
msgmerge locales/pl/kalahari.po kalahari.pot
msgfmt locales/pl/kalahari.po -o locales/pl/kalahari.mo
```

### 6. Common Pitfalls & Solutions

| Problem | Solution |
|---------|----------|
| **Memory leaks** | Use `new` for wxWidgets controls (parent deletes children), smart pointers for non-GUI |
| **Event loops** | Never block main thread, use `wxThread` or thread pool |
| **Cross-platform paths** | Use `wxFileName`, `wxStandardPaths`, never hardcode `/` or `\\` |
| **Unicode issues** | Always use `wxString`, convert with `mb_str()`, `wc_str()`, `ToStdString()` |
| **Event propagation** | Call `event.Skip()` if you don't fully handle event |
| **Sizer issues** | Set `wxEXPAND`, `wxALL` flags, call `Layout()` after changes |

### 7. Context7 Integration

**MANDATORY:** Always fetch latest wxWidgets docs before coding:

```
1. resolve-library-id("wxWidgets")
2. get-library-docs(id, topic="wxRichTextCtrl")
3. Generate code based on current docs
```

**Topics to query:**
- "wxAUI docking manager"
- "wxRichTextCtrl formatting"
- "wxTreeCtrl custom rendering"
- "wxFrame menu and toolbar"
- "wxWidgets threading"

## Code Style

```cpp
/// @file main_window.h
/// @brief Main application window with wxAUI docking

#pragma once

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/richtext/richtextctrl.h>

namespace kalahari {
namespace gui {

/// Main application window
class MainWindow : public wxFrame {
public:
    /// Constructor
    MainWindow();

    /// Destructor
    ~MainWindow() override;

private:
    // Member variables (m_ prefix)
    wxAuiManager m_auiManager;
    wxRichTextCtrl* m_editor;

    // Event handlers (On prefix)
    void OnFileNew(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    // Helper methods (camelCase)
    void setupAUI();
    void createMenuBar();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
```

## Testing Patterns

```cpp
// Presenter can be unit tested without GUI
TEST_CASE("DocumentPresenter saves changes") {
    Document model;
    MockDocumentView view;  // Mock, no real wxWidgets
    DocumentPresenter presenter(&model, &view);

    presenter.SetText("Test content");
    presenter.Save();

    REQUIRE(model.GetContent() == "Test content");
}

// GUI tests use wxUIActionSimulator
TEST_CASE("MainWindow menu") {
    MainWindow window;
    wxUIActionSimulator sim;

    sim.Select(wxID_NEW);  // Simulates menu click
    REQUIRE(window.GetDocumentCount() == 1);
}
```

## Resources

- **Official Docs:** https://docs.wxwidgets.org/3.3/
- **Kalahari Docs:** project_docs/08_gui_design.md
- **Architecture:** project_docs/03_architecture.md (MVP pattern)
- **i18n:** project_docs/09_i18n.md

## Quick Reference

**Most Used Classes:**
- `wxFrame` - Main window
- `wxPanel` - Container for controls
- `wxBoxSizer`, `wxGridSizer` - Layout management
- `wxMenu`, `wxMenuBar`, `wxToolBar` - Menus and toolbars
- `wxStatusBar` - Bottom status display
- `wxAuiManager`, `wxAuiPaneInfo` - Docking system
- `wxRichTextCtrl` - Rich text editor
- `wxTreeCtrl` - Tree view (project navigator)
- `wxListCtrl`, `wxDataViewCtrl` - List/table displays
- `wxDialog`, `wxMessageBox` - Dialogs

**Most Used Events:**
- `wxEVT_BUTTON` - Button clicks
- `wxEVT_MENU` - Menu selection
- `wxEVT_CLOSE_WINDOW` - Window closing
- `wxEVT_SIZE` - Window resized
- `wxEVT_IDLE` - Idle processing
- `wxEVT_THREAD` - Worker thread → GUI communication

---

**Skill Version:** 1.0
**Last Updated:** 2025-10-26
**Framework Compatibility:** Kalahari Phase 0+
