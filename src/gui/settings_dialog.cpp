/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog
///
/// Architecture: Dialog collects data, MainWindow applies with BusyIndicator.
/// See settings_dialog.h for detailed flow description.

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/busy_indicator.h"
#include "kalahari/gui/widgets/color_config_widget.h"
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
    , m_primaryColorWidget(nullptr)
    , m_secondaryColorWidget(nullptr)
    , m_infoHeaderColorWidget(nullptr)
    , m_infoSecondaryColorWidget(nullptr)
    , m_infoPrimaryColorWidget(nullptr)
    , m_tooltipBackgroundColorWidget(nullptr)
    , m_tooltipTextColorWidget(nullptr)
    , m_placeholderTextColorWidget(nullptr)
    , m_brightTextColorWidget(nullptr)
    // Palette colors
    , m_paletteWindowColorWidget(nullptr)
    , m_paletteWindowTextColorWidget(nullptr)
    , m_paletteBaseColorWidget(nullptr)
    , m_paletteAlternateBaseColorWidget(nullptr)
    , m_paletteTextColorWidget(nullptr)
    , m_paletteButtonColorWidget(nullptr)
    , m_paletteButtonTextColorWidget(nullptr)
    , m_paletteHighlightColorWidget(nullptr)
    , m_paletteHighlightedTextColorWidget(nullptr)
    , m_paletteLightColorWidget(nullptr)
    , m_paletteMidlightColorWidget(nullptr)
    , m_paletteMidColorWidget(nullptr)
    , m_paletteDarkColorWidget(nullptr)
    , m_paletteShadowColorWidget(nullptr)
    , m_paletteLinkColorWidget(nullptr)
    , m_paletteLinkVisitedColorWidget(nullptr)
    // Log colors
    , m_logTraceColorWidget(nullptr)
    , m_logDebugColorWidget(nullptr)
    , m_logInfoColorWidget(nullptr)
    , m_logWarningColorWidget(nullptr)
    , m_logErrorColorWidget(nullptr)
    , m_logCriticalColorWidget(nullptr)
    , m_logBackgroundColorWidget(nullptr)
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
    , m_showKalahariNewsCheckBox(nullptr)
    , m_showRecentFilesCheckBox(nullptr)
    , m_autoLoadLastProjectCheckBox(nullptr)
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
    // General (top-level, not a category)
    // ========================================================================
    QTreeWidgetItem* generalItem = new QTreeWidgetItem(m_navTree);
    generalItem->setText(0, tr("General"));
    m_itemToPage[generalItem] = PAGE_GENERAL;

    // ========================================================================
    // Appearance category (4 sub-items)
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

    QTreeWidgetItem* appearanceDashboard = new QTreeWidgetItem(appearanceItem);
    appearanceDashboard->setText(0, tr("Dashboard"));
    m_itemToPage[appearanceDashboard] = PAGE_APPEARANCE_DASHBOARD;

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

    QTreeWidgetItem* advancedLog = new QTreeWidgetItem(advancedItem);
    advancedLog->setText(0, tr("Log"));
    m_itemToPage[advancedLog] = PAGE_ADVANCED_LOG;

    logger.debug("SettingsDialog: Navigation tree created with 5 categories, 16 pages");
}

void SettingsDialog::createSettingsPages() {
    // ========================================================================
    // General page (top-level)
    // ========================================================================
    // Page 0: General
    m_pageStack->addWidget(createGeneralPage());

    // ========================================================================
    // Appearance pages (1-4)
    // ========================================================================
    // Page 1: Appearance/General
    m_pageStack->addWidget(createAppearanceGeneralPage());
    // Page 2: Appearance/Theme
    m_pageStack->addWidget(createAppearanceThemePage());
    // Page 3: Appearance/Icons
    m_pageStack->addWidget(createAppearanceIconsPage());
    // Page 4: Appearance/Dashboard
    m_pageStack->addWidget(createAppearanceDashboardPage());

    // ========================================================================
    // Editor pages (5-8)
    // ========================================================================
    // Page 5: Editor/General
    m_pageStack->addWidget(createEditorGeneralPage());
    // Page 6: Editor/Spelling
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Spelling"),
        tr("Spelling settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Spell check language selection\n"
           "- Custom dictionary management\n"
           "- Ignore rules for technical terms")
    ));
    // Page 7: Editor/Auto-correct
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Auto-correct"),
        tr("Auto-correct settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic capitalization\n"
           "- Common typo corrections\n"
           "- Custom replacement rules")
    ));
    // Page 8: Editor/Completion
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Completion"),
        tr("Completion settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Word completion suggestions\n"
           "- Character name completion\n"
           "- Location name completion")
    ));

    // ========================================================================
    // Files pages (9-11)
    // ========================================================================
    // Page 9: Files/Backup
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Backup"),
        tr("Backup settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic backup frequency\n"
           "- Backup location selection\n"
           "- Number of backup copies to keep\n"
           "- Restore from backup")
    ));
    // Page 10: Files/Auto-save
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Auto-save"),
        tr("Auto-save settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Auto-save interval\n"
           "- Auto-save on focus loss\n"
           "- Session recovery options")
    ));
    // Page 11: Files/Import/Export
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Import/Export"),
        tr("Import/Export settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Default export format\n"
           "- Import source preferences\n"
           "- Encoding settings")
    ));

    // ========================================================================
    // Network pages (12-13)
    // ========================================================================
    // Page 12: Network/Cloud Sync
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Cloud Sync"),
        tr("Cloud Sync settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Cloud provider selection\n"
           "- Sync frequency\n"
           "- Conflict resolution\n"
           "- Sync status and history")
    ));
    // Page 13: Network/Updates
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Updates"),
        tr("Update settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic update checks\n"
           "- Update channel (stable/beta)\n"
           "- Plugin updates")
    ));

    // ========================================================================
    // Advanced pages (14-16)
    // ========================================================================
    // Page 14: Advanced/General
    m_pageStack->addWidget(createAdvancedGeneralPage());
    // Page 15: Advanced/Performance
    m_pageStack->addWidget(createPlaceholderPage(
        tr("Performance"),
        tr("Performance settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Memory usage limits\n"
           "- Thread pool configuration\n"
           "- Cache settings\n"
           "- Hardware acceleration")
    ));
    // Page 16: Advanced/Log
    m_pageStack->addWidget(createAdvancedLogPage());
}

QWidget* SettingsDialog::createGeneralPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Startup group
    QGroupBox* startupGroup = new QGroupBox(tr("Startup"));
    QVBoxLayout* startupLayout = new QVBoxLayout(startupGroup);

    m_autoLoadLastProjectCheckBox = new QCheckBox(tr("Open last project on startup"));
    m_autoLoadLastProjectCheckBox->setToolTip(tr("Automatically open the most recently used project when Kalahari starts"));
    startupLayout->addWidget(m_autoLoadLastProjectCheckBox);

    layout->addWidget(startupGroup);
    layout->addStretch();
    return page;
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

    // Note about restart - use mid color from theme for muted text
    QLabel* restartNote = new QLabel(tr("Note: Some changes require application restart."));
    const auto& appearanceTheme = core::ThemeManager::getInstance().getCurrentTheme();
    restartNote->setStyleSheet(QString("color: %1; font-style: italic;")
        .arg(appearanceTheme.palette.mid.name()));
    grid->addWidget(restartNote, 2, 0, 1, 2);

    grid->setColumnStretch(1, 1);
    layout->addWidget(group);
    layout->addStretch();

    return page;
}

