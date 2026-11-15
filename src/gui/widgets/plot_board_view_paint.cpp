// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view_paint.cpp
/// @brief PlotBoardView: the cards, the titles of the parts and the drawers of the scenes
/// painted

#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_board_view_internal.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRandomGenerator>
#include <QRect>
#include <QRectF>
#include <QScrollBar>
#include <QString>
#include <QTextLayout>
#include <QTextOption>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstdint>

namespace kalahari::gui {

using board_view::boldFont;
using board_view::descriptionLines;
using board_view::DOT_GAP;
using board_view::DRAWER_EDGE;
using board_view::DRAWER_PADDING;
using board_view::HALF_PIXEL;
using board_view::HEADER_LINE;
using board_view::HEADER_TITLE_GAP;
using board_view::ICON_GAP;
using board_view::ICON_INSET;
using board_view::ITEM_GAP;
using board_view::LABEL_PADDING;
using board_view::LINE_GAP;
using board_view::MARGIN;
using board_view::MARKED_OPACITY;
using board_view::MIN_ICON;
using board_view::NOTE_GAP;
using board_view::PADDING;
using board_view::PLANNED_OPACITY;
using board_view::quietColor;
using board_view::QUIET_RING_ALPHA;
using board_view::RADIUS;
using board_view::READABLE_CONTRAST;
using board_view::RING_WIDTH;
using board_view::SECTION_OPACITY;
using board_view::smallFont;
using board_view::STATE_GAP;
using board_view::STRIP;
using board_view::TINT_SHARE;
using board_view::titleLines;

namespace {

// The cork under the cards: a tile of its color with darker and lighter grains
constexpr int CORK_TILE = 64;              ///< The side of the tile
constexpr int CORK_GRAINS = 140;           ///< The grains of a tile
constexpr std::uint32_t CORK_SEED = 7;     ///< The same grains on every board
constexpr int CORK_DARKER = 116;           ///< The darker grains (QColor::darker())
constexpr int CORK_LIGHTER = 108;          ///< The lighter grains (QColor::lighter())
constexpr double CORK_GRAIN_MIN = 0.5;     ///< The smallest grain, its radius in pixels
constexpr double CORK_GRAIN_RANGE = 0.9;   ///< How much larger a grain can be
constexpr int CORK_LIGHT_SHARE = 3;        ///< One grain in this many is lighter

/// @brief The tile of the cork of color @p cork
QPixmap corkTile(const QColor& cork) {
    QPixmap tile(CORK_TILE, CORK_TILE);
    tile.fill(cork);
    QPainter painter(&tile);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QRandomGenerator grains(CORK_SEED);
    const std::array<QColor, 2> shades{cork.darker(CORK_DARKER), cork.lighter(CORK_LIGHTER)};
    for (int grain = 0; grain < CORK_GRAINS; ++grain) {
        const QPointF at(grains.bounded(static_cast<double>(CORK_TILE)),
                         grains.bounded(static_cast<double>(CORK_TILE)));
        const double radius = CORK_GRAIN_MIN + grains.bounded(CORK_GRAIN_RANGE);
        painter.setBrush(shades.at(grain % CORK_LIGHT_SHARE == 0 ? 1 : 0));
        painter.drawEllipse(at, radius, radius);
    }
    return tile;
}

/// @brief Draw @p text in at most @p maxLines lines of @p width from @p topLeft, the last line
/// ending in an ellipsis when the text is longer
/// @return The height drawn
int drawWrapped(QPainter& painter, const QPoint& topLeft, int width, const QString& text,
                int maxLines) {
    const QFontMetrics metrics(painter.font());
    QTextLayout layout(text, painter.font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    int y = 0;
    for (int lines = 1; lines <= maxLines; ++lines) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        const QPoint at(topLeft.x(), topLeft.y() + y + metrics.ascent());
        if (lines == maxLines && line.textStart() + line.textLength() < text.size()) {
            // The rest of the text does not fit: the last line ends in an ellipsis
            const QString rest = text.mid(line.textStart()).simplified();
            painter.drawText(at, metrics.elidedText(rest, Qt::ElideRight, width));
        } else {
            painter.drawText(at, text.mid(line.textStart(), line.textLength()).trimmed());
        }
        y += metrics.lineSpacing();
    }
    layout.endLayout();
    return y;
}

/// @brief The line of the plot at the bottom of a card, painted item by item
struct PlotLine {
    QFont font;      ///< Of its items
    int x = 0;       ///< Where the next item goes
    int right = 0;   ///< Where the line ends
    int top = 0;     ///< Its top
    int height = 0;  ///< Its height
    int icon = 0;    ///< The size of the icons of the items
};

/// @brief Paint the item of @p line with @p icon and @p text; in italics when it is
/// @p derived from the scenes of the text
void paintPlotItem(QPainter& painter, PlotLine& line, const char* icon, const QString& text,
                   bool derived) {
    if (text.isEmpty() || line.x >= line.right - line.icon) {
        return;
    }
    QFont font = line.font;
    if (derived) {
        font.setItalic(true);
    }
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const QPixmap pixmap =
        core::ArtProvider::getInstance().getPixmap(QString::fromLatin1(icon), line.icon);
    painter.drawPixmap(
        QRect(line.x, line.top + ((line.height - line.icon) / 2), line.icon, line.icon), pixmap);
    line.x += line.icon + ICON_GAP;
    const int room = line.right - line.x;
    const QString shown = metrics.elidedText(text, Qt::ElideRight, room);
    painter.drawText(QRect(line.x, line.top, room, line.height), Qt::AlignLeft | Qt::AlignVCenter,
                     shown);
    line.x += metrics.horizontalAdvance(shown) + ITEM_GAP;
}

}  // namespace

void PlotBoardView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPoint scrolled(horizontalScrollBar()->value(), verticalScrollBar()->value());
    painter.translate(-scrolled);
    const QRect shown(scrolled, viewport()->size());
    if (m_bands.isEmpty()) {
        paintEmptyBoard(painter, shown);
        return;
    }
    paintBands(painter, shown);
    if (m_drop.isValid() && m_drop.marker.isValid()) {
        painter.fillRect(m_drop.marker, palette().color(QPalette::Highlight));
    }
}

