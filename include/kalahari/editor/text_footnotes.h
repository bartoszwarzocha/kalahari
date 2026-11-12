/// @file text_footnotes.h
/// @brief Footnotes and endnotes: a number in the text with the text of its note inside it
///
/// A note is a character of its paragraph (the object replacement character, U+FFFC) with a
/// format of the object type FOOTNOTE_OBJECT that keeps the note's id, its kind and its text.
/// The editor shows the character as the note's number, raised; the number is not kept: the
/// notes of each kind are counted in the order of the text (FootnoteNumbers). The note's own
/// text is shown apart from the text: a footnote at the bottom of its page (under its
/// paragraph in the continuous views), an endnote after the last paragraph.
///
/// The note travels with its character: the document's undo, the clipboard and drag and drop
/// take it with the text around it, and deleting the number deletes the note. Its text is
/// KML of the inside of a paragraph (formatting, links, line breaks); a note of several
/// paragraphs has them as <p> elements. It has no images, annotations or notes of its own.
///
/// KML writes a note as <footnote id="f3a9c1">Its text</footnote> or
/// <endnote id="f3a9c1">Its text</endnote> at the place of its number (docs/kml_format.md).
/// The old <footnote> on a range of the text (with a "number" attribute) is read as the text it
/// held, without a mark.

#pragma once

#include <kalahari/editor/format_run.h>

#include <QSet>
#include <QString>
#include <QTextFormat>

#include <optional>
#include <vector>

class QTextBlock;
class QTextCharFormat;
class QTextDocument;
class QXmlStreamAttributes;
class QXmlStreamReader;

namespace kalahari::editor {

/// @brief The object type of a note's number
inline constexpr int FOOTNOTE_OBJECT = QTextFormat::UserObject + 8;

/// @brief The note's id, unique in its text (QString)
inline constexpr int FOOTNOTE_ID_PROPERTY = QTextFormat::UserProperty + 210;

/// @brief Whether the note is an endnote (bool); without it a footnote
inline constexpr int FOOTNOTE_ENDNOTE_PROPERTY = QTextFormat::UserProperty + 211;

/// @brief The note's text: KML of the inside of a paragraph, or of its <p> elements (QString)
inline constexpr int FOOTNOTE_CONTENT_PROPERTY = QTextFormat::UserProperty + 212;

/// @brief The paragraph style the text of the notes is shown in
inline constexpr const char* FOOTNOTE_STYLE_ID = "footnote";

/// @brief What a note is
enum class NoteKind {
    Footnote,  ///< At the bottom of its page
    Endnote,   ///< After the last paragraph of its text
};

/// @brief A note: its id, its kind and its text
struct Footnote {
    QString id;                         ///< Unique in its text
    NoteKind kind = NoteKind::Footnote;
    QString content;                    ///< KML of its paragraphs (see the file's comment)

    bool operator==(const Footnote&) const = default;
};

/// @brief How the numbers of the notes of a kind look
enum class NoteNumberFormat {
    Decimal,     ///< 1, 2, 3
    LowerAlpha,  ///< a, b, c ... z, aa, bb
    UpperAlpha,  ///< A, B, C
    LowerRoman,  ///< i, ii, iii
    UpperRoman,  ///< I, II, III
    Symbols,     ///< *, †, ‡, §, then the same twice: **, ††
};

/// @brief What the numbers of the notes of a text depend on outside it
struct FootnoteContext {
    NoteNumberFormat footnoteFormat = NoteNumberFormat::Decimal;
    NoteNumberFormat endnoteFormat = NoteNumberFormat::LowerRoman;
    int footnotesBefore = 0;  ///< The numbers the text goes on from (the notes of the texts
    int endnotesBefore = 0;   ///< before it, when the book numbers them through)

