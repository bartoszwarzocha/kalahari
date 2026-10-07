/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog
///
/// Architecture: Dialog collects data, writes the changed settings and emits settingsApplied.
/// See settings_dialog.h for detailed flow description.

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/editor/editor_appearance.h"  // For CursorStyle enum

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
#include <QDoubleSpinBox>
#include <cmath>
#include <QFontComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <QScreen>
#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <map>
#include <utility>

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
    , m_dashboardSecondaryColorWidget(nullptr)
    , m_dashboardPrimaryColorWidget(nullptr)
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
    , m_dashboardMaxItemsSpinBox(nullptr)
    , m_dashboardIconSizeSpinBox(nullptr)
    , m_fontFamilyComboBox(nullptr)
    , m_editorFontSizeSpinBox(nullptr)
    , m_tabSizeSpinBox(nullptr)
    , m_lineNumbersCheckBox(nullptr)
    , m_wordWrapCheckBox(nullptr)
    , m_lineHeightSpinBox(nullptr)
    , m_paragraphSpacingSpinBox(nullptr)
    , m_firstLineIndentCheckBox(nullptr)
    , m_indentSizeSpinBox(nullptr)
    // Editor colors
    , m_editorDarkModeCheckBox(nullptr)
    , m_editorBackgroundLightWidget(nullptr)
    , m_editorTextLightWidget(nullptr)
    , m_editorInactiveLightWidget(nullptr)
    , m_editorBackgroundDarkWidget(nullptr)
    , m_editorTextDarkWidget(nullptr)
    , m_editorInactiveDarkWidget(nullptr)
    // Cursor settings
    , m_cursorStyleComboBox(nullptr)
    , m_cursorUseCustomColorCheckBox(nullptr)
    , m_cursorColorWidget(nullptr)
    , m_cursorBlinkingCheckBox(nullptr)
    , m_cursorBlinkIntervalSpinBox(nullptr)
    , m_cursorLineWidthSpinBox(nullptr)
    , m_cursorLineWidthLabel(nullptr)
    // Margin settings
    , m_viewMarginHorizontalSpinBox(nullptr)
    , m_viewMarginVerticalSpinBox(nullptr)
    , m_pageMarginTopSpinBox(nullptr)
    , m_pageMarginBottomSpinBox(nullptr)
    , m_pageMarginLeftSpinBox(nullptr)
    , m_pageMarginRightSpinBox(nullptr)
    , m_pageMirrorMarginsCheckBox(nullptr)
    , m_pageMarginInnerSpinBox(nullptr)
    , m_pageMarginOuterSpinBox(nullptr)
    , m_pageMarginLeftLabel(nullptr)
    , m_pageMarginRightLabel(nullptr)
    , m_pageMarginInnerLabel(nullptr)
    , m_pageMarginOuterLabel(nullptr)
    , m_textFrameBorderShowCheckBox(nullptr)
    , m_textFrameBorderColorWidget(nullptr)
    , m_textFrameBorderWidthSpinBox(nullptr)
    , m_diagModeCheckbox(nullptr)
    , m_originalSettings(currentSettings)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Constructor called (new architecture)");

    setWindowTitle(tr("Settings"));
    setModal(true);
    // Tall enough for the longest editor page where the screen allows; a page that does
    // not fit scrolls
    QSize size(800, 700);
    if (const QScreen* screen = this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * 0.9);
    }
    resize(size);
    setMinimumSize(600, 400);

    createUI();
    populateFromSettings(currentSettings);
    m_themeColorBaseline = currentSettings;

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
        // Grayed-out placeholder foreground from the theme's mid tone
        const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
        item->setForeground(0, theme.palette.mid);
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
    // Editor category (7 sub-items: 4 active + 3 placeholders)
    // ========================================================================
    QTreeWidgetItem* editorItem = new QTreeWidgetItem(m_navTree);
    editorItem->setText(0, tr("Editor"));
    editorItem->setExpanded(true);

    QTreeWidgetItem* editorGeneral = new QTreeWidgetItem(editorItem);
    editorGeneral->setText(0, tr("General"));
    m_itemToPage[editorGeneral] = PAGE_EDITOR_GENERAL;

    QTreeWidgetItem* editorColors = new QTreeWidgetItem(editorItem);
    editorColors->setText(0, tr("Colors"));
    m_itemToPage[editorColors] = PAGE_EDITOR_COLORS;

    QTreeWidgetItem* editorCursor = new QTreeWidgetItem(editorItem);
    editorCursor->setText(0, tr("Cursor"));
    m_itemToPage[editorCursor] = PAGE_EDITOR_CURSOR;

    QTreeWidgetItem* editorMargins = new QTreeWidgetItem(editorItem);
    editorMargins->setText(0, tr("Pages and Margins"));
    m_itemToPage[editorMargins] = PAGE_EDITOR_MARGINS;

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
    // Network category (1 sub-item)
    // ========================================================================
    QTreeWidgetItem* networkItem = new QTreeWidgetItem(m_navTree);
    networkItem->setText(0, tr("Network"));
    networkItem->setExpanded(true);

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

    logger.debug("SettingsDialog: Navigation tree created with 5 categories, 19 pages");
}

