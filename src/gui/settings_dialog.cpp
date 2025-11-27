/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog
///
/// Architecture: Dialog collects data, MainWindow applies with BusyIndicator.
/// See settings_dialog.h for detailed flow description.

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/busy_indicator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/icon_registry.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>

namespace kalahari {
namespace gui {

// ============================================================================
// Constructor
// ============================================================================

SettingsDialog::SettingsDialog(QWidget* parent, const SettingsData& currentSettings)
    : QDialog(parent)
    , m_navTree(nullptr)
    , m_pageStack(nullptr)
    , m_buttonBox(nullptr)
    , m_languageComboBox(nullptr)
    , m_uiFontSizeSpinBox(nullptr)
    , m_themeComboBox(nullptr)
    , m_primaryColorButton(nullptr)
    , m_secondaryColorButton(nullptr)
    , m_themePreviewLabel(nullptr)
    , m_iconThemeComboBox(nullptr)
    , m_toolbarIconSizeSpinBox(nullptr)
    , m_menuIconSizeSpinBox(nullptr)
    , m_treeViewIconSizeSpinBox(nullptr)
    , m_tabBarIconSizeSpinBox(nullptr)
    , m_statusBarIconSizeSpinBox(nullptr)
    , m_buttonIconSizeSpinBox(nullptr)
    , m_comboBoxIconSizeSpinBox(nullptr)
    , m_iconPreviewLabel(nullptr)
    , m_iconPreviewLayout(nullptr)
    , m_fontFamilyComboBox(nullptr)
    , m_editorFontSizeSpinBox(nullptr)
    , m_tabSizeSpinBox(nullptr)
    , m_lineNumbersCheckBox(nullptr)
    , m_wordWrapCheckBox(nullptr)
    , m_diagModeCheckbox(nullptr)
    , m_originalSettings(currentSettings)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Constructor called (new architecture)");

    setWindowTitle(tr("Settings"));
    setModal(true);
    resize(750, 500);
    setMinimumSize(600, 400);

    createUI();
    populateFromSettings(currentSettings);

    logger.debug("SettingsDialog: Initialized successfully");
}

// ============================================================================
// UI Creation
// ============================================================================

void SettingsDialog::createUI() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Creating UI with tree navigation");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create splitter for tree and pages
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel: Navigation tree
    m_navTree = new QTreeWidget(splitter);
    m_navTree->setHeaderHidden(true);
    m_navTree->setMinimumWidth(180);
    m_navTree->setMaximumWidth(250);
    createNavigationTree();

    // Right panel: Stacked pages in scroll area
    m_pageStack = new QStackedWidget(splitter);
    createSettingsPages();

    // Set splitter proportions
    splitter->addWidget(m_navTree);
    splitter->addWidget(m_pageStack);
    splitter->setStretchFactor(0, 0);  // Tree doesn't stretch
    splitter->setStretchFactor(1, 1);  // Pages stretch

    mainLayout->addWidget(splitter, 1);

    // Button box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this
    );
    mainLayout->addWidget(m_buttonBox);

    // Connect signals
    connect(m_navTree, &QTreeWidget::currentItemChanged,
            this, &SettingsDialog::onTreeItemChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &SettingsDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &SettingsDialog::onReject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::onApply);

    // Select first item
    if (m_navTree->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstItem = m_navTree->topLevelItem(0);
        if (firstItem->childCount() > 0) {
            m_navTree->setCurrentItem(firstItem->child(0));
        } else {
            m_navTree->setCurrentItem(firstItem);
        }
    }

    logger.debug("SettingsDialog: UI created successfully");
}