void PlotBoardView::updateBackground() {
    if (m_background == Background::Theme) {
        // All the colors of the board's, and so of the window
        viewport()->setPalette(QPalette());
        return;
    }
    QPalette under;
    if (m_background == Background::Cork) {
        under.setBrush(QPalette::Window, QBrush(corkTile(m_colors.cork)));
    } else {
        under.setColor(QPalette::Window, m_colors.background);
    }
    viewport()->setPalette(under);
}

QColor PlotBoardView::boardTextColor(bool quiet) const {
    const QPalette& colors = palette();
    if (m_background == Background::Theme) {
        return quiet ? quietColor(colors) : colors.color(QPalette::WindowText);
    }
    // Of the colors of the theme, the one that reads best on the background
    const QColor under =
        m_background == Background::Cork ? m_colors.cork : m_colors.background;
    QColor best = colors.color(QPalette::WindowText);
    if (core::contrastRatio(best, under) < READABLE_CONTRAST) {
        for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base}) {
            if (core::contrastRatio(colors.color(role), under) > core::contrastRatio(best, under)) {
                best = colors.color(role);
            }
        }
    }
    return best;
}

void PlotBoardView::paintEmptyBoard(QPainter& painter, const QRect& shown) const {
    painter.setPen(boardTextColor(true));
    //: The plot board of a book without texts in its main section (Body). In Polish: W
    //: tekście głównym książki nie ma jeszcze rozdziałów. Insert dodaje rozdział.
    painter.drawText(shown.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     tr("The main section of the book has no texts yet. Insert adds a "
                        "chapter."));
}

void PlotBoardView::paintBands(QPainter& painter, const QRect& shown) const {
    for (const SectionPlace& section : m_sectionPlaces) {
        paintSection(painter, section, shown);
    }
    for (const BandPlace& band : m_bandPlaces) {
        if (band.header.isValid() || band.area.intersects(shown)) {
            paintBandHeader(painter, band);
        }
    }
    for (const DrawerPlace& drawer : m_drawers) {
        if (drawer.rect.intersects(shown)) {
            paintDrawer(painter, drawer);
        }
    }
    for (const CardPlace& place : m_cards) {
        if (place.rect.intersects(shown)) {
            paintCard(painter, place);
        }
    }
}

