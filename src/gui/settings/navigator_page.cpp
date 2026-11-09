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
///
/// The kinds of materials the Navigator shows in every book (MaterialList::HIDDEN_SETTING),
/// with the numbers of materials of the open book; the materials of a kind hidden stay in the
/// book. How the texts of the cover are counted, in characters with spaces or in words, and
/// the target lengths of a description, a tagline and an opinion in each unit
/// (core::CoverMaterial). The Navigator, the lists and the cards follow them at once.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/settings_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include <algorithm>

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

/// The values of core::CoverMaterial::UNIT_SETTING
constexpr const char* CHARACTERS = "characters";
constexpr const char* WORDS = "words";

/// The longest target of a text of the cover, in characters or in words
constexpr int MAXIMUM_TARGET = 100000;

/// The values of NavigatorPanel::QUICK_MATERIALS_SETTING
constexpr const char* LINK = "link";
constexpr const char* ADD = "add";
constexpr const char* WINDOW = "window";

} // namespace

NavigatorPage::NavigatorPage(Part part, QWidget* parent)
    : SettingsPage(parent)
{
    switch (part) {
    case Part::Adding:
        createAdding();
        break;
    case Part::Materials:
        createMaterials();
        break;
    case Part::Kinds:
        createKinds();
        break;
    case Part::Cover:
        createCover();
        break;
    }
    pageLayout()->addStretch();
}

void NavigatorPage::createAdding()
{
    QFormLayout* menus = addGroup(tr("Node Menus"));
    addChoice(menus, tr("Add commands:"),
              {{tr("\"Add...\" and a command for each kind"), QLatin1String(NAMED),
                tr("\"Add Chapter...\", \"Add Part...\" and the like.")},
               {tr("Only \"Add...\""), QLatin1String(SINGLE),
                tr("The kind is chosen in the Add Element window.")}},
              NavigatorPanel::ADD_COMMANDS_SETTING)
        ->setObjectName(QStringLiteral("navigatorAddCommands"));
    addNote(menus, tr("Applies to the menu the right mouse button opens on a node of the "
                      "Navigator. The Book menu, the \"+\" button and the keys work the same in "
                      "both variants."));

    // The top of the Add Element window
    QFormLayout* window = addGroup(tr("Add Element Window"));
    QComboBox* addWindow = addChoice(
        window, tr("Choice at the top of the window:"),
        {{tr("Sections and the Workshop"), QLatin1String(SECTIONS),
          tr("The window shows one group at a time, first the one of the node chosen in the "
             "Navigator.")},
         {tr("The book and the Workshop"), QLatin1String(BOOK),
          tr("All kinds of the book at once.")},
         {tr("No choice"), QLatin1String(PLACE),
          tr("Only the kinds of the place the window is opened on.")}},
        std::string());
    addWindow->setObjectName(QStringLiteral("navigatorAddWindow"));
    bindChoice(addWindow, NavigatorPanel::ADD_WINDOW_SETTING, SECTIONS);

    // Files, web addresses and pictures dropped on the Navigator or pasted in it
    //: The group of Settings > Navigator > Adding. In Polish: Upuszczone i wklejone
    QFormLayout* materials = addGroup(tr("Dropped and Pasted"));
    QComboBox* quick = addChoice(
        materials,
        //: The choice of the group Dropped and Pasted. In Polish: Dodawane:
        tr("Added:"),
        //: In Polish: Od razu, z powiązaniem
        {{tr("At once, linked"), QLatin1String(LINK),
          //: In Polish: Materiał dochodzi bez okna. Upuszczony albo wklejony na rozdział, część
          //: albo sekcję od razu się z nimi wiąże.
          tr("The material is added without a window. Dropped or pasted on a chapter, a part "
             "or a section, it is linked with it at once.")},
         //: In Polish: Od razu
         {tr("At once"), QLatin1String(ADD),
          //: In Polish: Materiał dochodzi bez okna, zawsze bez powiązań.
          tr("The material is added without a window, always without links.")},
         //: In Polish: W oknie „Dodaj element”
         {tr("In the Add Element window"), QLatin1String(WINDOW),
          //: In Polish: Okno otwiera się wypełnione plikiem, adresem albo obrazem; materiał
          //: dodaje dopiero „Dodaj”.
          tr("The window opens filled with the file, the address or the picture; Add adds the "
             "material.")}},
        std::string());
    quick->setObjectName(QStringLiteral("navigatorQuickMaterials"));
    bindChoice(quick, NavigatorPanel::QUICK_MATERIALS_SETTING, LINK);
    //: In Polish: Dotyczy pliku przeciągniętego z Eksploratora, adresu przeciągniętego z
    //: przeglądarki oraz adresu, pliku albo obrazu wklejonego w Nawigatorze (Ctrl+V).
    addNote(materials, tr("Applies to a file dragged from the file manager, an address dragged "
                          "from the browser, and an address, a file or a picture pasted in the "
                          "Navigator (Ctrl+V)."));
}

