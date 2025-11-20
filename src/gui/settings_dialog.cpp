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

namespace kalahari {
namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_appearanceTab(nullptr)
    , m_editorTab(nullptr)
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

    // Appearance tab (placeholder)
    m_appearanceTab = new QWidget();
    QVBoxLayout* appearanceLayout = new QVBoxLayout(m_appearanceTab);
    QLabel* appearanceLabel = new QLabel(tr("Appearance settings will be implemented in Task #00005."));
    appearanceLabel->setWordWrap(true);
    appearanceLayout->addWidget(appearanceLabel);
    appearanceLayout->addStretch();
    m_tabWidget->addTab(m_appearanceTab, tr("Appearance"));

    // Editor tab (placeholder)
    m_editorTab = new QWidget();
    QVBoxLayout* editorLayout = new QVBoxLayout(m_editorTab);
    QLabel* editorLabel = new QLabel(tr("Editor settings will be implemented in Task #00006."));
    editorLabel->setWordWrap(true);
    editorLayout->addWidget(editorLabel);
    editorLayout->addStretch();
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

    // Placeholder: No actual controls to populate yet (Tasks #00005-00006)
    auto& settings = core::SettingsManager::getInstance();

    // Log current settings for debugging
    logger.debug("Current theme: {}", settings.getTheme());
    logger.debug("Current language: {}", settings.getLanguage());

    logger.debug("SettingsDialog: Settings loaded (placeholder)");
}

void SettingsDialog::saveSettings() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Saving settings to SettingsManager");

    // Placeholder: No actual controls to read yet (Tasks #00005-00006)
    auto& settings = core::SettingsManager::getInstance();

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
