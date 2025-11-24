/// @file icon_downloader_dialog.cpp
/// @brief Implementation of IconDownloaderDialog

#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/utils/svg_converter.h"
#include "kalahari/core/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QSvgWidget>
#include <QMessageBox>
#include <QDir>
#include <QFile>

using namespace kalahari::gui;
using namespace kalahari::core;

IconDownloaderDialog::IconDownloaderDialog(QWidget* parent)
    : QDialog(parent)
    , m_iconNameEdit(nullptr)
    , m_twotoneCheckBox(nullptr)
    , m_roundedCheckBox(nullptr)
    , m_outlinedCheckBox(nullptr)
    , m_sourceUrlEdit(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_downloadButton(nullptr)
    , m_cancelButton(nullptr)
    , m_errorDisplay(nullptr)
    , m_previewWidget(nullptr)
    // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // , m_downloader(nullptr)
    , m_converter(nullptr)
    , m_totalDownloads(0)
    , m_completedDownloads(0)
    , m_failedDownloads(0)
    , m_isDownloading(false)
{
    setupUi();
    setupConnections();

    // Initialize downloader and converter
    // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // m_downloader = new IconDownloader(IconDownloader::getDefaultSourceUrl(), this);
    m_converter = new SvgConverter(); // SvgConverter doesn't inherit QObject, no parent needed

    Logger::getInstance().debug("IconDownloaderDialog initialized");
}

IconDownloaderDialog::~IconDownloaderDialog() {
    delete m_converter; // Not a QObject, so needs manual cleanup
    // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // m_downloader is deleted by Qt parent-child relationship
}

void IconDownloaderDialog::setupUi() {
    setWindowTitle("Icon Downloader - Material Design Icons");
    setModal(true);
    resize(500, 600);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // === Icon Name Group ===
    QGroupBox* iconGroup = new QGroupBox("Icon Name", this);
    QVBoxLayout* iconLayout = new QVBoxLayout(iconGroup);

    m_iconNameEdit = new QLineEdit(iconGroup);
    m_iconNameEdit->setPlaceholderText("e.g., save, folder_open, search");
    m_iconNameEdit->setToolTip("Enter Material Design icon name (snake_case)");
    iconLayout->addWidget(m_iconNameEdit);

    mainLayout->addWidget(iconGroup);

    // === Themes Group ===
    QGroupBox* themesGroup = new QGroupBox("Icon Themes", this);
    QVBoxLayout* themesLayout = new QVBoxLayout(themesGroup);

    m_twotoneCheckBox = new QCheckBox("TwoTone (default)", themesGroup);
    m_twotoneCheckBox->setChecked(true);
    m_twotoneCheckBox->setToolTip("Material Icons TwoTone variant");
    themesLayout->addWidget(m_twotoneCheckBox);

    m_roundedCheckBox = new QCheckBox("Rounded", themesGroup);
    m_roundedCheckBox->setChecked(true);
    m_roundedCheckBox->setToolTip("Material Icons Rounded variant");
    themesLayout->addWidget(m_roundedCheckBox);

    m_outlinedCheckBox = new QCheckBox("Outlined", themesGroup);
    m_outlinedCheckBox->setChecked(true);
    m_outlinedCheckBox->setToolTip("Material Icons Outlined variant");
    themesLayout->addWidget(m_outlinedCheckBox);

    mainLayout->addWidget(themesGroup);

    // === Source URL Group (optional) ===
    QGroupBox* sourceGroup = new QGroupBox("Source URL (optional)", this);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);

    m_sourceUrlEdit = new QLineEdit(sourceGroup);
    m_sourceUrlEdit->setPlaceholderText("Default: GitHub Raw Material Design Icons");
    m_sourceUrlEdit->setToolTip("Override default Material Design icon source URL");
    sourceLayout->addWidget(m_sourceUrlEdit);

    mainLayout->addWidget(sourceGroup);

    // === Progress Group ===
    QGroupBox* progressGroup = new QGroupBox("Download Progress", this);
    QVBoxLayout* progressLayout = new QVBoxLayout(progressGroup);

    m_statusLabel = new QLabel("Ready to download", progressGroup);
    progressLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);

    mainLayout->addWidget(progressGroup);

    // === Preview Group ===
    QGroupBox* previewGroup = new QGroupBox("Preview (TwoTone)", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    m_previewWidget = new QSvgWidget(previewGroup);
    m_previewWidget->setFixedSize(120, 120);
    m_previewWidget->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    previewLayout->addWidget(m_previewWidget, 0, Qt::AlignCenter);

    mainLayout->addWidget(previewGroup);

    // === Error Display ===
    QLabel* errorLabel = new QLabel("Errors:", this);
    mainLayout->addWidget(errorLabel);

    m_errorDisplay = new QTextEdit(this);
    m_errorDisplay->setReadOnly(true);
    m_errorDisplay->setMaximumHeight(100);
    m_errorDisplay->setPlaceholderText("No errors");
    mainLayout->addWidget(m_errorDisplay);

    // === Buttons ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_downloadButton = new QPushButton("Download", this);
    m_downloadButton->setDefault(true);
    m_downloadButton->setToolTip("Start downloading selected icons (Ctrl+Enter)");
    buttonLayout->addWidget(m_downloadButton);

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setToolTip("Close dialog (Esc)");
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void IconDownloaderDialog::setupConnections() {
    // Button connections
    connect(m_downloadButton, &QPushButton::clicked, this, &IconDownloaderDialog::onDownloadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &IconDownloaderDialog::onCancelClicked);

    // Enter key in icon name field triggers download
    connect(m_iconNameEdit, &QLineEdit::returnPressed, this, &IconDownloaderDialog::onDownloadClicked);
}

void IconDownloaderDialog::onDownloadClicked() {
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

    // Get icon name and themes
    m_currentIconName = m_iconNameEdit->text().trimmed();
    QStringList themes = getSelectedThemes();
    m_totalDownloads = themes.size();

#if 0  // TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
    // Update source URL if provided
    QString sourceUrl = m_sourceUrlEdit->text().trimmed();
    if (!sourceUrl.isEmpty()) {
        m_downloader->setSourceUrl(sourceUrl);
    } else {
        m_downloader->setSourceUrl(IconDownloader::getDefaultSourceUrl());
    }

    // Connect downloader signals (use old-style SIGNAL/SLOT for Windows DLL compatibility)
    connect(m_downloader, SIGNAL(downloadComplete(QString,QString)),
            this, SLOT(onDownloadComplete(QString,QString)), Qt::UniqueConnection);
    connect(m_downloader, SIGNAL(downloadError(QString,QString,QString)),
            this, SLOT(onDownloadError(QString,QString,QString)), Qt::UniqueConnection);
    connect(m_downloader, SIGNAL(progress(int,int,QString)),
            this, SLOT(onDownloadProgress(int,int,QString)), Qt::UniqueConnection);

    // Update UI state
    m_isDownloading = true;
    m_downloadButton->setEnabled(false);
    m_statusLabel->setText(QString("Downloading '%1'...").arg(m_currentIconName));

    Logger::getInstance().info("IconDownloaderDialog: Starting download of '{}' in {} themes",
                               m_currentIconName.toStdString(), themes.size());

    // Start download
    m_downloader->downloadIcon(m_currentIconName, themes);
#endif  // IconDownloader disabled
}

void IconDownloaderDialog::onCancelClicked() {
    if (m_isDownloading) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Cancel Download",
            "Download is in progress. Are you sure you want to cancel?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::No) {
            return;
        }
    }

    Logger::getInstance().info("IconDownloaderDialog: Closing dialog");
    reject(); // Close dialog with Rejected status
}