void SettingsDialog::createSettingsPages() {
    // Each page sits in a scroll area: a page taller than the dialog scrolls instead of
    // squeezing its groups until their fields overlap
    const auto addPage = [this](QWidget* page) {
        auto* scrollArea = new QScrollArea();
        scrollArea->setWidget(page);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        m_pageStack->addWidget(scrollArea);
    };

    // ========================================================================
    // General page (top-level)
    // ========================================================================
    // Page 0: General
    addPage(createGeneralPage());

    // ========================================================================
    // Appearance pages (1-4)
    // ========================================================================
    // Page 1: Appearance/General
    addPage(createAppearanceGeneralPage());
    // Page 2: Appearance/Theme
    addPage(createAppearanceThemePage());
    // Page 3: Appearance/Icons
    addPage(createAppearanceIconsPage());
    // Page 4: Appearance/Dashboard
    addPage(createAppearanceDashboardPage());

    // ========================================================================
    // Editor pages (5-11)
    // ========================================================================
    // Page 5: Editor/General
    addPage(createEditorGeneralPage());
    // Page 6: Editor/Colors
    addPage(createEditorColorsPage());
    // Page 7: Editor/Cursor
    addPage(createEditorCursorPage());
    // Page 8: Editor/Margins
    addPage(createEditorMarginsPage());
    // Page 9: Editor/Spelling
    addPage(createPlaceholderPage(
        tr("Spelling"),
        tr("Spelling settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Spell check language selection\n"
           "- Custom dictionary management\n"
           "- Ignore rules for technical terms")
    ));
    // Page 10: Editor/Auto-correct
    addPage(createPlaceholderPage(
        tr("Auto-correct"),
        tr("Auto-correct settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic capitalization\n"
           "- Common typo corrections\n"
           "- Custom replacement rules")
    ));
    // Page 11: Editor/Completion
    addPage(createPlaceholderPage(
        tr("Completion"),
        tr("Completion settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Word completion suggestions\n"
           "- Character name completion\n"
           "- Location name completion")
    ));

    // ========================================================================
    // Files pages (12-14)
    // ========================================================================
    // Page 12: Files/Backup
    addPage(createPlaceholderPage(
        tr("Backup"),
        tr("Backup settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic backup frequency\n"
           "- Backup location selection\n"
           "- Number of backup copies to keep\n"
           "- Restore from backup")
    ));
    // Page 13: Files/Auto-save
    addPage(createPlaceholderPage(
        tr("Auto-save"),
        tr("Auto-save settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Auto-save interval\n"
           "- Auto-save on focus loss\n"
           "- Session recovery options")
    ));
    // Page 14: Files/Import/Export
    addPage(createPlaceholderPage(
        tr("Import/Export"),
        tr("Import/Export settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Default export format\n"
           "- Import source preferences\n"
           "- Encoding settings")
    ));

    // ========================================================================
    // Network pages (15)
    // ========================================================================
    // Page 15: Network/Updates
    addPage(createPlaceholderPage(
        tr("Updates"),
        tr("Update settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Automatic update checks\n"
           "- Update channel (stable/beta)\n"
           "- Plugin updates")
    ));

    // ========================================================================
    // Advanced pages (16-18)
    // ========================================================================
    // Page 16: Advanced/General
    addPage(createAdvancedGeneralPage());
    // Page 17: Advanced/Performance
    addPage(createPlaceholderPage(
        tr("Performance"),
        tr("Performance settings will be available in a future version.\n\n"
           "Planned features:\n"
           "- Memory usage limits\n"
           "- Thread pool configuration\n"
           "- Cache settings\n"
           "- Hardware acceleration")
    ));
    // Page 18: Advanced/Log
    addPage(createAdvancedLogPage());
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
    // Stored, but nothing applies it yet (planned with the new Settings pages)
    fontSizeLabel->setEnabled(false);
    m_uiFontSizeSpinBox->setEnabled(false);
    m_uiFontSizeSpinBox->setToolTip(tr("Coming in future version"));

    // Note about restart - use mid color from theme for muted text
    QLabel* restartNote = new QLabel(tr("A language change takes effect after restarting Kalahari."));
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

    m_dashboardSecondaryColorWidget = new ColorConfigWidget(tr("Dashboard Secondary"), uiColorsGroup);
    m_dashboardSecondaryColorWidget->setToolTip(tr("Secondary dashboard accent color"));
    uiColorsLayout->addWidget(m_dashboardSecondaryColorWidget);

    m_dashboardPrimaryColorWidget = new ColorConfigWidget(tr("Dashboard Primary"), uiColorsGroup);
    m_dashboardPrimaryColorWidget->setToolTip(tr("Primary dashboard accent color"));
    uiColorsLayout->addWidget(m_dashboardPrimaryColorWidget);

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

        // Show the theme file colors; they are stored on OK/Apply like any other edit,
        // so Cancel keeps the custom colors
        core::Theme defaults;
        try {
            defaults = core::ThemeManager::getInstance().loadTheme(theme);
        } catch (const std::exception& e) {
            core::Logger::getInstance().warn("SettingsDialog: Cannot load theme '{}': {}", themeName, e.what());
            return;
        }

        m_primaryColorWidget->setColor(defaults.colors.primary);
        m_secondaryColorWidget->setColor(defaults.colors.secondary);
        m_infoHeaderColorWidget->setColor(defaults.colors.infoHeader);
        m_dashboardSecondaryColorWidget->setColor(defaults.colors.dashboardSecondary);
        m_dashboardPrimaryColorWidget->setColor(defaults.colors.dashboardPrimary);
        m_infoSecondaryColorWidget->setColor(defaults.colors.infoSecondary);
        m_infoPrimaryColorWidget->setColor(defaults.colors.infoPrimary);

        m_tooltipBackgroundColorWidget->setColor(defaults.palette.toolTipBase);
        m_tooltipTextColorWidget->setColor(defaults.palette.toolTipText);
        m_placeholderTextColorWidget->setColor(defaults.palette.placeholderText);
        m_brightTextColorWidget->setColor(defaults.palette.brightText);

        m_paletteWindowColorWidget->setColor(defaults.palette.window);
        m_paletteWindowTextColorWidget->setColor(defaults.palette.windowText);
        m_paletteBaseColorWidget->setColor(defaults.palette.base);
        m_paletteAlternateBaseColorWidget->setColor(defaults.palette.alternateBase);
        m_paletteTextColorWidget->setColor(defaults.palette.text);
        m_paletteButtonColorWidget->setColor(defaults.palette.button);
        m_paletteButtonTextColorWidget->setColor(defaults.palette.buttonText);
        m_paletteHighlightColorWidget->setColor(defaults.palette.highlight);
        m_paletteHighlightedTextColorWidget->setColor(defaults.palette.highlightedText);
        m_paletteLightColorWidget->setColor(defaults.palette.light);
        m_paletteMidlightColorWidget->setColor(defaults.palette.midlight);
        m_paletteMidColorWidget->setColor(defaults.palette.mid);
        m_paletteDarkColorWidget->setColor(defaults.palette.dark);
        m_paletteShadowColorWidget->setColor(defaults.palette.shadow);
        m_paletteLinkColorWidget->setColor(defaults.palette.link);
        m_paletteLinkVisitedColorWidget->setColor(defaults.palette.linkVisited);

        m_logTraceColorWidget->setColor(defaults.log.trace);
        m_logDebugColorWidget->setColor(defaults.log.debug);
        m_logInfoColorWidget->setColor(defaults.log.info);
        m_logWarningColorWidget->setColor(defaults.log.warning);
        m_logErrorColorWidget->setColor(defaults.log.error);
        m_logCriticalColorWidget->setColor(defaults.log.critical);
        m_logBackgroundColorWidget->setColor(defaults.log.background);

        core::Logger::getInstance().info("SettingsDialog: Reset all colors to theme defaults for '{}'", themeName);
    });
    layout->addWidget(resetColorsBtn);

    layout->addStretch();

    return contentWidget;
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

    // Settings grid (for stretched spinboxes like Icons tab)
    QGridLayout* settingsGrid = new QGridLayout();
    int row = 0;

    // Number of items setting
    QLabel* maxItemsLabel = new QLabel(tr("Maximum items per section:"));
    m_dashboardMaxItemsSpinBox = new QSpinBox();
    m_dashboardMaxItemsSpinBox->setRange(3, 9);
    m_dashboardMaxItemsSpinBox->setValue(5);
    m_dashboardMaxItemsSpinBox->setToolTip(tr("Number of items to show in News and Recent Files sections (3-9)"));
    settingsGrid->addWidget(maxItemsLabel, row, 0);
    settingsGrid->addWidget(m_dashboardMaxItemsSpinBox, row, 1);
    row++;

    // Icon size setting
    QLabel* iconSizeLabel = new QLabel(tr("Icon size:"));
    m_dashboardIconSizeSpinBox = new QSpinBox();
    m_dashboardIconSizeSpinBox->setRange(24, 64);
    m_dashboardIconSizeSpinBox->setValue(48);
    m_dashboardIconSizeSpinBox->setSuffix(" px");
    m_dashboardIconSizeSpinBox->setToolTip(tr("Size of icons in Dashboard panels (24-64 pixels)"));
    settingsGrid->addWidget(iconSizeLabel, row, 0);
    settingsGrid->addWidget(m_dashboardIconSizeSpinBox, row, 1);

    settingsGrid->setColumnStretch(1, 1);
    contentLayout->addLayout(settingsGrid);

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
    m_fontFamilyComboBox->setFontFilters(QFontComboBox::AllFonts);
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

    // Typography group (a view setting: the chapter files are not changed)
    QGroupBox* typographyGroup = new QGroupBox(tr("Typography"));
    QGridLayout* typographyGrid = new QGridLayout(typographyGroup);

    QLabel* lineHeightLabel = new QLabel(tr("Line Spacing:"));
    m_lineHeightSpinBox = new QDoubleSpinBox();
    m_lineHeightSpinBox->setRange(1.0, 3.0);
    m_lineHeightSpinBox->setSingleStep(0.1);
    m_lineHeightSpinBox->setDecimals(1);
    m_lineHeightSpinBox->setToolTip(tr("Multiple of the font's line height (1.0 = single spacing)"));
    typographyGrid->addWidget(lineHeightLabel, 0, 0);
    typographyGrid->addWidget(m_lineHeightSpinBox, 0, 1);

    QLabel* paragraphSpacingLabel = new QLabel(tr("Space After Paragraph:"));
    m_paragraphSpacingSpinBox = new QSpinBox();
    m_paragraphSpacingSpinBox->setRange(0, 48);
    m_paragraphSpacingSpinBox->setSuffix(" px");
    typographyGrid->addWidget(paragraphSpacingLabel, 1, 0);
    typographyGrid->addWidget(m_paragraphSpacingSpinBox, 1, 1);

    m_firstLineIndentCheckBox = new QCheckBox(tr("Indent First Line:"));
    m_indentSizeSpinBox = new QSpinBox();
    m_indentSizeSpinBox->setRange(0, 96);
    m_indentSizeSpinBox->setSuffix(" px");
    typographyGrid->addWidget(m_firstLineIndentCheckBox, 2, 0);
    typographyGrid->addWidget(m_indentSizeSpinBox, 2, 1);
    connect(m_firstLineIndentCheckBox, &QCheckBox::toggled,
            m_indentSizeSpinBox, &QSpinBox::setEnabled);

    typographyGrid->setColumnStretch(1, 1);
    layout->addWidget(typographyGroup);

    // Typewriter scrolling (turned on and off with View > Typewriter Scrolling)
    QGroupBox* typewriterGroup = new QGroupBox(tr("Typewriter Scrolling"));
    QGridLayout* typewriterGrid = new QGridLayout(typewriterGroup);

    QLabel* typewriterInfo = new QLabel(
        tr("View > Typewriter Scrolling (Ctrl+3) keeps the line you write at one height of the "
           "view, in the Continuous and the Page Layout view."));
    typewriterInfo->setWordWrap(true);
    typewriterGrid->addWidget(typewriterInfo, 0, 0, 1, 2);

    QLabel* typewriterFocusLabel = new QLabel(tr("Cursor Line Height:"));
    m_typewriterFocusSpinBox = new QSpinBox();
    m_typewriterFocusSpinBox->setRange(10, 90);
    m_typewriterFocusSpinBox->setSingleStep(5);
    m_typewriterFocusSpinBox->setSuffix(tr(" % from the top"));
    m_typewriterFocusSpinBox->setToolTip(tr("Where the line with the cursor stays (50% = middle)"));
    typewriterGrid->addWidget(typewriterFocusLabel, 1, 0);
    typewriterGrid->addWidget(m_typewriterFocusSpinBox, 1, 1);

    m_typewriterSmoothCheckBox = new QCheckBox(tr("Smooth scrolling"));
    m_typewriterSmoothCheckBox->setToolTip(tr("Glide to the next line instead of jumping"));
    typewriterGrid->addWidget(m_typewriterSmoothCheckBox, 2, 0, 1, 2);

    typewriterGrid->setColumnStretch(1, 1);
    layout->addWidget(typewriterGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createEditorColorsPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Info label
    QLabel* infoLabel = new QLabel(
        tr("Configure editor colors for light and dark mode.\n"
           "Editor color mode is independent from the application theme.")
    );
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Dark mode checkbox
    m_editorDarkModeCheckBox = new QCheckBox(tr("Use dark mode for editor"));
    m_editorDarkModeCheckBox->setToolTip(tr("Toggle between light and dark editor colors"));
    layout->addWidget(m_editorDarkModeCheckBox);

    // Light mode colors group
    QGroupBox* lightGroup = new QGroupBox(tr("Light Mode Colors"));
    QGridLayout* lightGrid = new QGridLayout(lightGroup);

    m_editorBackgroundLightWidget = new ColorConfigWidget(tr("Background"), lightGroup);
    m_editorBackgroundLightWidget->setColor(QColor(255, 255, 255));
    lightGrid->addWidget(m_editorBackgroundLightWidget, 0, 0, 1, 2);

    m_editorTextLightWidget = new ColorConfigWidget(tr("Text"), lightGroup);
    m_editorTextLightWidget->setColor(QColor(30, 30, 30));
    lightGrid->addWidget(m_editorTextLightWidget, 1, 0, 1, 2);

    m_editorInactiveLightWidget = new ColorConfigWidget(tr("Inactive (Focus mode)"), lightGroup);
    m_editorInactiveLightWidget->setColor(QColor(170, 170, 170));
    lightGrid->addWidget(m_editorInactiveLightWidget, 2, 0, 1, 2);

    lightGrid->setColumnStretch(1, 1);
    layout->addWidget(lightGroup);

    // Dark mode colors group
    QGroupBox* darkGroup = new QGroupBox(tr("Dark Mode Colors"));
    QGridLayout* darkGrid = new QGridLayout(darkGroup);

    m_editorBackgroundDarkWidget = new ColorConfigWidget(tr("Background"), darkGroup);
    m_editorBackgroundDarkWidget->setColor(QColor(35, 35, 40));
    darkGrid->addWidget(m_editorBackgroundDarkWidget, 0, 0, 1, 2);

    m_editorTextDarkWidget = new ColorConfigWidget(tr("Text"), darkGroup);
    m_editorTextDarkWidget->setColor(QColor(224, 224, 224));
    darkGrid->addWidget(m_editorTextDarkWidget, 1, 0, 1, 2);

    m_editorInactiveDarkWidget = new ColorConfigWidget(tr("Inactive (Focus mode)"), darkGroup);
    m_editorInactiveDarkWidget->setColor(QColor(120, 120, 125));
    darkGrid->addWidget(m_editorInactiveDarkWidget, 2, 0, 1, 2);

    darkGrid->setColumnStretch(1, 1);
    layout->addWidget(darkGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createEditorCursorPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Info label
    QLabel* infoLabel = new QLabel(
        tr("Configure the appearance of the text cursor in the editor.")
    );
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Cursor Style group
    QGroupBox* styleGroup = new QGroupBox(tr("Cursor Style"));
    QGridLayout* styleGrid = new QGridLayout(styleGroup);

    QLabel* styleLabel = new QLabel(tr("Style:"));
    styleLabel->setToolTip(tr("Select the cursor shape"));
    m_cursorStyleComboBox = new QComboBox();
    m_cursorStyleComboBox->addItem(tr("Line (|)"), static_cast<int>(editor::CursorStyle::Line));
    m_cursorStyleComboBox->addItem(tr("Block"), static_cast<int>(editor::CursorStyle::Block));
    m_cursorStyleComboBox->addItem(tr("Underline (_)"), static_cast<int>(editor::CursorStyle::Underline));
    m_cursorStyleComboBox->setToolTip(tr("Select the cursor shape:\n"
                                          "- Line: vertical bar (|)\n"
                                          "- Block: rectangle on character\n"
                                          "- Underline: line under character (_)"));
    styleGrid->addWidget(styleLabel, 0, 0);
    styleGrid->addWidget(m_cursorStyleComboBox, 0, 1);

    // Line width (only for Line style)
    m_cursorLineWidthLabel = new QLabel(tr("Cursor width (px):"));
    m_cursorLineWidthLabel->setToolTip(tr("Width of the line cursor in pixels"));
    m_cursorLineWidthSpinBox = new QSpinBox();
    m_cursorLineWidthSpinBox->setRange(1, 5);
    m_cursorLineWidthSpinBox->setValue(2);
    m_cursorLineWidthSpinBox->setToolTip(tr("Width of the line cursor (1-5 pixels)"));
    styleGrid->addWidget(m_cursorLineWidthLabel, 1, 0);
    styleGrid->addWidget(m_cursorLineWidthSpinBox, 1, 1);

    // Connect style change to show/hide line width
    connect(m_cursorStyleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        int style = m_cursorStyleComboBox->itemData(index).toInt();
        bool isLineStyle = (style == static_cast<int>(editor::CursorStyle::Line));
        m_cursorLineWidthLabel->setVisible(isLineStyle);
        m_cursorLineWidthSpinBox->setVisible(isLineStyle);
    });

    styleGrid->setColumnStretch(1, 1);
    layout->addWidget(styleGroup);

    // Cursor Color group
    QGroupBox* colorGroup = new QGroupBox(tr("Cursor Color"));
    QVBoxLayout* colorLayout = new QVBoxLayout(colorGroup);

    m_cursorUseCustomColorCheckBox = new QCheckBox(tr("Use custom color"));
    m_cursorUseCustomColorCheckBox->setToolTip(tr("When unchecked, cursor uses the text color.\n"
                                                   "When checked, cursor uses a custom color."));
    colorLayout->addWidget(m_cursorUseCustomColorCheckBox);

    m_cursorColorWidget = new ColorConfigWidget(tr("Custom Color"), colorGroup);
    m_cursorColorWidget->setColor(QColor(255, 255, 255));
    m_cursorColorWidget->setToolTip(tr("Custom cursor color (only used when 'Use custom color' is checked)"));
    m_cursorColorWidget->setEnabled(false);
    colorLayout->addWidget(m_cursorColorWidget);

    // Connect checkbox to enable/disable color widget
    connect(m_cursorUseCustomColorCheckBox, &QCheckBox::toggled,
            m_cursorColorWidget, &QWidget::setEnabled);

    layout->addWidget(colorGroup);

    // Blinking group
    QGroupBox* blinkGroup = new QGroupBox(tr("Blinking"));
    QGridLayout* blinkGrid = new QGridLayout(blinkGroup);

    m_cursorBlinkingCheckBox = new QCheckBox(tr("Enable cursor blinking"));
    m_cursorBlinkingCheckBox->setChecked(true);
    m_cursorBlinkingCheckBox->setToolTip(tr("Enable or disable cursor blinking animation"));
    blinkGrid->addWidget(m_cursorBlinkingCheckBox, 0, 0, 1, 2);

    QLabel* intervalLabel = new QLabel(tr("Blink interval (ms):"));
    intervalLabel->setToolTip(tr("Time between cursor blink states in milliseconds"));
    m_cursorBlinkIntervalSpinBox = new QSpinBox();
    m_cursorBlinkIntervalSpinBox->setRange(100, 2000);
    m_cursorBlinkIntervalSpinBox->setValue(500);
    m_cursorBlinkIntervalSpinBox->setSingleStep(50);
    m_cursorBlinkIntervalSpinBox->setToolTip(tr("Blink interval (100-2000 ms)"));
    blinkGrid->addWidget(intervalLabel, 1, 0);
    blinkGrid->addWidget(m_cursorBlinkIntervalSpinBox, 1, 1);

    // Connect blinking checkbox to enable/disable interval
    connect(m_cursorBlinkingCheckBox, &QCheckBox::toggled,
            m_cursorBlinkIntervalSpinBox, &QWidget::setEnabled);
    connect(m_cursorBlinkingCheckBox, &QCheckBox::toggled,
            intervalLabel, &QWidget::setEnabled);

    blinkGrid->setColumnStretch(1, 1);
    layout->addWidget(blinkGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createEditorMarginsPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Info label
    QLabel* infoLabel = new QLabel(
        tr("Configure the page format, the margins of the editor views and the text frame border.")
    );
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // ========================================================================
    // View Margins group (for Continuous/Focus views)
    // ========================================================================
    QGroupBox* viewMarginsGroup = new QGroupBox(tr("View Margins (Continuous/Focus)"));
    QGridLayout* viewMarginsGrid = new QGridLayout(viewMarginsGroup);

    QLabel* viewMarginHorizontalLabel = new QLabel(tr("Horizontal:"));
    viewMarginHorizontalLabel->setToolTip(tr("Left and right margin for continuous editor views"));
    m_viewMarginHorizontalSpinBox = new QSpinBox();
    m_viewMarginHorizontalSpinBox->setRange(0, 200);
    m_viewMarginHorizontalSpinBox->setValue(50);
    m_viewMarginHorizontalSpinBox->setSuffix(tr(" px"));
    m_viewMarginHorizontalSpinBox->setToolTip(tr("Horizontal margin in pixels (0-200)"));
    viewMarginsGrid->addWidget(viewMarginHorizontalLabel, 0, 0);
    viewMarginsGrid->addWidget(m_viewMarginHorizontalSpinBox, 0, 1);

    QLabel* viewMarginVerticalLabel = new QLabel(tr("Vertical:"));
    viewMarginVerticalLabel->setToolTip(tr("Top and bottom margin for continuous editor views"));
    m_viewMarginVerticalSpinBox = new QSpinBox();
    m_viewMarginVerticalSpinBox->setRange(0, 200);
    m_viewMarginVerticalSpinBox->setValue(30);
    m_viewMarginVerticalSpinBox->setSuffix(tr(" px"));
    m_viewMarginVerticalSpinBox->setToolTip(tr("Vertical margin in pixels (0-200)"));
    viewMarginsGrid->addWidget(viewMarginVerticalLabel, 1, 0);
    viewMarginsGrid->addWidget(m_viewMarginVerticalSpinBox, 1, 1);

    viewMarginsGrid->setColumnStretch(1, 1);
    layout->addWidget(viewMarginsGroup);

    // ========================================================================
    // Page group (Page Layout view)
    // ========================================================================
    QGroupBox* pageGroup = new QGroupBox(tr("Page (Page Layout)"));
    QGridLayout* pageGrid = new QGridLayout(pageGroup);

    QLabel* pageSizeLabel = new QLabel(tr("Format:"));
    m_pageSizeComboBox = new QComboBox();
    m_pageSizeComboBox->addItem(tr("A4 (210 x 297 mm)"), QStringLiteral("A4"));
    m_pageSizeComboBox->addItem(tr("A5 (148 x 210 mm)"), QStringLiteral("A5"));
    m_pageSizeComboBox->addItem(tr("B5 (176 x 250 mm)"), QStringLiteral("B5"));
    m_pageSizeComboBox->addItem(tr("6 x 9 in (152 x 229 mm)"), QStringLiteral("6x9"));
    m_pageSizeComboBox->addItem(tr("Letter (8.5 x 11 in)"), QStringLiteral("Letter"));
    m_pageSizeComboBox->addItem(tr("Legal (8.5 x 14 in)"), QStringLiteral("Legal"));
    m_pageSizeComboBox->addItem(tr("Custom"), QStringLiteral("Custom"));
    pageGrid->addWidget(pageSizeLabel, 0, 0);
    pageGrid->addWidget(m_pageSizeComboBox, 0, 1);

    QLabel* pageWidthLabel = new QLabel(tr("Width:"));
    m_pageCustomWidthSpinBox = new QDoubleSpinBox();
    m_pageCustomWidthSpinBox->setRange(50.0, 500.0);
    m_pageCustomWidthSpinBox->setDecimals(1);
    m_pageCustomWidthSpinBox->setSuffix(tr(" mm"));
    pageGrid->addWidget(pageWidthLabel, 1, 0);
    pageGrid->addWidget(m_pageCustomWidthSpinBox, 1, 1);

    QLabel* pageHeightLabel = new QLabel(tr("Height:"));
    m_pageCustomHeightSpinBox = new QDoubleSpinBox();
    m_pageCustomHeightSpinBox->setRange(50.0, 500.0);
    m_pageCustomHeightSpinBox->setDecimals(1);
    m_pageCustomHeightSpinBox->setSuffix(tr(" mm"));
    pageGrid->addWidget(pageHeightLabel, 2, 0);
    pageGrid->addWidget(m_pageCustomHeightSpinBox, 2, 1);

    // The width and height are set for a custom format only
    connect(m_pageSizeComboBox, &QComboBox::currentIndexChanged, this, [this]() {
        const bool custom = m_pageSizeComboBox->currentData().toString() == QStringLiteral("Custom");
        m_pageCustomWidthSpinBox->setEnabled(custom);
        m_pageCustomHeightSpinBox->setEnabled(custom);
    });

    QLabel* pageGapLabel = new QLabel(tr("Gap between pages:"));
    m_pageGapSpinBox = new QSpinBox();
    m_pageGapSpinBox->setRange(0, 100);
    m_pageGapSpinBox->setSuffix(tr(" px"));
    m_pageGapSpinBox->setToolTip(tr("Space between the pages and around them, at 100% zoom"));
    pageGrid->addWidget(pageGapLabel, 3, 0);
    pageGrid->addWidget(m_pageGapSpinBox, 3, 1);

    m_pageShowNumbersCheckBox = new QCheckBox(tr("Show page numbers"));
    pageGrid->addWidget(m_pageShowNumbersCheckBox, 4, 0, 1, 2);

    pageGrid->setColumnStretch(1, 1);
    layout->addWidget(pageGroup);

    // ========================================================================
    // Page Margins group (Page Layout view)
    // ========================================================================
    QGroupBox* pageMarginsGroup = new QGroupBox(tr("Page Margins (Page Layout)"));
    QGridLayout* pageMarginsGrid = new QGridLayout(pageMarginsGroup);
    int row = 0;

    // Top margin
    QLabel* pageMarginTopLabel = new QLabel(tr("Top:"));
    pageMarginTopLabel->setToolTip(tr("Top margin of the page"));
    m_pageMarginTopSpinBox = new QDoubleSpinBox();
    m_pageMarginTopSpinBox->setRange(0.0, 100.0);
    m_pageMarginTopSpinBox->setValue(25.4);
    m_pageMarginTopSpinBox->setDecimals(1);
    m_pageMarginTopSpinBox->setSuffix(tr(" mm"));
    m_pageMarginTopSpinBox->setToolTip(tr("Top margin in millimeters (0-100)"));
    pageMarginsGrid->addWidget(pageMarginTopLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginTopSpinBox, row, 1);
    row++;

    // Bottom margin
    QLabel* pageMarginBottomLabel = new QLabel(tr("Bottom:"));
    pageMarginBottomLabel->setToolTip(tr("Bottom margin of the page"));
    m_pageMarginBottomSpinBox = new QDoubleSpinBox();
    m_pageMarginBottomSpinBox->setRange(0.0, 100.0);
    m_pageMarginBottomSpinBox->setValue(25.4);
    m_pageMarginBottomSpinBox->setDecimals(1);
    m_pageMarginBottomSpinBox->setSuffix(tr(" mm"));
    m_pageMarginBottomSpinBox->setToolTip(tr("Bottom margin in millimeters (0-100)"));
    pageMarginsGrid->addWidget(pageMarginBottomLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginBottomSpinBox, row, 1);
    row++;

    // Left margin
    m_pageMarginLeftLabel = new QLabel(tr("Left:"));
    m_pageMarginLeftLabel->setToolTip(tr("Left margin of the page"));
    m_pageMarginLeftSpinBox = new QDoubleSpinBox();
    m_pageMarginLeftSpinBox->setRange(0.0, 100.0);
    m_pageMarginLeftSpinBox->setValue(25.4);
    m_pageMarginLeftSpinBox->setDecimals(1);
    m_pageMarginLeftSpinBox->setSuffix(tr(" mm"));
    m_pageMarginLeftSpinBox->setToolTip(tr("Left margin in millimeters (0-100)"));
    pageMarginsGrid->addWidget(m_pageMarginLeftLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginLeftSpinBox, row, 1);
    row++;

    // Right margin
    m_pageMarginRightLabel = new QLabel(tr("Right:"));
    m_pageMarginRightLabel->setToolTip(tr("Right margin of the page"));
    m_pageMarginRightSpinBox = new QDoubleSpinBox();
    m_pageMarginRightSpinBox->setRange(0.0, 100.0);
    m_pageMarginRightSpinBox->setValue(25.4);
    m_pageMarginRightSpinBox->setDecimals(1);
    m_pageMarginRightSpinBox->setSuffix(tr(" mm"));
    m_pageMarginRightSpinBox->setToolTip(tr("Right margin in millimeters (0-100)"));
    pageMarginsGrid->addWidget(m_pageMarginRightLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginRightSpinBox, row, 1);
    row++;

    // Mirror margins checkbox
    m_pageMirrorMarginsCheckBox = new QCheckBox(tr("Mirror margins (for book binding)"));
    m_pageMirrorMarginsCheckBox->setToolTip(
        tr("Enable mirror margins for book binding.\n"
           "When enabled, uses inner/outer margins instead of left/right.\n"
           "Inner margin is the binding side, outer is the edge."));
    pageMarginsGrid->addWidget(m_pageMirrorMarginsCheckBox, row, 0, 1, 2);
    row++;

    // Inner margin (only visible when mirror is enabled)
    m_pageMarginInnerLabel = new QLabel(tr("Inner (binding):"));
    m_pageMarginInnerLabel->setToolTip(tr("Inner margin (binding side) for book layout"));
    m_pageMarginInnerSpinBox = new QDoubleSpinBox();
    m_pageMarginInnerSpinBox->setRange(0.0, 100.0);
    m_pageMarginInnerSpinBox->setValue(30.0);
    m_pageMarginInnerSpinBox->setDecimals(1);
    m_pageMarginInnerSpinBox->setSuffix(tr(" mm"));
    m_pageMarginInnerSpinBox->setToolTip(tr("Inner margin in millimeters (0-100)"));
    m_pageMarginInnerLabel->setVisible(false);
    m_pageMarginInnerSpinBox->setVisible(false);
    pageMarginsGrid->addWidget(m_pageMarginInnerLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginInnerSpinBox, row, 1);
    row++;

    // Outer margin (only visible when mirror is enabled)
    m_pageMarginOuterLabel = new QLabel(tr("Outer (edge):"));
    m_pageMarginOuterLabel->setToolTip(tr("Outer margin (edge side) for book layout"));
    m_pageMarginOuterSpinBox = new QDoubleSpinBox();
    m_pageMarginOuterSpinBox->setRange(0.0, 100.0);
    m_pageMarginOuterSpinBox->setValue(20.0);
    m_pageMarginOuterSpinBox->setDecimals(1);
    m_pageMarginOuterSpinBox->setSuffix(tr(" mm"));
    m_pageMarginOuterSpinBox->setToolTip(tr("Outer margin in millimeters (0-100)"));
    m_pageMarginOuterLabel->setVisible(false);
    m_pageMarginOuterSpinBox->setVisible(false);
    pageMarginsGrid->addWidget(m_pageMarginOuterLabel, row, 0);
    pageMarginsGrid->addWidget(m_pageMarginOuterSpinBox, row, 1);

    // Connect mirror checkbox to show/hide inner/outer vs left/right
    connect(m_pageMirrorMarginsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        // Show/hide left/right
        m_pageMarginLeftLabel->setVisible(!checked);
        m_pageMarginLeftSpinBox->setVisible(!checked);
        m_pageMarginRightLabel->setVisible(!checked);
        m_pageMarginRightSpinBox->setVisible(!checked);
        // Show/hide inner/outer
        m_pageMarginInnerLabel->setVisible(checked);
        m_pageMarginInnerSpinBox->setVisible(checked);
        m_pageMarginOuterLabel->setVisible(checked);
        m_pageMarginOuterSpinBox->setVisible(checked);
    });

    pageMarginsGrid->setColumnStretch(1, 1);
    layout->addWidget(pageMarginsGroup);

    // ========================================================================
    // Text Frame Border group
    // ========================================================================
    QGroupBox* textFrameGroup = new QGroupBox(tr("Text Frame Border"));
    QGridLayout* textFrameGrid = new QGridLayout(textFrameGroup);
    row = 0;

    m_textFrameBorderShowCheckBox = new QCheckBox(tr("Show text frame border"));
    m_textFrameBorderShowCheckBox->setToolTip(
        tr("Display a visible border around the text content area.\n"
           "Useful for visualizing margin boundaries."));
    textFrameGrid->addWidget(m_textFrameBorderShowCheckBox, row, 0, 1, 2);
    row++;

    m_textFrameBorderColorWidget = new ColorConfigWidget(tr("Border color"), textFrameGroup);
    m_textFrameBorderColorWidget->setColor(QColor(180, 180, 180));
    m_textFrameBorderColorWidget->setToolTip(tr("Color of the text frame border"));
    m_textFrameBorderColorWidget->setEnabled(false);
    textFrameGrid->addWidget(m_textFrameBorderColorWidget, row, 0, 1, 2);
    row++;

    QLabel* textFrameWidthLabel = new QLabel(tr("Border width:"));
    textFrameWidthLabel->setToolTip(tr("Width of the text frame border in pixels"));
    m_textFrameBorderWidthSpinBox = new QSpinBox();
    m_textFrameBorderWidthSpinBox->setRange(1, 5);
    m_textFrameBorderWidthSpinBox->setValue(1);
    m_textFrameBorderWidthSpinBox->setSuffix(tr(" px"));
    m_textFrameBorderWidthSpinBox->setToolTip(tr("Border width in pixels (1-5)"));
    m_textFrameBorderWidthSpinBox->setEnabled(false);
    textFrameGrid->addWidget(textFrameWidthLabel, row, 0);
    textFrameGrid->addWidget(m_textFrameBorderWidthSpinBox, row, 1);

    // Connect show checkbox to enable/disable color and width
    connect(m_textFrameBorderShowCheckBox, &QCheckBox::toggled, this, [this, textFrameWidthLabel](bool checked) {
        m_textFrameBorderColorWidget->setEnabled(checked);
        m_textFrameBorderWidthSpinBox->setEnabled(checked);
        textFrameWidthLabel->setEnabled(checked);
    });

    textFrameGrid->setColumnStretch(1, 1);
    layout->addWidget(textFrameGroup);

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
        .arg(advTheme.palette.placeholderText.name()));
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

    // Help text - use placeholderText for muted but readable description text
    const auto& logTheme = core::ThemeManager::getInstance().getCurrentTheme();
    QLabel* helpLabel = new QLabel(
        tr("The log panel displays application messages in real-time.\n\n"
           "Buffer size determines how many log entries are kept in memory.\n"
           "When the buffer is full, oldest entries are removed.\n\n"
           "Note: Log files are always saved to disk regardless of this setting.")
    );
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QString("color: %1; margin-top: 10px;")
        .arg(logTheme.palette.placeholderText.name()));

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
    fileInfo->setStyleSheet(QString("color: %1;").arg(logTheme.palette.placeholderText.name()));
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
        logger.debug("SettingsDialog: Settings changed, applying");
        applySettings(settings);
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
        logger.debug("SettingsDialog: Settings changed, applying");
        applySettings(settings);
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
    auto& logger = core::Logger::getInstance();

    // Defaults come from the theme file, the same values the theme uses after a restart
    core::Theme themeDefaults = core::ThemeManager::getInstance().getCurrentTheme();
    try {
        themeDefaults = core::ThemeManager::getInstance().loadTheme(theme);
    } catch (const std::exception& e) {
        logger.warn("SettingsDialog: Cannot load theme '{}' for defaults: {}", themeName, e.what());
    }
    auto hex = [](const QColor& color) { return color.name().toStdString(); };

    std::string defaultPrimary = hex(themeDefaults.colors.primary);
    std::string defaultSecondary = hex(themeDefaults.colors.secondary);
    std::string defaultInfoHeader = hex(themeDefaults.colors.infoHeader);
    std::string defaultDashboardSecondary = hex(themeDefaults.colors.dashboardSecondary);
    std::string defaultDashboardPrimary = hex(themeDefaults.colors.dashboardPrimary);
    std::string defaultInfoSecondary = hex(themeDefaults.colors.infoSecondary);
    std::string defaultInfoPrimary = hex(themeDefaults.colors.infoPrimary);
    std::string defToolTipBase = hex(themeDefaults.palette.toolTipBase);
    std::string defToolTipText = hex(themeDefaults.palette.toolTipText);
    std::string defPlaceholderText = hex(themeDefaults.palette.placeholderText);
    std::string defBrightText = hex(themeDefaults.palette.brightText);
    std::string defTrace = hex(themeDefaults.log.trace);
    std::string defDebug = hex(themeDefaults.log.debug);
    std::string defInfo = hex(themeDefaults.log.info);
    std::string defWarning = hex(themeDefaults.log.warning);
    std::string defError = hex(themeDefaults.log.error);
    std::string defCritical = hex(themeDefaults.log.critical);
    std::string defBackground = hex(themeDefaults.log.background);

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

    // Info panel and Dashboard colors: stored per theme, else the theme defaults
    auto panelColor = [&](const char* key, const std::string& fallback) {
        return QColor(QString::fromStdString(
            settings.get<std::string>("themes." + themeName + ".colors." + key, fallback)));
    };
    m_infoHeaderColorWidget->setColor(panelColor("infoHeader", defaultInfoHeader));
    m_dashboardSecondaryColorWidget->setColor(panelColor("dashboardSecondary", defaultDashboardSecondary));
    m_dashboardPrimaryColorWidget->setColor(panelColor("dashboardPrimary", defaultDashboardPrimary));
    m_infoSecondaryColorWidget->setColor(panelColor("infoSecondary", defaultInfoSecondary));
    m_infoPrimaryColorWidget->setColor(panelColor("infoPrimary", defaultInfoPrimary));

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

    // Palette color defaults from the theme file
    std::string defWindow = hex(themeDefaults.palette.window);
    std::string defWindowText = hex(themeDefaults.palette.windowText);
    std::string defBase = hex(themeDefaults.palette.base);
    std::string defAlternateBase = hex(themeDefaults.palette.alternateBase);
    std::string defText = hex(themeDefaults.palette.text);
    std::string defButton = hex(themeDefaults.palette.button);
    std::string defButtonText = hex(themeDefaults.palette.buttonText);
    std::string defHighlight = hex(themeDefaults.palette.highlight);
    std::string defHighlightedText = hex(themeDefaults.palette.highlightedText);
    std::string defLight = hex(themeDefaults.palette.light);
    std::string defMidlight = hex(themeDefaults.palette.midlight);
    std::string defMid = hex(themeDefaults.palette.mid);
    std::string defDark = hex(themeDefaults.palette.dark);
    std::string defShadow = hex(themeDefaults.palette.shadow);
    std::string defLink = hex(themeDefaults.palette.link);
    std::string defLinkVisited = hex(themeDefaults.palette.linkVisited);

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

    // What this theme shows before any edit; only colors that differ get stored
    m_themeColorBaseline = collectSettings();
}