void SettingsDialog::createNavigationTree() {
    auto& logger = core::Logger::getInstance();

    // Helper lambda for creating placeholder items (grayed out)
    auto createPlaceholderItem = [](QTreeWidgetItem* parent, const QString& text) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, text);
        item->setForeground(0, QColor(Qt::gray));
        item->setToolTip(0, QObject::tr("Coming in future version"));
        return item;
    };

    // ========================================================================
    // Appearance category (3 sub-items)
    // ========================================================================
    QTreeWidgetItem* appearanceItem = new QTreeWidgetItem(m_navTree);
    appearanceItem->setText(0, tr("Appearance"));
    appearanceItem->setExpanded(true);

    QTreeWidgetItem* appearanceGeneral = new QTreeWidgetItem(appearanceItem);
    appearanceGeneral->setText(0, tr("General"));
    m_itemToPage[appearanceGeneral] = PAGE_APPEARANCE_GENERAL;

    QTreeWidgetItem* appearanceTheme = new QTreeWidgetItem(appearanceItem);
    appearanceTheme->setText(0, tr("Theme"));
    m_itemToPage[appearanceTheme] = PAGE_APPEARANCE_THEME;

    QTreeWidgetItem* appearanceIcons = new QTreeWidgetItem(appearanceItem);
    appearanceIcons->setText(0, tr("Icons"));
    m_itemToPage[appearanceIcons] = PAGE_APPEARANCE_ICONS;

    // ========================================================================
    // Editor category (4 sub-items)
    // ========================================================================
    QTreeWidgetItem* editorItem = new QTreeWidgetItem(m_navTree);
    editorItem->setText(0, tr("Editor"));
    editorItem->setExpanded(true);

    QTreeWidgetItem* editorGeneral = new QTreeWidgetItem(editorItem);
    editorGeneral->setText(0, tr("General"));
    m_itemToPage[editorGeneral] = PAGE_EDITOR_GENERAL;

    QTreeWidgetItem* editorSpelling = createPlaceholderItem(editorItem, tr("Spelling"));
    m_itemToPage[editorSpelling] = PAGE_EDITOR_SPELLING;

    QTreeWidgetItem* editorAutocorrect = createPlaceholderItem(editorItem, tr("Auto-correct"));
    m_itemToPage[editorAutocorrect] = PAGE_EDITOR_AUTOCORRECT;

    QTreeWidgetItem* editorCompletion = createPlaceholderItem(editorItem, tr("Completion"));
    m_itemToPage[editorCompletion] = PAGE_EDITOR_COMPLETION;

    // ========================================================================
    // Files category (3 sub-items)
    // ========================================================================
    QTreeWidgetItem* filesItem = new QTreeWidgetItem(m_navTree);
    filesItem->setText(0, tr("Files"));
    filesItem->setExpanded(true);

    QTreeWidgetItem* filesBackup = createPlaceholderItem(filesItem, tr("Backup"));
    m_itemToPage[filesBackup] = PAGE_FILES_BACKUP;

    QTreeWidgetItem* filesAutosave = createPlaceholderItem(filesItem, tr("Auto-save"));
    m_itemToPage[filesAutosave] = PAGE_FILES_AUTOSAVE;

    QTreeWidgetItem* filesImportExport = createPlaceholderItem(filesItem, tr("Import/Export"));
    m_itemToPage[filesImportExport] = PAGE_FILES_IMPORT_EXPORT;

    // ========================================================================
    // Network category (2 sub-items)
    // ========================================================================
    QTreeWidgetItem* networkItem = new QTreeWidgetItem(m_navTree);
    networkItem->setText(0, tr("Network"));
    networkItem->setExpanded(true);

    QTreeWidgetItem* networkCloudSync = createPlaceholderItem(networkItem, tr("Cloud Sync"));
    m_itemToPage[networkCloudSync] = PAGE_NETWORK_CLOUD_SYNC;

    QTreeWidgetItem* networkUpdates = createPlaceholderItem(networkItem, tr("Updates"));
    m_itemToPage[networkUpdates] = PAGE_NETWORK_UPDATES;

    // ========================================================================
    // Advanced category (2 sub-items)
    // ========================================================================
    QTreeWidgetItem* advancedItem = new QTreeWidgetItem(m_navTree);
    advancedItem->setText(0, tr("Advanced"));
    advancedItem->setExpanded(true);

    QTreeWidgetItem* advancedGeneral = new QTreeWidgetItem(advancedItem);
    advancedGeneral->setText(0, tr("General"));
    m_itemToPage[advancedGeneral] = PAGE_ADVANCED_GENERAL;

    QTreeWidgetItem* advancedPerformance = createPlaceholderItem(advancedItem, tr("Performance"));
    m_itemToPage[advancedPerformance] = PAGE_ADVANCED_PERFORMANCE;

    logger.debug("SettingsDialog: Navigation tree created with 5 categories, 14 pages");
}

void SettingsDialog::createSettingsPages() {
    // ========================================================================
    // Appearance pages (0-2)
    // ========================================================================
    // Page 0: Appearance/General
    m_pageStack->addWidget(createAppearanceGeneralPage());
    // Page 1: Appearance/Theme
    m_pageStack->addWidget(createAppearanceThemePage());
    // Page 2: Appearance/Icons
    m_pageStack->addWidget(createAppearanceIconsPage());

    // ========================================================================
    // Editor pages (3-6)
    // ========================================================================
    // Page 3: Editor/General
    m_pageStack->addWidget(createEditorGeneralPage());
    // Page 4: Editor/Spelling
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Spelling"),
        tr("Spelling settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Spell check language selection\n"
           "- Custom dictionary management\n"
           "- Ignore rules for technical terms")
    ));
    // Page 5: Editor/Auto-correct
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Auto-correct"),
        tr("Auto-correct settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic capitalization\n"
           "- Common typo corrections\n"
           "- Custom replacement rules")
    ));
    // Page 6: Editor/Completion
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Completion"),
        tr("Completion settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Word completion suggestions\n"
           "- Character name completion\n"
           "- Location name completion")
    ));

    // ========================================================================
    // Files pages (7-9)
    // ========================================================================
    // Page 7: Files/Backup
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Backup"),
        tr("Backup settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic backup frequency\n"
           "- Backup location selection\n"
           "- Number of backup copies to keep\n"
           "- Restore from backup")
    ));
    // Page 8: Files/Auto-save
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Auto-save"),
        tr("Auto-save settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Auto-save interval\n"
           "- Auto-save on focus loss\n"
           "- Session recovery options")
    ));
    // Page 9: Files/Import/Export
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Import/Export"),
        tr("Import/Export settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Default export format\n"
           "- Import source preferences\n"
           "- Encoding settings")
    ));

    // ========================================================================
    // Network pages (10-11)
    // ========================================================================
    // Page 10: Network/Cloud Sync
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Cloud Sync"),
        tr("Cloud Sync settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Cloud provider selection\n"
           "- Sync frequency\n"
           "- Conflict resolution\n"
           "- Sync status and history")
    ));
    // Page 11: Network/Updates
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Updates"),
        tr("Update settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic update checks\n"
           "- Update channel (stable/beta)\n"
           "- Plugin updates")
    ));

    // ========================================================================
    // Advanced pages (12-13)
    // ========================================================================
    // Page 12: Advanced/General
    m_pageStack->addWidget(createAdvancedGeneralPage());
    // Page 13: Advanced/Performance
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Performance"),
        tr("Performance settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Memory usage limits\n"
           "- Thread pool configuration\n"
           "- Cache settings\n"
           "- Hardware acceleration")
    ));
}

