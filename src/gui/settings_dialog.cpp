/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QFontComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QColorDialog>

namespace kalahari {
namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent, bool diagnosticModeEnabled)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_appearanceTab(nullptr)
    , m_editorTab(nullptr)
    , m_advancedTab(nullptr)
    , m_themeComboBox(nullptr)
    , m_languageComboBox(nullptr)
    , m_fontSizeSpinBox(nullptr)
    , m_primaryColorButton(nullptr)
    , m_secondaryColorButton(nullptr)
    , m_fontFamilyComboBox(nullptr)
    , m_editorFontSizeSpinBox(nullptr)
    , m_tabSizeSpinBox(nullptr)
    , m_lineNumbersCheckBox(nullptr)
    , m_wordWrapCheckBox(nullptr)
    , m_diagModeCheckbox(nullptr)
    , m_initialDiagMode(diagnosticModeEnabled)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Constructor called");

    setWindowTitle(tr("Settings"));
    setModal(true);  // Block main window when open
    resize(600, 400);  // Default size

    createUI();
    loadSettings();

    logger.debug("SettingsDialog: Initialized successfully");
}

void SettingsDialog::createUI() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Creating UI");

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create tab widget
    m_tabWidget = new QTabWidget(this);

    // Appearance tab
    m_appearanceTab = new QWidget();
    QVBoxLayout* appearanceLayout = new QVBoxLayout(m_appearanceTab);

    // Create group box for visual grouping
    QGroupBox* appearanceGroup = new QGroupBox(tr("Appearance Settings"));
    QGridLayout* gridLayout = new QGridLayout();

    // Theme selection (row 0)
    QLabel* themeLabel = new QLabel(tr("Theme:"));
    m_themeComboBox = new QComboBox();
    m_themeComboBox->addItem(tr("Light"), "Light");
    m_themeComboBox->addItem(tr("Dark"), "Dark");
    m_themeComboBox->addItem(tr("Savanna"), "Savanna");
    m_themeComboBox->addItem(tr("Midnight"), "Midnight");
    gridLayout->addWidget(themeLabel, 0, 0);
    gridLayout->addWidget(m_themeComboBox, 0, 1);

    // Language selection (row 1)
    QLabel* languageLabel = new QLabel(tr("Language:"));
    m_languageComboBox = new QComboBox();
    m_languageComboBox->addItem(tr("English"), "en");
    m_languageComboBox->addItem(tr("Polski"), "pl");
    gridLayout->addWidget(languageLabel, 1, 0);
    gridLayout->addWidget(m_languageComboBox, 1, 1);

    // Font size (row 2)
    QLabel* fontSizeLabel = new QLabel(tr("Font Size:"));
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setMinimum(8);
    m_fontSizeSpinBox->setMaximum(32);
    m_fontSizeSpinBox->setSingleStep(1);
    m_fontSizeSpinBox->setSuffix(" pt");
    gridLayout->addWidget(fontSizeLabel, 2, 0);
    gridLayout->addWidget(m_fontSizeSpinBox, 2, 1);

    // Primary icon color (row 3) - Task #00020
    QLabel* primaryColorLabel = new QLabel(tr("Primary Icon Color:"));
    m_primaryColorButton = new QPushButton();
    m_primaryColorButton->setMinimumHeight(30);
    m_primaryColorButton->setToolTip(tr("Click to change primary icon color"));
    connect(m_primaryColorButton, &QPushButton::clicked, this, &SettingsDialog::onPrimaryColorButtonClicked);
    gridLayout->addWidget(primaryColorLabel, 3, 0);
    gridLayout->addWidget(m_primaryColorButton, 3, 1);

    // Secondary icon color (row 4) - Task #00020
    QLabel* secondaryColorLabel = new QLabel(tr("Secondary Icon Color:"));
    m_secondaryColorButton = new QPushButton();
    m_secondaryColorButton->setMinimumHeight(30);
    m_secondaryColorButton->setToolTip(tr("Click to change secondary icon color"));
    connect(m_secondaryColorButton, &QPushButton::clicked, this, &SettingsDialog::onSecondaryColorButtonClicked);
    gridLayout->addWidget(secondaryColorLabel, 4, 0);
    gridLayout->addWidget(m_secondaryColorButton, 4, 1);

    // Make controls stretch horizontally
    gridLayout->setColumnStretch(1, 1);

    appearanceGroup->setLayout(gridLayout);
    appearanceLayout->addWidget(appearanceGroup);
    appearanceLayout->addStretch();  // Push group to top

    m_tabWidget->addTab(m_appearanceTab, tr("Appearance"));

    // Editor tab
    m_editorTab = new QWidget();
    QVBoxLayout* editorLayout = new QVBoxLayout(m_editorTab);

    // Create group box for visual grouping
    QGroupBox* editorGroup = new QGroupBox(tr("Editor Settings"));
    QGridLayout* editorGridLayout = new QGridLayout();

    // Font family (row 0)
    QLabel* fontFamilyLabel = new QLabel(tr("Font Family:"));
    m_fontFamilyComboBox = new QFontComboBox();
    m_fontFamilyComboBox->setFontFilters(QFontComboBox::MonospacedFonts);  // Only monospace
    editorGridLayout->addWidget(fontFamilyLabel, 0, 0);
    editorGridLayout->addWidget(m_fontFamilyComboBox, 0, 1);

    // Font size (row 1)
    QLabel* editorFontSizeLabel = new QLabel(tr("Font Size:"));
    m_editorFontSizeSpinBox = new QSpinBox();
    m_editorFontSizeSpinBox->setMinimum(8);
    m_editorFontSizeSpinBox->setMaximum(32);
    m_editorFontSizeSpinBox->setSingleStep(1);
    m_editorFontSizeSpinBox->setSuffix(" pt");
    editorGridLayout->addWidget(editorFontSizeLabel, 1, 0);
    editorGridLayout->addWidget(m_editorFontSizeSpinBox, 1, 1);

    // Tab size (row 2)
    QLabel* tabSizeLabel = new QLabel(tr("Tab Size:"));
    m_tabSizeSpinBox = new QSpinBox();
    m_tabSizeSpinBox->setMinimum(2);
    m_tabSizeSpinBox->setMaximum(8);
    m_tabSizeSpinBox->setSingleStep(1);
    m_tabSizeSpinBox->setSuffix(" spaces");
    editorGridLayout->addWidget(tabSizeLabel, 2, 0);
    editorGridLayout->addWidget(m_tabSizeSpinBox, 2, 1);

    // Line numbers (row 3)
    m_lineNumbersCheckBox = new QCheckBox(tr("Show Line Numbers"));
    editorGridLayout->addWidget(m_lineNumbersCheckBox, 3, 0, 1, 2);  // Span 2 columns

    // Word wrap (row 4)
    m_wordWrapCheckBox = new QCheckBox(tr("Enable Word Wrap"));
    editorGridLayout->addWidget(m_wordWrapCheckBox, 4, 0, 1, 2);  // Span 2 columns

    // Make controls stretch horizontally
    editorGridLayout->setColumnStretch(1, 1);

    editorGroup->setLayout(editorGridLayout);
    editorLayout->addWidget(editorGroup);
    editorLayout->addStretch();  // Push group to top

    m_tabWidget->addTab(m_editorTab, tr("Editor"));

    // Advanced tab (Task #00018)
    m_advancedTab = new QWidget();
    QVBoxLayout* advancedLayout = new QVBoxLayout(m_advancedTab);

    // Warning label
    QLabel* warningLabel = new QLabel(tr("⚠️ Warning: Diagnostic tools are for developers only.\n"
        "Some tools may crash the application or consume significant resources."));
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("QLabel { color: #ff6600; font-weight: bold; }");
    advancedLayout->addWidget(warningLabel);

    advancedLayout->addSpacing(10);

    // Diagnostic mode checkbox
    m_diagModeCheckbox = new QCheckBox(tr("Enable Diagnostic Menu"), this);
    m_diagModeCheckbox->setChecked(false); // Will be set in loadSettings() (Task #00018)
    connect(m_diagModeCheckbox, &QCheckBox::toggled, this, &SettingsDialog::onDiagModeCheckboxToggled);
    advancedLayout->addWidget(m_diagModeCheckbox);

    advancedLayout->addStretch(); // Push controls to top

    m_tabWidget->addTab(m_advancedTab, tr("Advanced"));

    mainLayout->addWidget(m_tabWidget);

    // Button box (OK, Cancel, Apply)
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this
    );

    mainLayout->addWidget(m_buttonBox);

    // Connect button signals
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onReject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::onApply);

    logger.debug("SettingsDialog: UI created successfully");
}

