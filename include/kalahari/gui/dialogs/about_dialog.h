/// @file about_dialog.h
/// @brief About dialog for Kalahari Writer's IDE

#pragma once

#include <QDialog>
#include <QPixmap>

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Custom About dialog displaying application information, third-party components, and license
///
/// AboutDialog provides a professional About dialog with:
/// - Custom banner at top (580×100px placeholder)
/// - 3 tabs: About, Third-Party Components, License
/// - Modal behavior (blocks main window)
/// - Fixed size 600×720px
///
/// Migrated from wxWidgets AboutDialog with feature parity.
///
/// @see MainWindow::onAbout()
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructs About dialog
    /// @param parent Parent widget (typically MainWindow)
    explicit AboutDialog(QWidget* parent = nullptr);

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

    /// @brief Creates placeholder banner image
    /// @param width Banner width in pixels (580px)
    /// @param height Banner height in pixels (100px)
    /// @return Pixmap with black background and white "KALAHARI" text
    QPixmap createPlaceholderBanner(int width, int height);
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