QWidget* SettingsDialog::createAppearanceGeneralPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // General settings group
    QGroupBox* group = new QGroupBox(tr("General Appearance"));
    QGridLayout* grid = new QGridLayout(group);

    // Language
    QLabel* langLabel = new QLabel(tr("Language:"));
    m_languageComboBox = new QComboBox();
    m_languageComboBox->addItem(tr("English"), "en");
    m_languageComboBox->addItem(tr("Polski"), "pl");
    grid->addWidget(langLabel, 0, 0);
    grid->addWidget(m_languageComboBox, 0, 1);

    // UI Font Size
    QLabel* fontSizeLabel = new QLabel(tr("UI Font Size:"));
    m_uiFontSizeSpinBox = new QSpinBox();
    m_uiFontSizeSpinBox->setRange(8, 24);
    m_uiFontSizeSpinBox->setSuffix(" pt");
    grid->addWidget(fontSizeLabel, 1, 0);
    grid->addWidget(m_uiFontSizeSpinBox, 1, 1);

    // Note about restart
    QLabel* restartNote = new QLabel(tr("Note: Some changes require application restart."));
    restartNote->setStyleSheet("color: #666; font-style: italic;");
    grid->addWidget(restartNote, 2, 0, 1, 2);

    grid->setColumnStretch(1, 1);
    layout->addWidget(group);
    layout->addStretch();

    return page;
}

