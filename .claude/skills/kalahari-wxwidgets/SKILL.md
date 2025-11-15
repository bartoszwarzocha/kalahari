---
name: kalahari-wxwidgets
description: USE ME when implementing wxAUI panels, wxRichTextCtrl, wxStaticBoxSizer layouts, wxWidgets event handlers, or wxFrame/wxDialog GUI code. Contains Kalahari-specific wxWidgets patterns and Context7 integration workflow.
---

# Kalahari wxWidgets Expert Skill

## Quick Activation Triggers

USE this skill when you see:
- "panel", "dialog", "sizer", "wxAUI", "perspective"
- "wxRichTextCtrl", "wxTreeCtrl", "wxBoxSizer", "wxStaticBoxSizer"
- "event handler", "Bind()", "wxEVT_", "EVT_MENU"
- "GUI code", "wxWidgets", "wxFrame", "wxWindow"
- "layout", "docking", "toolbar", "menu", "statusbar"

## Critical Patterns (Kalahari-Specific)

### 1. wxStaticBoxSizer Layout (MANDATORY)

```cpp
// ALWAYS use this pattern for configuration sections
wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Group Title");

// Parent controls to StaticBox, use wxEXPAND, set proportions
wxCheckBox* checkbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY, "Label");
box->Add(checkbox, 0, wxALL | wxEXPAND, 5);  // proportion=0 (fixed)

wxStaticText* text = new wxStaticText(box->GetStaticBox(), wxID_ANY, "Description");
box->Add(text, 1, wxALL | wxEXPAND, 5);  // proportion=1 (flexible)

mainSizer->Add(box, 1, wxALL | wxEXPAND, 10);
SetSizer(mainSizer);  // CRITICAL: always call SetSizer()
```

### 2. wxAUI Docking Setup

```cpp
void MainWindow::SetupAUI() {
    m_auiManager.SetManagedWindow(this);

    m_auiManager.AddPane(m_panel,
        wxAuiPaneInfo()
            .Name("PanelName")
            .Caption("Title")
            .Left()  // or Right(), Center(), Top(), Bottom()
            .MinSize(200, -1)
            .BestSize(250, -1)
            .CloseButton(false)
    );

    m_auiManager.Update();
}
```

### 3. Event Handling (MVP Pattern)

```cpp
// Dynamic binding (preferred)
m_button->Bind(wxEVT_BUTTON, &MyClass::OnButton, this);

// Thread-safe GUI update
wxTheApp->CallAfter([this]() {
    m_statusBar->SetStatusText("Updated from worker thread");
});

// Always check event.Skip() for propagation
void OnEvent(wxCommandEvent& event) {
    // Handle event
    event.Skip();  // Allow other handlers
}
```

### 4. Context7 Integration (MANDATORY)

**ALWAYS fetch wxWidgets docs before coding:**

```
1. mcp__context7__resolve-library-id("wxWidgets")
2. mcp__context7__get-library-docs(id, topic="wxRichTextCtrl")
3. Generate code based on current docs
```

Common topics: "wxAUI", "wxRichTextCtrl", "wxTreeCtrl", "wxFrame", "wxWidgets threading"

### 5. BWX SDK Reactive System (Task #00043)

**Purpose:** Observer pattern for broadcast GUI updates (theme, icons, DPI).

**Architecture:**
- `bwxReactive` - Base class with static registry + pure virtual handlers
- `bwxManaged<T>` - Template wrapper (wxControl + bwxReactive multiple inheritance)
- `bwxReactiveDialog` - Dialogs that auto-respond to broadcasts

**When to use:**
- Creating controls that need dynamic theme/icon updates
- Implementing dialogs that should react to appearance changes
- **DO NOT use for:** Font scaling (use wxFont::Scaled() instead!)

**Usage:**

