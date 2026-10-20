/// @file icon_downloader_dialog.cpp
/// @brief Implementation of IconDownloaderDialog

#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/utils/svg_converter.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QSvgWidget>
#include <QDir>
#include <QFile>

using namespace kalahari::gui;
using namespace kalahari::core;

IconDownloaderDialog::IconDownloaderDialog(QWidget* parent)
    : KalahariDialog(parent)
    , m_iconNameEdit(nullptr)
    , m_twotoneCheckBox(nullptr)
    , m_roundedCheckBox(nullptr)
    , m_outlinedCheckBox(nullptr)
    , m_sourceUrlEdit(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_errorDisplay(nullptr)
    , m_previewWidget(nullptr)
    , m_downloader(nullptr)
    , m_converter(nullptr)
    , m_totalDownloads(0)
    , m_completedDownloads(0)
    , m_failedDownloads(0)
    , m_isDownloading(false)
{
    setupUi();

    // Initialize downloader and converter
    m_downloader = new IconDownloader(this);
    m_converter = new SvgConverter();

    Logger::getInstance().debug("IconDownloaderDialog initialized");
}

IconDownloaderDialog::~IconDownloaderDialog() {
    delete m_converter;
}

void IconDownloaderDialog::setupUi() {
    setHeading(tr("Icon Downloader"),
               //: The heading of the icon downloader, a tool of the developers. In Polish:
               //: Pobiera ikonę SVG z podanego adresu i zapisuje ją wśród ikon wybranego stylu.
               tr("Downloads an SVG icon from its URL and saves it with the icons of a theme."));
    setHeadingIcon(QStringLiteral("common.download"));
    setModal(true);
    // Higher by the heading, or less on a smaller screen
    kalahari::gui::utils::resizeWithinScreen(this, withHeading(QSize(600, 650)));

    // The settings scroll on a small screen (e.g. 1366x768 at 150%); the buttons stay
    QVBoxLayout* mainLayout = contentLayout();

    // === URL Input Group ===
    QGroupBox* urlGroup = new QGroupBox(tr("Icon URL"), this);
    QVBoxLayout* urlLayout = new QVBoxLayout(urlGroup);

    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    QLabel* urlHint = new QLabel(
        tr("Enter full URL to SVG icon. Example:") + QStringLiteral("\n"
        "https://raw.githubusercontent.com/google/material-design-icons/master/src/content/save/materialiconstwotone/24px.svg"),
        urlGroup);
    urlHint->setWordWrap(true);
    // The color of the notes: readable in the group
    urlHint->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(SettingsPage::noteColor().name()));
    urlLayout->addWidget(urlHint);

    m_sourceUrlEdit = new QLineEdit(urlGroup);
    m_sourceUrlEdit->setPlaceholderText("https://...");
    m_sourceUrlEdit->setToolTip(tr("Full URL to SVG file"));
    urlLayout->addWidget(m_sourceUrlEdit);

    mainLayout->addWidget(urlGroup);

    // === Output Settings Group ===
    QGroupBox* outputGroup = new QGroupBox(tr("Output Settings"), this);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Icon name:"), outputGroup));
    m_iconNameEdit = new QLineEdit(outputGroup);
    m_iconNameEdit->setPlaceholderText(tr("e.g., save, folder_open"));
    m_iconNameEdit->setToolTip(tr("Name for saved file (without .svg)"));
    nameLayout->addWidget(m_iconNameEdit);
    outputLayout->addLayout(nameLayout);

    QHBoxLayout* themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel(tr("Save to theme:"), outputGroup));
    m_themeCombo = new QComboBox(outputGroup);
    m_themeCombo->addItems({"twotone", "rounded", "outlined"});
    m_themeCombo->setToolTip(tr("Target theme directory"));
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addStretch();
    outputLayout->addLayout(themeLayout);

    mainLayout->addWidget(outputGroup);

    // === Progress Group ===
    QGroupBox* progressGroup = new QGroupBox(tr("Download Progress"), this);
    QVBoxLayout* progressLayout = new QVBoxLayout(progressGroup);

    m_statusLabel = new QLabel(tr("Ready to download"), progressGroup);
    progressLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);

    mainLayout->addWidget(progressGroup);

    // === Preview Group ===
    QGroupBox* previewGroup = new QGroupBox(tr("Preview"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    m_previewWidget = new QSvgWidget(previewGroup);
    m_previewWidget->setFixedSize(120, 120);
    // Use theme-aware colors for preview background and border
    m_previewWidget->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
        .arg(theme.palette.base.name()).arg(theme.palette.mid.name()));
    previewLayout->addWidget(m_previewWidget, 0, Qt::AlignCenter);

    mainLayout->addWidget(previewGroup);

    // === Error Display ===
    QLabel* errorLabel = new QLabel(tr("Errors:"), this);
    mainLayout->addWidget(errorLabel);

    m_errorDisplay = new QTextEdit(this);
    m_errorDisplay->setReadOnly(true);
    m_errorDisplay->setMaximumHeight(100);
    m_errorDisplay->setPlaceholderText(tr("No errors"));
    mainLayout->addWidget(m_errorDisplay);
    mainLayout->addStretch(1);

    // === Buttons ===
    // Download (also Enter in a field) keeps the dialog open; Close closes it
    setAcceptText(tr("Download"));
    cancelButton()->setText(tr("Close"));
}