void SettingsDialog::loadSettings() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Loading settings from SettingsManager");

    auto& settings = core::SettingsManager::getInstance();

    // Load theme
    std::string currentTheme = settings.getTheme();
    int themeIndex = m_themeComboBox->findData(QString::fromStdString(currentTheme));
    if (themeIndex != -1) {
        m_themeComboBox->setCurrentIndex(themeIndex);
    }
    logger.debug("Loaded theme: {}", currentTheme);

    // Load language
    std::string currentLang = settings.getLanguage();
    int langIndex = m_languageComboBox->findData(QString::fromStdString(currentLang));
    if (langIndex != -1) {
        m_languageComboBox->setCurrentIndex(langIndex);
    }
    logger.debug("Loaded language: {}", currentLang);

    // Load font size (using default 12 - SettingsManager doesn't have getFontSize() yet)
    m_fontSizeSpinBox->setValue(12);
    logger.debug("Loaded font size: 12 (default)");

    // Load editor settings
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", "Consolas");
    QFont font(QString::fromStdString(fontFamily));
    m_fontFamilyComboBox->setCurrentFont(font);
    logger.debug("Loaded editor font family: {}", fontFamily);

    int editorFontSize = settings.get<int>("editor.fontSize", 12);
    m_editorFontSizeSpinBox->setValue(editorFontSize);
    logger.debug("Loaded editor font size: {}", editorFontSize);

    int tabSize = settings.get<int>("editor.tabSize", 4);
    m_tabSizeSpinBox->setValue(tabSize);
    logger.debug("Loaded tab size: {}", tabSize);

    bool lineNumbers = settings.get<bool>("editor.lineNumbers", true);
    m_lineNumbersCheckBox->setChecked(lineNumbers);
    logger.debug("Loaded line numbers: {}", lineNumbers);

    bool wordWrap = settings.get<bool>("editor.wordWrap", false);
    m_wordWrapCheckBox->setChecked(wordWrap);
    logger.debug("Loaded word wrap: {}", wordWrap);

    // Load diagnostic mode state (Task #00018)
    // Block signals to avoid triggering confirmation dialog during initialization
    m_diagModeCheckbox->blockSignals(true);
    m_diagModeCheckbox->setChecked(m_initialDiagMode);
    m_diagModeCheckbox->blockSignals(false);
    logger.debug("Loaded diagnostic mode: {}", m_initialDiagMode);

    // Load icon colors (Task #00020)
    std::string primaryColor = settings.getIconColorPrimary();
    QString primaryColorStyle = QString("background-color: %1; border: 1px solid #ccc;")
                                    .arg(QString::fromStdString(primaryColor));
    m_primaryColorButton->setStyleSheet(primaryColorStyle);
    logger.debug("Loaded primary icon color: {}", primaryColor);

    std::string secondaryColor = settings.getIconColorSecondary();
    QString secondaryColorStyle = QString("background-color: %1; border: 1px solid #ccc;")
                                      .arg(QString::fromStdString(secondaryColor));
    m_secondaryColorButton->setStyleSheet(secondaryColorStyle);
    logger.debug("Loaded secondary icon color: {}", secondaryColor);

    logger.debug("SettingsDialog: Settings loaded successfully");
}

