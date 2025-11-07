/// @file files_tab.cpp
/// @brief Implementation of FilesTab (Task #00020 - Phase 1 PLACEHOLDER)

#include "kalahari/gui/panels/files_tab.h"

namespace kalahari {
namespace gui {

FilesTab::FilesTab(wxWindow* parent)
    : wxPanel(parent)
{
    // Gray background to distinguish placeholder
    SetBackgroundColour(wxColour(240, 240, 240));

    // Main sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Centered placeholder text
    m_placeholder = new wxStaticText(this, wxID_ANY,
        "Files Browser\n\n"
        "Coming in Phase 2\n\n"
        "Will display:\n"
        "- Project file structure\n"
        "- .klh contents (chapters, media)\n"
        "- Context menu operations",
        wxDefaultPosition, wxDefaultSize,
        wxALIGN_CENTRE_HORIZONTAL);

    // Slightly larger font
    wxFont font = m_placeholder->GetFont();
    font.SetPointSize(font.GetPointSize() + 1);
    m_placeholder->SetFont(font);

    // Gray text color
    m_placeholder->SetForegroundColour(wxColour(128, 128, 128));

    mainSizer->AddStretchSpacer(1);
    mainSizer->Add(m_placeholder, 0, wxALIGN_CENTER | wxALL, 20);
    mainSizer->AddStretchSpacer(1);

    SetSizer(mainSizer);
}

} // namespace gui
} // namespace kalahari
