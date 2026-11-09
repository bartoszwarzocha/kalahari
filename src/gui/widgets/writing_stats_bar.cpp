/// @file writing_stats_bar.cpp
/// @brief The statistics bar above the tabs of the texts

#include "kalahari/gui/widgets/writing_stats_bar.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QToolButton>
#include <QToolTip>

#include <algorithm>
#include <functional>
#include <cmath>

namespace kalahari::gui {

namespace {

constexpr int DAY_MINUTES = 24 * 60;
/// The room of the column of the buttons on the right of the graph
constexpr int BUTTONS_ROOM = 24;
/// The margin of the texts on the graph, at each side
constexpr int TEXTS_PADDING = 6;
/// The gap between the texts in the line
constexpr int LINE_GAP = 8;

/// @brief The minute of the day at or before @p minute where an hour of the clock starts
int hourBefore(int minute, int dayStart)
{
    const int clock = ((minute + dayStart) % 60 + 60) % 60;
    return minute - clock;
}

/// @brief The minute of the day at or after @p minute where an hour of the clock starts
int hourAfter(int minute, int dayStart)
{
    const int before = hourBefore(minute, dayStart);
    return before == minute ? minute : before + 60;
}

/// @brief The button of the mini graph: the graph and its text, framed under the mouse and
/// with the focus, without a panel that would cover them
class MiniGraphButton : public QToolButton {
public:
    using Paint = std::function<void(QPainter&, const QRect&)>;

    MiniGraphButton(Paint paint, QWidget* parent)
        : QToolButton(parent)
        , m_paint(std::move(paint))
    {
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        m_paint(painter, rect());
        if (underMouse() || hasFocus() || isDown()) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            QColor frame = palette().color(QPalette::Highlight);
            painter.setPen(QPen(frame, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        }
    }

private:
    Paint m_paint;
};

}  // namespace

QString WritingStatsBar::Text::plain() const
{
    QString text;
    for (const Run& run : runs) {
        text += run.text;
    }
    return text;
}

WritingStatsBar::WritingStatsBar(WritingStatsModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("writingStatsBar"));
    setAccessibleName(tr("Statistics bar"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMouseTracking(true);

    const auto makeButton = [this](const QString& toolTip) {
        auto* button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setIconSize(QSize(14, 14));
        button->setToolTip(toolTip);
        button->setAccessibleName(toolTip);
        button->setFocusPolicy(Qt::TabFocus);
        return button;
    };
    m_fold = makeButton(QString());
    m_goals = makeButton(tr("Writing Goals and Deadlines"));
    m_panel = makeButton(tr("Statistics panel"));
    m_settings = makeButton(tr("Statistics settings"));
    // The mini graph of the line, a button of the mouse and the keys
    m_mini = new MiniGraphButton(
        [this](QPainter& painter, const QRect& area) { paintMiniGraph(painter, area); }, this);
    m_mini->setFocusPolicy(Qt::TabFocus);
    m_mini->setCursor(Qt::PointingHandCursor);

    connect(m_fold, &QToolButton::clicked, this, [this]() {
        core::SettingsManager::getInstance().set<std::string>(
            "stats.bar.mode", m_folded ? "graph" : "line");
        refresh();
    });
    connect(m_goals, &QToolButton::clicked, this, &WritingStatsBar::goalsRequested);
    connect(m_panel, &QToolButton::clicked, this, &WritingStatsBar::panelRequested);
    connect(m_settings, &QToolButton::clicked, this, &WritingStatsBar::settingsRequested);
    connect(m_mini, &QToolButton::clicked, this, &WritingStatsBar::cycleMiniGraph);

    if (m_model != nullptr) {
        connect(m_model, &WritingStatsModel::changed, this, &WritingStatsBar::refresh);
        connect(m_model, &WritingStatsModel::goalsChanged, this, &WritingStatsBar::refresh);
    }
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &WritingStatsBar::refresh);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this, &WritingStatsBar::refresh);
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &WritingStatsBar::updateIcons);

