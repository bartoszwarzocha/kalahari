// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file schedule_view.cpp
/// @brief The chart of the schedule of a book

#include "kalahari/gui/widgets/schedule_view.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/plot_board.h"

#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace kalahari::gui {

namespace {

/// @brief The width of a day at each scale
constexpr int DAY_PIXELS_DAYS = 30;
constexpr int DAY_PIXELS_WEEKS = 10;
constexpr int DAY_PIXELS_MONTHS = 4;

/// @brief The part of the right edge of a bar that changes its end
constexpr int GRIP_PIXELS = 5;

/// @brief The indent of a text in a part, after the icon
constexpr int INDENT = 18;

/// @brief The space around the texts of a cell
constexpr int PADDING = 6;

/// @brief The size of the icons of the rows
constexpr int ICON_SIZE = 16;

/// @brief Days of the axis before the first and after the last day it shows
constexpr int MARGIN_DAYS = 14;

/// @brief A forecast further than this from today is not on the axis
constexpr int FAR_DAYS = 3 * 366;

/// @brief The words a day that make the tallest mark of a day of writing
constexpr int TALL_TICK_WORDS = 1000;

/// @brief The columns on the left
enum Column { TitleColumn, StatusColumn, WordsColumn, PlanColumn };

QDate mondayOf(const QDate& day)
{
    return day.addDays(1 - day.dayOfWeek());
}

/// @brief @p a over @p b in the share @p share of @p a
QColor blend(const QColor& a, const QColor& b, double share)
{
    const auto mix = [share](int from, int to) { return qRound(to + (from - to) * share); };
    return {mix(a.red(), b.red()), mix(a.green(), b.green()), mix(a.blue(), b.blue())};
}

QDate earlierOf(const QDate& a, const QDate& b)
{
    return !a.isValid() ? b : (!b.isValid() ? a : std::min(a, b));
}

QDate laterOf(const QDate& a, const QDate& b)
{
    return !a.isValid() ? b : (!b.isValid() ? a : std::max(a, b));
}

/// @brief A small rotated square: a deadline
void drawDiamond(QPainter& painter, const QPointF& center, double size, const QColor& fill,
                 const QColor& edge)
{
    const double half = size / 2.0;
    QPainterPath path;
    path.moveTo(center.x(), center.y() - half);
    path.lineTo(center.x() + half, center.y());
    path.lineTo(center.x(), center.y() + half);
    path.lineTo(center.x() - half, center.y());
    path.closeSubpath();
    painter.setPen(QPen(edge, 1.0));
    painter.setBrush(fill);
    painter.drawPath(path);
}

}  // namespace

ScheduleView::ScheduleView(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setObjectName(QStringLiteral("scheduleView"));
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    //: In Polish: Harmonogram: teksty książki na osi czasu
    setAccessibleName(tr("Schedule: the texts of the book on a time axis"));
    refreshColors();
    updateLeftWidth();
    updateAxis();
}

QSize ScheduleView::sizeHint() const
{
    return {m_leftWidth + 60 * DAY_PIXELS_WEEKS, headerHeight() + 12 * rowHeight()};
}

// =============================================================================
// What it shows
// =============================================================================

void ScheduleView::setRows(const QList<ScheduleRow>& rows)
{
    if (rows == m_rows) {
        return;
    }
    const QString kept = m_current >= 0 && m_current < m_rows.size() ? m_rows[m_current].id
                                                                       : QString();
    const bool hadRow = m_current >= 0;
    m_rows = rows;
    int current = hadRow ? rowOf(kept) : -1;
    if (current < 0 && !m_rows.isEmpty()) {
        current = 0;
    }
    m_current = -1;
    updateAxis();
    setCurrentRow(current);
    viewport()->update();
}

void ScheduleView::setPlan(const core::WritingPlan& plan)
{
    if (m_drag.grip != Grip::None) {
        m_drag = {};
    }
    m_plan = plan;
    updateAxis();
    setAccessibleDescription(describe(m_current));
    viewport()->update();
}

void ScheduleView::setFacts(const ScheduleFacts& facts)
{
    m_facts = facts;
    updateAxis();
    setAccessibleDescription(describe(m_current));
    viewport()->update();
}

void ScheduleView::setScale(Scale scale)
{
    if (scale == m_scale) {
        return;
    }
    const int lanes = std::max(1, viewport()->width() - m_leftWidth);
    const QDate middle = dateAt(m_leftWidth + lanes / 2);
    m_scale = scale;
    updateAxis();
    scrollToDay(middle, 0.5);
    viewport()->update();
}

void ScheduleView::setDaysOffShown(bool shown)
{
    m_daysOffShown = shown;
    viewport()->update();
}

void ScheduleView::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    if (readOnly) {
        m_drag = {};
    }
    setAccessibleDescription(describe(m_current));
    viewport()->update();
}

void ScheduleView::setCurrentRow(int row)
{
    const int clamped = m_rows.isEmpty() ? -1 : std::clamp(row, 0, int(m_rows.size()) - 1);
    if (clamped == m_current) {
        return;
    }
    m_current = clamped;
    if (m_current >= 0) {
        // Into sight
        const int top = m_current * rowHeight();
        const int body = std::max(0, viewport()->height() - headerHeight());
        QScrollBar* bar = verticalScrollBar();
        if (top < bar->value()) {
            bar->setValue(top);
        } else if (top + rowHeight() > bar->value() + body) {
            bar->setValue(top + rowHeight() - body);
        }
    }
    setAccessibleDescription(describe(m_current));
    if (hasFocus()) {
        QAccessibleEvent event(this, QAccessible::DescriptionChanged);
        QAccessible::updateAccessibility(&event);
    }
    viewport()->update();
    emit currentChanged(m_current);
}

QString ScheduleView::currentText() const
{
    if (m_current < 0 || m_current >= m_rows.size()
        || m_rows[m_current].kind != ScheduleRow::Kind::Text) {
        return {};
    }
    return m_rows[m_current].id;
}

int ScheduleView::rowOf(const QString& id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].id == id) {
            return i;
        }
    }
    return -1;
}

void ScheduleView::showDate(const QDate& day)
{
    if (!day.isValid()) {
        return;
    }
    // A little before it, so that the days before it are in sight too
    double share = 0.1;
    if (m_scale == Scale::Days) {
        share = 0.2;
    }
    scrollToDay(day, share);
}

