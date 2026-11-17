// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_book_view.cpp
/// @brief The time and the book

#include <kalahari/timeline/timeline_book_view.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>

#include <QFontMetricsF>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace kalahari::timeline {

namespace {

constexpr double LABELS = 200.0;   ///< The column of the chapters
constexpr double AXIS_LEFT = 10.0;
constexpr double TOP = 40.0;
constexpr double ROW = 34.0;
constexpr double ORDER_STEP = 40.0;
constexpr double ORDER_LEFT = 14.0;
constexpr double ROOM_BESIDE = 110.0;
constexpr double ROOM_AFTER = 80.0;
constexpr double ROOM_END = 20.0;
constexpr double EDGE = 16.0;
constexpr double STRIPE = 8.0;    ///< The stripes of a gap drawn short
constexpr double SHORT_FROM = 60.0;
constexpr double GAP_WIDTH = 38.0;
constexpr double LEAST_SEGMENT = 30.0;
constexpr double LABEL_ROOM = 90.0;
constexpr double DOT = 6.0;
constexpr double RING = 10.0;
constexpr double BAR = 10.0;
constexpr double BAR_FROM = 6.0;
constexpr double DIM_OPACITY = 0.25;
constexpr double PADDING = 8.0;
constexpr double CHIP_PADDING = 6.0;
constexpr double CHIP_GAP = 6.0;
constexpr int DAYS_OF_FLASHBACK = 7;
constexpr int SCROLL_STEP = 20;
/// The scales the view tries, from the widest, until the whole story fits the window
constexpr std::array<double, 7> SCALES{26.0, 20.0, 16.0, 12.0, 9.0, 6.0, 4.0};

const TimelineIndex& emptyIndex() {
    static const TimelineIndex index;
    return index;
}

void stripes(QPainter& painter, const QRectF& rect, const QColor& color) {
    painter.save();
    painter.setClipRect(rect, Qt::IntersectClip);
    painter.setPen(QPen(color, STRIPE / 2.0));
    const double first = rect.left() - rect.height();
    for (int i = 0; i * STRIPE < rect.width() + rect.height(); ++i) {
        const double x = first + i * STRIPE;
        painter.drawLine(QPointF(x, rect.bottom()), QPointF(x + rect.height(), rect.top()));
    }
    painter.restore();
}

}  // namespace

TimelineBookView::TimelineBookView(TimelineDocument& document, QWidget* parent)
    : QAbstractScrollArea(parent),
      m_document(document),
      m_index(&emptyIndex()),
      m_look(TimelineLook::current()) {
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    horizontalScrollBar()->setSingleStep(SCROLL_STEP);
    verticalScrollBar()->setSingleStep(SCROLL_STEP);
    //: The accessible name of the view of a timeline. In Polish: Czas i książka
    setAccessibleName(tr("Time and the book"));
    connect(&m_document, &TimelineDocument::changed, this, &TimelineBookView::refresh);
    connect(&m_document, &TimelineDocument::viewChanged, this, &TimelineBookView::refresh);
    connect(&m_document, &TimelineDocument::selectionChanged, viewport(),
            qOverload<>(&QWidget::update));
    refresh();
}

void TimelineBookView::setIndex(const TimelineIndex* index) {
    m_index = index != nullptr ? index : &emptyIndex();
    refresh();
}

void TimelineBookView::setLook(const TimelineLook& look) {
    m_look = look;
    refresh();
}

void TimelineBookView::setHighlight(const QString& filter) {
    m_highlight = filter;
    viewport()->update();
}

void TimelineBookView::refresh() {
    layout();
    viewport()->update();
}

void TimelineBookView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    refresh();
}

// =============================================================================
// The layout
// =============================================================================

