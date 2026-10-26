/// @file general_pages.cpp
/// @brief Settings pages: General, Appearance > General and Dashboard, Advanced

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

using json = nlohmann::json;

// ============================================================================
// General
// ============================================================================

GeneralPage::GeneralPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* startup = addGroup(tr("Startup"));
    QCheckBox* autoLoad = addCheckBox(startup, tr("Open last book on startup"),
                                      "startup.autoLoadLastProject");
    autoLoad->setToolTip(tr("Automatically open the most recently used book when Kalahari starts"));

    QFormLayout* units = addGroup(tr("Units"));
    auto* lengthUnit = new QComboBox();
    lengthUnit->addItem(tr("Millimeters"), QStringLiteral("mm"));
    lengthUnit->addItem(tr("Centimeters"), QStringLiteral("cm"));
    lengthUnit->addItem(tr("Inches"), QStringLiteral("in"));
    lengthUnit->addItem(tr("Points"), QStringLiteral("pt"));
    lengthUnit->addItem(tr("Pixels"), QStringLiteral("px"));
    lengthUnit->setToolTip(tr("Unit of the margins, page sizes, spacing and other lengths "
                              "(icon and font sizes keep their own units)"));
    addField(units, tr("Length unit:"), lengthUnit, "ui.lengthUnit");
    connect(lengthUnit, &QComboBox::currentIndexChanged, this, [this, lengthUnit]() {
        emit lengthUnitChanged(lengthUnit->currentData().toString());
    });

    pageLayout()->addStretch();
}

// ============================================================================
// Appearance > General
// ============================================================================

AppearanceGeneralPage::AppearanceGeneralPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* form = addGroup(tr("General Appearance"));

    auto* language = new QComboBox();
    // Language names in their own language, so anyone can find theirs
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("Polski"), QStringLiteral("pl"));
    addField(form, tr("Language:"), language, "ui.language");

    auto* uiFontSize = new QSpinBox();
    uiFontSize->setRange(8, 24);
    uiFontSize->setSuffix(tr(" pt"));
    QLabel* uiFontSizeLabel = addField(form, tr("UI Font Size:"), uiFontSize, "appearance.uiFontSize");
    markNotUsedYet(uiFontSize, uiFontSizeLabel);

    QFormLayout* panels = addGroup(tr("Panels"));
    auto* paragraphPlace = new QComboBox();
    paragraphPlace->addItem(tr("In a panel of its own"),
                            QString::fromLatin1(DockCoordinator::PARAGRAPH_IN_PANEL));
    paragraphPlace->addItem(tr("At the bottom of the Properties panel"),
                            QString::fromLatin1(DockCoordinator::PARAGRAPH_IN_PROPERTIES));
    paragraphPlace->setToolTip(tr("Where the paragraph at the cursor and the styles of the book "
                                  "show: a tab of its own next to Properties, or the lower part "
                                  "of the Properties panel, which folds"));
    addField(panels, tr("Paragraph and styles:"), paragraphPlace, DockCoordinator::PARAGRAPH_PLACE_KEY);

    // The tabs of panels docked one over another (PanelTabs)
    auto* tabLook = new QComboBox();
    tabLook->addItem(tr("Icons only"), QString::fromLatin1(PanelTabs::LOOK_ICONS));
    tabLook->addItem(tr("Icons and names"), QString::fromLatin1(PanelTabs::LOOK_ICONS_AND_NAMES));
    tabLook->addItem(tr("Names only"), QString::fromLatin1(PanelTabs::LOOK_NAMES));
    tabLook->setToolTip(tr("What the tabs of the panels show"));
    addField(panels, tr("Panel tabs:"), tabLook, PanelTabs::LOOK_KEY);

    auto* tabRows = new QComboBox();
    tabRows->addItem(tr("Wrap to the next row"), QString::fromLatin1(PanelTabs::ROWS_WRAP));
    tabRows->addItem(tr("Keep one row"), QString::fromLatin1(PanelTabs::ROWS_SINGLE));
    tabRows->setToolTip(tr("Tabs that do not fit the width of the panel: in more rows, or in "
                           "one row with shortened names"));
    addField(panels, tr("When tabs do not fit:"), tabRows, PanelTabs::ROWS_KEY);

    auto* tabSide = new QComboBox();
    tabSide->addItem(tr("Above the panels"), QString::fromLatin1(PanelTabs::SIDE_ABOVE));
    tabSide->addItem(tr("Below the panels"), QString::fromLatin1(PanelTabs::SIDE_BELOW));
    tabSide->setToolTip(tr("Where the tabs are: above the panel in front, or under it"));
    addField(panels, tr("Tab position:"), tabSide, PanelTabs::SIDE_KEY);

    addNote(panels, tr("With icons only, the name of a panel is in its tooltip. In one row, "
                       "names that do not fit are shortened."));

    pageLayout()->addStretch();
}

// ============================================================================
// Appearance > Dashboard
// ============================================================================

