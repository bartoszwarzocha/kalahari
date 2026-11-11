/// @file writing_stats_panel.cpp
/// @brief The Statistics panel

#include "kalahari/gui/panels/writing_stats_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/writing_goals.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/widgets/badge_painter.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_awards_tracker.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr int DAY_MINUTES = 24 * 60;
/// The times of writing count the last 4 weeks
constexpr int HOURS_DAYS = 28;
/// The narrowest column of the big numbers
constexpr int NUMBER_WIDTH = 84;
/// The gap between the parts of a view
constexpr int GAP = 6;
/// The roles of the items of the list of views
constexpr int SUBTITLE_ROLE = Qt::UserRole + 1;
constexpr int VIEW_ROLE = Qt::UserRole + 2;

/// @brief @p font, @p factor times its size
QFont scaledFont(const QFont& font, double factor)
{
    QFont scaled = font;
    if (scaled.pointSizeF() > 0) {
        scaled.setPointSizeF(std::max(7.0, scaled.pointSizeF() * factor));
    } else if (scaled.pixelSize() > 0) {
        scaled.setPixelSize(std::max(9, int(std::lround(scaled.pixelSize() * factor))));
    }
    return scaled;
}

/// @brief A color between @p from and @p to, @p t of the way
QColor mixed(const QColor& from, const QColor& to, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(float(from.redF() + (to.redF() - from.redF()) * t),
                            float(from.greenF() + (to.greenF() - from.greenF()) * t),
                            float(from.blueF() + (to.blueF() - from.blueF()) * t));
}

