/// @file annotation_cards.cpp
/// @brief The cards of the annotations under their paragraphs (the continuous views)

#include "kalahari/editor/annotation_cards.h"
#include "kalahari/gui/panels/annotation_colors.h"

#include <QDate>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QList>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

// Lengths in ems of the cards' font, as in the mockup approved on 26.10.2026 (the text of a
// card 14.4 pixels high under text of 16)
constexpr qreal INDENT = 1.4;        ///< The cards start this far right of the text's edge
constexpr qreal TOP_GAP = 0.3;       ///< Above the first card, below the paragraph's spacing
constexpr qreal CARD_GAP = 0.4;      ///< Between two cards
constexpr qreal BOTTOM_GAP = 0.75;   ///< Under the last card
constexpr qreal MIN_WIDTH = 8.0;     ///< The narrowest card
constexpr qreal LINE_HEIGHT = 1.42;  ///< A line of a card's text
constexpr qreal PAD_TOP = 0.5;       ///< Inside a card: above the header...
constexpr qreal PAD_BOTTOM = 0.55;   ///< ...below the text...
constexpr qreal PAD_RIGHT = 0.7;     ///< ...right of the text...
constexpr qreal PAD_LEFT = 0.97;     ///< ...and left of it, from the card's edge (the bar within)
constexpr qreal COMPACT_PAD = 0.42;  ///< Above and below the text of a compact card
constexpr qreal BAR = 0.28;          ///< The bar in the kind's color on the left
constexpr qreal RADIUS = 0.55;       ///< The rounded corners
constexpr qreal SMALL = 0.88;        ///< The header, the author and date of a compact card
constexpr qreal HEADER = 1.4;        ///< The header's height at least
constexpr qreal GAP = 0.5;           ///< Between the parts of a header or a line
constexpr qreal ICON = 1.05;         ///< The kind's icon
constexpr qreal CHECK = 0.97;        ///< The box of a to-do
constexpr qreal TEXT_GAP = 0.14;     ///< Between the header and the text, the text and "Show all"
constexpr qreal MENU_WIDTH = 1.55;   ///< The "..." button
constexpr qreal MENU_HEIGHT = 1.4;
constexpr qreal MENU_EDGE = 0.35;    ///< From the card's right edge
constexpr qreal LINE_FONT = 0.92;    ///< A collapsed one's line
constexpr qreal LINE_PAD = 0.21;     ///< Above and below the line
constexpr qreal LINK_FONT = 0.86;    ///< "Show all"
constexpr qreal OUTLINE = 0.14;      ///< The outline of the card gone to...
constexpr qreal OUTLINE_GAP = 0.07;  ///< ...this far outside it
constexpr qreal SHADOW = 0.07;       ///< The shadow under a card

/// @brief A collapsed one's line has less of the kind's color than a card
constexpr double LINE_TINT = 0.6;

/// @brief The shadow under a card: the paper this much darker (QColor::darker())
constexpr int SHADOW_DARKER = 112;

/// @brief How much of the kind's color the "..." button under the mouse has
constexpr double MENU_HOVER = 0.2;

/// @brief A font of another size
QFont scaledFont(const QFont& font, qreal factor) {
    QFont scaled = font;
    if (font.pointSizeF() > 0) {
        scaled.setPointSizeF(font.pointSizeF() * factor);
    } else if (font.pixelSize() > 0) {
        scaled.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * factor))));
    }
    return scaled;
}

/// @brief The fonts and the em of the cards
struct Metrics {
    QFont text;      ///< A card's text
    QFont small;     ///< The header, the author and the date of a compact card
    QFont kindName;  ///< The kind's name: small and bold
    QFont line;      ///< A collapsed one's line
    QFont lineName;  ///< The kind's name on it: bold
    QFont link;      ///< "Show all"
    qreal em = 1.0;  ///< The text's size: every length is in ems of it

    qreal lineHeight() const { return LINE_HEIGHT * em; }
    qreal headerHeight() const {
        return std::max(HEADER * em, QFontMetricsF(small).height());
    }
    qreal lineCardHeight() const { return (2.0 * LINE_PAD + LINE_FONT * LINE_HEIGHT) * em; }
    qreal linkHeight() const { return LINK_FONT * LINE_HEIGHT * em; }
};