void ScheduleView::scrollToDay(const QDate& day, double share)
{
    if (!day.isValid() || !m_first.isValid()) {
        return;
    }
    const int lanes = std::max(0, viewport()->width() - m_leftWidth);
    const int x = m_first.daysTo(day) * dayWidth() - qRound(lanes * share);
    horizontalScrollBar()->setValue(x);
}

QDate ScheduleView::leftDay() const
{
    return m_first.isValid() ? dateAt(m_leftWidth) : QDate();
}

void ScheduleView::refreshColors()
{
    m_colors = StatsColors::current();
    const PlotColors plot = PlotColors::current();
    m_statusColors.clear();
    for (const char* status : {"draft", "revision", "final"}) {
        m_statusColors.insert(QString::fromLatin1(status), plot.ofStatus(QString::fromLatin1(status)));
    }
    m_muted = blend(m_colors.text, m_colors.background, 0.62);
    viewport()->update();
}

// =============================================================================
// The geometry
// =============================================================================

int ScheduleView::dayWidth() const
{
    switch (m_scale) {
    case Scale::Days:
        return DAY_PIXELS_DAYS;
    case Scale::Weeks:
        return DAY_PIXELS_WEEKS;
    case Scale::Months:
        return DAY_PIXELS_MONTHS;
    }
    return DAY_PIXELS_WEEKS;
}

int ScheduleView::headerHeight() const
{
    return 3 * (fontMetrics().height() + 4) + 2;
}

int ScheduleView::rowHeight() const
{
    return fontMetrics().height() + 14;
}

int ScheduleView::xOf(const QDate& day) const
{
    if (!day.isValid() || !m_first.isValid()) {
        return m_leftWidth;
    }
    return m_leftWidth + int(m_first.daysTo(day)) * dayWidth() - horizontalScrollBar()->value();
}

QDate ScheduleView::dateAt(int x) const
{
    if (!m_first.isValid()) {
        return {};
    }
    const int offset = x - m_leftWidth + horizontalScrollBar()->value();
    return m_first.addDays(int(std::floor(double(offset) / dayWidth())));
}

QRect ScheduleView::rowRect(int row) const
{
    return {0, headerHeight() + row * rowHeight() - verticalScrollBar()->value(),
            viewport()->width(), rowHeight()};
}

int ScheduleView::rowAt(int y) const
{
    if (y < headerHeight()) {
        return -1;
    }
    const int row = (y - headerHeight() + verticalScrollBar()->value()) / rowHeight();
    return row >= 0 && row < m_rows.size() ? row : -1;
}

QRect ScheduleView::barRect(int row, Bar bar) const
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].kind != ScheduleRow::Kind::Text) {
        return {};
    }
    const core::TextPlan* plan = m_plan.find(m_rows[row].id);
    if (plan == nullptr) {
        return {};
    }
    const bool writing = bar == Bar::Writing;
    if (writing ? !plan->hasWriting() : !plan->hasRevision()) {
        return {};
    }
    const QDate& start = writing ? plan->start : plan->revisionStart;
    const QDate& end = writing ? plan->end : plan->revisionEnd;
    const QRect rect = rowRect(row);
    const int height = std::max(6, rect.height() - 16);
    const int left = xOf(start);
    return {left, rect.top() + 5, xOf(end.addDays(1)) - left, height};
}

void ScheduleView::updateLeftWidth()
{
    const QFontMetrics metrics = fontMetrics();
    m_columns[TitleColumn] = std::max(metrics.averageCharWidth() * 30, 200);
    int status = 0;
    for (const char* name : {"draft", "revision", "final"}) {
        status = std::max(status,
                          metrics.horizontalAdvance(PlotBoard::statusName(QString::fromLatin1(name))));
    }
    m_columns[StatusColumn] = status + 4 * PADDING;
    m_columns[WordsColumn] =
        metrics.horizontalAdvance(QStringLiteral("%1 / %1").arg(stats_painter::number(888888)))
        + 2 * PADDING;
    const QDate wide(2026, 12, 28);
    m_columns[PlanColumn] =
        metrics.horizontalAdvance(dayText(wide) + QStringLiteral("–") + dayText(wide))
        + 2 * PADDING;
    int width = 0;
    for (int column : m_columns) {
        width += column;
    }
    m_leftWidth = width;
}

void ScheduleView::updateAxis()
{
    if (m_drag.grip != Grip::None) {
        // The axis stays as it is under a bar being dragged
        updateScrollBars();
        return;
    }
    const QDate kept = leftDay();
    QDate low = m_facts.today.isValid() ? m_facts.today : QDate::currentDate();
    QDate high = low;
    const QDate today = low;
    const auto take = [&low, &high](const QDate& day) {
        if (day.isValid()) {
            low = std::min(low, day);
            high = std::max(high, day);
        }
    };
    take(m_plan.firstDay());
    take(m_plan.lastDay());
    take(m_facts.bookDeadline);
    if (m_facts.forecast.isValid() && std::abs(today.daysTo(m_facts.forecast)) <= FAR_DAYS) {
        take(m_facts.forecast);
    }
    for (const core::WritingDeadline& deadline : m_facts.deadlines) {
        take(deadline.date);
    }
    QDate first;
    QDate last;
    switch (m_scale) {
    case Scale::Days:
        first = mondayOf(low.addDays(-MARGIN_DAYS / 2));
        last = mondayOf(high.addDays(MARGIN_DAYS)).addDays(6);
        break;
    case Scale::Weeks:
        first = mondayOf(low.addDays(-MARGIN_DAYS));
        last = mondayOf(high.addDays(2 * MARGIN_DAYS)).addDays(6);
        break;
    case Scale::Months:
        first = QDate(low.year(), low.month(), 1).addMonths(-1);
        last = QDate(high.year(), high.month(), 1).addMonths(2).addDays(-1);
        break;
    }
    // The chart fills the window
    const int lanes = std::max(0, viewport()->width() - m_leftWidth);
    const int days = lanes / dayWidth() + 1;
    if (first.daysTo(last) + 1 < days) {
        last = first.addDays(days - 1);
    }
    m_first = first;
    m_last = last;
    updateScrollBars();
    if (!m_scrolledOnce && lanes > 0 && m_facts.today.isValid()) {
        m_scrolledOnce = true;
        showDate(today);
    } else if (kept.isValid()) {
        horizontalScrollBar()->setValue(int(m_first.daysTo(kept)) * dayWidth());
    }
}