void PlotBoardView::paintBandHeader(QPainter& painter, const BandPlace& place) const {
    const PlotBand& band = m_bands.at(place.band);
    if (band.partId.isEmpty()) {
        return;
    }
    if (place.header.isValid()) {
        paintPartTitle(painter, place.header, band);
    }
    if (band.cards.isEmpty()) {
        paintEmptyPart(painter, place.area);
    }
}

void PlotBoardView::paintPartTitle(QPainter& painter, const QRect& header,
                                   const PlotBand& band) const {
    // The title of the part and the words of its texts, over a line
    const QPalette& colors = palette();
    painter.setFont(smallFont(font()));
    //: The texts and the words of a part on the plot board; %1 the texts ("3 texts"), %2
    //: the words. In Polish: %1 · %2
    const QString count = tr("%1 · %2").arg(
        PlotBoard::textsText(static_cast<int>(band.cards.size())), PlotBoard::wordsText(band.words()));
    const int countWidth = painter.fontMetrics().horizontalAdvance(count);
    painter.setPen(boardTextColor(true));
    painter.drawText(header.adjusted(0, 0, -2, -LABEL_PADDING), Qt::AlignRight | Qt::AlignBottom,
                     count);
    painter.setFont(boldFont(font()));
    painter.setPen(boardTextColor(false));
    const QRect titleRect = header.adjusted(2, 0, -countWidth - HEADER_TITLE_GAP, -LABEL_PADDING);
    painter.drawText(
        titleRect, Qt::AlignLeft | Qt::AlignBottom,
        painter.fontMetrics().elidedText(band.title, Qt::ElideRight, titleRect.width()));
    painter.fillRect(QRect(header.left(), header.bottom() - 1, header.width(), HEADER_LINE),
                     colors.color(QPalette::Mid));
}

void PlotBoardView::paintEmptyPart(QPainter& painter, const QRect& area) const {
    // An empty part: the place a card can be dropped on
    const QPalette& colors = palette();
    const QRect empty = area.adjusted(1, 1, -1, -1);
    QPen pen(colors.color(QPalette::Mid));
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(empty, RADIUS, RADIUS);
    QFont italic = font();
    italic.setItalic(true);
    painter.setFont(italic);
    painter.setPen(boardTextColor(true));
    //: A part without texts on the plot board. In Polish: Pusta część
    painter.drawText(empty, Qt::AlignCenter | Qt::TextWordWrap, tr("Empty part"));
}

void PlotBoardView::paintDrawer(QPainter& painter, const DrawerPlace& drawer) const {
    const QPalette& colors = palette();
    painter.save();
    QPainterPath shape;
    shape.addRoundedRect(QRectF(drawer.rect), RADIUS, RADIUS);
    painter.fillPath(shape, colors.color(QPalette::AlternateBase));
    painter.fillRect(QRect(drawer.rect.left(), drawer.rect.top(), DRAWER_EDGE, drawer.rect.height()),
                     colors.color(QPalette::Highlight));
    const QFont small = smallFont(font());
    const QFontMetrics metrics(small);
    painter.setFont(small);
    painter.setPen(quietColor(colors));
    const QRect label(drawer.rect.left() + DRAWER_PADDING, drawer.rect.top() + (DRAWER_PADDING / 2),
                      drawer.rect.width() - (2 * DRAWER_PADDING), metrics.height() + LABEL_PADDING);
    //: Over the scenes of a text in its drawer on the plot board; %1 the title of the text. In
    //: Polish: Sceny – %1
    painter.drawText(label, Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(tr("Scenes – %1").arg(drawer.title), Qt::ElideRight,
                                        label.width()));
    painter.restore();
}

void PlotBoardView::paintSection(QPainter& painter, const SectionPlace& place,
                                 const QRect& shown) const {
    const PlotBand& section = sectionOf(place);
    paintPartTitle(painter, place.header, section);
    for (qsizetype i = 0; i < place.cards.size(); ++i) {
        if (place.cards.at(i).intersects(shown)) {
            paintSectionCard(painter, section.cards.at(i), place.cards.at(i));
        }
    }
}

