// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view_layout.cpp
/// @brief PlotBoardView: where the cards, the titles of the parts and the drawers of the
/// scenes are, in columns and in a grid

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_board_view_internal.h"

#include <QFontMetrics>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QWidget>

#include <algorithm>

namespace kalahari::gui {

using board_view::BAND_GAP;
using board_view::boldFont;
using board_view::descriptionLines;
using board_view::DRAWER_PADDING;
using board_view::GAP;
using board_view::HEADER_GAP;
using board_view::HEADER_PADDING;
using board_view::LABEL_PADDING;
using board_view::LINE_GAP;
using board_view::MARGIN;
using board_view::MIN_CARD_WIDTH;
using board_view::PADDING;
using board_view::SCENE_INDENT;
using board_view::smallFont;
using board_view::STRIP;
using board_view::titleLines;
using board_view::widthInCharacters;

QSize PlotBoardView::cardSizePixels() const {
    const QFontMetrics metrics(font());
    const QFontMetrics small(smallFont(font()));
    const QFontMetrics bold(boldFont(font()));
    const int width =
        std::max(MIN_CARD_WIDTH, metrics.averageCharWidth() * widthInCharacters(m_cardSize));
    int height = (2 * PADDING) + STRIP + small.height();
    height += LINE_GAP + (bold.lineSpacing() * titleLines(m_cardSize));
    if (m_fields.description) {
        height += LINE_GAP + (metrics.lineSpacing() * descriptionLines(m_cardSize));
    }
    if (m_fields.pov || m_fields.place || m_fields.time) {
        height += LINE_GAP + small.height();
    }
    return {width, height};
}

void PlotBoardView::relayout() {
    m_cards.clear();
    m_bandPlaces.clear();
    m_drawers.clear();
    m_sectionPlaces.clear();
    const QSize card = cardSizePixels();
    const int header = QFontMetrics(boldFont(font())).height() + HEADER_PADDING;
    if (m_arrangement == Arrangement::Columns) {
        layOutColumns(card, header);
    } else {
        layOutGrid(card, header);
    }
    updateScrollBars();
}

bool PlotBoardView::sectionsOnBoard() const {
    return m_showSections && !m_bands.isEmpty();
}

const PlotBand& PlotBoardView::sectionOf(const SectionPlace& place) const {
    return place.back ? m_back : m_front;
}

int PlotBoardView::layOutSectionColumns(bool back, int x, const QSize& card, int header) {
    const PlotBand& section = back ? m_back : m_front;
    if (!sectionsOnBoard() || section.cards.isEmpty()) {
        return x;
    }
    const int count = static_cast<int>(section.cards.size());
    const int width = (count * card.width()) + ((count - 1) * GAP);
    const int cardsTop = MARGIN + header + HEADER_GAP;
    SectionPlace place;
    place.back = back;
    place.header = QRect(x, MARGIN, width, header);
    for (int i = 0; i < count; ++i) {
        place.cards << QRect(x + (i * (card.width() + GAP)), cardsTop, card.width(),
                             card.height());
    }
    m_sectionPlaces << place;
    return x + width + BAND_GAP;
}

int PlotBoardView::layOutSectionGrid(bool back, int top, const GridRows& rows) {
    const PlotBand& section = back ? m_back : m_front;
    if (!sectionsOnBoard() || section.cards.isEmpty()) {
        return top;
    }
    SectionPlace place;
    place.back = back;
    place.header = QRect(MARGIN, top, rows.width, rows.header);
    const int cardsTop = top + rows.header + HEADER_GAP;
    const QSize card = rows.card;
    for (int i = 0; i < static_cast<int>(section.cards.size()); ++i) {
        place.cards << QRect(MARGIN + ((i % rows.perRow) * (card.width() + GAP)),
                             cardsTop + ((i / rows.perRow) * (card.height() + GAP)),
                             card.width(), card.height());
    }
    const int bottom = place.cards.constLast().bottom() + 1;
    m_sectionPlaces << place;
    return bottom + BAND_GAP;
}

void PlotBoardView::layOutColumns(const QSize& card, int header) {
    // The front section, a preview, before the bands; the back section after them
    int x = layOutSectionColumns(false, MARGIN, card, header);
    const int cardsTop = MARGIN + header + HEADER_GAP;
    int bottom = cardsTop + card.height();
    int column = 0;
    for (int b = 0; b < m_bands.size(); ++b) {
        const int count = static_cast<int>(m_bands.at(b).cards.size());
        const int width =
            count > 0 ? (count * card.width()) + ((count - 1) * GAP) : card.width();
        int bandBottom = cardsTop + card.height();
        for (int i = 0; i < count; ++i, ++column) {
            const QRect rect(x + (i * (card.width() + GAP)), cardsTop, card.width(),
                             card.height());
            const CardPlace text{
                .id = m_bands.at(b).cards.at(i).id, .band = b, .index = i, .scene = -1,
                .column = column, .rect = rect};
            bandBottom = std::max(bandBottom, layOutColumn(text, card));
        }
        BandPlace place;
        place.band = b;
        place.header = QRect(x, MARGIN, width, header);
        place.area = QRect(x, cardsTop, width, bandBottom - cardsTop);
        m_bandPlaces << place;
        bottom = std::max(bottom, bandBottom);
        x += width + BAND_GAP;
    }
    x = layOutSectionColumns(true, x, card, header);
    m_contentsSize = QSize(std::max(x - BAND_GAP + MARGIN, 0), bottom + MARGIN);
}

int PlotBoardView::layOutColumn(const CardPlace& text, const QSize& card) {
    m_cards << text;
    int bottom = text.rect.bottom() + 1;
    if (!m_showScenes) {
        return bottom;
    }
    // Its scenes under it, a little to the right
    const QList<PlotCard>& scenes = cardOfPlace(text).scenes;
    for (int s = 0; s < scenes.size(); ++s) {
        const QRect rect(text.rect.left() + SCENE_INDENT, bottom + GAP,
                         card.width() - SCENE_INDENT, card.height());
        m_cards << CardPlace{.id = scenes.at(s).id, .band = text.band, .index = text.index,
                             .scene = s, .column = text.column, .rect = rect};
        bottom = rect.bottom() + 1;
    }
    return bottom;
}

void PlotBoardView::layOutGrid(const QSize& card, int header) {
    GridRows rows;
    rows.card = card;
    rows.header = header;
    rows.width = std::max(card.width(), viewport()->width() - (2 * MARGIN));
    rows.perRow = cardsPerRow();
    rows.perDrawerRow =
        std::max(1, (rows.width - (2 * DRAWER_PADDING) + GAP) / (card.width() + GAP));
    rows.label = QFontMetrics(smallFont(font())).height() + LABEL_PADDING;
    int y = layOutSectionGrid(false, MARGIN, rows);
    for (int b = 0; b < m_bands.size(); ++b) {
        y = layOutGridBand(b, y, rows) + BAND_GAP;
    }
    y = layOutSectionGrid(true, y, rows);
    m_contentsSize = QSize(rows.width + (2 * MARGIN), std::max(y - BAND_GAP + MARGIN, 0));
}

int PlotBoardView::layOutGridBand(int band, int top, const GridRows& rows) {
    const PlotBand& shown = m_bands.at(band);
    BandPlace place;
    place.band = band;
    if (!shown.partId.isEmpty()) {
        place.header = QRect(MARGIN, top, rows.width, rows.header);
        top += rows.header + HEADER_GAP;
    }
    const QSize card = rows.card;
    int rowTop = top;
    int column = 0;
    int bottom = top + card.height();  // an empty part has the place of a card
    const int count = static_cast<int>(shown.cards.size());
    for (int i = 0; i < count; ++i) {
        if (column == rows.perRow) {
            column = 0;
            rowTop = bottom + GAP;
        }
        const PlotCard& text = shown.cards.at(i);
        const QRect rect(MARGIN + (column * (card.width() + GAP)), rowTop, card.width(),
                         card.height());
        const CardPlace textPlace{.id = text.id, .band = band, .index = i, .scene = -1,
                                  .column = 0, .rect = rect};
        m_cards << textPlace;
        bottom = rect.bottom() + 1;
        ++column;
        if (!text.scenes.isEmpty() && scenesShown(text.id)) {
            // The drawer of its scenes under its row, as wide as the board; the texts after
            // it go on under the drawer, in a row of their own
            bottom = layOutDrawer(textPlace, rowTop + card.height() + GAP, rows);
            column = rows.perRow;
        }
    }
    place.area = QRect(MARGIN, top, count > 0 ? rows.width : card.width(), bottom - top);
    m_bandPlaces << place;
    return bottom;
}

int PlotBoardView::layOutDrawer(const CardPlace& text, int top, const GridRows& rows) {
    const PlotCard& shown = cardOfPlace(text);
    const QSize card = rows.card;
    const int perRow = rows.perDrawerRow;
    const int scenes = static_cast<int>(shown.scenes.size());
    const int lines = (scenes + perRow - 1) / perRow;
    const int height =
        DRAWER_PADDING + rows.label + (lines * (card.height() + GAP)) - GAP + DRAWER_PADDING;
    m_drawers << DrawerPlace{.textId = shown.id, .title = shown.title,
                             .rect = QRect(MARGIN, top, rows.width, height)};
    for (int s = 0; s < scenes; ++s) {
        const int left = MARGIN + DRAWER_PADDING + ((s % perRow) * (card.width() + GAP));
        const int sceneTop =
            top + DRAWER_PADDING + rows.label + ((s / perRow) * (card.height() + GAP));
        m_cards << CardPlace{.id = shown.scenes.at(s).id, .band = text.band,
                             .index = text.index, .scene = s, .column = 0,
                             .rect = QRect(left, sceneTop, card.width(), card.height())};
    }
    return top + height;
}

void PlotBoardView::updateScrollBars() {
    const QSize area = viewport()->size();
    horizontalScrollBar()->setRange(0, std::max(0, m_contentsSize.width() - area.width()));
    horizontalScrollBar()->setPageStep(area.width());
    verticalScrollBar()->setRange(0, std::max(0, m_contentsSize.height() - area.height()));
    verticalScrollBar()->setPageStep(area.height());
}

int PlotBoardView::cardsPerRow() const {
    if (m_arrangement == Arrangement::Columns) {
        return std::max(1, static_cast<int>(PlotBoard::cardsOf(m_bands).size()));
    }
    const int width = cardSizePixels().width();
    const int available = std::max(width, viewport()->width() - (2 * MARGIN));
    return std::max(1, (available + GAP) / (width + GAP));
}

QRect PlotBoardView::cardRect(const QString& elementId) const {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return {};
    }
    return place->rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
}