void TimelineBookView::layout() {
    const Timeline& timeline = m_document.timeline();
    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    const QList<const TimelineEvent*> inOrder = timeline.eventsInOrder();
    QList<const TimelineEvent*> linked;
    for (const TimelineEvent* event : inOrder) {
        if (!event->chapters.isEmpty() && (ordered || event->time)) {
            linked.append(event);
        }
    }
    m_axis.reset();
    double width = 0.0;
    if (ordered) {
        width = static_cast<double>(inOrder.size()) * ORDER_STEP + ROOM_END + EDGE;
    } else {
        QList<TimeAxis::Span> spans;
        for (const TimelineEvent* event : linked) {
            spans.append(TimeAxis::Span{event->time->start(), event->time->end()});
        }
        // An overview: the days narrow until the whole story fits the window
        const double room = viewport()->width() - LABELS - ROOM_BESIDE;
        for (const double perDay : SCALES) {
            TimeAxis::Options options;
            options.pixelsPerDay = perDay;
            options.roomBefore = EDGE;
            options.roomAfter = EDGE;
            options.compress = true;
            options.shortFrom = SHORT_FROM;
            options.gapWidth = GAP_WIDTH;
            m_axis.emplace(spans, options, static_cast<double>(timeline.dayOne()) + 0.5);
            if (m_axis->width() + ROOM_END <= room) {
                break;
            }
        }
        width = m_axis->width() + ROOM_END;
    }
    const auto from = [this, ordered, &inOrder](const TimelineEvent& event) {
        return LABELS + AXIS_LEFT +
               (ordered ? static_cast<double>(inOrder.indexOf(&event)) * ORDER_STEP + ORDER_LEFT
                        : m_axis->x(event.time->start()));
    };
    const auto to = [this, ordered, &from](const TimelineEvent& event) {
        return ordered || !event.time->isSpan() ? from(event)
                                                : LABELS + AXIS_LEFT + m_axis->x(event.time->end());
    };
    const auto when = [ordered](const TimelineEvent& event) {
        return ordered ? static_cast<double>(event.order) : event.time->start();
    };

    m_marks.clear();
    m_mainOfRow.clear();
    const QList<TimelineIndex::Entry>& rows = m_index->chapters;
    std::optional<double> before;
    for (qsizetype row = 0; row < rows.size(); ++row) {
        const double y = TOP + static_cast<double>(row) * ROW + ROW / 2.0;
        int main = -1;
        const qsizetype first = m_marks.size();
        for (const TimelineEvent* event : linked) {
            if (!event->chapters.contains(rows.at(row).id)) {
                continue;
            }
            m_marks.append(Mark{event->id, static_cast<int>(row), from(*event), to(*event), y,
                                false});
            if (main < 0 || when(*event) > when(*timeline.event(m_marks.at(main).id))) {
                main = static_cast<int>(m_marks.size() - 1);
            }
        }
        // A flashback: earlier than what happened in the chapter before
        const double room = ordered ? 1.0 : static_cast<double>(DAYS_OF_FLASHBACK);
        for (qsizetype i = first; i < m_marks.size(); ++i) {
            const TimelineEvent* event = timeline.event(m_marks.at(i).id);
            m_marks[i].flashback = before.has_value() && when(*event) < *before - room;
        }
        if (main >= 0) {
            before = when(*timeline.event(m_marks.at(main).id));
        }
        m_mainOfRow.append(main);
    }
    m_contentWidth = LABELS + width + ROOM_AFTER;
    layoutChips(TOP + static_cast<double>(rows.size()) * ROW + PADDING * 2);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setRange(
        0, std::max(0, static_cast<int>(std::ceil(m_contentWidth - viewport()->width()))));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(
        0, std::max(0, static_cast<int>(std::ceil(m_contentHeight - viewport()->height()))));
}

