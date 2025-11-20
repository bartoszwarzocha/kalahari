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

namespace kalahari {
namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_appearanceTab(nullptr)
    , m_editorTab(nullptr)
    , m_themeComboBox(nullptr)
    , m_languageComboBox(nullptr)
    , m_fontSizeSpinBox(nullptr)
    , m_fontFamilyComboBox(nullptr)
    , m_editorFontSizeSpinBox(nullptr)
    , m_tabSizeSpinBox(nullptr)
    , m_lineNumbersCheckBox(nullptr)
    , m_wordWrapCheckBox(nullptr)
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
    // Don't close dialog
}

} // namespace gui
} // namespace kalahari