void IconDownloaderDialog::accept() {
    if (m_isDownloading) {
        Logger::getInstance().warn("IconDownloaderDialog: Download already in progress");
        return;
    }

    if (!validateInput()) {
        return;
    }

    // Clear previous state
    clearErrors();
    m_completedDownloads = 0;
    m_failedDownloads = 0;
    m_progressBar->setValue(0);

    // Get input values
    QString url = m_sourceUrlEdit->text().trimmed();
    m_currentIconName = m_iconNameEdit->text().trimmed();
    QString theme = m_themeCombo->currentText();
    m_totalDownloads = 1;

    // Connect downloader signals
    connect(m_downloader, &IconDownloader::downloadComplete,
            this, &IconDownloaderDialog::onDownloadComplete, Qt::UniqueConnection);
    connect(m_downloader, &IconDownloader::downloadError,
            this, &IconDownloaderDialog::onDownloadError, Qt::UniqueConnection);

    // Update UI state
    m_isDownloading = true;
    acceptButton()->setEnabled(false);
    m_statusLabel->setText(tr("Downloading..."));

    Logger::getInstance().info("IconDownloaderDialog: Downloading from {}", url.toStdString());

    // Start download
    m_downloader->downloadFromUrl(url, theme);
}

void IconDownloaderDialog::reject() {
    if (m_isDownloading) {
        const bool stop = dialogs::MessageDialog::confirm(this, tr("Cancel Download"),
            tr("The download is in progress. Do you want to stop it?"),
            tr("&Stop Download"));

        if (!stop) {
            return;
        }
    }

    Logger::getInstance().info("IconDownloaderDialog: Closing dialog");
    KalahariDialog::reject();
}

void IconDownloaderDialog::onDownloadComplete(const QString& theme, const QString& svgData) {
    Logger::getInstance().info("IconDownloaderDialog: Download complete for theme '{}'",
                               theme.toStdString());

    // Convert SVG to template format
    auto conversionResult = m_converter->convertToTemplate(svgData);
    if (!conversionResult.success) {
        addError(tr("Conversion failed: %1").arg(conversionResult.errorMessage));
        m_failedDownloads++;
        checkDownloadComplete();
        return;
    }

    // Save to file
    QString dirPath = QString("resources/icons/%1").arg(theme);
    QDir dir;
    if (!dir.exists(dirPath)) {
        if (!dir.mkpath(dirPath)) {
            addError(tr("Failed to create directory: %1").arg(dirPath));
            m_failedDownloads++;
            checkDownloadComplete();
            return;
        }
    }

    QString filePath = QString("resources/icons/%1/%2.svg").arg(theme, m_currentIconName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        addError(tr("Failed to write file: %1").arg(filePath));
        m_failedDownloads++;
        checkDownloadComplete();
        return;
    }

    file.write(conversionResult.svg.toUtf8());
    file.close();

    Logger::getInstance().info("IconDownloaderDialog: Saved {}", filePath.toStdString());

    // Update preview
    updatePreview(conversionResult.svg);

    m_completedDownloads++;
    checkDownloadComplete();
}

