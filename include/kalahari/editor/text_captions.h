/// @file text_captions.h
/// @brief Numbered captions, cross-references to them and the list of figures
///
/// A caption is a paragraph of text, as any other, in the style Caption next to its image:
/// "Figure 3. The bridge in 1931". Its label ("Figure") is text the writer can change; its
/// number is a field, a character of the paragraph (the object replacement character, U+FFFC)
/// with a format of the object type CAPTION_NUMBER_OBJECT that keeps the label it counts and
/// an id. The program gives the number: each label has a numbering of its own, through the
/// whole book or anew in each chapter (CaptionNumbering), and a field shows the number of its
/// place among the fields of its label.
///
/// A cross-reference is a field too (CROSS_REFERENCE_OBJECT): it keeps the id of the caption
/// it names and shows its number, its label and number, the whole caption, or whether the
/// caption is above or below it. A cross-reference to a caption the book no longer has shows
/// "?". The list of figures is an object of its own block (TextObject::FigureList): the
/// captions of the book, each with its number, in the order of the book.
///
/// The numbers of a text depend on the book around it: the captions of the texts before it,
/// the chapter it is and the numbering the book has for each label. The editor gets them as a
/// CaptionContext; CaptionNumbers gives every field its text.
///
/// KML writes a number as <caption-number label="Figure" id="c4f1a2"/>, a cross-reference as
/// <cross-reference to="c4f1a2" show="label"/> and the list as <list-of-figures/>
/// (docs/kml_format.md).

#pragma once

#include <kalahari/editor/text_lists.h>

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextFormat>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QTextBlock;
class QTextBlockFormat;
class QTextCharFormat;
class QTextDocument;
class QXmlStreamAttributes;

namespace kalahari::editor {

/// @brief The object types of the fields: a caption's number and a cross-reference
inline constexpr int CAPTION_NUMBER_OBJECT = QTextFormat::UserObject + 5;
inline constexpr int CROSS_REFERENCE_OBJECT = QTextFormat::UserObject + 6;

/// @brief The label a number counts (QString): "Figure", "Map"
inline constexpr int CAPTION_LABEL_PROPERTY = QTextFormat::UserProperty + 200;

/// @brief The id of a number, which cross-references name (QString)
inline constexpr int CAPTION_ID_PROPERTY = QTextFormat::UserProperty + 201;

/// @brief The id of the number a cross-reference names (QString)
inline constexpr int REFERENCE_TARGET_PROPERTY = QTextFormat::UserProperty + 202;

/// @brief What a cross-reference shows (int, ReferenceShows)
inline constexpr int REFERENCE_SHOWS_PROPERTY = QTextFormat::UserProperty + 203;

/// @brief A cross-reference that Ctrl+click does not follow (bool)
inline constexpr int REFERENCE_PLAIN_PROPERTY = QTextFormat::UserProperty + 204;

/// @brief A caption above its image, the image of the paragraph after it (bool, a property
///        of the caption's block format); without it a caption is below its image
inline constexpr int CAPTION_ABOVE_PROPERTY = QTextFormat::UserProperty + 205;

/// @brief The longest label (characters)
inline constexpr int MAX_CAPTION_LABEL = 40;

/// @brief The text a cross-reference shows of its caption
enum class ReferenceShows {
    Number,          ///< "3" (the writer writes "Fig." or "in figure" around it)
    LabelAndNumber,  ///< "Figure 3"
    Caption,         ///< The whole caption: "Figure 3. The bridge in 1931"
    Position,        ///< "above" or "below": where the caption is
};

/// @brief How a book numbers the captions of a label
struct CaptionNumbering {
    ListNumberFormat format = ListNumberFormat::Decimal;  ///< 1, a, A, i or I (not Bullet)
    bool perChapter = false;   ///< Anew in each chapter; false: through the whole book
    bool withChapter = false;  ///< With the number of the chapter first ("2.3"), when anew
                               ///< in each chapter
    QString separator = QStringLiteral(".");  ///< Between the chapter's number and its own

