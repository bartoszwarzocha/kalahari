// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_axis_view.cpp
/// @brief The axis of a timeline: its coordinates, zoom and the field of a title

#include <kalahari/timeline/timeline_axis_view.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>

#include <QFocusEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace kalahari::timeline {

namespace {

/// The zoom of the program's start: 100%
constexpr int HOME_ZOOM = 4;
constexpr double SHOW_MARGIN = 10.0;
constexpr int EDITOR_INSET = 4;
constexpr int SCROLL_STEP = 20;

const TimelineIndex& emptyIndex() {
    static const TimelineIndex index;
    return index;
}

}  // namespace

TimelineAxisView::TimelineAxisView(TimelineDocument& document, QWidget* parent)
    : QAbstractScrollArea(parent),
      m_document(document),
      m_index(&emptyIndex()),
      m_look(TimelineLook::current()) {
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    horizontalScrollBar()->setSingleStep(SCROLL_STEP);
    verticalScrollBar()->setSingleStep(SCROLL_STEP);
    //: The accessible name of the axis of a timeline. In Polish: Oś czasu
    setAccessibleName(tr("Timeline axis"));
    m_scene = std::make_unique<TimelineScene>(m_document.timeline(), *m_index, m_look, font());

    connect(&m_document, &TimelineDocument::changed, this, &TimelineAxisView::refresh);
    connect(&m_document, &TimelineDocument::viewChanged, this, [this]() {
        refresh();
        emit zoomChanged();
    });
    connect(&m_document, &TimelineDocument::selectionChanged, this, [this]() {
        ensureShown(m_document.selected());
        viewport()->update();
    });
    refresh();
}

TimelineAxisView::~TimelineAxisView() = default;

void TimelineAxisView::setIndex(const TimelineIndex* index) {
    m_index = index != nullptr ? index : &emptyIndex();
    refresh();
}

void TimelineAxisView::setLook(const TimelineLook& look) {
    m_look = look;
    refresh();
}

void TimelineAxisView::setHighlight(const QString& filter) {
    m_highlight = filter;
    viewport()->update();
}

void TimelineAxisView::refresh() {
    if (isEditing() && m_document.timeline().event(m_editingId) == nullptr) {
        m_editingNew = false;
        finishEditing(false);
    }
    m_scene = std::make_unique<TimelineScene>(m_document.timeline(), *m_index, m_look, font());
    m_scene->layout(viewport()->width());
    updateScrollBars();
    if (isEditing()) {
        if (const TimelineItem* item = m_scene->itemOf(m_editingId, m_editingLane);
            item != nullptr) {
            const bool tray = item->lane.isEmpty() && m_scene->itemsOf(item->id).isEmpty();
            const QRectF rect = viewRectOf(*item, tray);
            m_editor->setGeometry(QRect(static_cast<int>(rect.x()) + EDITOR_INSET,
                                        static_cast<int>(rect.y()) + 2,
                                        static_cast<int>(rect.width()) - 2 * EDITOR_INSET,
                                        m_editor->height()));
        }
    }
    viewport()->update();
}

void TimelineAxisView::updateScrollBars() {
    const double laneWidth = viewport()->width() - TimelineScene::HEAD_WIDTH;
    const double laneHeight = viewport()->height() - m_scene->lanesTop();
    horizontalScrollBar()->setPageStep(std::max(1, static_cast<int>(laneWidth)));
    horizontalScrollBar()->setRange(
        0, std::max(0, static_cast<int>(std::ceil(m_scene->width() - laneWidth))));
    verticalScrollBar()->setPageStep(std::max(1, static_cast<int>(laneHeight)));
    verticalScrollBar()->setRange(
        0, std::max(0, static_cast<int>(std::ceil(m_scene->lanesHeight() - laneHeight))));
}

void TimelineAxisView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    m_scene->layout(viewport()->width());
    updateScrollBars();
}

void TimelineAxisView::scrollContentsBy(int /*dx*/, int /*dy*/) {
    if (isEditing()) {
        finishEditing(true);
    }
    viewport()->update();
}

// =============================================================================
// Coordinates
// =============================================================================

QPointF TimelineAxisView::toLanes(const QPointF& point) const {
    return {point.x() - TimelineScene::HEAD_WIDTH + horizontalScrollBar()->value(),
            point.y() - m_scene->lanesTop() + verticalScrollBar()->value()};
}

QPointF TimelineAxisView::fromLanes(const QPointF& point) const {
    return {point.x() + TimelineScene::HEAD_WIDTH - horizontalScrollBar()->value(),
            point.y() + m_scene->lanesTop() - verticalScrollBar()->value()};
}

QPointF TimelineAxisView::toTray(const QPointF& point) const {
    return {point.x(), point.y() - TimelineScene::AXIS_HEIGHT};
}

bool TimelineAxisView::inLanes(const QPointF& point) const {
    return point.x() >= TimelineScene::HEAD_WIDTH && point.y() >= m_scene->lanesTop();
}

