// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view_internal.h
/// @brief PlotBoardView: the sizes of the cards, the parts and the drawers, the fonts of the
/// cards and the ids of the scenes; used only by the sources of the view

#ifndef KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_INTERNAL_H
#define KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_INTERNAL_H

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

#include <optional>
#include <utility>

namespace kalahari::gui::board_view {

constexpr int MARGIN = 12;          ///< Around the cards
constexpr int GAP = 10;             ///< Between two cards
constexpr int HALF_GAP = GAP / 2;   ///< The part of the gap that belongs to a card
constexpr int BAND_GAP = 22;        ///< Between two parts
constexpr int PADDING = 8;          ///< Inside a card
constexpr int STRIP = 4;            ///< The color of a card at its top
constexpr int LINE_GAP = 3;         ///< Between the lines of a card
constexpr int RADIUS = 4;           ///< Of the corners of a card
constexpr int SCROLL_STEP = 24;     ///< A step of the scroll bars
constexpr int SCROLL_EDGE = 2 * MARGIN;  ///< Where the board scrolls while a card is dragged
constexpr int SCENE_INDENT = 14;    ///< Of the cards of the scenes under their text, in columns
constexpr int DRAWER_PADDING = 8;   ///< Inside the drawer of the scenes of a text, in a grid
constexpr int DRAWER_EDGE = 3;      ///< The colored edge of a drawer on its left
constexpr int HEADER_PADDING = 8;   ///< Under the title of a part, in its header
constexpr int HEADER_GAP = 6;       ///< Between the header of a part and its cards
constexpr int HEADER_LINE = 2;      ///< The line under the title of a part
constexpr int HEADER_TITLE_GAP = 12;  ///< Between the title of a part and its words
constexpr int LABEL_PADDING = 4;    ///< Around a small text: a drawer's, the scenes of a card
constexpr int MIN_CARD_WIDTH = 120; ///< The narrowest card, whatever the font
constexpr int MARKER_WIDTH = 3;     ///< The mark of the place a card dragged goes to
constexpr int MARKER_GAP = 4;       ///< Between the mark of the start of a part and its cards
constexpr int STATE_GAP = 6;        ///< Between the kind of a card and its state
constexpr int DOT_GAP = 4;          ///< Between the dot of the state and its name
constexpr int NOTE_GAP = 6;         ///< Between the line of the plot and "3 scenes"
constexpr int ICON_GAP = 3;         ///< Between an icon of the line of the plot and its text
constexpr int ITEM_GAP = 10;        ///< Between the items of the line of the plot
constexpr int MIN_ICON = 8;         ///< The smallest icon of the line of the plot
constexpr int ICON_INSET = 3;       ///< The icons of the line of the plot are this smaller
constexpr int RING_WIDTH = 2;       ///< The ring of the card chosen

constexpr double SMALL_FONT_SCALE = 0.86;   ///< Of the small lines: the kind, the plot
constexpr double MARKED_OPACITY = 0.55;     ///< Of a card marked to move (Ctrl+X)
constexpr double PLANNED_OPACITY = 0.6;     ///< Of a scene in the plan among the scenes written
constexpr double HALF_PIXEL = 0.5;          ///< The border of a card on the middle of a pixel
constexpr float QUIET_RING_ALPHA = 0.5F;    ///< The ring of the card chosen without the keys

constexpr int DEFAULT_WIDTH = 640;   ///< The size the board asks for
constexpr int DEFAULT_HEIGHT = 360;  ///< The size the board asks for

constexpr int SMALL_CARD_CHARACTERS = 22;   ///< The width of a small card in characters
constexpr int MEDIUM_CARD_CHARACTERS = 28;  ///< The width of a medium card in characters
constexpr int LARGE_CARD_CHARACTERS = 36;   ///< The width of a large card in characters
constexpr int SMALL_CARD_LINES = 2;   ///< The lines of the description on a small card
constexpr int MEDIUM_CARD_LINES = 3;  ///< The lines of the description on a medium card
constexpr int LARGE_CARD_LINES = 5;   ///< The lines of the description on a large card

/// @brief The width of a card in average characters of the font
inline int widthInCharacters(PlotBoardView::CardSize size) {
    switch (size) {
    case PlotBoardView::CardSize::Small:
        return SMALL_CARD_CHARACTERS;
    case PlotBoardView::CardSize::Large:
        return LARGE_CARD_CHARACTERS;
    case PlotBoardView::CardSize::Medium:
        break;
    }
    return MEDIUM_CARD_CHARACTERS;
}

/// @brief The lines of the description on a card
inline int descriptionLines(PlotBoardView::CardSize size) {
    switch (size) {
    case PlotBoardView::CardSize::Small:
        return SMALL_CARD_LINES;
    case PlotBoardView::CardSize::Large:
        return LARGE_CARD_LINES;
    case PlotBoardView::CardSize::Medium:
        break;
    }
    return MEDIUM_CARD_LINES;
}

/// @brief The lines of the title on a card
inline int titleLines(PlotBoardView::CardSize size) {
    return size == PlotBoardView::CardSize::Large ? 2 : 1;
}

/// @brief The font of the small lines of a card: its kind and state, its plot
inline QFont smallFont(const QFont& font) {
    QFont small = font;
    small.setPointSizeF(font.pointSizeF() * SMALL_FONT_SCALE);
    return small;
}

/// @brief The font of the titles: of the cards, of the parts
inline QFont boldFont(const QFont& font) {
    QFont bold = font;
    bold.setBold(true);
    return bold;
}

/// @brief The color of the text that says less: the words, the plot
inline QColor quietColor(const QPalette& palette) {
    return palette.color(QPalette::PlaceholderText);
}

/// @brief Whether card @p cardId is the card of a scene: written or in the plan
inline bool isScene(const QString& cardId) {
    return PlotBoard::sceneOf(cardId).has_value() || PlotBoard::plannedOf(cardId).has_value();
}

/// @brief The text of card @p cardId: of a scene, the text of the scene; else the card itself
inline QString textOfCard(const QString& cardId) {
    return PlotBoard::textOf(cardId);
}

}  // namespace kalahari::gui::board_view

#endif  // KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_INTERNAL_H
