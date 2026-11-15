// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view.cpp
/// @brief The cards of the plot board: the texts of the book in the order of the book, in
/// columns of the parts or in rows, moved with the keys or the mouse

#include "kalahari/gui/widgets/plot_board_view.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QFocusEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextLayout>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::gui {

namespace {

constexpr int MARGIN = 12;       ///< Around the cards
constexpr int GAP = 10;          ///< Between two cards
constexpr int BAND_GAP = 22;     ///< Between two parts
constexpr int PADDING = 8;       ///< Inside a card
constexpr int STRIP = 4;         ///< The color of a card at its top
constexpr int LINE_GAP = 3;      ///< Between the lines of a card
constexpr int RADIUS = 4;        ///< Of the corners of a card
constexpr int SCROLL_STEP = 24;  ///< A step of the scroll bars

/// @brief The width of a card in average characters of the font
int widthInCharacters(PlotBoardView::CardSize size) {
    switch (size) {
    case PlotBoardView::CardSize::Small:
        return 22;
    case PlotBoardView::CardSize::Large:
        return 36;
    case PlotBoardView::CardSize::Medium:
        break;
    }
    return 28;
}

/// @brief The lines of the description on a card
int descriptionLines(PlotBoardView::CardSize size) {
    switch (size) {
    case PlotBoardView::CardSize::Small:
        return 2;
    case PlotBoardView::CardSize::Large:
        return 5;
    case PlotBoardView::CardSize::Medium:
        break;
    }
    return 3;
}

/// @brief The lines of the title on a card
int titleLines(PlotBoardView::CardSize size) {
    return size == PlotBoardView::CardSize::Large ? 2 : 1;
}

/// @brief The font of the small lines of a card: its kind and state, its plot
QFont smallFont(const QFont& font) {
    QFont small = font;
    small.setPointSizeF(font.pointSizeF() * 0.86);
    return small;
}

QFont boldFont(const QFont& font) {
    QFont bold = font;
    bold.setBold(true);
    return bold;
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
    int lines = 0;
    while (lines < maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        ++lines;
        const bool last = lines == maxLines;
        const int end = line.textStart() + line.textLength();
        const QString lineText = text.mid(line.textStart(), line.textLength());
        if (last && end < text.size()) {
            // The rest of the text does not fit: the last line ends in an ellipsis
            const QString rest = text.mid(line.textStart()).simplified();
            painter.drawText(QPoint(topLeft.x(), topLeft.y() + y + metrics.ascent()),
                             metrics.elidedText(rest, Qt::ElideRight, width));
        } else {
            painter.drawText(QPoint(topLeft.x(), topLeft.y() + y + metrics.ascent()),
                             lineText.trimmed());
        }
        y += metrics.lineSpacing();
    }
    layout.endLayout();
    return y;
}

/// @brief The color of the text that says less: the words, the plot
QColor quietColor(const QPalette& palette) {
    return palette.color(QPalette::PlaceholderText);
}

}  // namespace

PlotBoardView::PlotBoardView(QWidget* parent) : QAbstractScrollArea(parent) {
    setObjectName(QStringLiteral("plotBoardView"));
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    viewport()->setAcceptDrops(true);
    viewport()->setMouseTracking(false);
    horizontalScrollBar()->setSingleStep(SCROLL_STEP);
    verticalScrollBar()->setSingleStep(SCROLL_STEP);
    //: The cards of the texts of the book on the plot board, for the screen reader. In
    //: Polish: Tablica fabuły
    setAccessibleName(tr("Plot Board"));
    m_colors = PlotColors::current();
}

PlotBoardView::~PlotBoardView() = default;