/// @brief A clock time of a minute after midnight, which may pass the next midnight
QString clockOf(int minutes)
{
    const int clock = ((minutes % DAY_MINUTES) + DAY_MINUTES) % DAY_MINUTES;
    return QStringLiteral("%1:%2").arg(clock / 60).arg(clock % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

// =============================================================================
// The parts of a view
// =============================================================================

/// @brief What a graph of the panel shows
struct GraphData {
    enum class Kind { None, Day, Columns, Book, Hours, Trophy };
    Kind kind = Kind::None;
    /// Day: the pieces of the day; Columns: the days (NaN: still to come); Book: the words at
    /// the end of each day, up to today
    QList<double> values;
    QList<bool> off;         ///< Columns: the days off
    QList<bool> done;        ///< Columns: the days with the goal reached
    QStringList tips;        ///< The tooltip of each piece or column
    QStringList labels;      ///< Columns: under the columns, when they fit
    double max = 10.0;       ///< The top of the scale
    double goal = 0.0;       ///< Columns: the daily goal; Book: the length of the book
    double now = -1.0;       ///< Day: where now is, 0 to 1 of the width
    int days = 0;            ///< Book: the days of the graph
    int deadline = -1;       ///< Book: the day of the deadline in the graph
    int forecast = -1;       ///< Book: the day the forecast reaches the length (may be later)
    std::array<std::array<double, 24>, 7> heat{};  ///< Hours: Monday first
    QString topLeft;
    QString topRight;
    QString bottomLeft;
    QString bottomRight;
    QStringList rowLabels;   ///< Hours: the days, left of the map
};

namespace {

/// @brief The plot of a graph in @p rect: the large one of a view or the small one of the list
void paintPlot(QPainter& painter, const QRectF& rect, const GraphData& graph,
               const StatsColors& colors, bool small)
{
    painter.save();
    painter.fillRect(rect, colors.background);
    const int rows = small ? 2 : 4;
    const QRectF inner = rect.adjusted(1, 1, -1, -1);
    const auto yOf = [&](double value) {
        return inner.bottom() - std::clamp(value / graph.max, 0.0, 1.0) * inner.height();
    };
    const auto dashed = [&](const QColor& color) {
        QPen pen(color, 1);
        pen.setDashPattern({4, 3});
        return pen;
    };
    painter.setRenderHint(QPainter::Antialiasing, true);
    switch (graph.kind) {
    case GraphData::Kind::None:
        stats_painter::drawGrid(painter, rect, 4, rows, colors.grid);
        break;
    case GraphData::Kind::Trophy: {
        // The cup alone, as on the button of the tab: not a graph
        const double side = std::min(inner.height() - 6.0, 20.0);
        badge_painter::drawTrophy(painter,
                                  QRectF(inner.center().x() - side / 2.0,
                                         inner.center().y() - side / 2.0, side, side),
                                  colors.line);
        break;
    }
    case GraphData::Kind::Day: {
        stats_painter::drawGrid(painter, rect, small ? 4 : 6, rows, colors.grid);
        stats_painter::drawArea(painter, inner, graph.values, graph.max, colors.line,
                                colors.fill());
        if (graph.now >= 0.0 && !small) {
            const double x = inner.left() + graph.now * inner.width();
            painter.setPen(QPen(colors.text, 1));
            painter.drawLine(QPointF(x, inner.top()), QPointF(x, inner.bottom()));
        }
        break;
    }
    case GraphData::Kind::Columns: {
        const auto count = graph.values.size();
        stats_painter::drawGrid(painter, rect, count <= 7 ? int(count) : 6, rows, colors.grid);
        if (count == 0) {
            break;
        }
        const double slot = inner.width() / double(count);
        const double pad = slot * (count <= 7 ? 0.2 : 0.14);
        for (qsizetype i = 0; i < count; ++i) {
            const double value = graph.values[i];
            if (std::isnan(value)) {
                continue;
            }
            const bool off = i < graph.off.size() && graph.off[i];
            const bool done = i < graph.done.size() && graph.done[i];
            const QColor line = done ? colors.done : colors.line;
            QColor fill = line;
            fill.setAlpha(colors.fill().alpha());
            const double left = inner.left() + double(i) * slot + pad;
            const double width = std::max(1.0, slot - 2 * pad);
            if (value <= 0.0) {
                // A day off without writing: a dashed mark at the bottom
                if (off && !small) {
                    painter.setPen(dashed(colors.frame));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(left, inner.bottom() - 3, width, 3));
                }
                continue;
            }
            const QRectF bar(left, yOf(value), width, inner.bottom() - yOf(value));
            painter.setPen(off ? dashed(line) : QPen(line, 1));
            painter.setBrush(fill);
            painter.drawRect(bar.adjusted(0.5, 0.5, -0.5, 0));
        }
        if (graph.goal > 0.0 && !small) {
            painter.setPen(dashed(colors.goal));
            const double y = std::round(yOf(graph.goal)) + 0.5;
            painter.drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
        }
        break;
    }
    case GraphData::Kind::Book: {
        stats_painter::drawGrid(painter, rect, 4, rows, colors.grid);
        const int last = std::max(1, graph.days - 1);
        const auto xOf = [&](double day) { return inner.left() + day / last * inner.width(); };
        if (!graph.values.isEmpty()) {
            const QRectF area(inner.left(), inner.top(),
                              xOf(double(graph.values.size() - 1)) - inner.left(),
                              inner.height());
            stats_painter::drawArea(painter, area, graph.values, graph.max, colors.line,
                                    colors.fill());
        }
        if (small) {
            break;
        }
        if (graph.goal > 0.0) {
            painter.setPen(dashed(colors.goal));
            const double y = std::round(yOf(graph.goal)) + 0.5;
            painter.drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
        }
        if (graph.deadline >= 0) {
            painter.setPen(dashed(colors.goal));
            const double x = std::round(xOf(graph.deadline)) + 0.5;
            painter.drawLine(QPointF(x, inner.top()), QPointF(x, inner.bottom()));
        }
        if (!graph.values.isEmpty()) {
            const double today = double(graph.values.size() - 1);
            const QPointF from(xOf(today), yOf(graph.values.last()));
            if (graph.forecast > today && graph.goal > 0.0) {
                QPointF to(xOf(graph.forecast), yOf(graph.goal));
                if (to.x() > inner.right()) {
                    // Cut where the graph ends
                    const double t = (inner.right() - from.x()) / (to.x() - from.x());
                    to = QPointF(inner.right(), from.y() + (to.y() - from.y()) * t);
                }
                QPen pen(colors.line, 1.2);
                pen.setStyle(Qt::DotLine);
                painter.setPen(pen);
                painter.drawLine(from, to);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.line);
            painter.drawEllipse(from, 2.5, 2.5);
        }
        break;
    }
    case GraphData::Kind::Hours: {
        double max = 0.0;
        for (const auto& day : graph.heat) {
            for (double value : day) {
                max = std::max(max, value);
            }
        }
        const double cell = inner.width() / 24.0;
        const double row = inner.height() / 7.0;
        painter.setRenderHint(QPainter::Antialiasing, false);
        for (int d = 0; d < 7; ++d) {
            for (int h = 0; h < 24; ++h) {
                const double value = graph.heat[d][h];
                if (!(value > 0.0) || !(max > 0.0)) {
                    continue;
                }
                const QRectF box(inner.left() + h * cell, inner.top() + d * row, cell, row);
                painter.fillRect(box.adjusted(0, 0, -0.5, -0.5),
                                 mixed(colors.hoursLow, colors.hoursHigh, value / max));
            }
        }
        painter.setPen(QPen(colors.grid, 1));
        for (int h = 6; h < 24; h += 6) {
            const double x = std::round(inner.left() + h * cell) + 0.5;
            painter.drawLine(QPointF(x, inner.top()), QPointF(x, inner.bottom()));
        }
        break;
    }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(colors.frame, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
}

/// @brief The small graph of an item of the list
QPixmap miniGraph(const GraphData& graph, const StatsColors& colors, qreal ratio)
{
    QPixmap pixmap(QSize(WritingStatsPanel::MINI_WIDTH, WritingStatsPanel::MINI_HEIGHT) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintPlot(painter,
              QRectF(0, 0, WritingStatsPanel::MINI_WIDTH, WritingStatsPanel::MINI_HEIGHT), graph,
              colors, true);
    return pixmap;
}

/// @brief The items of the list of views: the small graph, the name in bold and a line of
/// numbers under it
/// @brief The text of a check box, wrapped when the panel is narrow: a click on it clicks the
/// box, and its mnemonic too (the box is its buddy)
class BoxText : public QLabel {
public:
    BoxText(QAbstractButton* box, QWidget* parent)
        : QLabel(parent)
        , m_box(box)
    {
        setWordWrap(true);
        setBuddy(box);
        setTextInteractionFlags(Qt::NoTextInteraction);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) &&
            m_box->isEnabled()) {
            m_box->setFocus(Qt::MouseFocusReason);
            m_box->click();
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }

private:
    QAbstractButton* m_box;
};

class ViewDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const override
    {
        const int lines = 2 * option.fontMetrics.height();
        return {WritingStatsPanel::MINI_WIDTH * 2,
                std::max(WritingStatsPanel::MINI_HEIGHT, lines) + 8};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        const QWidget* widget = option.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        // The background of the item as the style draws it: chosen, under the mouse, focused
        item.text.clear();
        item.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        const QRect area = option.rect.adjusted(5, 4, -5, -4);
        const QPixmap graph = index.data(Qt::DecorationRole).value<QPixmap>();
        const int graphTop = area.top() + (area.height() - WritingStatsPanel::MINI_HEIGHT) / 2;
        if (!graph.isNull()) {
            painter->drawPixmap(area.left(), graphTop, graph);
        }
        const bool chosen = (option.state & QStyle::State_Selected) != 0;
        const QPalette::ColorGroup group =
            (option.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
        const QColor text = option.palette.color(
            group, chosen ? QPalette::HighlightedText : QPalette::Text);
        const QColor note = chosen ? text : SettingsPage::noteColorOn(option.palette.base().color());
        const int left = area.left() + WritingStatsPanel::MINI_WIDTH + 8;
        const int width = std::max(0, area.right() - left);
        const QFontMetrics metrics(option.font);
        const int lineHeight = metrics.height();
        const int top = area.top() + (area.height() - 2 * lineHeight) / 2;

        painter->save();
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(text);
        const QString title = index.data(Qt::DisplayRole).toString();
        painter->drawText(QRect(left, top, width, lineHeight), Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(title, Qt::ElideRight, width));
        painter->setFont(option.font);
        painter->setPen(note);
        const QString subtitle = index.data(SUBTITLE_ROLE).toString();
        painter->drawText(QRect(left, top + lineHeight, width, lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(subtitle, Qt::ElideRight, width));
        painter->restore();
    }
};

}  // namespace

/// @brief A large graph of a view: its captions, its plot and the texts under it
class StatsGraph : public QWidget {
public:
    explicit StatsGraph(QWidget* parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMouseTracking(true);
    }

    void setGraph(const GraphData& graph, const StatsColors& colors)
    {
        m_graph = graph;
        m_colors = colors;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override { return {200, captionHeight() * 2 + plotHeight()}; }
    QSize minimumSizeHint() const override { return {120, sizeHint().height()}; }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ToolTip) {
            auto* help = static_cast<QHelpEvent*>(event);
            const QString tip = tipAt(help->pos());
            if (tip.isEmpty()) {
                QToolTip::hideText();
            } else {
                QToolTip::showText(help->globalPos(), tip, this);
            }
            return true;
        }
        return QWidget::event(event);
    }

    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QFont small = scaledFont(font(), 0.85);
        painter.setFont(small);
        const QColor note = SettingsPage::noteColor();
        const int caption = captionHeight();
        const QRectF plotRect = plot();
        painter.setPen(note);
        // The captions: what it shows on the left, the top of the scale on the right
        const QFontMetrics metrics(small);
        const int right = metrics.horizontalAdvance(m_graph.topRight);
        painter.drawText(QRect(0, 0, width() - right - 6, caption), Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(m_graph.topLeft, Qt::ElideRight, width() - right - 6));
        painter.drawText(QRect(0, 0, width(), caption), Qt::AlignRight | Qt::AlignVCenter,
                         m_graph.topRight);

        paintPlot(painter, plotRect, m_graph, m_colors, false);

        // The days left of the times of writing
        painter.setPen(note);
        if (m_graph.kind == GraphData::Kind::Hours) {
            const double row = (plotRect.height() - 2) / 7.0;
            for (int d = 0; d < m_graph.rowLabels.size() && d < 7; ++d) {
                painter.drawText(QRectF(0, plotRect.top() + 1 + d * row, plotRect.left() - 4, row),
                                 Qt::AlignRight | Qt::AlignVCenter, m_graph.rowLabels[d]);
            }
        }

        // Under the plot: the labels of the columns when they fit, or the texts at its ends
        const QRectF under(plotRect.left(), plotRect.bottom() + 1, plotRect.width(), caption);
        if (!labelsFit(metrics)) {
            painter.drawText(under, Qt::AlignLeft | Qt::AlignVCenter, m_graph.bottomLeft);
            painter.drawText(under, Qt::AlignRight | Qt::AlignVCenter, m_graph.bottomRight);
            return;
        }
        const double slot = under.width() / double(m_graph.labels.size());
        for (qsizetype i = 0; i < m_graph.labels.size(); ++i) {
            const QString& label = m_graph.labels[i];
            if (m_graph.kind == GraphData::Kind::Hours) {
                // The hours at the lines of the map: 0, 6, 12, 18, 24
                const double x = under.left() + under.width() * double(i) / double(m_graph.labels.size() - 1);
                const int w = metrics.horizontalAdvance(label);
                const double left = std::clamp(x - w / 2.0, under.left(), under.right() - w);
                painter.drawText(QRectF(left, under.top(), w, caption), Qt::AlignCenter, label);
            } else {
                painter.drawText(QRectF(under.left() + double(i) * slot, under.top(), slot, caption),
                                 Qt::AlignCenter, label);
            }
        }
    }

private:
    int captionHeight() const { return QFontMetrics(scaledFont(font(), 0.85)).height() + 2; }
    int plotHeight() const { return WritingStatsPanel::GRAPH_HEIGHT; }

    QRectF plot() const
    {
        int left = 0;
        if (m_graph.kind == GraphData::Kind::Hours) {
            const QFontMetrics metrics(scaledFont(font(), 0.85));
            for (const QString& label : m_graph.rowLabels) {
                left = std::max(left, metrics.horizontalAdvance(label) + 6);
            }
        }
        return QRectF(left, captionHeight(), std::max(10, width() - left), plotHeight());
    }

    bool labelsFit(const QFontMetrics& metrics) const
    {
        if (m_graph.labels.isEmpty()) {
            return false;
        }
        if (m_graph.kind == GraphData::Kind::Hours) {
            return true;
        }
        const double slot = plot().width() / double(m_graph.labels.size());
        return std::all_of(m_graph.labels.begin(), m_graph.labels.end(), [&](const QString& label) {
            return metrics.horizontalAdvance(label) + 2 <= slot;
        });
    }

    QString tipAt(const QPoint& pos) const
    {
        const QRectF rect = plot();
        if (!rect.contains(pos) || m_graph.tips.isEmpty()) {
            return QString();
        }
        if (m_graph.kind == GraphData::Kind::Hours) {
            const int d = std::clamp(int((pos.y() - rect.top()) / rect.height() * 7), 0, 6);
            const int h = std::clamp(int((pos.x() - rect.left()) / rect.width() * 24), 0, 23);
            const qsizetype i = d * 24 + h;
            return i < m_graph.tips.size() ? m_graph.tips[i] : QString();
        }
        const qsizetype count = m_graph.tips.size();
        const auto i = qsizetype((pos.x() - rect.left()) / rect.width() * double(count));
        return m_graph.tips.value(std::clamp<qsizetype>(i, 0, count - 1));
    }

    GraphData m_graph;
    StatsColors m_colors;
};

/// @brief The big numbers of a view, in as many columns as fit
class StatsKpiGrid : public QWidget {
public:
    struct Number {
        QString caption;
        QString value;
        QString unit;
    };

    explicit StatsKpiGrid(QWidget* parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    void setNumbers(const QList<Number>& numbers, const QFont& big)
    {
        qDeleteAll(m_cells);
        m_cells.clear();
        const QString note = SettingsPage::noteColor().name();
        QFont unitFont = scaledFont(font(), 0.9);
        for (const Number& number : numbers) {
            auto* cell = new QWidget(this);
            auto* layout = new QVBoxLayout(cell);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            auto* caption = new QLabel(number.caption, cell);
            caption->setFont(scaledFont(font(), 0.9));
            caption->setStyleSheet(QStringLiteral("color: %1;").arg(note));
            auto* row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(3);
            auto* value = new QLabel(number.value, cell);
            value->setFont(big);
            row->addWidget(value, 0, Qt::AlignBaseline);
            if (!number.unit.isEmpty()) {
                auto* unit = new QLabel(number.unit, cell);
                unit->setFont(unitFont);
                unit->setStyleSheet(QStringLiteral("color: %1;").arg(note));
                row->addWidget(unit, 0, Qt::AlignBaseline);
            }
            row->addStretch();
            layout->addWidget(caption);
            layout->addLayout(row);
            cell->setAccessibleName(QStringLiteral("%1: %2 %3")
                                        .arg(number.caption, number.value, number.unit)
                                        .trimmed());
            cell->show();
            m_cells.append(cell);
        }
        placeCells();
        updateGeometry();
    }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override
    {
        if (m_cells.isEmpty()) {
            return 0;
        }
        const int columns = columnsFor(width);
        const auto rows = (m_cells.size() + columns - 1) / columns;
        return int(rows) * cellHeight() + int(rows - 1) * GAP;
    }
    QSize sizeHint() const override { return {NUMBER_WIDTH * 3, heightForWidth(NUMBER_WIDTH * 3)}; }
    QSize minimumSizeHint() const override
    {
        int width = 0;
        for (const QWidget* cell : m_cells) {
            width = std::max(width, cell->minimumSizeHint().width());
        }
        return {std::min(width, NUMBER_WIDTH * 2), heightForWidth(NUMBER_WIDTH)};
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        placeCells();
    }

private:
    int columnsFor(int width) const
    {
        return std::clamp(width / NUMBER_WIDTH, 1, std::max(1, int(m_cells.size())));
    }
    int cellHeight() const
    {
        int height = 0;
        for (const QWidget* cell : m_cells) {
            height = std::max(height, cell->sizeHint().height());
        }
        return height;
    }
    void placeCells()
    {
        if (m_cells.isEmpty()) {
            return;
        }
        const int columns = columnsFor(width());
        const int cellWidth = (width() - (columns - 1) * GAP * 2) / columns;
        const int height = cellHeight();
        for (qsizetype i = 0; i < m_cells.size(); ++i) {
            const int column = int(i) % columns;
            const int row = int(i) / columns;
            m_cells[i]->setGeometry(column * (cellWidth + GAP * 2), row * (height + GAP),
                                    cellWidth, height);
        }
    }

    QList<QWidget*> m_cells;
};

/// @brief A thin bar of a share: of the weekly goal, of the length of the book
class StatsProgress : public QWidget {
public:
    explicit StatsProgress(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setShare(double share, const StatsColors& colors)
    {
        m_share = std::clamp(share, 0.0, 1.0);
        m_colors = colors;
        setAccessibleDescription(QStringLiteral("%1%").arg(std::lround(m_share * 100)));
        update();
    }
    QSize sizeHint() const override { return {100, 8}; }
    QSize minimumSizeHint() const override { return {20, 8}; }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        const QRectF track = QRectF(rect()).adjusted(0, 1, 0, -1);
        const double radius = track.height() / 2.0;
        painter.setBrush(m_colors.grid);
        painter.drawRoundedRect(track, radius, radius);
        if (m_share > 0.0) {
            painter.setBrush(m_share >= 1.0 ? m_colors.done : m_colors.line);
            painter.drawRoundedRect(QRectF(track.left(), track.top(),
                                           std::max(track.height(), track.width() * m_share),
                                           track.height()),
                                    radius, radius);
        }
    }

private:
    double m_share = 0.0;
    StatsColors m_colors;
};

/// @brief The details of a view: a name and its value on one line, or the value under the
/// name when the panel is too narrow for both
class StatsDetails : public QWidget {
public:
    struct Detail {
        Detail(QString detailName, QString detailValue, const QColor& valueColor = {},
               QString detailToolTip = {})
            : name(std::move(detailName))
            , value(std::move(detailValue))
            , color(valueColor)
            , toolTip(std::move(detailToolTip))
        {
        }

        QString name;
        QString value;
        QColor color;     ///< Of the value; invalid: the color of the text
        QString toolTip;  ///< What the name means, when it needs saying
    };

    explicit StatsDetails(QWidget* parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    void setDetails(const QList<Detail>& details)
    {
        qDeleteAll(m_names);
        qDeleteAll(m_values);
        m_names.clear();
        m_values.clear();
        const QString note = SettingsPage::noteColor().name();
        for (const Detail& detail : details) {
            auto* name = new QLabel(detail.name, this);
            name->setWordWrap(true);
            name->setStyleSheet(QStringLiteral("color: %1;").arg(note));
            auto* value = new QLabel(detail.value, this);
            value->setWordWrap(true);
            value->setAlignment(Qt::AlignRight | Qt::AlignTop);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            if (detail.color.isValid()) {
                value->setStyleSheet(QStringLiteral("color: %1;").arg(detail.color.name()));
            }
            name->setBuddy(value);
            name->setToolTip(detail.toolTip);
            value->setToolTip(detail.toolTip);
            value->setAccessibleName(detail.name);
            value->setAccessibleDescription(detail.toolTip);
            name->show();
            value->show();
            m_names.append(name);
            m_values.append(value);
        }
        place(width(), true);
        updateGeometry();
    }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return const_cast<StatsDetails*>(this)->place(width, false); }
    QSize sizeHint() const override { return {200, heightForWidth(200)}; }
    QSize minimumSizeHint() const override { return {80, heightForWidth(80)}; }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        place(width(), true);
    }

private:
    /// @brief The height the details take in @p width, placing them there when @p move is
    int place(int width, bool move)
    {
        width = std::max(width, 40);
        int y = 0;
        for (qsizetype i = 0; i < m_names.size(); ++i) {
            QLabel* name = m_names[i];
            QLabel* value = m_values[i];
            const int nameWidth = name->fontMetrics().horizontalAdvance(name->text());
            const int valueWidth = value->fontMetrics().horizontalAdvance(value->text()) + 2;
            const int line = std::max(name->fontMetrics().height(), value->fontMetrics().height());
            if (nameWidth + 12 + valueWidth <= width) {
                if (move) {
                    name->setGeometry(0, y, nameWidth + 2, line);
                    value->setGeometry(width - valueWidth, y, valueWidth, line);
                }
                y += line + 2;
                continue;
            }
            // Too narrow for both: the name, and the value under it at the right
            const int nameHeight = name->heightForWidth(width);
            const int valueHeight = value->heightForWidth(width);
            if (move) {
                name->setGeometry(0, y, width, nameHeight);
                value->setGeometry(0, y + nameHeight, width, valueHeight);
            }
            y += nameHeight + valueHeight + 4;
        }
        return std::max(0, y - 2);
    }

    QList<QLabel*> m_names;
    QList<QLabel*> m_values;
};

/// @brief The badge won last, large: its ring, its name, the day it was won and the badges and
/// challenges; a click opens the tab Challenges and Badges
class BigBadgeButton : public QAbstractButton {
public:
    /// @brief The size of the ring
    static constexpr int CIRCLE = 88;

    explicit BigBadgeButton(QWidget* parent)
        : QAbstractButton(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }

    /// @brief What it shows: a badge won (@p won) of @p kind and @p level, or none yet
    void setBadge(bool won, core::BadgeKind kind, int level, const QStringList& lines)
    {
        m_won = won;
        m_kind = kind;
        m_level = level;
        m_lines = lines;
        setAccessibleName(lines.join(QStringLiteral(". ")));
        updateGeometry();
        update();
    }

    /// @brief Its lines: the name in bold, then the smaller ones
    QStringList lines() const { return m_lines; }

    QSize sizeHint() const override { return {200, heightForWidth(200)}; }
    QSize minimumSizeHint() const override { return {100, heightForWidth(100)}; }
    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override
    {
        const int textWidth = std::max(20, width - 2 * PADDING);
        int height = PADDING + circle(width) + 8;
        for (int i = 0; i < m_lines.size(); ++i) {
            const QFontMetrics metrics(fontOf(i));
            height += metrics.boundingRect(QRect(0, 0, textWidth, 10000),
                                           Qt::AlignHCenter | Qt::TextWordWrap, m_lines[i])
                          .height() +
                      2;
        }
        return height + PADDING;
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const StatsColors colors = StatsColors::current();
        const QColor muted = SettingsPage::noteColor();
        const bool lit = hasFocus() || underMouse();
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(palette().color(lit ? QPalette::Highlight : QPalette::Mid),
                            hasFocus() ? 2.0 : 1.0));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(frame, 6.0, 6.0);

        const int side = circle(width());
        const QRectF ring(width() / 2.0 - side / 2.0, PADDING, side, side);
        if (m_won) {
            badge_painter::drawBadge(painter, ring, m_kind, colors.badge(m_level), true, 3.0);
        } else {
            QPen pen(muted, 2.0);
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({2.0, 1.6});
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(ring.adjusted(1, 1, -1, -1));
            badge_painter::drawTrophy(painter, ring.adjusted(side / 4.0, side / 4.0, -side / 4.0,
                                                             -side / 4.0),
                                      muted);
        }

        int top = int(ring.bottom()) + 8;
        const int textWidth = std::max(20, width() - 2 * PADDING);
        for (int i = 0; i < m_lines.size(); ++i) {
            const QFont font = fontOf(i);
            painter.setFont(font);
            painter.setPen(i == 0 ? palette().color(QPalette::Text) : muted);
            const QRect bounds =
                QFontMetrics(font).boundingRect(QRect(PADDING, top, textWidth, 10000),
                                                Qt::AlignHCenter | Qt::TextWordWrap, m_lines[i]);
            painter.drawText(QRect(PADDING, top, textWidth, bounds.height()),
                             Qt::AlignHCenter | Qt::TextWordWrap, m_lines[i]);
            top += bounds.height() + 2;
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        // Enter opens the tab too, as Space does
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            event->accept();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

private:
    static constexpr int PADDING = 12;

    int circle(int width) const { return std::max(40, std::min(CIRCLE, width - 2 * PADDING)); }

    /// @brief The first line a little larger and bold, the others smaller
    QFont fontOf(int line) const
    {
        if (line == 0) {
            QFont bold = scaledFont(font(), 1.1);
            bold.setBold(true);
            return bold;
        }
        return font();
    }

    bool m_won = false;
    core::BadgeKind m_kind = core::BadgeKind::Words;
    int m_level = 0;
    QStringList m_lines;
};

// =============================================================================
// What the views show
// =============================================================================

struct WritingStatsPanel::Context {
    const WritingStatsModel* model = nullptr;
    core::WritingRules rules;
    core::WritingStatistics statistics;  ///< The record the daily goals count
    QDate today;
    QDateTime now;
    StatsColors colors;
    QLocale locale;
    int pieceMinutes = 10;
    int fromHour = 6;
    int toHour = 24;
};

struct WritingStatsPanel::ViewData {
    QString title;
    QString subtitle;
    QString listText;        ///< The line of the view in the list
    QString note;            ///< A sentence instead of the graph and the numbers
    GraphData graph;
    QList<StatsKpiGrid::Number> numbers;
    double progress = -1.0;  ///< The share of the bar; below 0: no bar
    QList<StatsDetails::Detail> details;
    bool goals = false;      ///< The button of the goals
    bool awards = false;     ///< Add Challenge and the badge won last
    bool badgeWon = false;   ///< A badge was won: the last one
    core::BadgeKind badgeKind = core::BadgeKind::Words;
    int badgeLevel = 0;
    QStringList badgeLines;  ///< Its name, the day it was won, the badges and challenges
    bool periods = false;    ///< The arrows to earlier and later periods
    bool later = false;      ///< A later period is there
};

WritingStatsPanel::WritingStatsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("writingStatsPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("statsViewList"));
    m_list->setItemDelegate(new ViewDelegate(m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_list->setAccessibleName(tr("Views of the statistics"));
    root->addWidget(m_list);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_filling || row < 0 || row >= m_listed.size()) {
            return;
        }
        setView(m_listed[row]);
    });

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(8, 8, 8, 12);
    layout->setSpacing(GAP);

    auto* header = new QHBoxLayout();
    header->setSpacing(2);
    m_title = new QLabel(view);
    m_title->setObjectName(QStringLiteral("statsViewTitle"));
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->addWidget(m_title, 1);
    const auto arrow = [view](const char* icon, const QString& tip) {
        auto* button = new QToolButton(view);
        button->setAutoRaise(true);
        button->setIcon(core::ArtProvider::getInstance().getIcon(QString::fromLatin1(icon)));
        button->setProperty("iconId", QString::fromLatin1(icon));
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        return button;
    };
    m_previous = arrow("common.chevronLeft", tr("Earlier"));
    m_next = arrow("common.chevronRight", tr("Later"));
    header->addWidget(m_previous);
    header->addWidget(m_next);
    connect(m_previous, &QToolButton::clicked, this, [this]() { setPeriodOffset(m_offset - 1); });
    connect(m_next, &QToolButton::clicked, this, [this]() { setPeriodOffset(m_offset + 1); });
    layout->addLayout(header);

    m_subtitle = new QLabel(view);
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);

    // The achievements: Add Challenge on top, then the badge won last, large
    m_addChallenge = new QPushButton(view);
    m_addChallenge->setObjectName(QStringLiteral("statsAddChallenge"));
    m_addChallenge->setIcon(
        core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.addChallenge")));
    m_addChallenge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_addChallenge, &QPushButton::clicked, this, &WritingStatsPanel::addChallengeRequested);
    layout->addWidget(m_addChallenge, 0, Qt::AlignLeft);
    m_awardsCaption = new QLabel(view);
    m_awardsCaption->setObjectName(QStringLiteral("statsLastBadgeCaption"));
    layout->addWidget(m_awardsCaption);
    m_bigBadge = new BigBadgeButton(view);
    m_bigBadge->setObjectName(QStringLiteral("statsLastBadge"));
    connect(m_bigBadge, &QAbstractButton::clicked, this, &WritingStatsPanel::awardsRequested);
    layout->addWidget(m_bigBadge);
    retranslateAwards();

    m_graph = new StatsGraph(view);
    m_graph->setObjectName(QStringLiteral("statsViewGraph"));
    layout->addWidget(m_graph);

    m_note = new QLabel(view);
    m_note->setWordWrap(true);
    layout->addWidget(m_note);

    m_numbers = new StatsKpiGrid(view);
    layout->addWidget(m_numbers);

    m_progress = new StatsProgress(view);
    layout->addWidget(m_progress);

    m_details = new StatsDetails(view);
    layout->addWidget(m_details);

    m_goals = new QPushButton(view);
    m_goals->setText(tr("&Goals and Deadlines"));
    m_goals->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.writingGoals")));
    m_goals->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_goals, &QPushButton::clicked, this, &WritingStatsPanel::goalsRequested);
    layout->addWidget(m_goals, 0, Qt::AlignLeft);

    root->addWidget(view);
    root->addStretch(1);

    // The foot: whether the text pasted counts, at hand in every view
    m_footer = new QWidget(this);
    m_footer->setObjectName(QStringLiteral("statsPanelFooter"));
    auto* foot = new QVBoxLayout(m_footer);
    foot->setContentsMargins(0, 0, 0, 0);
    foot->setSpacing(0);
    auto* footLine = new QFrame(m_footer);
    footLine->setFrameShape(QFrame::HLine);
    footLine->setFrameShadow(QFrame::Sunken);
    foot->addWidget(footLine);
    auto* boxRow = new QHBoxLayout();
    boxRow->setContentsMargins(8, 6, 8, 6);
    boxRow->setSpacing(6);
    m_countPasted = new QCheckBox(m_footer);
    m_countPasted->setObjectName(QStringLiteral("statsCountPasted"));
    m_countPastedText = new BoxText(m_countPasted, m_footer);
    //: In Polish: &Wliczaj wklejony tekst do statystyk
    m_countPastedText->setText(tr("Count &pasted text in the statistics"));
    m_countPasted->setAccessibleName(tr("Count pasted text in the statistics"));
    m_countPasted->setToolTip(
        //: In Polish: To samo co Ustawienia > Statystyki i cele > Liczenie
        tr("The same as Settings > Statistics and Goals > Counting"));
    boxRow->addWidget(m_countPasted, 0, Qt::AlignTop);
    boxRow->addWidget(m_countPastedText, 1);
    foot->addLayout(boxRow);
    root->addWidget(m_footer);
    connect(m_countPasted, &QCheckBox::toggled, this, [](bool on) {
        auto& settings = core::SettingsManager::getInstance();
        if (settings.get<bool>("stats.countPasted", false) != on) {
            settings.set<bool>("stats.countPasted", on);
            core::Logger::getInstance().info("Statistics: pasted text {}",
                                             on ? "counted" : "not counted");
        }
    });
    showCountPasted();

    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            [this]() {
                for (QToolButton* button : {m_previous, m_next}) {
                    button->setIcon(core::ArtProvider::getInstance().getIcon(
                        button->property("iconId").toString()));
                }
                m_goals->setIcon(core::ArtProvider::getInstance().getIcon(
                    QStringLiteral("tools.writingGoals")));
                m_addChallenge->setIcon(core::ArtProvider::getInstance().getIcon(
                    QStringLiteral("tools.addChallenge")));
                refresh();
            });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            &WritingStatsPanel::refresh);

    updateFonts();
    refresh();
}