void IconDownloaderDialog::onDownloadComplete(const QString& theme, const QString& svgData) {
    Logger::getInstance().info("IconDownloaderDialog: Download complete for theme '{}'",
                               theme.toStdString());

    // Convert SVG
    auto conversionResult = m_converter->convertToTemplate(svgData);
    if (!conversionResult.success) {
        addError(QString("Conversion failed (%1): %2").arg(theme, conversionResult.errorMessage));
        m_failedDownloads++;
        checkDownloadComplete();
        return;
    }

    // Save to file
    QString dirPath = QString("resources/icons/%1").arg(theme);
    QDir dir;
    if (!dir.exists(dirPath)) {
        if (!dir.mkpath(dirPath)) {
            addError(QString("Failed to create directory: %1").arg(dirPath));
            m_failedDownloads++;
            checkDownloadComplete();
            return;
        }
    }

    QString filePath = QString("resources/icons/%1/%2.svg").arg(theme, m_currentIconName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        addError(QString("Failed to write file: %1").arg(filePath));
        m_failedDownloads++;
        checkDownloadComplete();
        return;
    }

    file.write(conversionResult.svg.toUtf8());
    file.close();

    Logger::getInstance().info("IconDownloaderDialog: Saved {}", filePath.toStdString());

    // Update preview with first downloaded icon (TwoTone)
    if (theme == "twotone" && m_previewWidget) {
        updatePreview(conversionResult.svg);
    }

    m_completedDownloads++;
    checkDownloadComplete();
}