QWidget* SettingsDialog::createAppearanceThemePage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Theme selection group
    QGroupBox* themeGroup = new QGroupBox(tr("Color Theme"));
    QGridLayout* themeGrid = new QGridLayout(themeGroup);

    QLabel* themeLabel = new QLabel(tr("Theme:"));
    m_themeComboBox = new QComboBox();
    m_themeComboBox->addItem(tr("Light"), "Light");
    m_themeComboBox->addItem(tr("Dark"), "Dark");
    connect(m_themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onThemeComboChanged);
    themeGrid->addWidget(themeLabel, 0, 0);
    themeGrid->addWidget(m_themeComboBox, 0, 1);

    themeGrid->setColumnStretch(1, 1);
    layout->addWidget(themeGroup);

    // Color overrides group
    QGroupBox* colorGroup = new QGroupBox(tr("Color Overrides"));
    QGridLayout* colorGrid = new QGridLayout(colorGroup);

    QLabel* primaryLabel = new QLabel(tr("Primary Color:"));
    m_primaryColorButton = new QPushButton();
    m_primaryColorButton->setMinimumHeight(30);
    m_primaryColorButton->setToolTip(tr("Click to change primary color"));
    connect(m_primaryColorButton, &QPushButton::clicked,
            this, &SettingsDialog::onPrimaryColorButtonClicked);
    colorGrid->addWidget(primaryLabel, 0, 0);
    colorGrid->addWidget(m_primaryColorButton, 0, 1);

    QLabel* secondaryLabel = new QLabel(tr("Secondary Color:"));
    m_secondaryColorButton = new QPushButton();
    m_secondaryColorButton->setMinimumHeight(30);
    m_secondaryColorButton->setToolTip(tr("Click to change secondary color"));
    connect(m_secondaryColorButton, &QPushButton::clicked,
            this, &SettingsDialog::onSecondaryColorButtonClicked);
    colorGrid->addWidget(secondaryLabel, 1, 0);
    colorGrid->addWidget(m_secondaryColorButton, 1, 1);

    // Reset button - clears custom colors and restores theme defaults (Task #00025)
    QPushButton* resetColorsBtn = new QPushButton(tr("Reset to Theme Defaults"));
    connect(resetColorsBtn, &QPushButton::clicked, [this]() {
        QString theme = m_themeComboBox->currentData().toString();
        std::string themeName = theme.toStdString();

        // Clear custom colors from storage
        core::SettingsManager::getInstance().clearCustomIconColorsForTheme(themeName);

        // Reset buttons to theme defaults
        if (theme == "Dark") {
            updateColorButton(m_primaryColorButton, QColor("#999999"));
            updateColorButton(m_secondaryColorButton, QColor("#333333"));
        } else {
            updateColorButton(m_primaryColorButton, QColor("#333333"));
            updateColorButton(m_secondaryColorButton, QColor("#999999"));
        }

        core::Logger::getInstance().info("SettingsDialog: Reset colors to theme defaults for '{}'", themeName);
    });
    colorGrid->addWidget(resetColorsBtn, 2, 0, 1, 2);

    colorGrid->setColumnStretch(1, 1);
    layout->addWidget(colorGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createAppearanceIconsPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Icon theme group
    QGroupBox* themeGroup = new QGroupBox(tr("Icon Style"));
    QGridLayout* themeGrid = new QGridLayout(themeGroup);

    QLabel* iconThemeLabel = new QLabel(tr("Icon Style:"));
    m_iconThemeComboBox = new QComboBox();
    m_iconThemeComboBox->addItem(tr("Two-tone (Default)"), "twotone");
    m_iconThemeComboBox->addItem(tr("Filled"), "filled");
    m_iconThemeComboBox->addItem(tr("Outlined"), "outlined");
    m_iconThemeComboBox->addItem(tr("Rounded"), "rounded");
    connect(m_iconThemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onIconThemeComboChanged);
    themeGrid->addWidget(iconThemeLabel, 0, 0);
    themeGrid->addWidget(m_iconThemeComboBox, 0, 1);

    // Preview - horizontal layout with sample icons
    QLabel* previewTitleLabel = new QLabel(tr("Preview:"));
    themeGrid->addWidget(previewTitleLabel, 1, 0, Qt::AlignVCenter);

    QWidget* previewWidget = new QWidget();
    previewWidget->setAutoFillBackground(true);
    // Use palette for theme-aware background
    QPalette previewPalette = previewWidget->palette();
    previewPalette.setColor(QPalette::Window, palette().color(QPalette::Base));
    previewWidget->setPalette(previewPalette);

    m_iconPreviewLayout = new QHBoxLayout(previewWidget);
    m_iconPreviewLayout->setContentsMargins(12, 8, 12, 8);
    m_iconPreviewLayout->setSpacing(16);
    m_iconPreviewLayout->setAlignment(Qt::AlignCenter);
    previewWidget->setMinimumHeight(48);
    previewWidget->setFixedHeight(52);
    themeGrid->addWidget(previewWidget, 1, 1);

    themeGrid->setColumnStretch(1, 1);
    layout->addWidget(themeGroup);

    // Icon sizes group
    QGroupBox* sizeGroup = new QGroupBox(tr("Icon Sizes"));
    QGridLayout* sizeGrid = new QGridLayout(sizeGroup);
    int row = 0;

    // Toolbar Icons
    QLabel* toolbarSizeLabel = new QLabel(tr("Toolbar:"));
    m_toolbarIconSizeSpinBox = new QSpinBox();
    m_toolbarIconSizeSpinBox->setRange(16, 48);
    m_toolbarIconSizeSpinBox->setSingleStep(2);
    m_toolbarIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(toolbarSizeLabel, row, 0);
    sizeGrid->addWidget(m_toolbarIconSizeSpinBox, row, 1);
    row++;

    // Menu Icons
    QLabel* menuSizeLabel = new QLabel(tr("Menu:"));
    m_menuIconSizeSpinBox = new QSpinBox();
    m_menuIconSizeSpinBox->setRange(12, 32);
    m_menuIconSizeSpinBox->setSingleStep(2);
    m_menuIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(menuSizeLabel, row, 0);
    sizeGrid->addWidget(m_menuIconSizeSpinBox, row, 1);
    row++;

    // TreeView/Navigator Icons
    QLabel* treeViewSizeLabel = new QLabel(tr("Navigator/Tree:"));
    m_treeViewIconSizeSpinBox = new QSpinBox();
    m_treeViewIconSizeSpinBox->setRange(12, 32);
    m_treeViewIconSizeSpinBox->setSingleStep(2);
    m_treeViewIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(treeViewSizeLabel, row, 0);
    sizeGrid->addWidget(m_treeViewIconSizeSpinBox, row, 1);
    row++;

    // TabBar Icons
    QLabel* tabBarSizeLabel = new QLabel(tr("Tab Bar:"));
    m_tabBarIconSizeSpinBox = new QSpinBox();
    m_tabBarIconSizeSpinBox->setRange(12, 32);
    m_tabBarIconSizeSpinBox->setSingleStep(2);
    m_tabBarIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(tabBarSizeLabel, row, 0);
    sizeGrid->addWidget(m_tabBarIconSizeSpinBox, row, 1);
    row++;

    // Button Icons
    QLabel* buttonSizeLabel = new QLabel(tr("Buttons:"));
    m_buttonIconSizeSpinBox = new QSpinBox();
    m_buttonIconSizeSpinBox->setRange(12, 32);
    m_buttonIconSizeSpinBox->setSingleStep(2);
    m_buttonIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(buttonSizeLabel, row, 0);
    sizeGrid->addWidget(m_buttonIconSizeSpinBox, row, 1);
    row++;

    // StatusBar Icons
    QLabel* statusBarSizeLabel = new QLabel(tr("Status Bar:"));
    m_statusBarIconSizeSpinBox = new QSpinBox();
    m_statusBarIconSizeSpinBox->setRange(12, 24);
    m_statusBarIconSizeSpinBox->setSingleStep(2);
    m_statusBarIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(statusBarSizeLabel, row, 0);
    sizeGrid->addWidget(m_statusBarIconSizeSpinBox, row, 1);
    row++;

    // ComboBox Icons
    QLabel* comboBoxSizeLabel = new QLabel(tr("Combo Boxes:"));
    m_comboBoxIconSizeSpinBox = new QSpinBox();
    m_comboBoxIconSizeSpinBox->setRange(12, 24);
    m_comboBoxIconSizeSpinBox->setSingleStep(2);
    m_comboBoxIconSizeSpinBox->setSuffix(" px");
    sizeGrid->addWidget(comboBoxSizeLabel, row, 0);
    sizeGrid->addWidget(m_comboBoxIconSizeSpinBox, row, 1);

    sizeGrid->setColumnStretch(1, 1);
    layout->addWidget(sizeGroup);

    layout->addStretch();

    // Initial preview update
    updateIconPreview();

    return page;
}