WritingStatsPanel::~WritingStatsPanel() = default;

void WritingStatsPanel::setModel(WritingStatsModel* model)
{
    if (m_model != nullptr) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model != nullptr) {
        connect(m_model, &WritingStatsModel::changed, this, &WritingStatsPanel::refresh);
        connect(m_model, &WritingStatsModel::goalsChanged, this, &WritingStatsPanel::refresh);
        connect(m_model, &QObject::destroyed, this, [this]() { m_model = nullptr; });
    }
    m_dirty = true;
    refresh();
}

void WritingStatsPanel::setAwardsTracker(WritingAwardsTracker* tracker)
{
    if (m_tracker != nullptr) {
        disconnect(m_tracker, nullptr, this, nullptr);
    }
    m_tracker = tracker;
    if (m_tracker != nullptr) {
        connect(m_tracker, &WritingAwardsTracker::changed, this, &WritingStatsPanel::refresh);
        connect(m_tracker, &QObject::destroyed, this, [this]() { m_tracker = nullptr; });
    }
    m_dirty = true;
    refresh();
}

QAbstractButton* WritingStatsPanel::lastBadgeButton() const
{
    return m_bigBadge;
}

void WritingStatsPanel::retranslateAwards()
{
    //: In Polish: &Dodaj wyzwanie...
    m_addChallenge->setText(tr("&Add Challenge..."));
    //: In Polish: Ostatnio zdobyta odznaka
    m_awardsCaption->setText(tr("Last badge won"));
    //: In Polish: Otwórz kartę „Wyzwania i odznaki” w środku okna
    m_bigBadge->setToolTip(tr("Open the tab Challenges and Badges in the middle of the window"));
}

