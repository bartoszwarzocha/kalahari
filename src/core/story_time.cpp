// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file story_time.cpp
/// @brief The time of a story: its days of the calendar, its place on a time axis, its text in
/// dates and in days of the story, and its object in a file

#include <kalahari/core/story_time.h>

#include <QDate>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace kalahari::core {

namespace {

/// Where a day without a part of the day stands: its middle
constexpr double MIDDLE_OF_DAY = 0.5;
/// Where a time of a month stands: about its middle
constexpr double MIDDLE_OF_MONTH = 14.0;
/// Where a time of a year stands: about its middle
constexpr double MIDDLE_OF_YEAR = 182.0;
constexpr int MINUTES_PER_DAY = 24 * 60;
constexpr int MONTHS = 12;

/// The minus sign of a negative day of the story: "Dzień −2"
const QString MINUS = QStringLiteral("−");
/// The dash between the ends of days in a row
const QString DASH = QStringLiteral("–");

constexpr std::array<const char*, MONTHS> ENGLISH_MONTHS{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};

const std::array<QString, MONTHS>& polishMonths() {
    static const std::array<QString, MONTHS> names{
        QStringLiteral("styczeń"),  QStringLiteral("luty"),     QStringLiteral("marzec"),
        QStringLiteral("kwiecień"), QStringLiteral("maj"),      QStringLiteral("czerwiec"),
        QStringLiteral("lipiec"),   QStringLiteral("sierpień"), QStringLiteral("wrzesień"),
        QStringLiteral("październik"), QStringLiteral("listopad"), QStringLiteral("grudzień")};
    return names;
}

/// Polish months after a day: "14 kwietnia"
const std::array<QString, MONTHS>& polishMonthsOfDays() {
    static const std::array<QString, MONTHS> names{
        QStringLiteral("stycznia"), QStringLiteral("lutego"),   QStringLiteral("marca"),
        QStringLiteral("kwietnia"), QStringLiteral("maja"),     QStringLiteral("czerwca"),
        QStringLiteral("lipca"),    QStringLiteral("sierpnia"), QStringLiteral("września"),
        QStringLiteral("października"), QStringLiteral("listopada"), QStringLiteral("grudnia")};
    return names;
}

/// The name of a month after a day: "kwietnia" in Polish, "April" in English
QString monthOfDay(int month, TimeLanguage language) {
    return language == TimeLanguage::Polish
               ? polishMonthsOfDays().at(static_cast<std::size_t>(month - 1))
               : QString::fromLatin1(ENGLISH_MONTHS.at(static_cast<std::size_t>(month - 1)));
}

/// The ids of the parts of the day in a file, in the order of Part (without None and Clock)
constexpr std::array<const char*, 6> PART_IDS{"dawn",      "morning", "noon",
                                              "afternoon", "evening", "night"};

/// Where on its day a part of the day stands
constexpr std::array<double, 6> PART_PLACES{0.22, 0.3, 0.5, 0.62, 0.78, 0.92};

/// The hour that stands for a part of the day when the style writes a clock
constexpr std::array<int, 6> PART_HOURS{5, 8, 12, 15, 19, 23};

constexpr std::array<const char*, 6> ENGLISH_PARTS{
    "at dawn", "in the morning", "at noon", "in the afternoon", "in the evening", "at night"};

const std::array<QString, 6>& polishParts() {
    static const std::array<QString, 6> words{
        QStringLiteral("o świcie"),    QStringLiteral("rano"),      QStringLiteral("w południe"),
        QStringLiteral("po południu"), QStringLiteral("wieczorem"), QStringLiteral("w nocy")};
    return words;
}

/// Index of a named part of the day in the tables above
std::size_t partIndex(StoryTime::Part part) {
    return static_cast<std::size_t>(part) - static_cast<std::size_t>(StoryTime::Part::Dawn);
}

bool isNamedPart(StoryTime::Part part) {
    return part != StoryTime::Part::None && part != StoryTime::Part::Clock;
}

QString clockText(int minutes) {
    return QStringLiteral("%1:%2")
        .arg(minutes / 60)
        .arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

/// The part of the day of @p time as @p style writes it; empty without one
QString partText(const StoryTime& time, const StoryTimeStyle& style) {
    if (time.part == StoryTime::Part::Clock) {
        return clockText(time.minutes);
    }
    if (!isNamedPart(time.part)) {
        return QString();
    }
    const std::size_t index = partIndex(time.part);
    if (style.parts == PartStyle::Clock) {
        return clockText(PART_HOURS.at(index) * 60);
    }
    return style.language == TimeLanguage::Polish ? polishParts().at(index)
                                                  : QString::fromLatin1(ENGLISH_PARTS.at(index));
}

QString approximately(TimeLanguage language) {
    return language == TimeLanguage::Polish ? QStringLiteral("ok. ") : QStringLiteral("c. ");
}

/// A day in numbers: "14.04.1931", "15.03.44 p.n.e."
QString dayInNumbers(const CalendarDay& day, TimeLanguage language) {
    return QStringLiteral("%1.%2.%3")
        .arg(day.day, 2, 10, QLatin1Char('0'))
        .arg(day.month, 2, 10, QLatin1Char('0'))
        .arg(StoryTime::yearText(day.year, language));
}

/// A day in ISO 8601: "1931-04-14"; in words before the Common Era
QString dayInIso(const CalendarDay& day, TimeLanguage language) {
    if (day.year <= 0) {
        return QStringLiteral("%1 %2 %3")
            .arg(day.day)
            .arg(monthOfDay(day.month, language), StoryTime::yearText(day.year, language));
    }
    return QStringLiteral("%1-%2-%3")
        .arg(day.year, 4, 10, QLatin1Char('0'))
        .arg(day.month, 2, 10, QLatin1Char('0'))
        .arg(day.day, 2, 10, QLatin1Char('0'));
}

QString dayInWords(const CalendarDay& day, TimeLanguage language) {
    return QStringLiteral("%1 %2 %3")
        .arg(day.day)
        .arg(monthOfDay(day.month, language), StoryTime::yearText(day.year, language));
}

QString oneDay(const CalendarDay& day, const StoryTimeStyle& style) {
    switch (style.dates) {
    case DateStyle::Numbers:
        return dayInNumbers(day, style.language);
    case DateStyle::Iso:
        return dayInIso(day, style.language);
    case DateStyle::Words:
        break;
    }
    return dayInWords(day, style.language);
}

/// Days in a row: "16–17 kwietnia 1931", "2 marca – 5 kwietnia 1931", or both days whole
QString daysInRow(const CalendarDay& from, const CalendarDay& to, const StoryTimeStyle& style) {
    if (style.dates == DateStyle::Words && from.year == to.year) {
        const QString year = StoryTime::yearText(to.year, style.language);
        if (from.month == to.month) {
            return QStringLiteral("%1%2%3 %4 %5")
                .arg(from.day)
                .arg(DASH)
                .arg(to.day)
                .arg(monthOfDay(to.month, style.language), year);
        }
        return QStringLiteral("%1 %2 %3 %4 %5 %6")
            .arg(from.day)
            .arg(monthOfDay(from.month, style.language), DASH)
            .arg(to.day)
            .arg(monthOfDay(to.month, style.language), year);
    }
    return QStringLiteral("%1 %2 %3").arg(oneDay(from, style), DASH, oneDay(to, style));
}

/// A number of a day of the story with a true minus sign: "3", "−2"
QString dayNumberText(int number) {
    return number < 0 ? MINUS + QString::number(-number) : QString::number(number);
}

/// The Polish form of a count: 1 "rok", 2-4 (not 12-14) "lata", else "lat"
QString polishCount(int count, const char* one, const char* few, const char* many) {
    const int lastDigit = count % 10;
    const int lastTwo = count % 100;
    if (count == 1) {
        return QString::fromUtf8(one);
    }
    if (lastDigit >= 2 && lastDigit <= 4 && (lastTwo < 12 || lastTwo > 14)) {
        return QString::fromUtf8(few);
    }
    return QString::fromUtf8(many);
}

QString englishCount(int count, const char* one, const char* many) {
    return QString::fromLatin1(count == 1 ? one : many);
}

/// The day of a file: "1931-04-14", "-0043-03-15", "1931-04", "1931"
QString fileDay(qint64 julianDay, StoryTime::Precision precision) {
    const CalendarDay day = CalendarDay::of(julianDay);
    QString text = QStringLiteral("%1%2")
                       .arg(day.year < 0 ? QStringLiteral("-") : QString())
                       .arg(std::abs(day.year), 4, 10, QLatin1Char('0'));
    if (precision != StoryTime::Precision::Year) {
        text += QStringLiteral("-%1").arg(day.month, 2, 10, QLatin1Char('0'));
    }
    if (precision == StoryTime::Precision::Day) {
        text += QStringLiteral("-%1").arg(day.day, 2, 10, QLatin1Char('0'));
    }
    return text;
}

/// A day of a file with how exactly it is known, or nullopt when the text is none
struct FileDay {
    qint64 julianDay = 0;
    StoryTime::Precision precision = StoryTime::Precision::Day;
};

std::optional<FileDay> readFileDay(const QString& text) {
    static const QRegularExpression pattern(
        QStringLiteral("^(-?\\d{4,})(?:-(\\d{2})(?:-(\\d{2}))?)?$"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    CalendarDay day{match.captured(1).toInt(), 1, 1};
    FileDay result{0, StoryTime::Precision::Year};
    if (match.hasCaptured(2)) {
        day.month = match.captured(2).toInt();
        result.precision = StoryTime::Precision::Month;
    }
    if (match.hasCaptured(3)) {
        day.day = match.captured(3).toInt();
        result.precision = StoryTime::Precision::Day;
    }
    const std::optional<qint64> julianDay = day.julianDay();
    if (!julianDay) {
        return std::nullopt;
    }
    result.julianDay = *julianDay;
    return result;
}

/// The part of the day of a file: "evening", "19:30"; nullopt when it is none
std::optional<std::pair<StoryTime::Part, int>> readFilePart(const QString& text) {
    for (std::size_t i = 0; i < PART_IDS.size(); ++i) {
        if (text == QLatin1String(PART_IDS.at(i))) {
            return std::make_pair(
                static_cast<StoryTime::Part>(static_cast<std::size_t>(StoryTime::Part::Dawn) + i),
                0);
        }
    }
    static const QRegularExpression clock(QStringLiteral("^(\\d{1,2}):(\\d{2})$"));
    const QRegularExpressionMatch match = clock.match(text);
    const int hours = match.captured(1).toInt();
    const int minutes = match.captured(2).toInt();
    if (!match.hasMatch() || hours > 23 || minutes > 59) {
        return std::nullopt;
    }
    return std::make_pair(StoryTime::Part::Clock, hours * 60 + minutes);
}

}  // namespace

// =============================================================================
// CalendarDay
// =============================================================================

std::optional<qint64> CalendarDay::julianDay() const {
    // QDate has no year 0: 1 BC is its year -1
    const QDate date(year <= 0 ? year - 1 : year, month, day);
    if (!date.isValid()) {
        return std::nullopt;
    }
    return date.toJulianDay();
}

CalendarDay CalendarDay::of(qint64 julianDay) {
    const QDate date = QDate::fromJulianDay(julianDay);
    const int year = date.year();
    return CalendarDay{year < 0 ? year + 1 : year, date.month(), date.day()};
}

// =============================================================================
// StoryTime
// =============================================================================

StoryTime StoryTime::day(qint64 julianDay) {
    StoryTime time;
    time.first = julianDay;
    time.last = julianDay;
    return time;
}

double StoryTime::start() const {
    const auto day = static_cast<double>(first);
    switch (precision) {
    case Precision::Month:
        return day + MIDDLE_OF_MONTH;
    case Precision::Year:
        return day + MIDDLE_OF_YEAR;
    case Precision::Day:
        break;
    }
    if (part == Part::Clock) {
        return day + static_cast<double>(minutes) / MINUTES_PER_DAY;
    }
    if (isNamedPart(part)) {
        return day + PART_PLACES.at(partIndex(part));
    }
    // Days in a row without a part of the day start at the start of their first day
    return isSpan() ? day : day + MIDDLE_OF_DAY;
}

double StoryTime::end() const {
    return isSpan() ? static_cast<double>(last) + 1.0 : start();
}

StoryTime StoryTime::movedByDays(qint64 days) const {
    StoryTime moved = *this;
    moved.first += days;
    moved.last += days;
    if (precision != Precision::Day) {
        // A month or a year starts on its first day
        const CalendarDay day = CalendarDay::of(moved.first);
        const CalendarDay start{day.year, precision == Precision::Year ? 1 : day.month, 1};
        moved.first = start.julianDay().value_or(moved.first);
        moved.last = moved.first;
    }
    return moved;
}

StoryTime StoryTime::movedByMonths(int months) const {
    const auto moveDay = [months](qint64 julianDay) {
        const CalendarDay day = CalendarDay::of(julianDay);
        const int index = day.year * MONTHS + (day.month - 1) + months;
        const int year = index >= 0 ? index / MONTHS : (index - (MONTHS - 1)) / MONTHS;
        const int month = index - year * MONTHS + 1;
        CalendarDay moved{year, month, day.day};
        while (!moved.julianDay() && moved.day > 1) {
            --moved.day;
        }
        return moved.julianDay().value_or(julianDay);
    };
    StoryTime moved = *this;
    moved.first = moveDay(first);
    moved.last = isSpan() ? moveDay(last) : moved.first;
    if (moved.last < moved.first) {
        moved.last = moved.first;
    }
    return moved;
}

QString StoryTime::dateText(const StoryTimeStyle& style) const {
    const CalendarDay from = CalendarDay::of(first);
    QString text;
    switch (precision) {
    case Precision::Year:
        text = yearText(from.year, style.language);
        break;
    case Precision::Month:
        text = QStringLiteral("%1 %2").arg(monthName(from.month, style.language),
                                           yearText(from.year, style.language));
        break;
    case Precision::Day:
        text = isSpan() ? daysInRow(from, CalendarDay::of(last), style) : oneDay(from, style);
        break;
    }
    const QString partOfDay = partText(*this, style);
    if (!partOfDay.isEmpty()) {
        text += QStringLiteral(", ") + partOfDay;
    }
    return approximate ? approximately(style.language) + text : text;
}

QString StoryTime::dayText(qint64 dayOne, const StoryTimeStyle& style) const {
    const bool polish = style.language == TimeLanguage::Polish;
    if (precision != Precision::Day) {
        const int middle = dayNumber(static_cast<qint64>(std::floor(start())), dayOne);
        return approximately(style.language) + (polish ? QStringLiteral("Dzień ")
                                                       : QStringLiteral("Day ")) +
               dayNumberText(middle);
    }
    QString text;
    if (isSpan()) {
        text = QStringLiteral("%1 %2%3%4")
                   .arg(polish ? QStringLiteral("Dni") : QStringLiteral("Days"),
                        dayNumberText(dayNumber(first, dayOne)), DASH,
                        dayNumberText(dayNumber(last, dayOne)));
    } else {
        text = QStringLiteral("%1 %2").arg(polish ? QStringLiteral("Dzień") : QStringLiteral("Day"),
                                           dayNumberText(dayNumber(first, dayOne)));
    }
    const QString partOfDay = partText(*this, style);
    if (!partOfDay.isEmpty()) {
        text += QStringLiteral(", ") + partOfDay;
    }
    return approximate ? approximately(style.language) + text : text;
}

int StoryTime::dayNumber(qint64 julianDay, qint64 dayOne) {
    const qint64 after = julianDay - dayOne;
    return static_cast<int>(after >= 0 ? after + 1 : after);
}

qint64 StoryTime::dayOfNumber(int number, qint64 dayOne) {
    return number > 0 ? dayOne + number - 1 : dayOne + number;
}

QString StoryTime::lengthText(double days, TimeLanguage language) {
    constexpr double YEARS_FROM = 540.0;
    constexpr double MONTHS_FROM = 45.0;
    constexpr double WEEKS_FROM = 14.0;
    constexpr double DAYS_PER_YEAR = 365.0;
    constexpr double DAYS_PER_MONTH = 30.4;
    constexpr double DAYS_PER_WEEK = 7.0;
    const bool polish = language == TimeLanguage::Polish;
    const QString about = QStringLiteral("≈ ");
    if (days >= YEARS_FROM) {
        const auto years = static_cast<int>(std::lround(days / DAYS_PER_YEAR));
        return about + QString::number(years) + QLatin1Char(' ') +
               (polish ? polishCount(years, "rok", "lata", "lat")
                       : englishCount(years, "year", "years"));
    }
    if (days >= MONTHS_FROM) {
        const auto months = static_cast<int>(std::lround(days / DAYS_PER_MONTH));
        return about + QString::number(months) + QLatin1Char(' ') +
               (polish ? QStringLiteral("mies.") : englishCount(months, "month", "months"));
    }
    if (days >= WEEKS_FROM) {
        const auto weeks = static_cast<int>(std::lround(days / DAYS_PER_WEEK));
        return about + QString::number(weeks) + QLatin1Char(' ') +
               (polish ? QStringLiteral("tyg.") : englishCount(weeks, "week", "weeks"));
    }
    const int count = std::max(1, static_cast<int>(std::lround(days)));
    return QString::number(count) + QLatin1Char(' ') +
           (polish ? polishCount(count, "dzień", "dni", "dni") : englishCount(count, "day", "days"));
}

QString StoryTime::yearText(int year, TimeLanguage language) {
    if (year > 0) {
        return QString::number(year);
    }
    return QString::number(1 - year) +
           (language == TimeLanguage::Polish ? QStringLiteral(" p.n.e.") : QStringLiteral(" BC"));
}

QString StoryTime::monthName(int month, TimeLanguage language) {
    if (month < 1 || month > MONTHS) {
        return QString();
    }
    return language == TimeLanguage::Polish
               ? polishMonths().at(static_cast<std::size_t>(month - 1))
               : QString::fromLatin1(ENGLISH_MONTHS.at(static_cast<std::size_t>(month - 1)));
}

QJsonObject StoryTime::toJson() const {
    QJsonObject json{{QStringLiteral("from"), fileDay(first, precision)}};
    if (isSpan()) {
        json.insert(QStringLiteral("to"), fileDay(last, Precision::Day));
    }
    if (part == Part::Clock) {
        json.insert(QStringLiteral("part"), clockText(minutes));
    } else if (isNamedPart(part)) {
        json.insert(QStringLiteral("part"), QLatin1String(PART_IDS.at(partIndex(part))));
    }
    if (approximate) {
        json.insert(QStringLiteral("approx"), true);
    }
    return json;
}

std::optional<StoryTime> StoryTime::fromJson(const QJsonObject& json) {
    const std::optional<FileDay> from = readFileDay(json.value(QStringLiteral("from")).toString());
    if (!from) {
        return std::nullopt;
    }
    StoryTime time;
    time.first = from->julianDay;
    time.last = from->julianDay;
    time.precision = from->precision;
    time.approximate = json.value(QStringLiteral("approx")).toBool();
    if (time.precision == Precision::Day && json.contains(QStringLiteral("to"))) {
        const std::optional<FileDay> to = readFileDay(json.value(QStringLiteral("to")).toString());
        if (to && to->precision == Precision::Day && to->julianDay >= time.first) {
            time.last = to->julianDay;
        }
    }
    if (time.precision == Precision::Day) {
        if (const auto part = readFilePart(json.value(QStringLiteral("part")).toString())) {
            time.part = part->first;
            time.minutes = part->second;
        }
    }
    return time;
}

}  // namespace kalahari::core