QWidget* SettingsDialog::createAppearanceThemePage() {
    // Create the actual content widget
    auto* contentWidget = new QWidget();
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 8, 0);  // Right margin for scrollbar

    // Theme selection row
    QHBoxLayout* themeRow = new QHBoxLayout();
    QLabel* themeLabel = new QLabel(tr("Theme:"));
    m_themeComboBox = new QComboBox();
    m_themeComboBox->addItem(tr("Light"), "Light");
    m_themeComboBox->addItem(tr("Dark"), "Dark");
    connect(m_themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onThemeComboChanged);
    themeRow->addWidget(themeLabel);
    themeRow->addWidget(m_themeComboBox, 1);
    layout->addLayout(themeRow);

    // ========================================================================
    // Icon Colors group
    // ========================================================================
    QGroupBox* iconColorsGroup = new QGroupBox(tr("Icon Colors"));
    QVBoxLayout* iconColorsLayout = new QVBoxLayout(iconColorsGroup);

    m_primaryColorWidget = new ColorConfigWidget(tr("Primary"), iconColorsGroup);
    m_primaryColorWidget->setToolTip(tr("Primary icon color used for main icon elements"));
    iconColorsLayout->addWidget(m_primaryColorWidget);

    m_secondaryColorWidget = new ColorConfigWidget(tr("Secondary"), iconColorsGroup);
    m_secondaryColorWidget->setToolTip(tr("Secondary icon color used for icon accents"));
    iconColorsLayout->addWidget(m_secondaryColorWidget);

    layout->addWidget(iconColorsGroup);

    // ========================================================================
    // UI Colors group (QPalette roles)
    // ========================================================================
    QGroupBox* uiColorsGroup = new QGroupBox(tr("UI Colors"));
    QVBoxLayout* uiColorsLayout = new QVBoxLayout(uiColorsGroup);

    m_tooltipBackgroundColorWidget = new ColorConfigWidget(tr("Tooltip Background"), uiColorsGroup);
    m_tooltipBackgroundColorWidget->setToolTip(tr("Background color for tooltips"));
    uiColorsLayout->addWidget(m_tooltipBackgroundColorWidget);

    m_tooltipTextColorWidget = new ColorConfigWidget(tr("Tooltip Text"), uiColorsGroup);
    m_tooltipTextColorWidget->setToolTip(tr("Text color for tooltips"));
    uiColorsLayout->addWidget(m_tooltipTextColorWidget);

    m_placeholderTextColorWidget = new ColorConfigWidget(tr("Placeholder Text"), uiColorsGroup);
    m_placeholderTextColorWidget->setToolTip(tr("Color for placeholder text in input fields"));
    uiColorsLayout->addWidget(m_placeholderTextColorWidget);

    m_brightTextColorWidget = new ColorConfigWidget(tr("Bright Text"), uiColorsGroup);
    m_brightTextColorWidget->setToolTip(tr("High contrast text color for dark backgrounds"));
    uiColorsLayout->addWidget(m_brightTextColorWidget);

    m_infoHeaderColorWidget = new ColorConfigWidget(tr("Info Header"), uiColorsGroup);
    m_infoHeaderColorWidget->setToolTip(tr("Color for information panel headers"));
    uiColorsLayout->addWidget(m_infoHeaderColorWidget);

    m_infoPrimaryColorWidget = new ColorConfigWidget(tr("Info Primary"), uiColorsGroup);
    m_infoPrimaryColorWidget->setToolTip(tr("Primary color for info panels"));
    uiColorsLayout->addWidget(m_infoPrimaryColorWidget);

    m_infoSecondaryColorWidget = new ColorConfigWidget(tr("Info Secondary"), uiColorsGroup);
    m_infoSecondaryColorWidget->setToolTip(tr("Secondary color for info panels"));
    uiColorsLayout->addWidget(m_infoSecondaryColorWidget);

    layout->addWidget(uiColorsGroup);

    // ========================================================================
    // Palette Colors group (all 16 QPalette roles)
    // ========================================================================
    QGroupBox* paletteColorsGroup = new QGroupBox(tr("Palette Colors"));
    QVBoxLayout* paletteColorsLayout = new QVBoxLayout(paletteColorsGroup);

    // Get theme text color for section labels (must be readable on current theme)
    const auto& themePageTheme = core::ThemeManager::getInstance().getCurrentTheme();
    QString sectionLabelStyle = QString("font-weight: bold; margin-top: 8px; color: %1;")
        .arg(themePageTheme.palette.windowText.name());

    // --- Basic Colors section ---
    QLabel* basicColorsLabel = new QLabel(tr("Basic Colors"));
    basicColorsLabel->setStyleSheet(sectionLabelStyle);
    paletteColorsLayout->addWidget(basicColorsLabel);

    m_paletteWindowColorWidget = new ColorConfigWidget(tr("Window"), paletteColorsGroup);
    m_paletteWindowColorWidget->setToolTip(tr("General background color for windows and panels"));
    paletteColorsLayout->addWidget(m_paletteWindowColorWidget);

    m_paletteWindowTextColorWidget = new ColorConfigWidget(tr("Window Text"), paletteColorsGroup);
    m_paletteWindowTextColorWidget->setToolTip(tr("General text color used throughout the interface"));
    paletteColorsLayout->addWidget(m_paletteWindowTextColorWidget);

    m_paletteBaseColorWidget = new ColorConfigWidget(tr("Base"), paletteColorsGroup);
    m_paletteBaseColorWidget->setToolTip(tr("Background color for input fields and text editors"));
    paletteColorsLayout->addWidget(m_paletteBaseColorWidget);

    m_paletteAlternateBaseColorWidget = new ColorConfigWidget(tr("Alternate Base"), paletteColorsGroup);
    m_paletteAlternateBaseColorWidget->setToolTip(tr("Alternating row background color in lists and tables"));
    paletteColorsLayout->addWidget(m_paletteAlternateBaseColorWidget);

    m_paletteTextColorWidget = new ColorConfigWidget(tr("Text"), paletteColorsGroup);
    m_paletteTextColorWidget->setToolTip(tr("Text color for input fields and text editors"));
    paletteColorsLayout->addWidget(m_paletteTextColorWidget);

    // --- Button Colors section ---
    QLabel* buttonColorsLabel = new QLabel(tr("Button Colors"));
    buttonColorsLabel->setStyleSheet(sectionLabelStyle);
    paletteColorsLayout->addWidget(buttonColorsLabel);

    m_paletteButtonColorWidget = new ColorConfigWidget(tr("Button"), paletteColorsGroup);
    m_paletteButtonColorWidget->setToolTip(tr("Background color for buttons"));
    paletteColorsLayout->addWidget(m_paletteButtonColorWidget);

    m_paletteButtonTextColorWidget = new ColorConfigWidget(tr("Button Text"), paletteColorsGroup);
    m_paletteButtonTextColorWidget->setToolTip(tr("Text color for buttons"));
    paletteColorsLayout->addWidget(m_paletteButtonTextColorWidget);

    // --- Selection Colors section ---
    QLabel* selectionColorsLabel = new QLabel(tr("Selection Colors"));
    selectionColorsLabel->setStyleSheet(sectionLabelStyle);
    paletteColorsLayout->addWidget(selectionColorsLabel);

    m_paletteHighlightColorWidget = new ColorConfigWidget(tr("Highlight"), paletteColorsGroup);
    m_paletteHighlightColorWidget->setToolTip(tr("Background color for selected items"));
    paletteColorsLayout->addWidget(m_paletteHighlightColorWidget);

    m_paletteHighlightedTextColorWidget = new ColorConfigWidget(tr("Highlighted Text"), paletteColorsGroup);
    m_paletteHighlightedTextColorWidget->setToolTip(tr("Text color for selected items"));
    paletteColorsLayout->addWidget(m_paletteHighlightedTextColorWidget);

    // --- 3D Effect Colors section ---
    QLabel* effectColorsLabel = new QLabel(tr("3D Effect Colors"));
    effectColorsLabel->setStyleSheet(sectionLabelStyle);
    paletteColorsLayout->addWidget(effectColorsLabel);

    m_paletteLightColorWidget = new ColorConfigWidget(tr("Light"), paletteColorsGroup);
    m_paletteLightColorWidget->setToolTip(tr("Lightest color for 3D effects (bevels, shadows)"));
    paletteColorsLayout->addWidget(m_paletteLightColorWidget);

    m_paletteMidlightColorWidget = new ColorConfigWidget(tr("Midlight"), paletteColorsGroup);
    m_paletteMidlightColorWidget->setToolTip(tr("Color between Light and Button for 3D effects"));
    paletteColorsLayout->addWidget(m_paletteMidlightColorWidget);

    m_paletteMidColorWidget = new ColorConfigWidget(tr("Mid"), paletteColorsGroup);
    m_paletteMidColorWidget->setToolTip(tr("Medium color for borders and dividers"));
    paletteColorsLayout->addWidget(m_paletteMidColorWidget);

    m_paletteDarkColorWidget = new ColorConfigWidget(tr("Dark"), paletteColorsGroup);
    m_paletteDarkColorWidget->setToolTip(tr("Darker color for 3D effects"));
    paletteColorsLayout->addWidget(m_paletteDarkColorWidget);

    m_paletteShadowColorWidget = new ColorConfigWidget(tr("Shadow"), paletteColorsGroup);
    m_paletteShadowColorWidget->setToolTip(tr("Darkest color for shadows"));
    paletteColorsLayout->addWidget(m_paletteShadowColorWidget);

    // --- Link Colors section ---
    QLabel* linkColorsLabel = new QLabel(tr("Link Colors"));
    linkColorsLabel->setStyleSheet(sectionLabelStyle);
    paletteColorsLayout->addWidget(linkColorsLabel);

    m_paletteLinkColorWidget = new ColorConfigWidget(tr("Link"), paletteColorsGroup);
    m_paletteLinkColorWidget->setToolTip(tr("Color for hyperlinks"));
    paletteColorsLayout->addWidget(m_paletteLinkColorWidget);

    m_paletteLinkVisitedColorWidget = new ColorConfigWidget(tr("Link Visited"), paletteColorsGroup);
    m_paletteLinkVisitedColorWidget->setToolTip(tr("Color for visited hyperlinks"));
    paletteColorsLayout->addWidget(m_paletteLinkVisitedColorWidget);

    layout->addWidget(paletteColorsGroup);

    // ========================================================================
    // Log Panel Colors group
    // ========================================================================
    QGroupBox* logColorsGroup = new QGroupBox(tr("Log Panel Colors"));
    QVBoxLayout* logColorsLayout = new QVBoxLayout(logColorsGroup);

    m_logTraceColorWidget = new ColorConfigWidget(tr("Trace"), logColorsGroup);
    m_logTraceColorWidget->setToolTip(tr("Color for TRACE level log messages (diagnostic mode only)"));
    logColorsLayout->addWidget(m_logTraceColorWidget);

    m_logDebugColorWidget = new ColorConfigWidget(tr("Debug"), logColorsGroup);
    m_logDebugColorWidget->setToolTip(tr("Color for DEBUG level log messages (diagnostic mode only)"));
    logColorsLayout->addWidget(m_logDebugColorWidget);

    m_logInfoColorWidget = new ColorConfigWidget(tr("Info"), logColorsGroup);
    m_logInfoColorWidget->setToolTip(tr("Color for INFO level log messages"));
    logColorsLayout->addWidget(m_logInfoColorWidget);

    m_logWarningColorWidget = new ColorConfigWidget(tr("Warning"), logColorsGroup);
    m_logWarningColorWidget->setToolTip(tr("Color for WARNING level log messages"));
    logColorsLayout->addWidget(m_logWarningColorWidget);

    m_logErrorColorWidget = new ColorConfigWidget(tr("Error"), logColorsGroup);
    m_logErrorColorWidget->setToolTip(tr("Color for ERROR level log messages"));
    logColorsLayout->addWidget(m_logErrorColorWidget);

    m_logCriticalColorWidget = new ColorConfigWidget(tr("Critical"), logColorsGroup);
    m_logCriticalColorWidget->setToolTip(tr("Color for CRITICAL level log messages"));
    logColorsLayout->addWidget(m_logCriticalColorWidget);

    m_logBackgroundColorWidget = new ColorConfigWidget(tr("Background"), logColorsGroup);
    m_logBackgroundColorWidget->setToolTip(tr("Background color of the log panel"));
    logColorsLayout->addWidget(m_logBackgroundColorWidget);

    layout->addWidget(logColorsGroup);

    // ========================================================================
    // Reset button
    // ========================================================================
    QPushButton* resetColorsBtn = new QPushButton(tr("Reset to Theme Defaults"));
    resetColorsBtn->setToolTip(tr("Reset all colors to the default values for the selected theme"));
    connect(resetColorsBtn, &QPushButton::clicked, [this]() {
        QString theme = m_themeComboBox->currentData().toString();
        std::string themeName = theme.toStdString();
        bool isDark = (theme == "Dark");

        // Clear custom colors from storage
        auto& settings = core::SettingsManager::getInstance();
        settings.clearCustomIconColorsForTheme(themeName);
        settings.clearCustomLogColorsForTheme(themeName);
        settings.clearCustomUiColorsForTheme(themeName);
        settings.clearCustomPaletteColorsForTheme(themeName);

        // Reset icon colors to theme defaults
        if (isDark) {
            m_primaryColorWidget->setColor(QColor("#999999"));
            m_secondaryColorWidget->setColor(QColor("#333333"));
        } else {
            m_primaryColorWidget->setColor(QColor("#333333"));
            m_secondaryColorWidget->setColor(QColor("#999999"));
        }

        // Reset UI colors to theme defaults (values from theme.cpp)
        m_tooltipBackgroundColorWidget->setColor(isDark ? QColor("#3c3c3c") : QColor("#ffffdc"));
        m_tooltipTextColorWidget->setColor(isDark ? QColor("#e0e0e0") : QColor("#000000"));
        m_placeholderTextColorWidget->setColor(isDark ? QColor("#808080") : QColor("#a0a0a0"));
        m_brightTextColorWidget->setColor(QColor("#ffffff"));

        // Reset palette colors to theme defaults (values from theme.cpp)
        // Basic Colors
        m_paletteWindowColorWidget->setColor(isDark ? QColor("#2d2d2d") : QColor("#f0f0f0"));
        m_paletteWindowTextColorWidget->setColor(isDark ? QColor("#e0e0e0") : QColor("#000000"));
        m_paletteBaseColorWidget->setColor(isDark ? QColor("#252525") : QColor("#ffffff"));
        m_paletteAlternateBaseColorWidget->setColor(isDark ? QColor("#323232") : QColor("#f5f5f5"));
        m_paletteTextColorWidget->setColor(isDark ? QColor("#e0e0e0") : QColor("#000000"));
        // Button Colors
        m_paletteButtonColorWidget->setColor(isDark ? QColor("#404040") : QColor("#e0e0e0"));
        m_paletteButtonTextColorWidget->setColor(isDark ? QColor("#e0e0e0") : QColor("#000000"));
        // Selection Colors
        m_paletteHighlightColorWidget->setColor(isDark ? QColor("#0078d4") : QColor("#0078d4"));
        m_paletteHighlightedTextColorWidget->setColor(isDark ? QColor("#ffffff") : QColor("#ffffff"));
        // 3D Effect Colors
        m_paletteLightColorWidget->setColor(isDark ? QColor("#505050") : QColor("#ffffff"));
        m_paletteMidlightColorWidget->setColor(isDark ? QColor("#404040") : QColor("#e0e0e0"));
        m_paletteMidColorWidget->setColor(isDark ? QColor("#303030") : QColor("#a0a0a0"));
        m_paletteDarkColorWidget->setColor(isDark ? QColor("#202020") : QColor("#606060"));
        m_paletteShadowColorWidget->setColor(isDark ? QColor("#000000") : QColor("#000000"));
        // Link Colors
        m_paletteLinkColorWidget->setColor(isDark ? QColor("#5eb3f0") : QColor("#0078d4"));
        m_paletteLinkVisitedColorWidget->setColor(isDark ? QColor("#b48ade") : QColor("#551a8b"));

        // Reset log colors to theme defaults
        m_logTraceColorWidget->setColor(isDark ? QColor("#FF66FF") : QColor("#CC00CC"));
        m_logDebugColorWidget->setColor(isDark ? QColor("#FF66FF") : QColor("#CC00CC"));
        m_logInfoColorWidget->setColor(isDark ? QColor("#FFFFFF") : QColor("#000000"));
        m_logWarningColorWidget->setColor(isDark ? QColor("#FFA500") : QColor("#FF8C00"));
        m_logErrorColorWidget->setColor(isDark ? QColor("#FF4444") : QColor("#CC0000"));
        m_logCriticalColorWidget->setColor(isDark ? QColor("#FF4444") : QColor("#CC0000"));
        m_logBackgroundColorWidget->setColor(isDark ? QColor("#252525") : QColor("#F5F5F5"));

        core::Logger::getInstance().info("SettingsDialog: Reset all colors to theme defaults for '{}'", themeName);
    });
    layout->addWidget(resetColorsBtn);

    layout->addStretch();

    // Wrap in scroll area for Theme page (lots of color settings)
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidget(contentWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // Create wrapper page to return
    auto* page = new QWidget();
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);

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