void TimelineBookView::layoutChips(double top) {
    const Timeline& timeline = m_document.timeline();
    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    QList<const TimelineEvent*> unbooked;
    QList<const TimelineEvent*> untimed;
    for (const TimelineEvent* event : timeline.eventsInOrder()) {
        if (event->chapters.isEmpty()) {
            unbooked.append(event);
        } else if (!ordered && !event->time) {
            untimed.append(event);
        }
    }
    m_chips.clear();
    const QFontMetricsF metrics(font());
    const double right = std::max(static_cast<double>(viewport()->width()), 400.0) - PADDING;
    const double height = metrics.height() + 6.0;
    double y = top;
    const auto flow = [&](const QString& text, const QList<const TimelineEvent*>& events,
                          QRectF* rect) {
        *rect = QRectF();
        if (events.isEmpty()) {
            return;
        }
        const double textWidth = std::ceil(metrics.horizontalAdvance(text));
        *rect = QRectF(PADDING, y, textWidth, height);
        double x = PADDING + textWidth + CHIP_GAP;
        for (const TimelineEvent* event : events) {
            const double chipWidth =
                std::ceil(metrics.horizontalAdvance(event->title)) + 2 * CHIP_PADDING;
            if (x + chipWidth > right && x > PADDING) {
                x = PADDING;
                y += height + CHIP_GAP;
            }
            m_chips.append(Chip{event->id, QRectF(x, y, chipWidth, height)});
            x += chipWidth + CHIP_GAP;
        }
        y += height + CHIP_GAP * 2;
    };
    //: Under the time and the book. In Polish: Jeszcze poza książką (%1):
    m_unbookedText = tr("The book does not tell yet (%1):").arg(unbooked.size());
    //: In Polish: W książce, ale bez czasu (%1):
    m_untimedText = tr("In the book, but without a time (%1):").arg(untimed.size());
    flow(m_unbookedText, unbooked, &m_unbookedRect);
    flow(m_untimedText, untimed, &m_untimedRect);
    const double legendWidth = std::max(300.0, right - PADDING);
    //: In Polish: Przerywana linia łączy rozdziały w kolejności czytania. Obwódka oznacza
    //: retrospekcję: wydarzenie wcześniejsze niż to, co działo się w poprzednim rozdziale.
    //: Kliknięcie zaznacza wydarzenie, dwuklik otwiera okno „Wydarzenie”.
    m_legend =
        tr("The dashed line joins the chapters in the order of reading. A ring marks a "
           "flashback: an event earlier than what happened in the chapter before. A click "
           "selects an event, a double click opens the window “Event”.");
    const QRectF legendBox = metrics.boundingRect(QRectF(PADDING, y, legendWidth, 1000.0),
                                                  Qt::TextWordWrap | Qt::AlignLeft, m_legend);
    m_legendRect = QRectF(PADDING, y, legendWidth, legendBox.height());
    m_contentHeight = m_legendRect.bottom() + PADDING;
}

QRectF TimelineBookView::markRect(const Mark& mark) const {
    if (mark.to - mark.from > BAR_FROM) {
        return {mark.from, mark.y - BAR / 2.0, mark.to - mark.from, BAR};
    }
    return {mark.from - DOT, mark.y - DOT, 2 * DOT, 2 * DOT};
}

QColor TimelineBookView::colorOf(const QString& id) const {
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return m_look.none;
    }
    const QColor own = m_look.paletteColor(event->color);
    if (own.isValid()) {
        return own;
    }
    const TimelineThread* thread = m_document.timeline().thread(event->thread);
    const QColor color = thread != nullptr ? m_look.paletteColor(thread->color) : QColor();
    return color.isValid() ? color : m_look.none;
}

QString TimelineBookView::idAt(const QPointF& point) const {
    const QPointF at(point.x() + horizontalScrollBar()->value(),
                     point.y() + verticalScrollBar()->value());
    for (const Mark& mark : m_marks) {
        if (markRect(mark).adjusted(-4.0, -4.0, 4.0, 4.0).contains(at)) {
            return mark.id;
        }
    }
    for (const Chip& chip : m_chips) {
        if (chip.rect.contains(at)) {
            return chip.id;
        }
    }
    return QString();
}

// =============================================================================
// Painting
// =============================================================================