void PlotBoardView::setBands(const QList<PlotBand>& bands) {
    // The card at the place of the one chosen, when the board no longer has it
    const QList<PlotCard> before = PlotBoard::cardsOf(m_bands);
    qsizetype at = -1;
    for (qsizetype i = 0; i < before.size(); ++i) {
        if (before.at(i).id == m_current) {
            at = i;
        }
    }
    m_bands = bands;
    const QList<PlotCard> cards = PlotBoard::cardsOf(m_bands);
    m_povs = povsOf(cards);
    relayout();
    if (m_editor != nullptr && placeOf(m_editor->elementId()) == nullptr) {
        closeEditor(false);
    }
    if (!m_marked.isEmpty() && placeOf(m_marked) == nullptr) {
        m_marked.clear();
    }
    if (!m_current.isEmpty() && placeOf(m_current) == nullptr) {
        const QString previous = m_current;
        m_current = cards.isEmpty() ? QString()
                                    : cards.at(std::clamp<qsizetype>(at, 0, cards.size() - 1)).id;
        if (m_current != previous) {
            emit currentCardChanged(m_current);
        }
    }
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setArrangement(Arrangement arrangement) {
    if (m_arrangement == arrangement) {
        return;
    }
    m_arrangement = arrangement;
    relayout();
    if (const CardPlace* place = placeOf(m_current)) {
        ensureVisible(place->rect);
    }
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setCardSize(CardSize size) {
    if (m_cardSize == size) {
        return;
    }
    m_cardSize = size;
    relayout();
    if (const CardPlace* place = placeOf(m_current)) {
        ensureVisible(place->rect);
    }
    placeEditor();
    viewport()->update();
}

void PlotBoardView::setFields(const Fields& fields) {
    m_fields = fields;
    relayout();
    viewport()->update();
}

void PlotBoardView::setColoring(const QString& colorBy, const PlotColors& colors) {
    m_colorBy = colorBy;
    m_colors = colors;
    viewport()->update();
}

void PlotBoardView::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    if (readOnly) {
        closeEditor(false);
        m_marked.clear();
    }
    viewport()->update();
}

void PlotBoardView::setMarkedCard(const QString& elementId) {
    m_marked = placeOf(elementId) != nullptr ? elementId : QString();
    viewport()->update();
}

bool PlotBoardView::setCurrentCard(const QString& elementId) {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return false;
    }
    choose(elementId);
    return true;
}

QRect PlotBoardView::cardRect(const QString& elementId) const {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return {};
    }
    return place->rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
}

int PlotBoardView::cardsPerRow() const {
    if (m_arrangement == Arrangement::Columns) {
        return std::max<int>(1, int(PlotBoard::cardsOf(m_bands).size()));
    }
    const int width = cardSizePixels().width();
    const int available = std::max(width, viewport()->width() - 2 * MARGIN);
    return std::max(1, (available + GAP) / (width + GAP));
}

bool PlotBoardView::editCard(const QString& elementId, const QStringList& povs,
                             const QStringList& places) {
    const PlotCard* card = cardOf(elementId);
    if (card == nullptr || m_readOnly) {
        return false;
    }
    closeEditor(false);
    choose(elementId);
    m_editor = new PlotCardEditor(*card, povs, places, viewport());
    connect(m_editor, &PlotCardEditor::accepted, this, [this]() { closeEditor(true); });
    connect(m_editor, &PlotCardEditor::rejected, this, [this]() { closeEditor(false); });
    placeEditor();
    m_editor->show();
    m_editor->raise();
    m_editor->focusTitle();
    return true;
}

QString PlotBoardView::spokenText(const QString& elementId) const {
    const PlotCard* card = cardOf(elementId);
    if (card == nullptr) {
        return {};
    }
    QStringList parts;
    parts << (card->kindName.isEmpty() ? card->title
                                       : QStringLiteral("%1: %2").arg(card->kindName, card->title));
    parts << PlotBoard::statusName(card->status) << PlotBoard::wordsText(card->words);
    if (!card->description.isEmpty()) {
        parts << card->description;
    }
    if (!card->plot.pov.isEmpty()) {
        //: Read by the screen reader on a card of the plot board. In Polish: Punkt widzenia: %1
        parts << tr("Point of view: %1").arg(card->plot.pov);
    }
    if (!card->plot.place.isEmpty()) {
        //: In Polish: Miejsce: %1
        parts << tr("Place: %1").arg(card->plot.place);
    }
    if (!card->plot.time.isEmpty()) {
        //: In Polish: Czas akcji: %1
        parts << tr("Time of the action: %1").arg(card->plot.time);
    }
    if (card->id == m_marked) {
        //: A card marked to move with Ctrl+X. In Polish: Wycięta – Ctrl+V wstawia ją za
        //: bieżącą kartą
        parts << tr("Cut – Ctrl+V puts it after the current card");
    }
    return parts.join(QStringLiteral(". "));
}

QSize PlotBoardView::sizeHint() const {
    return {640, 360};
}

// =============================================================================
// Layout
// =============================================================================

QSize PlotBoardView::cardSizePixels() const {
    const QFontMetrics metrics(font());
    const QFontMetrics small(smallFont(font()));
    const QFontMetrics bold(boldFont(font()));
    const int width = std::max(120, metrics.averageCharWidth() * widthInCharacters(m_cardSize));
    int height = 2 * PADDING + STRIP + small.height();
    height += LINE_GAP + bold.lineSpacing() * titleLines(m_cardSize);
    if (m_fields.description) {
        height += LINE_GAP + metrics.lineSpacing() * descriptionLines(m_cardSize);
    }
    if (m_fields.pov || m_fields.place || m_fields.time) {
        height += LINE_GAP + small.height();
    }
    return {width, height};
}