void IconDownloaderDialog::onDownloadError(const QString& /*url*/, const QString& errorMessage) {
    Logger::getInstance().error("IconDownloaderDialog: Download error: {}", errorMessage.toStdString());

    addError(errorMessage);
    m_failedDownloads++;
    checkDownloadComplete();
}

void IconDownloaderDialog::onDownloadProgress(int current, int total, const QString& /*url*/) {
    int percentage = (total > 0) ? (current * 100 / total) : 0;
    m_progressBar->setValue(percentage);
    m_statusLabel->setText(tr("Downloading... (%1/%2)").arg(current).arg(total));
}

void IconDownloaderDialog::checkDownloadComplete() {
    int totalProcessed = m_completedDownloads + m_failedDownloads;

    if (totalProcessed >= m_totalDownloads) {
        m_isDownloading = false;
        acceptButton()->setEnabled(true);
        m_progressBar->setValue(100);

        if (m_failedDownloads == 0) {
            m_statusLabel->setText(QStringLiteral("✓ ") + tr("Downloaded and saved successfully"));
            Logger::getInstance().info("IconDownloaderDialog: Download successful");
        } else {
            m_statusLabel->setText(QStringLiteral("✗ ") + tr("Download failed"));
            Logger::getInstance().warn("IconDownloaderDialog: Download failed");
        }
    }
}

bool IconDownloaderDialog::validateInput() {
    QString url = m_sourceUrlEdit->text().trimmed();
    QString iconName = m_iconNameEdit->text().trimmed();

    if (url.isEmpty()) {
        dialogs::MessageDialog::warning(this, tr("Invalid Input"), tr("Please enter a URL."));
        m_sourceUrlEdit->setFocus();
        return false;
    }

    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        dialogs::MessageDialog::warning(this, tr("Invalid Input"),
                                        tr("The URL must start with http:// or https://."));
        m_sourceUrlEdit->setFocus();
        return false;
    }

    if (iconName.isEmpty()) {
        dialogs::MessageDialog::warning(this, tr("Invalid Input"), tr("Please enter an icon name."));
        m_iconNameEdit->setFocus();
        return false;
    }

    return true;
}

QStringList IconDownloaderDialog::getSelectedThemes() const {
    QStringList themes;
    themes << m_themeCombo->currentText();
    return themes;
}

void IconDownloaderDialog::updatePreview(const QString& svgData) {
    if (!m_previewWidget) {
        return;
    }

    // Replace placeholders with the colors of the icons
    const auto& art = ArtProvider::getInstance();
    QString previewSvg = svgData;
    previewSvg.replace("{COLOR_PRIMARY}", art.getPrimaryColor().name());
    previewSvg.replace("{COLOR_SECONDARY}", art.getSecondaryColor().name());

    m_previewWidget->load(previewSvg.toUtf8());

    Logger::getInstance().debug("IconDownloaderDialog: Preview updated");
}

void IconDownloaderDialog::clearErrors() {
    m_errorDisplay->clear();
}

void IconDownloaderDialog::addError(const QString& error) {
    QString currentText = m_errorDisplay->toPlainText();
    if (!currentText.isEmpty()) {
        currentText += "\n";
    }
    currentText += "• " + error;
    m_errorDisplay->setPlainText(currentText);

    QTextCursor cursor = m_errorDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_errorDisplay->setTextCursor(cursor);
}
