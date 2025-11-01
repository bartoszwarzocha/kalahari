/// @file about_dialog.cpp
/// @brief Implementation of AboutDialog

#include "kalahari/gui/dialogs/about_dialog.h"
#include <wx/statbox.h>
#include <wx/dcmemory.h>

using namespace kalahari::gui;

// ============================================================================
// Constructor
// ============================================================================

AboutDialog::AboutDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "About Kalahari Writer's IDE",
               wxDefaultPosition, wxSize(600, 720),  // Increased height: 520 → 720px
               wxDEFAULT_DIALOG_STYLE)  // Fixed size, not resizable
{
    // Main sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Banner at top (ABOVE notebook, common for all tabs)
    wxBitmap banner = createPlaceholderBanner(580, 100);
    wxStaticBitmap* bannerCtrl = new wxStaticBitmap(this, wxID_ANY, banner);
    mainSizer->Add(bannerCtrl, 0, wxALL | wxEXPAND, 10);

    // Create notebook (below banner)
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    // Add tabs (now without banner inside)
    notebook->AddPage(createMainPanel(notebook), "Kalahari");
    notebook->AddPage(createThirdPartyPanel(notebook), "Third-Party Components");
    notebook->AddPage(createLicensePanel(notebook), "License");
    notebook->AddPage(createCreditsPanel(notebook), "Credits");

    mainSizer->Add(notebook, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    // Close button
    wxButton* closeBtn = new wxButton(this, wxID_CLOSE, "Close");
    closeBtn->Bind(wxEVT_BUTTON, &AboutDialog::onClose, this);
    mainSizer->Add(closeBtn, 0, wxALIGN_RIGHT | wxALL, 10);

    SetSizer(mainSizer);
    Centre();
}

// ============================================================================
// Panel Creation
// ============================================================================

wxPanel* AboutDialog::createMainPanel(wxNotebook* notebook) {
    wxPanel* panel = new wxPanel(notebook);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Application name and version (no banner here - it's above notebook now)
    wxStaticText* appName = new wxStaticText(panel, wxID_ANY,
        "Kalahari Writer's IDE 0.1.0 (ALPHA)");
    wxFont nameFont = appName->GetFont();
    nameFont.SetPointSize(14);
    nameFont.SetWeight(wxFONTWEIGHT_BOLD);
    appName->SetFont(nameFont);
    sizer->Add(appName, 0, wxALL | wxALIGN_CENTER, 10);

    // Platform info
    wxStaticText* platform = new wxStaticText(panel, wxID_ANY,
        "Cross-platform Writer's IDE for Windows, macOS, and Linux");
    sizer->Add(platform, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER, 10);

    // Description
    wxStaticText* desc = new wxStaticText(panel, wxID_ANY,
        "Kalahari is a modern writing environment designed for book authors.\n"
        "Built with C++20 and wxWidgets 3.3+.\n\n"
        "Phase 0 Foundation - Icon System Implementation\n\n"
        "A comprehensive writing toolkit with project management,\n"
        "statistics tracking, and powerful export capabilities.");
    sizer->Add(desc, 1, wxALL | wxEXPAND, 10);

    // Copyright
    wxStaticText* copyright = new wxStaticText(panel, wxID_ANY,
        "Copyright (c) 2025 Kalahari Project");
    wxFont copyrightFont = copyright->GetFont();
    copyrightFont.SetPointSize(8);
    copyright->SetFont(copyrightFont);
    sizer->Add(copyright, 0, wxALL | wxALIGN_CENTER, 10);

    panel->SetSizer(sizer);
    return panel;
}

wxPanel* AboutDialog::createThirdPartyPanel(wxNotebook* notebook) {
    wxPanel* panel = new wxPanel(notebook);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Read-only text control with component list
    wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);

    // Build component list
    wxString components;
    components << "Kalahari uses the following third-party components:\n\n";

    components << "wxWidgets 3.3.0+ (www.wxwidgets.org)\n";
    components << "  Cross-platform GUI library\n";
    components << "  License: wxWindows Library Licence\n\n";

    components << "Material Design Icons (github.com/google/material-design-icons)\n";
    components << "  Icon set by Google\n";
    components << "  License: Apache License 2.0\n";
    components << "  Used for application UI icons (toolbar, menu, dialogs)\n\n";

    components << "spdlog (github.com/gabime/spdlog)\n";
    components << "  Fast C++ logging library\n";
    components << "  License: MIT License\n\n";

    components << "nlohmann_json (github.com/nlohmann/json)\n";
    components << "  JSON for Modern C++\n";
    components << "  License: MIT License\n\n";

    components << "libzip (libzip.org)\n";
    components << "  C library for reading, creating, and modifying zip archives\n";
    components << "  License: BSD 3-Clause License\n\n";

    components << "Catch2 (github.com/catchorg/Catch2)\n";
    components << "  Modern C++ test framework\n";
    components << "  License: Boost Software License 1.0\n\n";

    components << "pybind11 (github.com/pybind/pybind11)\n";
    components << "  Seamless C++/Python interoperability\n";
    components << "  License: BSD 3-Clause License\n\n";

    components << "Python 3.11 (www.python.org)\n";
    components << "  Embedded Python interpreter for plugins\n";
    components << "  License: Python Software Foundation License\n\n";

    components << "vcpkg (github.com/microsoft/vcpkg)\n";
    components << "  C++ package manager\n";
    components << "  License: MIT License\n";

    text->SetValue(components);
    sizer->Add(text, 1, wxALL | wxEXPAND, 10);

    panel->SetSizer(sizer);
    return panel;
}

wxPanel* AboutDialog::createLicensePanel(wxNotebook* notebook) {
    wxPanel* panel = new wxPanel(notebook);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Read-only text control with MIT license
    wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);

    wxString license;
    license << "MIT License\n\n";
    license << "Copyright (c) 2025 Kalahari Project\n\n";
    license << "Permission is hereby granted, free of charge, to any person obtaining a copy\n";
    license << "of this software and associated documentation files (the \"Software\"), to deal\n";
    license << "in the Software without restriction, including without limitation the rights\n";
    license << "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n";
    license << "copies of the Software, and to permit persons to whom the Software is\n";
    license << "furnished to do so, subject to the following conditions:\n\n";
    license << "The above copyright notice and this permission notice shall be included in all\n";
    license << "copies or substantial portions of the Software.\n\n";
    license << "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n";
    license << "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n";
    license << "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n";
    license << "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n";
    license << "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n";
    license << "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n";
    license << "SOFTWARE.\n\n";
    license << "Note: The \"Kalahari\" name and branding are trademarked.";

    text->SetValue(license);
    sizer->Add(text, 1, wxALL | wxEXPAND, 10);

    panel->SetSizer(sizer);
    return panel;
}