void IconDownloaderDialog::onDownloadError(const QString& iconName, const QString& theme,
                                          const QString& errorMessage) {
    Logger::getInstance().error("IconDownloaderDialog: Download error for '{}' ({}): {}",
                                iconName.toStdString(), theme.toStdString(),
                                errorMessage.toStdString());

    addError(QString("%1 (%2): %3").arg(iconName, theme, errorMessage));
    m_failedDownloads++;
    checkDownloadComplete();
}

void IconDownloaderDialog::onDownloadProgress(int current, int total, const QString& iconName) {
    int percentage = (total > 0) ? (current * 100 / total) : 0;
    m_progressBar->setValue(percentage);
    m_statusLabel->setText(QString("Downloading '%1' (%2/%3)").arg(iconName).arg(current).arg(total));

    Logger::getInstance().debug("IconDownloaderDialog: Progress {}/{} ({}%)",
                                current, total, percentage);
}

void IconDownloaderDialog::checkDownloadComplete() {
    int totalProcessed = m_completedDownloads + m_failedDownloads;

    if (totalProcessed >= m_totalDownloads) {
        // All downloads complete
        m_isDownloading = false;
        m_downloadButton->setEnabled(true);
        m_progressBar->setValue(100);

        if (m_failedDownloads == 0) {
            m_statusLabel->setText(QString("✓ Downloaded %1 theme(s) successfully")
                                  .arg(m_completedDownloads));
            Logger::getInstance().info("IconDownloaderDialog: All downloads successful ({} themes)",
                                       m_completedDownloads);
        } else {
            m_statusLabel->setText(QString("Downloaded %1/%2 themes (%3 failed)")
                                  .arg(m_completedDownloads)
                                  .arg(m_totalDownloads)
                                  .arg(m_failedDownloads));
            Logger::getInstance().warn("IconDownloaderDialog: Downloads complete with {} failures",
                                       m_failedDownloads);
        }
    }
}

bool IconDownloaderDialog::validateInput() {
    QString iconName = m_iconNameEdit->text().trimmed();

    if (iconName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter an icon name.");
        m_iconNameEdit->setFocus();
        return false;
    }

    QStringList themes = getSelectedThemes();
    if (themes.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please select at least one theme.");
        return false;
    }

    return true;
}

QStringList IconDownloaderDialog::getSelectedThemes() const {
    QStringList themes;

    if (m_twotoneCheckBox->isChecked()) {
        themes << "twotone";
    }
    if (m_roundedCheckBox->isChecked()) {
        themes << "rounded";
    }
    if (m_outlinedCheckBox->isChecked()) {
        themes << "outlined";
    }

    return themes;
}

void IconDownloaderDialog::updatePreview(const QString& svgData) {
    if (!m_previewWidget) {
        return;
    }

    // Replace color placeholders with actual colors for preview
    QString previewSvg = svgData;
    previewSvg.replace("{COLOR_PRIMARY}", "#333333");   // Dark gray for primary
    previewSvg.replace("{COLOR_SECONDARY}", "#999999"); // Light gray for secondary

    // Load SVG into widget
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

    // Scroll to bottom
    QTextCursor cursor = m_errorDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_errorDisplay->setTextCursor(cursor);
}