void SettingsDialog::onIconThemeComboChanged(int index) {
    Q_UNUSED(index);
    updateIconPreview();
}

void SettingsDialog::updateIconPreview() {
    if (!m_iconPreviewLayout || !m_iconThemeComboBox) return;

    // Clear existing preview icons
    utils::clearLayout(m_iconPreviewLayout);

    QString iconTheme = m_iconThemeComboBox->currentData().toString();
    int iconSize = 24;  // Simple 24px icons - Qt handles DPI scaling automatically

    // Get colors from ColorConfigWidgets
    QColor primaryColor = m_primaryColorWidget ? m_primaryColorWidget->color()
                                               : core::ArtProvider::getInstance().getPrimaryColor();
    QColor secondaryColor = m_secondaryColorWidget ? m_secondaryColorWidget->color()
                                                   : core::ArtProvider::getInstance().getSecondaryColor();

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
    m_dashboardSecondaryColorWidget->setColor(settings.dashboardSecondaryColor);
    m_dashboardPrimaryColorWidget->setColor(settings.dashboardPrimaryColor);
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
    m_storedFontFamily = settings.editorFontFamily;
    m_shownFontFamily = m_fontFamilyComboBox->currentFont().family();
    m_editorFontSizeSpinBox->setValue(settings.editorFontSize);
    m_tabSizeSpinBox->setValue(settings.tabSize);
    m_lineNumbersCheckBox->setChecked(settings.showLineNumbers);
    m_wordWrapCheckBox->setChecked(settings.wordWrap);
    m_lineHeightSpinBox->setValue(settings.lineHeight);
    m_paragraphSpacingSpinBox->setValue(static_cast<int>(std::lround(settings.paragraphSpacing)));
    m_firstLineIndentCheckBox->setChecked(settings.firstLineIndent);
    m_indentSizeSpinBox->setValue(static_cast<int>(std::lround(settings.indentSize)));
    m_indentSizeSpinBox->setEnabled(settings.firstLineIndent);

    // Editor/Colors
    m_editorDarkModeCheckBox->setChecked(settings.editorDarkMode);
    m_editorBackgroundLightWidget->setColor(settings.editorBackgroundLight);
    m_editorTextLightWidget->setColor(settings.editorTextLight);
    m_editorInactiveLightWidget->setColor(settings.editorInactiveLight);
    m_editorBackgroundDarkWidget->setColor(settings.editorBackgroundDark);
    m_editorTextDarkWidget->setColor(settings.editorTextDark);
    m_editorInactiveDarkWidget->setColor(settings.editorInactiveDark);

    // Editor/Cursor
    int cursorStyleIndex = m_cursorStyleComboBox->findData(static_cast<int>(settings.cursorStyle));
    if (cursorStyleIndex >= 0) m_cursorStyleComboBox->setCurrentIndex(cursorStyleIndex);
    m_cursorUseCustomColorCheckBox->setChecked(settings.cursorUseCustomColor);
    m_cursorColorWidget->setColor(settings.cursorCustomColor);
    m_cursorColorWidget->setEnabled(settings.cursorUseCustomColor);
    m_cursorBlinkingCheckBox->setChecked(settings.cursorBlinking);
    m_cursorBlinkIntervalSpinBox->setValue(settings.cursorBlinkInterval);
    m_cursorBlinkIntervalSpinBox->setEnabled(settings.cursorBlinking);
    m_cursorLineWidthSpinBox->setValue(settings.cursorLineWidth);
    // Show/hide line width based on cursor style
    bool isLineStyle = (settings.cursorStyle == editor::CursorStyle::Line);
    m_cursorLineWidthLabel->setVisible(isLineStyle);
    m_cursorLineWidthSpinBox->setVisible(isLineStyle);

    // Editor/Margins
    m_viewMarginHorizontalSpinBox->setValue(settings.viewMarginHorizontal);
    m_viewMarginVerticalSpinBox->setValue(settings.viewMarginVertical);
    m_pageMarginTopSpinBox->setValue(settings.pageMarginTop);
    m_pageMarginBottomSpinBox->setValue(settings.pageMarginBottom);
    m_pageMarginLeftSpinBox->setValue(settings.pageMarginLeft);
    m_pageMarginRightSpinBox->setValue(settings.pageMarginRight);
    m_pageMirrorMarginsCheckBox->setChecked(settings.pageMirrorMarginsEnabled);
    m_pageMarginInnerSpinBox->setValue(settings.pageMarginInner);
    m_pageMarginOuterSpinBox->setValue(settings.pageMarginOuter);
    // Show/hide left/right vs inner/outer based on mirror state
    bool mirrorEnabled = settings.pageMirrorMarginsEnabled;
    m_pageMarginLeftLabel->setVisible(!mirrorEnabled);
    m_pageMarginLeftSpinBox->setVisible(!mirrorEnabled);
    m_pageMarginRightLabel->setVisible(!mirrorEnabled);
    m_pageMarginRightSpinBox->setVisible(!mirrorEnabled);
    m_pageMarginInnerLabel->setVisible(mirrorEnabled);
    m_pageMarginInnerSpinBox->setVisible(mirrorEnabled);
    m_pageMarginOuterLabel->setVisible(mirrorEnabled);
    m_pageMarginOuterSpinBox->setVisible(mirrorEnabled);

    // Page format and typewriter scrolling
    const int pageSizeIndex =
        m_pageSizeComboBox->findData(QString::fromStdString(settings.pageSize));
    m_pageSizeComboBox->setCurrentIndex(std::max(0, pageSizeIndex));
    m_pageCustomWidthSpinBox->setValue(settings.pageCustomWidth);
    m_pageCustomHeightSpinBox->setValue(settings.pageCustomHeight);
    const bool customPage = m_pageSizeComboBox->currentData().toString() == QStringLiteral("Custom");
    m_pageCustomWidthSpinBox->setEnabled(customPage);
    m_pageCustomHeightSpinBox->setEnabled(customPage);
    m_pageGapSpinBox->setValue(settings.pageGap);
    m_pageShowNumbersCheckBox->setChecked(settings.pageShowNumbers);
    m_typewriterFocusSpinBox->setValue(settings.typewriterFocusPercent);
    m_typewriterSmoothCheckBox->setChecked(settings.typewriterSmoothScroll);

    // Text Frame Border
    m_textFrameBorderShowCheckBox->setChecked(settings.textFrameBorderShow);
    m_textFrameBorderColorWidget->setColor(settings.textFrameBorderColor);
    m_textFrameBorderColorWidget->setEnabled(settings.textFrameBorderShow);
    m_textFrameBorderWidthSpinBox->setValue(settings.textFrameBorderWidth);
    m_textFrameBorderWidthSpinBox->setEnabled(settings.textFrameBorderShow);

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
    m_dashboardMaxItemsSpinBox->setValue(settings.dashboardMaxItems);
    m_dashboardIconSizeSpinBox->setValue(settings.dashboardIconSize);

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
    settingsData.dashboardSecondaryColor = m_dashboardSecondaryColorWidget->color();
    settingsData.dashboardPrimaryColor = m_dashboardPrimaryColorWidget->color();
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
    // A font missing on this system is shown as its substitute: keep the stored name
    // until the user picks another font
    const QString shownFamily = m_fontFamilyComboBox->currentFont().family();
    settingsData.editorFontFamily = shownFamily == m_shownFontFamily ? m_storedFontFamily : shownFamily;
    settingsData.editorFontSize = m_editorFontSizeSpinBox->value();
    settingsData.tabSize = m_tabSizeSpinBox->value();
    settingsData.showLineNumbers = m_lineNumbersCheckBox->isChecked();
    settingsData.wordWrap = m_wordWrapCheckBox->isChecked();
    settingsData.lineHeight = m_lineHeightSpinBox->value();
    settingsData.paragraphSpacing = m_paragraphSpacingSpinBox->value();
    settingsData.firstLineIndent = m_firstLineIndentCheckBox->isChecked();
    settingsData.indentSize = m_indentSizeSpinBox->value();

    // Editor/Colors
    settingsData.editorDarkMode = m_editorDarkModeCheckBox->isChecked();
    settingsData.editorBackgroundLight = m_editorBackgroundLightWidget->color();
    settingsData.editorTextLight = m_editorTextLightWidget->color();
    settingsData.editorInactiveLight = m_editorInactiveLightWidget->color();
    settingsData.editorBackgroundDark = m_editorBackgroundDarkWidget->color();
    settingsData.editorTextDark = m_editorTextDarkWidget->color();
    settingsData.editorInactiveDark = m_editorInactiveDarkWidget->color();

    // Editor/Cursor
    settingsData.cursorStyle = static_cast<editor::CursorStyle>(
        m_cursorStyleComboBox->currentData().toInt());
    settingsData.cursorUseCustomColor = m_cursorUseCustomColorCheckBox->isChecked();
    settingsData.cursorCustomColor = m_cursorColorWidget->color();
    settingsData.cursorBlinking = m_cursorBlinkingCheckBox->isChecked();
    settingsData.cursorBlinkInterval = m_cursorBlinkIntervalSpinBox->value();
    settingsData.cursorLineWidth = m_cursorLineWidthSpinBox->value();

    // Editor/Margins
    settingsData.viewMarginHorizontal = m_viewMarginHorizontalSpinBox->value();
    settingsData.viewMarginVertical = m_viewMarginVerticalSpinBox->value();
    settingsData.pageMarginTop = m_pageMarginTopSpinBox->value();
    settingsData.pageMarginBottom = m_pageMarginBottomSpinBox->value();
    settingsData.pageMarginLeft = m_pageMarginLeftSpinBox->value();
    settingsData.pageMarginRight = m_pageMarginRightSpinBox->value();
    settingsData.pageMirrorMarginsEnabled = m_pageMirrorMarginsCheckBox->isChecked();
    settingsData.pageMarginInner = m_pageMarginInnerSpinBox->value();
    settingsData.pageMarginOuter = m_pageMarginOuterSpinBox->value();

    // Page format and typewriter scrolling
    settingsData.pageSize = m_pageSizeComboBox->currentData().toString().toStdString();
    settingsData.pageCustomWidth = m_pageCustomWidthSpinBox->value();
    settingsData.pageCustomHeight = m_pageCustomHeightSpinBox->value();
    settingsData.pageGap = m_pageGapSpinBox->value();
    settingsData.pageShowNumbers = m_pageShowNumbersCheckBox->isChecked();
    settingsData.typewriterFocusPercent = m_typewriterFocusSpinBox->value();
    settingsData.typewriterSmoothScroll = m_typewriterSmoothCheckBox->isChecked();

    // Text Frame Border
    settingsData.textFrameBorderShow = m_textFrameBorderShowCheckBox->isChecked();
    settingsData.textFrameBorderColor = m_textFrameBorderColorWidget->color();
    settingsData.textFrameBorderWidth = m_textFrameBorderWidthSpinBox->value();

    // Advanced/General
    settingsData.diagnosticMode = m_diagModeCheckbox->isChecked();

    // Advanced/Log
    settingsData.logBufferSize = m_logBufferSizeSpinBox->value();

    // General
    settingsData.autoLoadLastProject = m_autoLoadLastProjectCheckBox->isChecked();

    // Appearance/Dashboard
    settingsData.showKalahariNews = m_showKalahariNewsCheckBox->isChecked();
    settingsData.showRecentFiles = m_showRecentFilesCheckBox->isChecked();
    settingsData.dashboardMaxItems = m_dashboardMaxItemsSpinBox->value();
    settingsData.dashboardIconSize = m_dashboardIconSizeSpinBox->value();

    logger.debug("SettingsDialog: Settings collected");
    return settingsData;
}


