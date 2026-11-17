// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file icon_downloader_dialog.h
/// @brief Icon Downloader Dialog - GUI for downloading SVG icons from URL

#ifndef KALAHARI_GUI_DIALOGS_ICON_DOWNLOADER_DIALOG_H
#define KALAHARI_GUI_DIALOGS_ICON_DOWNLOADER_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>
#include <QStringList>

class QLineEdit;
class QCheckBox;
class QComboBox;
class QProgressBar;
class QLabel;
class QTextEdit;
class QSvgWidget;

namespace kalahari {
namespace core {
    class IconDownloader;
    class SvgConverter;
} // namespace core

namespace gui {

/// @brief Dialog for downloading SVG icons from URL with preview
///
/// One of the program's own dialogs: Download (Enter) downloads the icon and keeps the dialog
/// open; Close (Esc) asks first while a download is in progress.
class IconDownloaderDialog : public dialogs::KalahariDialog {
    Q_OBJECT

public:
    explicit IconDownloaderDialog(QWidget* parent = nullptr);
    ~IconDownloaderDialog() override;

    /// @brief Download: start the download of the icon; the dialog stays open
    void accept() override;

    /// @brief Close, Esc and the close button of the window: while a download is in progress,
    /// only after the user agrees to stop it
    void reject() override;

private slots:
    /// @brief Handle download completion
    void onDownloadComplete(const QString& theme, const QString& svgData);

    /// @brief Handle download error
    void onDownloadError(const QString& url, const QString& errorMessage);

    /// @brief Handle download progress
    void onDownloadProgress(int current, int total, const QString& url);

private:
    /// @brief Setup UI widgets
    void setupUi();

    /// @brief Validate input before download
    bool validateInput();

    /// @brief Get selected themes
    [[nodiscard]] QStringList getSelectedThemes() const;

    /// @brief Update preview with downloaded SVG
    void updatePreview(const QString& svgData);

    /// @brief Clear error display
    void clearErrors();

    /// @brief Add error message to display
    void addError(const QString& error);

    /// @brief Check if all downloads are complete and update UI
    void checkDownloadComplete();

    // UI Widgets
    QLineEdit* m_iconNameEdit = nullptr;
    QCheckBox* m_twotoneCheckBox = nullptr;
    QCheckBox* m_roundedCheckBox = nullptr;
    QCheckBox* m_outlinedCheckBox = nullptr;
    QLineEdit* m_sourceUrlEdit = nullptr;
    QComboBox* m_themeCombo;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTextEdit* m_errorDisplay = nullptr;
    QSvgWidget* m_previewWidget = nullptr;

    // Core components
    core::IconDownloader* m_downloader = nullptr;
    core::SvgConverter* m_converter = nullptr;

    // State
    QString m_currentIconName;
    int m_totalDownloads = 0;
    int m_completedDownloads = 0;
    int m_failedDownloads = 0;
    bool m_isDownloading = false;
};

} // namespace gui
} // namespace kalahari

#endif  // KALAHARI_GUI_DIALOGS_ICON_DOWNLOADER_DIALOG_H
