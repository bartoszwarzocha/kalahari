/// @file text_fields.h
/// @brief The fields of a text: the book's title, its author, the text's title, the page and the
///        pages, the text's words and the date, each showing its value as it is now
///
/// A field is a character of its paragraph (the object replacement character, U+FFFC) with a
/// format of the object type TEXT_FIELD_OBJECT that keeps what the field shows (FieldType) and
/// how (its format, by name: fieldFormats()). The program gives the value: the book's and the
/// text's from what the book says of them (FieldContext), the words and the pages from the text
/// itself (FieldCounts). A field changes with its value: the book's title in the Properties
/// panel, the date on the next day. A page field shows "#" in the views without pages: a page
/// is where the page view puts the field.
///
/// The fields are as the numbers of the captions are (text_captions.h): one character with a
/// gray background, which the cursor goes over at once, which is not a word and which search
/// and spelling skip. Convert to Text puts the value in its place.
///
/// KML writes a field as <field type="page" format="i"/>, without the format when it is the
/// type's first (docs/kml_format.md).

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTextFormat>

#include <array>
#include <optional>

class QTextBlock;
class QTextCharFormat;
class QXmlStreamAttributes;

namespace kalahari::editor {

/// @brief The object type of the fields
inline constexpr int TEXT_FIELD_OBJECT = QTextFormat::UserObject + 9;

/// @brief What a field shows (int, FieldType)
inline constexpr int FIELD_TYPE_PROPERTY = QTextFormat::UserProperty + 220;

/// @brief How a field shows it: the name of its format (QString, fieldFormats())
inline constexpr int FIELD_FORMAT_PROPERTY = QTextFormat::UserProperty + 221;

/// @brief What a field shows
enum class FieldType {
    BookTitle,  ///< The title of the book
    Author,     ///< The author of the book
    TextTitle,  ///< The title of the text the field is in (of its chapter)
    Page,       ///< The page the field is on, in the page view (from 1 in each text)
    Pages,      ///< The pages of the text, in the page view
    Words,      ///< The words of the text, as the counts of the program take them
    Date,       ///< The date, the time or both, today's
};

/// @brief The types of the fields, in the order of FieldType (the order of the window Field)
inline constexpr std::array<FieldType, 7> FIELD_TYPES = {
    FieldType::BookTitle, FieldType::Author, FieldType::TextTitle, FieldType::Page,
    FieldType::Pages,     FieldType::Words,  FieldType::Date};

/// @brief A field: what it shows and how
struct TextField {
    FieldType type = FieldType::BookTitle;
    QString format;  ///< The name of its format (fieldFormats()); empty: the type's first

    bool operator==(const TextField&) const = default;
};

/// @brief What a field shows of the book and the text it is in
struct FieldContext {
    QString bookTitle;  ///< The book's title; empty: none (outside a book)
    QString author;     ///< The book's author; empty: none
    QString textTitle;  ///< The title of the text (of a file outside the book: its name)
    QString language;   ///< The book's language ("pl", "en-US"); empty: the program's
    QDateTime now;      ///< What the dates show; invalid: the time they are shown

    bool operator==(const FieldContext&) const = default;
};

/// @brief What a field shows of the text itself
struct FieldCounts {
    int words = 0;  ///< The words of the text
    int page = 0;   ///< The page the field is on, from 1; 0: the view has no pages
    int pages = 0;  ///< The pages of the text; 0: the view has no pages
};

// =============================================================================
// The fields in the formats
// =============================================================================

/// @brief The field of a character with this format; std::nullopt: not one
std::optional<TextField> textFieldOf(const QTextFormat& format);

/// @brief Make the format one of a field (its other properties stay); the format is the type's
///        first when the type has no such one
void setTextField(QTextCharFormat& format, const TextField& field);

/// @brief Whether the format is of a field of this file (not a caption's number)
bool isTextFieldFormat(const QTextFormat& format);

/// @brief Take a field of this file off the format: its object type and properties
void clearTextField(QTextCharFormat& format);

/// @brief Whether a block holds a field of this file
bool hasTextFields(const QTextBlock& block);

// =============================================================================
// Names and formats
// =============================================================================

/// @brief The name of a type in KML: "book-title", "author", "title", "page", "pages", "words",
///        "date"
QString fieldTypeName(FieldType type);

/// @brief The type a name stands for; std::nullopt for an unknown one
std::optional<FieldType> fieldTypeOfName(const QString& name);

/// @brief The name of a type the writer reads: "Book Title", "Page Number" (translated)
QString fieldTypeTitle(FieldType type);

/// @brief The formats of a type, by name, the first the type's own:
///        - the titles and the author: "" (as they are written), "upper" (in capitals);
///        - the page and the pages: "1", "i", "I" (in Arabic or Roman numerals);
///        - the words: "" (in groups of digits: "12 345"), "plain" ("12345");
///        - the date: "long" (11 October 2026), "short" (11.10.2026), "full" (with the day of
///          the week), "iso" (2026-10-11), "time" (14:35), "date-time" (11.10.2026, 14:35)
QStringList fieldFormats(FieldType type);

/// @brief The format a field of the type has with this name: it, or the type's first for one
///        the type does not have
QString normalizedFieldFormat(FieldType type, const QString& format);

/// @brief Whether a field changes with the place it is on in the page view (the page)
bool isPageField(FieldType type);

// =============================================================================
// What a field shows
// =============================================================================

/// @brief The pattern of a date format in a language, for QLocale::toString(): "long",
///        "short", "full", "iso", "time", "date-time"
/// @param language The book's language; empty: the program's
QString datePattern(const QString& format, const QString& language);

/// @brief The text of a date in a format and a language: "11 października 2026"
QString dateText(const QDateTime& when, const QString& format, const QString& language);

/// @brief The text a field shows: its value in its format; in a view without pages "#" for the
///        page and the pages; for a value the book does not have (an author not written) the
///        name of the type in brackets: "[Author]"
QString textFieldText(const TextField& field, const FieldContext& context,
                      const FieldCounts& counts);

/// @brief Whether a field shows its type's name in place of a value the book does not have
bool isTextFieldEmpty(const TextField& field, const FieldContext& context);

// =============================================================================
// KML
// =============================================================================

/// @brief The element of a field: <field type="page"/>, with format="..." unless it is the
///        type's first
QString textFieldKml(const TextField& field);

/// @brief The field of the attributes of <field>; std::nullopt for an unknown type
std::optional<TextField> textFieldOfKml(const QXmlStreamAttributes& attributes);

}  // namespace kalahari::editor