void PlotBoardView::relayout() {
    m_cards.clear();
    m_bandPlaces.clear();
    const QSize card = cardSizePixels();
    const int header = QFontMetrics(boldFont(font())).height() + 8;
    if (m_arrangement == Arrangement::Columns) {
        int x = MARGIN;
        const int top = MARGIN;
        const int cardsTop = top + header + 6;
        for (int b = 0; b < m_bands.size(); ++b) {
            const PlotBand& band = m_bands.at(b);
            const int count = int(band.cards.size());
            const int width = count > 0 ? count * card.width() + (count - 1) * GAP : card.width();
            BandPlace place;
            place.band = b;
            place.header = QRect(x, top, width, header);
            place.area = QRect(x, cardsTop, width, card.height());
            m_bandPlaces << place;
            for (int i = 0; i < count; ++i) {
                m_cards << CardPlace{band.cards.at(i).id, b, i,
                                     QRect(x + i * (card.width() + GAP), cardsTop, card.width(),
                                           card.height())};
            }
            x += width + BAND_GAP;
        }
        m_contentsSize = QSize(std::max(x - BAND_GAP + MARGIN, 0),
                               cardsTop + card.height() + MARGIN);
    } else {
        const int perRow = cardsPerRow();
        const int available = std::max(card.width(), viewport()->width() - 2 * MARGIN);
        int y = MARGIN;
        for (int b = 0; b < m_bands.size(); ++b) {
            const PlotBand& band = m_bands.at(b);
            BandPlace place;
            place.band = b;
            if (!band.partId.isEmpty()) {
                place.header = QRect(MARGIN, y, available, header);
                y += header + 6;
            }
            const int count = int(band.cards.size());
            const int rows = std::max(1, (count + perRow - 1) / perRow);
            for (int i = 0; i < count; ++i) {
                const int column = i % perRow;
                const int row = i / perRow;
                m_cards << CardPlace{band.cards.at(i).id, b, i,
                                     QRect(MARGIN + column * (card.width() + GAP),
                                           y + row * (card.height() + GAP), card.width(),
                                           card.height())};
            }
            const int height = rows * (card.height() + GAP) - GAP;
            place.area = QRect(MARGIN, y, count > 0 ? available : card.width(), height);
            m_bandPlaces << place;
            y += height + BAND_GAP;
        }
        m_contentsSize = QSize(available + 2 * MARGIN, std::max(y - BAND_GAP + MARGIN, 0));
    }
    updateScrollBars();
}

void PlotBoardView::updateScrollBars() {
    const QSize area = viewport()->size();
    horizontalScrollBar()->setRange(0, std::max(0, m_contentsSize.width() - area.width()));
    horizontalScrollBar()->setPageStep(area.width());
    verticalScrollBar()->setRange(0, std::max(0, m_contentsSize.height() - area.height()));
    verticalScrollBar()->setPageStep(area.height());
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
    return &m_bands.at(place->band).cards.at(place->index);
}

const PlotBoardView::CardPlace* PlotBoardView::cardAt(const QPoint& contentsPos) const {
    for (const CardPlace& place : m_cards) {
        if (place.rect.contains(contentsPos)) {
            return &place;
        }
    }
    return nullptr;
}

QPoint PlotBoardView::contentsPos(const QPoint& viewportPos) const {
    return viewportPos + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}

QString PlotBoardView::neighbour(Key key) const {
    if (m_cards.isEmpty()) {
        return {};
    }
    const CardPlace* current = placeOf(m_current);
    if (current == nullptr || key == Key::First) {
        return m_cards.constFirst().id;
    }
    if (key == Key::Last) {
        return m_cards.constLast().id;
    }
    const qsizetype at = current - m_cards.constData();
    if (key == Key::Left) {
        return at > 0 ? m_cards.at(at - 1).id : QString();
    }
    if (key == Key::Right) {
        return at + 1 < m_cards.size() ? m_cards.at(at + 1).id : QString();
    }
    // Up and Down: the nearest row above or below, the card nearest to the middle of this one
    const QRect from = current->rect;
    const CardPlace* best = nullptr;
    int bestRow = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (const CardPlace& place : m_cards) {
        const bool beyond = key == Key::Up ? place.rect.bottom() < from.top()
                                           : place.rect.top() > from.bottom();
        if (!beyond) {
            continue;
        }
        const int row = key == Key::Up ? -place.rect.bottom() : place.rect.top();
        const int distance = std::abs(place.rect.center().x() - from.center().x());
        if (best == nullptr || row < bestRow || (row == bestRow && distance < bestDistance)) {
            best = &place;
            bestRow = row;
            bestDistance = distance;
        }
    }
    return best != nullptr ? best->id : QString();
}