    updateIcons();
    refresh();
}

// =============================================================================
// Settings and numbers
// =============================================================================

void WritingStatsBar::refresh()
{
    const bool wasFolded = m_folded;
    const int oldHeight = graphHeight();
    readSettings();
    readNumbers();
    if (wasFolded != m_folded || oldHeight != graphHeight()) {
        updateGeometry();
    }
    updateIcons();
    layoutParts();
    update();
    m_mini->update();
}

void WritingStatsBar::readSettings()
{
    auto& settings = core::SettingsManager::getInstance();
    m_folded = settings.get<std::string>("stats.bar.mode", "graph") == "line";
    m_graphHeight = std::clamp(settings.get<int>("stats.bar.height", MIN_GRAPH_HEIGHT),
                               MIN_GRAPH_HEIGHT, 400);
    m_textsLeft = settings.get<std::string>("stats.bar.textSide", "right") == "left";
    m_showWords = settings.get<bool>("stats.bar.showWords", true);
    m_showTime = settings.get<bool>("stats.bar.showTime", true);
    m_showStreak = settings.get<bool>("stats.bar.showStreak", true);
    m_showPieces = settings.get<bool>("stats.bar.showPieces", true);
    m_showCumulative = settings.get<bool>("stats.bar.showCumulative", true);
    const std::string mini = settings.get<std::string>("stats.bar.miniGraph", "7");
    m_miniGraph = mini == "day" ? MiniGraph::Today
                  : mini == "30" ? MiniGraph::Month
                                 : MiniGraph::Week;
    m_pieceMinutes = settings.get<int>("stats.bar.pieceMinutes", 10);
    m_fromHour = std::clamp(settings.get<int>("stats.bar.fromHour", 6), 0, 23);
    m_toHour = std::clamp(settings.get<int>("stats.bar.toHour", 24), 1, 24);
}