void ScheduleView::updateScrollBars()
{
    const int lanes = std::max(0, viewport()->width() - m_leftWidth);
    const int total = m_first.isValid() ? int(m_first.daysTo(m_last) + 1) * dayWidth() : 0;
    QScrollBar* across = horizontalScrollBar();
    across->setRange(0, std::max(0, total - lanes));
    across->setPageStep(std::max(1, lanes));
    int step = dayWidth();
    if (m_scale == Scale::Weeks) {
        step *= 7;
    } else if (m_scale == Scale::Months) {
        step *= 30;
    }
    across->setSingleStep(step);
    const int body = std::max(0, viewport()->height() - headerHeight());
    QScrollBar* down = verticalScrollBar();
    down->setRange(0, std::max(0, int(m_rows.size()) * rowHeight() - body));
    down->setPageStep(std::max(1, body));
    down->setSingleStep(rowHeight());
}

// =============================================================================
// The plan of the texts
// =============================================================================

core::TextPlan ScheduleView::planOf(const QString& elementId) const
{
    if (const core::TextPlan* plan = m_plan.find(elementId)) {
        return *plan;
    }
    core::TextPlan plan;
    plan.elementId = elementId;
    return plan;
}

void ScheduleView::changePlan(const core::TextPlan& plan)
{
    if (m_readOnly || plan.elementId.isEmpty()) {
        return;
    }
    core::WritingPlan changed = m_plan;
    changed.set(plan);
    if (changed == m_plan) {
        return;
    }
    m_plan = changed;
    updateAxis();
    setAccessibleDescription(describe(m_current));
    viewport()->update();
    emit planEdited(m_plan);
}

void ScheduleView::applyDrag(const QDate& day)
{
    if (m_drag.grip == Grip::None || !day.isValid() || m_drag.row < 0
        || m_drag.row >= m_rows.size()) {
        return;
    }
    const int delta = int(m_drag.pressDay.daysTo(day));
    const core::TextPlan& before = m_drag.before;
    core::TextPlan plan = before;
    const bool writing = m_drag.bar == Bar::Writing;
    switch (m_drag.grip) {
    case Grip::Move:
        if (writing) {
            plan.start = before.start.addDays(delta);
            plan.end = before.end.addDays(delta);
        } else {
            plan.revisionStart = before.revisionStart.addDays(delta);
            plan.revisionEnd = before.revisionEnd.addDays(delta);
        }
        break;
    case Grip::End:
        if (writing) {
            plan.end = std::max(before.start, before.end.addDays(delta));
        } else {
            plan.revisionEnd = std::max(before.revisionStart, before.revisionEnd.addDays(delta));
        }
        break;
    case Grip::Create: {
        const QDate from = std::min(m_drag.pressDay, day);
        const QDate to = std::max(m_drag.pressDay, day);
        if (writing) {
            plan.start = from;
            plan.end = to;
        } else {
            plan.revisionStart = from;
            plan.revisionEnd = to;
        }
        break;
    }
    case Grip::None:
        break;
    }
    m_plan.set(plan);
    viewport()->update();
}

std::pair<QDate, QDate> ScheduleView::spanOf(const QString& partId, bool wholeBook) const
{
    QDate first;
    QDate last;
    for (const ScheduleRow& row : m_rows) {
        if (row.kind != ScheduleRow::Kind::Text || (!wholeBook && row.partId != partId)) {
            continue;
        }
        if (const core::TextPlan* plan = m_plan.find(row.id)) {
            first = earlierOf(first, plan->firstDay());
            last = laterOf(last, plan->lastDay());
        }
    }
    return {first, last};
}

std::pair<int, int> ScheduleView::wordsAndTargetOf(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {0, 0};
    }
    const ScheduleRow& shown = m_rows[row];
    if (shown.kind == ScheduleRow::Kind::Text) {
        const core::TextPlan* plan = m_plan.find(shown.id);
        return {shown.words, plan != nullptr ? plan->targetWords : 0};
    }
    if (shown.kind == ScheduleRow::Kind::Book) {
        return {shown.words, m_facts.bookTargetWords};
    }
    int target = 0;
    for (const ScheduleRow& text : m_rows) {
        if (text.kind == ScheduleRow::Kind::Text && text.partId == shown.id) {
            if (const core::TextPlan* plan = m_plan.find(text.id)) {
                target += plan->targetWords;
            }
        }
    }
    return {shown.words, target};
}

bool ScheduleView::isLate(int row, Bar bar) const
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].kind != ScheduleRow::Kind::Text
        || !m_facts.today.isValid()) {
        return false;
    }
    const ScheduleRow& text = m_rows[row];
    const core::TextPlan* plan = m_plan.find(text.id);
    if (plan == nullptr || text.status == QLatin1String("final")) {
        return false;
    }
    if (bar == Bar::Writing) {
        // Written: in revision, or as long as planned
        return plan->hasWriting() && plan->end < m_facts.today
               && text.status == QLatin1String("draft")
               && (plan->targetWords <= 0 || text.words < plan->targetWords);
    }
    return plan->hasRevision() && plan->revisionEnd < m_facts.today;
}

QList<core::WritingDeadline> ScheduleView::deadlinesOf(int row) const
{
    QList<core::WritingDeadline> found;
    if (row < 0 || row >= m_rows.size()) {
        return found;
    }
    const ScheduleRow& shown = m_rows[row];
    for (const core::WritingDeadline& deadline : m_facts.deadlines) {
        if (!deadline.date.isValid()) {
            continue;
        }
        // The deadlines of the whole book, and of parts not shown, on the row of the book
        const bool ofBook = deadline.elementId.isEmpty() || rowOf(deadline.elementId) < 0;
        if (shown.kind == ScheduleRow::Kind::Book ? ofBook : deadline.elementId == shown.id) {
            found.append(deadline);
        }
    }
    return found;
}

// =============================================================================
// In words
// =============================================================================

QString ScheduleView::dayText(const QDate& day, bool withYear)
{
    if (!day.isValid()) {
        return QStringLiteral("–");
    }
    return QLocale().toString(day, withYear ? QStringLiteral("d MMM yyyy") : QStringLiteral("d MMM"));
}

