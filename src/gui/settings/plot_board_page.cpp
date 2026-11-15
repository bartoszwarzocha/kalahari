// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_page.cpp
/// @brief Settings page: Plot Board
///
/// Which scenes the plot board and the outline show: the scenes of the texts and those in
/// their plans (the default), the scenes of the texts only, or those in the plans only
/// (PlotBoard::SCENES_VARIANT_SETTING). The board follows the setting at once. The colors of
/// the cards are those of the theme: kept for each theme, as the other colors of the editor.

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <QComboBox>
#include <QFormLayout>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>
#include <vector>

namespace kalahari::gui {

PlotBoardPage::PlotBoardPage(QWidget* parent) : SettingsPage(parent) {
    //: A group of Settings > Plot Board. In Polish: Sceny
    QFormLayout* scenes = addGroup(tr("Scenes"));
    auto* variant = new QComboBox();
    variant->setObjectName(QStringLiteral("plotBoardScenes"));
    //: Which scenes the plot board shows. In Polish: tekstów i w ich planach
    variant->addItem(tr("of the texts and in their plans"), QLatin1String(PlotBoard::SCENES_BOTH));
    //: In Polish: tylko tekstów
    variant->addItem(tr("of the texts only"), QLatin1String(PlotBoard::SCENES_TEXT));
    //: In Polish: tylko w planach
    variant->addItem(tr("in the plans only"), QLatin1String(PlotBoard::SCENES_PLAN));
    //: In Polish: Sceny na tablicy:
    addField(scenes, tr("Scenes on the board:"), variant, PlotBoard::SCENES_VARIANT_SETTING);
    //: The note under the choice of the scenes. In Polish: Sceny tekstu to jego fragmenty między
    //: podziałami sceny (Wstaw > Podział sceny): przeniesienie karty przenosi tekst sceny. Sceny
    //: w planie są tylko w pliku książki, w przerywanej ramce: nie mają tekstu, słów ani stanu,
    //: a Enter na takiej karcie obok scen tekstu wstawia w jej miejscu podział sceny z jej
    //: opisem i otwiera tam tekst.
    addNote(scenes,
            tr("The scenes of a text are its parts between the scene breaks (Insert > Scene "
               "Break): moving a card moves the text of the scene. The scenes in the plan are "
               "only in the file of the book, in a dashed frame: they have no text, no words "
               "and no state, and Enter on such a card beside the scenes of the text puts a "
               "scene break with its description at its place and opens the text there."));

    //: A group of Settings > Plot Board. In Polish: Kolory kart
    QFormLayout* colors = addGroup(tr("Colors of the Cards"));
    //: In Polish: Kolory stanów tekstów, scen w planie i własne kolory kart. Każdy motyw ma
    //: swoje kolory: to są kolory bieżącego motywu.
    addNote(colors, tr("The colors of the states of the texts, of the scenes in the plan and "
                       "the own colors of the cards. Each theme has its own colors: these are "
                       "the colors of the current theme."));
    const std::vector<std::string>& keys = PlotColors::keys();
    const QStringList titles{PlotBoard::statusName(QStringLiteral("draft")),
                             PlotBoard::statusName(QStringLiteral("revision")),
                             PlotBoard::statusName(QStringLiteral("final")),
                             PlotBoard::plannedName()};
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto own = static_cast<qsizetype>(i) - titles.size();
        addThemeColor(colors,
                      own < 0 ? titles.at(static_cast<qsizetype>(i))
                              : PlotColors::ownTitle(PlotColors::ownNames().value(own)),
                      keys.at(i));
    }
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

}  // namespace kalahari::gui
