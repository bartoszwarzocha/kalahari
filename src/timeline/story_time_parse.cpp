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

/// Months in a row split at the dash at @p dash: "marzec – maj 1931", "listopad 1931 – luty
/// 1932"; the start takes its year from the end when it says none
std::optional<StoryTime> monthsSplitAt(const QString& text, qsizetype dash, Era era) {
    const QString startText = text.left(dash).trimmed();
    const std::optional<DateParts> end = wholeDate(text.mid(dash + 1).trimmed(), era);
    if (startText.isEmpty() || !end || end->day > 0 || end->month == 0) {
        return std::nullopt;
    }
    std::optional<DateParts> start = wholeDate(startText, era);
    if (!start && monthOf(startText) > 0) {
        start = DateParts{0, monthOf(startText), end->year};
    }
    if (!start || start->day > 0 || start->month == 0 || !start->year) {
        return std::nullopt;
    }
    const int count = (*end->year - *start->year) * MONTHS + (end->month - start->month) + 1;
    if (count < 2) {
        return std::nullopt;
    }
    return StoryTime::monthsInRow(*start->year, start->month, count);
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
        if (std::optional<StoryTime> time = monthsSplitAt(text, at, era)) {
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

/// A season, a half of a year or a year in words, with what follows them: its year
struct PartOfYear {
    int firstMonth = 0;  ///< Of its run; 12 for winter, which starts in the year before
    int count = 0;       ///< Months; 12: the year itself, which needs its year
};

/// What the start of @p text names: "wiosna", "w pierwszej polowie", "rok", "the summer of";
/// @p rest gets what follows
std::optional<PartOfYear> partOfYearAt(const QString& text, QString& rest) {
    constexpr int SEASON = 3;
    constexpr int HALF = 6;
    static const std::array<std::pair<PartOfYear, QRegularExpression>, 7> names{{
        {{3, SEASON},
         QRegularExpression(QStringLiteral(
             "^(?:na |w |in |in the |the )?(?:wiosna|wiosne|wiosny|wiosnie|spring)\\b"))},
        {{6, SEASON},
         QRegularExpression(QStringLiteral(
             "^(?:w |in |in the |the )?(?:lato|latem|lecie|summer)\\b"))},
        {{9, SEASON},
         QRegularExpression(QStringLiteral(
             "^(?:na |w |in |in the |the )?(?:jesien|jesienia|jesieni|autumn|fall)\\b"))},
        {{12, SEASON},
         QRegularExpression(QStringLiteral(
             "^(?:na |w |in |in the |the )?(?:zima|zime|zimy|zimie|winter)\\b"))},
        {{1, HALF},
         QRegularExpression(QStringLiteral(
             "^(?:(?:w )?(?:pierwsza|pierwszej|i|1\\.?) (?:polowa|polowie|polowy)|(?:in )?"
             "(?:the )?first half)\\b"))},
        {{7, HALF},
         QRegularExpression(QStringLiteral(
             "^(?:(?:w )?(?:druga|drugiej|ii|2\\.?) (?:polowa|polowie|polowy)|(?:in )?"
             "(?:the )?second half)\\b"))},
        {{1, MONTHS},
         QRegularExpression(QStringLiteral("^(?:w |in |in the |the )?(?:rok|roku|year)\\b"))},
    }};
    for (const auto& [part, name] : names) {
        const QRegularExpressionMatch match = name.match(text);
        if (match.hasMatch()) {
            rest = text.mid(match.capturedLength()).trimmed();
            static const QRegularExpression of(QStringLiteral("^(?:roku|of|of the year)\\b"));
            rest = rest.remove(of).trimmed();
            return part;
        }
    }
    return std::nullopt;
}

/// The run of @p part in year @p year: winter from December of the year before
std::optional<StoryTime> runOf(const PartOfYear& part, int year) {
    if (part.count == MONTHS) {
        const std::optional<qint64> first = CalendarDay{year, 1, 1}.julianDay();
        if (!first) {
            return std::nullopt;
        }
        StoryTime time = StoryTime::day(*first);
        time.precision = StoryTime::Precision::Year;
        return time;
    }
    return StoryTime::monthsInRow(part.firstMonth == MONTHS ? year - 1 : year, part.firstMonth,
                                  part.count);
}

/// A season, a half of a year or a year: "wiosna 1931", "zima 1931/32", "w pierwszej połowie
/// 1931", "rok 1931", "summer 1931", "the first half of 1931"; a season or a half without its
/// year is the first one that does not end before day one
std::optional<StoryTime> partOfYear(const QString& text, qint64 dayOne) {
    QString rest;
    const std::optional<PartOfYear> part = partOfYearAt(text, rest);
    if (!part) {
        return std::nullopt;
    }
    if (rest.isEmpty()) {
        if (part->count == MONTHS) {
            return std::nullopt;  // "rok" alone says no year
        }
        const int year = CalendarDay::of(dayOne).year;
        for (int candidate = year - 1; candidate <= year + 1; ++candidate) {
            const std::optional<StoryTime> run = runOf(*part, candidate);
            if (run && run->last >= dayOne) {
                return run;
            }
        }
        return std::nullopt;
    }
    // "zima 1931/32", "zima 1931/1932": December 1931 to February 1932
    static const QRegularExpression winter(QStringLiteral("^(\\d{3,4})/(\\d{2}|\\d{4})$"));
    if (const QRegularExpressionMatch match = winter.match(rest);
        match.hasMatch() && part->firstMonth == MONTHS && part->count != MONTHS) {
        const int from = match.captured(1).toInt();
        const int to = match.captured(2).size() == 2 ? from / 100 * 100 + match.captured(2).toInt()
                                                     : match.captured(2).toInt();
        if (to % 100 != (from + 1) % 100) {
            return std::nullopt;
        }
        return runOf(*part, from + 1);
    }
    const Era era = takeEra(rest);
    const std::optional<int> year = yearOf(rest, era);
    if (!year) {
        return std::nullopt;
    }
    return runOf(*part, *year);
}

/// The number of a Polish ordinal of a day, 1-99: "pierwszy", "drugiego", "dwudziesty
/// pierwszy"; 0 for another text
int polishOrdinal(const QString& text) {
    static const QStringList units{"pierwszy", "drugi",  "trzeci", "czwarty",  "piaty",
                                   "szosty",   "siodmy", "osmy",   "dziewiaty"};
    static const QStringList teens{"dziesiaty",     "jedenasty",   "dwunasty",
                                   "trzynasty",     "czternasty",  "pietnasty",
                                   "szesnasty",     "siedemnasty", "osiemnasty",
                                   "dziewietnasty"};
    static const QStringList tens{"dwudziesty",       "trzydziesty",    "czterdziesty",
                                  "piecdziesiaty",    "szescdziesiaty", "siedemdziesiaty",
                                  "osiemdziesiaty",   "dziewiecdziesiaty"};
    constexpr int TEN = 10;
    // "drugiego", "pierwszego": the case after "dnia"
    const auto base = [](QString word) {
        if (word.endsWith(QStringLiteral("ego"))) {
            word.chop(3);
            if (!word.endsWith(QLatin1Char('i'))) {
                word += QLatin1Char('y');
            }
        }
        return word;
    };
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() == 1) {
        const QString word = base(words.first());
        if (const qsizetype unit = units.indexOf(word); unit >= 0) {
            return static_cast<int>(unit) + 1;
        }
        if (const qsizetype teen = teens.indexOf(word); teen >= 0) {
            return static_cast<int>(teen) + TEN;
        }
        if (const qsizetype ten = tens.indexOf(word); ten >= 0) {
            return (static_cast<int>(ten) + 2) * TEN;
        }
        return 0;
    }
    if (words.size() == 2) {
        const qsizetype ten = tens.indexOf(base(words.first()));
        const qsizetype unit = units.indexOf(base(words.last()));
        if (ten >= 0 && unit >= 0) {
            return (static_cast<int>(ten) + 2) * TEN + static_cast<int>(unit) + 1;
        }
    }
    return 0;
}

/// The number of an English number or ordinal, 1-99: "one", "twenty-one", "first",
/// "twenty-first"; 0 for another text
int englishNumber(const QString& text, bool ordinal) {
    static const QStringList units{"one", "two", "three", "four", "five",
                                   "six", "seven", "eight", "nine"};
    static const QStringList unitOrdinals{"first", "second",  "third",  "fourth", "fifth",
                                          "sixth", "seventh", "eighth", "ninth"};
    static const QStringList teens{"ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                                   "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const QStringList teenOrdinals{"tenth",       "eleventh",    "twelfth",
                                          "thirteenth",  "fourteenth",  "fifteenth",
                                          "sixteenth",   "seventeenth", "eighteenth",
                                          "nineteenth"};
    static const QStringList tens{"twenty", "thirty",  "forty",  "fifty",
                                  "sixty",  "seventy", "eighty", "ninety"};
    static const QStringList tenOrdinals{"twentieth", "thirtieth",  "fortieth",  "fiftieth",
                                         "sixtieth",  "seventieth", "eightieth", "ninetieth"};
    constexpr int TEN = 10;
    const QStringList& unitWords = ordinal ? unitOrdinals : units;
    const QStringList& teenWords = ordinal ? teenOrdinals : teens;
    const QStringList words = QString(text).replace(QLatin1Char('-'), QLatin1Char(' '))
                                  .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() == 1) {
        if (const qsizetype unit = unitWords.indexOf(words.first()); unit >= 0) {
            return static_cast<int>(unit) + 1;
        }
        if (const qsizetype teen = teenWords.indexOf(words.first()); teen >= 0) {
            return static_cast<int>(teen) + TEN;
        }
        const qsizetype ten = (ordinal ? tenOrdinals : tens).indexOf(words.first());
        return ten >= 0 ? (static_cast<int>(ten) + 2) * TEN : 0;
    }
    if (words.size() == 2) {
        const qsizetype ten = tens.indexOf(words.first());
        const qsizetype unit = unitWords.indexOf(words.last());
        if (ten >= 0 && unit >= 0) {
            return (static_cast<int>(ten) + 2) * TEN + static_cast<int>(unit) + 1;
        }
    }
    return 0;
}

/// A day of the story in words: "dzień pierwszy", "pierwszy dzień", "drugiego dnia", "dnia
/// trzeciego", "day one", "the first day", "on the second day"
std::optional<StoryTime> dayInWords(const QString& text, qint64 dayOne) {
    static const QRegularExpression polishAfter(QStringLiteral("^(?:w )?(?:dzien|dnia) (.+)$"));
    static const QRegularExpression polishBefore(QStringLiteral("^(?:w )?(.+) (?:dzien|dnia|dniu)$"));
    static const QRegularExpression englishAfter(QStringLiteral("^(?:on )?day (.+)$"));
    static const QRegularExpression englishBefore(QStringLiteral("^(?:on )?(?:the )?(.+) day$"));
    int number = 0;
    if (QRegularExpressionMatch match = polishAfter.match(text); match.hasMatch()) {
        number = polishOrdinal(match.captured(1));
    }
    if (QRegularExpressionMatch match = polishBefore.match(text);
        number == 0 && match.hasMatch()) {
        // "pierwszym dniu": the case after "w"
        QString words = match.captured(1);
        words.replace(QRegularExpression(QStringLiteral("ym\\b")), QStringLiteral("y"));
        words.replace(QRegularExpression(QStringLiteral("im\\b")), QStringLiteral("i"));
        number = polishOrdinal(words);
    }
    if (QRegularExpressionMatch match = englishAfter.match(text);
        number == 0 && match.hasMatch()) {
        number = englishNumber(match.captured(1), false);
    }
    if (QRegularExpressionMatch match = englishBefore.match(text);
        number == 0 && match.hasMatch()) {
        number = englishNumber(match.captured(1), true);
    }
    if (number == 0) {
        return std::nullopt;
    }
    return StoryTime::day(StoryTime::dayOfNumber(number, dayOne));
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
        time = dayInWords(rest, dayOne);
    }
    if (!time) {
        time = partOfYear(rest, dayOne);
    }
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