void WritingStatsBar::readNumbers()
{
    m_colors = StatsColors::current();
    m_texts.clear();
    m_lineText = {};
    if (m_model == nullptr) {
        return;
    }
    const core::WritingRules& rules = m_model->rules();
    const core::WritingStatistics statistics = m_model->goalStatistics();
    const QDateTime now = m_model->now();
    const QDate today = m_model->today();
    m_counting = rules.counting;
    m_dayStart = statistics.rules().dayStartMinutes;

    m_pieces = WritingStatsModel::dayPieces(statistics, today, m_pieceMinutes);
    const int piece = m_pieces.isEmpty() ? m_pieceMinutes : DAY_MINUTES / int(m_pieces.size());
    m_pieceMinutes = piece;
    const core::WritingTotals totals = statistics.day(today);
    m_words = totals.counted(m_counting);
    m_activeMs = totals.activeMs;
    m_goal = m_model->dailyGoalToday();
    m_streak = statistics.streak(today);
    m_temperature = statistics.temperature(now);
    m_nowMinute = std::clamp<qint64>(m_model->minutesIntoDay(today, now), 0, DAY_MINUTES);

    // The hours of the settings, wider when the writing or now is outside them
    int from = ((m_fromHour * 60 - m_dayStart) % DAY_MINUTES + DAY_MINUTES) % DAY_MINUTES;
    int to = m_toHour * 60 - m_dayStart;
    while (to <= from) {
        to += DAY_MINUTES;
    }
    to = std::min(to, DAY_MINUTES);
    for (int i = 0; i < m_pieces.size(); ++i) {
        if (m_pieces[i].written != 0 || m_pieces[i].deleted != 0) {
            from = std::min(from, i * piece);
            to = std::max(to, (i + 1) * piece);
        }
    }
    from = std::min<int>(from, int(m_nowMinute));
    to = std::max<int>(to, int(m_nowMinute) + 1);
    from = std::max(0, hourBefore(from, m_dayStart));
    to = std::min(DAY_MINUTES, hourAfter(to, m_dayStart));
    m_firstPiece = from / piece;
    m_endPiece = std::max(m_firstPiece + 1, (to + piece - 1) / piece);

    // The mini graph of the line
    m_miniValues.clear();
    m_miniSum = 0;
    if (m_miniGraph == MiniGraph::Today) {
        const int nowPiece = int(m_nowMinute / piece);
        for (int i = m_firstPiece; i <= std::min(nowPiece, int(m_pieces.size()) - 1); ++i) {
            m_miniValues.append(std::max(0, m_pieces[i].counted(m_counting)));
        }
        m_miniSum = m_words;
    } else {
        const int days = m_miniGraph == MiniGraph::Week ? 7 : 30;
        for (int i = days - 1; i >= 0; --i) {
            const int words = statistics.day(today.addDays(-i)).counted(m_counting);
            m_miniValues.append(std::max(0, words));
            m_miniSum += words;
        }
    }

    // The texts
    const bool done = m_goal > 0 && m_words >= m_goal;
    m_stateColor = done ? m_colors.done : m_colors.temperature(m_temperature);
    const QColor wordsColor = m_temperature == core::WritingTemperature::Unknown && !done
                                  ? m_colors.text
                                  : m_stateColor;
    QString temperatureName;
    core::WritingTemperature shown = m_temperature;
    if (done) {
        temperatureName = tr("goal reached");
    } else {
        switch (shown) {
        case core::WritingTemperature::MuchFast:
            temperatureName = tr("a lot, fast");
            break;
        case core::WritingTemperature::MuchSlow:
            temperatureName = tr("a lot, slowly");
            break;
        case core::WritingTemperature::LittleFast:
            temperatureName = tr("a little, fast");
            break;
        case core::WritingTemperature::LittleSlow:
            temperatureName = tr("a little, slowly");
            break;
        case core::WritingTemperature::Idle:
            temperatureName = tr("pause");
            break;
        case core::WritingTemperature::Unknown:
            break;
        }
    }

    // The words: the numbers in their colors, the rest of the sentence as the language has it
    Text words;
    const QString pattern = m_goal > 0 ? tr("%1 of %2 words") : tr("%1 words");
    static const QRegularExpression PLACEHOLDER(QStringLiteral("%[12]"));
    int at = 0;
    for (auto match = PLACEHOLDER.globalMatch(pattern); match.hasNext();) {
        const QRegularExpressionMatch found = match.next();
        if (found.capturedStart() > at) {
            words.runs.append({pattern.mid(at, found.capturedStart() - at), m_colors.text});
        }
        if (found.captured() == QLatin1String("%1")) {
            words.runs.append({stats_painter::number(m_words), wordsColor, true});
        } else {
            words.runs.append({stats_painter::number(m_goal), m_colors.goal});
        }
        at = found.capturedEnd();
    }
    if (at < pattern.size()) {
        words.runs.append({pattern.mid(at), m_colors.text});
    }

    if (m_showWords) {
        m_texts.append(words);
        m_lineText = words;
        if (!temperatureName.isEmpty()) {
            Text temperature;
            temperature.hasGlyph = true;
            temperature.glyph = done ? stats_painter::Glyph::Check
                                     : stats_painter::Glyph::Thermometer;
            temperature.glyphColor = m_stateColor;
            temperature.runs.append({temperatureName, m_stateColor});
            m_texts.append(temperature);
            m_lineText.runs.append({tr(", "), m_colors.text});
            m_lineText.runs.append({temperatureName, m_stateColor});
        }
    }
    if (m_showTime) {
        Text time;
        time.hasGlyph = true;
        time.glyph = stats_painter::Glyph::Clock;
        time.glyphColor = m_colors.text;
        time.runs.append(
            {tr("%1 of writing").arg(stats_painter::hoursMinutes(m_activeMs)), m_colors.text});
        m_texts.append(time);
    }
    if (m_showStreak) {
        Text streak;
        streak.hasGlyph = true;
        streak.glyph = stats_painter::Glyph::Flame;
        streak.glyphColor = m_colors.text;
        streak.runs.append({tr("%Ln day(s) in a row", nullptr, m_streak), m_colors.text});
        m_texts.append(streak);
    }
}