DashboardPage::DashboardPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* form = addGroup(tr("Dashboard Content"));

    addCheckBox(form, tr("Show Kalahari News"), "dashboard.showKalahariNews")
        ->setToolTip(tr("Display news and updates section on Dashboard"));
    addCheckBox(form, tr("Show Recent Books"), "dashboard.showRecentFiles")
        ->setToolTip(tr("Display recently opened books on Dashboard"));

    auto* maxItems = new QSpinBox();
    maxItems->setRange(3, 9);
    maxItems->setToolTip(tr("Number of items to show in News and Recent Books sections (3-9)"));
    addField(form, tr("Maximum items per section:"), maxItems, "dashboard.maxItems");

    auto* iconSize = new QSpinBox();
    iconSize->setRange(24, 64);
    iconSize->setSuffix(tr(" px"));
    iconSize->setToolTip(tr("Size of icons in Dashboard panels (24-64 pixels)"));
    addField(form, tr("Icon size:"), iconSize, "dashboard.iconSize");

    pageLayout()->addStretch();
}

// ============================================================================
// Advanced > General
// ============================================================================

AdvancedGeneralPage::AdvancedGeneralPage(bool diagnosticMode, QWidget* parent)
    : SettingsPage(parent)
{
    // Warning box in the theme's tooltip colors: readable in light and dark themes
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    auto* warning = new QLabel(
        tr("Warning: These settings are for advanced users and developers.\n"
           "Incorrect configuration may affect application stability."));
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral("QLabel { color: %1; background-color: %2; font-weight: bold; "
                                          "padding: 10px; border: 1px solid %3; }")
        .arg(theme.palette.toolTipText.name(), theme.palette.toolTipBase.name(),
             theme.palette.mid.name()));
    pageLayout()->addWidget(warning);

    QFormLayout* form = addGroup(tr("Diagnostic Tools"));
    auto* diagnosticMenu = new QCheckBox(tr("Enable Diagnostic Menu"));
    diagnosticMenu->setToolTip(tr("Shows additional menu with debugging tools"));
    form->addRow(diagnosticMenu);
    addNote(form, tr("When enabled, a 'Diagnostic' menu appears in the menu bar with:\n"
                     "- System information\n"
                     "- Log viewer\n"
                     "- Component status\n\n"
                     "The menu stays for this session only."));

    // This session only: nothing is stored
    Binding binding;
    binding.key = []() { return std::string(); };
    binding.stored = [diagnosticMode]() { return json(diagnosticMode); };
    binding.shown = [diagnosticMenu]() { return json(diagnosticMenu->isChecked()); };
    binding.show = [diagnosticMenu](const json& value) {
        const QSignalBlocker blocker(diagnosticMenu);
        diagnosticMenu->setChecked(value.get<bool>());
    };
    binding.store = [this](const json& value) { emit diagnosticModeChanged(value.get<bool>()); };
    bindCustom(std::move(binding));

    connect(diagnosticMenu, &QCheckBox::toggled, this, [this, diagnosticMenu](bool checked) {
        if (!checked) {
            return;
        }
        const bool enable = dialogs::MessageDialog::confirm(this, tr("Enable Diagnostic Menu"),
            tr("Do you want to enable the diagnostic menu?\n\n"
               "This exposes advanced debugging tools."),
            tr("&Enable"), dialogs::MessageDialog::Kind::Warning,
            dialogs::MessageDialog::DefaultButton::Cancel);
        if (!enable) {
            diagnosticMenu->setChecked(false);
        }
    });

    pageLayout()->addStretch();
}

// ============================================================================
// Advanced > Log
// ============================================================================

AdvancedLogPage::AdvancedLogPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* form = addGroup(tr("Log Panel Settings"));

    auto* bufferSize = new QSpinBox();
    bufferSize->setRange(1, 1000);
    bufferSize->setSuffix(tr(" lines"));
    bufferSize->setToolTip(tr("Higher values use more memory but keep more history"));
    addField(form, tr("Buffer Size (lines):"), bufferSize, "log.bufferSize")
        ->setToolTip(tr("Maximum number of log entries to keep in memory"));

    addCheckBox(form, tr("Show the Log button on the Panels toolbar"),
                ToolbarManager::LOG_BUTTON_KEY)
        ->setToolTip(tr("With the button hidden, View > Panels > Log (F4) opens the log "
                        "panel"));

    addNote(form, tr("The log panel displays application messages in real-time.\n\n"
                     "Buffer size determines how many log entries are kept in memory.\n"
                     "When the buffer is full, oldest entries are removed.\n\n"
                     "Note: Log files are always saved to disk regardless of this setting."));

    QFormLayout* file = addGroup(tr("Log File"));
    addNote(file, tr("Log files are stored in the application directory:\n"
                     "• kalahari.log - Current session log\n\n"
                     "Use the log panel toolbar buttons to:\n"
                     "• Open log folder in file explorer\n"
                     "• Copy log contents to clipboard\n"
                     "• Clear the log panel display"));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