void SettingsDialog::saveSettings() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Saving settings to SettingsManager");

    auto& settings = core::SettingsManager::getInstance();

    // Save theme
    QString themeData = m_themeComboBox->currentData().toString();
    settings.setTheme(themeData.toStdString());
    logger.debug("Saved theme: {}", themeData.toStdString());

    // Save language
    QString langData = m_languageComboBox->currentData().toString();
    settings.setLanguage(langData.toStdString());
    logger.debug("Saved language: {}", langData.toStdString());

    // Font size (log only - SettingsManager doesn't have setFontSize() yet)
    int fontSize = m_fontSizeSpinBox->value();
    logger.debug("Font size: {} (not saved - no setter available)", fontSize);

    // Save editor settings
    QString fontFamilyData = m_fontFamilyComboBox->currentFont().family();
    settings.set("editor.fontFamily", fontFamilyData.toStdString());
    logger.debug("Saved editor font family: {}", fontFamilyData.toStdString());

    int editorFontSize = m_editorFontSizeSpinBox->value();
    settings.set("editor.fontSize", editorFontSize);
    logger.debug("Saved editor font size: {}", editorFontSize);

    int tabSize = m_tabSizeSpinBox->value();
    settings.set("editor.tabSize", tabSize);
    logger.debug("Saved tab size: {}", tabSize);

    bool lineNumbers = m_lineNumbersCheckBox->isChecked();
    settings.set("editor.lineNumbers", lineNumbers);
    logger.debug("Saved line numbers: {}", lineNumbers);

    bool wordWrap = m_wordWrapCheckBox->isChecked();
    settings.set("editor.wordWrap", wordWrap);
    logger.debug("Saved word wrap: {}", wordWrap);

    // Save icon colors (Task #00020)
    // Extract color from button stylesheet (format: "background-color: #RRGGBB; ...")
    QString primaryStyle = m_primaryColorButton->styleSheet();
    int primaryStart = primaryStyle.indexOf("#");
    int primaryEnd = primaryStyle.indexOf(";", primaryStart);
    if (primaryStart != -1 && primaryEnd != -1) {
        QString primaryColor = primaryStyle.mid(primaryStart, primaryEnd - primaryStart).trimmed();
        settings.setIconColorPrimary(primaryColor.toStdString());
        logger.debug("Saved primary icon color: {}", primaryColor.toStdString());
    }

    QString secondaryStyle = m_secondaryColorButton->styleSheet();
    int secondaryStart = secondaryStyle.indexOf("#");
    int secondaryEnd = secondaryStyle.indexOf(";", secondaryStart);
    if (secondaryStart != -1 && secondaryEnd != -1) {
        QString secondaryColor = secondaryStyle.mid(secondaryStart, secondaryEnd - secondaryStart).trimmed();
        settings.setIconColorSecondary(secondaryColor.toStdString());
        logger.debug("Saved secondary icon color: {}", secondaryColor.toStdString());
    }

    // Save to disk
    if (settings.save()) {
        logger.info("SettingsDialog: Settings saved successfully");
    } else {
        logger.error("SettingsDialog: Failed to save settings");
    }
}