void NavigatorPage::createMaterials()
{
    // What the Materials show, and where the list of a kind clicked opens
    //: The group of Settings > Navigator > Materials. In Polish: Listy materiałów
    QFormLayout* materials = addGroup(tr("Lists of Materials"));

    // What the Materials show: their kinds, whose lists open on tabs, or their materials too
    QComboBox* tree = addChoice(
        materials,
        //: In Polish: W Nawigatorze:
        tr("In the Navigator:"),
        //: In Polish: Typy z liczbą materiałów
        {{tr("Kinds with their numbers of materials"), QLatin1String(TYPES),
          //: In Polish: Lista typu otwiera się na karcie na środku okna: tabela z wyszukiwaniem,
          //: filtrami i sortowaniem.
          tr("The list of a kind opens on a tab in the middle of the window: a table with a "
             "search, filters and sorting.")},
         //: In Polish: Typy z listą do rozwinięcia
         {tr("Kinds with a list to expand"), QLatin1String(EXPAND),
          //: In Polish: Pod każdym typem są także jego materiały, na początku zwinięte; lista
          //: typu otwiera się na karcie jak wyżej.
          tr("Each kind also has its materials under it, collapsed at first; its list opens on "
             "a tab as above.")}},
        std::string());
    tree->setObjectName(QStringLiteral("navigatorMaterialsTree"));
    bindChoice(tree, NavigatorPanel::MATERIALS_TREE_SETTING, TYPES);

    // Where the list of a kind clicked opens
    QComboBox* tabs = addChoice(
        materials,
        //: In Polish: Typ kliknięty w Nawigatorze:
        tr("A kind clicked in the Navigator:"),
        //: In Polish: Otwiera własną kartę
        {{tr("Opens a tab of its own"), QLatin1String(OWN),
          //: In Polish: Każdy typ ma swoją kartę, która pamięta zaznaczenie, wyszukiwanie i
          //: filtry. Ponowne kliknięcie przenosi ją na wierzch.
          tr("Each kind has its tab, which keeps what is chosen, the search and the filters. "
             "Clicked again, it brings its tab to the front.")},
         //: In Polish: Zmienia typ na jednej karcie
         {tr("Changes the kind on one tab"), QLatin1String(ONE),
          //: In Polish: Jedna karta list pokazuje typ kliknięty ostatnio i pamięta zaznaczenie,
          //: wyszukiwanie i filtry każdego typu.
          tr("One tab of the lists shows the kind clicked last, and keeps what is chosen, the "
             "search and the filters of each kind.")}},
        std::string());
    tabs->setObjectName(QStringLiteral("navigatorMaterialsTabs"));
    bindChoice(tabs, MaterialList::TABS_SETTING, OWN);

    // How a material opens
    //: The group of Settings > Navigator > Materials. In Polish: Otwieranie materiałów
    materials = addGroup(tr("Opening Materials"));
    QComboBox* open = addChoice(
        materials,
        //: The choice of the group Opening Materials. In Polish: Pliki i obrazy:
        tr("Files and pictures:"),
        //: In Polish: Na karcie w programie
        {{tr("On a tab of the program"), QLatin1String(CARD),
          //: In Polish: Obraz, także kopia obrazu z sieci, otwiera się na karcie podglądu, jak
          //: rozdział. Plik innego rodzaju, np. PDF, otwiera się w programie systemowym, dopóki
          //: program nie będzie miał jego podglądu. Materiał bez pliku i adresu otwiera się na
          //: swojej karcie.
          tr("A picture, also the copy of one from the web, opens on a preview tab, as a "
             "chapter does. A file of another kind, such as a PDF, opens in the system program "
             "until the program has a preview of it. A material without a file and an address "
             "opens on its card.")},
         //: In Polish: W programie systemowym
         {tr("In the system program"), QLatin1String(SYSTEM),
          //: In Polish: Także obrazy. Kartę podglądu otwiera wtedy przycisk „Podgląd” we
          //: Właściwościach.
          tr("Pictures too. The Preview button in the Properties panel then opens the preview "
             "tab.")}},
        std::string());
    open->setObjectName(QStringLiteral("navigatorOpenMaterials"));
    bindChoice(open, NavigatorPanel::OPEN_MATERIALS_SETTING, CARD);

    // A page, and a picture on the web without a copy: the program shows no web pages
    QComboBox* pages = addChoice(
        materials,
        //: In Polish: Strony i obrazy z sieci:
        tr("Pages and pictures on the web:"),
        //: In Polish: W przeglądarce
        {{tr("In the browser"), QLatin1String(BROWSER),
          //: In Polish: Dwuklik i Enter otwierają adres w przeglądarce systemowej. Kartę
          //: materiału na środku okna otwiera polecenie „Otwórz kartę” w menu węzła.
          tr("Double-click and Enter open the address in the system's browser. Open Card in "
             "the menu of the node opens the card of the material in the middle of the "
             "window.")},
         //: In Polish: Na karcie materiału
         {tr("On the card of the material"), QLatin1String(CARD),
          //: In Polish: Dwuklik i Enter otwierają kartę materiału na środku okna, a adres –
          //: przycisk „Otwórz w przeglądarce” na tej karcie.
          tr("Double-click and Enter open the card of the material in the middle of the "
             "window; its Open in Browser button opens the address.")}},
        std::string());
    pages->setObjectName(QStringLiteral("navigatorOpenPages"));
    bindChoice(pages, NavigatorPanel::OPEN_PAGES_SETTING, BROWSER);

    //: The group of Settings > Navigator > Materials. In Polish: Powiązania
    materials = addGroup(tr("Links"));

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
}