QWidget* SettingsDialog::createEditorGeneralPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Font group
    QGroupBox* fontGroup = new QGroupBox(tr("Editor Font"));
    QGridLayout* fontGrid = new QGridLayout(fontGroup);

    QLabel* fontFamilyLabel = new QLabel(tr("Font Family:"));
    m_fontFamilyComboBox = new QFontComboBox();
    m_fontFamilyComboBox->setFontFilters(QFontComboBox::MonospacedFonts);
    fontGrid->addWidget(fontFamilyLabel, 0, 0);
    fontGrid->addWidget(m_fontFamilyComboBox, 0, 1);

    QLabel* fontSizeLabel = new QLabel(tr("Font Size:"));
    m_editorFontSizeSpinBox = new QSpinBox();
    m_editorFontSizeSpinBox->setRange(8, 32);
    m_editorFontSizeSpinBox->setSuffix(" pt");
    fontGrid->addWidget(fontSizeLabel, 1, 0);
    fontGrid->addWidget(m_editorFontSizeSpinBox, 1, 1);

    fontGrid->setColumnStretch(1, 1);
    layout->addWidget(fontGroup);

    // Behavior group
    QGroupBox* behaviorGroup = new QGroupBox(tr("Editor Behavior"));
    QGridLayout* behaviorGrid = new QGridLayout(behaviorGroup);

    QLabel* tabSizeLabel = new QLabel(tr("Tab Size:"));
    m_tabSizeSpinBox = new QSpinBox();
    m_tabSizeSpinBox->setRange(2, 8);
    m_tabSizeSpinBox->setSuffix(tr(" spaces"));
    behaviorGrid->addWidget(tabSizeLabel, 0, 0);
    behaviorGrid->addWidget(m_tabSizeSpinBox, 0, 1);

    m_lineNumbersCheckBox = new QCheckBox(tr("Show Line Numbers"));
    behaviorGrid->addWidget(m_lineNumbersCheckBox, 1, 0, 1, 2);

    m_wordWrapCheckBox = new QCheckBox(tr("Enable Word Wrap"));
    behaviorGrid->addWidget(m_wordWrapCheckBox, 2, 0, 1, 2);

    behaviorGrid->setColumnStretch(1, 1);
    layout->addWidget(behaviorGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createAdvancedGeneralPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Warning
    QLabel* warningLabel = new QLabel(
        tr("Warning: These settings are for advanced users and developers.\n"
           "Incorrect configuration may affect application stability.")
    );
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("QLabel { color: #ff6600; font-weight: bold; padding: 10px; "
                                 "background-color: #fff3e0; border: 1px solid #ffcc80; }");
    layout->addWidget(warningLabel);

    // Diagnostic group
    QGroupBox* diagGroup = new QGroupBox(tr("Diagnostic Tools"));
    QVBoxLayout* diagLayout = new QVBoxLayout(diagGroup);

    m_diagModeCheckbox = new QCheckBox(tr("Enable Diagnostic Menu"));
    m_diagModeCheckbox->setToolTip(tr("Shows additional menu with debugging tools"));
    connect(m_diagModeCheckbox, &QCheckBox::toggled,
            this, &SettingsDialog::onDiagModeCheckboxToggled);
    diagLayout->addWidget(m_diagModeCheckbox);

    QLabel* diagNote = new QLabel(
        tr("When enabled, a 'Diagnostic' menu appears in the menu bar with:\n"
           "- System information\n"
           "- Log viewer\n"
           "- Component status")
    );
    diagNote->setStyleSheet("color: #666; margin-left: 20px;");
    diagLayout->addWidget(diagNote);

    layout->addWidget(diagGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createPlaceholderPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #666;");
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #888; margin-top: 20px;");
    layout->addWidget(descLabel);

    layout->addStretch();
    return page;
}

// ============================================================================
// Slots
// ============================================================================

void SettingsDialog::onTreeItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
    if (!current) return;

    auto& logger = core::Logger::getInstance();

    if (m_itemToPage.contains(current)) {
        int pageIndex = m_itemToPage[current];
        m_pageStack->setCurrentIndex(pageIndex);
        logger.debug("SettingsDialog: Switched to page {}", pageIndex);
    } else {
        // Parent item clicked - select first child
        if (current->childCount() > 0) {
            m_navTree->setCurrentItem(current->child(0));
        }
    }
}