void PlotBoardView::choose(const QString& elementId) {
    const CardPlace* place = placeOf(elementId);
    if (place == nullptr) {
        return;
    }
    ensureVisible(place->rect);
    if (m_current != elementId) {
        m_current = elementId;
        emit currentCardChanged(m_current);
        tellScreenReader();
    }
    viewport()->update();
}

void PlotBoardView::ensureVisible(const QRect& rect) {
    const QRect shown(QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()),
                      viewport()->size());
    const QRect wanted = rect.adjusted(-MARGIN, -MARGIN, MARGIN, MARGIN);
    if (wanted.left() < shown.left()) {
        horizontalScrollBar()->setValue(wanted.left());
    } else if (wanted.right() > shown.right()) {
        horizontalScrollBar()->setValue(wanted.right() - shown.width() + 1);
    }
    if (wanted.top() < shown.top()) {
        verticalScrollBar()->setValue(wanted.top());
    } else if (wanted.bottom() > shown.bottom()) {
        verticalScrollBar()->setValue(wanted.bottom() - shown.height() + 1);
    }
}

void PlotBoardView::tellScreenReader() {
    setAccessibleDescription(spokenText(m_current));
    if (QAccessible::isActive()) {
        QAccessibleEvent event(this, QAccessible::DescriptionChanged);
        QAccessible::updateAccessibility(&event);
    }
}

// =============================================================================
// Painting
// =============================================================================

void PlotBoardView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
    const QRect shown(QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()),
                      viewport()->size());

    if (m_bands.isEmpty()) {
        painter.setPen(quietColor(palette()));
        //: The plot board of a book without texts in its main section (Body). In Polish: W
        //: tekście głównym książki nie ma jeszcze rozdziałów. Insert dodaje rozdział.
        painter.drawText(shown.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         tr("The main section of the book has no texts yet. Insert adds a "
                            "chapter."));
        return;
    }
    for (const BandPlace& band : m_bandPlaces) {
        if (band.header.isValid() || band.area.intersects(shown)) {
            paintBandHeader(painter, band);
        }
    }
    for (const CardPlace& place : m_cards) {
        if (place.rect.intersects(shown)) {
            paintCard(painter, place);
        }
    }
    if (m_drop.isValid() && m_drop.marker.isValid()) {
        painter.fillRect(m_drop.marker, palette().color(QPalette::Highlight));
    }
}

void PlotBoardView::paintBandHeader(QPainter& painter, const BandPlace& place) const {
    const PlotBand& band = m_bands.at(place.band);
    if (band.partId.isEmpty()) {
        return;
    }
    const QPalette& colors = palette();
    // The title of the part and the words of its texts, over a line
    if (place.header.isValid()) {
        painter.setFont(smallFont(font()));
        //: The texts and the words of a part on the plot board; %1 the texts ("3 texts"), %2
        //: the words. In Polish: %1 · %2
        const QString count =
            tr("%1 · %2").arg(PlotBoard::textsText(int(band.cards.size())),
                             PlotBoard::wordsText(band.words()));
        const int countWidth = painter.fontMetrics().horizontalAdvance(count);
        painter.setPen(quietColor(colors));
        painter.drawText(place.header.adjusted(0, 0, -2, -4), Qt::AlignRight | Qt::AlignBottom,
                         count);
        painter.setFont(boldFont(font()));
        painter.setPen(colors.color(QPalette::WindowText));
        const QRect titleRect = place.header.adjusted(2, 0, -countWidth - 12, -4);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignBottom,
                         painter.fontMetrics().elidedText(band.title, Qt::ElideRight,
                                                          titleRect.width()));
        painter.fillRect(QRect(place.header.left(), place.header.bottom() - 1,
                               place.header.width(), 2),
                         colors.color(QPalette::Mid));
    }
    if (band.cards.isEmpty()) {
        // An empty part: the place a card can be dropped on
        const QRect empty = place.area.adjusted(1, 1, -1, -1);
        QPen pen(colors.color(QPalette::Mid));
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(empty, RADIUS, RADIUS);
        QFont italic = font();
        italic.setItalic(true);
        painter.setFont(italic);
        painter.setPen(quietColor(colors));
        //: A part without texts on the plot board. In Polish: Pusta część
        painter.drawText(empty, Qt::AlignCenter | Qt::TextWordWrap, tr("Empty part"));
    }
}