void NavigatorPage::createKinds()
{
    auto& settings = core::SettingsManager::getInstance();
    // The kinds of materials the Navigator shows, in every book
    //: The group of Settings > Navigator that hides kinds of materials. In Polish: Typy
    //: materiałów w Nawigatorze
    QFormLayout* kinds = addGroup(tr("Material Kinds in the Navigator"));
    auto* kindChoices = new QWidget();
    auto* kindsLayout = new QVBoxLayout(kindChoices);
    kindsLayout->setContentsMargins(0, 0, 0, 0);
    kindsLayout->setSpacing(4);
    for (const QString& list : MaterialList::settingsKinds()) {
        const int count = MaterialList::countOf(list);
        auto* box = new QCheckBox(count < 0 ? MaterialList::nameOf(list)
                                            //: A kind of materials with the number of its
                                            //: materials in the open book. In Polish: %1 (%2)
                                            : tr("%1 (%2)").arg(MaterialList::nameOf(list))
                                                  .arg(count));
        box->setObjectName(QStringLiteral("navigatorKind_") + list);
        box->setProperty("kind", list);
        box->setIcon(core::ArtProvider::getInstance().getIcon(MaterialList::iconIdOf(list),
                                                              core::IconContext::Menu));
        kindsLayout->addWidget(box);
        m_kindBoxes << box;
    }
    //: In Polish: Odznaczony typ znika z Nawigatora we wszystkich książkach. Jego materiały
    //: zostają w książce i są we „Wszystkich materiałach”; po zaznaczeniu typ wraca. To samo
    //: robi polecenie „Ukryj typ” w menu typu w Nawigatorze. Liczby dotyczą otwartej książki.
    addNote(kindsLayout, tr("A kind unchecked leaves the Navigator in every book. Its materials "
                            "stay in the book and in All Materials; checked again, the kind "
                            "comes back. \"Hide Kind\" in the menu of a kind in the Navigator "
                            "does the same. The numbers are those of the open book."));
    //: In Polish: &Pokazuj:
    auto* kindsLabel = new QLabel(tr("&Show:"));
    if (!m_kindBoxes.isEmpty()) {
        kindsLabel->setBuddy(m_kindBoxes.constFirst());
    }
    kinds->addRow(kindsLabel, kindChoices);

    Binding hidden;
    hidden.key = []() { return std::string(MaterialList::HIDDEN_SETTING); };
    // In the order of the boxes, so that the same kinds are the same value
    hidden.stored = [this]() {
        const QStringList kinds = MaterialList::hiddenKinds();
        json value = json::array();
        for (const QCheckBox* box : m_kindBoxes) {
            if (kinds.contains(box->property("kind").toString())) {
                value.push_back(box->property("kind").toString().toStdString());
            }
        }
        return value;
    };
    hidden.shown = [this]() {
        json value = json::array();
        for (const QCheckBox* box : m_kindBoxes) {
            if (!box->isChecked()) {
                value.push_back(box->property("kind").toString().toStdString());
            }
        }
        return value;
    };
    hidden.show = [this](const json& value) {
        for (QCheckBox* box : m_kindBoxes) {
            const std::string kind = box->property("kind").toString().toStdString();
            box->setChecked(!value.is_array() ||
                            std::find(value.begin(), value.end(), json(kind)) == value.end());
        }
    };
    hidden.store = [&settings](const json& value) {
        settings.set<json>(MaterialList::HIDDEN_SETTING, value);
    };
    bindCustom(std::move(hidden));
}

