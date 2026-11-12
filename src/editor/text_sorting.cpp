/// @file text_sorting.cpp
/// @brief Sorting the rows of a table and paragraphs: text, numbers and dates

#include "kalahari/editor/text_sorting.h"

#include <QCollator>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>
#include <numeric>
#include <variant>

namespace kalahari::editor {

namespace {

/// The month (1-12) a word names in the language or in English; 0: none
int monthNamed(const QString& word, const QLocale& locale) {
    const QString name = word.toLower();
    for (const QLocale& language : {locale, QLocale(QLocale::English)}) {
        for (int month = 1; month <= 12; ++month) {
            for (const QString& form :
                 {language.monthName(month, QLocale::LongFormat),
                  language.standaloneMonthName(month, QLocale::LongFormat),
                  language.monthName(month, QLocale::ShortFormat),
                  language.standaloneMonthName(month, QLocale::ShortFormat)}) {
                QString own = form.toLower();
                if (own.endsWith(QLatin1Char('.'))) {
                    own.chop(1);
                }
                if (!own.isEmpty() && own == name) {
                    return month;
                }
            }
        }
    }
    return 0;
}

/// A year of two digits in this century, as the short formats write it
int fullYear(int year, qsizetype digits) {
    return digits <= 2 ? 2000 + year : year;
}

/// The value of a key of a row: a text, a number or a date; nothing
using SortValue = std::variant<std::monostate, QString, double, QDate>;

}  // anonymous namespace

std::optional<double> firstNumberIn(const QString& text)
{
    static const QRegularExpression number(
        QStringLiteral("([-\\x{2212}]?)(\\d+)(?:[.,](\\d+))?"));
    const QRegularExpressionMatch match = number.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    QString value = match.captured(2);
    if (!match.captured(3).isEmpty()) {
        value += QLatin1Char('.') + match.captured(3);
    }
    bool ok = false;
    const double result = value.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return match.captured(1).isEmpty() ? result : -result;
}

std::optional<QDate> firstDateIn(const QString& text, const QLocale& locale)
{
    struct Found {
        qsizetype at = -1;
        QDate date;
    };
    Found best;
    const auto consider = [&best](qsizetype at, const QDate& date) {
        if (date.isValid() && (best.at < 0 || at < best.at)) {
            best = {at, date};
        }
    };

    // 2026-11-03
    static const QRegularExpression iso(QStringLiteral("\\b(\\d{4})-(\\d{1,2})-(\\d{1,2})\\b"));
    if (const QRegularExpressionMatch match = iso.match(text); match.hasMatch()) {
        consider(match.capturedStart(), QDate(match.captured(1).toInt(), match.captured(2).toInt(),
                                              match.captured(3).toInt()));
    }
    // 3.11.2026, 3/11/26: the day first, but in a language that writes the month first
    static const QRegularExpression numeric(
        QStringLiteral("\\b(\\d{1,2})[./](\\d{1,2})[./](\\d{4}|\\d{2})\\b"));
    const bool monthFirst =
        locale.dateFormat(QLocale::ShortFormat).trimmed().startsWith(QLatin1Char('M'));
    if (const QRegularExpressionMatch match = numeric.match(text); match.hasMatch()) {
        const int first = match.captured(1).toInt();
        const int second = match.captured(2).toInt();
        const int year = fullYear(match.captured(3).toInt(), match.captured(3).size());
        consider(match.capturedStart(),
                 monthFirst ? QDate(year, first, second) : QDate(year, second, first));
    }
    // 3 listopada 2026, 3 Nov 2026
    static const QRegularExpression dayMonth(
        QStringLiteral("\\b(\\d{1,2})\\.?\\s+(\\p{L}+)\\.?,?\\s+(\\d{4})\\b"));
    for (auto it = dayMonth.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        if (const int month = monthNamed(match.captured(2), locale); month > 0) {
            consider(match.capturedStart(),
                     QDate(match.captured(3).toInt(), month, match.captured(1).toInt()));
            break;
        }
    }
    // November 3, 2026
    static const QRegularExpression monthDay(
        QStringLiteral("\\b(\\p{L}+)\\.?\\s+(\\d{1,2}),?\\s+(\\d{4})\\b"));
    for (auto it = monthDay.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        if (const int month = monthNamed(match.captured(1), locale); month > 0) {
            consider(match.capturedStart(),
                     QDate(match.captured(3).toInt(), month, match.captured(2).toInt()));
            break;
        }
    }
    if (best.at < 0) {
        return std::nullopt;
    }
    return best.date;
}

std::vector<int> sortedOrder(const std::vector<QStringList>& rows, const SortOptions& options)
{
    std::vector<int> order(rows.size());
    std::iota(order.begin(), order.end(), 0);
    if (options.keys.empty()) {
        return order;
    }
    const QLocale locale = options.language.isEmpty()
                               ? QLocale()
                               : QLocale(QString(options.language).replace(QLatin1Char('-'), QLatin1Char('_')));
    QCollator collator(locale);
    collator.setCaseSensitivity(options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    collator.setIgnorePunctuation(false);

    // The values of each key of each row, read once
    std::vector<std::vector<SortValue>> values(rows.size());
    for (size_t row = 0; row < rows.size(); ++row) {
        for (const SortKey& key : options.keys) {
            const QString text = key.column >= 0 && key.column < rows[row].size()
                                     ? rows[row][key.column].trimmed()
                                     : QString();
            SortValue value;
            switch (key.type) {
            case SortType::Text:
                value = text;
                break;
            case SortType::Numbers:
                if (const std::optional<double> number = firstNumberIn(text)) {
                    value = *number;
                }
                break;
            case SortType::Dates:
                if (const std::optional<QDate> date = firstDateIn(text, locale)) {
                    value = *date;
                }
                break;
            }
            values[row].push_back(std::move(value));
        }
    }

    const auto compare = [&](int a, int b) {
        for (size_t k = 0; k < options.keys.size(); ++k) {
            const SortValue& left = values[static_cast<size_t>(a)][k];
            const SortValue& right = values[static_cast<size_t>(b)][k];
            const bool leftNone = std::holds_alternative<std::monostate>(left);
            const bool rightNone = std::holds_alternative<std::monostate>(right);
            if (leftNone || rightNone) {
                if (leftNone != rightNone) {
                    return rightNone;  // a row without a value after the others
                }
                continue;
            }
            int result = 0;
            if (const auto* text = std::get_if<QString>(&left)) {
                result = collator.compare(*text, std::get<QString>(right));
            } else if (const auto* number = std::get_if<double>(&left)) {
                const double other = std::get<double>(right);
                result = *number < other ? -1 : (*number > other ? 1 : 0);
            } else {
                const QDate& date = std::get<QDate>(left);
                const QDate& other = std::get<QDate>(right);
                result = date < other ? -1 : (date > other ? 1 : 0);
            }
            if (result != 0) {
                return options.keys[k].descending ? result > 0 : result < 0;
            }
        }
        return false;
    };
    std::stable_sort(order.begin(), order.end(), compare);
    return order;
}

}  // namespace kalahari::editor
