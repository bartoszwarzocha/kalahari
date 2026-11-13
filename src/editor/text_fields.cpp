/// @file text_fields.cpp
/// @brief The fields of a text: the book's title, its author, the text's title, the page and the
///        pages, the text's words and the date

#include "kalahari/editor/text_fields.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/text_lists.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QXmlStreamAttributes>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// The names of the types in KML, in the order of FieldType
constexpr std::array<const char*, 7> TYPE_NAMES = {"book-title", "author", "title", "page",
                                                   "pages",      "words",  "date"};

/// What a page field shows in a view without pages
constexpr char NO_PAGE_TEXT[] = "#";

/// The locale of a language ("pl", "en-US"); the program's without one
QLocale localeOf(const QString& language)
{
    if (language.trimmed().isEmpty()) {
        return QLocale();
    }
    return QLocale(QString(language.trimmed()).replace(QLatin1Char('-'), QLatin1Char('_')));
}

/// A date pattern with the year in four digits ("d.MM.yy" -> "d.MM.yyyy")
QString withFullYear(const QString& pattern)
{
    static const QRegularExpression shortYear(QStringLiteral("(?<!y)yy(?!y)"));
    return QString(pattern).replace(shortYear, QStringLiteral("yyyy"));
}

/// A date pattern without the day of the week ("dddd, d MMMM yyyy" -> "d MMMM yyyy")
QString withoutWeekday(const QString& pattern)
{
    static const QRegularExpression weekday(QStringLiteral("dddd[,.]?\\s*|\\s*[,.]?\\s*dddd"));
    return QString(pattern).replace(weekday, QString()).trimmed();
}

/// The number of a page in a format of the page fields: "1", "i", "I"
QString pageNumberText(int number, const QString& format)
{
    if (format == QLatin1String("i")) {
        return listNumberText(number, ListNumberFormat::LowerRoman);
    }
    if (format == QLatin1String("I")) {
        return listNumberText(number, ListNumberFormat::UpperRoman);
    }
    return QString::number(number);
}

}  // namespace

// =============================================================================
// The fields in the formats
// =============================================================================

std::optional<TextField> textFieldOf(const QTextFormat& format)
{
    if (format.objectType() != TEXT_FIELD_OBJECT) {
        return std::nullopt;
    }
    TextField field;
    const int type = format.intProperty(FIELD_TYPE_PROPERTY);
    if (type >= 0 && type < static_cast<int>(FIELD_TYPES.size())) {
        field.type = static_cast<FieldType>(type);
    }
    field.format = normalizedFieldFormat(field.type, format.stringProperty(FIELD_FORMAT_PROPERTY));
    return field;
}

void setTextField(QTextCharFormat& format, const TextField& field)
{
    clearTextField(format);
    format.setObjectType(TEXT_FIELD_OBJECT);
    format.setProperty(FIELD_TYPE_PROPERTY, static_cast<int>(field.type));
    // The format always by its full name: formats of the same field compare equal
    format.setProperty(FIELD_FORMAT_PROPERTY, normalizedFieldFormat(field.type, field.format));
}

bool isTextFieldFormat(const QTextFormat& format)
{
    return format.objectType() == TEXT_FIELD_OBJECT;
}

void clearTextField(QTextCharFormat& format)
{
    if (isTextFieldFormat(format)) {
        format.clearProperty(QTextFormat::ObjectType);
    }
    format.clearProperty(FIELD_TYPE_PROPERTY);
    format.clearProperty(FIELD_FORMAT_PROPERTY);
}