void NavigatorPage::createCover()
{
    auto& settings = core::SettingsManager::getInstance();

    // The texts of the cover: the unit of their lengths and the targets of their kinds
    //: The group of Settings > Navigator of the materials of the cover. In Polish: Okładka
    QFormLayout* cover = addGroup(tr("Cover"));
    m_coverUnit = addChoice(cover,
                            //: In Polish: &Długość liczona w:
                            tr("&Length counted in:"),
                            //: The unit of the lengths of the texts of the cover. In Polish:
                            //: znakach ze spacjami
                            {{tr("characters with spaces"), QLatin1String(CHARACTERS), QString()},
                             //: In Polish: słowach
                             {tr("words"), QLatin1String(WORDS), QString()}},
                            std::string());
    m_coverUnit->setObjectName(QStringLiteral("navigatorCoverUnit"));

    Binding unit;
    unit.key = []() { return std::string(core::CoverMaterial::UNIT_SETTING); };
    unit.stored = []() {
        return json(core::CoverMaterial::countsWords() ? WORDS : CHARACTERS);
    };
    unit.shown = [this]() { return json(m_coverUnit->currentData().toString().toStdString()); };
    unit.show = [this](const json& value) {
        const bool words = value.is_string() && value.get<std::string>() == WORDS;
        m_coverUnit->setCurrentIndex(words ? 1 : 0);
    };
    unit.store = [&settings](const json& value) {
        settings.set<std::string>(core::CoverMaterial::UNIT_SETTING, value.get<std::string>());
    };
    bindCustom(std::move(unit));

    // The targets of the unit chosen; those of the other unit wait on their page. The unit is
    // said above them, so that the page fits a narrow window
    auto* targetsLabel = new QLabel();
    targetsLabel->setWordWrap(true);
    cover->addRow(targetsLabel);
    auto* targets = new QStackedWidget();
    for (const bool words : {false, true}) {
        auto* unitPage = new QWidget();
        auto* grid = new QGridLayout(unitPage);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);
        int row = 0;
        for (const char* part : {core::CoverMaterial::BLURB, core::CoverMaterial::TAGLINE,
                                 core::CoverMaterial::OPINION}) {
            const QString partName = CoverNames::partName(QString::fromLatin1(part));
            auto* label = new QLabel(partName);
            auto* spin = new QSpinBox();
            spin->setRange(1, MAXIMUM_TARGET);
            spin->setObjectName(QStringLiteral("navigatorCoverTarget_%1_%2")
                                    .arg(QString::fromLatin1(part),
                                         words ? QStringLiteral("words")
                                               : QStringLiteral("characters")));
            //: The accessible name of a target: %1 the kind of text, %2 the unit. In Polish:
            //: %1, %2
            spin->setAccessibleName(tr("%1, %2").arg(partName, CoverNames::unitName(words)));
            label->setBuddy(spin);
            grid->addWidget(label, row, 0);
            grid->addWidget(spin, row, 1);
            bind(spin, core::CoverMaterial::targetSettingOf(QString::fromLatin1(part), words));
            (words ? m_wordTargets : m_characterTargets) << spin;
            ++row;
        }
        grid->setColumnStretch(2, 1);
        targets->addWidget(unitPage);
    }
    cover->addRow(targets);
    //: In Polish: Każdy materiał może mieć własną docelową długość na swojej karcie. Wydawnictwa
    //: podają zwykle znaki ze spacjami.
    addNote(cover, tr("Each material can have its own target length on its card. Publishers "
                      "usually count characters with spaces."));
    const auto showUnit = [this, targets, targetsLabel]() {
        const bool words = m_coverUnit->currentData().toString() == QLatin1String(WORDS);
        targets->setCurrentIndex(words ? 1 : 0);
        targetsLabel->setText(words
                                  //: In Polish: Docelowa długość w słowach:
                                  ? tr("Target length in words:")
                                  //: In Polish: Docelowa długość w znakach ze spacjami:
                                  : tr("Target length in characters with spaces:"));
    };
    connect(m_coverUnit, &QComboBox::currentIndexChanged, this, showUnit);
    whenLoaded(showUnit);
}

} // namespace gui
} // namespace kalahari