QString ScheduleView::describe(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    const ScheduleRow& shown = m_rows[row];
    const auto range = [](const QDate& from, const QDate& to) {
        return dayText(from) + QStringLiteral("–") + dayText(to, true);
    };
    QStringList parts;
    const auto [words, target] = wordsAndTargetOf(row);
    const QString wordsText =
        target > 0
            //: %1: words written, %2: the words planned. In Polish: %1 z %2
            ? tr("%1 of %2").arg(stats_painter::number(words), stats_painter::words(target))
            : stats_painter::words(words);

    if (shown.kind == ScheduleRow::Kind::Book) {
        parts << wordsText;
        if (m_facts.bookDeadline.isValid()) {
            //: In Polish: termin %1
            parts << tr("deadline %1").arg(dayText(m_facts.bookDeadline, true));
        }
        if (m_facts.forecast.isValid()) {
            //: In Polish: w obecnym tempie koniec około %1
            parts << tr("at the present pace done about %1").arg(dayText(m_facts.forecast, true));
        }
        const auto [first, last] = spanOf(QString(), true);
        if (first.isValid()) {
            //: In Polish: plan %1
            parts << tr("planned %1").arg(range(first, last));
        }
    } else if (shown.kind == ScheduleRow::Kind::Part) {
        int texts = 0;
        for (const ScheduleRow& text : m_rows) {
            texts += text.kind == ScheduleRow::Kind::Text && text.partId == shown.id ? 1 : 0;
        }
        parts << PlotBoard::textsText(texts) << wordsText;
        const auto [first, last] = spanOf(shown.id, false);
        if (first.isValid()) {
            parts << tr("planned %1").arg(range(first, last));
        }
    } else {
        const core::TextPlan plan = planOf(shown.id);
        if (!plan.hasWriting() && !plan.hasRevision()) {
            parts << wordsText;
            parts << (m_readOnly
                          //: In Polish: bez planu
                          ? tr("no plan")
                          //: In Polish: bez planu; Enter albo przeciągnięcie w wierszu tworzy plan
                          : tr("no plan; Enter or a drag in its row plans it"));
        } else {
            if (plan.hasWriting()) {
                const int days = core::writingDaysBetween(m_facts.rules, plan.start, plan.end);
                //: %1: the days. In Polish: pisanie %1 (%n dzień pisania / dni pisania)
                parts << tr("writing %1 (%n writing days)", nullptr, days)
                             .arg(range(plan.start, plan.end));
            }
            parts << wordsText;
            if (shown.status == QLatin1String("final")) {
                //: The state of a text. In Polish: gotowy
                parts << tr("final");
            } else if (isLate(row, Bar::Writing)) {
                parts << (target > words
                              //: In Polish: po terminie, brakuje %1
                              ? tr("late, %1 missing").arg(stats_painter::words(target - words))
                              //: In Polish: po terminie
                              : tr("late"));
            } else if (plan.hasWriting() && m_facts.today.isValid() && plan.start > m_facts.today) {
                //: In Polish: zaczyna się za %n dzień / dni
                parts << tr("starts in %n days", nullptr, int(m_facts.today.daysTo(plan.start)));
            } else if (plan.hasWriting() && m_facts.today.isValid() && plan.end >= m_facts.today
                       && target > words && shown.status == QLatin1String("draft")) {
                const int left = target - words;
                const int days = core::writingDaysBetween(m_facts.rules, m_facts.today, plan.end);
                //: %1: the words left, %2: the words of a writing day. In Polish: zostało %1,
                //: %2 na dzień pisania
                parts << tr("%1 left, %2 a writing day")
                             .arg(stats_painter::words(left),
                                  stats_painter::words(core::wordsPerWritingDay(left, days)));
            }
            if (plan.hasRevision()) {
                const int days =
                    core::writingDaysBetween(m_facts.rules, plan.revisionStart, plan.revisionEnd);
                //: %1: the days. In Polish: poprawki %1 (%n dzień pisania / dni pisania)
                QString revision = tr("revisions %1 (%n writing days)", nullptr, days)
                                       .arg(range(plan.revisionStart, plan.revisionEnd));
                if (isLate(row, Bar::Revision)) {
                    //: In Polish: %1, po terminie
                    revision = tr("%1, late").arg(revision);
                }
                parts << revision;
            }
        }
        int writtenDays = 0;
        const auto found = m_facts.written.constFind(shown.id);
        if (found != m_facts.written.constEnd()) {
            for (const int day : *found) {
                writtenDays += day > 0 ? 1 : 0;
            }
        }
        if (writtenDays > 0) {
            //: In Polish: pisany %n dzień / dni
            parts << tr("written on %n days", nullptr, writtenDays);
        }
    }
    for (const core::WritingDeadline& deadline : deadlinesOf(row)) {
        //: %1: the name of a deadline, %2: its day. In Polish: termin „%1” %2
        parts << tr("deadline “%1” %2").arg(deadline.name, dayText(deadline.date, true));
    }
    //: %1: the title of a text, a part or the book, %2: what the schedule has of it. In
    //: Polish: %1: %2.
    return tr("%1: %2.").arg(shown.title, parts.join(QStringLiteral(", ")));
}

// =============================================================================
// Painting
// =============================================================================