void PlotBoardView::paintSectionCard(QPainter& painter, const PlotCard& card,
                                     const QRect& rect) const {
    const QPalette& colors = palette();
    painter.save();
    painter.setOpacity(SECTION_OPACITY);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect).adjusted(HALF_PIXEL, HALF_PIXEL, -HALF_PIXEL, -HALF_PIXEL),
                         RADIUS, RADIUS);
    painter.fillPath(shape, colors.color(QPalette::Base));
    QPen border(colors.color(QPalette::Mid));
    border.setStyle(Qt::DashLine);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
    const QRect inner = rect.adjusted(PADDING, PADDING + STRIP, -PADDING, -PADDING);
    const int titleTop = paintCardHead(painter, card, inner);
    const int descriptionTop = paintCardTitle(painter, card, inner, titleTop) + LINE_GAP;
    if (m_fields.description && !card.description.isEmpty()) {
        paintCardDescription(painter, card, inner, descriptionTop, false);
    }
    painter.restore();
}

void PlotBoardView::paintCard(QPainter& painter, const CardPlace& place) const {
    const PlotCard& card = cardOfPlace(place);
    const QRect inner = place.rect.adjusted(PADDING, PADDING + STRIP, -PADDING, -PADDING);
    painter.save();
    if (card.id == m_marked) {
        painter.setOpacity(MARKED_OPACITY);
    }
    const bool planned = isPlanBesideText(place);
    paintCardFrame(painter, card, place.rect, planned);
    const int titleTop = paintCardHead(painter, card, inner);
    const int descriptionTop = paintCardTitle(painter, card, inner, titleTop) + LINE_GAP;
    if (m_fields.description) {
        paintCardDescription(painter, card, inner, descriptionTop, planned);
    }
    // The scenes of a text, when they are not shown under it, at the bottom right
    const int right = paintScenesNote(painter, place, inner.right());
    if (m_fields.pov || m_fields.place || m_fields.time) {
        paintPlotLine(painter, card, inner, right);
    }
    painter.restore();
    if (card.id == m_current) {
        paintCurrentRing(painter, place.rect);
    }
}

void PlotBoardView::paintCardFrame(QPainter& painter, const PlotCard& card, const QRect& rect,
                                   bool planned) const {
    const QPalette& colors = palette();
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect).adjusted(HALF_PIXEL, HALF_PIXEL, -HALF_PIXEL, -HALF_PIXEL),
                         RADIUS, RADIUS);
    // The color of the card as its background, or in a strip at its top
    const QColor own = m_colors.ofCard(card, m_colorBy, m_povs);
    QColor base = colors.color(QPalette::Base);
    if (m_tint && own.isValid()) {
        base = core::mixedColor(base, own, TINT_SHARE);
    }
    // A scene in the plan is lighter than the scenes written
    if (planned) {
        base.setAlphaF(PLANNED_OPACITY);
    }
    painter.fillPath(shape, base);
    if (!m_tint && own.isValid()) {
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRect(rect.left(), rect.top(), rect.width(), STRIP), own);
        painter.restore();
    }
    QPen border(colors.color(QPalette::Mid));
    if (card.id == m_marked || planned) {
        border.setStyle(Qt::DashLine);
    }
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