const PlotBoardView::CardPlace* PlotBoardView::placeOf(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return nullptr;
    }
    for (const CardPlace& place : m_cards) {
        if (place.id == elementId) {
            return &place;
        }
    }
    return nullptr;
}

const PlotCard* PlotBoardView::cardOf(const QString& elementId) const {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return nullptr;
    }
    return &cardOfPlace(*place);
}

const PlotCard& PlotBoardView::cardOfPlace(const CardPlace& place) const {
    const PlotCard& text = m_bands.at(place.band).cards.at(place.index);
    return place.scene >= 0 ? text.scenes.at(place.scene) : text;
}

QString PlotBoardView::scenesNote(const PlotCard& card) const {
    if (!m_fields.scenes || card.scenes.isEmpty() ||
        (m_arrangement == Arrangement::Columns && m_showScenes)) {
        return {};
    }
    QString scenes = PlotBoard::scenesText(static_cast<int>(card.scenes.size()));
    if (m_arrangement == Arrangement::Columns) {
        return scenes;
    }
    // In a grid it opens and closes the drawer
    return (scenesShown(card.id) ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) +
           scenes;
}

bool PlotBoardView::isPlanBesideText(const CardPlace& place) const {
    if (place.scene < 0 || !cardOfPlace(place).planned) {
        return false;
    }
    const QList<PlotCard>& scenes = m_bands.at(place.band).cards.at(place.index).scenes;
    return std::ranges::any_of(scenes, [](const PlotCard& scene) { return !scene.planned; });
}

