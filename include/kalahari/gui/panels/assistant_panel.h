/// @file assistant_panel.h
/// @brief AI Assistant panel (wxAUI panel)
///
/// Interface for AI writing assistant (Lion, Meerkat, Elephant, Cheetah).
/// Stub implementation - full features in Phase 2.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Assistant panel for AI writing help
///
/// Chat interface with AI assistant animals.
/// This is a stub implementation - full AI integration in Phase 2.
class AssistantPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit AssistantPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~AssistantPanel() = default;

private:
    /// @brief Chat history display
    wxTextCtrl* m_chatDisplay = nullptr;

    /// @brief User input field
    wxTextCtrl* m_inputCtrl = nullptr;

    /// @brief Setup the panel layout
    void setupLayout();

    /// @brief Handle send button click
    void onSend(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