    bool operator==(const CaptionNumbering&) const = default;
};

/// @brief The separators between the chapter's number and a caption's own: ".", "–", ":"
const QStringList& captionSeparators();

/// @brief The numbering clamped to what it allows: a format of a number (not Bullet), a
///        known separator, the chapter's number only anew in each chapter
CaptionNumbering normalizedNumbering(const CaptionNumbering& numbering);

/// @brief The number field of a caption
struct CaptionNumberField {
    QString label;  ///< The label it counts
    QString id;     ///< Its id, unique in the book

    bool operator==(const CaptionNumberField&) const = default;
};

/// @brief The caption of an image as Insert Caption and Image Properties show it: a paragraph
///        in the style Caption next to the image's, "Figure 3. The bridge in 1931"
struct ImageCaption {
    bool shown = false;    ///< The image has one
    bool numbered = true;  ///< With its label and number; false: its text alone
    QString label;         ///< The label it counts (numbered)
    QString text;          ///< Its text after the number, or all of it
    bool above = false;    ///< Above the image; false: below it
    bool operator==(const ImageCaption&) const = default;
};

/// @brief A cross-reference to a caption
struct CrossReference {
    QString target;                               ///< The id of the caption's number
    ReferenceShows shows = ReferenceShows::Number;  ///< What it shows
    bool link = true;  ///< Ctrl+click goes to the caption's image

    bool operator==(const CrossReference&) const = default;
};

// =============================================================================
// The fields in the formats
// =============================================================================

/// @brief The number field of a character with this format; std::nullopt: not one
std::optional<CaptionNumberField> captionNumberOf(const QTextFormat& format);

/// @brief Make the format one of a caption's number (its other properties stay)
void setCaptionNumber(QTextCharFormat& format, const CaptionNumberField& field);

/// @brief The cross-reference of a character with this format; std::nullopt: not one
std::optional<CrossReference> crossReferenceOf(const QTextFormat& format);

/// @brief Make the format one of a cross-reference (its other properties stay)
void setCrossReference(QTextCharFormat& format, const CrossReference& reference);

/// @brief Whether the format is of a field: a number, a cross-reference or a field of the
///        book, the page, the words or the date (text_fields.h)
bool isFieldFormat(const QTextFormat& format);

/// @brief Take the field off the format: its object type and properties (the format of text
///        typed after a field)
void clearField(QTextCharFormat& format);

/// @brief Whether a block holds a field
bool hasFields(const QTextBlock& block);

/// @brief Whether a caption paragraph is above its image
bool isCaptionAbove(const QTextBlockFormat& format);

/// @brief Put a caption paragraph above its image or below it
void setCaptionAbove(QTextBlockFormat& format, bool above);

/// @brief The caption of the block of an image: the paragraph after it in the style Caption
///        (unless it is above the image after it), else the paragraph before it in the style
///        Caption above its image; in a table, in the same cell; invalid: none
QTextBlock captionBlockOf(const QTextBlock& imageBlock);

/// @brief The block of the image a caption's block goes with; invalid: none
QTextBlock imageBlockOfCaption(const QTextBlock& captionBlock);

/// @brief A caption's block in parts: its first number, and where its text starts after the
///        number and the separator after it ("Figure 3. The bridge")
struct CaptionParts {
    int numberAt = -1;          ///< The number's character; -1: a caption without a number
    CaptionNumberField number;  ///< Of the number
    int textAt = 0;             ///< Where its text starts: after the number's separator, or
                                ///< the block's start without a number
};

/// @brief The parts of a caption's block
CaptionParts captionParts(const QTextBlock& block);

/// @brief The ids of the numbers in a block
QStringList captionIdsOf(const QTextBlock& block);

/// @brief A label without control characters and the spaces at its ends, at most
///        MAX_CAPTION_LABEL characters long
QString normalizedLabel(const QString& label);

/// @brief A new id of a number, none of @p taken
QString newCaptionId(const QSet<QString>& taken);

// =============================================================================
// Numbers
// =============================================================================

/// @brief The text of a caption's number: its own in the numbering's format, after the
///        chapter's number and the separator when the numbering has it ("2.3")
/// @param chapter The number of the chapter; 0: none (the number is the caption's own)
QString captionNumberText(int number, const CaptionNumbering& numbering, int chapter);

/// @brief The name of a number format in the settings and the book: "1", "a", "A", "i", "I"
QString numberFormatName(ListNumberFormat format);

/// @brief The number format a name stands for; Decimal for an unknown one
ListNumberFormat numberFormatOfName(const QString& name);

/// @brief The name of what a cross-reference shows in the settings and KML: "number",
///        "label", "caption", "position"
QString referenceShowsName(ReferenceShows shows);

/// @brief What a name stands for; Number for an unknown one
ReferenceShows referenceShowsOfName(const QString& name);

// =============================================================================
// The captions of a text
// =============================================================================

/// @brief A part of a caption's paragraph: text, its number, or a cross-reference in it
struct CaptionPiece {
    enum class Kind { Text, Number, Reference };
    Kind kind = Kind::Text;
    QString text;              ///< Kind::Text: the text; Kind::Number: the number's id
    CrossReference reference;  ///< Kind::Reference