void ScheduleView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(viewport());
    const QRect all = viewport()->rect();
    painter.fillRect(all, m_colors.background);
    const QRect body(0, headerHeight(), all.width(), std::max(0, all.height() - headerHeight()));
    const QRect lanes(m_leftWidth, body.top(), std::max(0, all.width() - m_leftWidth),
                      body.height());

    // The rows: the chosen one, the lanes with their bars
    painter.save();
    painter.setClipRect(body);
    const int firstRow = std::max(0, verticalScrollBar()->value() / rowHeight());
    for (int row = firstRow; row < m_rows.size(); ++row) {
        const QRect rect = rowRect(row);
        if (rect.top() > body.bottom()) {
            break;
        }
        if (row == m_current) {
            QColor selection = palette().color(QPalette::Highlight);
            selection.setAlphaF(hasFocus() ? 0.28F : 0.14F);
            painter.fillRect(rect, selection);
        }
    }
    painter.setClipRect(lanes);
    paintDaysOff(painter, lanes);
    for (int row = firstRow; row < m_rows.size(); ++row) {
        const QRect rect = rowRect(row);
        if (rect.top() > body.bottom()) {
            break;
        }
        paintLane(painter, row);
        painter.setPen(QPen(m_colors.grid, 1.0));
        painter.drawLine(QPoint(rect.left(), rect.bottom()), QPoint(rect.right(), rect.bottom()));
    }
    paintLines(painter, lanes);

    // The list on the left over the chart
    painter.setClipRect(QRect(0, body.top(), m_leftWidth, body.height()));
    painter.fillRect(QRect(0, body.top(), m_leftWidth, body.height()), m_colors.background);
    for (int row = firstRow; row < m_rows.size(); ++row) {
        const QRect rect = rowRect(row);
        if (rect.top() > body.bottom()) {
            break;
        }
        if (row == m_current) {
            QColor selection = palette().color(QPalette::Highlight);
            selection.setAlphaF(hasFocus() ? 0.28F : 0.14F);
            painter.fillRect(QRect(0, rect.top(), m_leftWidth, rect.height()), selection);
        }
        paintLeft(painter, row);
        painter.setPen(QPen(m_colors.grid, 1.0));
        painter.drawLine(QPoint(0, rect.bottom()), QPoint(m_leftWidth, rect.bottom()));
    }
    painter.restore();

    paintHeader(painter);
    painter.setPen(QPen(m_colors.frame, 1.0));
    painter.drawLine(QPoint(m_leftWidth, 0), QPoint(m_leftWidth, all.bottom()));
    painter.drawLine(QPoint(0, body.top() - 1), QPoint(all.right(), body.top() - 1));

    // The chosen row has the focus
    if (hasFocus() && m_current >= 0) {
        const QRect rect = rowRect(m_current).adjusted(0, 0, -1, -1);
        if (rect.bottom() >= body.top()) {
            painter.setClipRect(body);
            painter.setPen(QPen(palette().color(QPalette::Highlight), 1.0, Qt::DotLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect);
        }
    }
}

void ScheduleView::paintDaysOff(QPainter& painter, const QRect& body) const
{
    if (!m_daysOffShown || m_scale == Scale::Months || !m_first.isValid()) {
        return;
    }
    QColor shade = m_colors.grid;
    shade.setAlphaF(0.45F);
    const QDate from = dateAt(body.left());
    const QDate to = dateAt(body.right());
    for (QDate day = from; day <= to; day = day.addDays(1)) {
        if (!m_facts.rules.isWritingDay(day)) {
            painter.fillRect(QRect(xOf(day), body.top(), dayWidth(), body.height()), shade);
        }
    }
    // The first days of the months
    painter.setPen(QPen(m_colors.grid, 1.0));
    for (QDate day = QDate(from.year(), from.month(), 1); day <= to; day = day.addMonths(1)) {
        if (day >= from) {
            painter.drawLine(QPoint(xOf(day), body.top()), QPoint(xOf(day), body.bottom()));
        }
    }
}

void ScheduleView::paintLeft(QPainter& painter, int row) const
{
    const ScheduleRow& shown = m_rows[row];
    const QRect rect = rowRect(row);
    const QFontMetrics metrics = fontMetrics();
    int x = PADDING;
    if (shown.kind == ScheduleRow::Kind::Text && !shown.partId.isEmpty()) {
        x += INDENT;
    }
    // The icon and the title
    if (!shown.iconId.isEmpty()) {
        const QIcon icon = core::ArtProvider::getInstance().getIcon(shown.iconId);
        icon.paint(&painter, QRect(x, rect.center().y() - ICON_SIZE / 2, ICON_SIZE, ICON_SIZE));
    }
    x += ICON_SIZE + PADDING;
    QFont font = painter.font();
    font.setBold(shown.kind == ScheduleRow::Kind::Part);
    painter.setFont(font);
    painter.setPen(shown.kind == ScheduleRow::Kind::Book ? m_muted : m_colors.text);
    const int titleWidth = m_columns[TitleColumn] - x - PADDING;
    painter.drawText(QRect(x, rect.top(), titleWidth, rect.height()),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(font).elidedText(shown.title, Qt::ElideRight, titleWidth));
    font.setBold(false);
    painter.setFont(font);
    x = m_columns[TitleColumn];

    // The state of a text
    if (shown.kind == ScheduleRow::Kind::Text && !shown.status.isEmpty()) {
        const QColor color = m_statusColors.value(shown.status, m_muted);
        const QString name = PlotBoard::statusName(shown.status);
        const int width = metrics.horizontalAdvance(name) + 2 * PADDING;
        const QRect chip(x + PADDING / 2, rect.center().y() - metrics.height() / 2 - 1, width,
                         metrics.height() + 2);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(color, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(chip).adjusted(0.5, 0.5, -0.5, -0.5), chip.height() / 2.0,
                                chip.height() / 2.0);
        painter.drawText(chip, Qt::AlignCenter, name);
        painter.restore();
    }
    x += m_columns[StatusColumn];

    // The words and the plan
    const auto [words, target] = wordsAndTargetOf(row);
    QString wordsText = stats_painter::number(words);
    if (target > 0) {
        wordsText += QStringLiteral(" / ") + stats_painter::number(target);
    }
    painter.setPen(m_colors.text);
    painter.drawText(QRect(x, rect.top(), m_columns[WordsColumn] - PADDING, rect.height()),
                     Qt::AlignVCenter | Qt::AlignRight, wordsText);
    x += m_columns[WordsColumn];
    QDate first;
    QDate last;
    if (shown.kind == ScheduleRow::Kind::Text) {
        const core::TextPlan plan = planOf(shown.id);
        first = plan.hasWriting() ? plan.start : plan.revisionStart;
        last = plan.hasWriting() ? plan.end : plan.revisionEnd;
    } else {
        std::tie(first, last) = spanOf(shown.id, shown.kind == ScheduleRow::Kind::Book);
    }
    const QString planText = first.isValid() && last.isValid()
                                 ? dayText(first) + QStringLiteral("–") + dayText(last)
                                 : QStringLiteral("–");
    painter.setPen(first.isValid() ? m_colors.text : m_muted);
    painter.drawText(QRect(x, rect.top(), m_columns[PlanColumn] - PADDING, rect.height()),
                     Qt::AlignVCenter | Qt::AlignRight, planText);
}