void SettingsDialog::onAccept() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: OK clicked - applying settings with spinner");

    // Collect and apply settings with spinner ON THIS DIALOG
    SettingsData settings = collectSettings();
    applySettingsWithSpinner(settings);

    // Update original settings
    m_originalSettings = settings;

    // NOW close dialog (after spinner finished)
    accept();
}

void SettingsDialog::onReject() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Cancel clicked");
    reject();
}

void SettingsDialog::onApply() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Apply clicked - applying settings with spinner");

    // Collect and apply settings with spinner ON THIS DIALOG
    SettingsData settings = collectSettings();
    applySettingsWithSpinner(settings);

    // Update original settings so we know what changed next time
    m_originalSettings = settings;

    // Dialog stays open
}

void SettingsDialog::onDiagModeCheckboxToggled(bool checked) {
    if (checked) {
        QMessageBox::StandardButton reply = QMessageBox::warning(this,
            tr("Enable Diagnostic Menu"),
            tr("Are you sure you want to enable diagnostic menu?\n\n"
               "This exposes advanced debugging tools."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::No) {
            m_diagModeCheckbox->setChecked(false);
        }
    }
}

void SettingsDialog::onPrimaryColorButtonClicked() {
    QString currentStyle = m_primaryColorButton->styleSheet();
    QColor currentColor = QColor("#333333");

    int start = currentStyle.indexOf("#");
    int end = currentStyle.indexOf(";", start);
    if (start != -1 && end != -1) {
        currentColor = QColor(currentStyle.mid(start, end - start));
    }

    QColor color = QColorDialog::getColor(currentColor, this, tr("Select Primary Color"));
    if (color.isValid()) {
        updateColorButton(m_primaryColorButton, color);
    }
}

void SettingsDialog::onSecondaryColorButtonClicked() {
    QString currentStyle = m_secondaryColorButton->styleSheet();
    QColor currentColor = QColor("#999999");

    int start = currentStyle.indexOf("#");
    int end = currentStyle.indexOf(";", start);
    if (start != -1 && end != -1) {
        currentColor = QColor(currentStyle.mid(start, end - start));
    }

    QColor color = QColorDialog::getColor(currentColor, this, tr("Select Secondary Color"));
    if (color.isValid()) {
        updateColorButton(m_secondaryColorButton, color);
    }
}

void SettingsDialog::onThemeComboChanged(int index) {
    Q_UNUSED(index);
    QString theme = m_themeComboBox->currentData().toString();
    std::string themeName = theme.toStdString();

    // Theme defaults:
    // Light: primary=#333333 (dark gray for icons), secondary=#999999 (light gray)
    // Dark: primary=#999999 (light gray for icons), secondary=#333333 (dark gray)
    std::string defaultPrimary = (theme == "Dark") ? "#999999" : "#333333";
    std::string defaultSecondary = (theme == "Dark") ? "#333333" : "#999999";

    // Check if user has custom colors for this theme (Task #00025)
    auto& settings = core::SettingsManager::getInstance();
    if (settings.hasCustomIconColorsForTheme(themeName)) {
        // Load user's custom colors for this theme
        std::string primary = settings.getIconColorPrimaryForTheme(themeName, defaultPrimary);
        std::string secondary = settings.getIconColorSecondaryForTheme(themeName, defaultSecondary);
        updateColorButton(m_primaryColorButton, QColor(QString::fromStdString(primary)));
        updateColorButton(m_secondaryColorButton, QColor(QString::fromStdString(secondary)));
        core::Logger::getInstance().debug("SettingsDialog: Loaded custom colors for theme '{}': primary={}, secondary={}",
                                          themeName, primary, secondary);
    } else {
        // Use theme defaults
        updateColorButton(m_primaryColorButton, QColor(QString::fromStdString(defaultPrimary)));
        updateColorButton(m_secondaryColorButton, QColor(QString::fromStdString(defaultSecondary)));
        core::Logger::getInstance().debug("SettingsDialog: Using default colors for theme '{}': primary={}, secondary={}",
                                          themeName, defaultPrimary, defaultSecondary);
    }
}

void SettingsDialog::onIconThemeComboChanged(int index) {
    Q_UNUSED(index);
    updateIconPreview();
}

void SettingsDialog::updateIconPreview() {
    if (!m_iconPreviewLayout || !m_iconThemeComboBox) return;

    // Clear existing preview icons
    QLayoutItem* item;
    while ((item = m_iconPreviewLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    QString iconTheme = m_iconThemeComboBox->currentData().toString();
    int logicalSize = 32;  // Logical size for preview
    qreal dpr = devicePixelRatioF();
    int physicalSize = static_cast<int>(logicalSize * dpr);

    // Extract colors from UI buttons (not from IconRegistry cache!)
    QColor primaryColor("#424242");  // Default fallback
    QColor secondaryColor("#757575");

    if (m_primaryColorButton) {
        QString style = m_primaryColorButton->styleSheet();
        int start = style.indexOf("#");
        int end = style.indexOf(";", start);
        if (start != -1 && end != -1) {
            primaryColor = QColor(style.mid(start, end - start));
        }
    }

    if (m_secondaryColorButton) {
        QString style = m_secondaryColorButton->styleSheet();
        int start = style.indexOf("#");
        int end = style.indexOf(";", start);
        if (start != -1 && end != -1) {
            secondaryColor = QColor(style.mid(start, end - start));
        }
    }

    // Sample icons to preview
    QStringList sampleIcons = {
        "file.new", "file.open", "file.save",
        "edit.undo", "edit.redo", "edit.copy"
    };

    for (const QString& cmdId : sampleIcons) {
        // Get icon with UI colors (not cached theme colors!)
        QIcon icon = core::IconRegistry::getInstance().getIconWithColors(
            cmdId, iconTheme, physicalSize, primaryColor, secondaryColor);
        if (!icon.isNull()) {
            QPixmap pixmap = icon.pixmap(physicalSize, physicalSize);
            // Set device pixel ratio for crisp rendering
            pixmap.setDevicePixelRatio(dpr);

            QLabel* iconLabel = new QLabel();
            iconLabel->setPixmap(pixmap);
            iconLabel->setFixedSize(logicalSize, logicalSize);
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setToolTip(cmdId);
            m_iconPreviewLayout->addWidget(iconLabel);
        }
    }
}

// ============================================================================
// Settings Management
// ============================================================================

void SettingsDialog::populateFromSettings(const SettingsData& settings) {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Populating UI from SettingsData");

    // Appearance/General
    int langIndex = m_languageComboBox->findData(settings.language);
    if (langIndex >= 0) m_languageComboBox->setCurrentIndex(langIndex);
    m_uiFontSizeSpinBox->setValue(settings.uiFontSize);

    // Appearance/Theme
    int themeIndex = m_themeComboBox->findData(settings.theme);
    if (themeIndex >= 0) m_themeComboBox->setCurrentIndex(themeIndex);

    // Icon colors
    updateColorButton(m_primaryColorButton, settings.primaryColor);
    updateColorButton(m_secondaryColorButton, settings.secondaryColor);

    // Appearance/Icons
    int iconThemeIndex = m_iconThemeComboBox->findData(settings.iconTheme);
    if (iconThemeIndex >= 0) m_iconThemeComboBox->setCurrentIndex(iconThemeIndex);

    // Icon sizes
    m_toolbarIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::Toolbar, 24));
    m_menuIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::Menu, 16));
    m_treeViewIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::TreeView, 16));
    m_tabBarIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::TabBar, 16));
    m_buttonIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::Button, 20));
    m_statusBarIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::StatusBar, 16));
    m_comboBoxIconSizeSpinBox->setValue(settings.iconSizes.value(core::IconContext::ComboBox, 16));

    // Update preview after loading
    updateIconPreview();

    // Editor/General
    m_fontFamilyComboBox->setCurrentFont(QFont(settings.editorFontFamily));
    m_editorFontSizeSpinBox->setValue(settings.editorFontSize);
    m_tabSizeSpinBox->setValue(settings.tabSize);
    m_lineNumbersCheckBox->setChecked(settings.showLineNumbers);
    m_wordWrapCheckBox->setChecked(settings.wordWrap);

    // Advanced/General
    m_diagModeCheckbox->blockSignals(true);
    m_diagModeCheckbox->setChecked(settings.diagnosticMode);
    m_diagModeCheckbox->blockSignals(false);

    logger.debug("SettingsDialog: UI populated");
}

