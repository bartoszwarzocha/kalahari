/// @file stats_painter.cpp
/// @brief The colors and the drawing of the statistics

#include "kalahari/gui/widgets/stats_painter.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>

namespace kalahari::gui {

namespace {

/// @brief The colors when a theme file has none (those of the Light theme)
struct DefaultColor {
    const char* key;
    const char* light;
    const char* dark;
};

constexpr std::array<DefaultColor, 17> DEFAULT_COLORS{{
    {"statsBackground", "#FFFFFF", "#161B1F"},
    {"statsGrid", "#E2ECF2", "#25313A"},
    {"statsFrame", "#8FB5D0", "#3C6B87"},
    {"statsLine", "#1673B8", "#4CB0E8"},
    {"statsGoal", "#A86F00", "#E3B341"},
    {"statsDone", "#2E7D32", "#5CC178"},
    {"statsMuchFast", "#D4521F", "#EF6B3B"},
    {"statsMuchSlow", "#B7790A", "#E7A93B"},
    {"statsLittleFast", "#1B88B3", "#4FB6DC"},
    {"statsLittleSlow", "#6B7088", "#8B90A6"},
    {"statsHoursLow", "#EEF4F8", "#1C2730"},
    {"statsHoursHigh", "#1673B8", "#4CB0E8"},
    {"statsText", "#2E3D49", "#C3D0DA"},
    {"statsBadgeBronze", "#A3532A", "#C98A55"},
    {"statsBadgeSilver", "#6F7D88", "#B9C3CC"},
    {"statsBadgeGold", "#B08400", "#E3B341"},
    {"statsBadgePlatinum", "#23879A", "#8FD8E8"},
}};

bool isDarkTheme()
{
    return core::ThemeManager::getInstance().getCurrentTheme().palette.window.lightness() < 128;
}

}  // namespace

// =============================================================================
// StatsColors
// =============================================================================

QColor StatsColors::color(const std::string& key) const
{
    static const std::array<std::pair<const char*, QColor StatsColors::*>, 17> MEMBERS{{
        {"statsBackground", &StatsColors::background},
        {"statsGrid", &StatsColors::grid},
        {"statsFrame", &StatsColors::frame},
        {"statsLine", &StatsColors::line},
        {"statsGoal", &StatsColors::goal},
        {"statsDone", &StatsColors::done},
        {"statsMuchFast", &StatsColors::muchFast},
        {"statsMuchSlow", &StatsColors::muchSlow},
        {"statsLittleFast", &StatsColors::littleFast},
        {"statsLittleSlow", &StatsColors::littleSlow},
        {"statsHoursLow", &StatsColors::hoursLow},
        {"statsHoursHigh", &StatsColors::hoursHigh},
        {"statsText", &StatsColors::text},
        {"statsBadgeBronze", &StatsColors::bronze},
        {"statsBadgeSilver", &StatsColors::silver},
        {"statsBadgeGold", &StatsColors::gold},
        {"statsBadgePlatinum", &StatsColors::platinum},
    }};
    for (const auto& [name, member] : MEMBERS) {
        if (key == name) {
            return this->*member;
        }
    }
    return {};
}

QColor StatsColors::fill() const
{
    QColor color = line;
    color.setAlphaF(float(std::clamp(fillOpacity, 0, 100)) / 100.0F);
    return color;
}

QColor StatsColors::temperature(core::WritingTemperature temperature) const
{
    switch (temperature) {
    case core::WritingTemperature::MuchFast:
        return muchFast;
    case core::WritingTemperature::MuchSlow:
        return muchSlow;
    case core::WritingTemperature::LittleFast:
        return littleFast;
    case core::WritingTemperature::LittleSlow:
        return littleSlow;
    case core::WritingTemperature::Unknown:
    case core::WritingTemperature::Idle:
        break;
    }
    return line;
}

QColor StatsColors::badge(int level) const
{
    switch (level) {
    case 0:
        return bronze;
    case 1:
        return silver;
    case 2:
        return gold;
    default:
        return platinum;
    }
}

StatsColors StatsColors::current()
{
    const auto& themes = core::ThemeManager::getInstance();
    const bool dark = isDarkTheme();
    StatsColors colors;
    const auto take = [&](const DefaultColor& entry) {
        return themes.editorColor(entry.key, QColor(QString::fromLatin1(dark ? entry.dark
                                                                                : entry.light)));
    };
    colors.background = take(DEFAULT_COLORS[0]);
    colors.grid = take(DEFAULT_COLORS[1]);
    colors.frame = take(DEFAULT_COLORS[2]);
    colors.line = take(DEFAULT_COLORS[3]);
    colors.goal = take(DEFAULT_COLORS[4]);
    colors.done = take(DEFAULT_COLORS[5]);
    colors.muchFast = take(DEFAULT_COLORS[6]);
    colors.muchSlow = take(DEFAULT_COLORS[7]);
    colors.littleFast = take(DEFAULT_COLORS[8]);
    colors.littleSlow = take(DEFAULT_COLORS[9]);
    colors.hoursLow = take(DEFAULT_COLORS[10]);
    colors.hoursHigh = take(DEFAULT_COLORS[11]);
    colors.text = take(DEFAULT_COLORS[12]);
    colors.bronze = take(DEFAULT_COLORS[13]);
    colors.silver = take(DEFAULT_COLORS[14]);
    colors.gold = take(DEFAULT_COLORS[15]);
    colors.platinum = take(DEFAULT_COLORS[16]);
    colors.fillOpacity = core::SettingsManager::getInstance().get<int>(
        fillOpacitySetting(themes.getCurrentTheme().name), defaultFillOpacity(dark));
    return colors;
}

const std::vector<std::string>& StatsColors::keys()
{
    static const std::vector<std::string> KEYS = [] {
        std::vector<std::string> keys;
        keys.reserve(DEFAULT_COLORS.size());
        for (const DefaultColor& entry : DEFAULT_COLORS) {
            keys.emplace_back(entry.key);
        }
        return keys;
    }();
    return KEYS;
}

std::string StatsColors::fillOpacitySetting(const std::string& themeName)
{
    return "stats.fillOpacity." + themeName;
}

int StatsColors::defaultFillOpacity(bool darkTheme)
{
    return darkTheme ? 22 : 14;
}

// =============================================================================
// Drawing
// =============================================================================

namespace stats_painter {

double niceMax(double value, double minimum)
{
    const double v = std::max(value, minimum);
    if (!(v > 0.0) || !std::isfinite(v)) {
        return 1.0;
    }
    static constexpr std::array<double, 11> STEPS{1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10};
    const double power = std::pow(10.0, std::floor(std::log10(v)));
    for (double step : STEPS) {
        // A little tolerance keeps 1200 at 1200, not 1500
        if (step * power >= v * (1.0 - 1e-9)) {
            return step * power;
        }
    }
    return 10.0 * power;
}

QString number(qint64 value)
{
    return QLocale().toString(value);
}

QString words(qint64 count)
{
    if (count == 1) {
        //: In Polish: 1 słowo
        return QCoreApplication::translate("StatsPainter", "1 word");
    }
    // The form of the language follows the number; the count of words fits easily
    const int forms = int(std::clamp<qint64>(count, -1'000'000'000, 1'000'000'000));
    //: In Polish: %1 słowo / %1 słowa / %1 słów
    return QCoreApplication::translate("StatsPainter", "%1 words", nullptr, forms)
        .arg(number(count));
}

QString hoursMinutes(qint64 ms)
{
    const qint64 minutes = std::max<qint64>(0, ms) / 60000;
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

QString temperatureName(core::WritingTemperature temperature)
{
    switch (temperature) {
    case core::WritingTemperature::MuchFast:
        //: In Polish: dużo i szybko
        return QCoreApplication::translate("StatsPainter", "a lot, fast");
    case core::WritingTemperature::MuchSlow:
        //: In Polish: dużo i powoli
        return QCoreApplication::translate("StatsPainter", "a lot, slowly");
    case core::WritingTemperature::LittleFast:
        //: In Polish: mało i szybko
        return QCoreApplication::translate("StatsPainter", "a little, fast");
    case core::WritingTemperature::LittleSlow:
        //: In Polish: mało i powoli
        return QCoreApplication::translate("StatsPainter", "a little, slowly");
    case core::WritingTemperature::Idle:
        //: In Polish: przerwa
        return QCoreApplication::translate("StatsPainter", "pause");
    case core::WritingTemperature::Unknown:
        break;
    }
    return QString();
}

void drawGrid(QPainter& painter, const QRectF& rect, int columns, int rows, const QColor& color)
{
    painter.save();
    painter.setPen(QPen(color, 1.0));
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 1; i < columns; ++i) {
        const double x = std::round(rect.left() + rect.width() * i / columns) + 0.5;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (int i = 1; i < rows; ++i) {
        const double y = std::round(rect.top() + rect.height() * i / rows) + 0.5;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
    painter.restore();
}

void drawArea(QPainter& painter, const QRectF& rect, const QList<double>& values, double max,
              const QColor& line, const QColor& fill)
{
    if (values.isEmpty() || !(max > 0.0)) {
        return;
    }
    const auto x = [&](int i) {
        return values.size() == 1 ? rect.center().x()
                                  : rect.left() + rect.width() * i / double(values.size() - 1);
    };
    const auto y = [&](double v) {
        return rect.bottom() - std::clamp(v / max, 0.0, 1.0) * rect.height();
    };
    QPainterPath path(QPointF(x(0), y(values[0])));
    for (int i = 1; i < values.size(); ++i) {
        path.lineTo(x(i), y(values[i]));
    }
    QPainterPath area = path;
    area.lineTo(x(int(values.size()) - 1), rect.bottom());
    area.lineTo(x(0), rect.bottom());
    area.closeSubpath();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillPath(area, fill);
    painter.setPen(QPen(line, 1.2));
    painter.drawPath(path);
    painter.restore();
}

void drawGlyph(QPainter& painter, Glyph glyph, const QRectF& rect, const QColor& color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double s = std::min(rect.width(), rect.height());
    const QPointF c = rect.center();
    QPen pen(color, std::max(1.0, s / 10.0));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Clock: {
        const double r = s * 0.42;
        painter.drawEllipse(c, r, r);
        painter.drawLine(c, QPointF(c.x(), c.y() - r * 0.6));
        painter.drawLine(c, QPointF(c.x() + r * 0.5, c.y()));
        break;
    }
    case Glyph::Flame: {
        QPainterPath path;
        const double l = c.x() - s * 0.32;
        const double r = c.x() + s * 0.32;
        const double b = c.y() + s * 0.44;
        const double t = c.y() - s * 0.46;
        path.moveTo(c.x(), b);
        path.cubicTo(l - s * 0.05, b, l, c.y() + s * 0.05, c.x() - s * 0.1, c.y() - s * 0.12);
        path.cubicTo(c.x() - s * 0.02, c.y() - s * 0.25, c.x() + s * 0.02, t + s * 0.1,
                     c.x(), t);
        path.cubicTo(r + s * 0.1, c.y() - s * 0.2, r, b - s * 0.05, c.x(), b);
        painter.drawPath(path);
        break;
    }
    case Glyph::Thermometer: {
        const double r = s * 0.17;
        const QPointF bulb(c.x(), c.y() + s * 0.28);
        painter.drawEllipse(bulb, r, r);
        painter.drawLine(QPointF(c.x(), bulb.y() - r), QPointF(c.x(), c.y() - s * 0.42));
        break;
    }
    case Glyph::Check: {
        QPainterPath path(QPointF(c.x() - s * 0.36, c.y()));
        path.lineTo(c.x() - s * 0.1, c.y() + s * 0.26);
        path.lineTo(c.x() + s * 0.38, c.y() - s * 0.28);
        painter.drawPath(path);
        break;
    }
    }
    painter.restore();
}

}  // namespace stats_painter

}  // namespace kalahari::gui