QStringList WritingStatsBar::texts() const
{
    QStringList list;
    for (const Text* text : lineParts()) {
        list.append(text->plain());
    }
    return list;
}

QList<const WritingStatsBar::Text*> WritingStatsBar::lineParts() const
{
    // Folded, the words and the temperature are one text; the time and the days in a row
    // follow
    QList<const Text*> parts;
    if (m_folded && !m_lineText.runs.isEmpty()) {
        parts.append(&m_lineText);
    }
    for (const Text& text : m_texts) {
        const bool own = text.hasGlyph && (text.glyph == stats_painter::Glyph::Clock ||
                                           text.glyph == stats_painter::Glyph::Flame);
        if (!m_folded || own) {
            parts.append(&text);
        }
    }
    return parts;
}

// =============================================================================
// Sizes and places
// =============================================================================

QFont WritingStatsBar::smallFont(bool bold) const
{
    QFont small = font();
    if (small.pointSizeF() > 0) {
        small.setPointSizeF(std::max(7.0, small.pointSizeF() * 0.88));
    } else if (small.pixelSize() > 0) {
        small.setPixelSize(std::max(9, int(small.pixelSize() * 0.88)));
    }
    small.setBold(bold);
    return small;
}

int WritingStatsBar::rowHeight() const
{
    return QFontMetrics(smallFont()).height();
}

int WritingStatsBar::graphHeight() const
{
    // The buttons, and the texts under the line of the goal
    const int buttons = 2 + 4 * BUTTON_SIZE + 1;
    const int texts = int(m_texts.size()) * rowHeight() + 22;
    return std::max({m_graphHeight, MIN_GRAPH_HEIGHT, buttons, texts});
}

QSize WritingStatsBar::sizeHint() const
{
    return {400, m_folded ? LINE_HEIGHT : graphHeight()};
}

QSize WritingStatsBar::minimumSizeHint() const
{
    return {120, m_folded ? LINE_HEIGHT : graphHeight()};
}

double WritingStatsBar::textWidth(const Text& text) const
{
    const QFontMetricsF plain(smallFont());
    const QFontMetricsF bold(smallFont(true));
    double width = text.hasGlyph ? plain.height() * 0.8 + 3 : 0;
    for (const Run& run : text.runs) {
        width += (run.bold ? bold : plain).horizontalAdvance(run.text);
    }
    return width;
}

QString WritingStatsBar::miniGraphText() const
{
    switch (m_miniGraph) {
    case MiniGraph::Today:
        return tr("today: %1").arg(stats_painter::number(m_miniSum));
    case MiniGraph::Week:
        return tr("7 days: %1").arg(stats_painter::number(m_miniSum));
    case MiniGraph::Month:
        return tr("30 days: %1").arg(stats_painter::number(m_miniSum));
    }
    return {};
}

