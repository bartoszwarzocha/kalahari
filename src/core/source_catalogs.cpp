// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_catalogs.cpp
/// @brief The catalogs on the web that give the description of a source by its ISBN or DOI,
/// and what their answers say

#include "kalahari/core/source_catalogs.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>
#include <string>

namespace kalahari::core {

namespace {

/// @brief Digits of an ISBN of the old form and of the new one
constexpr qsizetype SHORT_ISBN = 10;
constexpr qsizetype LONG_ISBN = 13;

/// @brief What the sums of the digits of a good ISBN divide by
constexpr int SHORT_ISBN_MODULUS = 11;
constexpr int LONG_ISBN_MODULUS = 10;

/// @brief The weight of every other digit of a 13-digit ISBN, and the value of an X
constexpr int LONG_ISBN_WEIGHT = 3;
constexpr int X_VALUE = 10;

/// @brief The character some answers in UTF-8 begin with
constexpr char16_t BYTE_ORDER_MARK = 0xFEFF;

/// @brief Whether the last digit of 10-digit ISBN @p digits checks
bool shortIsbnChecks(const QString& digits) {
    int sum = 0;
    for (qsizetype i = 0; i < SHORT_ISBN; ++i) {
        const QChar digit = digits.at(i);
        const int value = digit == QLatin1Char('X') ? X_VALUE : digit.digitValue();
        sum += static_cast<int>(SHORT_ISBN - i) * value;
    }
    return sum % SHORT_ISBN_MODULUS == 0;
}

/// @brief Whether the last digit of 13-digit ISBN @p digits checks
bool longIsbnChecks(const QString& digits) {
    int sum = 0;
    for (qsizetype i = 0; i < LONG_ISBN; ++i) {
        sum += digits.at(i).digitValue() * (i % 2 == 0 ? 1 : LONG_ISBN_WEIGHT);
    }
    return sum % LONG_ISBN_MODULUS == 0;
}

/// @brief The first year of four digits in @p date: "October 1, 1988" gives "1988"
QString yearOf(const QString& date) {
    static const QRegularExpression year(QStringLiteral("\\b(\\d{4})\\b"));
    const QRegularExpressionMatch match = year.match(date);
    return match.hasMatch() ? match.captured(1) : QString();
}

/// @brief The people of @p names, all of them authors
QList<SourcePerson> authorsOf(const QStringList& names) {
    QList<SourcePerson> people;
    for (const QString& name : names) {
        if (!name.trimmed().isEmpty()) {
            people << SourcePerson{.name = name.simplified(), .role = {}, .extra = {}};
        }
    }
    return people;
}

/// @brief The texts of array @p array; with @p key, those of its objects under @p key
QStringList textsOf(const QJsonArray& array, const char* key = nullptr) {
    QStringList texts;
    for (const auto& each : array) {
        const QString text = key == nullptr
                                 ? each.toString()
                                 : each.toObject().value(QLatin1String(key)).toString();
        if (!text.trimmed().isEmpty()) {
            texts << text.trimmed();
        }
    }
    return texts;
}

/// @brief The ISBN Google Books gives in @p identifiers, the 13 digits rather than the 10;
/// empty for none
QString googleIsbn(const QJsonArray& identifiers) {
    QString shortOne;
    for (const auto& each : identifiers) {
        const QJsonObject identifier = each.toObject();
        const QString type = identifier.value(QLatin1String("type")).toString();
        const QString number = identifier.value(QLatin1String("identifier")).toString();
        if (type == QLatin1String("ISBN_13")) {
            return number;
        }
        if (type == QLatin1String("ISBN_10")) {
            shortOne = number;
        }
    }
    return shortOne;
}

/// @brief The ISBN Open Library gives in @p identifiers, the 13 digits rather than the 10;
/// empty for none
QString openLibraryIsbn(const QJsonObject& identifiers) {
    for (const char* key : {"isbn_13", "isbn_10"}) {
        const QStringList numbers = textsOf(identifiers.value(QLatin1String(key)).toArray());
        if (!numbers.isEmpty()) {
            return numbers.first();
        }
    }
    return {};
}

/// @brief Field @p field of @p source, set to @p text
void put(SourceDescription& source, const char* field, const QString& text) {
    source.setValue(QString::fromLatin1(field), text);
}

/// @brief The pages of a book as a number: none for 0 or less
QString pagesOf(const QJsonValue& value) {
    const int pages = value.toInt(0);
    return pages > 0 ? QString::number(pages) : QString();
}

/// @brief Whether address @p url is only that of DOI @p doi
bool isAddressOfDoi(const QString& url, const QString& doi) {
    const QUrl address(url);
    const QString host = address.host().toLower();
    if (host != QLatin1String("doi.org") && host != QLatin1String("dx.doi.org")) {
        return false;
    }
    return SourceCatalogs::doi(url).compare(doi, Qt::CaseInsensitive) == 0;
}

}  // namespace

bool SourceCatalogs::lookupAllowed() {
    return SettingsManager::getInstance().get<bool>(LOOKUP_SETTING, false);
}

const QStringList& SourceCatalogs::isbnCatalogs() {
    static const QStringList all = {QString::fromLatin1(GOOGLE_BOOKS),
                                    QString::fromLatin1(OPEN_LIBRARY)};
    return all;
}

QString SourceCatalogs::settingsIsbnCatalog() {
    const QString catalog = QString::fromStdString(
        SettingsManager::getInstance().get<std::string>(ISBN_CATALOG_SETTING, GOOGLE_BOOKS));
    return isbnCatalogs().contains(catalog) ? catalog : QString::fromLatin1(GOOGLE_BOOKS);
}

QString SourceCatalogs::catalogName(const QString& catalog) {
    if (catalog == QLatin1String(OPEN_LIBRARY)) {
        return QStringLiteral("Open Library");
    }
    if (catalog == QLatin1String(DOI_ORG)) {
        return QStringLiteral("doi.org");
    }
    return QStringLiteral("Google Books");
}

QString SourceCatalogs::isbn(const QString& text) {
    static const QRegularExpression prefix(QStringLiteral("^ISBN(?:-1[03])?:?"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression separators(QStringLiteral("[\\s\\x{2010}-\\x{2015}-]"));
    static const QRegularExpression form(QStringLiteral("^(?:\\d{9}[\\dX]|\\d{13})$"));
    QString digits = text.trimmed();
    digits.remove(prefix);
    digits.remove(separators);
    digits = digits.toUpper();
    if (!form.match(digits).hasMatch()) {
        return {};
    }
    const bool checks =
        digits.size() == SHORT_ISBN ? shortIsbnChecks(digits) : longIsbnChecks(digits);
    return checks ? digits : QString();
}

QString SourceCatalogs::doi(const QString& text) {
    static const QRegularExpression prefix(
        QStringLiteral("^(?:https?://(?:dx\\.)?doi\\.org/|doi:\\s*|doi\\s+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression form(QStringLiteral("^10\\.\\d{4,9}/\\S+$"));
    QString number = text.trimmed();
    if (number.contains(QLatin1String("doi.org/"), Qt::CaseInsensitive)) {
        number = QUrl::fromPercentEncoding(number.toUtf8());
    }
    number.remove(prefix);
    return form.match(number).hasMatch() ? number : QString();
}

QString SourceCatalogs::addressPattern(const QString& catalog) {
    if (catalog == QLatin1String(DOI_ORG)) {
        return QStringLiteral("https://doi.org/%1");
    }
    if (catalog == QLatin1String(OPEN_LIBRARY)) {
        return QStringLiteral(
            "https://openlibrary.org/api/books?bibkeys=ISBN:%1&format=json&jscmd=data");
    }
    return QStringLiteral("https://www.googleapis.com/books/v1/volumes?q=isbn:%1");
}

std::optional<SourceDescription> SourceCatalogs::fromGoogleBooks(const QByteArray& json,
                                                                 const QString& isbn) {
    const QJsonArray items = QJsonDocument::fromJson(json).object().value(QLatin1String("items"))
                                 .toArray();
    if (items.isEmpty()) {
        return std::nullopt;
    }
    const QJsonObject info = items.first().toObject().value(QLatin1String("volumeInfo"))
                                 .toObject();
    SourceDescription source;
    source.type = QString::fromLatin1(SourceDescription::BOOK);
    source.people = authorsOf(textsOf(info.value(QLatin1String("authors")).toArray()));
    put(source, SourceDescription::TITLE, info.value(QLatin1String("title")).toString());
    put(source, SourceDescription::SUBTITLE, info.value(QLatin1String("subtitle")).toString());
    put(source, SourceDescription::PUBLISHER, info.value(QLatin1String("publisher")).toString());
    put(source, SourceDescription::YEAR,
        yearOf(info.value(QLatin1String("publishedDate")).toString()));
    put(source, SourceDescription::PAGES, pagesOf(info.value(QLatin1String("pageCount"))));
    const QString found = googleIsbn(info.value(QLatin1String("industryIdentifiers")).toArray());
    put(source, SourceDescription::ISBN, found.isEmpty() ? isbn : found);
    if (source.value(SourceDescription::TITLE).isEmpty()) {
        return std::nullopt;
    }
    return source;
}

std::optional<SourceDescription> SourceCatalogs::fromOpenLibrary(const QByteArray& json,
                                                                 const QString& isbn) {
    const QJsonObject answer = QJsonDocument::fromJson(json).object();
    QJsonObject book = answer.value(QStringLiteral("ISBN:") + isbn).toObject();
    if (book.isEmpty() && answer.size() == 1) {
        book = answer.constBegin().value().toObject();
    }
    SourceDescription source;
    source.type = QString::fromLatin1(SourceDescription::BOOK);
    source.people = authorsOf(textsOf(book.value(QLatin1String("authors")).toArray(), "name"));
    put(source, SourceDescription::TITLE, book.value(QLatin1String("title")).toString());
    put(source, SourceDescription::SUBTITLE, book.value(QLatin1String("subtitle")).toString());
    put(source, SourceDescription::PUBLISHER,
        textsOf(book.value(QLatin1String("publishers")).toArray(), "name")
            .join(QStringLiteral(", ")));
    put(source, SourceDescription::PLACE,
        textsOf(book.value(QLatin1String("publish_places")).toArray(), "name")
            .join(QStringLiteral(", ")));
    put(source, SourceDescription::YEAR,
        yearOf(book.value(QLatin1String("publish_date")).toString()));
    put(source, SourceDescription::PAGES, pagesOf(book.value(QLatin1String("number_of_pages"))));
    const QString found = openLibraryIsbn(book.value(QLatin1String("identifiers")).toObject());
    put(source, SourceDescription::ISBN, found.isEmpty() ? isbn : found);
    if (source.value(SourceDescription::TITLE).isEmpty()) {
        return std::nullopt;
    }
    return source;
}

std::optional<SourceDescription> SourceCatalogs::fromDoiRecord(const QByteArray& ris,
                                                               const QString& doi) {
    // A RIS record only: a page of HTML of a DOI without one is none
    QString text = QString::fromUtf8(ris).trimmed();
    if (text.startsWith(QChar(BYTE_ORDER_MARK))) {
        text = text.mid(1).trimmed();
    }
    if (!text.startsWith(QLatin1String("TY  -"))) {
        return std::nullopt;
    }
    std::optional<SourceDescription> source = Bibliography::parse(text);
    if (!source) {
        return std::nullopt;
    }
    if (source->value(SourceDescription::DOI).isEmpty()) {
        put(*source, SourceDescription::DOI, doi);
    }
    const QString url = source->value(SourceDescription::URL);
    if (!url.isEmpty() && isAddressOfDoi(url, source->value(SourceDescription::DOI))) {
        put(*source, SourceDescription::URL, QString());
    }
    return source;
}

SourceDescription SourceCatalogs::merged(const SourceDescription& shown,
                                         const SourceDescription& found) {
    SourceDescription result = shown;
    if (!found.type.isEmpty()) {
        result.type = found.type;
    }
    if (!found.people.isEmpty()) {
        result.people = found.people;
    }
    for (auto field = found.values.cbegin(); field != found.values.cend(); ++field) {
        result.setValue(field.key(), field.value());
    }
    return result;
}

}  // namespace kalahari::core
