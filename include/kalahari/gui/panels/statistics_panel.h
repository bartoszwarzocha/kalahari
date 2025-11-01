/// @file statistics_panel.h
/// @brief Statistics panel for writing metrics (wxAUI panel)
///
/// Displays word count, character count, reading time, etc.
/// Stub implementation - full features in Phase 1.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Statistics panel showing writing metrics
///
/// Displays real-time statistics about the current document.
/// This is a stub implementation - full charts/analytics in later tasks.
class StatisticsPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit StatisticsPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~StatisticsPanel() = default;

private:
    /// @brief Setup the panel layout
    void setupLayout();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