void PlotBoardView::paintCard(QPainter& painter, const CardPlace& place) const {
    const PlotCard& card = m_bands.at(place.band).cards.at(place.index);
    const QPalette& colors = palette();
    const QRect rect = place.rect;
    const bool current = card.id == m_current;
    const bool marked = card.id == m_marked;

    painter.save();
    if (marked) {
        painter.setOpacity(0.55);
    }
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), RADIUS, RADIUS);
    painter.fillPath(shape, colors.color(QPalette::Base));
    const QColor own = m_colors.ofCard(card, m_colorBy, m_povs);
    if (own.isValid()) {
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRect(rect.left(), rect.top(), rect.width(), STRIP), own);
        painter.restore();
    }
    QPen border(colors.color(QPalette::Mid));
    if (marked) {
        border.setStyle(Qt::DashLine);
    }
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    const QRect inner = rect.adjusted(PADDING, PADDING + STRIP, -PADDING, -PADDING);
    int y = inner.top();

    // The kind and the words; the state on the right
    const QFont small = smallFont(font());
    painter.setFont(small);
    const QFontMetrics smallMetrics(small);
    int stateWidth = 0;
    if (m_fields.status) {
        const QString state = PlotBoard::statusName(card.status);
        const int dot = smallMetrics.height() / 2;
        stateWidth = smallMetrics.horizontalAdvance(state) + dot + 4;
        const QRect stateRect(inner.right() - stateWidth + 1, y, stateWidth, smallMetrics.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_colors.ofStatus(card.status));
        const int dotTop = y + (smallMetrics.height() - dot) / 2;
        painter.drawEllipse(QRect(stateRect.left(), dotTop, dot, dot));
        painter.setPen(quietColor(colors));
        painter.drawText(stateRect.adjusted(dot + 4, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                         state);
    }
    QString kind = card.kindName;
    if (m_fields.words) {
        const QString words = PlotBoard::wordsText(card.words);
        kind = kind.isEmpty() ? words : QStringLiteral("%1 · %2").arg(kind, words);
    }
    painter.setPen(quietColor(colors));
    const int kindWidth = inner.width() - stateWidth - (stateWidth > 0 ? 6 : 0);
    painter.drawText(QRect(inner.left(), y, kindWidth, smallMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     smallMetrics.elidedText(kind, Qt::ElideRight, kindWidth));
    y += smallMetrics.height() + LINE_GAP;

    // The title
    painter.setFont(boldFont(font()));
    painter.setPen(colors.color(QPalette::Text));
    y += drawWrapped(painter, QPoint(inner.left(), y), inner.width(), card.title,
                     titleLines(m_cardSize));
    y = std::max(y, inner.top() + smallMetrics.height() + LINE_GAP +
                        QFontMetrics(boldFont(font())).lineSpacing());

    // The description, or where it goes
    if (m_fields.description) {
        y += LINE_GAP;
        QFont text = font();
        if (card.description.isEmpty()) {
            text.setItalic(true);
            painter.setFont(text);
            painter.setPen(quietColor(colors));
            if (!m_readOnly) {
                //: A card of the plot board without a description. In Polish: Bez opisu – F2
                painter.drawText(QPoint(inner.left(), y + QFontMetrics(text).ascent()),
                                 tr("No description – F2"));
            }
        } else {
            painter.setFont(text);
            painter.setPen(colors.color(QPalette::Text));
            drawWrapped(painter, QPoint(inner.left(), y), inner.width(), card.description,
                        descriptionLines(m_cardSize));
        }
    }

    // The point of view, the place and the time at the bottom
    if (m_fields.pov || m_fields.place || m_fields.time) {
        painter.setFont(small);
        painter.setPen(quietColor(colors));
        const int lineTop = inner.bottom() - smallMetrics.height() + 1;
        const int iconSize = std::max(8, smallMetrics.height() - 3);
        int x = inner.left();
        const auto item = [&](bool shown, const char* icon, const QString& text) {
            if (!shown || text.isEmpty() || x >= inner.right() - iconSize) {
                return;
            }
            const QPixmap pixmap =
                core::ArtProvider::getInstance().getPixmap(QString::fromLatin1(icon), iconSize);
            painter.drawPixmap(QRect(x, lineTop + (smallMetrics.height() - iconSize) / 2,
                                     iconSize, iconSize),
                               pixmap);
            x += iconSize + 3;
            const int room = inner.right() - x;
            const QString shownText = smallMetrics.elidedText(text, Qt::ElideRight, room);
            painter.drawText(QRect(x, lineTop, room, smallMetrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, shownText);
            x += smallMetrics.horizontalAdvance(shownText) + 10;
        };
        item(m_fields.pov, "plotBoard.pov", card.plot.pov);
        item(m_fields.place, "plotBoard.place", card.plot.place);
        item(m_fields.time, "plotBoard.time", card.plot.time);
    }
    painter.restore();

    // The card chosen
    if (current) {
        QColor ring = colors.color(QPalette::Highlight);
        if (!hasFocus() && (m_editor == nullptr || !m_editor->isAncestorOf(focusWidget()))) {
            ring.setAlphaF(0.5F);
        }
        QPen pen(ring, 2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect).adjusted(-1, -1, 1, 1), RADIUS + 1, RADIUS + 1);
    }
}

// =============================================================================
// Events
// =============================================================================

bool PlotBoardView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride &&
        keyOf(static_cast<QKeyEvent*>(event)) != Key::None) {
        // The keys of the board are its own, not the window's commands
        event->accept();
        return true;
    }
    return QAbstractScrollArea::event(event);
}

