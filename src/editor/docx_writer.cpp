// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file docx_writer.cpp
/// @brief Export of a book to a Word document (WordprocessingML, ECMA-376)
///
/// Each item of the export is a section of the document: a section starts it on a new page
/// (on a right one with mirror margins, unless the text starts on a left one) or goes on after
/// the item before, and it numbers the item's notes as the book does. The paragraph styles of
/// the book are the document's: Normal, Heading 1-3, Quote, Caption and the footnotes' style
/// are Word's own (Word shows their names in its language), the others have their names in
/// the program's language. The lists are Word's lists, the notes Word's footnotes and
/// endnotes, the annotations, when the export takes them, Word's comments.

#include "book_export_internal.h"

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_tables.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QTextTable>
#include <QTimeZone>

#include <zip.h>

#include <array>
#include <cmath>
#include <deque>
#include <map>

namespace kalahari::editor {

namespace {

using namespace exporting;

// =============================================================================
// Units and names
// =============================================================================

constexpr double POINTS_PER_MILLIMETRE = 72.0 / 25.4;

/// Twentieths of a point: the lengths of WordprocessingML
int twips(double points) {
    return static_cast<int>(std::lround(points * 20.0));
}

/// English Metric Units: the lengths of DrawingML
qint64 emu(double points) {
    return static_cast<qint64>(std::llround(points * 12700.0));
}

/// Half points: the font sizes of WordprocessingML
int halfPoints(double points) {
    return std::max(2, static_cast<int>(std::lround(points * 2.0)));
}

QString number(qint64 value) {
    return QString::number(value);
}

/// An attribute: a space, its name and its escaped value in quotes
QString attribute(const char* name, const QString& value) {
    QString escaped = xmlEscaped(value);
    escaped.replace(QLatin1Char('\n'), QLatin1String("&#10;"));
    return QLatin1Char(' ') + QLatin1String(name) + QStringLiteral("=\"") + escaped + QLatin1Char('"');
}

/// An empty element with one value: <w:name w:val="value"/>
QString valueElement(const char* name, const QString& value) {
    return QStringLiteral("<w:") + QLatin1String(name) + attribute("w:val", value) +
           QStringLiteral("/>");
}

/// A color as WordprocessingML writes it: RRGGBB
QString colorValue(const QColor& color) {
    return color.name(QColor::HexRgb).mid(1).toUpper();
}

/// A switch of the properties: <w:name/> on, <w:name w:val="0"/> off
QString flagElement(const char* name, bool on) {
    return on ? QStringLiteral("<w:") + QLatin1String(name) + QStringLiteral("/>")
              : valueElement(name, QStringLiteral("0"));
}

/// The language of the book as Word names it: "pl-PL"
QString languageTag(const QString& code) {
    if (code.isEmpty()) {
        return QString();
    }
    const QLocale locale(code);
    if (locale.language() == QLocale::C) {
        return code;
    }
    return locale.name().replace(QLatin1Char('_'), QLatin1Char('-'));
}

/// The value of an alignment of the properties: "left", "center", "right", "both"
QString alignmentValue(Qt::Alignment alignment) {
    QString name = styleAlignmentName(alignment);
    if (name == QLatin1String("justify")) {
        return QStringLiteral("both");
    }
    return name;
}

/// The format of a number of a list or a note, as WordprocessingML names it
QString numberFormatValue(ListNumberFormat format) {
    switch (format) {
        case ListNumberFormat::Decimal: return QStringLiteral("decimal");
        case ListNumberFormat::LowerAlpha: return QStringLiteral("lowerLetter");
        case ListNumberFormat::UpperAlpha: return QStringLiteral("upperLetter");
        case ListNumberFormat::LowerRoman: return QStringLiteral("lowerRoman");
        case ListNumberFormat::UpperRoman: return QStringLiteral("upperRoman");
        case ListNumberFormat::Bullet: return QStringLiteral("bullet");
    }
    return QStringLiteral("decimal");
}

QString noteFormatValue(NoteNumberFormat format) {
    switch (format) {
        case NoteNumberFormat::Decimal: return QStringLiteral("decimal");
        case NoteNumberFormat::LowerAlpha: return QStringLiteral("lowerLetter");
        case NoteNumberFormat::UpperAlpha: return QStringLiteral("upperLetter");
        case NoteNumberFormat::LowerRoman: return QStringLiteral("lowerRoman");
        case NoteNumberFormat::UpperRoman: return QStringLiteral("upperRoman");
        case NoteNumberFormat::Symbols: return QStringLiteral("chicago");  // *, †, ‡, §, **
    }
    return QStringLiteral("decimal");
}

/// A name of a bookmark (at most 40 characters, a letter or "_" first, no spaces)
QString bookmarkName(const QString& prefix, const QString& id) {
    QString name = prefix;
    for (const QChar ch : id) {
        name += (ch.isLetterOrNumber() && ch.unicode() < 128) ? ch : QChar(u'_');
    }
    return name.left(40);
}

const QString W_NAMESPACES = QStringLiteral(
    " xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
    " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\""
    " xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\""
    " xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\""
    " xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"");

const QString XML_HEAD =
    QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");

const QString REL_HYPERLINK = QStringLiteral(
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink");
const QString REL_IMAGE =
    QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/image");

// =============================================================================
// Properties of runs and paragraphs
// =============================================================================

/// The run properties of a character format (the order of the schema), with a character
/// style; empty without any
QString runProperties(const QTextCharFormat& format, const QString& characterStyle = QString(),
                      bool noProof = false) {
    QString properties;
    if (!characterStyle.isEmpty()) {
        properties += valueElement("rStyle", characterStyle);
    }
    QString family;
    const QStringList families = format.fontFamilies().toStringList();
    if (!families.isEmpty()) {
        family = families.constFirst();
    } else if (format.hasProperty(QTextFormat::FontFamily)) {
        family = format.stringProperty(QTextFormat::FontFamily);
    }
    if (!family.isEmpty()) {
        properties += QStringLiteral("<w:rFonts") + attribute("w:ascii", family) +
                      attribute("w:hAnsi", family) + attribute("w:eastAsia", family) +
                      attribute("w:cs", family) + QStringLiteral("/>");
    }
    if (format.hasProperty(QTextFormat::FontWeight)) {
        const bool bold = format.fontWeight() > QFont::Normal;
        properties += flagElement("b", bold) + flagElement("bCs", bold);
    }
    if (format.hasProperty(QTextFormat::FontItalic)) {
        properties += flagElement("i", format.fontItalic()) + flagElement("iCs", format.fontItalic());
    }
    if (format.fontCapitalization() == QFont::AllUppercase) {
        properties += QStringLiteral("<w:caps/>");
    } else if (format.fontCapitalization() == QFont::SmallCaps) {
        properties += QStringLiteral("<w:smallCaps/>");
    }
    if (format.hasProperty(QTextFormat::FontStrikeOut)) {
        properties += flagElement("strike", format.fontStrikeOut());
    }
    if (noProof) {
        properties += QStringLiteral("<w:noProof/>");
    }
    if (format.foreground().style() != Qt::NoBrush && format.foreground().color().isValid()) {
        properties += valueElement("color", colorValue(format.foreground().color()));
    }
    if (const double spacing = format.doubleProperty(KmlPropLetterSpacing); spacing != 0.0) {
        properties += valueElement("spacing", number(twips(spacing)));
    }
    if (format.fontPointSize() > 0) {
        const QString size = number(halfPoints(format.fontPointSize()));
        properties += valueElement("sz", size) + valueElement("szCs", size);
    }
    if (format.hasProperty(QTextFormat::TextUnderlineStyle) ||
        format.hasProperty(QTextFormat::FontUnderline)) {
        properties += valueElement("u", format.fontUnderline() ? QStringLiteral("single")
                                                              : QStringLiteral("none"));
    }
    if (format.background().style() != Qt::NoBrush && format.background().color().isValid() &&
        format.background().color().alpha() > 0) {
        properties += QStringLiteral("<w:shd w:val=\"clear\" w:color=\"auto\"") +
                      attribute("w:fill", colorValue(format.background().color())) +
                      QStringLiteral("/>");
    }
    if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript) {
        properties += valueElement("vertAlign", QStringLiteral("superscript"));
    } else if (format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
        properties += valueElement("vertAlign", QStringLiteral("subscript"));
    }
    return properties.isEmpty() ? QString() : QStringLiteral("<w:rPr>") + properties + QStringLiteral("</w:rPr>");
}

/// A run of text: its characters, its tabs and line breaks
QString textRun(const QString& text, const QString& properties) {
    QString run = QStringLiteral("<w:r>") + properties;
    QString characters;
    const auto flush = [&run, &characters]() {
        if (!characters.isEmpty()) {
            run += QStringLiteral("<w:t xml:space=\"preserve\">") + xmlEscaped(characters) +
                   QStringLiteral("</w:t>");
            characters.clear();
        }
    };
    for (const QChar ch : text) {
        if (ch == u'\t') {
            flush();
            run += QStringLiteral("<w:tab/>");
        } else if (ch == QChar::LineSeparator || ch == u'\n' || ch == u'\r' || ch == u'\v') {
            flush();
            run += QStringLiteral("<w:br/>");
        } else if (ch == QChar::ParagraphSeparator || ch == QChar::ObjectReplacementCharacter) {
            continue;
        } else {
            characters += ch;
        }
    }
    flush();
    return run + QStringLiteral("</w:r>");
}

/// The properties of a paragraph, each part set or left to its style
struct ParagraphProperties {
    QString style;
    std::optional<bool> keepNext;
    std::optional<bool> keepLines;
    std::optional<bool> pageBreakBefore;
    std::optional<bool> widowControl;
    QString numbering;  ///< <w:numPr>...</w:numPr>
    QString borders;    ///< <w:pBdr>...</w:pBdr>
    std::optional<double> spaceBefore;  ///< Points
    std::optional<double> spaceAfter;
    std::optional<LineSpacing> lineSpacing;
    std::optional<double> leftIndent;
    std::optional<double> rightIndent;
    std::optional<double> firstLineIndent;  ///< Negative: hanging
    QString alignment;  ///< "left", "center", "right", "both"
    std::optional<int> outlineLevel;
    bool noProof = false;  ///< Of a style: no spelling check (in its run properties)

    /// @brief The parts of @p own that are set, over these
    void merge(const ParagraphFormat& own) {
        if (own.leftIndent) leftIndent = own.leftIndent;
        if (own.rightIndent) rightIndent = own.rightIndent;
        if (own.firstLineIndent) firstLineIndent = own.firstLineIndent;
        if (own.spaceBefore) spaceBefore = own.spaceBefore;
        if (own.spaceAfter) spaceAfter = own.spaceAfter;
        if (own.lineSpacing) lineSpacing = own.lineSpacing;
        if (own.keepWithNext) keepNext = own.keepWithNext;
        if (own.keepTogether) keepLines = own.keepTogether;
        if (own.pageBreakBefore) pageBreakBefore = own.pageBreakBefore;
        if (own.widowControl) widowControl = own.widowControl;
    }

    /// @brief The elements inside <w:pPr>, in the order of the schema
    QString xml() const {
        QString result;
        if (!style.isEmpty()) result += valueElement("pStyle", style);
        if (keepNext) result += flagElement("keepNext", *keepNext);
        if (keepLines) result += flagElement("keepLines", *keepLines);
        if (pageBreakBefore) result += flagElement("pageBreakBefore", *pageBreakBefore);
        if (widowControl) result += flagElement("widowControl", *widowControl);
        result += numbering;
        result += borders;
        if (spaceBefore || spaceAfter || lineSpacing) {
            result += QStringLiteral("<w:spacing");
            if (spaceBefore) result += attribute("w:before", number(twips(*spaceBefore)));
            if (spaceAfter) result += attribute("w:after", number(twips(*spaceAfter)));
            if (lineSpacing) {
                switch (lineSpacing->rule) {
                    case LineSpacingRule::Multiple:
                        result += attribute("w:line", number(std::lround(lineSpacing->value * 240.0))) +
                                  attribute("w:lineRule", QStringLiteral("auto"));
                        break;
                    case LineSpacingRule::AtLeast:
                        result += attribute("w:line", number(twips(lineSpacing->value))) +
                                  attribute("w:lineRule", QStringLiteral("atLeast"));
                        break;
                    case LineSpacingRule::Exactly:
                        result += attribute("w:line", number(twips(lineSpacing->value))) +
                                  attribute("w:lineRule", QStringLiteral("exact"));
                        break;
                }
            }
            result += QStringLiteral("/>");
        }
        if (leftIndent || rightIndent || firstLineIndent) {
            result += QStringLiteral("<w:ind");
            if (leftIndent) result += attribute("w:left", number(twips(*leftIndent)));
            if (rightIndent) result += attribute("w:right", number(twips(*rightIndent)));
            if (firstLineIndent) {
                result += *firstLineIndent < 0
                              ? attribute("w:hanging", number(twips(-*firstLineIndent)))
                              : attribute("w:firstLine", number(twips(*firstLineIndent)));
            }
            result += QStringLiteral("/>");
        }
        if (!alignment.isEmpty()) result += valueElement("jc", alignment);
        if (outlineLevel) result += valueElement("outlineLvl", number(*outlineLevel));
        return result;
    }
};

/// The paragraph and run properties of a style's properties (as its package names them)
void styleProperties(const QVariantMap& properties, ParagraphProperties* paragraph,
                     QString* run) {
    const auto numberOf = [&properties](const char* name) -> std::optional<double> {
        const QVariant value = properties.value(QLatin1String(name));
        bool ok = false;
        const double result = value.toDouble(&ok);
        return value.isValid() && ok ? std::optional<double>(result) : std::nullopt;
    };
    const auto flagOf = [&properties](const char* name) -> std::optional<bool> {
        const QVariant value = properties.value(QLatin1String(name));
        return value.isValid() ? std::optional<bool>(value.toBool()) : std::nullopt;
    };
    paragraph->leftIndent = numberOf("leftMargin");
    paragraph->rightIndent = numberOf("rightMargin");
    paragraph->firstLineIndent = numberOf("firstLineIndent");
    paragraph->spaceBefore = numberOf("spaceBefore");
    paragraph->spaceAfter = numberOf("spaceAfter");
    if (const std::optional<double> lineHeight = numberOf("lineHeight"); lineHeight && *lineHeight > 0) {
        paragraph->lineSpacing = LineSpacing{LineSpacingRule::Multiple, *lineHeight};
    }
    paragraph->keepNext = flagOf("keepWithNext");
    paragraph->keepLines = flagOf("keepTogether");
    paragraph->pageBreakBefore = flagOf("pageBreakBefore");
    paragraph->widowControl = flagOf("widowControl");
    if (const Qt::Alignment alignment =
            styleAlignmentOf(properties.value(QStringLiteral("alignment")).toString());
        alignment) {
        paragraph->alignment = alignmentValue(alignment);
    }
    if (const std::optional<bool> check = flagOf("spellCheck"); check && !*check) {
        paragraph->noProof = true;
    }

    QTextCharFormat format;
    if (const QString family = properties.value(QStringLiteral("fontFamily")).toString(); !family.isEmpty()) {
        format.setFontFamilies(QStringList{family});
    }
    if (const std::optional<double> size = numberOf("fontSize"); size && *size > 0) {
        format.setFontPointSize(*size);
    }
    if (const std::optional<bool> bold = flagOf("bold")) {
        format.setFontWeight(*bold ? QFont::Bold : QFont::Normal);
    }
    if (const std::optional<bool> italic = flagOf("italic")) {
        format.setFontItalic(*italic);
    }
    if (const std::optional<bool> underline = flagOf("underline")) {
        format.setFontUnderline(*underline);
    }
    if (const QColor color(properties.value(QStringLiteral("textColor")).toString()); color.isValid()) {
        format.setForeground(color);
    }
    *run = runProperties(format, QString(), paragraph->noProof);
}

// =============================================================================
// The package
// =============================================================================

/// The relationships of a part of the package
struct Relationships {
    QStringList entries;
    int next = 1;

    QString add(const QString& type, const QString& target, bool external = false) {
        const QString id = QStringLiteral("rId%1").arg(next++);
        entries << QStringLiteral("<Relationship") + attribute("Id", id) + attribute("Type", type) +
                       attribute("Target", target) +
                       (external ? QStringLiteral(" TargetMode=\"External\"") : QString()) +
                       QStringLiteral("/>");
        return id;
    }

    QString xml() const {
        return XML_HEAD +
               QStringLiteral("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">") +
               entries.join(QString()) + QStringLiteral("</Relationships>");
    }
};

/// A list of a text: its numbering in Word and the numbers Word gives its levels
struct WordList {
    int abstractId = 0;
    int numId = 0;
    ListDefinition definition;
    std::array<int, LIST_LEVELS> counters{};  ///< The last number of each level; -1: none

    WordList() { counters.fill(-1); }
};

/// The comment of an annotation
struct WordComment {
    int id = 0;
    Annotation annotation;
};

/// The writer of the document
class DocxWriter {
public:
    DocxWriter(const BookExport& book, const ExportProgress& progress)
        : m_book(book), m_progress(progress) {
        m_images.setFolder(book.imagesFolder);
        m_textWidth = book.page.textWidthPoints();
        m_textHeight = book.page.textHeightPoints();
        mapStyles();
    }

    /// @brief The package's files, as they go to the archive
    std::vector<std::pair<QString, QByteArray>> build(ExportResult* result);

private:
    // The parts with relationships of their own
    enum class Part { Document, Footnotes, Endnotes, Comments };

    void mapStyles();
    QString wordStyleOf(const QString& id) const;
    QString stylesXml() const;
    QString numberingXml() const;
    QString settingsXml() const;
    QString footerXml() const;
    QString notesXml(bool endnotes) const;
    QString commentsXml() const;
    QString commentsExtendedXml() const;

    void collectHeadings();
    void writeItem(size_t index);
    void writeContents(const ExportItem& item);
    void writeBlocks(const ExportText& text, size_t item, int fromBlock, int toBlock,
                     double lineWidth, bool topLevel);
    void writeTable(const ExportText& text, size_t item, QTextTable& table, double lineWidth);
    void writeParagraph(const ExportText& text, size_t item, const QTextBlock& block,
                        double lineWidth, bool topLevel);
    void writeObject(const QTextBlock& block, TextObject object, const ExportText& text,
                     bool topLevel);
    QString paragraphContent(const ExportText& text, const QTextBlock& block, double lineWidth,
                             Part part);
    QString commentEvents(const ExportText& text, int upTo, Part part);
    QString drawing(const ImageProperties& image, double lineWidth, Part part);
    QString noteXml(const Footnote& note, int id, bool endnote);
    QString hyperlinkStart(const QString& address, Part part);
    QString listNumbering(size_t item, const ListItem& listItem, int number);
    void appendParagraph(const QString& properties, const QString& content, bool topLevel);
    void endSection(size_t index);
    QString sectionProperties(size_t index) const;

    Relationships& relationshipsOf(Part part) {
        switch (part) {
            case Part::Footnotes: return m_footnoteRels;
            case Part::Endnotes: return m_endnoteRels;
            case Part::Comments: return m_commentRels;
            case Part::Document: break;
        }
        return m_documentRels;
    }

    const BookExport& m_book;
    ExportProgress m_progress;
    ImageStore m_images;
    double m_textWidth = 0.0;
    double m_textHeight = 0.0;
    std::vector<std::unique_ptr<ExportText>> m_texts;
    std::vector<bool> m_withTitle;  ///< Of each item: its title goes to the file

    QHash<QString, QString> m_styleIds;    ///< Word's id of each style of the book
    QHash<QString, QString> m_styleNames;  ///< Word's name of each style of the book

    QString m_body;
    qsizetype m_lastParagraphProperties = -1;  ///< Where the pPr of the last paragraph ends
    bool m_lastIsParagraph = false;            ///< The body ends with a paragraph (not a table)
    bool m_pageBreakPending = false;           ///< The next paragraph starts on a new page

    Relationships m_documentRels;
    Relationships m_footnoteRels;
    Relationships m_endnoteRels;
    Relationships m_commentRels;
    QHash<QByteArray, QString> m_mediaByHash;  ///< The media file of each picture's data
    std::vector<std::pair<QString, QByteArray>> m_media;
    QHash<QString, QString> m_imageRels[4];    ///< The relationship of each media file, by part
    QHash<QString, QString> m_linkRels[4];     ///< The relationship of each address, by part
    QString m_footerRelationship;              ///< The footer with the page numbers; empty: none
    QSet<QString> m_extensions;
    int m_nextDrawing = 1;
    int m_images_written = 0;
    int m_missingImages = 0;

    std::vector<QString> m_abstractNums;
    std::vector<QString> m_nums;
    std::map<std::pair<size_t, int>, WordList> m_lists;

    QStringList m_footnotes;
    QStringList m_endnotes;
    int m_nextNote = 1;
    std::vector<int> m_footnotesOf;  ///< The footnotes written by the end of each item
    int m_footnotesWritten = 0;
    int m_endnotesWritten = 0;
    std::vector<int> m_footnotesBefore;  ///< Of each item: how many footnotes Word gives before it
    std::vector<int> m_endnotesBefore;
    std::vector<int> m_footnotesIn;      ///< Of each item: how many footnotes it has
    std::vector<int> m_endnotesIn;

    std::vector<WordComment> m_comments;
    QHash<QString, int> m_commentIds;  ///< The comment of each annotation started
    QSet<QString> m_commentsEnded;     ///< The annotations whose comments ended

    int m_nextBookmark = 1;
    QHash<QPair<int, int>, QString> m_headingBookmarks;  ///< (item, block; -1 its title)
    struct Heading {
        int level = 1;
        QString text;
        QString bookmark;
    };
    std::vector<Heading> m_headings;
    QSet<QString> m_captionIds;  ///< The captions of the file (cross-references go to them)
};

// =============================================================================
// Styles
// =============================================================================

/// Word's own styles a style of the book becomes: its id and its name (Word shows the name in
/// its language)
struct BuiltInStyle {
    const char* bookId;
    const char* wordId;
    const char* wordName;
};

constexpr std::array<BuiltInStyle, 7> BUILT_IN_STYLES{{
    {"normal", "Normal", "Normal"},
    {"heading_1", "Heading1", "heading 1"},
    {"heading_2", "Heading2", "heading 2"},
    {"heading_3", "Heading3", "heading 3"},
    {"quote", "Quote", "Quote"},
    {"caption", "Caption", "caption"},
    {"footnote", "FootnoteText", "footnote text"},
}};

/// The ids and names of Word's styles the writer adds itself
const QStringList& reservedStyleNames() {
    static const QStringList names{
        QStringLiteral("Normal"), QStringLiteral("heading 1"), QStringLiteral("heading 2"),
        QStringLiteral("heading 3"), QStringLiteral("Quote"), QStringLiteral("caption"),
        QStringLiteral("footnote text"), QStringLiteral("footnote reference"),
        QStringLiteral("endnote text"), QStringLiteral("endnote reference"),
        QStringLiteral("Hyperlink"), QStringLiteral("footer"), QStringLiteral("TOC Heading"),
        QStringLiteral("toc 1"), QStringLiteral("toc 2"), QStringLiteral("toc 3"),
        QStringLiteral("table of figures"), QStringLiteral("annotation text"),
        QStringLiteral("annotation reference"), QStringLiteral("Default Paragraph Font"),
        QStringLiteral("Normal Table"), QStringLiteral("No List")};
    return names;
}

void DocxWriter::mapStyles() {
    QSet<QString> ids;
    QSet<QString> names;
    for (const QString& name : reservedStyleNames()) {
        names.insert(name.toLower());
    }
    for (const char* id : {"DefaultParagraphFont", "TableNormal", "NoList", "FootnoteReference",
                           "EndnoteText", "EndnoteReference", "Hyperlink", "Footer", "TOCHeading",
                           "TOC1", "TOC2", "TOC3", "TableofFigures", "CommentText",
                           "CommentReference"}) {
        ids.insert(QString::fromLatin1(id).toLower());
    }
    for (const BuiltInStyle& style : BUILT_IN_STYLES) {
        ids.insert(QString::fromLatin1(style.wordId).toLower());
    }
    for (const ParagraphStyle& style : m_book.styles.styles()) {
        bool builtIn = false;
        for (const BuiltInStyle& word : BUILT_IN_STYLES) {
            if (style.id == QLatin1String(word.bookId)) {
                m_styleIds.insert(style.id, QString::fromLatin1(word.wordId));
                m_styleNames.insert(style.id, QString::fromLatin1(word.wordName));
                builtIn = true;
            }
        }
        if (builtIn) {
            continue;
        }
        // The id: its words in capitals ("chapter_title": ChapterTitle), unique
        QString base;
        bool upper = true;
        for (const QChar ch : style.id) {
            if (ch.isLetterOrNumber() && ch.unicode() < 128) {
                base += upper ? ch.toUpper() : ch;
                upper = false;
            } else {
                upper = true;
            }
        }
        if (base.isEmpty() || !base.at(0).isLetter()) {
            base.prepend(QStringLiteral("Style"));
        }
        QString id = base;
        for (int n = 2; ids.contains(id.toLower()); ++n) {
            id = base + QString::number(n);
        }
        ids.insert(id.toLower());
        // The name: the book's, unique and none of Word's own
        const QString wanted = style.name.trimmed().isEmpty() ? style.id : style.name.trimmed();
        QString name = wanted;
        for (int n = 2; names.contains(name.toLower()); ++n) {
            name = QStringLiteral("%1 (%2)").arg(wanted).arg(n);
        }
        names.insert(name.toLower());
        m_styleIds.insert(style.id, id);
        m_styleNames.insert(style.id, name);
    }
}

QString DocxWriter::wordStyleOf(const QString& id) const {
    return m_styleIds.value(id, QStringLiteral("Normal"));
}

QString DocxWriter::stylesXml() const {
    const ParagraphStyle& normal = m_book.styles.normal();
    const double normalSize = [&normal]() {
        bool ok = false;
        const double size = normal.properties.value(QStringLiteral("fontSize")).toDouble(&ok);
        return ok && size > 0 ? size : DEFAULT_NORMAL_POINT_SIZE;
    }();
    const QString font = m_book.fontFamily;
    const QString language = languageTag(m_book.language);

    QString xml = XML_HEAD + QStringLiteral("<w:styles") + W_NAMESPACES + QStringLiteral(">");
    xml += QStringLiteral("<w:docDefaults><w:rPrDefault><w:rPr>");
    if (!font.isEmpty()) {
        xml += QStringLiteral("<w:rFonts") + attribute("w:ascii", font) + attribute("w:hAnsi", font) +
               attribute("w:eastAsia", font) + attribute("w:cs", font) + QStringLiteral("/>");
    }
    xml += valueElement("sz", number(halfPoints(normalSize))) +
           valueElement("szCs", number(halfPoints(normalSize)));
    if (!language.isEmpty()) {
        xml += QStringLiteral("<w:lang") + attribute("w:val", language) + attribute("w:eastAsia", language) +
               attribute("w:bidi", language) + QStringLiteral("/>");
    }
    // Without a style's own: single lines, no spacing, justified, no widows (as the editor)
    xml += QStringLiteral("</w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:widowControl/>"
                          "<w:spacing w:after=\"0\" w:line=\"240\" w:lineRule=\"auto\"/>"
                          "<w:jc w:val=\"both\"/></w:pPr></w:pPrDefault></w:docDefaults>");

    const auto style = [](const QString& type, const QString& id, const QString& name,
                          const QString& basedOn, const QString& next, const QString& extra,
                          const QString& pPr, const QString& rPr, bool isDefault = false) {
        QString result = QStringLiteral("<w:style") + attribute("w:type", type) +
                         (isDefault ? QStringLiteral(" w:default=\"1\"") : QString()) +
                         attribute("w:styleId", id) + QStringLiteral(">") + valueElement("name", name);
        if (!basedOn.isEmpty()) result += valueElement("basedOn", basedOn);
        if (!next.isEmpty()) result += valueElement("next", next);
        result += extra;
        if (!pPr.isEmpty()) result += QStringLiteral("<w:pPr>") + pPr + QStringLiteral("</w:pPr>");
        result += rPr;
        return result + QStringLiteral("</w:style>");
    };

    // The book's styles
    for (const ParagraphStyle& bookStyle : m_book.styles.styles()) {
        const bool isNormal = bookStyle.id == QLatin1String(NORMAL_STYLE_ID);
        const bool hasBase = !isNormal && !bookStyle.baseStyle.isEmpty() &&
                             m_book.styles.contains(bookStyle.baseStyle);
        ParagraphProperties paragraph;
        QString run;
        QVariantMap properties = hasBase ? bookStyle.ownProperties : bookStyle.properties;
        if (isNormal && !font.isEmpty()) {
            properties.remove(QStringLiteral("fontFamily"));  // the export's font, in the defaults
        }
        styleProperties(properties, &paragraph, &run);
        const int level = headingLevelOf(bookStyle.id);
        if (level > 0) {
            paragraph.outlineLevel = level - 1;
        }
        QString next;
        if (!bookStyle.nextStyle.isEmpty() && m_book.styles.contains(bookStyle.nextStyle)) {
            next = wordStyleOf(bookStyle.nextStyle);
        }
        const QString extra = level > 0 || isNormal ? QStringLiteral("<w:uiPriority w:val=\"%1\"/><w:qFormat/>")
                                                          .arg(isNormal ? 0 : 9)
                                                    : QStringLiteral("<w:qFormat/>");
        xml += style(QStringLiteral("paragraph"), wordStyleOf(bookStyle.id), m_styleNames.value(bookStyle.id),
                     hasBase ? wordStyleOf(bookStyle.baseStyle) : QString(), next, extra,
                     paragraph.xml(), run, isNormal);
    }

    // Word's own styles the document uses
    const QString normalId = QStringLiteral("Normal");
    const QString noIndent = QStringLiteral("<w:ind w:left=\"0\" w:right=\"0\" w:firstLine=\"0\"/>");
    xml += style(QStringLiteral("character"), QStringLiteral("DefaultParagraphFont"),
                 QStringLiteral("Default Paragraph Font"), {}, {},
                 QStringLiteral("<w:uiPriority w:val=\"1\"/><w:semiHidden/><w:unhideWhenUsed/>"), {}, {},
                 true);
    xml += QStringLiteral(
        "<w:style w:type=\"table\" w:default=\"1\" w:styleId=\"TableNormal\"><w:name w:val=\"Normal Table\"/>"
        "<w:uiPriority w:val=\"99\"/><w:semiHidden/><w:unhideWhenUsed/>"
        "<w:tblPr><w:tblInd w:w=\"0\" w:type=\"dxa\"/><w:tblCellMar><w:top w:w=\"0\" w:type=\"dxa\"/>"
        "<w:left w:w=\"108\" w:type=\"dxa\"/><w:bottom w:w=\"0\" w:type=\"dxa\"/>"
        "<w:right w:w=\"108\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr></w:style>");
    xml += QStringLiteral(
        "<w:style w:type=\"numbering\" w:default=\"1\" w:styleId=\"NoList\"><w:name w:val=\"No List\"/>"
        "<w:uiPriority w:val=\"99\"/><w:semiHidden/><w:unhideWhenUsed/></w:style>");
    const QString hidden = QStringLiteral("<w:uiPriority w:val=\"99\"/><w:semiHidden/><w:unhideWhenUsed/>");
    xml += style(QStringLiteral("character"), QStringLiteral("FootnoteReference"),
                 QStringLiteral("footnote reference"), QStringLiteral("DefaultParagraphFont"), {}, hidden,
                 {}, QStringLiteral("<w:rPr><w:vertAlign w:val=\"superscript\"/></w:rPr>"));
    xml += style(QStringLiteral("paragraph"), QStringLiteral("EndnoteText"), QStringLiteral("endnote text"),
                 m_styleIds.contains(QStringLiteral("footnote")) ? QStringLiteral("FootnoteText") : normalId,
                 {}, hidden, {}, {});
    xml += style(QStringLiteral("character"), QStringLiteral("EndnoteReference"),
                 QStringLiteral("endnote reference"), QStringLiteral("DefaultParagraphFont"), {}, hidden,
                 {}, QStringLiteral("<w:rPr><w:vertAlign w:val=\"superscript\"/></w:rPr>"));
    if (!m_styleIds.contains(QStringLiteral("footnote"))) {
        xml += style(QStringLiteral("paragraph"), QStringLiteral("FootnoteText"),
                     QStringLiteral("footnote text"), normalId, {}, hidden, noIndent,
                     QStringLiteral("<w:rPr><w:sz w:val=\"20\"/><w:szCs w:val=\"20\"/></w:rPr>"));
    }
    if (!m_styleIds.contains(QStringLiteral("caption"))) {
        xml += style(QStringLiteral("paragraph"), QStringLiteral("Caption"), QStringLiteral("caption"),
                     normalId, normalId, QStringLiteral("<w:qFormat/>"), noIndent,
                     QStringLiteral("<w:rPr><w:i/><w:iCs/></w:rPr>"));
    }
    xml += style(QStringLiteral("character"), QStringLiteral("Hyperlink"), QStringLiteral("Hyperlink"),
                 QStringLiteral("DefaultParagraphFont"), {}, hidden, {},
                 QStringLiteral("<w:rPr><w:color w:val=\"0563C1\"/><w:u w:val=\"single\"/></w:rPr>"));
    xml += style(QStringLiteral("paragraph"), QStringLiteral("Footer"), QStringLiteral("footer"), normalId, {},
                 hidden, noIndent + QStringLiteral("<w:jc w:val=\"center\"/>"), {});
    const QString headingBase = m_styleIds.contains(headingStyleId(1)) ? QStringLiteral("Heading1") : normalId;
    xml += style(QStringLiteral("paragraph"), QStringLiteral("TOCHeading"), QStringLiteral("TOC Heading"),
                 headingBase, normalId, QStringLiteral("<w:uiPriority w:val=\"39\"/><w:unhideWhenUsed/><w:qFormat/>"),
                 QStringLiteral("<w:outlineLvl w:val=\"9\"/>"), {});
    for (int level = 1; level <= HEADING_LEVELS; ++level) {
        xml += style(QStringLiteral("paragraph"), QStringLiteral("TOC%1").arg(level),
                     QStringLiteral("toc %1").arg(level), normalId, normalId,
                     QStringLiteral("<w:uiPriority w:val=\"39\"/><w:unhideWhenUsed/>"),
                     QStringLiteral("<w:spacing w:after=\"100\"/><w:ind w:left=\"%1\" w:right=\"0\" w:firstLine=\"0\"/>"
                                    "<w:jc w:val=\"left\"/>")
                         .arg((level - 1) * 220),
                     {});
    }
    xml += style(QStringLiteral("paragraph"), QStringLiteral("TableofFigures"), QStringLiteral("table of figures"),
                 normalId, normalId, hidden, noIndent + QStringLiteral("<w:jc w:val=\"left\"/>"), {});
    xml += style(QStringLiteral("paragraph"), QStringLiteral("CommentText"), QStringLiteral("annotation text"),
                 normalId, {}, hidden, noIndent + QStringLiteral("<w:jc w:val=\"left\"/>"),
                 QStringLiteral("<w:rPr><w:sz w:val=\"20\"/><w:szCs w:val=\"20\"/></w:rPr>"));
    xml += style(QStringLiteral("character"), QStringLiteral("CommentReference"),
                 QStringLiteral("annotation reference"), QStringLiteral("DefaultParagraphFont"), {}, hidden, {},
                 QStringLiteral("<w:rPr><w:sz w:val=\"16\"/><w:szCs w:val=\"16\"/></w:rPr>"));
    return xml + QStringLiteral("</w:styles>");
}

// =============================================================================
// Lists
// =============================================================================

QString DocxWriter::listNumbering(size_t item, const ListItem& listItem, int shownNumber) {
    const auto key = std::make_pair(item, listItem.id);
    auto found = m_lists.find(key);
    if (found == m_lists.end()) {
        WordList list;
        list.definition = listItem.definition;
        list.abstractId = static_cast<int>(m_abstractNums.size());
        QString abstract = QStringLiteral("<w:abstractNum w:abstractNumId=\"%1\">"
                                          "<w:multiLevelType w:val=\"multilevel\"/>")
                               .arg(list.abstractId);
        for (int level = 0; level < LIST_LEVELS; ++level) {
            const ListLevel& format = list.definition.levels[static_cast<size_t>(level)];
            const QString text = format.isBullet()
                                     ? (format.bullet.isEmpty() ? QString(QChar(BULLET_DISC)) : format.bullet)
                                     : QStringLiteral("%%1").arg(level + 1) + format.suffix;
            const QString alignment = format.alignment == ListMarkerAlignment::Center ? QStringLiteral("center")
                                      : format.alignment == ListMarkerAlignment::Right ? QStringLiteral("right")
                                                                                        : QStringLiteral("left");
            const double hanging = std::max(0.0, format.textIndent - format.markerIndent);
            abstract += QStringLiteral("<w:lvl w:ilvl=\"%1\">").arg(level) +
                        valueElement("start", number(std::max(0, format.start))) +
                        valueElement("numFmt", numberFormatValue(format.format)) +
                        valueElement("lvlText", text) + valueElement("lvlJc", alignment) +
                        QStringLiteral("<w:pPr><w:ind") + attribute("w:left", number(twips(format.textIndent))) +
                        attribute("w:hanging", number(twips(hanging))) + QStringLiteral("/></w:pPr>") +
                        (format.isBullet() ? QStringLiteral("<w:rPr><w:rFonts w:hint=\"default\"/></w:rPr>") : QString()) +
                        QStringLiteral("</w:lvl>");
        }
        abstract += QStringLiteral("</w:abstractNum>");
        m_abstractNums.push_back(abstract);
        list.numId = static_cast<int>(m_nums.size()) + 1;
        m_nums.push_back(QStringLiteral("<w:num w:numId=\"%1\"><w:abstractNumId w:val=\"%2\"/></w:num>")
                             .arg(list.numId)
                             .arg(list.abstractId));
        found = m_lists.emplace(key, list).first;
    }
    WordList& list = found->second;
    const int level = std::clamp(listItem.level, 0, LIST_LEVELS - 1);
    const ListLevel& format = list.definition.levels[static_cast<size_t>(level)];
    const int expected = list.counters[static_cast<size_t>(level)] >= 0
                             ? list.counters[static_cast<size_t>(level)] + 1
                             : format.start;
    if (!format.isBullet() && shownNumber >= 0 && shownNumber != expected) {
        // The item has a number of its own: a new instance of the list that starts the level
        // at it, and the levels above it where they are now
        QString overrides;
        for (int above = 0; above < level; ++above) {
            const int counter = list.counters[static_cast<size_t>(above)];
            if (counter >= 0) {
                overrides += QStringLiteral("<w:lvlOverride w:ilvl=\"%1\"><w:startOverride w:val=\"%2\"/></w:lvlOverride>")
                                 .arg(above)
                                 .arg(counter + 1);
            }
        }
        overrides += QStringLiteral("<w:lvlOverride w:ilvl=\"%1\"><w:startOverride w:val=\"%2\"/></w:lvlOverride>")
                         .arg(level)
                         .arg(shownNumber);
        list.numId = static_cast<int>(m_nums.size()) + 1;
        m_nums.push_back(QStringLiteral("<w:num w:numId=\"%1\"><w:abstractNumId w:val=\"%2\"/>%3</w:num>")
                             .arg(list.numId)
                             .arg(list.abstractId)
                             .arg(overrides));
    }
    list.counters[static_cast<size_t>(level)] = shownNumber >= 0 ? shownNumber : expected;
    for (int below = level + 1; below < LIST_LEVELS; ++below) {
        list.counters[static_cast<size_t>(below)] = -1;
    }
    return QStringLiteral("<w:numPr><w:ilvl w:val=\"%1\"/><w:numId w:val=\"%2\"/></w:numPr>")
        .arg(level)
        .arg(list.numId);
}

QString DocxWriter::numberingXml() const {
    QString xml = XML_HEAD + QStringLiteral("<w:numbering") + W_NAMESPACES + QStringLiteral(">");
    for (const QString& abstract : m_abstractNums) {
        xml += abstract;
    }
    for (const QString& num : m_nums) {
        xml += num;
    }
    return xml + QStringLiteral("</w:numbering>");
}

// =============================================================================
// The body
// =============================================================================

void DocxWriter::appendParagraph(const QString& properties, const QString& content, bool topLevel) {
    QString pPr = properties;
    if (m_pageBreakPending && topLevel) {
        // The page break of the text before it: the paragraph starts a page (the element after
        // its style, keepNext and keepLines, as the schema orders them)
        pPr.remove(QStringLiteral("<w:pageBreakBefore w:val=\"0\"/>"));
        if (!pPr.contains(QLatin1String("<w:pageBreakBefore/>"))) {
            qsizetype at = 0;
            for (const char* before : {"<w:pStyle", "<w:keepNext", "<w:keepLines"}) {
                if (pPr.indexOf(QLatin1String(before), at) == at) {
                    at = pPr.indexOf(QLatin1String("/>"), at) + 2;
                }
            }
            pPr.insert(at, QStringLiteral("<w:pageBreakBefore/>"));
        }
        m_pageBreakPending = false;
    }
    m_body += QStringLiteral("<w:p><w:pPr>") + pPr;
    if (topLevel) {
        m_lastParagraphProperties = m_body.size();
        m_lastIsParagraph = true;
    }
    m_body += QStringLiteral("</w:pPr>") + content + QStringLiteral("</w:p>");
}

QString DocxWriter::hyperlinkStart(const QString& address, Part part) {
    QHash<QString, QString>& links = m_linkRels[static_cast<int>(part)];
    QString id = links.value(address);
    if (id.isEmpty()) {
        id = relationshipsOf(part).add(REL_HYPERLINK, address, true);
        links.insert(address, id);
    }
    return QStringLiteral("<w:hyperlink") + attribute("r:id", id) + QStringLiteral(" w:history=\"1\">");
}

QString DocxWriter::drawing(const ImageProperties& image, double lineWidth, Part part) {
    if (m_book.imagesFolder.isEmpty()) {
        ++m_missingImages;
        return QString();
    }
    const ExportPicture picture = exportPicture(m_images, image, lineWidth, m_textHeight);
    if (picture.missing) {
        ++m_missingImages;
        return QString();
    }
    const QByteArray hash = QCryptographicHash::hash(picture.data, QCryptographicHash::Sha1);
    QString media = m_mediaByHash.value(hash);
    if (media.isEmpty()) {
        media = QStringLiteral("media/image%1.%2").arg(m_media.size() + 1).arg(picture.extension);
        m_media.emplace_back(QStringLiteral("word/") + media, picture.data);
        m_mediaByHash.insert(hash, media);
        m_extensions.insert(picture.extension);
    }
    QHash<QString, QString>& rels = m_imageRels[static_cast<int>(part)];
    QString relationship = rels.value(media);
    if (relationship.isEmpty()) {
        relationship = relationshipsOf(part).add(REL_IMAGE, media);
        rels.insert(media, relationship);
    }
    ++m_images_written;

    const int id = m_nextDrawing++;
    const QString cx = number(emu(picture.points.width()));
    const QString cy = number(emu(picture.points.height()));
    QString description = image.alt;
    if (!image.description.isEmpty()) {
        description += (description.isEmpty() ? QString() : QStringLiteral("\n")) + image.description;
    }
    const QString name = image.name.isEmpty() ? QStringLiteral("Picture %1").arg(id) : image.name;
    QString docPr = QStringLiteral("<wp:docPr") + attribute("id", number(id)) + attribute("name", name);
    if (!description.isEmpty() && !image.decorative) {
        docPr += attribute("descr", description);
    }
    if (image.decorative) {
        docPr += QStringLiteral(
            "><a:extLst><a:ext uri=\"{C183D7F6-B498-43B3-948B-1728B52AA6E4}\">"
            "<adec:decorative xmlns:adec=\"http://schemas.microsoft.com/office/drawing/2017/decorative\" val=\"1\"/>"
            "</a:ext></a:extLst></wp:docPr>");
    } else {
        docPr += QStringLiteral("/>");
    }
    return QStringLiteral("<w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
                          "<wp:extent cx=\"%1\" cy=\"%2\"/><wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/>")
               .arg(cx, cy) +
           docPr +
           QStringLiteral("<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
                          "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
                          "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"%1\"")
               .arg(id) +
           attribute("name", QFileInfo(image.source).fileName()) +
           QStringLiteral("/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip") + attribute("r:embed", relationship) +
           QStringLiteral("/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/>"
                          "<a:ext cx=\"%1\" cy=\"%2\"/></a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom>"
                          "</pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>")
               .arg(cx, cy);
}

QString DocxWriter::noteXml(const Footnote& note, int id, bool endnote) {
    const QString element = endnote ? QStringLiteral("w:endnote") : QStringLiteral("w:footnote");
    const QString style = endnote ? QStringLiteral("EndnoteText") : QStringLiteral("FootnoteText");
    const QString reference = endnote ? QStringLiteral("EndnoteReference") : QStringLiteral("FootnoteReference");
    const QString mark = endnote ? QStringLiteral("<w:endnoteRef/>") : QStringLiteral("<w:footnoteRef/>");
    const Part part = endnote ? Part::Endnotes : Part::Footnotes;
    QString xml = QStringLiteral("<") + element + QStringLiteral(" w:id=\"%1\">").arg(id);
    bool first = true;
    for (const NoteParagraph& paragraph : noteParagraphs(note.content)) {
        QString content;
        if (first) {
            content += QStringLiteral("<w:r><w:rPr>") + valueElement("rStyle", reference) +
                       QStringLiteral("</w:rPr>") + mark + QStringLiteral("</w:r><w:r><w:t xml:space=\"preserve\"> </w:t></w:r>");
            first = false;
        }
        // The text in the formats of its runs; the rest plain
        std::vector<FormatRun> runs = paragraph.formats;
        std::sort(runs.begin(), runs.end(), [](const FormatRun& a, const FormatRun& b) { return a.start < b.start; });
        size_t at = 0;
        const auto write = [&](size_t from, size_t to, const QTextCharFormat& format) {
            if (to <= from) {
                return;
            }
            const QString text = paragraph.text.mid(static_cast<qsizetype>(from), static_cast<qsizetype>(to - from));
            const QString address = linkAddressOf(format);
            const bool link = isOpenableLinkAddress(address);
            if (link) {
                content += hyperlinkStart(address, part);
            }
            content += textRun(text, runProperties(format, link ? QStringLiteral("Hyperlink") : QString()));
            if (link) {
                content += QStringLiteral("</w:hyperlink>");
            }
        };
        const size_t length = static_cast<size_t>(paragraph.text.size());
        for (const FormatRun& run : runs) {
            const size_t start = std::min(run.start, length);
            const size_t end = std::min(run.end, length);
            if (start < at) {
                continue;
            }
            write(at, start, QTextCharFormat());
            write(start, end, run.format);
            at = end;
        }
        write(at, length, QTextCharFormat());
        xml += QStringLiteral("<w:p><w:pPr>") + valueElement("pStyle", style) + QStringLiteral("</w:pPr>") + content +
               QStringLiteral("</w:p>");
    }
    return xml + QStringLiteral("</") + element + QStringLiteral(">");
}

QString DocxWriter::commentEvents(const ExportText& text, int upTo, Part part) {
    if (!m_book.annotationsAsComments || part != Part::Document) {
        return QString();
    }
    // The comments that start by @p upTo, then the ends of those that end by it (an annotation
    // on a place: both at once)
    QString markup;
    for (const AnnotationPlace& place : text.annotations()) {
        if (place.start <= upTo && !m_commentIds.contains(place.annotation.id)) {
            const int id = static_cast<int>(m_comments.size());
            m_commentIds.insert(place.annotation.id, id);
            m_comments.push_back({id, place.annotation});
            markup += QStringLiteral("<w:commentRangeStart w:id=\"%1\"/>").arg(id);
        }
    }
    for (const AnnotationPlace& place : text.annotations()) {
        const QString& key = place.annotation.id;
        if (place.end <= upTo && m_commentIds.contains(key) && !m_commentsEnded.contains(key)) {
            m_commentsEnded.insert(key);
            markup += QStringLiteral("<w:commentRangeEnd w:id=\"%1\"/><w:r><w:rPr><w:rStyle w:val=\"CommentReference\"/>"
                                     "</w:rPr><w:commentReference w:id=\"%1\"/></w:r>")
                          .arg(m_commentIds.value(key));
        }
    }
    return markup;
}

QString DocxWriter::paragraphContent(const ExportText& text, const QTextBlock& block,
                                     double lineWidth, Part part) {
    QString content;
    QString openLink;  // the address of the link open; empty: none
    const auto closeLink = [&]() {
        if (!openLink.isEmpty()) {
            content += QStringLiteral("</w:hyperlink>");
            openLink.clear();
        }
    };
    for (const Piece& piece : piecesOf(block, text)) {
        const QString events = commentEvents(text, piece.position, part);
        if (!events.isEmpty()) {
            closeLink();
            content += events;
        }
        // A link of the text: Word's link to the address
        if (piece.link != openLink) {
            closeLink();
            if (!piece.link.isEmpty()) {
                content += hyperlinkStart(piece.link, part);
                openLink = piece.link;
            }
        }
        const QString style = openLink.isEmpty() ? QString() : QStringLiteral("Hyperlink");
        switch (piece.kind) {
            case Piece::Kind::Text:
                content += textRun(piece.text, runProperties(piece.format, style));
                break;
            case Piece::Kind::Image:
                content += drawing(piece.image, lineWidth, part);
                break;
            case Piece::Kind::Field: {
                QTextCharFormat format = piece.format;
                if (piece.captionNumber && !piece.captionNumber->id.isEmpty() &&
                    m_captionIds.contains(piece.captionNumber->id)) {
                    const int id = m_nextBookmark++;
                    content += QStringLiteral("<w:bookmarkStart w:id=\"%1\"").arg(id) +
                               attribute("w:name", bookmarkName(QStringLiteral("_KalC_"), piece.captionNumber->id)) +
                               QStringLiteral("/>") + textRun(piece.text, runProperties(format, style)) +
                               QStringLiteral("<w:bookmarkEnd w:id=\"%1\"/>").arg(id);
                } else if (piece.reference && piece.reference->link && openLink.isEmpty() &&
                           m_captionIds.contains(piece.reference->target)) {
                    content += QStringLiteral("<w:hyperlink") +
                               attribute("w:anchor", bookmarkName(QStringLiteral("_KalC_"), piece.reference->target)) +
                               QStringLiteral(">") + textRun(piece.text, runProperties(format)) +
                               QStringLiteral("</w:hyperlink>");
                } else {
                    content += textRun(piece.text, runProperties(format, style));
                }
                break;
            }
            case Piece::Kind::Note: {
                if (part != Part::Document) {
                    break;  // a note has no notes of its own
                }
                const bool endnote = piece.note.kind == NoteKind::Endnote;
                const int id = m_nextNote++;
                (endnote ? m_endnotes : m_footnotes) << noteXml(piece.note, id, endnote);
                (endnote ? m_endnotesWritten : m_footnotesWritten) += 1;
                content += QStringLiteral("<w:r><w:rPr>") +
                           valueElement("rStyle", endnote ? QStringLiteral("EndnoteReference")
                                                          : QStringLiteral("FootnoteReference")) +
                           QStringLiteral("</w:rPr><w:%1 w:id=\"%2\"/></w:r>")
                               .arg(endnote ? QStringLiteral("endnoteReference") : QStringLiteral("footnoteReference"))
                               .arg(id);
                break;
            }
        }
    }
    closeLink();
    content += commentEvents(text, block.position() + std::max(0, block.length() - 1), part);
    return content;
}

void DocxWriter::writeParagraph(const ExportText& text, size_t item, const QTextBlock& block,
                                double lineWidth, bool topLevel) {
    const QTextBlockFormat format = block.blockFormat();
    const QString styleId = shownStyleOf(m_book.styles, block);
    ParagraphProperties properties;
    properties.style = wordStyleOf(styleId);
    if (const std::optional<ListItem> listItem = listItemOf(format)) {
        // The indents of its level, also on the paragraph: a style's indent does not move it
        const ListLevel& level = listItem->levelFormat();
        properties.leftIndent = level.textIndent;
        if (listItem->numbered) {
            properties.numbering = listNumbering(item, *listItem, text.listNumber(block));
            properties.firstLineIndent = -std::max(0.0, level.textIndent - level.markerIndent);
        } else {
            properties.firstLineIndent = 0.0;  // an item without a number: at the text of its level
        }
        ParagraphFormat own = paragraphFormatOf(format);
        own.leftIndent.reset();  // the list's indents (as the editor shows the items)
        own.firstLineIndent.reset();
        properties.merge(own);
    } else {
        properties.merge(paragraphFormatOf(format));
    }
    if (const Qt::Alignment alignment = ownAlignment(format); alignment) {
        properties.alignment = alignmentValue(alignment);
    }
    // A heading of the text: a bookmark the table of contents goes to
    QString content;
    const QString bookmark = m_headingBookmarks.value(qMakePair(static_cast<int>(item), block.blockNumber()));
    if (!bookmark.isEmpty()) {
        const int id = m_nextBookmark++;
        content += QStringLiteral("<w:bookmarkStart w:id=\"%1\"").arg(id) + attribute("w:name", bookmark) +
                   QStringLiteral("/>") + paragraphContent(text, block, lineWidth, Part::Document) +
                   QStringLiteral("<w:bookmarkEnd w:id=\"%1\"/>").arg(id);
    } else {
        content = paragraphContent(text, block, lineWidth, Part::Document);
    }
    appendParagraph(properties.xml(), content, topLevel);
}

void DocxWriter::writeObject(const QTextBlock& block, TextObject object, const ExportText& text,
                             bool topLevel) {
    const QString noIndent = QStringLiteral("<w:ind w:left=\"0\" w:right=\"0\" w:firstLine=\"0\"/>");
    switch (object) {
        case TextObject::PageBreak:
            if (topLevel) {
                m_pageBreakPending = true;
            } else {
                appendParagraph(QString(), QStringLiteral("<w:r><w:br w:type=\"page\"/></w:r>"), false);
            }
            return;
        case TextObject::HorizontalRule:
            appendParagraph(QStringLiteral("<w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:space=\"1\" w:color=\"auto\"/></w:pBdr>"
                                           "<w:spacing w:before=\"120\" w:after=\"120\"/>") +
                                noIndent,
                            QString(), topLevel);
            return;
        case TextObject::SceneBreak: {
            if (m_book.sceneBreak == SceneBreakLook::Line) {
                const double side = m_textWidth * (1.0 - SCENE_BREAK_LINE_SHARE) / 2.0;
                appendParagraph(QStringLiteral("<w:keepNext/><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:space=\"1\" w:color=\"auto\"/></w:pBdr>"
                                               "<w:spacing w:before=\"120\" w:after=\"240\"/><w:ind w:left=\"%1\" w:right=\"%1\" w:firstLine=\"0\"/>")
                                    .arg(twips(side)),
                                QString(), topLevel);
                return;
            }
            const QString symbol = sceneBreakText(m_book.sceneBreak);
            appendParagraph(noIndent + QStringLiteral("<w:jc w:val=\"center\"/>"),
                            symbol.isEmpty() ? QString() : textRun(symbol, QString()), topLevel);
            return;
        }
        case TextObject::FigureList: {
            for (const FigureListRow& row : text.captions().figureList()) {
                QString content;
                if (m_captionIds.contains(row.id)) {
                    content = QStringLiteral("<w:hyperlink") +
                              attribute("w:anchor", bookmarkName(QStringLiteral("_KalC_"), row.id)) +
                              QStringLiteral(" w:history=\"1\">") + textRun(row.text, QString()) +
                              QStringLiteral("</w:hyperlink>");
                } else {
                    content = textRun(row.text, QString());
                }
                appendParagraph(valueElement("pStyle", QStringLiteral("TableofFigures")), content, topLevel);
            }
            return;
        }
        case TextObject::None:
            break;
    }
    Q_UNUSED(block);
}

void DocxWriter::writeTable(const ExportText& text, size_t item, QTextTable& table, double lineWidth) {
    if (m_pageBreakPending) {
        // A table cannot start a page itself: a paragraph with the break before it
        appendParagraph(QString(), QStringLiteral("<w:r><w:br w:type=\"page\"/></w:r>"), true);
        m_pageBreakPending = false;
    }
    const TableShape shape = tableShapeOf(table);
    const double width = tableWidthPoints(shape, lineWidth);
    const std::vector<double> columns = columnWidthsPoints(shape, width);
    const TableBorders& borders = shape.properties.borders;

    QString xml = QStringLiteral("<w:tbl><w:tblPr><w:tblW") + attribute("w:w", number(twips(width))) +
                  QStringLiteral(" w:type=\"dxa\"/>");
    switch (shape.properties.alignment) {
        case TableAlignment::Center: xml += valueElement("jc", QStringLiteral("center")); break;
        case TableAlignment::Right: xml += valueElement("jc", QStringLiteral("right")); break;
        case TableAlignment::Left: break;
    }
    if (shape.properties.alignment == TableAlignment::Left && shape.properties.indent != 0.0) {
        xml += QStringLiteral("<w:tblInd") + attribute("w:w", number(twips(shape.properties.indent))) +
               QStringLiteral(" w:type=\"dxa\"/>");
    }
    const QString line = [&borders]() {
        switch (borders.style) {
            case BorderStyle::Dashed: return QStringLiteral("dashed");
            case BorderStyle::Dotted: return QStringLiteral("dotted");
            case BorderStyle::Double: return QStringLiteral("double");
            case BorderStyle::Solid: break;
        }
        return QStringLiteral("single");
    }();
    const QString size = number(std::clamp(static_cast<int>(std::lround(borders.width * 8.0)), 2, 96));
    const QString color = borders.color.isValid() ? colorValue(borders.color) : QStringLiteral("auto");
    const auto border = [&](const char* name, int flag) {
        if (!(borders.lines & flag)) {
            return QStringLiteral("<w:") + QLatin1String(name) + QStringLiteral(" w:val=\"nil\"/>");
        }
        return QStringLiteral("<w:") + QLatin1String(name) + attribute("w:val", line) + attribute("w:sz", size) +
               QStringLiteral(" w:space=\"0\"") + attribute("w:color", color) + QStringLiteral("/>");
    };
    xml += QStringLiteral("<w:tblBorders>") + border("top", BORDER_TOP) + border("left", BORDER_LEFT) +
           border("bottom", BORDER_BOTTOM) + border("right", BORDER_RIGHT) + border("insideH", BORDER_ROWS) +
           border("insideV", BORDER_COLUMNS) + QStringLiteral("</w:tblBorders>");
    xml += QStringLiteral("<w:tblLayout w:type=\"fixed\"/><w:tblLook w:val=\"0000\" w:firstRow=\"0\" w:lastRow=\"0\""
                          " w:firstColumn=\"0\" w:lastColumn=\"0\" w:noHBand=\"1\" w:noVBand=\"1\"/></w:tblPr><w:tblGrid>");
    for (const double column : columns) {
        xml += QStringLiteral("<w:gridCol") + attribute("w:w", number(twips(column))) + QStringLiteral("/>");
    }
    xml += QStringLiteral("</w:tblGrid>");
    m_body += xml;

    for (int row = 0; row < shape.rows; ++row) {
        const TableRowProperties rowProperties = rowPropertiesOf(table, row);
        QString trPr;
        if (!rowProperties.allowSplit) {
            trPr += QStringLiteral("<w:cantSplit/>");
        }
        if (rowProperties.minHeight > 0.0) {
            trPr += QStringLiteral("<w:trHeight") + attribute("w:val", number(twips(rowProperties.minHeight))) +
                    QStringLiteral(" w:hRule=\"atLeast\"/>");
        }
        if (row < shape.headerRows) {
            trPr += QStringLiteral("<w:tblHeader/>");
        }
        m_body += QStringLiteral("<w:tr>") + (trPr.isEmpty() ? QString() : QStringLiteral("<w:trPr>") + trPr + QStringLiteral("</w:trPr>"));
        for (int column = 0; column < shape.columns;) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (!cell.isValid()) {
                ++column;
                continue;
            }
            const int span = std::max(1, cell.columnSpan());
            double cellWidth = 0.0;
            for (int c = cell.column(); c < cell.column() + span && c < static_cast<int>(columns.size()); ++c) {
                cellWidth += columns[static_cast<size_t>(c)];
            }
            const TableCellProperties cellProperties = cellPropertiesOf(cell);
            QString tcPr = QStringLiteral("<w:tcW") + attribute("w:w", number(twips(cellWidth))) +
                           QStringLiteral(" w:type=\"dxa\"/>");
            if (span > 1) {
                tcPr += valueElement("gridSpan", number(span));
            }
            const bool covered = cell.row() != row;
            if (cell.rowSpan() > 1) {
                tcPr += covered ? QStringLiteral("<w:vMerge/>") : QStringLiteral("<w:vMerge w:val=\"restart\"/>");
            }
            if (cellProperties.background.isValid() && cellProperties.background.alpha() > 0) {
                tcPr += QStringLiteral("<w:shd w:val=\"clear\" w:color=\"auto\"") +
                        attribute("w:fill", colorValue(cellProperties.background)) + QStringLiteral("/>");
            }
            if (cellProperties.alignment == CellAlignment::Middle) {
                tcPr += valueElement("vAlign", QStringLiteral("center"));
            } else if (cellProperties.alignment == CellAlignment::Bottom) {
                tcPr += valueElement("vAlign", QStringLiteral("bottom"));
            }
            m_body += QStringLiteral("<w:tc><w:tcPr>") + tcPr + QStringLiteral("</w:tcPr>");
            if (covered) {
                m_body += QStringLiteral("<w:p/>");
            } else {
                const QTextDocument& document = text.document();
                const int first = document.findBlock(cell.firstPosition()).blockNumber();
                const int last = document.findBlock(cell.lastPosition()).blockNumber();
                const double inner = std::max(1.0, cellWidth - 2.0 * 108.0 / 20.0);  // the cell's margins
                writeBlocks(text, item, first, last, inner, false);
            }
            m_body += QStringLiteral("</w:tc>");
            column = cell.column() + span;
        }
        m_body += QStringLiteral("</w:tr>");
    }
    m_body += QStringLiteral("</w:tbl>");
    m_lastIsParagraph = false;
}

void DocxWriter::writeBlocks(const ExportText& text, size_t item, int fromBlock, int toBlock,
                             double lineWidth, bool topLevel) {
    const QTextDocument& document = text.document();
    for (QTextBlock block = document.findBlockByNumber(fromBlock);
         block.isValid() && block.blockNumber() <= toBlock; block = block.next()) {
        if (topLevel) {
            if (QTextTable* table = tableOf(block)) {
                writeTable(text, item, *table, lineWidth);
                block = document.findBlock(table->lastPosition());  // its last paragraph
                continue;
            }
        }
        if (const TextObject object = blockObject(block); object != TextObject::None) {
            writeObject(block, object, text, topLevel);
            continue;
        }
        writeParagraph(text, item, block, lineWidth, topLevel);
    }
}

void DocxWriter::collectHeadings() {
    // The headings of the file, in its order: the titles and the headings of the texts
    for (size_t index = 0; index < m_book.items.size(); ++index) {
        const ExportItem& item = m_book.items[index];
        const ExportText& text = *m_texts[index];
        if (item.kind == ExportItemKind::Contents) {
            continue;
        }
        if (m_withTitle[index]) {
            const QString bookmark = QStringLiteral("_KalH%1").arg(m_headings.size() + 1);
            const int level = std::max(1, headingLevelOfStyle(m_book.styles, titleStyleOf(item, m_book.styles)));
            m_headings.push_back({level, item.title.simplified(), bookmark});
            m_headingBookmarks.insert(qMakePair(static_cast<int>(index), -1), bookmark);
        }
        for (QTextBlock block = text.document().begin(); block.isValid(); block = block.next()) {
            if (tableOf(block) != nullptr || blockObject(block) != TextObject::None) {
                continue;
            }
            const int level = headingLevelOfStyle(m_book.styles, shownStyleOf(m_book.styles, block));
            const QString heading = level > 0 ? plainTextOf(block, text) : QString();
            if (heading.isEmpty()) {
                continue;
            }
            const QString bookmark = QStringLiteral("_KalH%1").arg(m_headings.size() + 1);
            m_headings.push_back({level, heading, bookmark});
            m_headingBookmarks.insert(qMakePair(static_cast<int>(index), block.blockNumber()), bookmark);
        }
        for (const BookCaption& caption : text.captions().own()) {
            m_captionIds.insert(caption.id);
        }
    }
}

void DocxWriter::writeContents(const ExportItem& item) {
    if (!item.title.trimmed().isEmpty()) {
        appendParagraph(valueElement("pStyle", QStringLiteral("TOCHeading")), textRun(item.title.simplified(), QString()),
                        true);
    }
    // Word's field of the table of contents, filled with the headings until Word updates it
    // (it asks to when the file opens: settings.xml updateFields)
    const QString begin = QStringLiteral("<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
                                         "<w:r><w:instrText xml:space=\"preserve\"> TOC \\o \"1-3\" \\h \\z \\u </w:instrText></w:r>"
                                         "<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r>");
    const QString end = QStringLiteral("<w:r><w:fldChar w:fldCharType=\"end\"/></w:r>");
    if (m_headings.empty()) {
        appendParagraph(valueElement("pStyle", QStringLiteral("TOC1")), begin + end, true);
        return;
    }
    for (size_t i = 0; i < m_headings.size(); ++i) {
        const Heading& heading = m_headings[i];
        QString content = i == 0 ? begin : QString();
        content += QStringLiteral("<w:hyperlink") + attribute("w:anchor", heading.bookmark) +
                   QStringLiteral(" w:history=\"1\">") + textRun(heading.text, QString()) +
                   QStringLiteral("</w:hyperlink>");
        if (i + 1 == m_headings.size()) {
            content += end;
        }
        appendParagraph(valueElement("pStyle", QStringLiteral("TOC%1").arg(std::clamp(heading.level, 1, HEADING_LEVELS))),
                        content, true);
    }
}

void DocxWriter::writeItem(size_t index) {
    const ExportItem& item = m_book.items[index];
    const ExportText& text = *m_texts[index];
    m_footnotesBefore.push_back(m_footnotesWritten);
    m_endnotesBefore.push_back(m_endnotesWritten);
    if (item.kind == ExportItemKind::Contents) {
        writeContents(item);
    } else {
        if (m_withTitle[index]) {
            ParagraphProperties title;
            title.style = wordStyleOf(titleStyleOf(item, m_book.styles));
            const QString bookmark = m_headingBookmarks.value(qMakePair(static_cast<int>(index), -1));
            const int id = m_nextBookmark++;
            appendParagraph(title.xml(),
                            QStringLiteral("<w:bookmarkStart w:id=\"%1\"").arg(id) + attribute("w:name", bookmark) +
                                QStringLiteral("/>") + textRun(item.title.simplified(), QString()) +
                                QStringLiteral("<w:bookmarkEnd w:id=\"%1\"/>").arg(id),
                            true);
        }
        if (item.kind == ExportItemKind::Text) {
            writeBlocks(text, index, 0, text.document().blockCount() - 1, m_textWidth, true);
        }
    }
    if (!m_lastIsParagraph || m_lastParagraphProperties < 0) {
        appendParagraph(QString(), QString(), true);  // a section ends with a paragraph
    }
    m_pageBreakPending = false;  // a page break at the end of a text: the next item's section
    m_footnotesIn.push_back(m_footnotesWritten - m_footnotesBefore.back());
    m_endnotesIn.push_back(m_endnotesWritten - m_endnotesBefore.back());
    endSection(index);
}

QString DocxWriter::sectionProperties(size_t index) const {
    static const ExportItem noItem;
    const ExportItem& item = index < m_book.items.size() ? m_book.items[index] : noItem;
    const int footnotesBefore = index < m_footnotesBefore.size() ? m_footnotesBefore[index] : 0;
    const int endnotesBefore = index < m_endnotesBefore.size() ? m_endnotesBefore[index] : 0;
    const int footnotesIn = index < m_footnotesIn.size() ? m_footnotesIn[index] : 0;
    const int endnotesIn = index < m_endnotesIn.size() ? m_endnotesIn[index] : 0;
    const ExportPage& page = m_book.page;
    QString xml = QStringLiteral("<w:sectPr>");
    if (!m_footerRelationship.isEmpty()) {
        xml += QStringLiteral("<w:footerReference w:type=\"default\"") + attribute("r:id", m_footerRelationship) +
               QStringLiteral("/>");
    }
    // The notes numbered as the book numbers them: on from those before in the file, or from
    // the item's own first number (a section without notes of a kind goes on)
    const auto notes = [&](const char* element, int before, int written, int in, NoteNumberFormat format) {
        QString properties = QStringLiteral("<w:") + QLatin1String(element) + QStringLiteral(">") +
                             valueElement("numFmt", noteFormatValue(format));
        if (before == written || in == 0) {
            properties += valueElement("numRestart", QStringLiteral("continuous"));
        } else {
            properties += valueElement("numStart", number(before + 1)) +
                          valueElement("numRestart", QStringLiteral("eachSect"));
        }
        return properties + QStringLiteral("</w:") + QLatin1String(element) + QStringLiteral(">");
    };
    xml += notes("footnotePr", item.footnotes.footnotesBefore, footnotesBefore, footnotesIn,
                 item.footnotes.footnoteFormat);
    xml += notes("endnotePr", item.footnotes.endnotesBefore, endnotesBefore, endnotesIn, item.footnotes.endnoteFormat);
    QString type = QStringLiteral("continuous");
    if (index > 0 && m_book.textsOnNewPage) {
        type = !page.mirrored ? QStringLiteral("nextPage")
               : item.firstPageLeft ? QStringLiteral("evenPage")
                                    : QStringLiteral("oddPage");
    }
    xml += valueElement("type", type);
    xml += QStringLiteral("<w:pgSz") + attribute("w:w", number(twips(page.width * POINTS_PER_MILLIMETRE))) +
           attribute("w:h", number(twips(page.height * POINTS_PER_MILLIMETRE))) + QStringLiteral("/>");
    const double left = page.mirrored ? page.inner : page.left;
    const double right = page.mirrored ? page.outer : page.right;
    xml += QStringLiteral("<w:pgMar") + attribute("w:top", number(twips(page.top * POINTS_PER_MILLIMETRE))) +
           attribute("w:right", number(twips(right * POINTS_PER_MILLIMETRE))) +
           attribute("w:bottom", number(twips(page.bottom * POINTS_PER_MILLIMETRE))) +
           attribute("w:left", number(twips(left * POINTS_PER_MILLIMETRE))) +
           attribute("w:header", number(twips(std::min(page.top, 12.5) * POINTS_PER_MILLIMETRE))) +
           attribute("w:footer", number(twips(std::min(page.bottom, 12.5) * POINTS_PER_MILLIMETRE))) +
           QStringLiteral(" w:gutter=\"0\"/>");
    xml += QStringLiteral("<w:cols w:space=\"708\"/>");
    return xml + QStringLiteral("</w:sectPr>");
}

void DocxWriter::endSection(size_t index) {
    if (index + 1 == m_book.items.size()) {
        return;  // the last section's properties end the body
    }
    m_body.insert(m_lastParagraphProperties, sectionProperties(index));
    m_lastIsParagraph = false;
    m_lastParagraphProperties = -1;
}

// =============================================================================
// The parts
// =============================================================================

QString DocxWriter::settingsXml() const {
    QString xml = XML_HEAD + QStringLiteral("<w:settings") + W_NAMESPACES + QStringLiteral(">");
    if (m_book.page.mirrored) {
        xml += QStringLiteral("<w:mirrorMargins/>");
    }
    xml += QStringLiteral("<w:defaultTabStop w:val=\"708\"/>");
    bool contents = false;
    for (const ExportItem& item : m_book.items) {
        contents = contents || item.kind == ExportItemKind::Contents;
    }
    if (contents) {
        xml += QStringLiteral("<w:updateFields w:val=\"true\"/>");
    }
    const NoteNumberFormat footnoteFormat =
        m_book.items.empty() ? NoteNumberFormat::Decimal : m_book.items.front().footnotes.footnoteFormat;
    const NoteNumberFormat endnoteFormat =
        m_book.items.empty() ? NoteNumberFormat::LowerRoman : m_book.items.front().footnotes.endnoteFormat;
    xml += QStringLiteral("<w:footnotePr>") + valueElement("numFmt", noteFormatValue(footnoteFormat)) +
           QStringLiteral("<w:footnote w:id=\"-1\"/><w:footnote w:id=\"0\"/></w:footnotePr>");
    xml += QStringLiteral("<w:endnotePr>") +
           valueElement("pos", m_book.endnotesAtBookEnd ? QStringLiteral("docEnd") : QStringLiteral("sectEnd")) +
           valueElement("numFmt", noteFormatValue(endnoteFormat)) +
           QStringLiteral("<w:endnote w:id=\"-1\"/><w:endnote w:id=\"0\"/></w:endnotePr>");
    // A line ended by a line break is not justified, as in the editor
    xml += QStringLiteral("<w:compat><w:doNotExpandShiftReturn/><w:compatSetting w:name=\"compatibilityMode\" "
                          "w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"15\"/></w:compat>");
    if (const QString language = languageTag(m_book.language); !language.isEmpty()) {
        xml += QStringLiteral("<w:themeFontLang") + attribute("w:val", language) + QStringLiteral("/>");
    }
    return xml + QStringLiteral("</w:settings>");
}

QString DocxWriter::footerXml() const {
    return XML_HEAD + QStringLiteral("<w:ftr") + W_NAMESPACES +
           QStringLiteral("><w:p><w:pPr><w:pStyle w:val=\"Footer\"/><w:jc w:val=\"center\"/></w:pPr>"
                          "<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
                          "<w:r><w:instrText xml:space=\"preserve\"> PAGE </w:instrText></w:r>"
                          "<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r><w:r><w:t>1</w:t></w:r>"
                          "<w:r><w:fldChar w:fldCharType=\"end\"/></w:r></w:p></w:ftr>");
}

QString DocxWriter::notesXml(bool endnotes) const {
    const QString root = endnotes ? QStringLiteral("w:endnotes") : QStringLiteral("w:footnotes");
    const QString element = endnotes ? QStringLiteral("w:endnote") : QStringLiteral("w:footnote");
    QString xml = XML_HEAD + QStringLiteral("<") + root + W_NAMESPACES + QStringLiteral(">");
    xml += QStringLiteral("<%1 w:type=\"separator\" w:id=\"-1\"><w:p><w:pPr><w:spacing w:after=\"0\" w:line=\"240\" "
                          "w:lineRule=\"auto\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:r><w:separator/></w:r></w:p></%1>")
               .arg(element);
    xml += QStringLiteral("<%1 w:type=\"continuationSeparator\" w:id=\"0\"><w:p><w:pPr><w:spacing w:after=\"0\" "
                          "w:line=\"240\" w:lineRule=\"auto\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:r>"
                          "<w:continuationSeparator/></w:r></w:p></%1>")
               .arg(element);
    for (const QString& note : endnotes ? m_endnotes : m_footnotes) {
        xml += note;
    }
    return xml + QStringLiteral("</") + root + QStringLiteral(">");
}

/// The id of the last paragraph of a comment, for its state in commentsExtended.xml
QString commentParagraphId(int comment) {
    return QStringLiteral("%1").arg(0x10000000 + comment, 8, 16, QLatin1Char('0')).toUpper();
}

QString DocxWriter::commentsXml() const {
    QString xml = XML_HEAD + QStringLiteral("<w:comments") + W_NAMESPACES +
                  QStringLiteral(" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\""
                                 " xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\""
                                 " mc:Ignorable=\"w14\">");
    for (const WordComment& comment : m_comments) {
        const Annotation& annotation = comment.annotation;
        QString initials;
        for (const QString& word : annotation.author.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            initials += word.at(0).toUpper();
        }
        xml += QStringLiteral("<w:comment w:id=\"%1\"").arg(comment.id) +
               attribute("w:author", annotation.author.isEmpty() ? QStringLiteral("Kalahari") : annotation.author);
        if (annotation.created.isValid()) {
            xml += attribute("w:date", annotation.created.toUTC().toString(Qt::ISODate));
        }
        if (!initials.isEmpty()) {
            xml += attribute("w:initials", initials.left(9));
        }
        xml += QStringLiteral(">");
        QString text = annotation.text;
        const QString label = m_book.annotationLabels.value(annotation.kind);
        if (!label.isEmpty()) {
            text = label + QStringLiteral(": ") + text;
        }
        QStringList lines = text.split(QLatin1Char('\n'));
        if (lines.isEmpty()) {
            lines << QString();
        }
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const bool last = i + 1 == lines.size();
            xml += QStringLiteral("<w:p") +
                   (last ? attribute("w14:paraId", commentParagraphId(comment.id)) +
                               QStringLiteral(" w14:textId=\"77777777\"")
                         : QString()) +
                   QStringLiteral("><w:pPr><w:pStyle w:val=\"CommentText\"/></w:pPr>");
            if (i == 0) {
                xml += QStringLiteral("<w:r><w:rPr><w:rStyle w:val=\"CommentReference\"/></w:rPr><w:annotationRef/></w:r>");
            }
            xml += textRun(lines[i], QString()) + QStringLiteral("</w:p>");
        }
        xml += QStringLiteral("</w:comment>");
    }
    return xml + QStringLiteral("</w:comments>");
}

QString DocxWriter::commentsExtendedXml() const {
    QString xml = XML_HEAD +
                  QStringLiteral("<w15:commentsEx xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\""
                                 " xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" mc:Ignorable=\"w15\">");
    for (const WordComment& comment : m_comments) {
        xml += QStringLiteral("<w15:commentEx w15:paraId=\"%1\" w15:done=\"%2\"/>")
                   .arg(commentParagraphId(comment.id), comment.annotation.done ? QStringLiteral("1") : QStringLiteral("0"));
    }
    return xml + QStringLiteral("</w15:commentsEx>");
}

std::vector<std::pair<QString, QByteArray>> DocxWriter::build(ExportResult* result) {
    const size_t count = m_book.items.size();
    const auto report = [this](int percent) {
        if (m_progress) {
            m_progress(std::clamp(percent, 0, 100));
        }
    };

    // The texts read, the titles decided, the headings and captions of the file counted
    for (size_t index = 0; index < count; ++index) {
        const ExportItem& item = m_book.items[index];
        m_texts.push_back(std::make_unique<ExportText>(item));
        const bool titled = m_book.titleHeadings && !item.title.trimmed().isEmpty() &&
                            item.kind != ExportItemKind::Contents &&
                            !(item.kind == ExportItemKind::Text &&
                              startsWithHeading(m_texts.back()->document(), m_book.styles));
        m_withTitle.push_back(titled);
        report(static_cast<int>(10 * (index + 1) / std::max<size_t>(1, count)));
    }
    collectHeadings();

    if (m_book.page.pageNumbers) {
        m_footerRelationship =
            m_documentRels.add(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer"),
                               QStringLiteral("footer1.xml"));
    }
    const QString& footerRelationship = m_footerRelationship;
    for (size_t index = 0; index < count; ++index) {
        writeItem(index);
        report(10 + static_cast<int>(80 * (index + 1) / std::max<size_t>(1, count)));
    }
    if (count == 0) {
        appendParagraph(QString(), QString(), true);  // a document has a paragraph
    }

    // The last section's properties end the body
    const QString lastSection = sectionProperties(count == 0 ? 0 : count - 1);
    const QString& body = m_body;

    // The parts of the package
    const QString styles = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");
    m_documentRels.add(styles + QStringLiteral("styles"), QStringLiteral("styles.xml"));
    m_documentRels.add(styles + QStringLiteral("settings"), QStringLiteral("settings.xml"));
    m_documentRels.add(styles + QStringLiteral("footnotes"), QStringLiteral("footnotes.xml"));
    m_documentRels.add(styles + QStringLiteral("endnotes"), QStringLiteral("endnotes.xml"));
    const bool lists = !m_abstractNums.empty();
    if (lists) {
        m_documentRels.add(styles + QStringLiteral("numbering"), QStringLiteral("numbering.xml"));
    }
    const bool comments = !m_comments.empty();
    if (comments) {
        m_documentRels.add(styles + QStringLiteral("comments"), QStringLiteral("comments.xml"));
        m_documentRels.add(QStringLiteral("http://schemas.microsoft.com/office/2011/relationships/commentsExtended"),
                           QStringLiteral("commentsExtended.xml"));
    }

    const QString document = XML_HEAD + QStringLiteral("<w:document") + W_NAMESPACES + QStringLiteral("><w:body>") +
                             body + lastSection + QStringLiteral("</w:body></w:document>");

    QString types = XML_HEAD +
                    QStringLiteral("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                                   "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                                   "<Default Extension=\"xml\" ContentType=\"application/xml\"/>");
    for (const QString& extension : m_extensions) {
        types += QStringLiteral("<Default") + attribute("Extension", extension) +
                 attribute("ContentType", QStringLiteral("image/") + extension) + QStringLiteral("/>");
    }
    const QString wml = QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.");
    const auto override = [](const QString& part, const QString& type) {
        return QStringLiteral("<Override") + attribute("PartName", part) + attribute("ContentType", type) +
               QStringLiteral("/>");
    };
    types += override(QStringLiteral("/word/document.xml"), wml + QStringLiteral("document.main+xml"));
    types += override(QStringLiteral("/word/styles.xml"), wml + QStringLiteral("styles+xml"));
    types += override(QStringLiteral("/word/settings.xml"), wml + QStringLiteral("settings+xml"));
    types += override(QStringLiteral("/word/footnotes.xml"), wml + QStringLiteral("footnotes+xml"));
    types += override(QStringLiteral("/word/endnotes.xml"), wml + QStringLiteral("endnotes+xml"));
    if (lists) {
        types += override(QStringLiteral("/word/numbering.xml"), wml + QStringLiteral("numbering+xml"));
    }
    if (comments) {
        types += override(QStringLiteral("/word/comments.xml"), wml + QStringLiteral("comments+xml"));
        types += override(QStringLiteral("/word/commentsExtended.xml"), wml + QStringLiteral("commentsExtended+xml"));
    }
    if (!footerRelationship.isEmpty()) {
        types += override(QStringLiteral("/word/footer1.xml"), wml + QStringLiteral("footer+xml"));
    }
    types += override(QStringLiteral("/docProps/core.xml"),
                      QStringLiteral("application/vnd.openxmlformats-package.core-properties+xml"));
    types += override(QStringLiteral("/docProps/app.xml"),
                      QStringLiteral("application/vnd.openxmlformats-officedocument.extended-properties+xml"));
    types += QStringLiteral("</Types>");

    Relationships package;
    package.add(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"),
                QStringLiteral("word/document.xml"));
    package.add(QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"),
                QStringLiteral("docProps/core.xml"));
    package.add(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties"),
                QStringLiteral("docProps/app.xml"));

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString core = XML_HEAD +
                   QStringLiteral("<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\""
                                  " xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\""
                                  " xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
    if (!m_book.title.isEmpty()) {
        core += QStringLiteral("<dc:title>") + xmlEscaped(m_book.title) + QStringLiteral("</dc:title>");
    }
    if (!m_book.author.isEmpty()) {
        core += QStringLiteral("<dc:creator>") + xmlEscaped(m_book.author) + QStringLiteral("</dc:creator>");
    }
    if (const QString language = languageTag(m_book.language); !language.isEmpty()) {
        core += QStringLiteral("<dc:language>") + xmlEscaped(language) + QStringLiteral("</dc:language>");
    }
    core += QStringLiteral("<dcterms:created xsi:type=\"dcterms:W3CDTF\">") + now +
            QStringLiteral("</dcterms:created><dcterms:modified xsi:type=\"dcterms:W3CDTF\">") + now +
            QStringLiteral("</dcterms:modified></cp:coreProperties>");
    const QString app = XML_HEAD +
                        QStringLiteral("<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">"
                                       "<Application>Kalahari</Application></Properties>");

    std::vector<std::pair<QString, QByteArray>> files;
    files.emplace_back(QStringLiteral("[Content_Types].xml"), types.toUtf8());
    files.emplace_back(QStringLiteral("_rels/.rels"), package.xml().toUtf8());
    files.emplace_back(QStringLiteral("docProps/core.xml"), core.toUtf8());
    files.emplace_back(QStringLiteral("docProps/app.xml"), app.toUtf8());
    files.emplace_back(QStringLiteral("word/document.xml"), document.toUtf8());
    files.emplace_back(QStringLiteral("word/_rels/document.xml.rels"), m_documentRels.xml().toUtf8());
    files.emplace_back(QStringLiteral("word/styles.xml"), stylesXml().toUtf8());
    files.emplace_back(QStringLiteral("word/settings.xml"), settingsXml().toUtf8());
    files.emplace_back(QStringLiteral("word/footnotes.xml"), notesXml(false).toUtf8());
    files.emplace_back(QStringLiteral("word/endnotes.xml"), notesXml(true).toUtf8());
    if (!m_footnoteRels.entries.isEmpty()) {
        files.emplace_back(QStringLiteral("word/_rels/footnotes.xml.rels"), m_footnoteRels.xml().toUtf8());
    }
    if (!m_endnoteRels.entries.isEmpty()) {
        files.emplace_back(QStringLiteral("word/_rels/endnotes.xml.rels"), m_endnoteRels.xml().toUtf8());
    }
    if (lists) {
        files.emplace_back(QStringLiteral("word/numbering.xml"), numberingXml().toUtf8());
    }
    if (comments) {
        files.emplace_back(QStringLiteral("word/comments.xml"), commentsXml().toUtf8());
        files.emplace_back(QStringLiteral("word/commentsExtended.xml"), commentsExtendedXml().toUtf8());
        if (!m_commentRels.entries.isEmpty()) {
            files.emplace_back(QStringLiteral("word/_rels/comments.xml.rels"), m_commentRels.xml().toUtf8());
        }
    }
    if (!footerRelationship.isEmpty()) {
        files.emplace_back(QStringLiteral("word/footer1.xml"), footerXml().toUtf8());
    }
    for (auto& media : m_media) {
        files.push_back(std::move(media));
    }
    if (result) {
        result->images = m_images_written;
        result->missingImages = m_missingImages;
    }
    return files;
}

/// The files packed as a ZIP archive in memory; empty, with what went wrong in @p problem,
/// when they cannot be
QByteArray zipped(const std::vector<std::pair<QString, QByteArray>>& files, ExportProblem* problem) {
    zip_error_t error;
    zip_error_init(&error);
    zip_source_t* buffer = zip_source_buffer_create(nullptr, 0, 0, &error);
    if (!buffer) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QString::fromUtf8(zip_error_strerror(&error));
        zip_error_fini(&error);
        return {};
    }
    zip_source_keep(buffer);
    zip_t* archive = zip_open_from_source(buffer, ZIP_TRUNCATE, &error);
    if (!archive) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QString::fromUtf8(zip_error_strerror(&error));
        zip_source_free(buffer);
        zip_source_free(buffer);
        zip_error_fini(&error);
        return {};
    }
    bool ok = true;
    for (const auto& [name, data] : files) {
        zip_source_t* source = zip_source_buffer(archive, data.constData(), static_cast<zip_uint64_t>(data.size()), 0);
        const zip_int64_t index =
            source ? zip_file_add(archive, name.toUtf8().constData(), source, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) : -1;
        if (index < 0) {
            if (source) {
                zip_source_free(source);
            }
            problem->lines << QStringLiteral("%1: %2").arg(name, QString::fromUtf8(zip_strerror(archive)));
            ok = false;
            break;
        }
        const bool packed = name.endsWith(QLatin1String(".png")) || name.endsWith(QLatin1String(".jpeg")) ||
                            name.endsWith(QLatin1String(".gif"));
        zip_set_file_compression(archive, static_cast<zip_uint64_t>(index), packed ? ZIP_CM_STORE : ZIP_CM_DEFLATE, 0);
    }
    if (!ok) {
        zip_discard(archive);
        zip_source_free(buffer);
        zip_error_fini(&error);
        problem->kind = ExportProblem::Kind::Other;
        return {};
    }
    if (zip_close(archive) != 0) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QString::fromUtf8(zip_strerror(archive));
        zip_discard(archive);
        zip_source_free(buffer);
        zip_error_fini(&error);
        return {};
    }
    QByteArray data;
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_source_stat(buffer, &stat) == 0 && zip_source_open(buffer) == 0) {
        data.resize(static_cast<qsizetype>(stat.size));
        const zip_int64_t read = zip_source_read(buffer, data.data(), stat.size);
        zip_source_close(buffer);
        if (read != static_cast<zip_int64_t>(stat.size)) {
            data.clear();
        }
    }
    zip_source_free(buffer);
    zip_error_fini(&error);
    if (data.isEmpty()) {
        problem->kind = ExportProblem::Kind::Other;
        problem->lines << QStringLiteral("The packed document could not be read from memory.");
    }
    return data;
}

}  // namespace

bool exportDocx(const BookExport& book, const QString& path, const ExportProgress& progress,
                ExportResult* result, ExportProblem* problem) {
    ExportProblem ignored;
    ExportProblem* reported = problem ? problem : &ignored;
    *reported = ExportProblem();
    ExportResult written;
    DocxWriter writer(book, progress);
    const std::vector<std::pair<QString, QByteArray>> files = writer.build(&written);
    const QByteArray data = zipped(files, reported);
    if (data.isEmpty()) {
        return false;
    }
    if (!writeExportFile(path, data, reported)) {
        return false;
    }
    written.file = path;
    if (result) {
        *result = written;
    }
    if (progress) {
        progress(100);
    }
    return true;
}

}  // namespace kalahari::editor