    bool operator==(const CaptionPiece&) const = default;
};

/// @brief A caption of a text: a number field with the paragraph it is in
struct CaptionEntry {
    QString id;                        ///< The id of its number
    QString label;                     ///< The label it counts
    std::vector<CaptionPiece> pieces;  ///< Its paragraph, with its number as a piece
    int position = -1;                 ///< The number's character in its document; -1 in KML

    bool operator==(const CaptionEntry&) const = default;
};

/// @brief The number fields of a document, in text order, each with its paragraph
std::vector<CaptionEntry> captionEntries(const QTextDocument& document);

/// @brief The number fields of a text's KML, in text order, each with its paragraph
std::vector<CaptionEntry> captionEntriesOfKml(const QString& kml);

/// @brief The ids of the numbers of a document
QSet<QString> captionIds(const QTextDocument& document);

/// @brief How many cross-references of a document name each caption (by the id of its number)
QHash<QString, int> crossReferenceCounts(const QTextDocument& document);

/// @brief How many cross-references of a text's KML name each caption
QHash<QString, int> crossReferenceCountsOfKml(const QString& kml);

// =============================================================================
// The book around a text
// =============================================================================

/// @brief A caption of the book with its number
struct BookCaption {
    QString id;
    QString label;
    QString number;                    ///< As the field shows it ("3", "2.3")
    std::vector<CaptionPiece> pieces;  ///< Its paragraph
    QString elementId;                 ///< The text it is in; empty: the editor's own
    int position = -1;                 ///< Its number's character, in the editor's own text

    bool operator==(const BookCaption&) const = default;
};

/// @brief What the numbers of a text depend on outside it
struct CaptionContext {
    int chapter = 0;                             ///< The number of its chapter; 0: none
    QHash<QString, CaptionNumbering> numbering;  ///< Of each label; without one: the default
    std::vector<BookCaption> before;             ///< The captions of the texts before it
    std::vector<BookCaption> after;              ///< The captions of the texts after it
    /// The numbers of each label the text goes on from (the captions of the label before it
    /// in its numbering: in the book, or in its chapter); a label without one: those of
    /// @c before, or none when it is numbered anew in each chapter
    QHash<QString, int> counted;

    /// @brief The numbering of a label
    CaptionNumbering numberingOf(const QString& label) const;

    bool operator==(const CaptionContext&) const = default;
};

/// @brief The text a field shows
struct FieldText {
    QString text;
    bool broken = false;  ///< A cross-reference to a caption the book does not have: "?"