int PlotBoardView::paintCardHead(QPainter& painter, const PlotCard& card,
                                 const QRect& inner) const {
    // The kind and the words; the state on the right (a scene in the plan has none yet: its
    // kind says it)
    const QFont small = smallFont(font());
    const QFontMetrics metrics(small);
    painter.setFont(small);
    const int stateWidth =
        m_fields.status && !card.planned ? paintCardState(painter, card, inner) : 0;
    QString kind = card.kindName;
    // A scene in the plan has no words yet
    if (m_fields.words && !card.planned) {
        const QString words = PlotBoard::wordsText(card.words);
        kind = kind.isEmpty() ? words : QStringLiteral("%1 · %2").arg(kind, words);
    }
    painter.setPen(quietColor(palette()));
    const int kindWidth = inner.width() - stateWidth - (stateWidth > 0 ? STATE_GAP : 0);
    painter.drawText(QRect(inner.left(), inner.top(), kindWidth, metrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(kind, Qt::ElideRight, kindWidth));
    return inner.top() + metrics.height() + LINE_GAP;
}

int PlotBoardView::paintCardState(QPainter& painter, const PlotCard& card,
                                  const QRect& inner) const {
    const QFontMetrics metrics(painter.font());
    const QString state = PlotBoard::statusName(card.status);
    const int dot = metrics.height() / 2;
    const int width = metrics.horizontalAdvance(state) + dot + DOT_GAP;
    const QRect rect(inner.right() - width + 1, inner.top(), width, metrics.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_colors.ofStatus(card.status));
    painter.drawEllipse(QRect(rect.left(), inner.top() + ((metrics.height() - dot) / 2), dot, dot));
    painter.setPen(quietColor(palette()));
    painter.drawText(rect.adjusted(dot + DOT_GAP, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     state);
    return width;
}

int PlotBoardView::paintCardTitle(QPainter& painter, const PlotCard& card, const QRect& inner,
                                  int top) const {
    const QFont bold = boldFont(font());
    painter.setFont(bold);
    painter.setPen(palette().color(QPalette::Text));
    const int height = drawWrapped(painter, QPoint(inner.left(), top), inner.width(), card.title,
                                   titleLines(m_cardSize));
    // A card without a title keeps the place of its line
    return top + std::max(height, QFontMetrics(bold).lineSpacing());
}

void PlotBoardView::paintCardDescription(QPainter& painter, const PlotCard& card,
                                         const QRect& inner, int top, bool planned) const {
    QFont text = font();
    if (!card.description.isEmpty()) {
        text.setItalic(planned);
        painter.setFont(text);
        painter.setPen(palette().color(QPalette::Text));
        drawWrapped(painter, QPoint(inner.left(), top), inner.width(), card.description,
                    descriptionLines(m_cardSize));
        return;
    }
    // Where the description goes
    if (m_readOnly) {
        return;
    }
    text.setItalic(true);
    painter.setFont(text);
    painter.setPen(quietColor(palette()));
    //: A card of the plot board without a description. In Polish: Bez opisu – F2
    painter.drawText(QPoint(inner.left(), top + QFontMetrics(text).ascent()),
                     tr("No description – F2"));
}

int PlotBoardView::paintScenesNote(QPainter& painter, const CardPlace& place, int right) const {
    const QString note = scenesNote(cardOfPlace(place));
    if (note.isEmpty()) {
        return right;
    }
    painter.setFont(smallFont(font()));
    painter.setPen(m_arrangement == Arrangement::Grid ? palette().color(QPalette::Link)
                                                      : quietColor(palette()));
    const QRect rect = scenesNoteRect(place);
    painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, note);
    return rect.left() - NOTE_GAP;
}

void PlotBoardView::paintPlotLine(QPainter& painter, const PlotCard& card, const QRect& inner,
                                  int right) const {
    // The point of view, the place and the time at the bottom; a text without its own point of
    // view has those of its scenes, in italics
    PlotLine line;
    line.font = smallFont(font());
    line.height = QFontMetrics(line.font).height();
    line.x = inner.left();
    line.right = right;
    line.top = inner.bottom() - line.height + 1;
    line.icon = std::max(MIN_ICON, line.height - ICON_INSET);
    painter.setPen(quietColor(palette()));
    const bool derived = card.plot.pov.isEmpty();
    if (m_fields.pov) {
        paintPlotItem(painter, line, "plotBoard.pov",
                      derived ? card.scenePovs().join(QStringLiteral(", ")) : card.plot.pov,
                      derived);
    }
    if (m_fields.place) {
        paintPlotItem(painter, line, "plotBoard.place", card.plot.place, false);
    }
    if (m_fields.time) {
        paintPlotItem(painter, line, "plotBoard.time", card.plot.time, false);
    }
}

void PlotBoardView::paintCurrentRing(QPainter& painter, const QRect& rect) const {
    QColor ring = palette().color(QPalette::Highlight);
    if (!hasFocus() && (m_editor == nullptr || !m_editor->isAncestorOf(focusWidget()))) {
        ring.setAlphaF(QUIET_RING_ALPHA);
    }
    painter.setPen(QPen(ring, RING_WIDTH));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect).adjusted(-1, -1, 1, 1), RADIUS + 1, RADIUS + 1);
}

}  // namespace kalahari::gui
