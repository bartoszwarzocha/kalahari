// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file story_time_parse.cpp
/// @brief The time a writer types: dates, months, years and days of the story, in Polish and in
/// English, with a part of the day, "about" and years before the Common Era

#include <kalahari/timeline/story_time.h>

#include <QRegularExpression>
#include <QStringList>

#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::timeline {

namespace {

constexpr int MONTHS = 12;

/// The text without Polish diacritics, in lower case, with single spaces, its dashes as en
/// dashes and its minus signs as hyphens, so that "Dzień −2" and "dzien -2" read the same
QString normalized(const QString& text) {
    static const std::array<std::pair<QChar, QChar>, 9> letters{{
        {QChar(0x0105), QLatin1Char('a')},
        {QChar(0x0107), QLatin1Char('c')},
        {QChar(0x0119), QLatin1Char('e')},
        {QChar(0x0142), QLatin1Char('l')},
        {QChar(0x0144), QLatin1Char('n')},
        {QChar(0x00F3), QLatin1Char('o')},
        {QChar(0x015B), QLatin1Char('s')},
        {QChar(0x017A), QLatin1Char('z')},
        {QChar(0x017C), QLatin1Char('z')},
    }};
    QString result = text.toLower().simplified();
    for (const auto& [from, to] : letters) {
        result.replace(from, to);
    }
    result.replace(QChar(0x2212), QLatin1Char('-'));  // minus sign
    result.replace(QChar(0x2014), QChar(0x2013));      // em dash
    return result;
}

/// The month a word names, 1-12, or 0: Polish names in any case of the calendar, Polish and
/// English abbreviations, English names, Roman numbers
int monthOf(QString word) {
    static const std::array<QStringList, MONTHS> words{{
        {"styczen", "stycznia", "styczniu", "sty", "january", "jan", "i"},
        {"luty", "lutego", "lutym", "lut", "february", "feb", "ii"},
        {"marzec", "marca", "marcu", "mar", "march", "iii"},
        {"kwiecien", "kwietnia", "kwietniu", "kwi", "april", "apr", "iv"},
        {"maj", "maja", "maju", "may", "v"},
        {"czerwiec", "czerwca", "czerwcu", "cze", "june", "jun", "vi"},
        {"lipiec", "lipca", "lipcu", "lip", "july", "jul", "vii"},
        {"sierpien", "sierpnia", "sierpniu", "sie", "august", "aug", "viii"},
        {"wrzesien", "wrzesnia", "wrzesniu", "wrz", "september", "sep", "sept", "ix"},
        {"pazdziernik", "pazdziernika", "pazdzierniku", "paz", "october", "oct", "x"},
        {"listopad", "listopada", "listopadzie", "lis", "november", "nov", "xi"},
        {"grudzien", "grudnia", "grudniu", "gru", "december", "dec", "xii"},
    }};
    if (word.endsWith(QLatin1Char('.'))) {
        word.chop(1);
    }
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (words.at(i).contains(word)) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

/// A part of the day at the end of a text, after a comma or a space
struct PartOfDay {
    StoryTime::Part part = StoryTime::Part::None;
    int minutes = 0;
    qsizetype at = -1;  ///< Where it starts in the text, with its comma
};

PartOfDay namedPartAtEnd(const QString& text) {
    static const std::array<std::pair<StoryTime::Part, QStringList>, 6> parts{{
        {StoryTime::Part::Dawn,
         {"o swicie", "swit", "przed switem", "at dawn", "dawn", "at daybreak", "daybreak"}},
        {StoryTime::Part::Morning,
         {"rano", "rankiem", "z rana", "ranek", "in the morning", "morning"}},
        {StoryTime::Part::Noon,
         {"w poludnie", "poludnie", "at noon", "noon", "at midday", "midday"}},
        {StoryTime::Part::Afternoon,
         {"po poludniu", "popoludnie", "in the afternoon", "afternoon"}},
        {StoryTime::Part::Evening, {"wieczorem", "wieczor", "in the evening", "evening"}},
        {StoryTime::Part::Night,
         {"w nocy", "noca", "noc", "at night", "in the night", "night"}},
    }};
    PartOfDay found;
    for (const auto& [part, words] : parts) {
        for (const QString& word : words) {
            static const QRegularExpression before(QStringLiteral("[ ,]$"));
            const qsizetype at = text.size() - word.size();
            if (at > 0 && text.endsWith(word) && (found.at < 0 || at < found.at) &&
                before.match(text.left(at)).hasMatch()) {
                found = PartOfDay{part, 0, at};
            }
        }
    }
    return found;
}

PartOfDay clockAtEnd(const QString& text) {
    static const QRegularExpression clock(
        QStringLiteral("[ ,](?:o |at )?(\\d{1,2})[:.](\\d{2})$"));
    const QRegularExpressionMatch match = clock.match(text);
    const int hours = match.captured(1).toInt();
    const int minutes = match.captured(2).toInt();
    if (!match.hasMatch() || hours > 23 || minutes > 59) {
        return PartOfDay{};
    }
    return PartOfDay{StoryTime::Part::Clock, hours * 60 + minutes, match.capturedStart() + 1};
}

/// Take the part of the day off the end of @p text
PartOfDay takePartOfDay(QString& text) {
    PartOfDay part = clockAtEnd(text);
    if (part.part == StoryTime::Part::None) {
        part = namedPartAtEnd(text);
    }
    if (part.part != StoryTime::Part::None) {
        text = text.left(part.at).trimmed();
        if (text.endsWith(QLatin1Char(','))) {
            text.chop(1);
            text = text.trimmed();
        }
    }
    return part;
}

/// Take "about" off the start of @p text: "ok.", "około", "c.", "circa", "about", "~"
bool takeApproximately(QString& text) {
    static const QRegularExpression about(
        QStringLiteral("^(?:okolo|ok\\.|ok(?= )|mniej wiecej|c\\.|ca\\.|circa|about|around|"
                       "approx\\.|approx(?= )|~|≈)\\s*"));
    const QRegularExpressionMatch match = about.match(text);
    if (!match.hasMatch() || match.capturedLength() == text.size()) {
        return false;
    }
    text = text.mid(match.capturedLength());
    return true;
}

/// What a text says of the era of its years
enum class Era {
    Unsaid,      ///< Nothing: a year has at least three digits, so that "12" is not one
    Common,      ///< "n.e.", "AD": the Common Era
    BeforeCommon ///< "p.n.e.", "BC"
};

/// Take a mark of the era off the end of @p text
Era takeEra(QString& text) {
    static const QRegularExpression beforeEra(QStringLiteral(
        "\\s*(?:p\\.?\\s?n\\.?\\s?e\\.?|przed nasza era|b\\.?\\s?c\\.?(?:\\s?e\\.?)?)$"));
    static const QRegularExpression era(
        QStringLiteral("\\s+(?:n\\.\\s?e\\.?|naszej ery|a\\.?\\s?d\\.?|c\\.e\\.?)$"));
    QRegularExpressionMatch match = beforeEra.match(text);
    if (match.hasMatch() && match.capturedStart() > 0) {
        text = text.left(match.capturedStart()).trimmed();
        return Era::BeforeCommon;
    }
    match = era.match(text);
    if (match.hasMatch()) {
        text = text.left(match.capturedStart()).trimmed();
        return Era::Common;
    }
    return Era::Unsaid;
}

/// What a text of a date says: a day, a month, a year, each perhaps missing
struct DateParts {
    int day = 0;    ///< 0: not said
    int month = 0;  ///< 0: not said
    std::optional<int> year;
};

/// The astronomical year a text says: "44" before the Common Era is -43; without a mark of
/// the era at least three digits
std::optional<int> yearOf(const QString& text, Era era) {
    static const QRegularExpression year(QStringLiteral("^\\d{1,4}$"));
    if (!year.match(text).hasMatch() || (era == Era::Unsaid && text.size() < 3)) {
        return std::nullopt;
    }
    return era == Era::BeforeCommon ? 1 - text.toInt() : text.toInt();
}

/// A day with a month in words or Roman numbers: "14 kwietnia", "april 14", "14 iv"
std::optional<DateParts> dayAndMonth(const QString& text) {
    static const QRegularExpression dayFirst(QStringLiteral("^(\\d{1,2})\\.?\\s+([a-z.]+)$"));
    static const QRegularExpression monthFirst(QStringLiteral("^([a-z.]+)\\s+(\\d{1,2})$"));
    QRegularExpressionMatch match = dayFirst.match(text);
    if (match.hasMatch() && monthOf(match.captured(2)) > 0) {
        return DateParts{match.captured(1).toInt(), monthOf(match.captured(2)), std::nullopt};
    }
    match = monthFirst.match(text);
    if (match.hasMatch() && monthOf(match.captured(1)) > 0) {
        return DateParts{match.captured(2).toInt(), monthOf(match.captured(1)), std::nullopt};
    }
    return std::nullopt;
}

/// A date without its year, the start of days in a row: "16", "2 marca", "14.04."
std::optional<DateParts> dateWithoutYear(const QString& text) {
    static const QRegularExpression dayOnly(QStringLiteral("^(\\d{1,2})\\.?$"));
    static const QRegularExpression numbers(QStringLiteral("^(\\d{1,2})\\.(\\d{1,2})\\.?$"));
    QRegularExpressionMatch match = dayOnly.match(text);
    if (match.hasMatch()) {
        return DateParts{match.captured(1).toInt(), 0, std::nullopt};
    }
    match = numbers.match(text);
    if (match.hasMatch()) {
        return DateParts{match.captured(1).toInt(), match.captured(2).toInt(), std::nullopt};
    }
    return dayAndMonth(text);
}

/// A date in numbers: "14.04.1931", "1931-04-14", "04.1931"
std::optional<DateParts> dateInNumbers(const QString& text, Era era) {
    static const QRegularExpression dotted(
        QStringLiteral("^(?:(\\d{1,2})\\.)?(\\d{1,2})\\.(\\d{1,4})$"));
    static const QRegularExpression iso(QStringLiteral("^(\\d{4})-(\\d{2})-(\\d{2})$"));
    QRegularExpressionMatch match = dotted.match(text);
    if (match.hasMatch()) {
        const std::optional<int> year = yearOf(match.captured(3), era);
        if (!year) {
            return std::nullopt;
        }
        return DateParts{match.captured(1).toInt(), match.captured(2).toInt(), year};
    }
    match = iso.match(text);
    if (match.hasMatch() && era != Era::BeforeCommon) {
        return DateParts{match.captured(3).toInt(), match.captured(2).toInt(),
                         match.captured(1).toInt()};
    }
    return std::nullopt;
}

/// A whole date: with its year, perhaps without a day or a month
std::optional<DateParts> wholeDate(const QString& text, Era era) {
    if (auto numbers = dateInNumbers(text, era)) {
        return numbers;
    }
    if (std::optional<int> year = yearOf(text, era)) {
        return DateParts{0, 0, year};
    }
    const qsizetype space = text.lastIndexOf(QLatin1Char(' '));
    if (space <= 0) {
        return std::nullopt;
    }
    const std::optional<int> year = yearOf(text.mid(space + 1), era);
    QString rest = text.left(space).trimmed();
    if (rest.endsWith(QLatin1Char(','))) {  // "april 14, 1931"
        rest.chop(1);
    }
    if (!year) {
        return std::nullopt;
    }
    // "marzec 1931", "w marcu 1931", "in march 1931"
    static const QRegularExpression in(QStringLiteral("^(?:w|in)\\s+"));
    const int month = monthOf(QString(rest).remove(in));
    if (month > 0 && !rest.contains(QRegularExpression(QStringLiteral("\\d")))) {
        return DateParts{0, month, year};
    }
    std::optional<DateParts> parts = dayAndMonth(rest);
    if (parts) {
        parts->year = year;
    }
    return parts;
}

StoryTime::Precision precisionOf(const DateParts& parts) {
    if (parts.day > 0) {
        return StoryTime::Precision::Day;
    }
    return parts.month > 0 ? StoryTime::Precision::Month : StoryTime::Precision::Year;
}

std::optional<qint64> julianDayOf(const DateParts& parts) {
    if (!parts.year) {
        return std::nullopt;
    }
    const CalendarDay day{*parts.year, parts.month > 0 ? parts.month : 1,
                          parts.day > 0 ? parts.day : 1};
    return day.julianDay();
}

/// One date: a day, a month or a year
std::optional<StoryTime> oneDate(const QString& text, Era era) {
    const std::optional<DateParts> parts = wholeDate(text, era);
    if (!parts || (parts->day > 0 && parts->month == 0)) {
        return std::nullopt;
    }
    const std::optional<qint64> julianDay = julianDayOf(*parts);
    if (!julianDay) {
        return std::nullopt;
    }
    StoryTime time = StoryTime::day(*julianDay);
    time.precision = precisionOf(*parts);
    return time;
}

/// Days in a row, split at the dash at @p dash: "16–17 kwietnia 1931", "2 marca – 5 kwietnia
/// 1931", "30 grudnia 1931 – 2 stycznia 1932"; the start takes what it does not say from the end
std::optional<StoryTime> daysSplitAt(const QString& text, qsizetype dash, Era era) {
    const QString startText = text.left(dash).trimmed();
    const QString endText = text.mid(dash + 1).trimmed();
    const std::optional<DateParts> end = wholeDate(endText, era);
    if (startText.isEmpty() || !end || end->day == 0) {
        return std::nullopt;
    }
    std::optional<DateParts> start = wholeDate(startText, era);
    if (!start) {
        start = dateWithoutYear(startText);
    }
    if (!start || start->day == 0) {
        return std::nullopt;
    }
    if (start->month == 0) {
        start->month = end->month;
    }
    if (!start->year) {
        start->year = end->year;
    }
    const std::optional<qint64> first = julianDayOf(*start);
    const std::optional<qint64> last = julianDayOf(*end);
    if (!first || !last || *last < *first) {
        return std::nullopt;
    }
    StoryTime time = StoryTime::day(*first);
    time.last = *last;
    return time;
}

std::optional<StoryTime> daysInRow(const QString& text, Era era) {
    for (qsizetype at = 0; at < text.size(); ++at) {
        const QChar character = text.at(at);
        if (character != QLatin1Char('-') && character != QChar(0x2013)) {
            continue;
        }
        if (std::optional<StoryTime> time = daysSplitAt(text, at, era)) {
            return time;
        }
    }
    return std::nullopt;
}

/// Days of the story: "dzien 3", "dni 5–7", "day -2", "days 3-4"
std::optional<StoryTime> daysOfStory(const QString& text, qint64 dayOne) {
    static const QRegularExpression days(QStringLiteral(
        "^(?:dzien|dni|day|days)\\s+(-?\\d{1,6})(?:\\s*[-–]\\s*(-?\\d{1,6}))?$"));
    const QRegularExpressionMatch match = days.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    const int from = match.captured(1).toInt();
    const int to = match.hasCaptured(2) ? match.captured(2).toInt() : from;
    if (from == 0 || to == 0 || to < from) {
        return std::nullopt;
    }
    StoryTime time = StoryTime::day(StoryTime::dayOfNumber(from, dayOne));
    time.last = StoryTime::dayOfNumber(to, dayOne);
    return time;
}

}  // namespace

std::optional<StoryTime> StoryTime::parse(const QString& text, qint64 dayOne) {
    QString rest = normalized(text);
    if (rest.isEmpty()) {
        return std::nullopt;
    }
    const bool approximate = takeApproximately(rest);
    const PartOfDay part = takePartOfDay(rest);
    std::optional<StoryTime> time = daysOfStory(rest, dayOne);
    if (!time) {
        const Era era = takeEra(rest);
        time = oneDate(rest, era);
        if (!time) {
            time = daysInRow(rest, era);
        }
    }
    // A part of the day goes only with days
    if (!time || (part.part != Part::None && time->precision != Precision::Day)) {
        return std::nullopt;
    }
    time->part = part.part;
    time->minutes = part.minutes;
    time->approximate = approximate;
    return time;
}

}  // namespace kalahari::timeline