wxPanel* AboutDialog::createCreditsPanel(wxNotebook* notebook) {
    wxPanel* panel = new wxPanel(notebook);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Read-only text control with credits
    wxTextCtrl* text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);

    wxString credits;
    credits << "Kalahari Writer's IDE - Development Team\n\n";
    credits << "Project Vision & Architecture:\n";
    credits << "  Collaborative development between human vision and AI execution\n\n";
    credits << "Technology Stack:\n";
    credits << "  C++20, wxWidgets 3.3+, CMake, vcpkg\n\n";
    credits << "Development Phase:\n";
    credits << "  Phase 0 - Foundation (Weeks 1-8)\n";
    credits << "  Current: Icon System Implementation\n\n";
    credits << "Special Thanks:\n";
    credits << "  - wxWidgets team for excellent cross-platform GUI framework\n";
    credits << "  - Google for Material Design Icons\n";
    credits << "  - Open source community for exceptional libraries\n\n";
    credits << "Project Timeline:\n";
    credits << "  Start: 2025-01\n";
    credits << "  Target Release: Q2-Q3 2026 (Kalahari 1.0)\n";

    text->SetValue(credits);
    sizer->Add(text, 1, wxALL | wxEXPAND, 10);

    panel->SetSizer(sizer);
    return panel;
}

// ============================================================================
// Helper Methods
// ============================================================================

wxBitmap AboutDialog::createPlaceholderBanner(int width, int height) {
    wxBitmap bitmap(width, height);
    wxMemoryDC dc(bitmap);

    // Fill with black
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();

    // Draw placeholder text
    dc.SetTextForeground(*wxWHITE);
    wxFont font = dc.GetFont();
    font.SetPointSize(24);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    dc.SetFont(font);

    wxString text = "KALAHARI";
    wxSize textSize = dc.GetTextExtent(text);
    int x = (width - textSize.GetWidth()) / 2;
    int y = (height - textSize.GetHeight()) / 2;
    dc.DrawText(text, x, y);

    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

// ============================================================================
// Event Handlers
// ============================================================================

void AboutDialog::onClose([[maybe_unused]] wxCommandEvent& event) {
    EndModal(wxID_CLOSE);
}