QRect PlotBoardView::scenesNoteRect(const CardPlace& place) const {
    const QString note = place.scene < 0 ? scenesNote(cardOfPlace(place)) : QString();
    if (note.isEmpty()) {
        return {};
    }
    const QFontMetrics metrics(smallFont(font()));
    const QRect inner = place.rect.adjusted(PADDING, PADDING + STRIP, -PADDING, -PADDING);
    const int width = metrics.horizontalAdvance(note) + LABEL_PADDING;
    return {inner.right() - width + 1, inner.bottom() - metrics.height() + 1, width,
            metrics.height()};
}

const PlotBoardView::CardPlace* PlotBoardView::cardAt(const QPoint& pos) const {
    for (const CardPlace& place : m_cards) {
        if (place.rect.contains(pos)) {
            return &place;
        }
    }
    return nullptr;
}

const PlotCard* PlotBoardView::sectionCardAt(const QPoint& pos) const {
    for (const SectionPlace& place : m_sectionPlaces) {
        for (qsizetype i = 0; i < place.cards.size(); ++i) {
            if (place.cards.at(i).contains(pos)) {
                return &sectionOf(place).cards.at(i);
            }
        }
    }
    return nullptr;
}

QRect PlotBoardView::sectionCardRect(const QString& elementId) const {
    for (const SectionPlace& place : m_sectionPlaces) {
        const QList<PlotCard>& cards = sectionOf(place).cards;
        for (qsizetype i = 0; i < cards.size(); ++i) {
            if (cards.at(i).id == elementId) {
                return place.cards.at(i).translated(-horizontalScrollBar()->value(),
                                                    -verticalScrollBar()->value());
            }
        }
    }
    return {};
}

QPoint PlotBoardView::contentsPos(const QPoint& viewportPos) const {
    return viewportPos + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}

}  // namespace kalahari::gui