QWidget* WritingStatsPanel::takeFooter()
{
    if (m_footer != nullptr && m_footer->parentWidget() == this) {
        layout()->removeWidget(m_footer);
    }
    return m_footer;
}

void WritingStatsPanel::showCountPasted()
{
    const QSignalBlocker blocker(m_countPasted);
    m_countPasted->setChecked(
        core::SettingsManager::getInstance().get<bool>("stats.countPasted", false));
}

WritingStatsModel* WritingStatsPanel::model() const
{
    return m_model;
}

void WritingStatsPanel::setTitleSources(std::function<QString(const QString&)> textTitle,
                                        std::function<QString()> bookTitle)
{
    m_textTitle = std::move(textTitle);
    m_bookTitle = std::move(bookTitle);
    refresh();
}

void WritingStatsPanel::setView(View view)
{
    if (view != m_view) {
        m_view = view;
        m_offset = 0;
    }
    m_dirty = true;
    refresh();
}

void WritingStatsPanel::setPeriodOffset(int offset)
{
    m_offset = std::min(0, offset);
    m_dirty = true;
    refresh();
}

const char* WritingStatsPanel::settingOf(View view)
{
    switch (view) {
    case View::Today:
        return "stats.panel.today";
    case View::Week:
        return "stats.panel.week";
    case View::Month:
        return "stats.panel.month";
    case View::Book:
        return "stats.panel.book";
    case View::Hours:
        return "stats.panel.hours";
    case View::Awards:
        return "stats.panel.awards";
    }
    return "stats.panel.today";
}