void WritingStatsBar::layoutParts()
{
    const int w = width();
    const int top = m_folded ? (LINE_HEIGHT - BUTTON_SIZE) / 2 : 2;
    // The fold button is always in the top right corner; the others beside it or under it
    m_fold->move(w - 1 - BUTTON_SIZE, top);
    const QList<QToolButton*> others{m_goals, m_panel, m_settings};
    for (int i = 0; i < others.size(); ++i) {
        if (m_folded) {
            others[i]->move(w - 1 - BUTTON_SIZE * (4 - i), top);
        } else {
            others[i]->move(w - 1 - BUTTON_SIZE, top + BUTTON_SIZE * (i + 1));
        }
    }

    m_mini->setVisible(m_folded);
    if (m_folded) {
        const QFontMetrics metrics(smallFont());
        const int graphWidth = m_miniGraph == MiniGraph::Week ? 44 : 60;
        m_miniRect = QRect(4, 2, graphWidth + 4 + metrics.horizontalAdvance(miniGraphText()) + 8,
                           LINE_HEIGHT - 4);
        m_mini->setGeometry(m_miniRect);
        const QString name = miniGraphText();
        m_mini->setToolTip(tr("%1. Click: today, 7 or 30 days").arg(name));
        m_mini->setAccessibleName(name);
        m_plot = {};
        m_textsRect = {};
        return;
    }

    double textsWidth = 0;
    for (const Text& text : m_texts) {
        textsWidth = std::max(textsWidth, textWidth(text));
    }
    const double box = m_texts.isEmpty() ? 0 : textsWidth + 2 * TEXTS_PADDING;
    const double h = graphHeight();
    double x0 = 1;
    double x1 = w - BUTTONS_ROOM - box - 2;
    if (m_textsLeft) {
        x0 = box + 4;
        x1 = w - BUTTONS_ROOM - 2;
    }
    if (x1 - x0 < 60) {
        x1 = x0 + 60;
    }
    m_plot = QRectF(x0, 5, x1 - x0, h - 6);

    // The texts under the line of the goal, so that it does not cross them
    double textsTop = 5;
    if (m_goal > 0 && m_showCumulative) {
        const double cmax = std::max({m_goal * 1.15, m_words * 1.05, 10.0});
        textsTop = std::round(m_plot.bottom() - m_goal / cmax * m_plot.height()) + 2;
    }
    const double textsHeight = m_texts.size() * rowHeight() + 3;
    textsTop = std::max(2.0, std::min(textsTop, h - textsHeight - 2));
    const double left = m_textsLeft ? 4 : w - BUTTONS_ROOM - 2 - box;
    m_textsRect = QRectF(left, textsTop, box, textsHeight);
}

void WritingStatsBar::updateIcons()
{
    auto& art = core::ArtProvider::getInstance();
    m_fold->setIcon(art.getIcon(m_folded ? QStringLiteral("common.expandMore")
                                         : QStringLiteral("common.expandLess"),
                                core::IconContext::Menu));
    const QString foldTip = m_folded ? tr("Unfold the bar to the graph")
                                     : tr("Fold the bar to one line");
    m_fold->setToolTip(foldTip);
    m_fold->setAccessibleName(foldTip);
    m_goals->setIcon(art.getIcon(QStringLiteral("tools.writingGoals"), core::IconContext::Menu));
    m_panel->setIcon(art.getIcon(QStringLiteral("menu.statistics"), core::IconContext::Menu));
    m_settings->setIcon(art.getIcon(QStringLiteral("edit.settings"), core::IconContext::Menu));
}

void WritingStatsBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutParts();
}

void WritingStatsBar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LanguageChange) {
        refresh();
    }
}

// =============================================================================
// Painting
// =============================================================================

void WritingStatsBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_colors.background);
    if (m_folded) {
        paintLine(painter);
    } else {
        paintGraph(painter);
    }
}

double WritingStatsBar::xOfMinute(double minute) const
{
    const double first = double(m_firstPiece) * m_pieceMinutes;
    const double last = double(m_endPiece) * m_pieceMinutes;
    return m_plot.left() + (minute - first) / std::max(1.0, last - first) * m_plot.width();
}

