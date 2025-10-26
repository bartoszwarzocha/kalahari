/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point
///
/// This file contains the wxWidgets application entry point.
/// The wxIMPLEMENT_APP macro creates the appropriate main() or WinMain()
/// function for the current platform.

#include "gui/kalahari_app.h"

// Macro creates platform-specific entry point:
// - Windows: WinMain() for GUI subsystem
// - Unix/Linux: main()
// - macOS: main() with NSApplicationMain setup
wxIMPLEMENT_APP(kalahari::gui::KalahariApp);