void ScheduleView::paintLane(QPainter& painter, int row) const
{
    const ScheduleRow& shown = m_rows[row];
    const QRect rect = rowRect(row);
    if (shown.kind != ScheduleRow::Kind::Text) {
        // The span of the plans of its texts: a thin bar with points at its ends
        const auto [first, last] = spanOf(shown.id, shown.kind == ScheduleRow::Kind::Book);
        if (first.isValid() && last.isValid()) {
            const int left = xOf(first);
            const int right = xOf(last.addDays(1));
            const int top = rect.center().y() - 3;
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_colors.frame);
            painter.drawRect(QRect(left, top, right - left, 6));
            for (const int x : {left, right}) {
                QPainterPath point;
                point.moveTo(x - 5, top + 6);
                point.lineTo(x + 5, top + 6);
                point.lineTo(x, top + 11);
                point.closeSubpath();
                painter.drawPath(point);
            }
            painter.restore();
        }
    } else {
        paintTextBars(painter, row);
    }
    paintDeadlines(painter, row);
}

void ScheduleView::paintTextBars(QPainter& painter, int row) const
{
    const ScheduleRow& text = m_rows[row];
    const QRect rect = rowRect(row);
    const core::TextPlan plan = planOf(text.id);
    QFont small = painter.font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    const QFontMetrics smallMetrics(small);
    painter.save();
    painter.setFont(small);
    if (!plan.hasWriting() && !plan.hasRevision()) {
        QFont italic = small;
        italic.setItalic(true);
        painter.setFont(italic);
        painter.setPen(m_muted);
        QString hint;
        if (m_readOnly) {
            hint = tr("no plan");
        } else {
            //: In Polish: bez planu – przeciągnij w wierszu albo naciśnij Enter
            hint = tr("no plan – drag in the row or press Enter");
        }
        painter.drawText(QRect(m_leftWidth + 2 * PADDING, rect.top(), 2000, rect.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, hint);
    }
    painter.setRenderHint(QPainter::Antialiasing);
    const bool final = text.status == QLatin1String("final");
    int labelLeft = 0;
    // Writing: the bar with the share of its words written
    const QRect writing = barRect(row, Bar::Writing);
    if (!writing.isEmpty()) {
        const bool late = isLate(row, Bar::Writing);
        const QColor edge = late ? m_colors.planLate : (final ? m_colors.done : m_colors.line);
        const QRectF bar = QRectF(writing).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(edge, late ? 2.0 : 1.0));
        painter.setBrush(m_colors.planBar);
        painter.drawRoundedRect(bar, 3.0, 3.0);
        double share = 0.0;
        if (plan.targetWords > 0) {
            share = std::clamp(double(text.words) / plan.targetWords, 0.0, 1.0);
        } else if (final) {
            share = 1.0;
        }
        if (share > 0.0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(final ? m_colors.done : m_colors.line);
            painter.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * share,
                                           bar.height()),
                                    3.0, 3.0);
        }
        labelLeft = writing.right() + PADDING;
    }
    // Revisions: a hatched bar
    const QRect revision = barRect(row, Bar::Revision);
    if (!revision.isEmpty()) {
        const bool late = isLate(row, Bar::Revision);
        const QRectF bar = QRectF(revision).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor under = m_colors.planRevision;
        under.setAlphaF(0.25F);
        painter.setPen(Qt::NoPen);
        painter.setBrush(under);
        painter.drawRoundedRect(bar, 3.0, 3.0);
        painter.setBrush(QBrush(m_colors.planRevision, Qt::BDiagPattern));
        painter.drawRoundedRect(bar, 3.0, 3.0);
        painter.setPen(QPen(late ? m_colors.planLate : m_colors.planRevision, late ? 2.0 : 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bar, 3.0, 3.0);
        labelLeft = std::max(labelLeft, revision.right() + PADDING);
    }
    // The words after the bars
    if (labelLeft > 0) {
        const bool late = isLate(row, Bar::Writing);
        QString label;
        if (late && plan.targetWords > text.words) {
            //: After the bar of a text past its end. In Polish: po terminie, brakuje %1
            label = tr("late, %1 missing")
                        .arg(stats_painter::number(plan.targetWords - text.words));
        } else if (late || isLate(row, Bar::Revision)) {
            label = tr("late");
        } else if (m_scale != Scale::Months) {
            label = plan.targetWords > 0 ? QStringLiteral("%1 / %2").arg(
                                               stats_painter::number(text.words),
                                               stats_painter::number(plan.targetWords))
                                         : stats_painter::words(text.words);
        }
        if (!label.isEmpty()) {
            QFont labelFont = small;
            labelFont.setBold(late || isLate(row, Bar::Revision));
            painter.setFont(labelFont);
            painter.setPen(labelFont.bold() ? m_colors.planLate : m_muted);
            painter.drawText(QRect(labelLeft, rect.top() + 3, 2000, smallMetrics.height() + 2),
                             Qt::AlignVCenter | Qt::AlignLeft, label);
        }
    }
    // The days it was written on: marks under the bars, taller for more words
    const auto found = m_facts.written.constFind(text.id);
    if (found != m_facts.written.constEnd() && m_first.isValid()) {
        const int width = std::clamp(dayWidth() - 1, 2, 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_colors.done);
        for (auto it = found->constBegin(); it != found->constEnd(); ++it) {
            if (it.value() <= 0 || it.key() < m_first || it.key() > m_last) {
                continue;
            }
            const int height = std::clamp(2 + it.value() * 4 / TALL_TICK_WORDS, 2, 6);
            const int x = xOf(it.key()) + std::max(0, dayWidth() / 2 - width / 2);
            painter.drawRect(QRect(x, rect.bottom() - 1 - height, width, height));
        }
    }
    painter.restore();
}

