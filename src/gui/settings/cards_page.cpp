// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cards_page.cpp
/// @brief Settings page: Cards
///
/// The colors of the cards of characters, locations and items: whether the Navigator shows
/// the color of a card beside its name, and the palette that new cards take their colors from,
/// the color that the fewest cards of the book have first. A changed palette does not change
/// the colors the cards have.

#include "kalahari/core/card_data.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

using json = nlohmann::json;

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
}

}  // namespace kalahari::gui