bool PlotBoardView::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const CardPlace* place = cardAt(contentsPos(help->pos()));
        if (place == nullptr) {
            QToolTip::hideText();
            event->ignore();
            return true;
        }
        const PlotCard& card = m_bands.at(place->band).cards.at(place->index);
        QStringList lines{QStringLiteral("<b>%1</b>").arg(card.title.toHtmlEscaped())};
        if (!card.description.isEmpty()) {
            lines << card.description.toHtmlEscaped();
        }
        QStringList plot;
        if (!card.plot.pov.isEmpty()) {
            plot << tr("Point of view: %1").arg(card.plot.pov.toHtmlEscaped());
        }
        if (!card.plot.place.isEmpty()) {
            plot << tr("Place: %1").arg(card.plot.place.toHtmlEscaped());
        }
        if (!card.plot.time.isEmpty()) {
            plot << tr("Time of the action: %1").arg(card.plot.time.toHtmlEscaped());
        }
        if (!plot.isEmpty()) {
            lines << plot.join(QStringLiteral("<br>"));
        }
        QToolTip::showText(help->globalPos(),
                           QStringLiteral("<p style='white-space:pre-wrap'>%1</p>")
                               .arg(lines.join(QStringLiteral("<br>"))),
                           viewport(), cardRect(place->id));
        return true;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void PlotBoardView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    relayout();
    placeEditor();
}

void PlotBoardView::scrollContentsBy(int /*dx*/, int /*dy*/) {
    placeEditor();
    viewport()->update();
}

void PlotBoardView::changeEvent(QEvent* event) {
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        relayout();
        placeEditor();
        viewport()->update();
    } else if (event->type() == QEvent::PaletteChange) {
        viewport()->update();
    }
}

void PlotBoardView::focusInEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusInEvent(event);
    if (placeOf(m_current) == nullptr && !m_cards.isEmpty()) {
        choose(m_cards.constFirst().id);
    }
    tellScreenReader();
    viewport()->update();
}

void PlotBoardView::focusOutEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

PlotBoardView::Key PlotBoardView::keyOf(const QKeyEvent* event) const {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool plain = modifiers == Qt::NoModifier;
    const bool control = modifiers == Qt::ControlModifier;
    switch (event->key()) {
    case Qt::Key_Left:
        return plain ? Key::Left : control ? Key::MoveBack : Key::None;
    case Qt::Key_Right:
        return plain ? Key::Right : control ? Key::MoveForward : Key::None;
    case Qt::Key_Up:
        return plain ? Key::Up : control ? Key::MoveUp : Key::None;
    case Qt::Key_Down:
        return plain ? Key::Down : control ? Key::MoveDown : Key::None;
    case Qt::Key_Home:
        return plain ? Key::First : Key::None;
    case Qt::Key_End:
        return plain ? Key::Last : Key::None;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return plain ? Key::Open : modifiers == Qt::AltModifier ? Key::Properties : Key::None;
    case Qt::Key_F2:
        return plain ? Key::Edit : Key::None;
    case Qt::Key_Insert:
        return plain ? Key::Add : Key::None;
    case Qt::Key_Delete:
        return plain ? Key::Delete : Key::None;
    case Qt::Key_X:
        return control ? Key::Cut : Key::None;
    case Qt::Key_V:
        return control ? Key::Paste : Key::None;
    case Qt::Key_Z:
        return control                                                ? Key::Undo
               : modifiers == (Qt::ControlModifier | Qt::ShiftModifier) ? Key::Redo
                                                                        : Key::None;
    case Qt::Key_Y:
        return control ? Key::Redo : Key::None;
    case Qt::Key_Escape:
        return plain && !m_marked.isEmpty() ? Key::CancelCut : Key::None;
    case Qt::Key_Menu:
        return plain ? Key::Menu : Key::None;
    case Qt::Key_F10:
        return modifiers == Qt::ShiftModifier ? Key::Menu : Key::None;
    default:
        break;
    }
    return Key::None;
}

