/// @file about_dialog.h
/// @brief About dialog for Kalahari Writer's IDE

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QTabWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief About: the version of the program, its authors, the third-party components and the
/// license
///
/// The heading of the program's own dialogs names the program, its version and the version of
/// Qt it runs with; under it three tabs: About, Third-Party Components and License. Close (or
/// Esc) closes the dialog.
///
/// @see MainWindow::onAbout()
class AboutDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Constructs About dialog
    /// @param parent Parent widget (typically MainWindow)
    explicit AboutDialog(QWidget* parent = nullptr);

    /// @brief The tabs: About, Third-Party Components, License
    QTabWidget* tabs() const { return m_tabs; }

private:
    /// @brief Creates "About" tab content
    /// @return Widget containing application info and credits
    QWidget* createAboutTab();

    /// @brief Creates "Third-Party Components" tab content
    /// @return Widget containing component attribution list
    QWidget* createComponentsTab();

    /// @brief Creates "License" tab content
    /// @return Widget containing full MIT license text
    QWidget* createLicenseTab();

    QTabWidget* m_tabs = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