void SettingsDialog::onAccept() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: OK button clicked");

    saveSettings();

    // Check if diagnostic mode changed (Task #00018)
    bool currentDiagMode = m_diagModeCheckbox->isChecked();
    if (currentDiagMode != m_initialDiagMode) {
        logger.info("Diagnostic mode changed: {} -> {}", m_initialDiagMode, currentDiagMode);
        emit diagnosticModeChanged(currentDiagMode);
    }

    accept();  // Close dialog with Accepted result
}

void SettingsDialog::onReject() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Cancel button clicked");

    // Don't save settings, just close
    reject();  // Close dialog with Rejected result
}

void SettingsDialog::onApply() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Apply button clicked");

    saveSettings();

    // Check if diagnostic mode changed (Task #00018)
    bool currentDiagMode = m_diagModeCheckbox->isChecked();
    if (currentDiagMode != m_initialDiagMode) {
        logger.info("Diagnostic mode changed: {} -> {}", m_initialDiagMode, currentDiagMode);
        emit diagnosticModeChanged(currentDiagMode);
        m_initialDiagMode = currentDiagMode;  // Update initial state after Apply
    }

    // Don't close dialog
}

void SettingsDialog::onDiagModeCheckboxToggled(bool checked) {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Diagnostic mode checkbox toggled: {}", checked);

    if (checked) {
        // Show confirmation dialog when enabling
        QMessageBox::StandardButton reply = QMessageBox::warning(this,
            tr("Enable Diagnostic Menu"),
            tr("Are you sure you want to enable diagnostic menu?\n\n"
               "This exposes advanced debugging tools that may affect application stability."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::No) {
            // User cancelled - revert checkbox
            m_diagModeCheckbox->setChecked(false);
            logger.debug("User cancelled diagnostic mode activation");
            return;
        }

        logger.debug("User confirmed diagnostic mode activation");
    }

    // Don't emit signal here - wait for Apply/OK (Task #00018)
    logger.debug("Diagnostic mode change will be applied when clicking Apply/OK");
}

void SettingsDialog::onPrimaryColorButtonClicked() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Primary color button clicked");

    // Extract current color from button stylesheet
    QString currentStyle = m_primaryColorButton->styleSheet();
    int colorStart = currentStyle.indexOf("#");
    int colorEnd = currentStyle.indexOf(";", colorStart);
    QColor currentColor(Qt::black);  // Default
    if (colorStart != -1 && colorEnd != -1) {
        QString colorStr = currentStyle.mid(colorStart, colorEnd - colorStart).trimmed();
        currentColor = QColor(colorStr);
    }

    // Open color dialog
    QColor color = QColorDialog::getColor(currentColor, this, tr("Select Primary Icon Color"));
    if (color.isValid()) {
        QString newStyle = QString("background-color: %1; border: 1px solid #ccc;")
                              .arg(color.name());
        m_primaryColorButton->setStyleSheet(newStyle);
        logger.debug("Primary icon color changed to: {}", color.name().toStdString());
    } else {
        logger.debug("Primary color selection cancelled");
    }
}

void SettingsDialog::onSecondaryColorButtonClicked() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Secondary color button clicked");

    // Extract current color from button stylesheet
    QString currentStyle = m_secondaryColorButton->styleSheet();
    int colorStart = currentStyle.indexOf("#");
    int colorEnd = currentStyle.indexOf(";", colorStart);
    QColor currentColor(Qt::gray);  // Default
    if (colorStart != -1 && colorEnd != -1) {
        QString colorStr = currentStyle.mid(colorStart, colorEnd - colorStart).trimmed();
        currentColor = QColor(colorStr);
    }

    // Open color dialog
    QColor color = QColorDialog::getColor(currentColor, this, tr("Select Secondary Icon Color"));
    if (color.isValid()) {
        QString newStyle = QString("background-color: %1; border: 1px solid #ccc;")
                              .arg(color.name());
        m_secondaryColorButton->setStyleSheet(newStyle);
        logger.debug("Secondary icon color changed to: {}", color.name().toStdString());
    } else {
        logger.debug("Secondary color selection cancelled");
    }
}

} // namespace gui
} // namespace kalahari
