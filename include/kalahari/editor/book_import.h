// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import.h
/// @brief Import of a Word document (DOCX), Markdown and plain text into a book
///
/// A reader reads the file into an ImportDocument: its paragraphs with their styles, headings,
/// lists, tables, images, notes and comments, as the file has them. splitDocument() divides it
/// into the texts and parts of the book (ImportPiece), at its headings, its page breaks or not
/// at all, and importKml() writes the KML of a text: the file's styles as the book's styles the
/// writer chose, as much of the file's own formatting as the writer keeps, with the cleanups of
/// the text (ImportCleanup). docs/kml_format.md says what each element of a file becomes.
///
/// The import is the reverse of the export (book_export.h): a file the program exported comes
/// back with its parts, texts, styles, notes and comments.

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/auto_correct.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_lists.h>

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace kalahari::editor {

// =============================================================================
// Encodings of plain text
// =============================================================================

/// @brief The encodings a plain text or Markdown file is read in
enum class TextEncoding {
    Utf8,         ///< UTF-8 (also with a byte order mark)
    Utf16,        ///< UTF-16, little or big endian (the byte order mark tells; without one,
                  ///< the places of the zero bytes)
    Windows1250,  ///< Windows-1250, Central European
    Iso8859_2,    ///< ISO 8859-2, Latin-2
};

/// @brief The encoding of a file's bytes: its byte order mark, else UTF-16 when every other
///        byte is zero, UTF-8 when the bytes are valid UTF-8, else the one of the two 8-bit
///        encodings whose letters the bytes are (Windows-1250 when both fit)
TextEncoding detectTextEncoding(const QByteArray& data);

/// @brief The text of a file's bytes in @p encoding, without its byte order mark, its line
///        ends (CR LF, CR) as '\n'
QString decodeText(const QByteArray& data, TextEncoding encoding);

// =============================================================================
// The document read from a file
// =============================================================================

/// @brief The formatting of a run of text in the file
struct ImportCharFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool subscript = false;
    bool superscript = false;
    QString font;        ///< Its own font family; empty: none
    double size = 0.0;   ///< Its own size in points; 0: none
    QString color;       ///< #rrggbb; empty: none (black, automatic)
    QString background;  ///< #rrggbb; empty: none
    QString caps;        ///< "small", "all"; empty: none
    double spacing = 0.0;  ///< Letter spacing in points

    /// @brief Whether it has bold, italic, underline, strikethrough or a raised or lowered
    ///        position
    bool hasEmphasis() const {
        return bold || italic || underline || strike || subscript || superscript;
    }

    bool operator==(const ImportCharFormat&) const = default;
};

/// @brief Whether a run of the file is in a tracked change
enum class ImportRevision {
    None,      ///< Not in a change
    Inserted,  ///< Inserted: in the text when the changes are accepted
    Deleted,   ///< Deleted: in the text when the changes are rejected
};

/// @brief A run of a paragraph: text in one format, or one object of the line
struct ImportRun {
    enum class Kind {
        Text,           ///< Characters (text)
        LineBreak,      ///< A line break in the paragraph
        Tab,            ///< A tab
        Note,           ///< The number of a footnote or an endnote (index: in notes)
        Image,          ///< An image (index: in images)
        Field,          ///< A field of KML (text: its type, "page"; field: its format)
        CaptionNumber,  ///< The number of a caption (text: its label, "Figure")
        Annotation,     ///< The place of an annotation on no text (index: in annotations)
    };

    Kind kind = Kind::Text;
    QString text;
    QString field;  ///< Kind::Field: its format; empty: the first of its type
    ImportCharFormat format;
    QString link;   ///< The address of its link (a page or an e-mail address); empty: none
    int index = -1;
    std::vector<int> annotations;  ///< The annotations whose fragment it is in
    ImportRevision revision = ImportRevision::None;
};

struct ImportTable;

