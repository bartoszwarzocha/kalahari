/// @file navigator_page.cpp
/// @brief Settings page: Navigator
///
/// The commands that the menu of a node of the Navigator has for adding: "Add..." and the
/// commands named after the kinds most added there ("Add Chapter...", "Add Part..."), or
/// "Add..." only. The Navigator reads the setting each time it opens a menu.
///
/// What the top of the Add Element window chooses from: a section of the book or the
/// Workshop, the whole book or the Workshop, or nothing (the kinds of the place the window
/// was opened on only). The window reads the setting each time it opens.

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

/// The values of NavigatorPanel::ADD_WINDOW_SETTING (AddChooser)
constexpr const char* SECTIONS = "sections";
constexpr const char* BOOK = "book";
constexpr const char* PLACE = "place";

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

    // The top of the Add Element window
    QFormLayout* window = addGroup(tr("Add Element Window"));
    m_sections = new QRadioButton(tr("Sections and the Workshop"));
    m_sections->setObjectName(QStringLiteral("navigatorAddWindowSections"));
    m_book = new QRadioButton(tr("The book and the Workshop"));
    m_book->setObjectName(QStringLiteral("navigatorAddWindowBook"));
    m_place = new QRadioButton(tr("No choice"));
    m_place->setObjectName(QStringLiteral("navigatorAddWindowPlace"));
    auto* windowChoices = new QWidget();
    auto* windowLayout = new QVBoxLayout(windowChoices);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->addWidget(m_sections);
    alignWithText(addNote(windowLayout, tr("The window shows one group at a time, first the one "
                                           "of the node chosen in the Navigator.")),
                  m_sections);
    windowLayout->addWidget(m_book);
    alignWithText(addNote(windowLayout, tr("All kinds of the book at once.")), m_book);
    windowLayout->addWidget(m_place);
    alignWithText(addNote(windowLayout, tr("Only the kinds of the place the window is opened on.")),
                  m_place);
    auto* windowLabel = new QLabel(tr("Choice at the top of the window:"));
    windowLabel->setBuddy(m_sections);
    window->addRow(windowLabel, windowChoices);

    Binding addWindow;
    addWindow.key = []() { return std::string(NavigatorPanel::ADD_WINDOW_SETTING); };
    addWindow.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::ADD_WINDOW_SETTING, SECTIONS));
    };
    addWindow.shown = [this]() {
        return json(m_book->isChecked() ? BOOK : m_place->isChecked() ? PLACE : SECTIONS);
    };
    addWindow.show = [this](const json& value) {
        const std::string chosen = value.is_string() ? value.get<std::string>() : SECTIONS;
        (chosen == BOOK ? m_book : chosen == PLACE ? m_place : m_sections)->setChecked(true);
    };
    addWindow.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::ADD_WINDOW_SETTING, value.get<std::string>());
    };
    bindCustom(std::move(addWindow));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