void TimelineBookView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), m_look.card);
    painter.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
    const Timeline& timeline = m_document.timeline();
    const QList<TimelineIndex::Entry>& rows = m_index->chapters;
    const double bottom = TOP + static_cast<double>(rows.size()) * ROW;
    const QFontMetricsF metrics(font());
    const double width = std::max(m_contentWidth, static_cast<double>(viewport()->width()));

    for (qsizetype row = 0; row < rows.size(); ++row) {
        const QRectF rect(0.0, TOP + static_cast<double>(row) * ROW, width, ROW);
        painter.fillRect(rect, row % 2 == 0 ? m_look.laneAlternate : m_look.lane);
        painter.setPen(m_look.text);
        painter.drawText(QRectF(PADDING, rect.top(), LABELS - 2 * PADDING, ROW),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(rows.at(row).name, Qt::ElideRight,
                                            LABELS - 2 * PADDING));
    }

    painter.setPen(m_look.mutedText);
    if (m_axis) {
        for (const TimeAxis::Gap& gap : m_axis->gaps()) {
            const QRectF rect(LABELS + AXIS_LEFT + gap.x, TOP - PADDING, gap.width,
                              bottom - TOP + PADDING);
            stripes(painter, rect, m_look.gap);
            painter.setPen(m_look.mutedText);
            const QString length = StoryTime::lengthText(gap.days, m_look.style.language);
            painter.drawText(QRectF(rect.center().x() - LABEL_ROOM / 2.0, TOP - 26.0, LABEL_ROOM,
                                    metrics.height()),
                             Qt::AlignCenter, length);
        }
        double lastX = -1e9;
        for (const TimeAxis::Segment& segment : m_axis->segments()) {
            const double segmentWidth = (segment.to - segment.from) * m_axis->pixelsPerDay();
            if (segmentWidth < LEAST_SEGMENT) {
                continue;
            }
            const double x = LABELS + AXIS_LEFT + segment.x;
            painter.setPen(m_look.tick);
            painter.drawLine(QPointF(x, TOP - 6.0), QPointF(x, bottom));
            if (x - lastX > LABEL_ROOM) {
                const StoryTime day = StoryTime::day(static_cast<qint64>(std::ceil(segment.from)));
                StoryTimeStyle style = m_look.style;
                style.dates = DateStyle::Words;
                const QString label = timeline.view().scale == TimelineView::Scale::Days
                                          ? day.dayText(timeline.dayOne(), style)
                                          : day.dateText(style);
                painter.setPen(m_look.mutedText);
                painter.drawText(QPointF(x + 3.0, TOP - 24.0 + metrics.ascent()), label);
                lastX = x;
            }
        }
    } else {
        //: Above the time and the book in the order of the story. In Polish: kolejność
        //: wydarzeń →
        painter.drawText(QPointF(LABELS + AXIS_LEFT, TOP - 14.0), tr("order of the events →"));
    }

    // The path of reading: through the main event of each chapter
    QPainterPath path;
    bool started = false;
    for (const int main : m_mainOfRow) {
        if (main < 0) {
            continue;
        }
        const Mark& mark = m_marks.at(main);
        const QPointF point((mark.from + mark.to) / 2.0, mark.y);
        if (started) {
            path.lineTo(point);
        } else {
            path.moveTo(point);
            started = true;
        }
    }
    painter.setPen(QPen(m_look.mutedText, 1.2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QString selected = m_document.selected();
    for (const Mark& mark : m_marks) {
        const TimelineEvent* event = timeline.event(mark.id);
        const QRectF rect = markRect(mark);
        const bool bar = mark.to - mark.from > BAR_FROM;
        if (mark.flashback) {
            painter.setPen(QPen(m_look.flashback, 2.0));
            painter.setBrush(Qt::NoBrush);
            if (bar) {
                painter.drawRoundedRect(rect.adjusted(-5.0, -5.0, 5.0, 5.0), RING, RING);
            } else {
                painter.drawEllipse(QPointF(mark.from, mark.y), RING, RING);
            }
        }
        painter.setOpacity(event != nullptr && TimelineEdits::matches(*event, m_highlight)
                               ? 1.0
                               : DIM_OPACITY);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colorOf(mark.id));
        if (bar) {
            painter.drawRoundedRect(rect, BAR / 2.0, BAR / 2.0);
        } else {
            painter.drawEllipse(rect);
        }
        painter.setOpacity(1.0);
        if (mark.id == selected && event != nullptr) {
            painter.setPen(QPen(m_look.selection, 2.0));
            painter.setBrush(Qt::NoBrush);
            if (bar) {
                painter.drawRoundedRect(rect.adjusted(-3.0, -3.0, 3.0, 3.0), 8.0, 8.0);
            } else {
                painter.drawEllipse(QPointF(mark.from, mark.y), DOT + 3.0, DOT + 3.0);
            }
            painter.setPen(m_look.text);
            painter.drawText(QPointF(mark.to + 12.0, mark.y - 9.0), event->title);
        }
    }

    // The events outside the drawing
    painter.setPen(m_look.mutedText);
    if (!m_unbookedRect.isNull()) {
        painter.drawText(m_unbookedRect, Qt::AlignLeft | Qt::AlignVCenter, m_unbookedText);
    }
    if (!m_untimedRect.isNull()) {
        painter.drawText(m_untimedRect, Qt::AlignLeft | Qt::AlignVCenter, m_untimedText);
    }
    for (const Chip& chip : m_chips) {
        const TimelineEvent* event = timeline.event(chip.id);
        if (event == nullptr) {
            continue;
        }
        painter.setPen(QPen(chip.id == selected ? m_look.selection : m_look.mid,
                            chip.id == selected ? 2.0 : 1.0));
        painter.setBrush(m_look.window);
        painter.drawRoundedRect(chip.rect, 3.0, 3.0);
        painter.setPen(m_look.text);
        painter.drawText(chip.rect, Qt::AlignCenter, event->title);
    }
    painter.setPen(m_look.mutedText);
    painter.drawText(m_legendRect, Qt::TextWordWrap | Qt::AlignLeft, m_legend);
}

