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
///
/// What the Materials of the Workshop show: their kinds, each with its number of materials,
/// whose lists open on tabs; or the kinds with their materials under them. A kind clicked opens
/// its list on a tab of its own, or the one tab of the lists shows it. The Navigator follows
/// the first setting at once; the second applies to the next kind clicked.
///
/// How a material opens: its picture on a preview tab of the program, or in the program the
/// system gives its file; a material with the address of a page in the browser or on its card.
/// What a file, a web address or a picture dropped on the Navigator or pasted in it gives: a
/// material at once, linked with the node or not, or the Add Element window. The Navigator
/// reads the settings each time a material opens or comes.
///
/// Whether a link of a material may name a paragraph of a chapter, marked with a paperclip in
/// the editor (core::LinkRoles::PARAGRAPHS_SETTING). The paperclips follow the setting at once.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/settings_manager.h"

#include <QCheckBox>
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

/// The values of NavigatorPanel::MATERIALS_TREE_SETTING
constexpr const char* TYPES = "types";
constexpr const char* EXPAND = "expand";

/// The values of MaterialList::TABS_SETTING
constexpr const char* OWN = "own";
constexpr const char* ONE = "one";

/// The values of NavigatorPanel::OPEN_MATERIALS_SETTING
constexpr const char* CARD = "card";
constexpr const char* SYSTEM = "system";

/// The values of NavigatorPanel::OPEN_PAGES_SETTING ("card" as above)
constexpr const char* BROWSER = "browser";

/// The values of NavigatorPanel::QUICK_MATERIALS_SETTING
constexpr const char* LINK = "link";
constexpr const char* ADD = "add";
constexpr const char* WINDOW = "window";

} // namespace

