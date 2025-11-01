/// @file properties_panel.h
/// @brief Properties panel for chapter/document metadata (wxAUI panel)
///
/// Displays and edits metadata (title, tags, notes, etc.).
/// Stub implementation - full features in Phase 1.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Properties panel for metadata editing
///
/// Shows and edits metadata for selected chapter/document.
/// This is a stub implementation - full wxPropertyGrid in later tasks.
class PropertiesPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit PropertiesPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~PropertiesPanel() = default;

private:
    /// @brief Setup the panel layout
    void setupLayout();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