/// @brief A block of the document: a paragraph, a line, a break or a table
struct ImportBlock {
    enum class Kind {
        Paragraph,       ///< A paragraph (a heading, a list item)
        HorizontalLine,  ///< A horizontal line
        SceneBreak,      ///< A scene break
        PageBreak,       ///< A page break
        Table,           ///< A table (table)
    };

    Kind kind = Kind::Paragraph;
    QString style;         ///< The key of its style (ImportDocument::styles); empty: the default
    int headingLevel = 0;  ///< 1 to 9: a heading of this level in the file; 0: none
    QString align;         ///< "left", "center", "right", "justify"; empty: its style's
    ParagraphFormat own;   ///< Its own formatting (ImportFormatting::All)
    std::vector<ImportRun> runs;

    int list = -1;         ///< A list item: its list (ImportDocument::lists); -1: none
    int listLevel = 0;     ///< 0 to LIST_LEVELS - 1
    bool numbered = true;  ///< The item shows its number or bullet
    std::optional<int> value;  ///< The item's own number

    bool startsText = false;  ///< A text starts here (SplitMode::Marks)
    ImportRevision revision = ImportRevision::None;  ///< Of its paragraph's end: an inserted
                                                     ///< or deleted paragraph
    std::shared_ptr<ImportTable> table;  ///< Kind::Table
};

/// @brief A cell of a table
struct ImportCell {
    int columnSpan = 1;
    int rowSpan = 1;
    QString background;  ///< #rrggbb; empty: none
    QString valign;      ///< "middle", "bottom"; empty: top
    std::vector<ImportBlock> blocks;  ///< Its paragraphs
};

/// @brief A row of a table
struct ImportRow {
    std::vector<ImportCell> cells;  ///< Without the places that cells above span into
    bool header = false;  ///< A header row (repeated at the top of each page)
    double height = 0.0;  ///< Its least height in points; 0: as high as its text
};

/// @brief A table
struct ImportTable {
    std::vector<double> columns;  ///< The widths of its columns, in any unit; empty: equal
    std::vector<ImportRow> rows;
    QString align;  ///< "center", "right"; empty: left
};

/// @brief A paragraph style of the file
struct ImportStyle {
    QString key;           ///< The file's id of it (Word's styleId; "heading_1" in Markdown)
    QString name;          ///< Its name in the file ("heading 1", "Tytuł rozdziału")
    QString basedOn;       ///< The key of its base style; empty: none
    int headingLevel = 0;  ///< 1 to 9: a heading of this level; 0: none
    int paragraphs = 0;    ///< The paragraphs of the text in it
};

/// @brief A footnote or an endnote
struct ImportNote {
    bool endnote = false;
    std::vector<ImportBlock> paragraphs;
};

/// @brief An image of the document
struct ImportImage {
    QByteArray data;   ///< The bytes of its file; empty: missing (missing) or to download (web)
    QString fileName;  ///< A name for its file in the book ("map.png"), with its extension
    QString address;   ///< Where the file named it (a path, an Internet address)
    ImageProperties look;  ///< Its size, crop, turn, text; without a source
    bool missing = false;  ///< The file names a file that cannot be read
    bool web = false;      ///< The file is on the Internet (address): not downloaded
};

/// @brief An annotation (a comment of Word)
struct ImportAnnotation {
    AnnotationKind kind = AnnotationKind::Comment;
    QString author;
    QDateTime created;
    bool done = false;
    QString text;
};

/// @brief What the import leaves out of a file
enum class ImportOmission {
    TextBox,          ///< A text box: its text goes after its paragraph
    Shape,            ///< A drawing of shapes
    Chart,            ///< A chart
    Equation,         ///< An equation
    Object,           ///< An embedded object (a spreadsheet, a picture of a program)
    HeaderFooter,     ///< The headers and footers of the pages
    TableOfContents,  ///< Word's table of contents (the book has its own)
    Image,            ///< An image the program cannot read (EMF, WMF, TIFF)
    Html,             ///< Markdown: HTML other than <u>, <sup>, <sub> and <br>
};

