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

### 5. Common Pitfalls

| Problem | Solution |
|---------|----------|
| Memory leaks | Use `new` for wxWidgets controls (parent deletes), smart pointers for non-GUI |
| Blocking main thread | Use wxThread or thread pool, CallAfter() for GUI updates |
| Layout issues | Set wxEXPAND, wxALL flags, call Layout() after changes |
| Cross-platform paths | Use wxFileName, wxStandardPaths, never hardcode `/` or `\\` |

### 6. Code Style (Kalahari Convention)

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