SettingsData SettingsDialog::collectSettings() const {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Collecting settings from UI");

    SettingsData settingsData;

    // Appearance/General
    settingsData.language = m_languageComboBox->currentData().toString();
    settingsData.uiFontSize = m_uiFontSizeSpinBox->value();

    // Appearance/Theme
    settingsData.theme = m_themeComboBox->currentData().toString();
    settingsData.primaryColor = getColorFromButton(m_primaryColorButton);
    settingsData.secondaryColor = getColorFromButton(m_secondaryColorButton);

    // Appearance/Icons
    settingsData.iconTheme = m_iconThemeComboBox->currentData().toString();
    settingsData.iconSizes[core::IconContext::Toolbar] = m_toolbarIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::Menu] = m_menuIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::TreeView] = m_treeViewIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::TabBar] = m_tabBarIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::Button] = m_buttonIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::StatusBar] = m_statusBarIconSizeSpinBox->value();
    settingsData.iconSizes[core::IconContext::ComboBox] = m_comboBoxIconSizeSpinBox->value();

    // Editor/General
    settingsData.editorFontFamily = m_fontFamilyComboBox->currentFont().family();
    settingsData.editorFontSize = m_editorFontSizeSpinBox->value();
    settingsData.tabSize = m_tabSizeSpinBox->value();
    settingsData.showLineNumbers = m_lineNumbersCheckBox->isChecked();
    settingsData.wordWrap = m_wordWrapCheckBox->isChecked();

    // Advanced/General
    settingsData.diagnosticMode = m_diagModeCheckbox->isChecked();

    logger.debug("SettingsDialog: Settings collected");
    return settingsData;
}