/// @brief A document read from a file
struct ImportDocument {
    QString title;     ///< Of its properties (Word's, the YAML header of Markdown)
    QString author;
    QString language;  ///< Its language code ("pl")
    bool fromKalahari = false;  ///< The program exported it

    std::vector<ImportBlock> blocks;
    std::vector<ImportStyle> styles;
    std::vector<ListDefinition> lists;
    std::vector<ImportNote> notes;
    std::vector<ImportImage> images;
    std::vector<ImportAnnotation> annotations;
    int trackedChanges = 0;  ///< The tracked changes of the file
    std::map<ImportOmission, int> omissions;  ///< What was left out, how many times

    /// @brief Its style with @p key, or nullptr
    const ImportStyle* style(const QString& key) const;

    /// @brief The level of the heading of a paragraph: its own, else of its style; 0: none
    int headingLevelOf(const ImportBlock& block) const;
};

/// @brief The document with its tracked changes accepted (the inserted text stays, the deleted
///        goes) or rejected (the reverse); its paragraphs of no other text in a deleted
///        paragraph go with it
ImportDocument withChangesResolved(const ImportDocument& document, bool reject);

// =============================================================================
// Readers
// =============================================================================

/// @brief What went wrong with the reading of a file
struct ImportProblem {
    enum class Kind {
        None,        ///< Nothing went wrong
        CannotOpen,  ///< The file cannot be opened
        NotDocx,     ///< The file is not a Word document (DOCX)
        Damaged,     ///< A part of the document cannot be read
    };

    Kind kind = Kind::None;
    QString detail;  ///< What went wrong, in English
};

/// @brief Read a Word document (DOCX)
///
/// Its paragraphs with their styles (styles.xml), lists (numbering.xml), tables, images
/// (in the text and anchored to a paragraph), notes, comments (with their replies and whether
/// they are resolved), fields, links and tracked changes (both versions, withChangesResolved()).
/// The headings are the paragraphs of an outline level, of their own or of their style
/// (Word's Heading 1-9); the program's export's part and chapter titles (styles PartTitle and
/// ChapterTitle) are headings of levels 1 and 2. A page break (w:br, pageBreakBefore, a section
/// of a new page) is a block; an empty paragraph with only a bottom border a line.
/// @return false, with what went wrong in @p problem, when it cannot be read
bool readDocx(const QString& path, ImportDocument* document, ImportProblem* problem);

/// @brief Read Markdown (GitHub Flavored, as Qt reads it), with footnotes ([^1]) and a YAML
///        header (title, author, lang)
///
/// The styles are the book's own ids: normal, heading_1 to heading_3 (#### and further as
/// heading_3), quote (>) and code (``` and indented code). A line of asterisks alone ("***",
/// "* * *") is a scene break, a paragraph of only a no-break space too (the export's blank
/// scene break); another thematic break ("---") a line.
/// @param folder The folder the paths of the images are relative to; empty: the images are
///        missing
ImportDocument readMarkdown(const QString& text, const QString& folder);

/// @brief The titles a plain text marks as the starts of its texts
enum class TitlePattern {
    None,     ///< None
    Polish,   ///< "Rozdział 1", "Rozdział I", "Rozdział pierwszy"
    English,  ///< "Chapter 1", "Chapter I", "Chapter One"
    Number,   ///< A number alone: "1", "1.", "I", "I."
    Capitals, ///< A line in capitals ("BURZA")
    Prefix,   ///< A line that starts with TextReadOptions::prefix
};

/// @brief The titles a plain text marks as the starts of its parts
enum class PartPattern {
    None,     ///< None
    Polish,   ///< "Część pierwsza", "Część I", "Księga 1", "Tom drugi"
    English,  ///< "Part One", "Part I", "Book 1", "Volume Two"
};