double WritingStatsBar::drawText(QPainter& painter, const Text& text, double x,
                                 double baseline) const
{
    const QFont plain = smallFont();
    const QFont bold = smallFont(true);
    const QFontMetricsF metrics(plain);
    if (text.hasGlyph) {
        const double size = metrics.height() * 0.8;
        stats_painter::drawGlyph(painter, text.glyph,
                                 QRectF(x, baseline - metrics.ascent() + (metrics.ascent() - size) / 2 + 1,
                                        size, size),
                                 text.glyphColor);
        x += size + 3;
    }
    for (const Run& run : text.runs) {
        painter.setFont(run.bold ? bold : plain);
        painter.setPen(run.color);
        painter.drawText(QPointF(x, baseline), run.text);
        x += QFontMetricsF(run.bold ? bold : plain).horizontalAdvance(run.text);
    }
    return x;
}

void WritingStatsBar::paintGraph(QPainter& painter)
{
    const double w = width();
    const double h = height();
    const QRectF& plot = m_plot;
    const int piece = m_pieceMinutes;

    // Layer 1: the grid of the hours, on the whole bar, also under the texts and the buttons
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(m_colors.grid, 1));
    const double hourWidth = xOfMinute(60) - xOfMinute(0);
    if (hourWidth > 2) {
        int minute = hourBefore(m_firstPiece * piece, m_dayStart);
        while (xOfMinute(minute) > 1) {
            minute -= 60;
        }
        for (; xOfMinute(minute) < w - 1; minute += 60) {
            const double x = std::round(xOfMinute(minute)) + 0.5;
            if (x > 1 && x < w - 1) {
                painter.drawLine(QPointF(x, 0), QPointF(x, h));
            }
        }
    }
    for (int i = 1; i < 4; ++i) {
        const double y = std::round(h * i / 4) + 0.5;
        painter.drawLine(QPointF(0, y), QPointF(w, y));
    }

    const int nowPiece = std::min<int>(int(m_nowMinute / piece), int(m_pieces.size()) - 1);
    const int lastPiece = std::min(nowPiece, m_endPiece - 1);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Layer 2: the words of each piece, a thin line with a translucent fill
    if (m_showPieces && lastPiece >= m_firstPiece) {
        int top = 0;
        for (int i = m_firstPiece; i <= lastPiece; ++i) {
            top = std::max(top, m_pieces[i].counted(m_counting));
        }
        const double max = stats_painter::niceMax(top);
        QPainterPath line;
        for (int i = m_firstPiece; i <= lastPiece; ++i) {
            const double v = std::max(0, m_pieces[i].counted(m_counting));
            const QPointF point(xOfMinute((i + 0.5) * piece),
                                plot.bottom() - v / max * plot.height());
            if (i == m_firstPiece) {
                line.moveTo(point);
            } else {
                line.lineTo(point);
            }
        }
        QPainterPath area = line;
        area.lineTo(line.currentPosition().x(), h);
        area.lineTo(xOfMinute((m_firstPiece + 0.5) * piece), h);
        area.closeSubpath();
        painter.fillPath(area, m_colors.fill());
        painter.setPen(QPen(m_colors.line, 1.2));
        painter.drawPath(line);
    }

    // Layer 2: the words of today adding up toward the line of the daily goal
    const double cmax = std::max({m_goal * 1.15, m_words * 1.05, 10.0});
    const auto cy = [&](double v) { return plot.bottom() - v / cmax * plot.height(); };
    if (m_showCumulative && m_goal > 0) {
        QPen dashed(m_colors.goal, 1);
        dashed.setDashPattern({3, 3});
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(dashed);
        const double gy = std::round(cy(m_goal)) + 0.5;
        painter.drawLine(QPointF(0, gy), QPointF(w, gy));
        painter.setRenderHint(QPainter::Antialiasing, true);
    }
    double sum = 0;
    for (int i = 0; i < m_firstPiece && i < m_pieces.size(); ++i) {
        sum += m_pieces[i].counted(m_counting);
    }
    if (m_showCumulative && lastPiece >= m_firstPiece) {
        QPainterPath cumulative(QPointF(xOfMinute(m_firstPiece * piece), cy(std::max(0.0, sum))));
        for (int i = m_firstPiece; i <= lastPiece; ++i) {
            sum += m_pieces[i].counted(m_counting);
            cumulative.lineTo(xOfMinute((i + 1) * piece), cy(std::max(0.0, sum)));
        }
        painter.setPen(QPen(m_stateColor, 1.6));
        painter.drawPath(cumulative);
    }

    // Now
    const double nx = xOfMinute(double(m_nowMinute));
    if (nx >= plot.left() && nx <= plot.right()) {
        QColor nowColor = m_colors.text;
        nowColor.setAlphaF(0.45);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(nowColor, 1));
        painter.drawLine(QPointF(std::round(nx) + 0.5, 0), QPointF(std::round(nx) + 0.5, h));
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (m_showCumulative) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_stateColor);
            painter.drawEllipse(QPointF(nx, cy(std::max(0.0, sum))), 2.4, 2.4);
            painter.setBrush(Qt::NoBrush);
        }
    }

    // Layer 1: the hours in small print at the bottom of the grid, every two hours
    QFont hours = smallFont();
    hours.setPointSizeF(std::max(6.5, hours.pointSizeF() * 0.92));
    painter.setFont(hours);
    QColor hoursColor = m_colors.text;
    hoursColor.setAlphaF(0.8);
    painter.setPen(hoursColor);
    const QFontMetricsF hourMetrics(hours);
    const double labelWidth = hourMetrics.horizontalAdvance(QStringLiteral("00:00")) + 6;
    int step = 120;
    while (step < DAY_MINUTES && (xOfMinute(step) - xOfMinute(0)) < labelWidth) {
        step += 120;
    }
    for (int minute = hourAfter(m_firstPiece * piece, m_dayStart);
         minute <= m_endPiece * piece; minute += 60) {
        const int clock = ((minute + m_dayStart) / 60) % 24;
        if (clock % (step / 60) != 0 && minute != hourAfter(m_firstPiece * piece, m_dayStart)) {
            continue;
        }
        const QString label = QStringLiteral("%1:00").arg(clock);
        const double x = xOfMinute(minute);
        const double labelX = minute == m_firstPiece * piece
                                  ? x + 3
                                  : x - hourMetrics.horizontalAdvance(label) / 2;
        if (labelX < plot.left() || labelX + hourMetrics.horizontalAdvance(label) > plot.right() + 2) {
            continue;
        }
        painter.drawText(QPointF(labelX, h - 3), label);
    }

    // Layer 4: the texts, in one place, in small print
    if (!m_texts.isEmpty()) {
        QColor box = m_colors.background;
        box.setAlphaF(0.72);
        painter.setPen(Qt::NoPen);
        painter.setBrush(box);
        painter.drawRoundedRect(m_textsRect, 3, 3);
        painter.setBrush(Qt::NoBrush);
        const QFontMetricsF metrics(smallFont());
        double baseline = m_textsRect.top() + 1 + metrics.ascent();
        for (const Text& text : m_texts) {
            const double width = textWidth(text);
            const double x = m_textsLeft ? m_textsRect.left() + TEXTS_PADDING
                                         : m_textsRect.right() - TEXTS_PADDING - width;
            drawText(painter, text, x, baseline);
            baseline += rowHeight();
        }
    }

    // The frame
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(m_colors.frame, 1));
    painter.drawRect(QRectF(0.5, 0.5, w - 1, h - 1));
}

