/// @file icon_downloader_dialog.h
/// @brief Icon Downloader Dialog - GUI for downloading Material Design icons
///
/// Task #00020: Icon Downloader Tool
/// Section 2: GUI IconDownloaderDialog

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLineEdit;
class QCheckBox;
class QProgressBar;
class QLabel;
class QPushButton;
class QTextEdit;
class QSvgWidget;

namespace kalahari {
namespace core {
    // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // class IconDownloader;
    class SvgConverter;
}

namespace gui {

/// @brief Dialog for downloading Material Design icons with preview
class IconDownloaderDialog : public QDialog {
    Q_OBJECT

public:
    explicit IconDownloaderDialog(QWidget* parent = nullptr);
    ~IconDownloaderDialog() override;

private slots:
    /// @brief Start icon download
    void onDownloadClicked();

    /// @brief Cancel ongoing download
    void onCancelClicked();

    /// @brief Handle download completion
    void onDownloadComplete(const QString& theme, const QString& svgData);

    /// @brief Handle download error
    void onDownloadError(const QString& iconName, const QString& theme, const QString& errorMessage);

    /// @brief Handle download progress
    void onDownloadProgress(int current, int total, const QString& iconName);

private:
    /// @brief Setup UI widgets
    void setupUi();

    /// @brief Setup signal/slot connections
    void setupConnections();

    /// @brief Validate input before download
    bool validateInput();

    /// @brief Get selected themes from checkboxes
    QStringList getSelectedThemes() const;

    /// @brief Update preview with downloaded SVG
    void updatePreview(const QString& svgData);

    /// @brief Clear error display
    void clearErrors();

    /// @brief Add error message to display
    void addError(const QString& error);

    /// @brief Check if all downloads are complete and update UI
    void checkDownloadComplete();

    // UI Widgets
    QLineEdit* m_iconNameEdit;
    QCheckBox* m_twotoneCheckBox;
    QCheckBox* m_roundedCheckBox;
    QCheckBox* m_outlinedCheckBox;
    QLineEdit* m_sourceUrlEdit;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_downloadButton;
    QPushButton* m_cancelButton;
    QTextEdit* m_errorDisplay;
    QSvgWidget* m_previewWidget;

    // Core components
    // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // core::IconDownloader* m_downloader;
    core::SvgConverter* m_converter;

    // State
    QString m_currentIconName;
    int m_totalDownloads;
    int m_completedDownloads;
    int m_failedDownloads;
    bool m_isDownloading;
};

} // namespace gui
} // namespace kalahari