void ScheduleView::paintDeadlines(QPainter& painter, int row) const
{
    const QList<core::WritingDeadline> deadlines = deadlinesOf(row);
    if (deadlines.isEmpty()) {
        return;
    }
    const QRect rect = rowRect(row);
    QFont small = painter.font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    painter.save();
    painter.setFont(small);
    painter.setRenderHint(QPainter::Antialiasing);
    const double size = std::min(12.0, rect.height() - 14.0);
    for (const core::WritingDeadline& deadline : deadlines) {
        const QPointF center(xOf(deadline.date) + dayWidth() / 2.0, rect.center().y());
        drawDiamond(painter, center, size, m_colors.goal, m_colors.background);
        painter.setPen(m_colors.text);
        painter.drawText(QRect(int(center.x() + size), rect.top(), 2000, rect.height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QStringLiteral("%1 – %2").arg(deadline.name, dayText(deadline.date)));
    }
    painter.restore();
}

void ScheduleView::paintLines(QPainter& painter, const QRect& body) const
{
    const int bottom = std::min(body.bottom(), headerHeight() + int(m_rows.size()) * rowHeight()
                                                   - verticalScrollBar()->value());
    const auto line = [&](const QDate& day, bool atEnd, const QPen& pen) {
        if (!day.isValid() || day < m_first || day > m_last) {
            return;
        }
        const int x = atEnd ? xOf(day.addDays(1)) : xOf(day);
        painter.setPen(pen);
        painter.drawLine(QPoint(x, body.top()), QPoint(x, bottom));
    };
    line(m_facts.forecast, true, QPen(m_muted, 2.0, Qt::DotLine));
    line(m_facts.bookDeadline, true, QPen(m_colors.goal, 2.0, Qt::DashLine));
    line(m_facts.today, false, QPen(m_colors.planToday, 2.0));
}

void ScheduleView::paintHeader(QPainter& painter) const
{
    const QFontMetrics metrics = fontMetrics();
    const int line = metrics.height() + 4;
    const QRect header(0, 0, viewport()->width(), headerHeight());
    painter.fillRect(header, m_colors.background);
    painter.save();
    const QRect scale(m_leftWidth, 0, std::max(0, header.width() - m_leftWidth), header.height());
    painter.setClipRect(scale);
    const QLocale locale;
    if (m_first.isValid()) {
        const QDate from = dateAt(scale.left());
        const QDate to = dateAt(scale.right());
        // The months, with the year in January and at the left
        QFont bold = painter.font();
        bold.setBold(true);
        for (QDate month = QDate(from.year(), from.month(), 1); month <= to;
             month = month.addMonths(1)) {
            const int x = std::max(xOf(month), scale.left());
            QString name = locale.standaloneMonthName(month.month());
            if (month.month() == 1 || month <= from) {
                name += QLatin1Char(' ') + QString::number(month.year());
            }
            painter.setPen(QPen(m_colors.grid, 1.0));
            painter.drawLine(QPoint(xOf(month), 2), QPoint(xOf(month), 2 * line));
            // Up to the next month: the month at the left may have only a few days in sight
            const int room = xOf(month.addMonths(1)) - x - 8;
            const QFontMetrics boldMetrics(bold);
            if (room < boldMetrics.averageCharWidth() * 3) {
                continue;
            }
            painter.setFont(bold);
            painter.setPen(m_colors.text);
            painter.drawText(QRect(x + 4, 2, room, line), Qt::AlignVCenter | Qt::AlignLeft,
                             boldMetrics.elidedText(name, Qt::ElideRight, room));
        }
        painter.setFont(font());
        // The days, the weeks
        for (QDate day = from; day <= to; day = day.addDays(1)) {
            const int x = xOf(day);
            if (m_scale == Scale::Days) {
                painter.setPen(m_facts.rules.isWritingDay(day) ? m_colors.text : m_muted);
                painter.drawText(QRect(x, line + 2, dayWidth(), line), Qt::AlignCenter,
                                 QString::number(day.day()));
            } else if (day.dayOfWeek() == 1) {
                painter.setPen(QPen(m_colors.grid, 1.0));
                painter.drawLine(QPoint(x, line + 4), QPoint(x, 2 * line));
                if (m_scale == Scale::Weeks) {
                    painter.setPen(m_muted);
                    painter.drawText(QRect(x + 3, line + 2, 7 * dayWidth(), line),
                                     Qt::AlignVCenter | Qt::AlignLeft,
                                     QString::number(day.day()));
                }
            }
        }
        // Today, the deadline and the forecast, as labels over their lines
        struct Label {
            QDate day;
            bool atEnd;
            QColor color;
            QString text;
        };
        const QList<Label> labels{
            //: Over the line of the forecast of the end of the book. In Polish: prognoza
            {m_facts.forecast, true, m_muted, tr("forecast")},
            //: Over the line of the deadline of the book. In Polish: termin
            {m_facts.bookDeadline, true, m_colors.goal, tr("deadline")},
            //: Over the line of today. In Polish: dziś
            {m_facts.today, false, m_colors.planToday, tr("today")},
        };
        painter.setRenderHint(QPainter::Antialiasing);
        for (const Label& label : labels) {
            if (!label.day.isValid() || label.day < m_first || label.day > m_last) {
                continue;
            }
            const int x = label.atEnd ? xOf(label.day.addDays(1)) : xOf(label.day);
            const int width = metrics.horizontalAdvance(label.text) + 2 * PADDING;
            const QRect pill(x - width / 2, 2 * line + 1, width, line - 2);
            painter.setPen(Qt::NoPen);
            painter.setBrush(label.color);
            painter.drawRoundedRect(pill, 3, 3);
            painter.setPen(m_colors.background);
            painter.drawText(pill, Qt::AlignCenter, label.text);
        }
    }
    painter.restore();

    // The captions of the columns
    painter.save();
    QFont bold = painter.font();
    bold.setBold(true);
    painter.setFont(bold);
    painter.setPen(m_muted);
    const QRect captions(0, header.bottom() - line, m_leftWidth, line);
    int x = PADDING;
    //: The column of the texts of the schedule. In Polish: Tekst
    painter.drawText(QRect(x, captions.top(), m_columns[TitleColumn], line),
                     Qt::AlignVCenter | Qt::AlignLeft, tr("Text"));
    x = m_columns[TitleColumn];
    //: In Polish: Stan
    painter.drawText(QRect(x + PADDING / 2, captions.top(), m_columns[StatusColumn], line),
                     Qt::AlignVCenter | Qt::AlignLeft, tr("State"));
    x += m_columns[StatusColumn];
    //: In Polish: Słowa
    painter.drawText(QRect(x, captions.top(), m_columns[WordsColumn] - PADDING, line),
                     Qt::AlignVCenter | Qt::AlignRight, tr("Words"));
    x += m_columns[WordsColumn];
    //: In Polish: Plan
    painter.drawText(QRect(x, captions.top(), m_columns[PlanColumn] - PADDING, line),
                     Qt::AlignVCenter | Qt::AlignRight, tr("Plan"));
    painter.restore();
}

// =============================================================================
// The mouse and the keys
// =============================================================================

void ScheduleView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const QPoint pos = event->position().toPoint();
    const int row = rowAt(pos.y());
    if (row < 0) {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    setCurrentRow(row);
    if (pos.x() < m_leftWidth || m_readOnly || m_rows[row].kind != ScheduleRow::Kind::Text) {
        return;
    }
    Drag drag;
    drag.row = row;
    drag.pressDay = dateAt(pos.x());
    drag.before = planOf(m_rows[row].id);
    // The ends first: a short bar is mostly its end
    for (const Bar bar : {Bar::Revision, Bar::Writing}) {
        const QRect rect = barRect(row, bar);
        if (drag.grip == Grip::None && !rect.isEmpty()
            && std::abs(pos.x() - (rect.right() + 1)) <= GRIP_PIXELS) {
            drag.grip = Grip::End;
            drag.bar = bar;
        }
    }
    for (const Bar bar : {Bar::Revision, Bar::Writing}) {
        const QRect rect = barRect(row, bar);
        if (drag.grip == Grip::None && !rect.isEmpty() && pos.x() >= rect.left()
            && pos.x() <= rect.right()) {
            drag.grip = Grip::Move;
            drag.bar = bar;
        }
    }
    if (drag.grip == Grip::None) {
        // A drag in the empty row plans writing, or revisions after it
        if (!drag.before.hasWriting()) {
            drag.grip = Grip::Create;
            drag.bar = Bar::Writing;
        } else if (!drag.before.hasRevision()) {
            drag.grip = Grip::Create;
            drag.bar = Bar::Revision;
        }
    }
    m_drag = drag;
    m_pressPos = pos;
}

void ScheduleView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (m_drag.grip != Grip::None) {
        if (!m_drag.moved
            && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_drag.moved = true;
        }
        if (m_drag.moved) {
            applyDrag(dateAt(pos.x()));
        }
        return;
    }
    // What a press would take hold of
    Qt::CursorShape shape = Qt::ArrowCursor;
    const int row = rowAt(pos.y());
    if (!m_readOnly && row >= 0 && pos.x() >= m_leftWidth) {
        for (const Bar bar : {Bar::Revision, Bar::Writing}) {
            const QRect rect = barRect(row, bar);
            if (rect.isEmpty()) {
                continue;
            }
            if (std::abs(pos.x() - (rect.right() + 1)) <= GRIP_PIXELS) {
                shape = Qt::SizeHorCursor;
                break;
            }
            if (pos.x() >= rect.left() && pos.x() <= rect.right()) {
                shape = Qt::OpenHandCursor;
            }
        }
    }
    viewport()->setCursor(shape);
}