void WritingStatsBar::paintLine(QPainter& painter)
{
    const double w = width();
    const double h = height();

    // The line fills up with the color of the temperature as the words of the day grow
    if (m_goal > 0) {
        const double share = std::clamp(double(m_words) / m_goal, 0.0, 1.0);
        QColor fill = m_stateColor;
        fill.setAlphaF(0.16);
        painter.fillRect(QRectF(0, 0, w * share, h), fill);
        painter.fillRect(QRectF(0, h - 2, w * share, 2), m_stateColor);
    }

    const QFontMetricsF metrics(smallFont());
    const double baseline = (h - metrics.height()) / 2 + metrics.ascent();

    // The texts, as far as there is room before the buttons
    const double end = w - 1 - 4 * BUTTON_SIZE - 6;
    double x = m_miniRect.right() + LINE_GAP;
    painter.save();
    painter.setClipRect(QRectF(x, 0, std::max(0.0, end - x), h));
    QColor separator = m_colors.text;
    separator.setAlphaF(0.45);
    for (const Text* text : lineParts()) {
        painter.setFont(smallFont());
        painter.setPen(separator);
        painter.drawText(QPointF(x, baseline), QStringLiteral("|"));
        x += metrics.horizontalAdvance(QStringLiteral("|")) + LINE_GAP;
        x = drawText(painter, *text, x, baseline) + LINE_GAP;
    }
    painter.restore();
}