void PlotBoardView::keyPressEvent(QKeyEvent* event) {
    const Key key = keyOf(event);
    if (key == Key::None) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
    doKey(key);
}

void PlotBoardView::doKey(Key key) {
    const bool changing = !m_readOnly && !m_current.isEmpty();
    switch (key) {
    case Key::Left:
    case Key::Right:
    case Key::Up:
    case Key::Down:
    case Key::First:
    case Key::Last: {
        const QString target = neighbour(key);
        if (!target.isEmpty()) {
            choose(target);
        }
        break;
    }
    case Key::MoveBack:
    case Key::MoveForward:
        if (changing) {
            emit moveRequested(m_current, key == Key::MoveBack ? -1 : 1);
        }
        break;
    case Key::MoveUp:
    case Key::MoveDown:
        // In a grid by a row; in columns the cards of a text are under it (its scenes)
        if (changing && m_arrangement == Arrangement::Grid) {
            emit moveRequested(m_current, key == Key::MoveUp ? -cardsPerRow() : cardsPerRow());
        }
        break;
    case Key::Open:
        if (!m_current.isEmpty()) {
            emit openRequested(m_current);
        }
        break;
    case Key::Edit:
        if (changing) {
            emit editRequested(m_current);
        }
        break;
    case Key::Add:
        if (!m_readOnly) {
            emit addRequested(m_current);
        }
        break;
    case Key::Delete:
        if (changing) {
            emit deleteRequested(m_current);
        }
        break;
    case Key::Cut:
        if (changing) {
            emit cutRequested(m_current);
        }
        break;
    case Key::Paste:
        if (!m_readOnly && !m_marked.isEmpty()) {
            emit pasteRequested(m_current);
        }
        break;
    case Key::CancelCut:
        emit cutCancelled();
        break;
    case Key::Undo:
        if (!m_readOnly) {
            emit undoRequested();
        }
        break;
    case Key::Redo:
        if (!m_readOnly) {
            emit redoRequested();
        }
        break;
    case Key::Properties:
        if (!m_current.isEmpty()) {
            emit propertiesRequested(m_current);
        }
        break;
    case Key::Menu: {
        const QRect rect = cardRect(m_current);
        const QPoint at = rect.isValid() ? rect.center() : viewport()->rect().center();
        emit menuRequested(m_current, viewport()->mapToGlobal(at));
        break;
    }
    case Key::None:
        break;
    }
}

void PlotBoardView::mousePressEvent(QMouseEvent* event) {
    if (m_editor != nullptr) {
        // A click beside the card edited keeps what was written
        closeEditor(true);
    }
    setFocus(Qt::MouseFocusReason);
    const CardPlace* place = cardAt(contentsPos(event->position().toPoint()));
    if (place != nullptr) {
        choose(place->id);
    }
    if (event->button() == Qt::LeftButton && place != nullptr) {
        m_pressed = place->id;
        m_pressPos = event->position().toPoint();
    }
    event->accept();
}

void PlotBoardView::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) == 0 || m_pressed.isEmpty() || m_readOnly) {
        return;
    }
    if ((event->position().toPoint() - m_pressPos).manhattanLength() <
        QApplication::startDragDistance()) {
        return;
    }
    startDrag();
}

void PlotBoardView::mouseReleaseEvent(QMouseEvent* event) {
    m_pressed.clear();
    event->accept();
}

void PlotBoardView::mouseDoubleClickEvent(QMouseEvent* event) {
    const CardPlace* place = cardAt(contentsPos(event->position().toPoint()));
    if (place != nullptr && event->button() == Qt::LeftButton) {
        emit openRequested(place->id);
    }
    event->accept();
}

void PlotBoardView::contextMenuEvent(QContextMenuEvent* event) {
    if (event->reason() == QContextMenuEvent::Keyboard) {
        doKey(Key::Menu);
        event->accept();
        return;
    }
    const CardPlace* place = cardAt(contentsPos(event->pos()));
    if (place != nullptr) {
        choose(place->id);
    }
    emit menuRequested(place != nullptr ? place->id : QString(), event->globalPos());
    event->accept();
}

void PlotBoardView::startDrag() {
    const QString dragged = m_pressed;
    m_pressed.clear();
    const QRect rect = cardRect(dragged);
    auto* mime = new QMimeData;
    mime->setData(QString::fromLatin1(CARD_MIME_TYPE), dragged.toUtf8());
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    if (rect.isValid()) {
        drag->setPixmap(viewport()->grab(rect));
        drag->setHotSpot(m_pressPos - rect.topLeft());
    }
    drag->exec(Qt::MoveAction);
    m_drop = {};
    viewport()->update();
}