```cpp
// Step 1: Use reactive control types (instead of raw wxWidgets)
#include <bwx_sdk/bwx_gui/bwx_managed.h>

// Replace: wxStaticText* label = new wxStaticText(...);
// With:
bwx::gui::StaticText* label = new bwx::gui::StaticText(parent, wxID_ANY, "Text");

// Step 2: Dialog inherits from ReactiveDialog
#include <bwx_sdk/bwx_gui/bwx_reactive_dialog.h>

class MyDialog : public bwx::gui::ReactiveDialog {
public:
    MyDialog(wxWindow* parent)
        : ReactiveDialog(parent, wxID_ANY, "Settings") {
        // Build UI with bwx::gui::* controls
    }
};

// Step 3: Broadcast changes (in coordinator/MainWindow)
bwx::gui::bwxReactive::broadcastThemeChange("Dark");  // All controls update!
```

**Available reactive types:**
- `bwx::gui::StaticText` (wxStaticText)
- `bwx::gui::Button` (wxButton)
- `bwx::gui::Choice` (wxChoice)
- `bwx::gui::SpinCtrlDouble` (wxSpinCtrlDouble)
- ... (full list in bwx_managed.h)

**CRITICAL LESSONS (Task #00043):**

⚠️ **DO NOT implement manual font scaling via bwxReactive!**

```cpp
// ❌ WRONG - Fights wxWidgets architecture (Task #00043 removed this)
void onFontScaleChanged(double scale) {
    wxFont font = GetFont();
    font.SetPointSize(static_cast<int>(baseSize * scale));
    SetFont(font);  // wxStaticBoxSizer labels won't resize!
}

// ✅ CORRECT - Use wxWidgets DPI API (future proper approach)
void onDPIChanged(double dpiScale) {
    SetFont(GetFont().Scaled(dpiScale));  // wxFont::Scaled()!
    SetInitialSize(wxDefaultSize);        // Inform sizer!
}
```

**Why font scaling was removed:**
1. wxStaticBoxSizer labels don't resize with manual SetFont()
2. Pixel-based approach fights sizer system
3. Should use dialog units (wxDLG_UNIT), not pixels
4. wxWidgets already has proper DPI API

**Future DPI handling:** Use wxFont::Scaled() + SetInitialSize() instead of manual scaling.

**Forum References:**
- https://forums.wxwidgets.org/viewtopic.php?t=4974 (wxStaticBoxSizer issues)
- https://forums.wxwidgets.org/viewtopic.php?t=41603 (DPI scaling)

### 6. Common Pitfalls

| Problem | Solution |
|---------|----------|
| Memory leaks | Use `new` for wxWidgets controls (parent deletes), smart pointers for non-GUI |
| Blocking main thread | Use wxThread or thread pool, CallAfter() for GUI updates |
| Layout issues | Set wxEXPAND, wxALL flags, call Layout() after changes |
| Cross-platform paths | Use wxFileName, wxStandardPaths, never hardcode `/` or `\\` |
| Manual font scaling | **DON'T!** Use wxFont::Scaled() + wxDLG_UNIT instead |
| wxStaticBoxSizer labels | Don't expect them to resize with control fonts |

### 7. Code Style (Kalahari Convention)

```cpp
namespace kalahari {
namespace gui {

class MainWindow : public wxFrame {
public:
    MainWindow();  // PascalCase for classes

private:
    wxAuiManager m_auiManager;  // m_ prefix for members
    wxRichTextCtrl* m_editor;

    void setupAUI();  // camelCase for methods
    void OnFileNew(wxCommandEvent& event);  // On prefix for event handlers
};

} // namespace gui
} // namespace kalahari
```

## Resources

- **Context7**: ALWAYS use before coding wxWidgets
- **Kalahari Docs**: project_docs/08_gui_design.md
- **Architecture**: project_docs/03_architecture.md (MVP pattern)
- **Official**: https://docs.wxwidgets.org/3.3/

## Most Used Classes

- wxFrame, wxPanel, wxDialog
- wxBoxSizer, wxStaticBoxSizer (ALWAYS use for Kalahari)
- wxAuiManager, wxAuiPaneInfo
- wxRichTextCtrl, wxTreeCtrl
- wxMenu, wxMenuBar, wxToolBar, wxStatusBar
