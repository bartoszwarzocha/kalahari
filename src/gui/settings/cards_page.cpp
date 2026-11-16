// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cards_page.cpp
/// @brief Settings page: Cards
///
/// The full card: the data beside the description, the relations, the chapters and the
/// materials, or each of them on a tab under the header. The portraits: square or round, and
/// without a picture the initials on the color of the card or the icon of its kind. The colors
/// of the cards of characters, locations and items: whether the Navigator shows
/// the color of a card beside its name, and the palette that new cards take their colors from,
/// the color that the fewest cards of the book have first. A changed palette does not change
/// the colors the cards have. The locations in the Navigator: in a tree, each under the one it
/// is part of, or in alphabetical order. What the Properties panel shows of the cards at the
/// cursor of a text: the card whose name it is in and the cards linked with the paragraph, only
/// those, or none. The fields of the cards that new books start with, in the window Card
/// Fields.

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/card_field_set.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/card_fields_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/cursor_card_box.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

using json = nlohmann::json;

/// The values of NavigatorPanel::CARDS_TREE_SETTING
constexpr const char* TREE = "tree";
constexpr const char* LIST = "list";

/// @p colors as the setting keeps them, a list of "#rrggbb"
json jsonOf(const QStringList& colors) {
    json list = json::array();
    for (const QString& color : colors) {
        list.push_back(color.toStdString());
    }
    return list;
}

/// The colors of the setting @p value; none when it is no list
QStringList colorsOf(const json& value) {
    QStringList colors;
    if (!value.is_array()) {
        return colors;
    }
    for (const json& each : value) {
        if (each.is_string()) {
            colors << QString::fromStdString(each.get<std::string>());
        }
    }
    return colors;
}

}  // namespace

CardsPage::CardsPage(QWidget* parent) : SettingsPage(parent) {
    addFullCardGroup();
    addPortraitGroup();
    addColorsGroup();
    addTextGroup();
    addNavigatorGroup();
    addFieldsGroup();
}

void CardsPage::bindChoices(const char* key, const char* fallback, const Choices& choices) {
    Binding binding;
    binding.key = [key]() { return std::string(key); };
    binding.stored = [key, fallback]() {
        return json(core::SettingsManager::getInstance().get<std::string>(key, fallback));
    };
    binding.shown = [choices, fallback]() {
        for (const auto& [button, value] : choices) {
            if (button->isChecked()) {
                return json(value);
            }
        }
        return json(fallback);
    };
    binding.show = [choices](const json& value) {
        const std::string shown = value.is_string() ? value.get<std::string>() : std::string();
        QRadioButton* chosen = choices.front().first;
        for (const auto& [button, each] : choices) {
            if (shown == each) {
                chosen = button;
            }
        }
        chosen->setChecked(true);
    };
    binding.store = [key](const json& value) {
        core::SettingsManager::getInstance().set<std::string>(key, value.get<std::string>());
    };
    bindCustom(std::move(binding));
}

void CardsPage::addFullCardGroup() {
    //: Settings > Cards: the group of the full card of a character, a location or an item. In
    //: Polish: Pełna karta
    QFormLayout* full = addGroup(tr("Full Card"));
    //: The data of the full card beside its description. In Polish: Dwie kolumny
    m_columns = new QRadioButton(tr("Two columns"));
    m_columns->setObjectName(QStringLiteral("cardsLayoutColumns"));
    //: In Polish: Zakładki
    m_tabs = new QRadioButton(tr("Tabs"));
    m_tabs->setObjectName(QStringLiteral("cardsLayoutTabs"));
    auto* choices = new QWidget();
    auto* choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->addWidget(m_columns);
    //: In Polish: Dane, odmiana i własne pola po lewej; opis, relacje, rozdziały i materiały
    //: po prawej.
    alignWithText(addNote(choicesLayout,
                          tr("The data, the declension and the own fields at the left; the "
                             "description, the relations, the chapters and the materials at "
                             "the right.")),
                  m_columns);
    choicesLayout->addWidget(m_tabs);
    //: In Polish: Ogólne, Opis, Relacje, Rozdziały i materiały – pod wspólnym nagłówkiem;
    //: Alt i podkreślona litera wybiera zakładkę.
    alignWithText(addNote(choicesLayout,
                          tr("General, Description, Relations, Chapters and Materials – under "
                             "the header; Alt and the underlined letter choose a tab.")),
                  m_tabs);
    //: The label of the choice of the layout of the full card. In Polish: Układ:
    auto* label = new QLabel(tr("Layout:"));
    label->setBuddy(m_columns);
    full->addRow(label, choices);
    bindChoices(CardPage::LAYOUT_SETTING, CardPage::LAYOUT_COLUMNS,
                {{m_columns, CardPage::LAYOUT_COLUMNS}, {m_tabs, CardPage::LAYOUT_TABS}});
}