NavigatorPage::NavigatorPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* menus = addGroup(tr("Node Menus"));
    m_named = new QRadioButton(tr("\"Add...\" and a command for each kind"));
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

    // How a material opens
    //: The group of the materials of the Workshop. In Polish: Materiały
    QFormLayout* materials = addGroup(tr("Materials"));

    // What the Materials show: their kinds, whose lists open on tabs, or their materials too
    //: In Polish: Typy z liczbą materiałów
    m_treeTypes = new QRadioButton(tr("Kinds with their numbers of materials"));
    m_treeTypes->setObjectName(QStringLiteral("navigatorMaterialsTypes"));
    //: In Polish: Typy z listą do rozwinięcia
    m_treeExpand = new QRadioButton(tr("Kinds with a list to expand"));
    m_treeExpand->setObjectName(QStringLiteral("navigatorMaterialsExpand"));
    auto* treeChoices = new QWidget();
    auto* treeLayout = new QVBoxLayout(treeChoices);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    treeLayout->addWidget(m_treeTypes);
    //: In Polish: Lista typu otwiera się na karcie na środku okna: tabela z wyszukiwaniem,
    //: filtrami i sortowaniem.
    alignWithText(addNote(treeLayout, tr("The list of a kind opens on a tab in the middle of the "
                                         "window: a table with a search, filters and sorting.")),
                  m_treeTypes);
    treeLayout->addWidget(m_treeExpand);
    //: In Polish: Pod każdym typem są także jego materiały, na początku zwinięte; lista typu
    //: otwiera się na karcie jak wyżej.
    alignWithText(addNote(treeLayout, tr("Each kind also has its materials under it, collapsed "
                                         "at first; its list opens on a tab as above.")),
                  m_treeExpand);
    //: In Polish: W Nawigatorze:
    auto* treeLabel = new QLabel(tr("In the Navigator:"));
    treeLabel->setBuddy(m_treeTypes);
    materials->addRow(treeLabel, treeChoices);

    Binding tree;
    tree.key = []() { return std::string(NavigatorPanel::MATERIALS_TREE_SETTING); };
    tree.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::MATERIALS_TREE_SETTING, TYPES));
    };
    tree.shown = [this]() { return json(m_treeExpand->isChecked() ? EXPAND : TYPES); };
    tree.show = [this](const json& value) {
        const bool expand = value.is_string() && value.get<std::string>() == EXPAND;
        (expand ? m_treeExpand : m_treeTypes)->setChecked(true);
    };
    tree.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::MATERIALS_TREE_SETTING,
                                  value.get<std::string>());
    };
    bindCustom(std::move(tree));

    // Where the list of a kind clicked opens
    //: In Polish: Otwiera własną kartę
    m_tabsOwn = new QRadioButton(tr("Opens a tab of its own"));
    m_tabsOwn->setObjectName(QStringLiteral("navigatorMaterialsOwnTabs"));
    //: In Polish: Zmienia typ na jednej karcie
    m_tabsOne = new QRadioButton(tr("Changes the kind on one tab"));
    m_tabsOne->setObjectName(QStringLiteral("navigatorMaterialsOneTab"));
    auto* tabsChoices = new QWidget();
    auto* tabsLayout = new QVBoxLayout(tabsChoices);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->addWidget(m_tabsOwn);
    //: In Polish: Każdy typ ma swoją kartę, która pamięta zaznaczenie, wyszukiwanie i filtry.
    //: Ponowne kliknięcie przenosi ją na wierzch.
    alignWithText(addNote(tabsLayout, tr("Each kind has its tab, which keeps what is chosen, "
                                         "the search and the filters. Clicked again, it brings "
                                         "its tab to the front.")),
                  m_tabsOwn);
    tabsLayout->addWidget(m_tabsOne);
    //: In Polish: Jedna karta list pokazuje typ kliknięty ostatnio i pamięta zaznaczenie,
    //: wyszukiwanie i filtry każdego typu.
    alignWithText(addNote(tabsLayout, tr("One tab of the lists shows the kind clicked last, and "
                                         "keeps what is chosen, the search and the filters of "
                                         "each kind.")),
                  m_tabsOne);
    //: In Polish: Typ kliknięty w Nawigatorze:
    auto* tabsLabel = new QLabel(tr("A kind clicked in the Navigator:"));
    tabsLabel->setBuddy(m_tabsOwn);
    materials->addRow(tabsLabel, tabsChoices);

    Binding tabs;
    tabs.key = []() { return std::string(MaterialList::TABS_SETTING); };
    tabs.stored = [&settings]() {
        return json(settings.get<std::string>(MaterialList::TABS_SETTING, OWN));
    };
    tabs.shown = [this]() { return json(m_tabsOne->isChecked() ? ONE : OWN); };
    tabs.show = [this](const json& value) {
        const bool one = value.is_string() && value.get<std::string>() == ONE;
        (one ? m_tabsOne : m_tabsOwn)->setChecked(true);
    };
    tabs.store = [&settings](const json& value) {
        settings.set<std::string>(MaterialList::TABS_SETTING, value.get<std::string>());
    };
    bindCustom(std::move(tabs));
    //: In Polish: Na karcie w programie
    m_openCard = new QRadioButton(tr("On a tab of the program"));
    m_openCard->setObjectName(QStringLiteral("navigatorOpenMaterialsCard"));
    //: In Polish: W programie systemowym
    m_openSystem = new QRadioButton(tr("In the system program"));
    m_openSystem->setObjectName(QStringLiteral("navigatorOpenMaterialsSystem"));
    auto* openChoices = new QWidget();
    auto* openLayout = new QVBoxLayout(openChoices);
    openLayout->setContentsMargins(0, 0, 0, 0);
    openLayout->addWidget(m_openCard);
    //: In Polish: Obraz, także kopia obrazu z sieci, otwiera się na karcie podglądu, jak
    //: rozdział. Plik innego rodzaju, np. PDF, otwiera się w programie systemowym, dopóki
    //: program nie będzie miał jego podglądu. Materiał bez pliku i adresu otwiera się na swojej
    //: karcie.
    alignWithText(addNote(openLayout, tr("A picture, also the copy of one from the web, opens "
                                         "on a preview tab, as a chapter does. A file of "
                                         "another kind, such as a PDF, opens in the system "
                                         "program until the program has a preview of it. A "
                                         "material without a file and an address opens on its "
                                         "card.")),
                  m_openCard);
    openLayout->addWidget(m_openSystem);
    //: In Polish: Także obrazy. Kartę podglądu otwiera wtedy przycisk „Podgląd” we
    //: Właściwościach.
    alignWithText(addNote(openLayout, tr("Pictures too. The Preview button in the Properties "
                                         "panel then opens the preview tab.")),
                  m_openSystem);
    //: In Polish: Otwieranie materiałów:
    auto* openLabel = new QLabel(tr("Opening materials:"));
    openLabel->setBuddy(m_openCard);
    materials->addRow(openLabel, openChoices);

    Binding openMaterials;
    openMaterials.key = []() { return std::string(NavigatorPanel::OPEN_MATERIALS_SETTING); };
    openMaterials.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, CARD));
    };
    openMaterials.shown = [this]() { return json(m_openSystem->isChecked() ? SYSTEM : CARD); };
    openMaterials.show = [this](const json& value) {
        const bool system = value.is_string() && value.get<std::string>() == SYSTEM;
        (system ? m_openSystem : m_openCard)->setChecked(true);
    };
    openMaterials.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING,
                                  value.get<std::string>());
    };
    bindCustom(std::move(openMaterials));

    // A page, and a picture on the web without a copy: the program shows no web pages
    //: In Polish: W przeglądarce
    m_pagesBrowser = new QRadioButton(tr("In the browser"));
    m_pagesBrowser->setObjectName(QStringLiteral("navigatorOpenPagesBrowser"));
    //: In Polish: Na karcie materiału
    m_pagesCard = new QRadioButton(tr("On the card of the material"));
    m_pagesCard->setObjectName(QStringLiteral("navigatorOpenPagesCard"));
    auto* pagesChoices = new QWidget();
    auto* pagesLayout = new QVBoxLayout(pagesChoices);
    pagesLayout->setContentsMargins(0, 0, 0, 0);
    pagesLayout->addWidget(m_pagesBrowser);
    //: In Polish: Dwuklik i Enter otwierają adres w przeglądarce systemowej. Kartę materiału
    //: na środku okna otwiera polecenie „Otwórz kartę” w menu węzła.
    alignWithText(addNote(pagesLayout, tr("Double-click and Enter open the address in the "
                                          "system's browser. Open Card in the menu of the node "
                                          "opens the card of the material in the middle of the "
                                          "window.")),
                  m_pagesBrowser);
    pagesLayout->addWidget(m_pagesCard);
    //: In Polish: Dwuklik i Enter otwierają kartę materiału na środku okna, a adres – przycisk
    //: „Otwórz w przeglądarce” na tej karcie.
    alignWithText(addNote(pagesLayout, tr("Double-click and Enter open the card of the material "
                                          "in the middle of the window; its Open in Browser "
                                          "button opens the address.")),
                  m_pagesCard);
    //: In Polish: Strony i obrazy z sieci:
    auto* pagesLabel = new QLabel(tr("Pages and pictures on the web:"));
    pagesLabel->setBuddy(m_pagesBrowser);
    materials->addRow(pagesLabel, pagesChoices);

    Binding openPages;
    openPages.key = []() { return std::string(NavigatorPanel::OPEN_PAGES_SETTING); };
    openPages.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::OPEN_PAGES_SETTING, BROWSER));
    };
    openPages.shown = [this]() { return json(m_pagesCard->isChecked() ? CARD : BROWSER); };
    openPages.show = [this](const json& value) {
        const bool card = value.is_string() && value.get<std::string>() == CARD;
        (card ? m_pagesCard : m_pagesBrowser)->setChecked(true);
    };
    openPages.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::OPEN_PAGES_SETTING, value.get<std::string>());
    };
    bindCustom(std::move(openPages));

    // Files, web addresses and pictures dropped on the Navigator or pasted in it
    //: In Polish: Od razu, z powiązaniem
    m_quickLink = new QRadioButton(tr("At once, linked"));
    m_quickLink->setObjectName(QStringLiteral("navigatorQuickMaterialsLink"));
    //: In Polish: Od razu
    m_quickAdd = new QRadioButton(tr("At once"));
    m_quickAdd->setObjectName(QStringLiteral("navigatorQuickMaterialsAdd"));
    //: In Polish: W oknie „Dodaj element”
    m_quickWindow = new QRadioButton(tr("In the Add Element window"));
    m_quickWindow->setObjectName(QStringLiteral("navigatorQuickMaterialsWindow"));
    auto* quickChoices = new QWidget();
    auto* quickLayout = new QVBoxLayout(quickChoices);
    quickLayout->setContentsMargins(0, 0, 0, 0);
    quickLayout->addWidget(m_quickLink);
    //: In Polish: Materiał dochodzi bez okna. Upuszczony albo wklejony na rozdział, część albo
    //: sekcję od razu się z nimi wiąże.
    alignWithText(addNote(quickLayout, tr("The material is added without a window. Dropped or "
                                          "pasted on a chapter, a part or a section, it is "
                                          "linked with it at once.")),
                  m_quickLink);
    quickLayout->addWidget(m_quickAdd);
    //: In Polish: Materiał dochodzi bez okna, zawsze bez powiązań.
    alignWithText(addNote(quickLayout, tr("The material is added without a window, always "
                                          "without links.")),
                  m_quickAdd);
    quickLayout->addWidget(m_quickWindow);
    //: In Polish: Okno otwiera się wypełnione plikiem, adresem albo obrazem; materiał dodaje
    //: dopiero „Dodaj”.
    alignWithText(addNote(quickLayout, tr("The window opens filled with the file, the address "
                                          "or the picture; Add adds the material.")),
                  m_quickWindow);
    //: In Polish: Upuszczone albo wklejone:
    auto* quickLabel = new QLabel(tr("Dropped or pasted:"));
    quickLabel->setBuddy(m_quickLink);
    materials->addRow(quickLabel, quickChoices);
    //: In Polish: Dotyczy pliku przeciągniętego z Eksploratora, adresu przeciągniętego z
    //: przeglądarki oraz adresu, pliku albo obrazu wklejonego w Nawigatorze (Ctrl+V).
    addNote(materials, tr("Applies to a file dragged from the file manager, an address dragged "
                          "from the browser, and an address, a file or a picture pasted in the "
                          "Navigator (Ctrl+V)."));

    Binding quick;
    quick.key = []() { return std::string(NavigatorPanel::QUICK_MATERIALS_SETTING); };
    quick.stored = [&settings]() {
        return json(settings.get<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING, LINK));
    };
    quick.shown = [this]() {
        return json(m_quickAdd->isChecked() ? ADD : m_quickWindow->isChecked() ? WINDOW : LINK);
    };
    quick.show = [this](const json& value) {
        const std::string chosen = value.is_string() ? value.get<std::string>() : LINK;
        (chosen == ADD ? m_quickAdd : chosen == WINDOW ? m_quickWindow : m_quickLink)
            ->setChecked(true);
    };
    quick.store = [&settings](const json& value) {
        settings.set<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING,
                                  value.get<std::string>());
    };
    bindCustom(std::move(quick));

    // Paragraphs of the links, with the paperclips beside them in the editor
    //: In Polish: Powiązania wskazują akapity tekstu
    QCheckBox* paragraphs = addCheckBox(materials, tr("Links name paragraphs of the text"),
                                        core::LinkRoles::PARAGRAPHS_SETTING);
    paragraphs->setObjectName(QStringLiteral("navigatorLinkParagraphs"));
    //: In Polish: W oknie „Powiąż” można wskazać akapit rozdziału, którego dotyczy materiał, a
    //: edytor oznacza taki akapit spinaczem na marginesie. Po wyłączeniu akapity i spinacze
    //: znikają z widoku, ale wskazania zostają zapisane.
    addNote(materials, tr("The Link window can name the paragraph of a chapter that a material "
                          "is about, and the editor marks that paragraph with a paperclip in "
                          "the margin. Turned off, the paragraphs and the paperclips are hidden; "
                          "the paragraphs named are kept."));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