Metrics metricsOf(const QFont& cardFont) {
    Metrics m;
    m.text = cardFont;
    m.em = std::max<qreal>(1.0, QFontInfo(cardFont).pixelSize());
    m.small = scaledFont(cardFont, SMALL);
    m.kindName = m.small;
    m.kindName.setBold(true);
    m.line = scaledFont(cardFont, LINE_FONT);
    m.lineName = m.line;
    m.lineName.setBold(true);
    m.link = scaledFont(cardFont, LINK_FONT);
    m.link.setUnderline(true);
    return m;
}

/// @brief A card's text as a line layout breaks it: its own lines kept
QString layoutText(const QString& text) {
    QString result = text;
    result.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return result;
}

/// @brief Break a text into lines of a width, one under another at the cards' line height
/// @return The number of lines
int breakLines(QTextLayout& layout, const QString& text, const QFont& font, qreal width,
               qreal lineHeight, const QList<QTextLayout::FormatRange>& formats = {}) {
    layout.setText(text);
    layout.setFont(font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.setFormats(formats);
    layout.beginLayout();
    int count = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(std::max<qreal>(1.0, width));
        // In the middle of its line box
        line.setPosition(QPointF(0.0, count * lineHeight + (lineHeight - line.height()) / 2.0));
        ++count;
    }
    layout.endLayout();
    return count;
}

/// @brief The day an annotation was made, short ("24.10", with the year when it is not this
///        one), as on the cards of the panel
QString shortDate(const QDateTime& created) {
    if (!created.isValid()) {
        return {};
    }
    const QDate date = created.toLocalTime().date();
    const bool thisYear = date.year() == QDate::currentDate().year();
    return QLocale().toString(date, thisYear ? QStringLiteral("dd.MM")
                                             : QStringLiteral("dd.MM.yyyy"));
}

/// @brief When an annotation was made, with the time
QString longDate(const QDateTime& created) {
    return created.isValid() ? QLocale().toString(created.toLocalTime(), QLocale::ShortFormat)
                             : QString();
}