QList<WritingStatsPanel::View> WritingStatsPanel::listedViews() const
{
    auto& settings = core::SettingsManager::getInstance();
    QList<View> views;
    for (View view :
         {View::Today, View::Week, View::Month, View::Book, View::Hours, View::Awards}) {
        if (settings.get<bool>(settingOf(view), true)) {
            views.append(view);
        }
    }
    return views;
}

QString WritingStatsPanel::titleText() const
{
    return m_title->text();
}

QString WritingStatsPanel::subtitleText() const
{
    return m_subtitle->text();
}

QStringList WritingStatsPanel::numbers() const
{
    return m_numberTexts;
}

QStringList WritingStatsPanel::details() const
{
    return m_detailTexts;
}

QStringList WritingStatsPanel::listTexts() const
{
    return m_listTexts;
}

void WritingStatsPanel::refresh()
{
    showCountPasted();
    // Not shown (under the tab of another panel): counted again when it shows
    if (!isVisible() && testAttribute(Qt::WA_WState_Created)) {
        m_dirty = true;
        return;
    }
    m_dirty = false;
    const Context now = context();
    fillList(now);
    fillView(now);
}

void WritingStatsPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_dirty) {
        m_dirty = false;
        const Context now = context();
        fillList(now);
        fillView(now);
    }
}

void WritingStatsPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LanguageChange ||
        event->type() == QEvent::PaletteChange) {
        updateFonts();
        if (event->type() != QEvent::PaletteChange) {
            m_goals->setText(tr("&Goals and Deadlines"));
            m_countPastedText->setText(tr("Count &pasted text in the statistics"));
            m_countPasted->setAccessibleName(tr("Count pasted text in the statistics"));
            m_countPasted->setToolTip(tr("The same as Settings > Statistics and Goals > Counting"));
            m_previous->setToolTip(tr("Earlier"));
            m_next->setToolTip(tr("Later"));
            retranslateAwards();
        }
        m_dirty = true;
        refresh();
    }
}

void WritingStatsPanel::updateFonts()
{
    QFont title = scaledFont(font(), 1.6);
    title.setWeight(QFont::Light);
    m_title->setFont(title);
    m_subtitle->setStyleSheet(
        QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    m_note->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
}

WritingStatsPanel::Context WritingStatsPanel::context() const
{
    Context context;
    context.colors = StatsColors::current();
    auto& settings = core::SettingsManager::getInstance();
    context.pieceMinutes = settings.get<int>("stats.bar.pieceMinutes", 10);
    context.fromHour = settings.get<int>("stats.bar.fromHour", 6);
    context.toHour = settings.get<int>("stats.bar.toHour", 24);
    if (m_model == nullptr) {
        context.today = QDate::currentDate();
        context.now = QDateTime::currentDateTime();
        return context;
    }
    context.model = m_model;
    context.rules = m_model->rules();
    context.statistics = m_model->goalStatistics();
    context.today = m_model->today();
    context.now = m_model->now();
    return context;
}

void WritingStatsPanel::fillList(const Context& context)
{
    const QList<View> listed = listedViews();
    m_filling = true;
    // The same views: their items change in place, so the keys and the focus stay on them
    if (listed != m_listed) {
        m_list->clear();
        for (View view : listed) {
            auto* item = new QListWidgetItem(m_list);
            item->setData(VIEW_ROLE, int(view));
        }
        m_listed = listed;
    }
    m_listTexts.clear();
    const qreal ratio = devicePixelRatioF();
    int row = -1;
    for (int i = 0; i < m_listed.size(); ++i) {
        const View view = m_listed[i];
        ViewData viewData;
        viewOf(view, context, viewData);
        QListWidgetItem* item = m_list->item(i);
        const QString text = QStringLiteral("%1: %2").arg(viewData.title, viewData.listText);
        item->setData(Qt::DisplayRole, viewData.title);
        item->setData(SUBTITLE_ROLE, viewData.listText);
        item->setData(Qt::DecorationRole, miniGraph(viewData.graph, context.colors, ratio));
        item->setData(Qt::AccessibleTextRole, text);
        m_listTexts.append(text);
        if (view == m_view) {
            row = i;
        }
    }
    if (m_list->currentRow() != row) {
        m_list->setCurrentRow(row);
    }
    m_list->setVisible(!m_listed.isEmpty());
    int height = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        height += m_list->sizeHintForRow(i);
    }
    m_list->setFixedHeight(height + 2 * m_list->frameWidth() + 6);
    m_filling = false;
}

void WritingStatsPanel::fillView(const Context& context)
{
    ViewData viewData;
    viewOf(m_view, context, viewData);
    m_title->setText(viewData.title);
    m_subtitle->setText(viewData.subtitle);
    m_subtitle->setVisible(!viewData.subtitle.isEmpty());
    m_previous->setVisible(viewData.periods);
    m_next->setVisible(viewData.periods);
    m_next->setEnabled(viewData.later);
    // The trophy is the small graph of Achievements in the list; the view shows the badge
    m_graph->setVisible(viewData.graph.kind != GraphData::Kind::None &&
                        viewData.graph.kind != GraphData::Kind::Trophy);
    m_graph->setGraph(viewData.graph, context.colors);
    m_note->setText(viewData.note);
    m_note->setVisible(!viewData.note.isEmpty());
    QFont big = scaledFont(font(), 1.4);
    big.setWeight(QFont::Light);
    m_numbers->setNumbers(viewData.numbers, big);
    m_numbers->setVisible(!viewData.numbers.isEmpty());
    m_progress->setVisible(viewData.progress >= 0.0);
    m_progress->setShare(viewData.progress, context.colors);
    m_details->setDetails(viewData.details);
    m_details->setVisible(!viewData.details.isEmpty());
    m_goals->setVisible(viewData.goals);
    m_addChallenge->setVisible(viewData.awards);
    m_awardsCaption->setVisible(viewData.awards && viewData.badgeWon);
    m_bigBadge->setVisible(viewData.awards);
    m_bigBadge->setBadge(viewData.badgeWon, viewData.badgeKind, viewData.badgeLevel,
                         viewData.badgeLines);

    m_numberTexts.clear();
    for (const StatsKpiGrid::Number& number : viewData.numbers) {
        m_numberTexts.append(
            QStringLiteral("%1: %2 %3").arg(number.caption, number.value, number.unit).trimmed());
    }
    m_detailTexts.clear();
    for (const StatsDetails::Detail& detail : viewData.details) {
        m_detailTexts.append(QStringLiteral("%1: %2").arg(detail.name, detail.value));
    }
}

void WritingStatsPanel::viewOf(View view, const Context& context, ViewData& viewData) const
{
    switch (view) {
    case View::Today:
        viewData.title = tr("Today");
        break;
    case View::Week:
        viewData.title = tr("Week");
        break;
    case View::Month:
        viewData.title = tr("30 Days");
        break;
    case View::Book:
        viewData.title = tr("Book");
        break;
    case View::Hours:
        //: The view of the hours and days the writer writes in. In Polish: Kiedy piszesz
        viewData.title = tr("When You Write");
        break;
    case View::Awards:
        //: The view of the badges and the challenges. In Polish: Osiągnięcia
        viewData.title = tr("Achievements");
        break;
    }
    if (context.model == nullptr) {
        viewData.note = tr("No statistics yet.");
        return;
    }
    switch (view) {
    case View::Today:
        showToday(context, viewData);
        break;
    case View::Week:
        showWeek(context, viewData);
        break;
    case View::Month:
        showMonth(context, viewData);
        break;
    case View::Book:
        showBook(context, viewData);
        break;
    case View::Hours:
        showHours(context, viewData);
        break;
    case View::Awards:
        showAwards(context, viewData);
        break;
    }
}

// -----------------------------------------------------------------------------
// Today
// -----------------------------------------------------------------------------