void WritingStatsBar::paintMiniGraph(QPainter& painter, const QRect& area) const
{
    const QFontMetricsF metrics(smallFont());
    const double baseline = (area.height() - metrics.height()) / 2 + metrics.ascent();
    const int graphWidth = m_miniGraph == MiniGraph::Week ? 44 : 60;
    const QRectF graph(3, (area.height() - 13) / 2.0, graphWidth, 13);
    QList<double> values;
    double top = 1;
    for (int v : m_miniValues) {
        values.append(v);
        top = std::max<double>(top, v);
    }
    stats_painter::drawArea(painter, graph, values, top, m_colors.line, m_colors.fill());
    painter.setFont(smallFont());
    painter.setPen(m_colors.text);
    painter.drawText(QPointF(graph.right() + 4, baseline), miniGraphText());
}

// =============================================================================
// Tooltips and the mini graph
// =============================================================================

int WritingStatsBar::pieceAt(double x) const
{
    if (m_folded || !m_plot.isValid() || x < m_plot.left() || x >= m_plot.right()) {
        return -1;
    }
    const double minute = m_firstPiece * double(m_pieceMinutes) +
                          (x - m_plot.left()) / m_plot.width() *
                              (m_endPiece - m_firstPiece) * double(m_pieceMinutes);
    const int piece = int(minute / m_pieceMinutes);
    return piece >= 0 && piece < m_pieces.size() ? piece : -1;
}

QString WritingStatsBar::pieceToolTip(int piece) const
{
    const auto clock = [this](int minute) {
        const int m = ((minute + m_dayStart) % DAY_MINUTES + DAY_MINUTES) % DAY_MINUTES;
        return QStringLiteral("%1:%2").arg(m / 60).arg(m % 60, 2, 10, QLatin1Char('0'));
    };
    const QString time = tr("%1–%2").arg(clock(piece * m_pieceMinutes),
                                         clock((piece + 1) * m_pieceMinutes));
    if (piece * m_pieceMinutes > m_nowMinute) {
        return time;
    }
    const DayPiece& entry = m_pieces[piece];
    if (entry.written == 0 && entry.deleted == 0) {
        return tr("%1: no writing").arg(time);
    }
    return tr("%1: %2 words (written %3, deleted %4)")
        .arg(time, stats_painter::number(entry.counted(m_counting)),
             stats_painter::number(entry.written), stats_painter::number(entry.deleted));
}

bool WritingStatsBar::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int piece = pieceAt(help->pos().x());
        if (piece >= 0 && !m_textsRect.contains(help->pos())) {
            QToolTip::showText(help->globalPos(), pieceToolTip(piece), this);
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return QWidget::event(event);
}

void WritingStatsBar::cycleMiniGraph()
{
    const char* next = m_miniGraph == MiniGraph::Today  ? "7"
                       : m_miniGraph == MiniGraph::Week ? "30"
                                                        : "day";
    core::SettingsManager::getInstance().set<std::string>("stats.bar.miniGraph", next);
    refresh();
}

}  // namespace kalahari::gui