/// @brief A comment's icon: a speech bubble with lines of text
void paintCommentIcon(QPainter* painter, const QRectF& box, const QColor& color) {
    const qreal s = box.width();
    QPainterPath bubble;
    const QRectF body(box.left() + 0.1 * s, box.top() + 0.12 * s, 0.8 * s, 0.62 * s);
    bubble.addRoundedRect(body, 0.1 * s, 0.1 * s);
    QPainterPath tail;
    tail.moveTo(body.right() - 0.28 * s, body.bottom() - 0.02 * s);
    tail.lineTo(body.right(), body.bottom() + 0.2 * s);
    tail.lineTo(body.right() - 0.02 * s, body.bottom() - 0.02 * s);
    tail.closeSubpath();
    bubble = bubble.united(tail);
    QColor fill = color;
    fill.setAlphaF(0.3);
    painter->fillPath(bubble, fill);
    painter->setPen(QPen(color, 0.09 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(bubble);
    for (int i = 0; i < 3; ++i) {
        const qreal y = body.top() + (0.2 + 0.17 * i) * s;
        painter->drawLine(QPointF(body.left() + 0.15 * s, y), QPointF(body.right() - 0.15 * s, y));
    }
}

/// @brief A note's icon: a sheet with a folded corner and lines of text
void paintNoteIcon(QPainter* painter, const QRectF& box, const QColor& color) {
    const qreal s = box.width();
    const QRectF sheet(box.left() + 0.14 * s, box.top() + 0.14 * s, 0.72 * s, 0.72 * s);
    const qreal fold = 0.26 * s;
    QPainterPath path;
    path.moveTo(sheet.topLeft());
    path.lineTo(sheet.topRight());
    path.lineTo(sheet.right(), sheet.bottom() - fold);
    path.lineTo(sheet.right() - fold, sheet.bottom());
    path.lineTo(sheet.bottomLeft());
    path.closeSubpath();
    QColor fill = color;
    fill.setAlphaF(0.3);
    painter->fillPath(path, fill);
    painter->setPen(QPen(color, 0.09 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
    painter->drawLine(QPointF(sheet.right(), sheet.bottom() - fold),
                      QPointF(sheet.right() - fold, sheet.bottom() - fold));
    painter->drawLine(QPointF(sheet.right() - fold, sheet.bottom() - fold),
                      QPointF(sheet.right() - fold, sheet.bottom()));
    for (int i = 0; i < 2; ++i) {
        const qreal y = sheet.top() + (0.22 + 0.18 * i) * s;
        painter->drawLine(QPointF(sheet.left() + 0.14 * s, y),
                          QPointF(sheet.right() - 0.14 * s, y));
    }
}

/// @brief A to-do's box, not checked (a to-do done has no card)
void paintCheckBox(QPainter* painter, const QRectF& box, const QColor& color) {
    const qreal s = box.width();
    painter->setPen(QPen(color, 0.12 * s));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(box.adjusted(0.06 * s, 0.06 * s, -0.06 * s, -0.06 * s), 0.2 * s,
                             0.2 * s);
}

/// @brief The "..." of the menu button
void paintMoreDots(QPainter* painter, const QRectF& button, const QColor& color, qreal em) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    const qreal radius = 0.11 * em;
    const qreal step = 0.36 * em;
    const QPointF center = button.center();
    for (int i = -1; i <= 1; ++i) {
        painter->drawEllipse(QPointF(center.x() + i * step, center.y()), radius, radius);
    }
}

/// @brief Where the parts a click finds are on a card
struct CardParts {
    QRectF icon;     ///< The kind's icon, or a to-do's box
    QRectF menu;     ///< The "..." button
    QRectF text;     ///< The text (its first line at the top)
    QRectF showAll;  ///< "Show all" (empty: the text is whole)
};

}  // namespace

// =============================================================================
// What is shown
// =============================================================================

void AnnotationCards::setSettings(const AnnotationCardSettings& settings) {
    m_settings = settings;
}

bool AnnotationCards::hasCard(AnnotationKind kind) const {
    if (!m_shown) {
        return false;
    }
    switch (kind) {
    case AnnotationKind::Comment:
        return m_settings.comments;
    case AnnotationKind::Todo:
        return m_settings.todos;
    case AnnotationKind::Note:
        return m_settings.notes;
    }
    return false;
}

void AnnotationCards::startText() {
    m_collapsedByDefault = m_settings.startCollapsed;
    m_toggled.clear();
    m_whole.clear();
    m_writing.reset();
    m_hovered.clear();
    m_hoveredPart.reset();
    m_selected.clear();
}

bool AnnotationCards::isCollapsed(const QString& id) const {
    return m_collapsedByDefault != m_toggled.contains(id);
}

void AnnotationCards::setCollapsed(const QString& id, bool collapsed) {
    if (collapsed == m_collapsedByDefault) {
        m_toggled.remove(id);
    } else {
        m_toggled.insert(id);
    }
}

void AnnotationCards::setAllCollapsed(bool collapsed) {
    m_collapsedByDefault = collapsed;
    m_toggled.clear();
}

void AnnotationCards::showWhole(const QString& id) {
    m_whole.insert(id);
}

bool AnnotationCards::cutsShort(const QString& id) const {
    return m_settings.longLines > 0 && !m_whole.contains(id);
}

// =============================================================================
// Looks
// =============================================================================

QFont AnnotationCards::cardFont() const {
    // The program's font, sized after the text
    QFont font = QGuiApplication::font();
    const qreal share = std::clamp(m_settings.fontPercent, 50, 200) / 100.0;
    if (m_textFont.pointSizeF() > 0) {
        font.setPointSizeF(m_textFont.pointSizeF() * share);
    } else if (m_textFont.pixelSize() > 0) {
        font.setPixelSize(
            std::max(1, static_cast<int>(std::lround(m_textFont.pixelSize() * share))));
    }
    return font;
}

QColor AnnotationCards::kindColor(AnnotationKind kind) const {
    QColor color;
    switch (kind) {
    case AnnotationKind::Comment:
        color = m_colors.comment;
        break;
    case AnnotationKind::Todo:
        color = m_colors.todo;
        break;
    case AnnotationKind::Note:
        color = m_colors.note;
        break;
    }
    return color.isValid() ? color : m_colors.text;
}

QString AnnotationCards::kindTitle(AnnotationKind kind) {
    switch (kind) {
    case AnnotationKind::Comment:
        return tr("Comment");
    case AnnotationKind::Todo:
        return tr("To do");
    case AnnotationKind::Note:
        return tr("Note");
    }
    return tr("Comment");
}

void AnnotationCards::setHovered(const QString& id, std::optional<AnnotationCardPart> part) {
    m_hovered = id;
    m_hoveredPart = id.isEmpty() ? std::nullopt : part;
}

// =============================================================================
// Geometry
// =============================================================================

std::vector<AnnotationMark> AnnotationCards::cardMarks(const QTextBlock& block) const {
    std::vector<AnnotationMark> marks;
    if (!m_shown || !block.isValid()) {
        return marks;
    }
    for (const AnnotationMark& mark : annotationMarksIn(block)) {
        if (hasCard(mark.kind)) {
            marks.push_back(mark);
        }
    }

    // A new one written in the frame: its room among the cards, in the order of its place
    // (after the marks already there)
    if (m_writing && m_writing->id.isEmpty() && m_writing->block == block.blockNumber()) {
        AnnotationMark writing;
        writing.offset = m_writing->offset;
        writing.kind = m_writing->kind;
        const auto after =
            std::upper_bound(marks.begin(), marks.end(), writing.offset,
                             [](int offset, const AnnotationMark& mark) { return offset < mark.offset; });
        marks.insert(after, writing);
    }
    return marks;
}

namespace {

/// @brief Everything a card needs to be measured and drawn
struct CardContext {
    const Metrics& m;
    bool compact = false;
};

/// @brief The text of a card as it is broken into lines (a compact one with its author and
///        date at its end, in the smaller font)
int layOutCardText(QTextLayout& layout, const AnnotationMark& mark, const CardContext& context,
                   qreal width, const QColor& secondary) {
    const Metrics& m = context.m;
    QString text = mark.text.trimmed().isEmpty() ? AnnotationCards::tr("(no text)") : mark.text;
    QList<QTextLayout::FormatRange> formats;
    if (context.compact) {
        QStringList made;
        if (!mark.author.trimmed().isEmpty()) {
            made << mark.author.trimmed();
        }
        if (const QString date = shortDate(mark.created); !date.isEmpty()) {
            made << date;
        }
        if (!made.isEmpty()) {
            // A no-break space keeps the dash with the author
            const QString meta = QString(QChar(0x00A0)) + QChar(0x2013) + QLatin1Char(' ') +
                                 made.join(QStringLiteral(", "));
            QTextLayout::FormatRange range;
            range.start = static_cast<int>(text.size());
            range.length = static_cast<int>(meta.size());
            range.format.setFont(m.small);
            if (secondary.isValid()) {
                range.format.setForeground(secondary);
            }
            formats << range;
            text += meta;
        }
    }
    return breakLines(layout, layoutText(text), m.text, width, m.lineHeight(), formats);
}

/// @brief The width of a card's text
qreal textWidthOf(const QRectF& card, const CardContext& context) {
    const Metrics& m = context.m;
    if (context.compact) {
        return card.width() - (PAD_LEFT + ICON + 2.0 * GAP + MENU_WIDTH + MENU_EDGE) * m.em;
    }
    return card.width() - (PAD_LEFT + PAD_RIGHT) * m.em;
}

/// @brief Where the parts of a card are, from its rectangle and the lines its text shows
CardParts partsOf(const QRectF& card, const CardContext& context, int shownLines, bool cut) {
    const Metrics& m = context.m;
    CardParts parts;
    const qreal menuLeft = card.right() - (MENU_EDGE + MENU_WIDTH) * m.em;
    if (context.compact) {
        const qreal top = card.top() + COMPACT_PAD * m.em;
        parts.icon = QRectF(card.left() + PAD_LEFT * m.em,
                            top + (m.lineHeight() - ICON * m.em) / 2.0, ICON * m.em, ICON * m.em);
        parts.menu = QRectF(menuLeft, top + (m.lineHeight() - MENU_HEIGHT * m.em) / 2.0,
                            MENU_WIDTH * m.em, MENU_HEIGHT * m.em);
        parts.text = QRectF(parts.icon.right() + GAP * m.em, top, textWidthOf(card, context),
                            shownLines * m.lineHeight());
    } else {
        const qreal top = card.top() + PAD_TOP * m.em;
        const qreal header = m.headerHeight();
        parts.icon = QRectF(card.left() + PAD_LEFT * m.em, top + (header - ICON * m.em) / 2.0,
                            ICON * m.em, ICON * m.em);
        parts.menu = QRectF(menuLeft, top + (header - MENU_HEIGHT * m.em) / 2.0,
                            MENU_WIDTH * m.em, MENU_HEIGHT * m.em);
        parts.text = QRectF(card.left() + PAD_LEFT * m.em, top + header + TEXT_GAP * m.em,
                            textWidthOf(card, context), shownLines * m.lineHeight());
    }
    if (cut) {
        const QString showAll = AnnotationCards::tr("Show all");
        parts.showAll = QRectF(parts.text.left(), parts.text.bottom() + TEXT_GAP * m.em,
                               QFontMetricsF(m.link).horizontalAdvance(showAll), m.linkHeight());
    }
    return parts;
}

}  // namespace

std::vector<AnnotationCardBox> AnnotationCards::cardsOf(const QTextBlock& block, qreal top,
                                                        qreal width) const {
    std::vector<AnnotationCardBox> cards;
    const std::vector<AnnotationMark> marks = cardMarks(block);
    if (marks.empty()) {
        return cards;
    }
    const Metrics m = metricsOf(cardFont());
    const CardContext context{m, m_settings.compact};
    const qreal left = INDENT * m.em;
    const qreal cardWidth = std::max(MIN_WIDTH * m.em, width - left);

    qreal y = top + TOP_GAP * m.em;
    for (const AnnotationMark& mark : marks) {
        AnnotationCardBox card;
        card.mark = mark;
        card.writing = m_writing && (mark.id.isEmpty() || mark.id == m_writing->id);
        card.collapsed = !card.writing && isCollapsed(mark.id);
        if (card.collapsed && !m_settings.collapsedLine) {
            continue;  // its mark alone
        }

        qreal height = 0.0;
        if (card.writing) {
            height = m_writing->height;
        } else if (card.collapsed) {
            height = m.lineCardHeight();
        } else {
            QTextLayout layout;
            const QRectF probe(left, 0.0, cardWidth, 0.0);
            const int lines = layOutCardText(layout, mark, context, textWidthOf(probe, context), {});
            const int limit = m_settings.longLines;
            const bool cut = cutsShort(mark.id) && lines > limit;
            card.shownLines = cut ? limit : 0;
            const int shown = cut ? limit : lines;
            const qreal text = shown * m.lineHeight() + (cut ? TEXT_GAP * m.em + m.linkHeight() : 0.0);
            height = context.compact
                         ? 2.0 * COMPACT_PAD * m.em + std::max(text, MENU_HEIGHT * m.em)
                         : (PAD_TOP + TEXT_GAP + PAD_BOTTOM) * m.em + m.headerHeight() + text;
        }
        if (!cards.empty()) {
            y += CARD_GAP * m.em;
        }
        card.rect = QRectF(left, y, cardWidth, height);
        y += height;
        cards.push_back(card);
    }
    return cards;
}

qreal AnnotationCards::spaceBelow(const QTextBlock& block, qreal width) const {
    const std::vector<AnnotationCardBox> cards = cardsOf(block, 0.0, width);
    if (cards.empty()) {
        return 0.0;
    }
    const qreal em = metricsOf(cardFont()).em;
    return cards.back().rect.bottom() + BOTTOM_GAP * em;
}

std::optional<AnnotationCardHit> AnnotationCards::hitTest(const QTextBlock& block, qreal top,
                                                          qreal width, const QPointF& point) const {
    if (point.y() < top) {
        return std::nullopt;
    }
    const Metrics m = metricsOf(cardFont());
    const CardContext context{m, m_settings.compact};
    for (const AnnotationCardBox& card : cardsOf(block, top, width)) {
        if (card.writing || !card.rect.contains(point)) {
            continue;
        }
        AnnotationCardHit hit;
        hit.id = card.mark.id;
        hit.rect = card.rect;
        if (card.collapsed) {
            hit.part = AnnotationCardPart::Line;
            return hit;
        }
        const int shown = card.shownLines;
        const CardParts parts = partsOf(card.rect, context, shown, shown > 0);
        if (parts.menu.contains(point)) {
            hit.part = AnnotationCardPart::Menu;
        } else if (card.mark.kind == AnnotationKind::Todo &&
                   parts.icon.adjusted(-0.2 * m.em, -0.2 * m.em, 0.2 * m.em, 0.2 * m.em)
                       .contains(point)) {
            hit.part = AnnotationCardPart::Done;
        } else if (!parts.showAll.isEmpty() && parts.showAll.contains(point)) {
            hit.part = AnnotationCardPart::ShowAll;
        }
        return hit;
    }
    return std::nullopt;
}

// =============================================================================
// Drawing
// =============================================================================

void AnnotationCards::paint(QPainter* painter, const QTextBlock& block, qreal top,
                            qreal width) const {
    const std::vector<AnnotationCardBox> cards = cardsOf(block, top, width);
    if (cards.empty()) {
        return;
    }
    const Metrics m = metricsOf(cardFont());
    const CardContext context{m, m_settings.compact};
    const qreal tint = std::clamp(m_settings.tintPercent, 0, 400) / 100.0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    for (const AnnotationCardBox& card : cards) {
        if (card.writing) {
            continue;  // the frame lies over its room
        }
        const QColor kind = kindColor(card.mark.kind);
        const QRectF& rect = card.rect;
        const qreal radius = RADIUS * m.em;
        QPainterPath shape;
        shape.addRoundedRect(rect, radius, radius);

        // A collapsed one: a line with the start of its text, in less of the kind's color
        if (card.collapsed) {
            const gui::AnnotationCardColors colors =
                gui::annotationCardColors(kind, m_colors.paper, m_colors.text, tint * LINE_TINT);
            painter->fillPath(shape, colors.background);
            painter->save();
            painter->setClipPath(shape);
            painter->fillRect(QRectF(rect.left(), rect.top(), BAR * m.em, rect.height()),
                              colors.kind);
            painter->restore();

            const qreal middle = rect.center().y();
            qreal x = rect.left() + PAD_LEFT * m.em;
            // The arrow of a collapsed one
            const qreal arrow = 0.32 * m.em;
            QPainterPath triangle;
            triangle.moveTo(x, middle - arrow);
            triangle.lineTo(x + arrow * 1.2, middle);
            triangle.lineTo(x, middle + arrow);
            triangle.closeSubpath();
            painter->fillPath(triangle, colors.kindName);
            x += arrow * 1.2 + GAP * m.em;
            const qreal icon = ICON * m.em * LINE_FONT;
            if (card.mark.kind == AnnotationKind::Comment) {
                paintCommentIcon(painter, QRectF(x, middle - icon / 2.0, icon, icon),
                                 colors.kindName);
                x += icon + GAP * m.em;
            } else if (card.mark.kind == AnnotationKind::Note) {
                paintNoteIcon(painter, QRectF(x, middle - icon / 2.0, icon, icon), colors.kindName);
                x += icon + GAP * m.em;
            }
            const QString name = kindTitle(card.mark.kind) + QLatin1Char(':');
            const QFontMetricsF nameMetrics(m.lineName);
            const qreal baseline = middle + (nameMetrics.ascent() - nameMetrics.descent()) / 2.0;
            painter->setFont(m.lineName);
            painter->setPen(colors.kindName);
            painter->drawText(QPointF(x, baseline), name);
            x += nameMetrics.horizontalAdvance(name) + GAP * m.em;
            const QString first = card.mark.text.trimmed().section(QLatin1Char('\n'), 0, 0);
            const qreal room = rect.right() - PAD_RIGHT * m.em - x;
            if (room > 0 && !first.isEmpty()) {
                painter->setFont(m.line);
                painter->setPen(colors.secondary);
                painter->drawText(QPointF(x, baseline),
                                  QFontMetricsF(m.line).elidedText(first, Qt::ElideRight, room));
            }
            continue;
        }

        const gui::AnnotationCardColors colors =
            gui::annotationCardColors(kind, m_colors.paper, m_colors.text, tint);

        // A rounded card with a light shadow, the bar in the kind's color on the left
        painter->fillPath(shape.translated(0.0, SHADOW * m.em),
                          m_colors.paper.darker(SHADOW_DARKER));
        painter->fillPath(shape, colors.background);
        painter->save();
        painter->setClipPath(shape);
        painter->fillRect(QRectF(rect.left(), rect.top(), BAR * m.em, rect.height()), colors.kind);
        painter->restore();

        // The card of the annotation gone to has an outline
        if (!m_selected.isEmpty() && card.mark.id == m_selected && m_colors.selection.isValid()) {
            const qreal out = OUTLINE_GAP * m.em + OUTLINE * m.em / 2.0;
            painter->setPen(QPen(m_colors.selection, OUTLINE * m.em));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect.adjusted(-out, -out, out, out), radius + out,
                                     radius + out);
        }

        const bool cut = card.shownLines > 0;
        const CardParts parts = partsOf(rect, context, card.shownLines, cut);

        // The kind's icon, or a to-do's box
        switch (card.mark.kind) {
        case AnnotationKind::Comment:
            paintCommentIcon(painter, parts.icon, colors.kindName);
            break;
        case AnnotationKind::Note:
            paintNoteIcon(painter, parts.icon, colors.kindName);
            break;
        case AnnotationKind::Todo: {
            const qreal box = CHECK * m.em;
            QRectF check(0.0, 0.0, box, box);
            check.moveCenter(parts.icon.center());
            paintCheckBox(painter, check, colors.kindName);
            break;
        }
        }

        // The "..." button: shown on the card under the mouse and on the one gone to
        const bool hovered = card.mark.id == m_hovered;
        if (hovered || card.mark.id == m_selected) {
            if (hovered && m_hoveredPart == AnnotationCardPart::Menu) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(gui::mixedColor(colors.background, colors.kind, MENU_HOVER));
                painter->drawRoundedRect(parts.menu, 0.28 * m.em, 0.28 * m.em);
            }
            paintMoreDots(painter, parts.menu, colors.secondary, m.em);
        }

        // The header: the kind, the author and on the right the date
        if (!context.compact) {
            const qreal headerTop = rect.top() + PAD_TOP * m.em;
            const qreal header = m.headerHeight();
            const QFontMetricsF small(m.small);
            const QFontMetricsF bold(m.kindName);
            const qreal baseline = headerTop + (header + bold.ascent() - bold.descent()) / 2.0;
            qreal x = parts.icon.right() + GAP * m.em;
            const QString name = kindTitle(card.mark.kind);
            painter->setFont(m.kindName);
            painter->setPen(colors.kindName);
            painter->drawText(QPointF(x, baseline), name);
            x += bold.horizontalAdvance(name) + GAP * m.em;

            qreal right = parts.menu.left() - GAP * m.em;
            painter->setFont(m.small);
            painter->setPen(colors.secondary);
            const QString date = longDate(card.mark.created);
            if (!date.isEmpty()) {
                const qreal dateWidth = small.horizontalAdvance(date);
                if (right - dateWidth > x) {
                    painter->drawText(QPointF(right - dateWidth, baseline), date);
                    right -= dateWidth + GAP * m.em;
                }
            }
            const QString author = card.mark.author.trimmed();
            if (!author.isEmpty() && right > x) {
                painter->drawText(QPointF(x, baseline),
                                  small.elidedText(author, Qt::ElideRight, right - x));
            }
        }

        // The text, as it was written; a long one cut short, with "Show all" under it
        QTextLayout layout;
        const int lines = layOutCardText(layout, card.mark, context, parts.text.width(),
                                         colors.secondary);
        painter->setPen(colors.text);
        painter->setFont(m.text);
        const int shown = cut ? std::min(card.shownLines, lines) : lines;
        for (int i = 0; i < shown; ++i) {
            const QTextLine line = layout.lineAt(i);
            if (cut && i == shown - 1) {
                // The last line shown ends with an ellipsis
                QString rest = layout.text().mid(line.textStart());
                rest.replace(QChar::LineSeparator, QLatin1Char(' '));
                painter->drawText(QPointF(parts.text.left(), parts.text.top() + line.y() + line.ascent()),
                                  QFontMetricsF(m.text).elidedText(rest.simplified(), Qt::ElideRight,
                                                                   parts.text.width()));
            } else {
                line.draw(painter, parts.text.topLeft());
            }
        }
        if (cut) {
            painter->setFont(m.link);
            painter->setPen(colors.kindName);
            const QFontMetricsF link(m.link);
            painter->drawText(QPointF(parts.showAll.left(),
                                      parts.showAll.top() +
                                          (parts.showAll.height() + link.ascent() - link.descent()) / 2.0),
                              tr("Show all"));
        }
    }
    painter->restore();
}

}  // namespace kalahari::editor