bool TimelineAxisView::inTray(const QPointF& point) const {
    return point.y() >= TimelineScene::AXIS_HEIGHT && point.y() < m_scene->lanesTop();
}

const TimelineItem* TimelineAxisView::itemAt(const QPointF& point, bool* inTrayOut) const {
    *inTrayOut = false;
    if (inTray(point)) {
        *inTrayOut = true;
        return m_scene->trayItemAt(toTray(point));
    }
    return inLanes(point) ? m_scene->itemAt(toLanes(point)) : nullptr;
}

QRectF TimelineAxisView::viewRectOf(const TimelineItem& item, bool tray) const {
    if (tray) {
        return item.rect.translated(0.0, TimelineScene::AXIS_HEIGHT);
    }
    return {fromLanes(item.rect.topLeft()), item.rect.size()};
}

// =============================================================================
// The zoom and what shows
// =============================================================================

int TimelineAxisView::zoomPercent() const {
    const int step = m_document.timeline().view().zoom;
    return static_cast<int>(std::lround(TimeAxis::pixelsPerDayOf(step) /
                                        TimeAxis::pixelsPerDayOf(HOME_ZOOM) * 100.0));
}

void TimelineAxisView::zoomTo(int step, double anchorX) {
    if (m_scene->ordered()) {
        return;
    }
    const int to = std::clamp(step, 0, TimeAxis::zoomSteps() - 1);
    if (to == m_document.timeline().view().zoom) {
        return;
    }
    const double day = m_scene->dayAt(toLanes(QPointF(anchorX, 0.0)).x());
    m_document.changeView([to](TimelineView& view) { view.zoom = to; });
    const double x = m_scene->xOfDay(day) - (anchorX - TimelineScene::HEAD_WIDTH);
    horizontalScrollBar()->setValue(static_cast<int>(std::lround(x)));
    if (!m_document.selected().isEmpty() && m_scene->itemOf(m_document.selected(), QString())) {
        ensureShown(m_document.selected());
    }
}

void TimelineAxisView::zoomBy(int steps) {
    zoomTo(m_document.timeline().view().zoom + steps,
           TimelineScene::HEAD_WIDTH + (viewport()->width() - TimelineScene::HEAD_WIDTH) / 2.0);
}

void TimelineAxisView::resetZoom() {
    zoomTo(HOME_ZOOM,
           TimelineScene::HEAD_WIDTH + (viewport()->width() - TimelineScene::HEAD_WIDTH) / 2.0);
}

void TimelineAxisView::ensureShown(const QString& id) {
    if (id.isEmpty()) {
        return;
    }
    QList<const TimelineItem*> items = m_scene->itemsOf(id);
    if (items.isEmpty()) {
        return;  // In the tray, which always shows
    }
    const TimelineItem* item = items.constFirst();
    for (const TimelineItem* each : items) {
        if (each->lane == m_document.selectedLane()) {
            item = each;
        }
    }
    const QRectF& rect = item->rect;
    const double width = viewport()->width() - TimelineScene::HEAD_WIDTH;
    const double height = viewport()->height() - m_scene->lanesTop();
    QScrollBar* across = horizontalScrollBar();
    QScrollBar* down = verticalScrollBar();
    if (rect.left() - SHOW_MARGIN < across->value()) {
        across->setValue(static_cast<int>(rect.left() - SHOW_MARGIN));
    } else if (rect.right() + SHOW_MARGIN > across->value() + width) {
        across->setValue(static_cast<int>(std::min(rect.left() - SHOW_MARGIN,
                                                   rect.right() + SHOW_MARGIN - width)));
    }
    if (rect.top() - SHOW_MARGIN < down->value()) {
        down->setValue(static_cast<int>(rect.top() - SHOW_MARGIN));
    } else if (rect.bottom() + SHOW_MARGIN > down->value() + height) {
        down->setValue(static_cast<int>(rect.bottom() + SHOW_MARGIN - height));
    }
}

// =============================================================================
// The field of a title
// =============================================================================

bool TimelineAxisView::isEditing() const {
    return m_editor != nullptr && m_editor->isVisible();
}

void TimelineAxisView::startEditing(const QString& id, const QString& seed) {
    const TimelineEvent* event = m_document.timeline().event(id);
    const TimelineItem* item = m_scene->itemOf(id, m_document.selectedLane());
    if (event == nullptr || item == nullptr) {
        return;
    }
    if (m_editor == nullptr) {
        m_editor = new QLineEdit(viewport());
        //: The field of the title of an event of a timeline. In Polish: Tytuł wydarzenia
        m_editor->setAccessibleName(tr("Title of the event"));
        m_editor->installEventFilter(this);
    }
    m_document.select(id, item->lane);
    ensureShown(id);
    const bool tray = m_scene->itemsOf(id).isEmpty();
    const QRectF rect = viewRectOf(*item, tray);
    m_editingId = id;
    m_editingLane = item->lane;
    m_editor->setFont(m_scene->titleFont());
    m_editor->setText(seed.isEmpty() ? event->title : seed);
    m_editor->adjustSize();
    m_editor->setGeometry(QRect(static_cast<int>(rect.x()) + EDITOR_INSET,
                                static_cast<int>(rect.y()) + 2,
                                std::max(40, static_cast<int>(rect.width()) - 2 * EDITOR_INSET),
                                m_editor->sizeHint().height()));
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
    if (seed.isEmpty()) {
        m_editor->selectAll();
    } else {
        m_editor->end(false);
    }
}