    bool operator==(const FootnoteContext&) const = default;
};

/// @brief The text of a number in a format: "3", "c", "C", "iii", "III", "‡"
QString noteNumberText(int number, NoteNumberFormat format);

// =============================================================================
// The notes in the formats
// =============================================================================

/// @brief The note a format is the number of (std::nullopt: none)
std::optional<Footnote> footnoteOf(const QTextFormat& format);

/// @brief Make a format the number of a note
void setFootnote(QTextCharFormat& format, const Footnote& note);

/// @brief Whether a format is the number of a note
bool isFootnoteFormat(const QTextFormat& format);

/// @brief A format without a note in it (the format of text typed next to a number)
void clearFootnote(QTextCharFormat& format);

/// @brief Whether a paragraph holds the number of a note
bool hasNotes(const QTextBlock& block);

/// @brief A new id of a note, none of @p taken: "f" and six hexadecimal digits
QString newFootnoteId(const QSet<QString>& taken);

// =============================================================================
// The notes of a text
// =============================================================================

/// @brief A note at its place
struct NotePlace {
    int position = 0;  ///< Its number's character in the document
    Footnote note;

    bool operator==(const NotePlace&) const = default;
};

/// @brief The notes of a document, in text order
std::vector<NotePlace> notesIn(const QTextDocument& document);

/// @brief The notes of a paragraph, in text order
std::vector<NotePlace> notesIn(const QTextBlock& block);

/// @brief The note with an id (std::nullopt: none)
std::optional<NotePlace> findNote(const QTextDocument& document, const QString& id);

/// @brief The ids of the notes of a document
QSet<QString> noteIds(const QTextDocument& document);

/// @brief The numbers of the notes of a text
class FootnoteNumbers {
public:
    FootnoteNumbers() = default;

    /// @brief Count the notes of a document
    FootnoteNumbers(const QTextDocument& document, const FootnoteContext& context);

    /// @brief The notes, in text order
    const std::vector<NotePlace>& notes() const { return m_notes; }

    /// @brief The number the note at a position shows (empty: no note there)
    QString numberAt(int position) const;

    /// @brief The number of the note @p index of notes()
    const QString& number(size_t index) const { return m_numbers.at(index); }

    /// @brief The number of the note with an id (empty: none)
    QString numberOf(const QString& id) const;

    /// @brief The note at a position (nullptr: none)
    const NotePlace* noteAt(int position) const;

    /// @brief The document's characters from @p from on moved by @p delta (a change of the
    ///        text without a note: the numbers stay, their places move)
    void shift(int from, int delta);

private:
    std::vector<NotePlace> m_notes;
    std::vector<QString> m_numbers;  ///< Of each note
    FootnoteContext m_context;
};

// =============================================================================
// The text of a note
// =============================================================================

/// @brief A paragraph of a note's text
struct NoteParagraph {
    QString text;                    ///< Its characters (a line break: QChar::LineSeparator)
    std::vector<FormatRun> formats;  ///< Its formatting, in text order

    bool operator==(const NoteParagraph& other) const;
};

/// @brief The paragraphs of a note's text (at least one)
std::vector<NoteParagraph> noteParagraphs(const QString& content);

/// @brief A note's text as KML paragraphs: its <p> elements, or its inline text in one
QString noteParagraphsKml(const QString& content);

/// @brief A note's text as plain text: its paragraphs one a line, a line break a space
QString notePlainText(const QString& content);

/// @brief Whether a note's text has no character but spaces
bool isEmptyNoteContent(const QString& content);

/// @brief A document of a note's text (the frame it is written in): its paragraphs with their
///        formatting; the document's own font stays
void setNoteContent(QTextDocument& document, const QString& content);

/// @brief The note's text of a document (the frame): KML of its paragraph, or of its <p>
///        elements; only the formatting a note keeps (bold, italic, underline, strikethrough,
///        raised and lowered text, links)
QString noteContentOf(const QTextDocument& document);

/// @brief The formatting a note keeps of a format: bold, italic, underline, strikethrough,
///        raised and lowered text and a link
QTextCharFormat noteFormatOf(const QTextCharFormat& format);

// =============================================================================
// KML
// =============================================================================

/// @brief The element of a note: <footnote id="...">its text</footnote> or <endnote ...>
QString noteKml(const Footnote& note);

/// @brief Whether a <footnote> is the old one on a range of the text (it has a number): its
///        text stays in the paragraph, without a mark
bool isOldFootnote(const QXmlStreamAttributes& attributes);

/// @brief The note of the element the reader is at (<footnote> or <endnote>): its id (empty
///        without one), its kind and its text, the inside of the element as it is written; the
///        reader is left at the element's end
Footnote readNoteElement(QXmlStreamReader& reader);

}  // namespace kalahari::editor