PlotBoardView::DropTarget PlotBoardView::dropTargetAt(const QPoint& pos,
                                                      const QString& dragged) const {
    DropTarget target;
    // Before or after the card under the mouse, or the nearest card of its row
    for (const CardPlace& place : m_cards) {
        const QRect near = place.rect.adjusted(-GAP / 2 - 1, -GAP / 2, GAP / 2 + 1, GAP / 2);
        if (!near.contains(pos)) {
            continue;
        }
        if (place.id == dragged) {
            return target;
        }
        target.anchorId = place.id;
        target.after = pos.x() > place.rect.center().x();
        const int x = target.after ? place.rect.right() + GAP / 2 : place.rect.left() - GAP / 2;
        target.marker = QRect(x - 1, place.rect.top(), 3, place.rect.height());
        return target;
    }
    for (const BandPlace& band : m_bandPlaces) {
        const PlotBand& shown = m_bands.at(band.band);
        // The title of a part, or an empty part: at its start
        if (!shown.partId.isEmpty() &&
            (band.header.contains(pos) || (shown.cards.isEmpty() && band.area.contains(pos)))) {
            target.partId = shown.partId;
            target.marker = QRect(band.area.left(), band.area.top() - 4, band.area.width(), 3);
            return target;
        }
        // Beyond the last card of a band: after it
        if (band.area.adjusted(0, 0, GAP, 0).contains(pos) && !shown.cards.isEmpty()) {
            const QString last = shown.cards.constLast().id;
            if (last == dragged) {
                return target;
            }
            const CardPlace* place = placeOf(last);
            target.anchorId = last;
            target.after = true;
            target.marker = QRect(place->rect.right() + GAP / 2 - 1, place->rect.top(), 3,
                                  place->rect.height());
            return target;
        }
    }
    return target;
}

void PlotBoardView::dragEnterEvent(QDragEnterEvent* event) {
    if (!m_readOnly && event->source() == this &&
        event->mimeData()->hasFormat(QString::fromLatin1(CARD_MIME_TYPE))) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void PlotBoardView::dragMoveEvent(QDragMoveEvent* event) {
    const QString dragged =
        QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(CARD_MIME_TYPE)));
    const QPoint pos = event->position().toPoint();
    m_drop = dropTargetAt(contentsPos(pos), dragged);
    // Near the edges the board scrolls
    if (pos.x() < MARGIN * 2) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - SCROLL_STEP);
    } else if (pos.x() > viewport()->width() - MARGIN * 2) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + SCROLL_STEP);
    }
    if (pos.y() < MARGIN * 2) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - SCROLL_STEP);
    } else if (pos.y() > viewport()->height() - MARGIN * 2) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + SCROLL_STEP);
    }
    if (m_drop.isValid()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    viewport()->update();
}

void PlotBoardView::dragLeaveEvent(QDragLeaveEvent* event) {
    m_drop = {};
    viewport()->update();
    event->accept();
}

void PlotBoardView::dropEvent(QDropEvent* event) {
    const QString dragged =
        QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(CARD_MIME_TYPE)));
    const DropTarget target = dropTargetAt(contentsPos(event->position().toPoint()), dragged);
    m_drop = {};
    viewport()->update();
    if (!target.isValid() || dragged.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    if (!target.anchorId.isEmpty()) {
        emit dropBesideRequested(dragged, target.anchorId, target.after);
    } else {
        emit dropInPartRequested(dragged, target.partId);
    }
}

// =============================================================================
// The editor of a card
// =============================================================================

void PlotBoardView::closeEditor(bool save) {
    if (m_editor == nullptr) {
        return;
    }
    PlotCardEditor* editor = m_editor;
    m_editor = nullptr;
    const QString elementId = editor->elementId();
    const PlotCardEdit edit = editor->edit();
    const bool hadKeys = editor->isAncestorOf(QApplication::focusWidget());
    editor->hide();
    editor->deleteLater();
    if (hadKeys) {
        setFocus(Qt::OtherFocusReason);
    }
    viewport()->update();
    if (save) {
        emit editFinished(elementId, edit);
    }
}

void PlotBoardView::placeEditor() {
    if (m_editor == nullptr) {
        return;
    }
    const QRect rect = cardRect(m_editor->elementId());
    if (!rect.isValid()) {
        return;
    }
    const QSize hint = m_editor->sizeHint();
    const int width = std::max(rect.width(), hint.width());
    const int height = hint.height();
    const QRect area = viewport()->rect();
    int x = std::min(rect.left(), area.right() - width);
    x = std::max(x, area.left());
    int y = std::min(rect.top(), area.bottom() - height);
    y = std::max(y, area.top());
    m_editor->setGeometry(x, y, width, height);
}

}  // namespace kalahari::gui