void CardsPage::addPortraitGroup() {
    //: Settings > Cards: the group of the portraits of the cards. In Polish: Portret
    QFormLayout* portrait = addGroup(tr("Portrait"));
    const auto pair = [](QRadioButton* first, QRadioButton* second) {
        auto* box = new QWidget();
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(first);
        layout->addWidget(second);
        return box;
    };
    //: The shape of the portraits. In Polish: Kwadrat
    m_square = new QRadioButton(tr("Square"));
    m_square->setObjectName(QStringLiteral("cardsPortraitSquare"));
    //: In Polish: Koło
    m_circle = new QRadioButton(tr("Circle"));
    m_circle->setObjectName(QStringLiteral("cardsPortraitCircle"));
    //: In Polish: Kształt:
    auto* shape = new QLabel(tr("Shape:"));
    shape->setBuddy(m_square);
    portrait->addRow(shape, pair(m_square, m_circle));
    bindChoices(CardPage::SHAPE_SETTING, CardPage::SHAPE_SQUARE,
                {{m_square, CardPage::SHAPE_SQUARE}, {m_circle, CardPage::SHAPE_CIRCLE}});

    //: What a portrait without a picture shows. In Polish: Inicjały na kolorze karty
    m_initials = new QRadioButton(tr("Initials on the color of the card"));
    m_initials->setObjectName(QStringLiteral("cardsPortraitInitials"));
    //: In Polish: Ikona rodzaju
    m_kindIcon = new QRadioButton(tr("Icon of the kind"));
    m_kindIcon->setObjectName(QStringLiteral("cardsPortraitIcon"));
    //: In Polish: Bez obrazu:
    auto* without = new QLabel(tr("Without a picture:"));
    without->setBuddy(m_initials);
    portrait->addRow(without, pair(m_initials, m_kindIcon));
    bindChoices(CardPage::NO_PICTURE_SETTING, CardPage::NO_PICTURE_INITIALS,
                {{m_initials, CardPage::NO_PICTURE_INITIALS},
                 {m_kindIcon, CardPage::NO_PICTURE_ICON}});
}

void CardsPage::addColorsGroup() {
    //: Settings > Cards: the group of the colors of the cards of characters, locations and
    //: items. In Polish: Kolory
    QFormLayout* colors = addGroup(tr("Colors"));
    //: The dot of the color of a card beside its name in the Navigator. In Polish: Kropki
    //: kolorów kart w Nawigatorze
    addCheckBox(colors, tr("Color dots of the cards in the Navigator"),
                core::CardColors::NAVIGATOR_DOT_SETTING);

    //: What the color window chooses from the palette of the cards. In Polish: Kolor palety
    //: nowych kart
    m_palette = new PaletteEdit(tr("A color of the palette of new cards"));
    m_palette->setObjectName(QStringLiteral("cardsPalette"));
    //: In Polish: Przywróć domyślne
    m_restorePalette = new QPushButton(tr("Restore Defaults"));
    m_restorePalette->setObjectName(QStringLiteral("cardsPaletteRestore"));
    connect(m_restorePalette, &QPushButton::clicked, this,
            [this]() { m_palette->setColors(core::CardColors::defaultPalette()); });
    auto* paletteBox = new QWidget();
    auto* paletteLayout = new QVBoxLayout(paletteBox);
    paletteLayout->setContentsMargins(0, 0, 0, 0);
    paletteLayout->addWidget(m_palette);
    auto* restoreRow = new QHBoxLayout();
    restoreRow->addWidget(m_restorePalette);
    restoreRow->addStretch(1);
    paletteLayout->addLayout(restoreRow);
    //: The palette that new cards of characters, locations and items take their colors from.
    //: In Polish: Paleta nowych kart:
    colors->addRow(new QLabel(tr("Palette of new cards:")), paletteBox);
    addNote(colors,
            //: In Polish: Nowa karta dostaje ten kolor palety, który ma najmniej kart książki.
            //: Kliknięcie próbki zmienia kolor, Delete go usuwa, a „+” albo Insert dodaje nowy.
            //: Kolor jednej karty zmienia się na jej pełnej karcie.
            tr("A new card gets the color of the palette that the fewest cards of the book "
               "have. A click on a swatch changes the color, Delete takes it out, \"+\" or "
               "Insert adds one. The color of one card is changed on its full card."));

    Binding palette;
    palette.key = []() { return std::string(core::CardColors::PALETTE_SETTING); };
    palette.stored = []() { return jsonOf(core::CardColors::palette()); };
    palette.shown = [this]() { return jsonOf(m_palette->colors()); };
    palette.show = [this](const json& value) { m_palette->setColors(colorsOf(value)); };
    palette.store = [](const json& value) {
        core::SettingsManager::getInstance().set<json>(core::CardColors::PALETTE_SETTING, value);
    };
    bindCustom(std::move(palette));
}