    bool operator==(const FieldText&) const = default;
};

/// @brief A line of the list of figures: a caption with its number
struct FigureListRow {
    QString id;            ///< Its number's id
    QString text;          ///< The whole caption: "Figure 3. The bridge in 1931"
    int numberStart = 0;   ///< Where its number is in the text
    int numberLength = 0;  ///< and how long it is
    QString elementId;     ///< The text it is in; empty: the editor's own

    bool operator==(const FigureListRow&) const = default;
};

/// @brief The numbers of a text in the book around it: the text of each field
class CaptionNumbers {
public:
    CaptionNumbers() = default;

    /// @brief Count the numbers of a document's fields
    CaptionNumbers(const QTextDocument& document, const CaptionContext& context);

    /// @brief The captions of the document, each with its number, in text order
    const std::vector<BookCaption>& own() const { return m_own; }

    /// @brief The captions of the book, in its order: of the texts before, the document's own,
    ///        of the texts after
    std::vector<BookCaption> book() const;

    /// @brief The caption whose number has the id: the document's own first, then the book's;
    ///        nullptr: none
    const BookCaption* find(const QString& id) const;

    /// @brief The text of the field at a position of the document with this format; a field of
    ///        text_fields.h shows its value without the book and the view (the editor's layout
    ///        has them: KalahariTextDocumentLayout::fieldText())
    FieldText fieldText(int position, const QTextFormat& format) const;

    /// @brief The text a cross-reference at a position of the document shows
    FieldText referenceText(const CrossReference& reference, int position) const;

    /// @brief The text of a caption's paragraph, with its numbers and cross-references
    /// @param numberStart Gets where the caption's number starts in it (-1: nowhere); may be
    ///        nullptr
    /// @param numberLength Gets how long it is; may be nullptr
    QString captionText(const BookCaption& caption, int* numberStart = nullptr,
                        int* numberLength = nullptr) const;

    /// @brief The lines of the list of figures: the captions of the book
    std::vector<FigureListRow> figureList() const;

    /// @brief The document's characters from @p from on moved by @p delta (a change of the
    ///        text without a field: the numbers stay, their places move)
    void shift(int from, int delta);

private:
    std::vector<BookCaption> m_own;
    QHash<int, int> m_ownAt;         ///< The index in m_own of the number at a position
    QHash<QString, int> m_ownById;   ///< The first of m_own with an id
    std::vector<BookCaption> m_before;
    std::vector<BookCaption> m_after;
    QHash<QString, int> m_beforeById;
    QHash<QString, int> m_afterById;
};

/// @brief The texts of the fields and of the lists of figures of [from, to) of a document, in
///        text order: what a field shows, the rows of a list one a line (what the clipboard
///        writes for them)
std::vector<QString> fieldTexts(const QTextDocument& document, const CaptionNumbers& numbers,
                                int from, int to);

/// @brief The text of the field at a position of a document with a format
using FieldTextOf = std::function<FieldText(int position, const QTextFormat& format)>;

/// @brief The texts of the fields and of the lists of figures of [from, to) of a document, the
///        fields' as @p textOf gives them (the editor's layout: with the book and the view)
std::vector<QString> fieldTexts(const QTextDocument& document, const CaptionNumbers& numbers,
                                const FieldTextOf& textOf, int from, int to);

// =============================================================================
// KML
// =============================================================================

/// @brief The element of a number: <caption-number label="Figure" id="c4f1a2"/>
QString captionNumberKml(const CaptionNumberField& field);

/// @brief The element of a cross-reference: <cross-reference to="c4f1a2"/>, with show="..."
///        unless it shows the number and link="false" when it is not a link
QString crossReferenceKml(const CrossReference& reference);

/// @brief The number of the attributes of <caption-number>; std::nullopt without a label (an
///        id it lacks is empty)
std::optional<CaptionNumberField> captionNumberOfKml(const QXmlStreamAttributes& attributes);

/// @brief The cross-reference of the attributes of <cross-reference>; std::nullopt without a
///        target
std::optional<CrossReference> crossReferenceOfKml(const QXmlStreamAttributes& attributes);

}  // namespace kalahari::editor
