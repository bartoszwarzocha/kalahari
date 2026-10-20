/// @file navigator_page.cpp
/// @brief Settings page: Navigator
///
/// The commands that the menu of a node of the Navigator has for adding: "Add..." and the
/// commands named after the kinds most added there ("Add Chapter...", "Add Part..."), or
/// "Add..." only. The Navigator reads the setting each time it opens a menu.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/settings_manager.h"

#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// The commands named after kinds, besides "Add..."
constexpr const char* NAMED = "named";

/// "Add..." only
constexpr const char* SINGLE = "single";

} // namespace

NavigatorPage::NavigatorPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* menus = addGroup(tr("Node Menus"));
    m_named = new QRadioButton(tr("\"Add...\" and the commands named after kinds"));
    m_named->setObjectName(QStringLiteral("navigatorAddNamed"));
    m_single = new QRadioButton(tr("Only \"Add...\""));
    m_single->setObjectName(QStringLiteral("navigatorAddSingle"));
    // Short texts with a note under each, which wraps in a narrow window
    auto* choices = new QWidget();
    auto* choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->addWidget(m_named);
    alignWithText(addNote(choicesLayout, tr("\"Add Chapter...\", \"Add Part...\" and the like.")),
                  m_named);
    choicesLayout->addWidget(m_single);
    alignWithText(addNote(choicesLayout, tr("The kind is chosen in the Add Element window.")),
                  m_single);
    auto* choicesLabel = new QLabel(tr("Add commands:"));
    choicesLabel->setBuddy(m_named);
    menus->addRow(choicesLabel, choices);
    addNote(menus, tr("Applies to the menu the right mouse button opens on a node of the "
                      "Navigator. The Book menu, the \"+\" button and the keys work the same in "
                      "both variants."));

    auto& settings = core::SettingsManager::getInstance();
    Binding addCommands;
    addCommands.key = []() { return std::string(NavigatorPanel::ADD_COMMANDS_SETTING); };
    addCommands.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING, NAMED));
    };
    addCommands.shown = [this]() { return json(m_single->isChecked() ? SINGLE : NAMED); };
    addCommands.show = [this](const json& value) {
        const bool single = value.is_string() && value.get<std::string>() == SINGLE;
        (single ? m_single : m_named)->setChecked(true);
    };
    addCommands.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING, value.get<std::string>());
    };
    bindCustom(std::move(addCommands));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
