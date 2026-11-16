// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cards_page.cpp
/// @brief Settings page: Cards
///
/// The colors of the cards of characters, locations and items: whether the Navigator shows
/// the color of a card beside its name, and the palette that new cards take their colors from,
/// the color that the fewest cards of the book have first. A changed palette does not change
/// the colors the cards have. The locations in the Navigator: in a tree, each under the one it
/// is part of, or in alphabetical order. What the Properties panel shows of the cards at the
/// cursor of a text: the card whose name it is in and the cards linked with the paragraph, only
/// those, or none.

#include "kalahari/core/card_data.h"
#include "kalahari/core/settings_manager.h"
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

    addNavigatorGroup();
    addTextGroup();
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

    Binding tree;
    tree.key = []() { return std::string(NavigatorPanel::CARDS_TREE_SETTING); };
    tree.stored = []() { return json(NavigatorPanel::cardsInTrees() ? TREE : LIST); };
    tree.shown = [this]() { return json(m_list->isChecked() ? LIST : TREE); };
    tree.show = [this](const json& value) {
        const bool list = value.is_string() && value.get<std::string>() == LIST;
        (list ? m_list : m_tree)->setChecked(true);
    };
    tree.store = [](const json& value) {
        core::SettingsManager::getInstance().set<std::string>(NavigatorPanel::CARDS_TREE_SETTING,
                                                             value.get<std::string>());
    };
    bindCustom(std::move(tree));
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

    Binding atCursor;
    atCursor.key = []() { return std::string(CursorCardBox::SETTING); };
    atCursor.stored = []() {
        return json(core::SettingsManager::getInstance().get<std::string>(
            CursorCardBox::SETTING, CursorCardBox::NAMES));
    };
    atCursor.shown = [this]() {
        for (const char* value :
             {CursorCardBox::NAMES, CursorCardBox::PARAGRAPHS, CursorCardBox::NONE}) {
            if (atCursorButton(value)->isChecked()) {
                return json(value);
            }
        }
        return json(CursorCardBox::NAMES);
    };
    atCursor.show = [this](const json& value) {
        QRadioButton* button =
            value.is_string() ? atCursorButton(value.get<std::string>().c_str()) : nullptr;
        (button != nullptr ? button : m_atCursorNames)->setChecked(true);
    };
    atCursor.store = [](const json& value) {
        core::SettingsManager::getInstance().set<std::string>(CursorCardBox::SETTING,
                                                             value.get<std::string>());
    };
    bindCustom(std::move(atCursor));

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