QColor SettingsDialog::getColorFromButton(QPushButton* button) const {
    if (!button) {
        // Fallback to theme default, never hardcode
        return core::ThemeManager::getInstance().getCurrentTheme().colors.primary;
    }

    QString style = button->styleSheet();
    int start = style.indexOf("#");
    int end = style.indexOf(";", start);
    if (start != -1 && end != -1) {
        return QColor(style.mid(start, end - start));
    }

    // Fallback to theme default
    return core::ThemeManager::getInstance().getCurrentTheme().colors.primary;
}

void SettingsDialog::updateColorButton(QPushButton* button, const QColor& color) {
    QString style = QString("background-color: %1; border: 1px solid #888;").arg(color.name());
    button->setStyleSheet(style);
}

void SettingsDialog::applySettingsWithSpinner(const SettingsData& settings) {
    auto& logger = core::Logger::getInstance();

    // Show BusyIndicator on THIS DIALOG (modal overlay)
    // Use BusyIndicator::tick() between steps to keep animation alive
    BusyIndicator::run(this, tr("Applying settings..."), [this, &settings, &logger]() {
        auto& settingsManager = core::SettingsManager::getInstance();
        auto& themeManager = core::ThemeManager::getInstance();
        auto& artProvider = core::ArtProvider::getInstance();

        // =====================================================================
        // Step 1: Appearance/General
        // =====================================================================
        settingsManager.setLanguage(settings.language.toStdString());
        settingsManager.set("appearance.uiFontSize", settings.uiFontSize);

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 2: Appearance/Theme
        // =====================================================================
        settingsManager.setTheme(settings.theme.toStdString());
        themeManager.switchTheme(settings.theme);

        // Save per-theme colors
        std::string themeName = settings.theme.toStdString();
        settingsManager.setIconColorPrimaryForTheme(themeName, settings.primaryColor.name().toStdString());
        settingsManager.setIconColorSecondaryForTheme(themeName, settings.secondaryColor.name().toStdString());

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 3: Appearance/Icons (slowest - triggers icon regeneration)
        // =====================================================================
        settingsManager.set("appearance.iconTheme", settings.iconTheme.toStdString());
        artProvider.setIconTheme(settings.iconTheme);

        BusyIndicator::tick();  // Animate

        artProvider.setPrimaryColor(settings.primaryColor);
        artProvider.setSecondaryColor(settings.secondaryColor);

        BusyIndicator::tick();  // Animate

        // Apply icon sizes
        for (auto it = settings.iconSizes.constBegin(); it != settings.iconSizes.constEnd(); ++it) {
            artProvider.setIconSize(it.key(), it.value());
        }

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 4: Editor/General
        // =====================================================================
        settingsManager.set("editor.fontFamily", settings.editorFontFamily.toStdString());
        settingsManager.set("editor.fontSize", settings.editorFontSize);
        settingsManager.set("editor.tabSize", settings.tabSize);
        settingsManager.set("editor.lineNumbers", settings.showLineNumbers);
        settingsManager.set("editor.wordWrap", settings.wordWrap);

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 5: Save to disk
        // =====================================================================
        settingsManager.save();

        logger.info("SettingsDialog: All settings persisted to disk");
    });

    // Emit signal AFTER spinner finished (MainWindow can react)
    emit settingsApplied(settings);
}

} // namespace gui
} // namespace kalahari