namespace {
/// A per-theme color: its storage key and the SettingsData field holding it
struct ThemeColorField {
    const char* key;
    QColor SettingsData::*field;
};
} // namespace

void SettingsDialog::applySettings(const SettingsData& settings) {
    auto& logger = core::Logger::getInstance();
    auto& settingsManager = core::SettingsManager::getInstance();
    auto& themeManager = core::ThemeManager::getInstance();
    auto& artProvider = core::ArtProvider::getInstance();
    const SettingsData& original = m_originalSettings;
    const SettingsData& baseline = m_themeColorBaseline;
    QElapsedTimer timer;
    timer.start();
    qint64 themeMs = 0;
    qint64 saveMs = 0;

    const bool themeChanged = settings.theme != original.theme;
    const bool colorsChanged = settings.themeColorsDiffer(original);
    const bool iconThemeChanged = settings.iconTheme != original.iconTheme;
    const bool iconSizesChanged = settings.iconSizes != original.iconSizes;
    const bool visualChange = themeChanged || colorsChanged || iconThemeChanged || iconSizesChanged;

    logger.debug("SettingsDialog: themeChanged={}, colorsChanged={}, iconThemeChanged={}, iconSizesChanged={}",
                 themeChanged, colorsChanged, iconThemeChanged, iconSizesChanged);

    // Only theme and icon changes take noticeable time (palette, icon re-render)
    if (visualChange) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        // One icon refresh at the end instead of one per changed property
        artProvider.beginBatchUpdate();
    }

    // Write only what changed: unchanged values keep following the defaults
    bool written = false;
    auto setIfChanged = [&](const std::string& key, const auto& value, const auto& previous) {
        if (value != previous) {
            settingsManager.set(key, value);
            written = true;
        }
    };
    auto setColorIfChanged = [&](const std::string& key, const QColor& value, const QColor& previous) {
        if (value != previous) {
            settingsManager.set(key, value.name().toStdString());
            written = true;
        }
    };

    // Appearance/General
    if (settings.language != original.language) {
        settingsManager.setLanguage(settings.language.toStdString());
        written = true;
    }
    setIfChanged("appearance.uiFontSize", settings.uiFontSize, original.uiFontSize);

    // Appearance/Theme: per-theme colors are stored only when they differ from what the
    // selected theme showed before editing (its stored custom colors or its defaults)
    const std::string themeName = settings.theme.toStdString();
    auto storeThemeColor = [&](const QColor& value, const QColor& previous, const auto& store) {
        if (value != previous) {
            store(value.name().toStdString());
            written = true;
        }
    };
    storeThemeColor(settings.primaryColor, baseline.primaryColor, [&](const std::string& c) {
        settingsManager.setIconColorPrimaryForTheme(themeName, c); });
    storeThemeColor(settings.secondaryColor, baseline.secondaryColor, [&](const std::string& c) {
        settingsManager.setIconColorSecondaryForTheme(themeName, c); });

    const ThemeColorField uiColors[] = {
        {"toolTipBase", &SettingsData::tooltipBackgroundColor},
        {"toolTipText", &SettingsData::tooltipTextColor},
        {"placeholderText", &SettingsData::placeholderTextColor},
        {"brightText", &SettingsData::brightTextColor},
    };
    for (const ThemeColorField& color : uiColors) {
        storeThemeColor(settings.*color.field, baseline.*color.field, [&](const std::string& c) {
            settingsManager.setUiColorForTheme(themeName, color.key, c); });
    }

    const ThemeColorField panelColors[] = {
        {"infoHeader", &SettingsData::infoHeaderColor},
        {"infoPrimary", &SettingsData::infoPrimaryColor},
        {"infoSecondary", &SettingsData::infoSecondaryColor},
        {"dashboardPrimary", &SettingsData::dashboardPrimaryColor},
        {"dashboardSecondary", &SettingsData::dashboardSecondaryColor},
    };
    for (const ThemeColorField& color : panelColors) {
        storeThemeColor(settings.*color.field, baseline.*color.field, [&](const std::string& c) {
            settingsManager.set("themes." + themeName + ".colors." + color.key, c); });
    }
    const ThemeColorField logColors[] = {
        {"trace", &SettingsData::logTraceColor},
        {"debug", &SettingsData::logDebugColor},
        {"info", &SettingsData::logInfoColor},
        {"warning", &SettingsData::logWarningColor},
        {"error", &SettingsData::logErrorColor},
        {"critical", &SettingsData::logCriticalColor},
        {"background", &SettingsData::logBackgroundColor},
    };
    for (const ThemeColorField& color : logColors) {
        storeThemeColor(settings.*color.field, baseline.*color.field, [&](const std::string& c) {
            settingsManager.setLogColorForTheme(themeName, color.key, c); });
    }

    const ThemeColorField paletteColors[] = {
        {"window", &SettingsData::paletteWindowColor},
        {"windowText", &SettingsData::paletteWindowTextColor},
        {"base", &SettingsData::paletteBaseColor},
        {"alternateBase", &SettingsData::paletteAlternateBaseColor},
        {"text", &SettingsData::paletteTextColor},
        {"button", &SettingsData::paletteButtonColor},
        {"buttonText", &SettingsData::paletteButtonTextColor},
        {"highlight", &SettingsData::paletteHighlightColor},
        {"highlightedText", &SettingsData::paletteHighlightedTextColor},
        {"light", &SettingsData::paletteLightColor},
        {"midlight", &SettingsData::paletteMidlightColor},
        {"mid", &SettingsData::paletteMidColor},
        {"dark", &SettingsData::paletteDarkColor},
        {"shadow", &SettingsData::paletteShadowColor},
        {"link", &SettingsData::paletteLinkColor},
        {"linkVisited", &SettingsData::paletteLinkVisitedColor},
    };
    for (const ThemeColorField& color : paletteColors) {
        storeThemeColor(settings.*color.field, baseline.*color.field, [&](const std::string& c) {
            settingsManager.setPaletteColorForTheme(themeName, color.key, c); });
    }

    // Theme switch and color changes in one pass (palette and icons once),
    // from the stored colors written above: the same result as after a restart
    if (themeChanged || colorsChanged) {
        const qint64 themeStart = timer.elapsed();
        if (themeManager.reloadTheme(settings.theme) && themeChanged) {
            written = true;
        }
        themeMs = timer.elapsed() - themeStart;
    }

    // Appearance/Icons
    if (iconThemeChanged) {
        settingsManager.set("appearance.iconTheme", settings.iconTheme.toStdString());
        artProvider.setIconTheme(settings.iconTheme);
        written = true;
    }
    if (iconSizesChanged) {
        for (auto it = settings.iconSizes.constBegin(); it != settings.iconSizes.constEnd(); ++it) {
            artProvider.setIconSize(it.key(), it.value());
        }
        written = true;
    }

    // Editor/General
    setIfChanged("editor.fontFamily", settings.editorFontFamily.toStdString(), original.editorFontFamily.toStdString());
    setIfChanged("editor.fontSize", settings.editorFontSize, original.editorFontSize);
    setIfChanged("editor.tabSize", settings.tabSize, original.tabSize);
    setIfChanged("editor.lineNumbers", settings.showLineNumbers, original.showLineNumbers);
    setIfChanged("editor.wordWrap", settings.wordWrap, original.wordWrap);
    setIfChanged("editor.lineHeight", settings.lineHeight, original.lineHeight);
    setIfChanged("editor.paragraphSpacing", settings.paragraphSpacing, original.paragraphSpacing);
    setIfChanged("editor.firstLineIndent", settings.firstLineIndent, original.firstLineIndent);
    setIfChanged("editor.indentSize", settings.indentSize, original.indentSize);

    // Editor/Colors
    setIfChanged("editor.darkMode", settings.editorDarkMode, original.editorDarkMode);
    setColorIfChanged("editor.colors.backgroundLight", settings.editorBackgroundLight, original.editorBackgroundLight);
    setColorIfChanged("editor.colors.textLight", settings.editorTextLight, original.editorTextLight);
    setColorIfChanged("editor.colors.inactiveLight", settings.editorInactiveLight, original.editorInactiveLight);
    setColorIfChanged("editor.colors.backgroundDark", settings.editorBackgroundDark, original.editorBackgroundDark);
    setColorIfChanged("editor.colors.textDark", settings.editorTextDark, original.editorTextDark);
    setColorIfChanged("editor.colors.inactiveDark", settings.editorInactiveDark, original.editorInactiveDark);

    // Editor/Cursor
    setIfChanged("editor.cursor.style", static_cast<int>(settings.cursorStyle), static_cast<int>(original.cursorStyle));
    setIfChanged("editor.cursor.useCustomColor", settings.cursorUseCustomColor, original.cursorUseCustomColor);
    setColorIfChanged("editor.cursor.customColor", settings.cursorCustomColor, original.cursorCustomColor);
    setIfChanged("editor.cursor.blinking", settings.cursorBlinking, original.cursorBlinking);
    setIfChanged("editor.cursor.blinkInterval", settings.cursorBlinkInterval, original.cursorBlinkInterval);
    setIfChanged("editor.cursor.lineWidth", settings.cursorLineWidth, original.cursorLineWidth);

    // Editor/Margins
    setIfChanged("editor.margins.viewHorizontal", static_cast<double>(settings.viewMarginHorizontal),
                 static_cast<double>(original.viewMarginHorizontal));
    setIfChanged("editor.margins.viewVertical", static_cast<double>(settings.viewMarginVertical),
                 static_cast<double>(original.viewMarginVertical));
    setIfChanged("editor.margins.pageTop", settings.pageMarginTop, original.pageMarginTop);
    setIfChanged("editor.margins.pageBottom", settings.pageMarginBottom, original.pageMarginBottom);
    setIfChanged("editor.margins.pageLeft", settings.pageMarginLeft, original.pageMarginLeft);
    setIfChanged("editor.margins.pageRight", settings.pageMarginRight, original.pageMarginRight);
    setIfChanged("editor.margins.mirrorEnabled", settings.pageMirrorMarginsEnabled, original.pageMirrorMarginsEnabled);
    setIfChanged("editor.margins.pageInner", settings.pageMarginInner, original.pageMarginInner);
    setIfChanged("editor.margins.pageOuter", settings.pageMarginOuter, original.pageMarginOuter);

    // Editor/Page and Typewriter
    setIfChanged("editor.page.size", settings.pageSize, original.pageSize);
    setIfChanged("editor.page.customWidth", settings.pageCustomWidth, original.pageCustomWidth);
    setIfChanged("editor.page.customHeight", settings.pageCustomHeight, original.pageCustomHeight);
    setIfChanged("editor.page.gap", settings.pageGap, original.pageGap);
    setIfChanged("editor.page.showNumbers", settings.pageShowNumbers, original.pageShowNumbers);
    setIfChanged("editor.typewriter.focusPosition", settings.typewriterFocusPercent / 100.0,
                 original.typewriterFocusPercent / 100.0);
    setIfChanged("editor.typewriter.smoothScroll", settings.typewriterSmoothScroll, original.typewriterSmoothScroll);

    // Editor/Text Frame Border
    setIfChanged("editor.textFrameBorder.show", settings.textFrameBorderShow, original.textFrameBorderShow);
    setColorIfChanged("editor.textFrameBorder.color", settings.textFrameBorderColor, original.textFrameBorderColor);
    setIfChanged("editor.textFrameBorder.width", settings.textFrameBorderWidth, original.textFrameBorderWidth);

    // Advanced/Log
    setIfChanged("log.bufferSize", settings.logBufferSize, original.logBufferSize);

    // Appearance/Dashboard
    setIfChanged("dashboard.showKalahariNews", settings.showKalahariNews, original.showKalahariNews);
    setIfChanged("dashboard.showRecentFiles", settings.showRecentFiles, original.showRecentFiles);
    setIfChanged("dashboard.maxItems", settings.dashboardMaxItems, original.dashboardMaxItems);
    setIfChanged("dashboard.iconSize", settings.dashboardIconSize, original.dashboardIconSize);
    setIfChanged("startup.autoLoadLastProject", settings.autoLoadLastProject, original.autoLoadLastProject);

    if (written) {
        const qint64 saveStart = timer.elapsed();
        settingsManager.save();
        saveMs = timer.elapsed() - saveStart;
    }

    const qint64 iconsStart = timer.elapsed();
    if (visualChange) {
        artProvider.endBatchUpdate();
        QApplication::restoreOverrideCursor();
    }
    const qint64 iconsMs = timer.elapsed() - iconsStart;
    const qint64 panelsStart = timer.elapsed();

    const SettingsData previous = m_originalSettings;
    m_originalSettings = settings;
    m_themeColorBaseline = settings;
    emit settingsApplied(settings, previous);

    // Timing for diagnosing slow Apply/OK on users' machines
    logger.info("SettingsDialog: settings applied in {} ms (theme {} ms, file {} ms, icons {} ms, "
                "panels {} ms)",
                timer.elapsed(), themeMs, saveMs, iconsMs, timer.elapsed() - panelsStart);
    // Editors and panels lay out and repaint later, in the event loop
    QTimer::singleShot(0, qApp, [timer]() {
        core::Logger::getInstance().info("SettingsDialog: window updated {} ms after Apply/OK",
                                         timer.elapsed());
    });
}

} // namespace gui
} // namespace kalahari