/// @brief How a plain text is read
struct TextReadOptions {
    bool emptyLineParagraphs = false;  ///< An empty line ends a paragraph, whose lines are
                                       ///< joined; false: each line is a paragraph
    TitlePattern titles = TitlePattern::None;
    QString prefix;  ///< TitlePattern::Prefix
    PartPattern parts = PartPattern::None;
    QStringList kindTitles;  ///< A line that is one of these names alone ("Prolog") is a title
    int emptyLinesForText = 0;  ///< At least so many empty lines start a text (startsText);
                                ///< 0: none do
};

/// @brief Read plain text: its paragraphs in the style "normal"; the part titles are headings
///        of level 1 (heading_1) and the titles of texts headings of level 2 (heading_2), of
///        level 1 when the text has no part titles; a form feed is a page break, and an empty
///        line an empty paragraph (in TextReadOptions::emptyLineParagraphs those after the
///        first)
ImportDocument readPlainText(const QString& text, const TextReadOptions& options);

/// @brief Whether the paragraphs of a plain text are separated by empty lines: more than half
///        of its lines of text are followed by an empty line, and some lines are not
bool paragraphsByEmptyLines(const QString& text);

/// @brief Whether a title names a part, in Polish or in English ("Część pierwsza", "Part II",
///        "Księga 1", "Book One")
bool isPartTitle(const QString& title);

// =============================================================================
// Texts and parts
// =============================================================================

/// @brief Where a document is divided into texts
enum class SplitMode {
    Headings,    ///< At its headings of the levels of SplitOptions
    PageBreaks,  ///< At its page breaks
    Marks,       ///< At its blocks that start a text (ImportBlock::startsText)
    Whole,       ///< Not at all: one text
};

/// @brief How a document is divided
struct SplitOptions {
    SplitMode mode = SplitMode::Headings;
    int partLevel = 0;  ///< A heading of this level with headings of textLevel after it before
                        ///< the next one of its level is a part; 0: no parts
    int textLevel = 1;  ///< A heading of this level starts a text (and one of partLevel
                        ///< without texts in it)
    bool headingAsTitle = true;  ///< The heading that starts a text is its title only: it is
                                 ///< not in its text
};

/// @brief A text or a part the document becomes
struct ImportPiece {
    bool part = false;    ///< A part (a group of texts): only its title
    bool inPart = false;  ///< A text of the part before it
    bool beforeTitles = false;  ///< The text before the first title (a title page)
    QString title;        ///< Its heading's text; empty: none
    size_t from = 0;      ///< Its blocks [from, to) of the document
    size_t to = 0;
    int words = 0;        ///< The words of its text, with those of its notes
};

/// @brief The texts and parts of a document
///
/// With SplitMode::Headings the text before the first heading that starts a text is one of
/// its own (beforeTitles), and a heading of another level stays in the text it is in. A part's
/// text before its first text is a text of the part with the part's title. With the page
/// breaks and the marks a text's title is its first heading, else the start of its first
/// paragraph. A text with nothing in it is left out, but one of a heading.
std::vector<ImportPiece> splitDocument(const ImportDocument& document,
                                       const SplitOptions& options);

/// @brief The number of the headings of each level of a document (of its body, not in tables)
std::map<int, int> headingCounts(const ImportDocument& document);

/// @brief The division a document suggests: at the headings when it has some (a part level
///        when the headings of the first level have those of the next under them), else at the
///        marks or its page breaks, else none
///
/// Chapters with the headings of their scenes look like parts with their chapters, so the
/// headings of the first level are parts only in the program's own file (its parts' titles)
/// and when one of them with headings of the next level under it names a part ("Część
/// pierwsza"). A single heading of the first level that names no part is the book's title,
/// and the texts start at the next level.
SplitOptions suggestedSplit(const ImportDocument& document);