void CardsPage::addNavigatorGroup() {
    //: Settings > Cards: the group of how the cards are in the Navigator. In Polish: Nawigator
    QFormLayout* navigator = addGroup(tr("Navigator"));
    //: The locations in the Navigator, each under the location it is part of. In Polish:
    //: Drzewo według miejsca nadrzędnego
    m_tree = new QRadioButton(tr("A tree by parent location"));
    m_tree->setObjectName(QStringLiteral("cardsNavigatorTree"));
    //: In Polish: Lista alfabetyczna
    m_list = new QRadioButton(tr("An alphabetical list"));
    m_list->setObjectName(QStringLiteral("cardsNavigatorList"));
    auto* choices = new QWidget();
    auto* choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->addWidget(m_tree);
    //: In Polish: Na przykład Żoliborz pod Warszawą. Tak samo są ułożone karty innych rodzajów,
    //: których pole wskazuje kartę tego samego rodzaju.
    alignWithText(addNote(choicesLayout,
                          tr("For example, a district under its city. The cards of other "
                             "kinds whose field names a card of the same kind are in trees as "
                             "well.")),
                  m_tree);
    choicesLayout->addWidget(m_list);
    //: The label of the choice between a tree and a list of the locations. In Polish: Miejsca
    //: w Nawigatorze:
    auto* label = new QLabel(tr("Locations in the Navigator:"));
    label->setBuddy(m_tree);
    navigator->addRow(label, choices);

    bindChoices(NavigatorPanel::CARDS_TREE_SETTING, TREE, {{m_tree, TREE}, {m_list, LIST}});
}

void CardsPage::addTextGroup() {
    //: Settings > Cards: the group of the cards in the texts of the book. In Polish: W tekście
    QFormLayout* text = addGroup(tr("In the Text"));
    //: The Properties panel shows the card whose name the cursor is in and the cards linked
    //: with the paragraph. In Polish: Imiona i nazwy z kart oraz powiązane akapity
    m_atCursorNames = new QRadioButton(tr("Names of the cards and linked paragraphs"));
    m_atCursorNames->setObjectName(QStringLiteral("cardsAtCursorNames"));
    //: In Polish: Tylko powiązane akapity
    m_atCursorParagraphs = new QRadioButton(tr("Only linked paragraphs"));
    m_atCursorParagraphs->setObjectName(QStringLiteral("cardsAtCursorParagraphs"));
    //: In Polish: Nie pokazuj
    m_atCursorNone = new QRadioButton(tr("Do not show"));
    m_atCursorNone->setObjectName(QStringLiteral("cardsAtCursorNone"));
    auto* choices = new QWidget();
    auto* choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->addWidget(m_atCursorNames);
    //: In Polish: Gdy kursor jest w imieniu lub nazwie z karty, w dowolnej formie, panel
    //: „Właściwości” pokazuje tę kartę pod danymi tekstu. Formy zna słownik książki.
    alignWithText(addNote(choicesLayout,
                          tr("When the cursor is in the name of a card, in any of its forms, "
                             "the Properties panel shows the card under the data of the text. "
                             "The book's dictionary knows the forms.")),
                  m_atCursorNames);
    choicesLayout->addWidget(m_atCursorParagraphs);
    choicesLayout->addWidget(m_atCursorNone);
    //: The label of the choice of what the Properties panel shows of the cards at the cursor.
    //: In Polish: Karta przy kursorze:
    auto* label = new QLabel(tr("Card at the cursor:"));
    label->setBuddy(m_atCursorNames);
    text->addRow(label, choices);

    bindChoices(CursorCardBox::SETTING, CursorCardBox::NAMES,
                {{m_atCursorNames, CursorCardBox::NAMES},
                 {m_atCursorParagraphs, CursorCardBox::PARAGRAPHS},
                 {m_atCursorNone, CursorCardBox::NONE}});

    //: The signs of the kinds of the cards beside a paragraph linked with them, like the
    //: paperclip of a material; keep it short, as the narrowest window of the settings has
    //: room for about 40 characters. In Polish: Znaki kart przy powiązanych akapitach
    QCheckBox* marks = addCheckBox(text, tr("Signs of the cards beside the linked paragraphs"),
                                   ParagraphMarksCoordinator::CARD_MARKS_SETTING);
    marks->setObjectName(QStringLiteral("cardsParagraphMarks"));
    //: In Polish: Kliknięcie znaku otwiera kartę. Bez znaków panel „Właściwości” nadal wymienia
    //: karty akapitu.
    addNote(text, tr("A click on a sign opens the card. Without the signs, the Properties panel "
                     "still names the cards of the paragraph."));
}