QWidget* SettingsDialog::createAppearanceDashboardPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Dashboard Content group
    QGroupBox* contentGroup = new QGroupBox(tr("Dashboard Content"));
    QVBoxLayout* contentLayout = new QVBoxLayout(contentGroup);

    m_showKalahariNewsCheckBox = new QCheckBox(tr("Show Kalahari News"));
    m_showKalahariNewsCheckBox->setToolTip(tr("Display news and updates section on Dashboard"));
    contentLayout->addWidget(m_showKalahariNewsCheckBox);

    m_showRecentFilesCheckBox = new QCheckBox(tr("Show Recent Files"));
    m_showRecentFilesCheckBox->setToolTip(tr("Display recently opened projects on Dashboard"));
    contentLayout->addWidget(m_showRecentFilesCheckBox);

    layout->addWidget(contentGroup);
    layout->addStretch();
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

    // Warning - use theme-aware warning colors
    QLabel* warningLabel = new QLabel(
        tr("Warning: These settings are for advanced users and developers.\n"
           "Incorrect configuration may affect application stability.")
    );
    warningLabel->setWordWrap(true);
    const auto& advTheme = core::ThemeManager::getInstance().getCurrentTheme();
    bool isDarkAdvanced = advTheme.palette.window.lightnessF() < 0.5;
    QString warningTextColor = isDarkAdvanced ? "#ff9933" : "#ff6600";
    QString warningBgColor = isDarkAdvanced ? "#4a3000" : "#fff3e0";
    QString warningBorderColor = isDarkAdvanced ? "#996600" : "#ffcc80";
    warningLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; padding: 10px; "
                                 "background-color: %2; border: 1px solid %3; }")
                                 .arg(warningTextColor).arg(warningBgColor).arg(warningBorderColor));
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
    diagNote->setStyleSheet(QString("color: %1; margin-left: 20px;")
        .arg(advTheme.palette.mid.name()));
    diagLayout->addWidget(diagNote);

    layout->addWidget(diagGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createAdvancedLogPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Log Panel Settings group
    QGroupBox* logGroup = new QGroupBox(tr("Log Panel Settings"));
    QGridLayout* logGrid = new QGridLayout(logGroup);

    // Buffer Size
    QLabel* bufferLabel = new QLabel(tr("Buffer Size (lines):"));
    bufferLabel->setToolTip(tr("Maximum number of log entries to keep in memory"));
    m_logBufferSizeSpinBox = new QSpinBox();
    m_logBufferSizeSpinBox->setRange(1, 1000);
    m_logBufferSizeSpinBox->setValue(500);
    m_logBufferSizeSpinBox->setSuffix(tr(" lines"));
    m_logBufferSizeSpinBox->setToolTip(tr("Higher values use more memory but keep more history"));

    logGrid->addWidget(bufferLabel, 0, 0);
    logGrid->addWidget(m_logBufferSizeSpinBox, 0, 1);

    // Help text - use mid color from theme for muted text
    const auto& logTheme = core::ThemeManager::getInstance().getCurrentTheme();
    QLabel* helpLabel = new QLabel(
        tr("The log panel displays application messages in real-time.\n\n"
           "Buffer size determines how many log entries are kept in memory.\n"
           "When the buffer is full, oldest entries are removed.\n\n"
           "Note: Log files are always saved to disk regardless of this setting.")
    );
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QString("color: %1; margin-top: 10px;")
        .arg(logTheme.palette.mid.name()));

    logGrid->addWidget(helpLabel, 1, 0, 1, 2);
    logGrid->setColumnStretch(1, 1);
    layout->addWidget(logGroup);

    // Log File Info group
    QGroupBox* fileGroup = new QGroupBox(tr("Log File"));
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);

    QLabel* fileInfo = new QLabel(
        tr("Log files are stored in the application directory:\n"
           "• kalahari.log - Current session log\n\n"
           "Use the log panel toolbar buttons to:\n"
           "• Open log folder in file explorer\n"
           "• Copy log contents to clipboard\n"
           "• Clear the log panel display")
    );
    fileInfo->setWordWrap(true);
    fileInfo->setStyleSheet(QString("color: %1;").arg(logTheme.palette.mid.name()));
    fileLayout->addWidget(fileInfo);

    layout->addWidget(fileGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createPlaceholderPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    const auto& placeholderTheme = core::ThemeManager::getInstance().getCurrentTheme();

    // Use windowText for readable text on window background
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;")
        .arg(placeholderTheme.palette.windowText.name()));
    layout->addWidget(titleLabel);

    // Use placeholderText for muted description text (still readable, but subtle)
    QLabel* descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QString("color: %1; margin-top: 20px;")
        .arg(placeholderTheme.palette.placeholderText.name()));
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
    logger.debug("SettingsDialog: OK clicked");

    // Collect current settings
    SettingsData settings = collectSettings();

    // Only apply if settings actually changed (dirty check)
    if (settings != m_originalSettings) {
        logger.debug("SettingsDialog: Settings changed, applying with spinner");
        applySettingsWithSpinner(settings);
        m_originalSettings = settings;
    } else {
        logger.debug("SettingsDialog: No changes detected, skipping apply");
    }

    // Close dialog
    accept();
}