bool hasTextFields(const QTextBlock& block)
{
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        if (it.fragment().isValid() && isTextFieldFormat(it.fragment().charFormat())) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Names and formats
// =============================================================================

QString fieldTypeName(FieldType type)
{
    return QString::fromLatin1(TYPE_NAMES.at(static_cast<size_t>(type)));
}

std::optional<FieldType> fieldTypeOfName(const QString& name)
{
    const QString key = name.trimmed().toLower();
    for (size_t i = 0; i < TYPE_NAMES.size(); ++i) {
        if (key == QLatin1String(TYPE_NAMES.at(i))) {
            return FIELD_TYPES.at(i);
        }
    }
    return std::nullopt;
}

QString fieldTypeTitle(FieldType type)
{
    switch (type) {
        case FieldType::BookTitle:
            //: A field: the title of the book. In Polish: Tytuł książki
            return QCoreApplication::translate("TextFields", "Book Title");
        case FieldType::Author:
            //: A field: the author of the book. In Polish: Autor
            return QCoreApplication::translate("TextFields", "Author");
        case FieldType::TextTitle:
            //: A field: the title of the text (the chapter) it is in. In Polish: Tytuł tekstu
            return QCoreApplication::translate("TextFields", "Text Title");
        case FieldType::Page:
            //: A field: the page it is on. In Polish: Numer strony
            return QCoreApplication::translate("TextFields", "Page Number");
        case FieldType::Pages:
            //: A field: the pages of the text. In Polish: Liczba stron
            return QCoreApplication::translate("TextFields", "Page Count");
        case FieldType::Words:
            //: A field: the words of the text. In Polish: Liczba słów
            return QCoreApplication::translate("TextFields", "Word Count");
        case FieldType::Date:
            //: A field: today's date or time. In Polish: Data
            return QCoreApplication::translate("TextFields", "Date");
    }
    return QString();
}

QStringList fieldFormats(FieldType type)
{
    switch (type) {
        case FieldType::BookTitle:
        case FieldType::Author:
        case FieldType::TextTitle:
            return {QString(), QStringLiteral("upper")};
        case FieldType::Page:
        case FieldType::Pages:
            return {QStringLiteral("1"), QStringLiteral("i"), QStringLiteral("I")};
        case FieldType::Words:
            return {QString(), QStringLiteral("plain")};
        case FieldType::Date:
            return {QStringLiteral("long"), QStringLiteral("short"),     QStringLiteral("full"),
                    QStringLiteral("iso"),  QStringLiteral("time"), QStringLiteral("date-time")};
    }
    return {QString()};
}

QString normalizedFieldFormat(FieldType type, const QString& format)
{
    const QStringList formats = fieldFormats(type);
    const QString trimmed = format.trimmed();
    // The Roman numerals are told apart by their case, the other names are not
    for (const QString& known : formats) {
        if (known == trimmed) {
            return known;
        }
    }
    for (const QString& known : formats) {
        if (known.size() > 1 && known.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return known;
        }
    }
    return formats.value(0);
}

bool isPageField(FieldType type)
{
    return type == FieldType::Page || type == FieldType::Pages;
}

// =============================================================================
// What a field shows
// =============================================================================

QString datePattern(const QString& format, const QString& language)
{
    const QLocale locale = localeOf(language);
    QString time = locale.timeFormat(QLocale::ShortFormat);
    QString shortDate = withFullYear(locale.dateFormat(QLocale::ShortFormat));
    if (format == QLatin1String("short")) {
        return shortDate;
    }
    if (format == QLatin1String("full")) {
        return locale.dateFormat(QLocale::LongFormat);
    }
    if (format == QLatin1String("iso")) {
        return QStringLiteral("yyyy-MM-dd");
    }
    if (format == QLatin1String("time")) {
        return time;
    }
    if (format == QLatin1String("date-time")) {
        // As in "11.10.2026, 14:35"
        return shortDate + QStringLiteral(", ") + time;
    }
    return withoutWeekday(locale.dateFormat(QLocale::LongFormat));  // "long"
}

QString dateText(const QDateTime& when, const QString& format, const QString& language)
{
    return localeOf(language).toString(when, datePattern(format, language));
}

bool isTextFieldEmpty(const TextField& field, const FieldContext& context)
{
    switch (field.type) {
        case FieldType::BookTitle:
            return context.bookTitle.trimmed().isEmpty();
        case FieldType::Author:
            return context.author.trimmed().isEmpty();
        case FieldType::TextTitle:
            return context.textTitle.trimmed().isEmpty();
        case FieldType::Page:
        case FieldType::Pages:
        case FieldType::Words:
        case FieldType::Date:
            break;
    }
    return false;
}

QString textFieldText(const TextField& field, const FieldContext& context,
                      const FieldCounts& counts)
{
    const QString format = normalizedFieldFormat(field.type, field.format);
    const QLocale locale = localeOf(context.language);
    switch (field.type) {
        case FieldType::BookTitle:
        case FieldType::Author:
        case FieldType::TextTitle: {
            if (isTextFieldEmpty(field, context)) {
                return QStringLiteral("[%1]").arg(fieldTypeTitle(field.type));
            }
            const QString& value = field.type == FieldType::BookTitle ? context.bookTitle
                                   : field.type == FieldType::Author  ? context.author
                                                                      : context.textTitle;
            // One line: the line breaks of a title are spaces
            QString text = value.simplified();
            return format == QLatin1String("upper") ? locale.toUpper(text) : text;
        }
        case FieldType::Page:
        case FieldType::Pages: {
            const int number = field.type == FieldType::Page ? counts.page : counts.pages;
            if (number <= 0) {
                return QString::fromLatin1(NO_PAGE_TEXT);
            }
            return pageNumberText(number, format);
        }
        case FieldType::Words: {
            const int words = std::max(0, counts.words);
            return format == QLatin1String("plain") ? QString::number(words)
                                                    : locale.toString(words);
        }
        case FieldType::Date:
            return dateText(context.now.isValid() ? context.now : QDateTime::currentDateTime(),
                            format, context.language);
    }
    return QString();
}

// =============================================================================
// KML
// =============================================================================

QString textFieldKml(const TextField& field)
{
    QString kml = QStringLiteral("<field type=\"") + fieldTypeName(field.type) + QLatin1Char('"');
    const QString format = normalizedFieldFormat(field.type, field.format);
    if (format != fieldFormats(field.type).value(0)) {
        kml += QStringLiteral(" format=\"") + KmlFormatRegistry::escapeXml(format) +
               QLatin1Char('"');
    }
    return kml + QStringLiteral("/>");
}

std::optional<TextField> textFieldOfKml(const QXmlStreamAttributes& attributes)
{
    const std::optional<FieldType> type =
        fieldTypeOfName(attributes.value(QStringLiteral("type")).toString());
    if (!type) {
        return std::nullopt;
    }
    TextField field;
    field.type = *type;
    field.format =
        normalizedFieldFormat(*type, attributes.value(QStringLiteral("format")).toString());
    return field;
}

}  // namespace kalahari::editor