// =============================================================================
// The keys and the mouse
// =============================================================================

void TimelineBookView::selectAcross(int direction) {
    QList<const Mark*> sequence;
    for (const Mark& mark : m_marks) {
        sequence.append(&mark);
    }
    std::stable_sort(sequence.begin(), sequence.end(),
                     [](const Mark* a, const Mark* b) { return a->from < b->from; });
    QStringList ids;
    for (const Mark* mark : sequence) {
        if (!ids.contains(mark->id)) {
            ids.append(mark->id);
        }
    }
    if (ids.isEmpty()) {
        return;
    }
    const qsizetype at = ids.indexOf(m_document.selected());
    const qsizetype to = at < 0 ? 0 : std::clamp<qsizetype>(at + direction, 0, ids.size() - 1);
    m_document.select(ids.at(to));
}

void TimelineBookView::selectDown(int direction) {
    const Mark* current = nullptr;
    for (const Mark& mark : m_marks) {
        if (mark.id == m_document.selected()) {
            current = &mark;
            break;
        }
    }
    if (current == nullptr) {
        selectAcross(1);
        return;
    }
    const int rows = static_cast<int>(m_index->chapters.size());
    for (int row = current->row + direction; row >= 0 && row < rows; row += direction) {
        const Mark* nearest = nullptr;
        double distance = std::numeric_limits<double>::max();
        for (const Mark& mark : m_marks) {
            if (mark.row == row && std::abs(mark.from - current->from) < distance) {
                distance = std::abs(mark.from - current->from);
                nearest = &mark;
            }
        }
        if (nearest != nullptr) {
            m_document.select(nearest->id);
            return;
        }
    }
}

bool TimelineBookView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride) {
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
        const int code = key->key();
        const bool own =
            (modifiers == Qt::NoModifier &&
             (code == Qt::Key_Return || code == Qt::Key_Enter || code == Qt::Key_Home ||
              code == Qt::Key_End || code == Qt::Key_Escape || code == Qt::Key_Left ||
              code == Qt::Key_Right || code == Qt::Key_Up || code == Qt::Key_Down)) ||
            (modifiers == Qt::ControlModifier &&
             (code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_Return ||
              code == Qt::Key_Enter));
        if (own) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const QString id = idAt(viewport()->mapFrom(this, help->pos()));
        const TimelineEvent* shown = m_document.timeline().event(id);
        if (shown != nullptr) {
            QToolTip::showText(help->globalPos(),
                               shown->title + QStringLiteral(" – ") +
                                   TimelineWords::when(m_document.timeline(), *shown, m_look),
                               this);
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return QAbstractScrollArea::event(event);
}

void TimelineBookView::keyPressEvent(QKeyEvent* event) {
    const bool control = (event->modifiers() & Qt::ControlModifier) != 0;
    const QString selected = m_document.selected();
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
            selectAcross(event->key() == Qt::Key_Left ? -1 : 1);
            return;
        case Qt::Key_Up:
        case Qt::Key_Down:
            selectDown(event->key() == Qt::Key_Up ? -1 : 1);
            return;
        case Qt::Key_Home:
        case Qt::Key_End:
            selectAcross(event->key() == Qt::Key_Home ? -static_cast<int>(m_marks.size())
                                                      : static_cast<int>(m_marks.size()));
            return;
        case Qt::Key_Escape:
            m_document.select(QString());
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!selected.isEmpty()) {
                if (control) {
                    emit chapterRequested(selected);
                } else {
                    emit eventRequested(selected);
                }
            }
            return;
        case Qt::Key_Z:
        case Qt::Key_Y:
            if (control) {
                const bool undo = event->key() == Qt::Key_Z &&
                                  !(event->modifiers() & Qt::ShiftModifier);
                const QString what = undo ? m_document.undoText() : m_document.redoText();
                if (undo) {
                    m_document.undo();
                } else {
                    m_document.redo();
                }
                if (!what.isEmpty()) {
                    //: In Polish: Cofnięto: %1. / Ponowiono: %1.
                    emit told(undo ? tr("Undone: %1.").arg(what) : tr("Redone: %1.").arg(what),
                              false);
                }
                return;
            }
            break;
        default:
            break;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void TimelineBookView::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    m_document.select(idAt(event->position()));
}

void TimelineBookView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QString id = idAt(event->position());
    if (!id.isEmpty()) {
        emit eventRequested(id);
    }
}

}  // namespace kalahari::timeline