void TimelineAxisView::finishEditing(bool keep) {
    if (!isEditing()) {
        return;
    }
    const QString id = m_editingId;
    const bool added = m_editingNew;
    const QString text = m_editor->text().trimmed();
    m_editingId.clear();
    m_editingLane.clear();
    m_editingNew = false;
    m_editor->hide();
    setFocus(Qt::OtherFocusReason);
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return;
    }
    if (added && (!keep || text.isEmpty())) {
        // A new event left without a title goes away
        m_document.undo();
        return;
    }
    if (keep && !text.isEmpty() && text != event->title) {
        //: What Undo undoes in a timeline. In Polish: zmiana tytułu „%1”
        m_document.changeEvent(id, tr("changing the title of %1").arg(TimelineEdits::quoted(
                                       event->title)),
                               [&text](TimelineEvent& changed) { changed.title = text; });
    }
}

bool TimelineAxisView::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_editor || !isEditing()) {
        return QAbstractScrollArea::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            finishEditing(true);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            finishEditing(false);
            return true;
        }
    } else if (event->type() == QEvent::FocusOut) {
        const auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() != Qt::PopupFocusReason) {
            finishEditing(true);
        }
    }
    return QAbstractScrollArea::eventFilter(watched, event);
}

bool TimelineAxisView::focusNextPrevChild(bool next) {
    return isEditing() ? false : QAbstractScrollArea::focusNextPrevChild(next);
}

bool TimelineAxisView::typed(const QKeyEvent& key) {
    const Qt::KeyboardModifiers modifiers =
        key.modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier);
    const QString text = key.text();
    return modifiers == Qt::NoModifier && !text.isEmpty() && text.at(0).isPrint() &&
           !text.at(0).isSpace();
}

bool TimelineAxisView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride && !isEditing()) {
        // The keys of the axis go to it before the commands of the window: Delete, Ctrl+Z...
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers =
            key->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
        const int code = key->key();
        const bool plain = modifiers == Qt::NoModifier;
        const bool control = modifiers == Qt::ControlModifier;
        const bool own =
            (plain && (code == Qt::Key_Delete || code == Qt::Key_F2 || code == Qt::Key_Return ||
                       code == Qt::Key_Enter || code == Qt::Key_Insert || code == Qt::Key_Home ||
                       code == Qt::Key_End || code == Qt::Key_Escape || code == Qt::Key_Left ||
                       code == Qt::Key_Right || code == Qt::Key_Up || code == Qt::Key_Down ||
                       code == Qt::Key_F10 || typed(*key))) ||
            (control && (code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_Plus ||
                         code == Qt::Key_Equal || code == Qt::Key_Minus || code == Qt::Key_0 ||
                         code == Qt::Key_Return || code == Qt::Key_Enter ||
                         code == Qt::Key_Left || code == Qt::Key_Right || code == Qt::Key_Up ||
                         code == Qt::Key_Down)) ||
            (modifiers == Qt::AltModifier && (code == Qt::Key_Return || code == Qt::Key_Enter));
        if (own) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const QPointF at = viewport()->mapFrom(this, help->pos());
        bool tray = false;
        const TimelineItem* item = itemAt(at, &tray);
        const TimelineEvent* shown =
            item != nullptr ? m_document.timeline().event(item->id) : nullptr;
        QString tip;
        if (shown != nullptr) {
            tip = shown->title + QStringLiteral(" – ") + m_scene->whenOf(*shown);
            if (!shown->description.isEmpty()) {
                tip += QStringLiteral("\n") + shown->description;
            }
        } else if (at.y() < TimelineScene::AXIS_HEIGHT && at.x() >= TimelineScene::HEAD_WIDTH) {
            const double x = at.x() - TimelineScene::HEAD_WIDTH + horizontalScrollBar()->value();
            for (const TimelineGapMark& gap : m_scene->gaps()) {
                if (x >= gap.x && x <= gap.x + gap.width) {
                    //: In Polish: Przerwa bez wydarzeń: %1
                    tip = tr("A gap without events: %1").arg(gap.length);
                }
            }
        }
        if (tip.isEmpty()) {
            QToolTip::hideText();
            event->ignore();
        } else {
            QToolTip::showText(help->globalPos(), tip, this);
        }
        return true;
    }
    return QAbstractScrollArea::event(event);
}

}  // namespace kalahari::timeline