void CardsPage::addFieldsGroup() {
    //: Settings > Cards: the group of the fields of the cards. In Polish: Pola kart
    QFormLayout* fields = addGroup(tr("Card Fields"));
    //: Opens the window Card Fields for the fields that new books start with. In Polish:
    //: Domyślne pola nowych książek...
    m_defaultFieldsButton = new QPushButton(tr("Default Fields of New Books..."));
    m_defaultFieldsButton->setObjectName(QStringLiteral("cardsDefaultFields"));
    m_defaultFieldsButton->setIcon(core::ArtProvider::getInstance().getIcon(
        QStringLiteral("card.fields"), core::IconContext::Button));
    connect(m_defaultFieldsButton, &QPushButton::clicked, this, &CardsPage::editDefaultFields);
    auto* row = new QHBoxLayout();
    row->addWidget(m_defaultFieldsButton);
    row->addStretch(1);
    fields->addRow(row);
    m_defaultFieldsLabel = addNote(fields, QString());
    m_defaultFieldsLabel->setObjectName(QStringLiteral("cardsDefaultFieldsNote"));

    Binding defaults;
    defaults.key = []() { return std::string(core::card_fields::DEFAULTS_SETTING); };
    defaults.stored = []() {
        return core::SettingsManager::getInstance().get<json>(
            core::card_fields::DEFAULTS_SETTING, json::object());
    };
    defaults.shown = [this]() { return m_defaultFields; };
    defaults.show = [this](const json& value) {
        m_defaultFields = value;
        showDefaultFields();
    };
    defaults.store = [](const json& value) {
        core::SettingsManager::getInstance().set<json>(core::card_fields::DEFAULTS_SETTING,
                                                       value);
    };
    bindCustom(std::move(defaults));
}

void CardsPage::editDefaultFields() {
    const std::optional<core::CardFieldSets> changed = dialogs::editDefaultCardFields(
        core::card_fields::fromSetting(m_defaultFields), window());
    if (changed) {
        m_defaultFields = core::card_fields::toSetting(*changed);
        showDefaultFields();
    }
}

void CardsPage::showDefaultFields() {
    QStringList kinds;
    const core::CardFieldSets sets = core::card_fields::fromSetting(m_defaultFields);
    const core::BookTypeRegistry& registry = core::ProjectManager::installedBookTypes();
    for (auto it = sets.cbegin(); it != sets.cend(); ++it) {
        const std::optional<core::KindReference> reference = core::KindReference::parse(it.key());
        const core::KindRef kind =
            reference ? registry.findKind(reference->packageId, reference->kindId)
                      : core::KindRef{};
        if (kind) {
            kinds << kind.kind->plural.text(programLanguage());
        }
    }
    if (kinds.isEmpty()) {
        //: In Polish: Nowe książki mają pola z typów książek. Przycisk „Pola...” na pełnej
        //: karcie zmienia pola jednej książki.
        m_defaultFieldsLabel->setText(tr("New books have the fields of the book types. "
                                         "Fields... on the full card changes the fields of one "
                                         "book."));
        return;
    }
    //: %1: the kinds with fields of their own, such as "Postacie, Przedmioty". In Polish:
    //: Własne pola w nowych książkach: %1. Przycisk „Pola...” na pełnej karcie zmienia pola
    //: jednej książki.
    m_defaultFieldsLabel->setText(tr("Fields of their own in new books: %1. Fields... on the "
                                     "full card changes the fields of one book.")
                                      .arg(kinds.join(QStringLiteral(", "))));
}

QRadioButton* CardsPage::layoutButton(const char* value) const {
    const std::string choice(value);
    if (choice == CardPage::LAYOUT_COLUMNS) {
        return m_columns;
    }
    return choice == CardPage::LAYOUT_TABS ? m_tabs : nullptr;
}

QRadioButton* CardsPage::portraitButton(const char* value) const {
    const std::string choice(value);
    if (choice == CardPage::SHAPE_SQUARE) {
        return m_square;
    }
    if (choice == CardPage::SHAPE_CIRCLE) {
        return m_circle;
    }
    if (choice == CardPage::NO_PICTURE_INITIALS) {
        return m_initials;
    }
    return choice == CardPage::NO_PICTURE_ICON ? m_kindIcon : nullptr;
}

QRadioButton* CardsPage::atCursorButton(const char* value) const {
    const std::string choice(value);
    if (choice == CursorCardBox::NAMES) {
        return m_atCursorNames;
    }
    if (choice == CursorCardBox::PARAGRAPHS) {
        return m_atCursorParagraphs;
    }
    return choice == CursorCardBox::NONE ? m_atCursorNone : nullptr;
}

}  // namespace kalahari::gui