void SettingsDialog::onReject() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Cancel clicked");
    reject();
}

void SettingsDialog::onApply() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Apply clicked");

    // Collect current settings
    SettingsData settings = collectSettings();

    // Only apply if settings actually changed (dirty check)
    if (settings != m_originalSettings) {
        logger.debug("SettingsDialog: Settings changed, applying with spinner");
        applySettingsWithSpinner(settings);
        m_originalSettings = settings;
    } else {
        logger.debug("SettingsDialog: No changes detected, skipping apply");
    }

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

void SettingsDialog::onThemeComboChanged(int index) {
    Q_UNUSED(index);
    QString theme = m_themeComboBox->currentData().toString();
    std::string themeName = theme.toStdString();
    bool isDark = (theme == "Dark");
    auto& logger = core::Logger::getInstance();

    // Theme defaults:
    // Light: primary=#333333 (dark gray for icons), secondary=#999999 (light gray)
    // Dark: primary=#999999 (light gray for icons), secondary=#333333 (dark gray)
    std::string defaultPrimary = isDark ? "#999999" : "#333333";
    std::string defaultSecondary = isDark ? "#333333" : "#999999";
    // Info header color (elegant navy blue)
    std::string defaultInfoHeader = isDark ? "#4A7A9E" : "#2B4763";
    // Secondary info color for panels
    std::string defaultInfoSecondary = isDark ? "#8FAED4" : "#5B8AC0";
    // Primary info color for panels
    std::string defaultInfoPrimary = isDark ? "#6B9BD2" : "#3D6A99";

    // UI color defaults per theme (QPalette roles)
    std::string defToolTipBase = isDark ? "#3c3c3c" : "#ffffdc";
    std::string defToolTipText = isDark ? "#e0e0e0" : "#000000";
    std::string defPlaceholderText = isDark ? "#808080" : "#a0a0a0";
    std::string defBrightText = "#ffffff";

    // Log color defaults per theme
    std::string defTrace = isDark ? "#FF66FF" : "#CC00CC";
    std::string defDebug = isDark ? "#FF66FF" : "#CC00CC";
    std::string defInfo = isDark ? "#FFFFFF" : "#000000";
    std::string defWarning = isDark ? "#FFA500" : "#FF8C00";
    std::string defError = isDark ? "#FF4444" : "#CC0000";
    std::string defCritical = isDark ? "#FF4444" : "#CC0000";
    std::string defBackground = isDark ? "#252525" : "#F5F5F5";

    // Check if user has custom icon colors for this theme (Task #00025)
    auto& settings = core::SettingsManager::getInstance();
    if (settings.hasCustomIconColorsForTheme(themeName)) {
        // Load user's custom colors for this theme
        std::string primary = settings.getIconColorPrimaryForTheme(themeName, defaultPrimary);
        std::string secondary = settings.getIconColorSecondaryForTheme(themeName, defaultSecondary);
        m_primaryColorWidget->setColor(QColor(QString::fromStdString(primary)));
        m_secondaryColorWidget->setColor(QColor(QString::fromStdString(secondary)));
        logger.debug("SettingsDialog: Loaded custom icon colors for theme '{}': primary={}, secondary={}",
                     themeName, primary, secondary);
    } else {
        // Use theme defaults
        m_primaryColorWidget->setColor(QColor(QString::fromStdString(defaultPrimary)));
        m_secondaryColorWidget->setColor(QColor(QString::fromStdString(defaultSecondary)));
        logger.debug("SettingsDialog: Using default icon colors for theme '{}': primary={}, secondary={}",
                     themeName, defaultPrimary, defaultSecondary);
    }

    // Info header color - use theme default (custom per-theme storage not yet implemented)
    m_infoHeaderColorWidget->setColor(QColor(QString::fromStdString(defaultInfoHeader)));
    m_infoSecondaryColorWidget->setColor(QColor(QString::fromStdString(defaultInfoSecondary)));
    m_infoPrimaryColorWidget->setColor(QColor(QString::fromStdString(defaultInfoPrimary)));

    // Check if user has custom UI colors for this theme (Task #00028)
    if (settings.hasCustomUiColorsForTheme(themeName)) {
        m_tooltipBackgroundColorWidget->setColor(QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipBase", defToolTipBase))));
        m_tooltipTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipText", defToolTipText))));
        m_placeholderTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "placeholderText", defPlaceholderText))));
        m_brightTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "brightText", defBrightText))));
        logger.debug("SettingsDialog: Loaded custom UI colors for theme '{}'", themeName);
    } else {
        m_tooltipBackgroundColorWidget->setColor(QColor(QString::fromStdString(defToolTipBase)));
        m_tooltipTextColorWidget->setColor(QColor(QString::fromStdString(defToolTipText)));
        m_placeholderTextColorWidget->setColor(QColor(QString::fromStdString(defPlaceholderText)));
        m_brightTextColorWidget->setColor(QColor(QString::fromStdString(defBrightText)));
        logger.debug("SettingsDialog: Using default UI colors for theme '{}'", themeName);
    }

    // Check if user has custom log colors for this theme (Task #00027)
    bool useStoredLogColors = false;
    if (settings.hasCustomLogColorsForTheme(themeName)) {
        // Validate stored colors - check if they're corrupted (all #000000)
        // This can happen if settings were saved before the bug fix in getCurrentSettingsAsData()
        std::string storedTrace = settings.getLogColorForTheme(themeName, "trace", defTrace);
        std::string storedWarning = settings.getLogColorForTheme(themeName, "warning", defWarning);
        std::string storedError = settings.getLogColorForTheme(themeName, "error", defError);

        // If trace, warning, AND error are all #000000, data is corrupted (these should never all be black)
        bool corrupted = (storedTrace == "#000000" && storedWarning == "#000000" && storedError == "#000000");

        if (corrupted) {
            logger.warn("SettingsDialog: Detected corrupted log colors for theme '{}', clearing and using defaults", themeName);
            settings.clearCustomLogColorsForTheme(themeName);
            useStoredLogColors = false;
        } else {
            useStoredLogColors = true;
        }
    }

    if (useStoredLogColors) {
        m_logTraceColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "trace", defTrace))));
        m_logDebugColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "debug", defDebug))));
        m_logInfoColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "info", defInfo))));
        m_logWarningColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "warning", defWarning))));
        m_logErrorColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "error", defError))));
        m_logCriticalColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "critical", defCritical))));
        m_logBackgroundColorWidget->setColor(QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "background", defBackground))));
        logger.debug("SettingsDialog: Loaded custom log colors for theme '{}'", themeName);
    } else {
        m_logTraceColorWidget->setColor(QColor(QString::fromStdString(defTrace)));
        m_logDebugColorWidget->setColor(QColor(QString::fromStdString(defDebug)));
        m_logInfoColorWidget->setColor(QColor(QString::fromStdString(defInfo)));
        m_logWarningColorWidget->setColor(QColor(QString::fromStdString(defWarning)));
        m_logErrorColorWidget->setColor(QColor(QString::fromStdString(defError)));
        m_logCriticalColorWidget->setColor(QColor(QString::fromStdString(defCritical)));
        m_logBackgroundColorWidget->setColor(QColor(QString::fromStdString(defBackground)));
        logger.debug("SettingsDialog: Using default log colors for theme '{}'", themeName);
    }

    // Palette color defaults per theme
    std::string defWindow = isDark ? "#2d2d2d" : "#f0f0f0";
    std::string defWindowText = isDark ? "#e0e0e0" : "#000000";
    std::string defBase = isDark ? "#252525" : "#ffffff";
    std::string defAlternateBase = isDark ? "#323232" : "#f5f5f5";
    std::string defText = isDark ? "#e0e0e0" : "#000000";
    std::string defButton = isDark ? "#404040" : "#e0e0e0";
    std::string defButtonText = isDark ? "#e0e0e0" : "#000000";
    std::string defHighlight = "#0078d4";
    std::string defHighlightedText = "#ffffff";
    std::string defLight = isDark ? "#505050" : "#ffffff";
    std::string defMidlight = isDark ? "#404040" : "#e0e0e0";
    std::string defMid = isDark ? "#303030" : "#a0a0a0";
    std::string defDark = isDark ? "#202020" : "#606060";
    std::string defShadow = "#000000";
    std::string defLink = isDark ? "#5eb3f0" : "#0078d4";
    std::string defLinkVisited = isDark ? "#b48ade" : "#551a8b";

    // Check if user has custom palette colors for this theme
    if (settings.hasCustomPaletteColorsForTheme(themeName)) {
        m_paletteWindowColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "window", defWindow))));
        m_paletteWindowTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "windowText", defWindowText))));
        m_paletteBaseColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "base", defBase))));
        m_paletteAlternateBaseColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "alternateBase", defAlternateBase))));
        m_paletteTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "text", defText))));
        m_paletteButtonColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "button", defButton))));
        m_paletteButtonTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "buttonText", defButtonText))));
        m_paletteHighlightColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlight", defHighlight))));
        m_paletteHighlightedTextColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlightedText", defHighlightedText))));
        m_paletteLightColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "light", defLight))));
        m_paletteMidlightColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "midlight", defMidlight))));
        m_paletteMidColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "mid", defMid))));
        m_paletteDarkColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "dark", defDark))));
        m_paletteShadowColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "shadow", defShadow))));
        m_paletteLinkColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "link", defLink))));
        m_paletteLinkVisitedColorWidget->setColor(QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "linkVisited", defLinkVisited))));
        logger.debug("SettingsDialog: Loaded custom palette colors for theme '{}'", themeName);
    } else {
        m_paletteWindowColorWidget->setColor(QColor(QString::fromStdString(defWindow)));
        m_paletteWindowTextColorWidget->setColor(QColor(QString::fromStdString(defWindowText)));
        m_paletteBaseColorWidget->setColor(QColor(QString::fromStdString(defBase)));
        m_paletteAlternateBaseColorWidget->setColor(QColor(QString::fromStdString(defAlternateBase)));
        m_paletteTextColorWidget->setColor(QColor(QString::fromStdString(defText)));
        m_paletteButtonColorWidget->setColor(QColor(QString::fromStdString(defButton)));
        m_paletteButtonTextColorWidget->setColor(QColor(QString::fromStdString(defButtonText)));
        m_paletteHighlightColorWidget->setColor(QColor(QString::fromStdString(defHighlight)));
        m_paletteHighlightedTextColorWidget->setColor(QColor(QString::fromStdString(defHighlightedText)));
        m_paletteLightColorWidget->setColor(QColor(QString::fromStdString(defLight)));
        m_paletteMidlightColorWidget->setColor(QColor(QString::fromStdString(defMidlight)));
        m_paletteMidColorWidget->setColor(QColor(QString::fromStdString(defMid)));
        m_paletteDarkColorWidget->setColor(QColor(QString::fromStdString(defDark)));
        m_paletteShadowColorWidget->setColor(QColor(QString::fromStdString(defShadow)));
        m_paletteLinkColorWidget->setColor(QColor(QString::fromStdString(defLink)));
        m_paletteLinkVisitedColorWidget->setColor(QColor(QString::fromStdString(defLinkVisited)));
        logger.debug("SettingsDialog: Using default palette colors for theme '{}'", themeName);
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
    int iconSize = 24;  // Simple 24px icons - Qt handles DPI scaling automatically

    // Get colors from ColorConfigWidgets
    QColor primaryColor = m_primaryColorWidget ? m_primaryColorWidget->color() : QColor("#424242");
    QColor secondaryColor = m_secondaryColorWidget ? m_secondaryColorWidget->color() : QColor("#757575");

    // Sample icons to preview
    QStringList sampleIcons = {
        "file.new", "file.open", "file.save",
        "edit.undo", "edit.redo", "edit.copy"
    };

    for (const QString& cmdId : sampleIcons) {
        // Get icon with UI colors (not cached theme colors!)
        QIcon icon = core::IconRegistry::getInstance().getIconWithColors(
            cmdId, iconTheme, iconSize, primaryColor, secondaryColor);
        if (!icon.isNull()) {
            QLabel* iconLabel = new QLabel();
            iconLabel->setPixmap(icon.pixmap(iconSize, iconSize));
            iconLabel->setFixedSize(iconSize, iconSize);
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
    m_primaryColorWidget->setColor(settings.primaryColor);
    m_secondaryColorWidget->setColor(settings.secondaryColor);
    m_infoHeaderColorWidget->setColor(settings.infoHeaderColor);
    m_infoSecondaryColorWidget->setColor(settings.infoSecondaryColor);
    m_infoPrimaryColorWidget->setColor(settings.infoPrimaryColor);

    // UI colors (QPalette roles)
    m_tooltipBackgroundColorWidget->setColor(settings.tooltipBackgroundColor);
    m_tooltipTextColorWidget->setColor(settings.tooltipTextColor);
    m_placeholderTextColorWidget->setColor(settings.placeholderTextColor);
    m_brightTextColorWidget->setColor(settings.brightTextColor);

    // Palette colors (all 16 QPalette roles)
    m_paletteWindowColorWidget->setColor(settings.paletteWindowColor);
    m_paletteWindowTextColorWidget->setColor(settings.paletteWindowTextColor);
    m_paletteBaseColorWidget->setColor(settings.paletteBaseColor);
    m_paletteAlternateBaseColorWidget->setColor(settings.paletteAlternateBaseColor);
    m_paletteTextColorWidget->setColor(settings.paletteTextColor);
    m_paletteButtonColorWidget->setColor(settings.paletteButtonColor);
    m_paletteButtonTextColorWidget->setColor(settings.paletteButtonTextColor);
    m_paletteHighlightColorWidget->setColor(settings.paletteHighlightColor);
    m_paletteHighlightedTextColorWidget->setColor(settings.paletteHighlightedTextColor);
    m_paletteLightColorWidget->setColor(settings.paletteLightColor);
    m_paletteMidlightColorWidget->setColor(settings.paletteMidlightColor);
    m_paletteMidColorWidget->setColor(settings.paletteMidColor);
    m_paletteDarkColorWidget->setColor(settings.paletteDarkColor);
    m_paletteShadowColorWidget->setColor(settings.paletteShadowColor);
    m_paletteLinkColorWidget->setColor(settings.paletteLinkColor);
    m_paletteLinkVisitedColorWidget->setColor(settings.paletteLinkVisitedColor);

    // Log colors
    m_logTraceColorWidget->setColor(settings.logTraceColor);
    m_logDebugColorWidget->setColor(settings.logDebugColor);
    m_logInfoColorWidget->setColor(settings.logInfoColor);
    m_logWarningColorWidget->setColor(settings.logWarningColor);
    m_logErrorColorWidget->setColor(settings.logErrorColor);
    m_logCriticalColorWidget->setColor(settings.logCriticalColor);
    m_logBackgroundColorWidget->setColor(settings.logBackgroundColor);

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

    // Advanced/Log
    m_logBufferSizeSpinBox->setValue(settings.logBufferSize);

    // General
    m_autoLoadLastProjectCheckBox->setChecked(settings.autoLoadLastProject);

    // Appearance/Dashboard
    m_showKalahariNewsCheckBox->setChecked(settings.showKalahariNews);
    m_showRecentFilesCheckBox->setChecked(settings.showRecentFiles);

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
    settingsData.primaryColor = m_primaryColorWidget->color();
    settingsData.secondaryColor = m_secondaryColorWidget->color();
    settingsData.infoHeaderColor = m_infoHeaderColorWidget->color();
    settingsData.infoSecondaryColor = m_infoSecondaryColorWidget->color();
    settingsData.infoPrimaryColor = m_infoPrimaryColorWidget->color();

    // UI colors (QPalette roles)
    settingsData.tooltipBackgroundColor = m_tooltipBackgroundColorWidget->color();
    settingsData.tooltipTextColor = m_tooltipTextColorWidget->color();
    settingsData.placeholderTextColor = m_placeholderTextColorWidget->color();
    settingsData.brightTextColor = m_brightTextColorWidget->color();

    // Palette colors (all 16 QPalette roles)
    settingsData.paletteWindowColor = m_paletteWindowColorWidget->color();
    settingsData.paletteWindowTextColor = m_paletteWindowTextColorWidget->color();
    settingsData.paletteBaseColor = m_paletteBaseColorWidget->color();
    settingsData.paletteAlternateBaseColor = m_paletteAlternateBaseColorWidget->color();
    settingsData.paletteTextColor = m_paletteTextColorWidget->color();
    settingsData.paletteButtonColor = m_paletteButtonColorWidget->color();
    settingsData.paletteButtonTextColor = m_paletteButtonTextColorWidget->color();
    settingsData.paletteHighlightColor = m_paletteHighlightColorWidget->color();
    settingsData.paletteHighlightedTextColor = m_paletteHighlightedTextColorWidget->color();
    settingsData.paletteLightColor = m_paletteLightColorWidget->color();
    settingsData.paletteMidlightColor = m_paletteMidlightColorWidget->color();
    settingsData.paletteMidColor = m_paletteMidColorWidget->color();
    settingsData.paletteDarkColor = m_paletteDarkColorWidget->color();
    settingsData.paletteShadowColor = m_paletteShadowColorWidget->color();
    settingsData.paletteLinkColor = m_paletteLinkColorWidget->color();
    settingsData.paletteLinkVisitedColor = m_paletteLinkVisitedColorWidget->color();

    // Log colors
    settingsData.logTraceColor = m_logTraceColorWidget->color();
    settingsData.logDebugColor = m_logDebugColorWidget->color();
    settingsData.logInfoColor = m_logInfoColorWidget->color();
    settingsData.logWarningColor = m_logWarningColorWidget->color();
    settingsData.logErrorColor = m_logErrorColorWidget->color();
    settingsData.logCriticalColor = m_logCriticalColorWidget->color();
    settingsData.logBackgroundColor = m_logBackgroundColorWidget->color();

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

    // Advanced/Log
    settingsData.logBufferSize = m_logBufferSizeSpinBox->value();

    // General
    settingsData.autoLoadLastProject = m_autoLoadLastProjectCheckBox->isChecked();

    // Appearance/Dashboard
    settingsData.showKalahariNews = m_showKalahariNewsCheckBox->isChecked();
    settingsData.showRecentFiles = m_showRecentFilesCheckBox->isChecked();

    logger.debug("SettingsDialog: Settings collected");
    return settingsData;
}


void SettingsDialog::applySettingsWithSpinner(const SettingsData& settings) {
    auto& logger = core::Logger::getInstance();

    // Show BusyIndicator on THIS DIALOG (modal overlay)
    // Use BusyIndicator::tick() between steps to keep animation alive
    BusyIndicator::run(this, tr("Applying settings..."), [&settings, &logger]() {
        auto& settingsManager = core::SettingsManager::getInstance();
        auto& themeManager = core::ThemeManager::getInstance();
        auto& artProvider = core::ArtProvider::getInstance();

        // =====================================================================
        // CRITICAL: Enable batch mode to prevent multiple resourcesChanged emissions
        // Without this, each setIconTheme/setPrimaryColor/setSecondaryColor/setIconSize
        // emits resourcesChanged, causing ~600-700 icon re-renders (134 actions × ~5 signals)
        // Batch mode coalesces all changes into a single resourcesChanged at the end
        // =====================================================================
        artProvider.beginBatchUpdate();

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

        // Save per-theme icon colors
        std::string themeName = settings.theme.toStdString();
        settingsManager.setIconColorPrimaryForTheme(themeName, settings.primaryColor.name().toStdString());
        settingsManager.setIconColorSecondaryForTheme(themeName, settings.secondaryColor.name().toStdString());

        // Save per-theme UI colors (Task #00028)
        settingsManager.setUiColorForTheme(themeName, "toolTipBase", settings.tooltipBackgroundColor.name().toStdString());
        settingsManager.setUiColorForTheme(themeName, "toolTipText", settings.tooltipTextColor.name().toStdString());
        settingsManager.setUiColorForTheme(themeName, "placeholderText", settings.placeholderTextColor.name().toStdString());
        settingsManager.setUiColorForTheme(themeName, "brightText", settings.brightTextColor.name().toStdString());

        // Save per-theme log colors (Task #00027)
        settingsManager.setLogColorForTheme(themeName, "trace", settings.logTraceColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "debug", settings.logDebugColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "info", settings.logInfoColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "warning", settings.logWarningColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "error", settings.logErrorColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "critical", settings.logCriticalColor.name().toStdString());
        settingsManager.setLogColorForTheme(themeName, "background", settings.logBackgroundColor.name().toStdString());

        // Save per-theme palette colors (Task #00028)
        settingsManager.setPaletteColorForTheme(themeName, "window", settings.paletteWindowColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "windowText", settings.paletteWindowTextColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "base", settings.paletteBaseColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "alternateBase", settings.paletteAlternateBaseColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "text", settings.paletteTextColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "button", settings.paletteButtonColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "buttonText", settings.paletteButtonTextColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "highlight", settings.paletteHighlightColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "highlightedText", settings.paletteHighlightedTextColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "light", settings.paletteLightColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "midlight", settings.paletteMidlightColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "mid", settings.paletteMidColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "dark", settings.paletteDarkColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "shadow", settings.paletteShadowColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "link", settings.paletteLinkColor.name().toStdString());
        settingsManager.setPaletteColorForTheme(themeName, "linkVisited", settings.paletteLinkVisitedColor.name().toStdString());

        // Apply palette color overrides to ThemeManager (Task #00028)
        // These override the theme's default palette and are applied to QApplication
        themeManager.setColorOverride("palette.window", settings.paletteWindowColor);
        themeManager.setColorOverride("palette.windowText", settings.paletteWindowTextColor);
        themeManager.setColorOverride("palette.base", settings.paletteBaseColor);
        themeManager.setColorOverride("palette.alternateBase", settings.paletteAlternateBaseColor);
        themeManager.setColorOverride("palette.text", settings.paletteTextColor);
        themeManager.setColorOverride("palette.button", settings.paletteButtonColor);
        themeManager.setColorOverride("palette.buttonText", settings.paletteButtonTextColor);
        themeManager.setColorOverride("palette.highlight", settings.paletteHighlightColor);
        themeManager.setColorOverride("palette.highlightedText", settings.paletteHighlightedTextColor);
        themeManager.setColorOverride("palette.light", settings.paletteLightColor);
        themeManager.setColorOverride("palette.midlight", settings.paletteMidlightColor);
        themeManager.setColorOverride("palette.mid", settings.paletteMidColor);
        themeManager.setColorOverride("palette.dark", settings.paletteDarkColor);
        themeManager.setColorOverride("palette.shadow", settings.paletteShadowColor);
        themeManager.setColorOverride("palette.link", settings.paletteLinkColor);
        themeManager.setColorOverride("palette.linkVisited", settings.paletteLinkVisitedColor);

        // Apply UI color overrides (tooltip, placeholder, brightText)
        themeManager.setColorOverride("palette.toolTipBase", settings.tooltipBackgroundColor);
        themeManager.setColorOverride("palette.toolTipText", settings.tooltipTextColor);
        themeManager.setColorOverride("palette.placeholderText", settings.placeholderTextColor);
        themeManager.setColorOverride("palette.brightText", settings.brightTextColor);

        // Apply info panel color overrides (Dashboard News icons use these)
        themeManager.setColorOverride("colors.infoHeader", settings.infoHeaderColor);
        themeManager.setColorOverride("colors.infoPrimary", settings.infoPrimaryColor);
        themeManager.setColorOverride("colors.infoSecondary", settings.infoSecondaryColor);

        // Apply log color overrides (Task #00027)
        themeManager.setColorOverride("log.trace", settings.logTraceColor);
        themeManager.setColorOverride("log.debug", settings.logDebugColor);
        themeManager.setColorOverride("log.info", settings.logInfoColor);
        themeManager.setColorOverride("log.warning", settings.logWarningColor);
        themeManager.setColorOverride("log.error", settings.logErrorColor);
        themeManager.setColorOverride("log.critical", settings.logCriticalColor);
        themeManager.setColorOverride("log.background", settings.logBackgroundColor);

        // Refresh the theme to apply all color overrides (palette + stylesheet)
        themeManager.refreshTheme();

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
        // Step 5: Advanced/Log
        // =====================================================================
        settingsManager.set("log.bufferSize", settings.logBufferSize);

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 6: Appearance/Dashboard
        // =====================================================================
        settingsManager.set("dashboard.showKalahariNews", settings.showKalahariNews);
        settingsManager.set("dashboard.showRecentFiles", settings.showRecentFiles);
        settingsManager.set("startup.autoLoadLastProject", settings.autoLoadLastProject);

        BusyIndicator::tick();  // Animate

        // =====================================================================
        // Step 7: Save to disk
        // =====================================================================
        settingsManager.save();

        // =====================================================================
        // CRITICAL: End batch mode - emit single resourcesChanged signal
        // This triggers ONE refresh of all 134 actions instead of ~11 refreshes
        // =====================================================================
        artProvider.endBatchUpdate();

        logger.info("SettingsDialog: All settings persisted to disk");
    });

    // Emit signal AFTER spinner finished (MainWindow can react)
    emit settingsApplied(settings);
}

} // namespace gui
} // namespace kalahari