void WritingStatsPanel::showToday(const Context& context, ViewData& viewData) const
{
    using stats_painter::number;
    const core::WritingStatistics& statistics = context.statistics;
    const core::GoalCounting counting = context.rules.counting;
    const QDate today = context.today;
    const core::WritingTotals totals = statistics.day(today);
    const int words = totals.counted(counting);
    const int goal = context.model->dailyGoalToday();
    const int dayStart = statistics.rules().dayStartMinutes;

    viewData.subtitle = context.locale.toString(today, QStringLiteral("dddd, d MMMM"));
    viewData.listText = goal > 0 ? tr("%1 of %2 words · %3")
                                   .arg(number(words), number(goal),
                                        stats_painter::hoursMinutes(totals.activeMs))
                             : tr("%1 · %2")
                                   .arg(stats_painter::words(words),
                                        stats_painter::hoursMinutes(totals.activeMs));

    // The graph of the day: the words in each piece, in the hours of the bar
    const QList<DayPiece> pieces =
        WritingStatsModel::dayPieces(statistics, today, context.pieceMinutes);
    const int piece = pieces.isEmpty() ? context.pieceMinutes : DAY_MINUTES / int(pieces.size());
    const int nowMinute = int(std::clamp<qint64>(
        context.model->minutesIntoDay(today, context.now), 0, DAY_MINUTES - 1));
    const auto [from, to] = WritingStatsModel::shownMinutes(
        pieces, piece, dayStart, context.fromHour, context.toHour, nowMinute);
    GraphData& graph = viewData.graph;
    graph.kind = GraphData::Kind::Day;
    double most = 0.0;
    for (int i = from / piece; i < (to + piece - 1) / piece && i < pieces.size(); ++i) {
        const double value = std::max(0, pieces[i].counted(counting));
        graph.values.append(value);
        most = std::max(most, value);
        const int start = dayStart + i * piece;
        graph.tips.append(tr("%1–%2: %3")
                              .arg(clockOf(start), clockOf(start + piece),
                                   stats_painter::words(qint64(value))));
    }
    graph.max = stats_painter::niceMax(most);
    graph.now = to > from ? double(nowMinute - from) / double(to - from) : -1.0;
    graph.topLeft = tr("Words in %1 min").arg(piece);
    graph.topRight = number(qint64(graph.max));
    graph.bottomLeft = QStringLiteral("%1 – %2").arg(clockOf(dayStart + from),
                                                     clockOf(dayStart + to) == QLatin1String("0:00") ? QStringLiteral("24:00") : clockOf(dayStart + to));

    // The sessions: the slots with writing, with pauses not longer than the idle time
    const QList<DayPiece> fiveMinutes = WritingStatsModel::dayPieces(statistics, today,
                                                                     core::ACTIVITY_SLOT_MINUTES);
    const int allowedGap = std::max(1, (context.rules.idleMinutes + core::ACTIVITY_SLOT_MINUTES - 1) /
                                           core::ACTIVITY_SLOT_MINUTES);
    QList<std::pair<int, int>> sessions;
    for (int i = 0; i < fiveMinutes.size(); ++i) {
        const DayPiece& slot = fiveMinutes[i];
        if (slot.written == 0 && slot.deleted == 0 && slot.activeMs == 0) {
            continue;
        }
        if (!sessions.isEmpty() && i - sessions.last().second <= allowedGap) {
            sessions.last().second = i + 1;
        } else {
            sessions.append({i, i + 1});
        }
    }
    QStringList times;
    int longest = 0;
    for (const auto& [first, end] : sessions) {
        times.append(QStringLiteral("%1–%2").arg(
            clockOf(dayStart + first * core::ACTIVITY_SLOT_MINUTES),
            clockOf(dayStart + end * core::ACTIVITY_SLOT_MINUTES)));
        longest = std::max(longest, (end - first) * core::ACTIVITY_SLOT_MINUTES);
    }

    const double pace = totals.wordsPerHour(counting);
    viewData.numbers.append({tr("Words today"), number(words), QString()});
    viewData.numbers.append({tr("Time of writing"), stats_painter::hoursMinutes(totals.activeMs), QString()});
    viewData.numbers.append({tr("Days in a row"), number(statistics.streak(today)), QString()});
    viewData.numbers.append({tr("Pace"), number(std::lround(pace)), tr("words/h")});
    if (goal > 0) {
        viewData.numbers.append({tr("Daily goal"),
                             QStringLiteral("%1%").arg(std::lround(100.0 * words / goal)),
                             QString()});
    }
    viewData.numbers.append({tr("Sessions"), number(sessions.size()), QString()});

    viewData.details.append({tr("Written"), number(totals.written), {}});
    viewData.details.append({tr("Deleted"), number(totals.deleted), {}});
    if (totals.pasted != 0) {
        //: In Polish: Wklejone; "%1 – wliczone" albo "%1 – niewliczone"
        viewData.details.append({tr("Pasted"),
                             statistics.rules().countPasted
                                 ? tr("%1, counted").arg(number(totals.pasted))
                                 : tr("%1, not counted").arg(number(totals.pasted)),
                             {}});
    }
    if (goal > 0) {
        viewData.details.append({tr("To the daily goal"),
                             words >= goal ? tr("reached")
                                           : stats_painter::words(goal - words),
                             words >= goal ? context.colors.done : QColor()});
    }
    const core::WritingTemperature temperature = statistics.temperature(context.now);
    const QString temperatureName = stats_painter::temperatureName(temperature);
    if (!temperatureName.isEmpty()) {
        //: In Polish: Tempo pracy
        viewData.details.append({tr("Pace of work"), temperatureName,
                                 context.colors.temperature(temperature),
                                 //: In Polish: Ostatnie 30 minut w porównaniu ze zwykłym pisaniem
                                 //: z ostatnich 4 tygodni
                                 tr("The last 30 minutes compared with the usual writing of the "
                                    "last 4 weeks")});
    }
    if (!times.isEmpty()) {
        viewData.details.append({tr("Sessions"), times.join(QStringLiteral(", ")), {}});
        viewData.details.append({tr("Longest session"),
                             stats_painter::hoursMinutes(qint64(longest) * 60 * 1000), {}});
    }
    // The texts written most today
    if (m_textTitle) {
        const QHash<QString, core::WritingTotals> texts = statistics.texts(today, today);
        QList<std::pair<int, QString>> written;
        for (auto it = texts.constBegin(); it != texts.constEnd(); ++it) {
            const QString title = it.key().isEmpty() ? QString() : m_textTitle(it.key());
            if (!title.isEmpty() && it.value().written > 0) {
                written.append({it.value().counted(counting), title});
            }
        }
        std::sort(written.begin(), written.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        QStringList shown;
        for (qsizetype i = 0; i < written.size() && i < 3; ++i) {
            shown.append(tr("%1 (%2)").arg(written[i].second, number(written[i].first)));
        }
        if (!shown.isEmpty()) {
            viewData.details.append({tr("Texts"), shown.join(QStringLiteral(", ")), {}});
        }
    }
    viewData.goals = true;
}

// -----------------------------------------------------------------------------
// The week and 30 days
// -----------------------------------------------------------------------------

namespace {

/// @brief The columns of the days from @p from to @p to: their words, the days off, the goal
/// reached and the tooltips
void fillDays(GraphData& graph, const core::WritingStatistics& statistics,
              const core::WritingRules& rules, const QDate& from, const QDate& to,
              const QDate& today, const QLocale& locale)
{
    graph.kind = GraphData::Kind::Columns;
    double most = 0.0;
    for (QDate day = from; day <= to; day = day.addDays(1)) {
        const QString name = locale.toString(day, QStringLiteral("dddd, d MMMM"));
        if (day > today) {
            graph.values.append(std::numeric_limits<double>::quiet_NaN());
            graph.off.append(!rules.isWritingDay(day));
            graph.done.append(false);
            graph.tips.append(name);
            continue;
        }
        const int words = statistics.day(day).counted(rules.counting);
        const int goal = rules.dailyGoalOn(day);
        graph.values.append(words);
        graph.off.append(!rules.isWritingDay(day));
        graph.done.append(goal > 0 && words >= goal);
        graph.tips.append(WritingStatsPanel::tr("%1: %2").arg(name, stats_painter::words(words)));
        most = std::max(most, double(words));
    }
    graph.goal = rules.dailyGoal;
    graph.max = stats_painter::niceMax(std::max(most, graph.goal * 1.2));
    graph.topLeft = rules.dailyGoal > 0
                        ? WritingStatsPanel::tr("Words a day · line: goal %1")
                              .arg(stats_painter::number(rules.dailyGoal))
                        : WritingStatsPanel::tr("Words a day");
    graph.topRight = stats_painter::number(qint64(graph.max));
}

}  // namespace

void WritingStatsPanel::showWeek(const Context& context, ViewData& viewData) const
{
    using stats_painter::number;
    const core::WritingRules& rules = context.rules;
    const QDate monday = context.today.addDays(1 - context.today.dayOfWeek() + 7 * m_offset);
    const QDate sunday = monday.addDays(6);
    viewData.periods = true;
    viewData.later = m_offset < 0;
    viewData.subtitle =
        tr("%1 – %2").arg(context.locale.toString(monday, QStringLiteral("d MMMM")),
                          context.locale.toString(sunday, QStringLiteral("d MMMM")));
    fillDays(viewData.graph, context.statistics, rules, monday, sunday, context.today,
             context.locale);
    for (QDate day = monday; day <= sunday; day = day.addDays(1)) {
        viewData.graph.labels.append(QStringLiteral("%1 %2").arg(
            context.locale.dayName(day.dayOfWeek(), QLocale::ShortFormat)).arg(day.day()));
    }
    viewData.graph.bottomLeft = context.locale.toString(monday, QStringLiteral("d MMMM"));
    viewData.graph.bottomRight = context.locale.toString(sunday, QStringLiteral("d MMMM"));

    int sum = 0;
    qint64 time = 0;
    int writingDays = 0;
    int goalDays = 0;
    int bestWords = 0;
    QDate best;
    QStringList offDays;
    for (QDate day = monday; day <= sunday; day = day.addDays(1)) {
        if (!rules.isWritingDay(day)) {
            offDays.append(context.locale.dayName(day.dayOfWeek()));
        }
        if (day > context.today) {
            continue;
        }
        const core::WritingTotals totals = context.statistics.day(day);
        const int words = totals.counted(rules.counting);
        sum += words;
        time += totals.activeMs;
        if (rules.isWritingDay(day)) {
            ++writingDays;
            if (rules.dailyGoalOn(day) > 0 && words >= rules.dailyGoalOn(day)) {
                ++goalDays;
            }
        }
        if (words > bestWords) {
            bestWords = words;
            best = day;
        }
    }
    const int weekly = rules.weeklyGoalWords();
    viewData.listText = weekly > 0 ? tr("%1 of %2 words").arg(number(sum), number(weekly))
                               : stats_painter::words(sum);
    viewData.numbers.append({m_offset == 0 ? tr("This week") : tr("In the week"), number(sum), QString()});
    if (weekly > 0) {
        viewData.numbers.append({tr("Weekly goal"),
                             QStringLiteral("%1%").arg(std::lround(100.0 * sum / weekly)), QString()});
        viewData.progress = double(sum) / weekly;
    }
    viewData.numbers.append({tr("Average"), number(writingDays > 0 ? std::lround(double(sum) / writingDays) : 0),
                         tr("words/day")});

    if (weekly > 0) {
        int days = 0;
        for (int d = 0; d < 7; ++d) {
            if ((rules.writingDays & (1 << d)) != 0) {
                ++days;
            }
        }
        viewData.details.append({tr("Weekly goal"),
                             rules.weeklyGoal > 0
                                 ? stats_painter::words(weekly)
                                 //: The weekly goal: %1 words, %2 writing days, %3 the daily
                                 //: goal. In Polish: %1 (%2 × %3)
                                 : tr("%1 (%2 × %3)")
                                       .arg(stats_painter::words(weekly),
                                            days == 1
                                                //: In Polish: 1 dzień pisania
                                                ? tr("1 writing day")
                                                //: In Polish: %1 dzień pisania / %1 dni pisania
                                                : tr("%1 writing days", nullptr, days)
                                                      .arg(number(days)),
                                            number(rules.dailyGoal)),
                             {}});
        if (m_offset == 0) {
            viewData.details.append({tr("Left"),
                                 sum >= weekly ? tr("goal reached")
                                               : stats_painter::words(weekly - sum),
                                 sum >= weekly ? context.colors.done : QColor()});
        }
    }
    if (best.isValid()) {
        viewData.details.append({tr("Best day"),
                             tr("%1, %2").arg(context.locale.dayName(best.dayOfWeek()),
                                              stats_painter::words(bestWords)),
                             {}});
    }
    if (rules.dailyGoal > 0) {
        viewData.details.append({tr("Days with the goal"),
                             tr("%1 of %2").arg(number(goalDays), number(writingDays)), {}});
    }
    viewData.details.append({tr("Time of writing"), stats_painter::hoursMinutes(time), {}});
    viewData.details.append({tr("Days off"), offDays.isEmpty() ? tr("none") : offDays.join(QStringLiteral(", ")), {}});
    viewData.goals = true;
}

void WritingStatsPanel::showMonth(const Context& context, ViewData& viewData) const
{
    using stats_painter::number;
    const core::WritingRules& rules = context.rules;
    const QDate last = context.today.addDays(30 * m_offset);
    const QDate first = last.addDays(-29);
    viewData.periods = true;
    viewData.later = m_offset < 0;
    viewData.subtitle =
        tr("%1 – %2").arg(context.locale.toString(first, QStringLiteral("d MMMM")),
                          context.locale.toString(last, QStringLiteral("d MMMM")));
    fillDays(viewData.graph, context.statistics, rules, first, last, context.today, context.locale);
    viewData.graph.bottomLeft = context.locale.toString(first, QStringLiteral("d MMMM"));
    viewData.graph.bottomRight = context.locale.toString(last, QStringLiteral("d MMMM"));

    int sum = 0;
    int writingWords = 0;
    int offWords = 0;
    int goalDays = 0;
    int bestWords = 0;
    int missed = 0;
    qint64 time = 0;
    QDate best;
    QDate lastMissed;
    const QDate start = context.statistics.firstDay();
    for (QDate day = first; day <= last; day = day.addDays(1)) {
        const core::WritingTotals totals = context.statistics.day(day);
        const int words = totals.counted(rules.counting);
        sum += words;
        time += totals.activeMs;
        if (rules.isWritingDay(day)) {
            writingWords += words;
            if (rules.dailyGoalOn(day) > 0 && words >= rules.dailyGoalOn(day)) {
                ++goalDays;
            }
            // A writing day without writing, after the record started and before today
            if (totals.isEmpty() && start.isValid() && day >= start && day < context.today) {
                ++missed;
                lastMissed = day;
            }
        } else {
            offWords += words;
        }
        if (words > bestWords) {
            bestWords = words;
            best = day;
        }
    }
    viewData.listText = stats_painter::words(sum);
    viewData.numbers.append({tr("Total"), number(sum), QString()});
    viewData.numbers.append({tr("Average"),
                         number(std::lround(context.statistics.averagePerWritingDay(first, last))),
                         tr("on a writing day")});
    if (rules.dailyGoal > 0) {
        viewData.numbers.append({tr("Days with the goal"), number(goalDays), QString()});
    }
    if (best.isValid()) {
        viewData.details.append({tr("Best day"),
                             tr("%1 – %2")
                                 .arg(context.locale.toString(best, QStringLiteral("dddd, d MMMM")),
                                      stats_painter::words(bestWords)),
                             {}});
    }
    if (sum > 0) {
        viewData.details.append({tr("Writing days and days off"),
                             tr("%1% and %2%")
                                 .arg(std::lround(100.0 * writingWords / sum))
                                 .arg(std::lround(100.0 * offWords / sum)),
                             {}});
    }
    viewData.details.append({tr("Writing days missed"),
                         missed == 0 ? tr("none")
                                     : tr("%1 (the last: %2)")
                                           .arg(number(missed),
                                                context.locale.toString(lastMissed, QStringLiteral("dddd, d MMMM"))),
                         {}});
    viewData.details.append({tr("Time of writing"), stats_painter::hoursMinutes(time), {}});
}

// -----------------------------------------------------------------------------
// The book
// -----------------------------------------------------------------------------

void WritingStatsPanel::showBook(const Context& context, ViewData& viewData) const
{
    using stats_painter::number;
    const WritingStatsModel* model = context.model;
    if (!model->hasBook()) {
        viewData.subtitle = tr("No book is open");
        viewData.listText = tr("No book is open");
        viewData.note = tr("Open a book to see its words, its goal and its deadline.");
        return;
    }
    const QString bookTitle = m_bookTitle ? m_bookTitle() : QString();
    viewData.subtitle = bookTitle.isEmpty() ? QString() : tr("“%1”").arg(bookTitle);
    const core::WritingRules& rules = context.rules;
    const core::WritingStatistics statistics = model->bookStatistics();
    const core::BookGoals& goals = model->goals();
    const QDate today = context.today;
    const int words = model->bookWords();
    const int target = goals.targetWords;

    // The words in the book at the end of each day: back from today by the growth of each day
    QDate first = statistics.firstDay();
    if (!first.isValid() || first > today) {
        first = today;
    }
    first = std::max(first, today.addDays(-3650));
    const auto daysUpToToday = first.daysTo(today) + 1;
    QList<double> values(daysUpToToday);
    double running = words;
    for (qint64 i = daysUpToToday - 1; i >= 0; --i) {
        values[i] = std::max(0.0, running);
        running -= statistics.day(first.addDays(i)).net();
    }
    QDate end = today;
    if (goals.deadline.isValid() && goals.deadline > end) {
        end = goals.deadline;
    }
    if (end == first) {
        end = first.addDays(1);
    }
    const int left = std::max(0, target - words);
    const double average = statistics.averagePerWritingDay(today.addDays(-27), today);
    const QDate forecast =
        target > 0 ? core::forecastDate(rules, today, left, average) : QDate();

    GraphData& graph = viewData.graph;
    graph.kind = GraphData::Kind::Book;
    graph.values = values;
    graph.days = int(first.daysTo(end)) + 1;
    graph.goal = target;
    graph.deadline = goals.deadline.isValid() && goals.deadline >= first ? int(first.daysTo(goals.deadline)) : -1;
    graph.forecast = forecast.isValid() ? int(first.daysTo(forecast)) : -1;
    graph.max = stats_painter::niceMax(std::max<double>(words, target));
    graph.topLeft = target > 0 || goals.deadline.isValid() ? tr("Words in the book · lines: goal and deadline")
                                                           : tr("Words in the book");
    graph.topRight = number(qint64(graph.max));
    graph.bottomLeft = context.locale.toString(first, QStringLiteral("d MMMM"));
    graph.bottomRight = context.locale.toString(end, QStringLiteral("d MMMM"));

    viewData.listText = target > 0 ? tr("%1 of %2 words").arg(number(words), number(target))
                               : stats_painter::words(words);
    viewData.numbers.append({tr("Words"), number(words), QString()});
    if (target > 0) {
        viewData.numbers.append({tr("Goal"), QStringLiteral("%1%").arg(std::lround(100.0 * words / target)), QString()});
        viewData.progress = double(words) / target;
    }
    if (goals.deadline.isValid() && goals.deadline >= today) {
        const auto days = today.daysTo(goals.deadline);
        viewData.numbers.append({tr("To the deadline"), number(days),
                             days == 1
                                 //: The unit after 1. In Polish: dzień
                                 ? tr("day")
                                 //: The unit after a number of days. In Polish: dni
                                 : tr("days", nullptr, int(days))});
    }

    if (target <= 0 && !goals.deadline.isValid()) {
        viewData.note = tr("The book has no length or deadline yet. Set them on the tab Writing "
                           "Goals and Deadlines.");
    }
    if (target > 0) {
        viewData.details.append({tr("Length of the book"), stats_painter::words(target), {}});
        viewData.details.append({tr("Left"), left == 0 ? tr("goal reached") : stats_painter::words(left),
                             left == 0 ? context.colors.done : QColor()});
    }
    if (goals.deadline.isValid()) {
        viewData.details.append({tr("Deadline"),
                             context.locale.toString(goals.deadline, QStringLiteral("d MMMM yyyy")), {}});
        if (target > 0 && goals.deadline >= today && left > 0) {
            const int writingDays = core::writingDaysBetween(rules, today, goals.deadline);
            viewData.details.append({tr("Writing days to the deadline"), number(writingDays), {}});
            viewData.details.append({tr("Needed on a writing day"),
                                 stats_painter::words(core::wordsPerWritingDay(left, writingDays)),
                                 {}});
        }
    }
    if (target > 0 && left > 0) {
        viewData.details.append({tr("Forecast at the present pace"),
                             forecast.isValid()
                                 ? tr("about %1").arg(context.locale.toString(forecast, QStringLiteral("d MMMM yyyy")))
                                 : tr("not known yet"),
                             forecast.isValid() && goals.deadline.isValid() && forecast > goals.deadline
                                 ? context.colors.muchFast
                                 : QColor()});
    }
    const int next = goals.nextDeadline(today);
    if (next >= 0) {
        const core::WritingDeadline& deadline = goals.deadlines[next];
        viewData.details.append({tr("Next deadline"),
                             tr("%1, %2").arg(deadline.name,
                                              context.locale.toString(deadline.date, QStringLiteral("d MMMM"))),
                             {}});
    }
    viewData.goals = true;
}

// -----------------------------------------------------------------------------
// The times of writing
// -----------------------------------------------------------------------------

void WritingStatsPanel::showHours(const Context& context, ViewData& viewData) const
{
    const QDate today = context.today;
    const core::WritingHourMap map =
        context.statistics.hourMap(today.addDays(1 - HOURS_DAYS), today);
    //: In Polish: Średnia liczba słów w każdej godzinie każdego dnia tygodnia, z ostatnich 4
    //: tygodni. Im intensywniejszy kolor, tym więcej słów.
    viewData.subtitle = tr("The average words in each hour of each day of the week, in the "
                           "last 4 weeks. The deeper the color, the more words.");
    GraphData& graph = viewData.graph;
    graph.kind = GraphData::Kind::Hours;
    graph.heat = map.words;
    graph.topLeft = tr("Average words in an hour");
    graph.topRight = tr("days × hours");
    for (int d = 1; d <= 7; ++d) {
        graph.rowLabels.append(context.locale.dayName(d, QLocale::NarrowFormat));
    }
    for (int h : {0, 6, 12, 18, 24}) {
        graph.labels.append(QString::number(h));
    }
    for (int d = 0; d < 7; ++d) {
        for (int h = 0; h < 24; ++h) {
            //: %1: a day of the week, %2 and %3: hours, %4: words. In Polish: %1, %2:00–%3:00:
            //: średnio %4
            graph.tips.append(tr("%1, %2:00–%3:00: %4 on average")
                                  .arg(context.locale.dayName(d + 1))
                                  .arg(h)
                                  .arg(h + 1)
                                  .arg(stats_painter::words(std::lround(map.words[d][h]))));
        }
    }

    const int hours = map.bestHours(2);
    const auto [bestDay, bestHour] = map.bestTime(2);
    if (hours < 0) {
        viewData.listText = tr("too little writing yet");
        viewData.note = tr("Too little writing in the last 4 weeks to tell.");
        return;
    }
    const QString bestHours = tr("%1–%2").arg(hours).arg((hours + 2) % 24 == 0 ? 24 : (hours + 2) % 24);
    viewData.listText = map.bestDayOfWeek() > 0
                        ? tr("best at %1, on %2").arg(bestHours, context.locale.dayName(map.bestDayOfWeek()))
                        : tr("best at %1").arg(bestHours);
    viewData.numbers.append({tr("Best time"), bestHours, QString()});
    if (map.bestDayOfWeek() > 0) {
        viewData.numbers.append({tr("Best day"), context.locale.dayName(map.bestDayOfWeek()), QString()});
    }
    if (bestDay > 0 && bestHour >= 0) {
        viewData.details.append({tr("Your best time"),
                             tr("%1, %2:00–%3:00")
                                 .arg(context.locale.dayName(bestDay))
                                 .arg(bestHour)
                                 .arg((bestHour + 2) % 24 == 0 ? 24 : (bestHour + 2) % 24),
                             {}});
    }
    const auto share = [](double value) { return QStringLiteral("%1%").arg(std::lround(value * 100)); };
    viewData.details.append({tr("Morning (5–12)"), share(map.shareOfHours(5, 12)), {}});
    viewData.details.append({tr("Afternoon (12–18)"), share(map.shareOfHours(12, 18)), {}});
    viewData.details.append({tr("Evening (18–24)"), share(map.shareOfHours(18, 24)), {}});
    viewData.details.append({tr("Night (0–5)"), share(map.shareOfHours(0, 5)), {}});
    viewData.details.append({tr("Monday to Friday and weekend"),
                         tr("%1 and %2").arg(share(map.shareOfDays(1, 5)), share(map.shareOfDays(6, 7))),
                         {}});
}

// -----------------------------------------------------------------------------
// Achievements
// -----------------------------------------------------------------------------

void WritingStatsPanel::showAwards(const Context& context, ViewData& viewData) const
{
    Q_UNUSED(context);
    viewData.graph.kind = GraphData::Kind::Trophy;
    if (m_tracker == nullptr) {
        //: In Polish: Nie ma jeszcze odznak.
        viewData.note = tr("No badges yet.");
        viewData.listText = viewData.note;
        return;
    }
    viewData.awards = true;
    // The graph is the badge won last, in its own button
    const int badges = int(m_tracker->shownBadges().size());
    int open = 0;
    for (const core::WritingChallenge& challenge : m_tracker->challenges()) {
        const core::ChallengeProgress progress = m_tracker->challengeProgress(challenge);
        open += !progress.done && !progress.over ? 1 : 0;
    }
    QString counts = badges == 1
                         //: In Polish: 1 odznaka
                         ? tr("1 badge")
                         //: In Polish: %n odznaki / %n odznak
                         : tr("%n badges", nullptr, badges);
    if (open > 0) {
        //: Two counts side by side. In Polish: %1 · %2
        counts = tr("%1 · %2").arg(counts, open == 1
                                               //: A challenge going on. In Polish: 1 wyzwanie
                                               ? tr("1 challenge")
                                               //: In Polish: %n wyzwania / %n wyzwań
                                               : tr("%n challenges", nullptr, open));
    }
    viewData.listText = counts;

    // The numbers: the badges won of all, and the challenges going on
    const int allBadges = int(core::BADGE_KINDS.size()) * core::BADGE_LEVELS;
    //: The badges won. In Polish: Odznaki
    viewData.numbers.append({tr("Badges"), stats_painter::number(badges),
                             //: Under the number of the badges won: of all. In Polish: z %1
                             tr("of %1").arg(allBadges)});
    //: The challenges not done and not over. In Polish: Trwające wyzwania
    viewData.numbers.append({tr("Challenges going on"), stats_painter::number(open), QString()});

    // The badge nearest to be won, with the bar of how far it is
    std::optional<core::BadgeProgress> nearest;
    double nearestShare = -1.0;
    for (core::BadgeKind kind : core::BADGE_KINDS) {
        const core::BadgeProgress progress = m_tracker->progressOf(kind);
        const int level = progress.level();
        if (!progress.available || level >= core::BADGE_LEVELS) {
            continue;
        }
        const double share = progress.share(level);
        if (share > nearestShare) {
            nearestShare = share;
            nearest = progress;
        }
    }
    if (nearest) {
        viewData.progress = nearestShare;
        //: In Polish: Najbliższa odznaka
        viewData.details.append({tr("Nearest badge"),
                                 badge_painter::title(*nearest, nearest->level()), QColor(),
                                 badge_painter::description(*nearest)});
        //: How far the nearest badge is. In Polish: Postęp
        viewData.details.append({tr("So far"), badge_painter::valueText(*nearest)});
    }
    // The challenges going on, each with how far it is and its last day
    for (const core::WritingChallenge& challenge : m_tracker->challenges()) {
        const core::ChallengeProgress progress = m_tracker->challengeProgress(challenge);
        if (progress.done || progress.over) {
            continue;
        }
        viewData.details.append({badge_painter::challengeTitle(challenge),
                                 tr("%1, %2").arg(
                                     badge_painter::challengeValue(challenge, progress.value),
                                     badge_painter::challengePeriod(challenge))});
    }
    //: In Polish: Kliknij odznakę, aby zobaczyć wszystkie odznaki i wyzwania.
    viewData.note = tr("Click the badge to see all the badges and challenges.");

    if (const std::optional<core::EarnedBadge> last = m_tracker->lastBadge()) {
        const core::BadgeProgress progress = m_tracker->progressOf(last->kind);
        viewData.badgeWon = true;
        viewData.badgeKind = last->kind;
        viewData.badgeLevel = last->level;
        viewData.badgeLines = {badge_painter::title(progress, last->level),
                           badge_painter::wonOn(last->date), counts};
    } else {
        viewData.badgeLines = {
            //: In Polish: Nie ma jeszcze odznak
            tr("No badges yet"),
            //: In Polish: Odznaki przychodzą z pisaniem: za słowa, dni z rzędu i osiągnięte cele.
            tr("The badges come with the writing: for the words, the days in a row and the goals "
               "reached."),
            counts};
    }
}

}  // namespace kalahari::gui