void ScheduleView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_drag.grip == Grip::None) {
        QAbstractScrollArea::mouseReleaseEvent(event);
        return;
    }
    const Drag drag = m_drag;
    if (drag.moved) {
        applyDrag(dateAt(event->position().toPoint().x()));
    }
    m_drag = {};
    const core::TextPlan after = planOf(drag.before.elementId);
    if (!drag.moved || after == drag.before) {
        // A click: nothing changes
        m_plan.set(drag.before);
        viewport()->update();
        return;
    }
    // One change for the whole drag, from the plan before it
    m_plan.set(drag.before);
    changePlan(after);
}

void ScheduleView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint().y());
    if (event->button() == Qt::LeftButton && row >= 0
        && m_rows[row].kind == ScheduleRow::Kind::Text) {
        m_drag = {};
        setCurrentRow(row);
        emit editRequested(m_rows[row].id);
        return;
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void ScheduleView::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool alt = modifiers.testFlag(Qt::AltModifier);
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const QString text = currentText();
    const int page = std::max(1, (viewport()->height() - headerHeight()) / rowHeight());
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_drag.grip != Grip::None) {
            m_plan.set(m_drag.before);
            m_drag = {};
            viewport()->update();
            return;
        }
        break;
    case Qt::Key_Up:
        setCurrentRow(m_current - 1);
        return;
    case Qt::Key_Down:
        setCurrentRow(m_current + 1);
        return;
    case Qt::Key_PageUp:
        setCurrentRow(m_current - page);
        return;
    case Qt::Key_PageDown:
        setCurrentRow(m_current + page);
        return;
    case Qt::Key_Home:
        if (modifiers == Qt::NoModifier || modifiers == Qt::ControlModifier) {
            setCurrentRow(0);
            return;
        }
        break;
    case Qt::Key_End:
        if (modifiers == Qt::NoModifier || modifiers == Qt::ControlModifier) {
            setCurrentRow(int(m_rows.size()) - 1);
            return;
        }
        break;
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const int step = event->key() == Qt::Key_Left ? -1 : 1;
        if (!alt) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value()
                                            + step * horizontalScrollBar()->singleStep());
            return;
        }
        if (text.isEmpty() || m_readOnly) {
            return;
        }
        core::TextPlan plan = planOf(text);
        if (shift) {
            // The end of writing, or of revisions when only they are planned
            if (plan.hasWriting()) {
                plan.end = std::max(plan.start, plan.end.addDays(step));
            } else if (plan.hasRevision()) {
                plan.revisionEnd = std::max(plan.revisionStart, plan.revisionEnd.addDays(step));
            }
        } else {
            plan.shift(step);
        }
        changePlan(plan);
        return;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!text.isEmpty()) {
            emit editRequested(text);
            return;
        }
        break;
    case Qt::Key_Delete:
        if (!text.isEmpty() && !m_readOnly && m_plan.find(text) != nullptr) {
            core::TextPlan none;
            none.elementId = text;
            changePlan(none);
            return;
        }
        break;
    default:
        break;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void ScheduleView::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    if (m_current < 0 && !m_rows.isEmpty()) {
        setCurrentRow(0);
    }
    viewport()->update();
}

void ScheduleView::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

void ScheduleView::changeEvent(QEvent* event)
{
    QAbstractScrollArea::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::LanguageChange:
        updateLeftWidth();
        updateAxis();
        setAccessibleName(tr("Schedule: the texts of the book on a time axis"));
        setAccessibleDescription(describe(m_current));
        viewport()->update();
        break;
    case QEvent::PaletteChange:
        refreshColors();
        break;
    default:
        break;
    }
}

bool ScheduleView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int row = rowAt(help->pos().y());
        if (row >= 0) {
            QToolTip::showText(help->globalPos(), describe(row), viewport());
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    if (event->type() == QEvent::Resize) {
        const bool result = QAbstractScrollArea::viewportEvent(event);
        updateAxis();
        return result;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void ScheduleView::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dx)
    Q_UNUSED(dy)
    viewport()->update();
}

}  // namespace kalahari::gui