/// @brief The names a kind of text has as a title, in any language ("Prolog", "Prologue")
struct KindTitle {
    QString kind;        ///< The kind's id
    QStringList titles;  ///< Its names
};

/// @brief The kind a title is the name of: a title that is a kind's name, alone or with a
///        title after it ("Prolog. Noc"), in any letter case; empty: none
QString kindOfTitle(const QString& title, const std::vector<KindTitle>& kinds);

// =============================================================================
// Styles
// =============================================================================

/// @brief The style of the book a style of the file becomes, as the program suggests it
///
/// The book's style it was exported from, a style of the book of the same name (in any letter
/// case), the book's style that Word's own style is (Normal, Heading 1-3, Quote, Caption,
/// Footnote Text, Plain Text...), the heading of its level (heading_3 below the third), else the
/// suggestion for its base style, else Normal.
QString suggestedBookStyle(const ImportDocument& document, const QString& key,
                           const ParagraphStyleSet& book);

// =============================================================================
// KML
// =============================================================================

/// @brief How much of the file's own formatting the text keeps
enum class ImportFormatting {
    All,       ///< Also the fonts, sizes, colors and the paragraphs' indents and spacing
    Emphasis,  ///< Bold, italic, underline, strikethrough, sub- and superscript, and the
               ///< paragraphs centered or aligned right (left and justified: the style's)
    StylesOnly,  ///< Only the styles
};

/// @brief What the empty paragraphs become
enum class EmptyParagraphs {
    Remove,      ///< They are left out
    Keep,        ///< They stay
    SceneBreak,  ///< Each run of them is a scene break (left out at the start and the end)
};

/// @brief The cleanups of the text; none in paragraphs in the style Code (or based on it)
struct ImportCleanup {
    bool typography = true;  ///< Quotation marks, apostrophes, dashes and ellipses as
                             ///< autocorrect makes them; double quotation marks of another
                             ///< language (“…”) as those of the book's language
    AutoCorrectOptions autoCorrect;  ///< Of typography: the dashes and what is changed
    QuoteMarks quoteMarks = QuoteMarks::forLanguage(QStringLiteral("en"));  ///< Of the book's
                                                                            ///< language
    bool doubleSpaces = true;   ///< Two and more spaces as one
    bool leadingSpaces = true;  ///< No spaces and tabs at the start of a paragraph
    bool sceneMarkers = true;   ///< A paragraph of "* * *", "***", "#", "⁂", "❦" or "───"
                                ///< alone is a scene break
    EmptyParagraphs empty = EmptyParagraphs::Remove;
};

/// @brief How the KML of a text is written
struct ImportKmlOptions {
    ImportFormatting formatting = ImportFormatting::Emphasis;
    QHash<QString, QString> styles;  ///< The book's style of each style of the file (by its
                                     ///< key); one without: suggestedBookStyle()
    ParagraphStyleSet bookStyles;    ///< The styles of the book
    bool dialogueStyle = false;  ///< A paragraph in Normal that starts with a dash goes to the
                                 ///< style dialogue (when the book has it)
    ImportCleanup cleanup;
    bool annotations = true;     ///< The comments as annotations; false: left out
    /// The words a comment starts with that make it a TODO or a note ("Do zrobienia", "To
    /// do"; the export writes them with a colon): the comment is of that kind, without them
    std::vector<std::pair<AnnotationKind, QString>> annotationLabels;
    std::vector<QString> imageSources;  ///< The source of each image in the book
                                        ///< ("images/map.png"); empty: its text stays
                                        ///< ("[map]"), or nothing
};

/// @brief The KML of a text of the document (<kml>...</kml>)
QString importKml(const ImportDocument& document, const ImportPiece& piece,
                  const ImportKmlOptions& options);

/// @brief The plain text of a paragraph: its characters, a line break a space, its notes and
///        images left out
QString importPlainText(const ImportBlock& block);

}  // namespace kalahari::editor
