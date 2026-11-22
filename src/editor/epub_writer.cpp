// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file epub_writer.cpp
/// @brief Export of a book to an EPUB 3 e-book
///
/// Each item of the export is a document of XHTML (a part, a text, the title page...), the
/// table of contents the navigation document, in the reading order where the book has it. The
/// reader divides the texts into its pages, so there are no page sizes, margins or numbers.
/// The look comes from the book's styles as CSS in units relative to the reader's letters, or
/// a simple one (the reader chooses the font and its size; the indents and spacing stay). The
/// footnotes are notes the reader shows over the text (EPUB 3), listed after their text; the
/// endnotes after their text or at the end of the book. An NCX table of contents serves the
/// readers of EPUB 2. The annotations stay in the book.

#include "kalahari/editor/detail/book_export_internal.h"

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_tables.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QImageReader>
#include <QRawFont>
#include <QRegularExpression>
#include <QSet>
#include <QTextTable>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

using namespace exporting;

/// The beginning of every document of XHTML
const QString XHTML_HEAD =
    QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE html>\n");

/// The share of the texts in the progress of the work, in percent
constexpr int TEXTS_SHARE = 90;

/// The bits of a font's embedding permissions (OS/2 fsType) that keep it out of an e-book,
/// whose fonts a reader may take out of it: restricted, preview and print only, its bitmaps
/// only
constexpr quint16 FONT_NOT_EMBEDDABLE = 0x0002 | 0x0004 | 0x0200;

/// The tables of a font copied to its file: those of its outlines, metrics, names and
/// typography, and of the variations of a variable font
constexpr std::array<const char*, 35> FONT_TABLES = {
    "OS/2", "cmap", "cvt ", "fpgm", "gasp", "glyf", "head", "hhea", "hmtx", "kern", "loca", "maxp",
    "name", "post", "prep", "vhea", "vmtx", "GDEF", "GPOS", "GSUB", "CFF ", "CFF2", "VORG", "BASE",
    "JSTF", "fvar", "gvar", "avar", "cvar", "HVAR", "VVAR", "MVAR", "STAT", "COLR", "CPAL"};

/// The checksum of a table of a font file (OpenType)
constexpr quint32 FONT_CHECKSUM_MAGIC = 0xB1B0AFBAU;

/// The width of a table's line in pixels of CSS for a width in points
constexpr double CSS_PIXELS_PER_POINT = 96.0 / 72.0;

/// A number of CSS: at most three decimals, without trailing zeros
QString cssNumber(double value) {
    QString text = QString::number(value, 'f', 3);
    while (text.contains(QLatin1Char('.')) &&
           (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        text.chop(1);
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

/// A length of CSS in em: @p points of a text of @p size points
QString emOf(double points, double size) {
    return cssNumber(points / std::max(1.0, size)) + QStringLiteral("em");
}

/// A name of a font as a string of CSS
QString cssString(const QString& text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
        .replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

/// An attribute of XML, with a space before it
QString attribute(const char* name, const QString& value) {
    return QLatin1Char(' ') + QLatin1String(name) + QStringLiteral("=\"") + xmlEscaped(value) +
           QLatin1Char('"');
}

/// A name of XML (an id, a class) from a text: its letters and digits of ASCII, hyphens and
/// underscores, the other characters underscores
QString xmlName(const QString& prefix, const QString& text) {
    QString name = prefix;
    for (const QChar ch : text) {
        const bool kept = (ch.unicode() < 128 && ch.isLetterOrNumber()) || ch == u'-' || ch == u'_';
        name += kept ? ch : QChar(u'_');
    }
    return name;
}

/// The type of an ordered list (the attribute of <ol>) of a list's numbers
QString listType(ListNumberFormat format) {
    switch (format) {
        case ListNumberFormat::LowerAlpha: return QStringLiteral("a");
        case ListNumberFormat::UpperAlpha: return QStringLiteral("A");
        case ListNumberFormat::LowerRoman: return QStringLiteral("i");
        case ListNumberFormat::UpperRoman: return QStringLiteral("I");
        case ListNumberFormat::Decimal:
        case ListNumberFormat::Bullet: break;
    }
    return QString();
}

/// Whether the items of a level need their marks written out: HTML lists have no text around
/// their numbers, no numbers of the levels above and no bullets but •, ◦ and ▪
bool writesOwnMarks(const ListLevel& level) {
    if (level.isBullet()) {
        return level.bullet != QString(QChar(BULLET_DISC)) &&
               level.bullet != QString(QChar(BULLET_CIRCLE)) &&
               level.bullet != QString(QChar(BULLET_SQUARE));
    }
    return level.suffix != QLatin1String(".") || !level.prefix.isEmpty() || level.shownLevels > 0;
}

/// The CSS of a border line of a table
QString borderCss(const TableBorders& borders) {
    QString style = QStringLiteral("solid");
    switch (borders.style) {
        case BorderStyle::Dashed: style = QStringLiteral("dashed"); break;
        case BorderStyle::Dotted: style = QStringLiteral("dotted"); break;
        case BorderStyle::Double: style = QStringLiteral("double"); break;
        case BorderStyle::Solid: break;
    }
    const double least = borders.style == BorderStyle::Double ? 3.0 : 1.0;
    const double width = std::max(least, std::round(borders.width * CSS_PIXELS_PER_POINT));
    return cssNumber(width) + QStringLiteral("px ") + style + QLatin1Char(' ') +
           (borders.color.isValid() ? borders.color.name(QColor::HexRgb)
                                    : QStringLiteral("currentColor"));
}

/// The language of the book as a tag of XML; "und" (undetermined) for none
QString languageTag(const QString& code) {
    static const QRegularExpression tag(QStringLiteral("^[A-Za-z]{2,8}(-[A-Za-z0-9]{1,8})*$"));
    return tag.match(code).hasMatch() ? code : QStringLiteral("und");
}

/// The checksum of a table of a font file: its 32-bit words added
quint32 fontChecksum(const QByteArray& data) {
    QByteArray padded = data;
    padded.append(QByteArray((4 - padded.size() % 4) % 4, '\0'));
    quint32 sum = 0;
    for (qsizetype i = 0; i < padded.size(); i += 4) {
        sum += qFromBigEndian<quint32>(padded.constData() + i);
    }
    return sum;
}

/// A big-endian number appended to a file
template <typename T>
void appendBigEndian(QByteArray& file, T value) {
    const T big = qToBigEndian(value);
    file.append(reinterpret_cast<const char*>(&big), sizeof(T));
}

/// A font as a font file (TrueType or OpenType), built from its tables; empty when it has not
/// the tables of one
/// @param cff Set: the outlines are of CFF (an .otf file)
QByteArray fontFileOf(const QRawFont& font, bool* cff) {
    std::vector<std::pair<QByteArray, QByteArray>> tables;
    for (const char* tag : FONT_TABLES) {
        if (const QByteArray data = font.fontTable(tag); !data.isEmpty()) {
            tables.emplace_back(QByteArray(tag), data);
        }
    }
    const auto has = [&tables](const char* tag) {
        return std::any_of(tables.cbegin(), tables.cend(),
                           [tag](const auto& table) { return table.first == tag; });
    };
    *cff = has("CFF ") || has("CFF2");
    if (!has("head") || !has("cmap") || !has("hmtx") || !has("maxp") || (!has("glyf") && !*cff)) {
        return {};
    }
    // The header and the directory of the tables (in the order of their tags), then the tables
    std::sort(tables.begin(), tables.end());
    const auto count = static_cast<quint16>(tables.size());
    quint16 searchRange = 1;
    quint16 entrySelector = 0;
    while (searchRange * 2 <= count) {
        searchRange = static_cast<quint16>(searchRange * 2);
        ++entrySelector;
    }
    QByteArray file;
    appendBigEndian<quint32>(file, *cff ? 0x4F54544FU : 0x00010000U);  // "OTTO" or TrueType
    appendBigEndian<quint16>(file, count);
    appendBigEndian<quint16>(file, static_cast<quint16>(searchRange * 16));
    appendBigEndian<quint16>(file, entrySelector);
    appendBigEndian<quint16>(file, static_cast<quint16>(count * 16 - searchRange * 16));
    auto offset = static_cast<quint32>(12 + 16 * count);
    for (auto& [tag, data] : tables) {
        if (tag == "head" && data.size() >= 12) {
            std::fill(data.begin() + 8, data.begin() + 12, '\0');  // its adjustment, set below
        }
        file.append(tag);
        appendBigEndian<quint32>(file, fontChecksum(data));
        appendBigEndian<quint32>(file, offset);
        appendBigEndian<quint32>(file, static_cast<quint32>(data.size()));
        offset += static_cast<quint32>((data.size() + 3) / 4 * 4);
    }
    qsizetype head = -1;
    for (const auto& [tag, data] : tables) {
        if (tag == "head") {
            head = file.size();
        }
        file.append(data);
        file.append(QByteArray((4 - data.size() % 4) % 4, '\0'));
    }
    if (head >= 0) {
        const quint32 adjustment =
            qToBigEndian(static_cast<quint32>(FONT_CHECKSUM_MAGIC - fontChecksum(file)));
        file.replace(head + 8, 4, reinterpret_cast<const char*>(&adjustment), 4);
    }
    return file;
}

/// A heading of the book: a line of the table of contents
struct NavHeading {
    int level = 1;
    QString text;
    QString target;  ///< Its document and its id: "text/item-003.xhtml#h4"
};

/// A note written after its text or at the end of the book
struct WrittenNote {
    NoteKind kind = NoteKind::Footnote;
    QString id;           ///< Its id
    QString reference;    ///< Its number in the text: "text/item-003.xhtml#r4"
    QString number;       ///< As the text shows it
    QStringList content;  ///< Its paragraphs as XHTML
};

/// A list open while the paragraphs are written
struct OpenList {
    int id = 0;
    int level = 0;
    bool numbered = true;  ///< <ol>; false: <ul>
};

/// A file of the publication besides its documents: an image, a font
struct Resource {
    QString id;
    QString href;  ///< From the folder of the publication
    QString mediaType;
    QString properties;  ///< Of its item of the manifest ("cover-image"); empty: none
};

/// The writer of the files of the e-book
class EpubWriter {
public:
    EpubWriter(const BookExport& book, const QString& fallbackTitle, const ExportProgress& progress)
        : m_book(book),
          m_title(book.title.simplified().isEmpty() ? fallbackTitle : book.title.simplified()),
          m_progress(progress) {
        m_images.setFolder(book.imagesFolder);
        m_textWidth = std::max(1.0, book.page.textWidthPoints());
        const double normal =
            book.styles.normal().properties.value(QStringLiteral("fontSize")).toDouble();
        m_normalSize = normal > 0.0 ? normal : DEFAULT_NORMAL_POINT_SIZE;
    }

    /// @brief The files of the archive in their order
    std::vector<std::pair<QString, QByteArray>> build(ExportResult* result);

private:
    void prepare();
    QString fileOf(size_t index) const;
    QString href(const QString& target) const;
    QString document(const QString& title, const QString& body, const QString& file) const;
    double styleSize(const QString& style) const;
    QString styleCss() const;
    QString styleRules(const ParagraphStyle& style) const;
    QString contentsList() const;

    QString itemDocument(size_t index);
    void writeBlocks(const ExportText& text, size_t index, int from, int to, bool topLevel,
                     QString& out);
    void writeListItem(const ExportText& text, const QTextBlock& block, const ListItem& item,
                       QString& out);
    void writeObject(const ExportText& text, TextObject object, QString& out);
    void writeTable(const ExportText& text, size_t index, const QTextTable& table, QString& out);
    void closeLists(QString& out);
    QString paragraphOf(const ExportText& text, const QTextBlock& block, const QString& tag,
                        const QString& attributes, bool inList, const QString& lead = QString());
    QString inlineOf(const ExportText& text, const QTextBlock& block, double size, bool code);
    QString runOf(const QString& characters, const QTextCharFormat& format, double size,
                  bool code) const;
    QString noteReference(const Piece& piece);
    QStringList noteContent(const Footnote& note) const;
    QString notesOf(const std::vector<WrittenNote>& notes) const;
    QString imageOf(const ImageProperties& image);

    QString navDocument() const;
    QString ncx() const;
    QString packageDocument() const;
    void addCover();
    void addFonts();
    QString identifier() const;

    const BookExport& m_book;
    QString m_title;
    ExportProgress m_progress;
    ImageStore m_images;
    double m_textWidth = 0.0;
    double m_normalSize = DEFAULT_NORMAL_POINT_SIZE;

    std::vector<std::unique_ptr<ExportText>> m_texts;
    std::vector<bool> m_withTitle;
    std::vector<NavHeading> m_headings;
    QHash<QPair<int, int>, int> m_headingAt;  ///< (item, block; -1 its title): its heading
    QHash<QString, QString> m_captionFiles;   ///< The id of a caption's number: its document
    int m_contentsItem = -1;                  ///< The table of contents (the navigation document)

    QString m_file;                          ///< The document written now
    QSet<QString> m_ids;                     ///< The ids given in it
    std::vector<OpenList> m_lists;           ///< The lists open, the innermost last
    std::vector<WrittenNote> m_notes;        ///< The notes after the text written now
    std::vector<WrittenNote> m_bookEndnotes;  ///< The endnotes at the end of the book
    int m_nextNote = 1;

    std::vector<std::pair<QString, QByteArray>> m_files;  ///< From the folder of the publication
    std::vector<Resource> m_resources;
    std::vector<std::pair<QString, QString>> m_documents;  ///< Id and file of the spine's documents
    QHash<QByteArray, QString> m_imageByHash;              ///< A picture's file
    QString m_coverFile;
    QString m_fontCss;

    int m_imagesWritten = 0;
    int m_missingImages = 0;
    bool m_imagesWithoutText = false;  ///< An image (not an ornament) without alternative text
    bool m_coverMissing = false;
    bool m_fontNotEmbedded = false;
};

QString EpubWriter::fileOf(size_t index) const {
    if (static_cast<int>(index) == m_contentsItem) {
        return QStringLiteral("nav.xhtml");
    }
    return QStringLiteral("text/item-%1.xhtml").arg(index + 1, 3, 10, QLatin1Char('0'));
}

QString EpubWriter::href(const QString& target) const {
    // From the document written now: the documents of the texts are in text/, the others
    // (the navigation document, the images, the styles) in the folder of the publication
    const QString path = target.section(QLatin1Char('#'), 0, 0);
    QString anchor = target.contains(QLatin1Char('#'))
                         ? QLatin1Char('#') + target.section(QLatin1Char('#'), 1)
                         : QString();
    if (path == m_file && !anchor.isEmpty()) {
        return anchor;
    }
    if (!m_file.startsWith(QLatin1String("text/"))) {
        return path + anchor;
    }
    return (path.startsWith(QLatin1String("text/")) ? path.mid(5) : QStringLiteral("../") + path) +
           anchor;
}

double EpubWriter::styleSize(const QString& style) const {
    const double size = m_book.styles.property(style, QStringLiteral("fontSize")).toDouble();
    return size > 0.0 ? size : m_normalSize;
}

void EpubWriter::prepare() {
    for (size_t index = 0; index < m_book.items.size(); ++index) {
        const ExportItem& item = m_book.items[index];
        m_texts.push_back(std::make_unique<ExportText>(item, exportFieldContext(m_book, item)));
        const ExportText& text = *m_texts.back();
        if (item.kind == ExportItemKind::Contents && m_contentsItem < 0) {
            m_contentsItem = static_cast<int>(index);
        }
        // The title of a text that starts with a heading of its own stays out, as in DOCX
        const bool titled = m_book.titleHeadings && !item.title.trimmed().isEmpty() &&
                            (item.kind != ExportItemKind::Text ||
                             !startsWithHeading(text.document(), m_book.styles));
        m_withTitle.push_back(titled);
        const QString file = fileOf(index);
        const auto addHeading = [&](int block, int level, const QString& heading) {
            const int number = static_cast<int>(m_headings.size()) + 1;
            m_headingAt.insert(qMakePair(static_cast<int>(index), block), number - 1);
            m_headings.push_back({level, heading, file + QStringLiteral("#h%1").arg(number)});
        };
        if (titled && item.kind != ExportItemKind::Contents) {
            addHeading(
                -1,
                std::max(1, headingLevelOfStyle(m_book.styles, titleStyleOf(item, m_book.styles))),
                item.title.simplified());
        }
        if (item.kind != ExportItemKind::Text) {
            continue;
        }
        for (QTextBlock block = text.document().begin(); block.isValid(); block = block.next()) {
            const bool inTable = tableOf(block) != nullptr;
            if (!inTable && blockObject(block) == TextObject::None &&
                !listItemOf(block.blockFormat())) {
                const int level =
                    headingLevelOfStyle(m_book.styles, shownStyleOf(m_book.styles, block));
                if (const QString heading = level > 0 ? plainTextOf(block, text) : QString();
                    !heading.isEmpty()) {
                    addHeading(block.blockNumber(), level, heading);
                }
            }
            if (!block.text().contains(QChar::ObjectReplacementCharacter)) {
                continue;
            }
            for (const Piece& piece : piecesOf(block, text)) {
                if (piece.captionNumber && !piece.captionNumber->id.isEmpty() &&
                    !m_captionFiles.contains(piece.captionNumber->id)) {
                    m_captionFiles.insert(piece.captionNumber->id, file);
                }
            }
        }
    }
}

// =============================================================================
// The look
// =============================================================================

QString EpubWriter::styleRules(const ParagraphStyle& style) const {
    const QVariantMap& properties = style.properties;
    const auto numberOf = [&properties](const char* name) -> std::optional<double> {
        bool ok = false;
        const double value = properties.value(QLatin1String(name)).toDouble(&ok);
        return properties.contains(QLatin1String(name)) && ok ? std::optional<double>(value)
                                                              : std::nullopt;
    };
    const double size = styleSize(style.id);
    QStringList rules;
    if (m_book.epub.bookStyles) {
        rules << QStringLiteral("font-size: ") + cssNumber(size / m_normalSize) +
                     QStringLiteral("em");
        const QString family = properties.value(QStringLiteral("fontFamily")).toString();
        if (style.id != QLatin1String(NORMAL_STYLE_ID) && !family.isEmpty()) {
            rules << QStringLiteral("font-family: ") + cssString(family);
        }
        if (const std::optional<double> lineHeight = numberOf("lineHeight");
            lineHeight && *lineHeight > 0.0) {
            rules << QStringLiteral("line-height: ") + cssNumber(*lineHeight);
        }
    }
    if (properties.contains(QStringLiteral("bold"))) {
        rules << (properties.value(QStringLiteral("bold")).toBool()
                      ? QStringLiteral("font-weight: bold")
                      : QStringLiteral("font-weight: normal"));
    }
    if (properties.contains(QStringLiteral("italic"))) {
        rules << (properties.value(QStringLiteral("italic")).toBool()
                      ? QStringLiteral("font-style: italic")
                      : QStringLiteral("font-style: normal"));
    }
    if (properties.value(QStringLiteral("underline")).toBool()) {
        rules << QStringLiteral("text-decoration: underline");
    }
    if (const QColor color(properties.value(QStringLiteral("textColor")).toString());
        color.isValid()) {
        rules << QStringLiteral("color: ") + color.name(QColor::HexRgb);
    }
    if (const QString alignment = styleAlignmentName(
            styleAlignmentOf(properties.value(QStringLiteral("alignment")).toString()));
        !alignment.isEmpty()) {
        rules << QStringLiteral("text-align: ") + alignment;
    }
    const std::array<std::pair<const char*, const char*>, 5> lengths = {
        {{"firstLineIndent", "text-indent"},
         {"leftMargin", "margin-left"},
         {"rightMargin", "margin-right"},
         {"spaceBefore", "margin-top"},
         {"spaceAfter", "margin-bottom"}}};
    for (const auto& [name, css] : lengths) {
        if (const std::optional<double> value = numberOf(name); value && *value != 0.0) {
            rules << QLatin1String(css) + QStringLiteral(": ") + emOf(*value, size);
        }
    }
    if (properties.value(QStringLiteral("keepWithNext")).toBool()) {
        rules << QStringLiteral("page-break-after: avoid");
    }
    if (properties.value(QStringLiteral("pageBreakBefore")).toBool()) {
        rules << QStringLiteral("page-break-before: always");
    }
    if (isStyleBasedOn(m_book.styles, style.id, QStringLiteral("code"))) {
        rules << QStringLiteral("white-space: pre-wrap");
        if (!m_book.epub.bookStyles) {
            rules << QStringLiteral("font-family: monospace");
        }
    }
    return rules.join(QStringLiteral("; "));
}

QString EpubWriter::styleCss() const {
    QString css = m_fontCss;
    QString body = QStringLiteral("margin: 0; padding: 0");
    if (m_book.epub.bookStyles && !m_book.fontFamily.isEmpty()) {
        body += QStringLiteral("; font-family: ") + cssString(m_book.fontFamily) +
                QStringLiteral(", serif");
    }
    css += QStringLiteral("body { ") + body + QStringLiteral(" }\n");
    css += QStringLiteral(
        "p { margin: 0; }\n"
        "h1, h2, h3, h4, h5, h6 { margin: 0; page-break-after: avoid; break-after: avoid; }\n"
        "img { max-width: 100%; height: auto; }\n"
        "li > p { text-indent: 0; margin-left: 0; }\n"
        "ol.marks, ul.marks { list-style: none; }\n"
        ".list-mark { display: inline-block; min-width: 1.5em; margin-left: -1.5em; }\n"
        "table { border-collapse: collapse; margin-top: 0.5em; margin-bottom: 0.5em; }\n"
        "td, th { padding: 0.2em 0.4em; vertical-align: top; text-align: left; }\n"
        "td > p, th > p { text-indent: 0; }\n"
        ".scene-break { text-align: center; text-indent: 0; margin: 1em 0; }\n"
        "hr.scene-break { width: 30%; margin: 1em auto; border: 0; border-top: 1px solid "
        "currentColor; }\n"
        ".page-break { page-break-after: always; break-after: page; }\n"
        ".figure-list { list-style: none; padding: 0; margin: 0; }\n"
        "a.noteref { text-decoration: none; }\n"
        "hr.notes { width: 25%; margin: 1.5em 0 0.5em 0; border: 0; border-top: 1px solid "
        "currentColor; }\n"
        ".note, .endnotes { font-size: 0.85em; }\n"
        ".note p, .endnotes p { text-indent: 0; }\n"
        ".endnotes ol { list-style: none; padding: 0; margin: 0; }\n"
        "nav ol { list-style: none; padding-left: 1.5em; margin: 0; }\n"
        "nav > ol { padding-left: 0; }\n"
        ".cover { margin: 0; padding: 0; text-align: center; }\n"
        ".cover img { max-height: 100%; }\n");
    for (const ParagraphStyle& style : m_book.styles.styles()) {
        const QString rules = styleRules(style);
        if (!rules.isEmpty()) {
            css += QLatin1Char('.') + xmlName(QStringLiteral("s-"), style.id) +
                   QStringLiteral(" { ") + rules + QStringLiteral(" }\n");
        }
    }
    return css;
}

// =============================================================================
// The documents of the texts
// =============================================================================

QString EpubWriter::document(const QString& title, const QString& body, const QString& file) const {
    const QString language = languageTag(m_book.language);
    QString css = QStringLiteral("styles/book.css");
    if (file.startsWith(QLatin1String("text/"))) {
        css.prepend(QStringLiteral("../"));
    }
    return XHTML_HEAD +
           QStringLiteral(
               "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
               "xmlns:epub=\"http://www.idpf.org/2007/ops\"") +
           attribute("xml:lang", language) + attribute("lang", language) +
           QStringLiteral(">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>") + xmlEscaped(title) +
           QStringLiteral("</title>\n<link rel=\"stylesheet\" type=\"text/css\"") +
           attribute("href", css) + QStringLiteral("/>\n</head>\n<body>\n") + body +
           QStringLiteral("</body>\n</html>\n");
}

QString EpubWriter::runOf(const QString& characters, const QTextCharFormat& format, double size,
                          bool code) const {
    QString text = xmlEscaped(characters);
    text.replace(QChar::LineSeparator, QStringLiteral("<br/>"));
    text.remove(QChar::ParagraphSeparator).remove(QChar::ObjectReplacementCharacter);
    if (!code) {
        text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    }
    if (text.isEmpty()) {
        return text;
    }

    // The look of the run: a span with its style, then its tags
    QStringList style;
    QStringList tags;
    if (format.hasProperty(QTextFormat::FontWeight)) {
        if (format.fontWeight() > QFont::Normal) {
            tags << QStringLiteral("b");
        } else {
            style << QStringLiteral("font-weight: normal");
        }
    }
    if (format.hasProperty(QTextFormat::FontItalic)) {
        if (format.fontItalic()) {
            tags << QStringLiteral("i");
        } else {
            style << QStringLiteral("font-style: normal");
        }
    }
    if (format.fontUnderline()) {
        tags << QStringLiteral("u");
    }
    if (format.fontStrikeOut()) {
        tags << QStringLiteral("s");
    }
    if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript) {
        tags << QStringLiteral("sup");
    } else if (format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
        tags << QStringLiteral("sub");
    }
    double shownSize = size;
    if (m_book.epub.bookStyles) {
        const QStringList families = format.fontFamilies().toStringList();
        const QString family = !families.isEmpty() ? families.constFirst()
                                                   : format.stringProperty(QTextFormat::FontFamily);
        if (!family.isEmpty()) {
            style << QStringLiteral("font-family: ") + cssString(family);
        }
        if (format.fontPointSize() > 0.0) {
            style << QStringLiteral("font-size: ") +
                         cssNumber(format.fontPointSize() / std::max(1.0, size)) +
                         QStringLiteral("em");
            shownSize = format.fontPointSize();
        }
    }
    if (format.foreground().style() != Qt::NoBrush && format.foreground().color().isValid()) {
        style << QStringLiteral("color: ") + format.foreground().color().name(QColor::HexRgb);
    }
    if (format.background().style() != Qt::NoBrush && format.background().color().isValid() &&
        format.background().color().alpha() > 0) {
        style << QStringLiteral("background-color: ") +
                     format.background().color().name(QColor::HexRgb);
    }
    if (const double spacing = format.doubleProperty(KmlPropLetterSpacing); spacing != 0.0) {
        style << QStringLiteral("letter-spacing: ") + emOf(spacing, shownSize);
    }
    switch (format.fontCapitalization()) {
        case QFont::SmallCaps: style << QStringLiteral("font-variant: small-caps"); break;
        case QFont::AllUppercase: style << QStringLiteral("text-transform: uppercase"); break;
        case QFont::AllLowercase: style << QStringLiteral("text-transform: lowercase"); break;
        case QFont::Capitalize: style << QStringLiteral("text-transform: capitalize"); break;
        case QFont::MixedCase: break;
    }
    QString open;
    QString close;
    if (!style.isEmpty()) {
        open = QStringLiteral("<span") + attribute("style", style.join(QStringLiteral("; "))) +
               QLatin1Char('>');
        close = QStringLiteral("</span>");
    }
    for (const QString& tag : std::as_const(tags)) {
        open += QLatin1Char('<') + tag + QLatin1Char('>');
        close.prepend(QStringLiteral("</") + tag + QLatin1Char('>'));
    }
    return open + text + close;
}

QStringList EpubWriter::noteContent(const Footnote& note) const {
    QStringList paragraphs;
    const double size = styleSize(QString::fromLatin1(FOOTNOTE_STYLE_ID));
    for (const NoteParagraph& paragraph : noteParagraphs(note.content)) {
        std::vector<FormatRun> runs = paragraph.formats;
        std::sort(runs.begin(), runs.end(),
                  [](const FormatRun& a, const FormatRun& b) { return a.start < b.start; });
        QString text;
        size_t at = 0;
        const size_t length = static_cast<size_t>(paragraph.text.size());
        const auto write = [&](size_t from, size_t to, const QTextCharFormat& format) {
            if (to <= from) {
                return;
            }
            const QString run = runOf(
                paragraph.text.mid(static_cast<qsizetype>(from), static_cast<qsizetype>(to - from)),
                format, size, false);
            const QString address = linkAddressOf(format);
            text += isOpenableLinkAddress(address)
                        ? QStringLiteral("<a") + attribute("href", address) + QLatin1Char('>') +
                              run + QStringLiteral("</a>")
                        : run;
        };
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
        paragraphs << text;
    }
    return paragraphs;
}

QString EpubWriter::noteReference(const Piece& piece) {
    // The number in the text goes to its note after the text, or at the end of the book
    const int number = m_nextNote++;
    WrittenNote note;
    note.kind = piece.note.kind;
    note.id = QStringLiteral("n%1").arg(number);
    note.reference = m_file + QStringLiteral("#r%1").arg(number);
    note.number = piece.text;
    note.content = noteContent(piece.note);
    const bool atBookEnd = note.kind == NoteKind::Endnote && m_book.endnotesAtBookEnd;
    const QString target =
        (atBookEnd ? QStringLiteral("text/endnotes.xhtml") : m_file) + QLatin1Char('#') + note.id;
    (atBookEnd ? m_bookEndnotes : m_notes).push_back(note);
    return QStringLiteral("<sup><a") + attribute("id", QStringLiteral("r%1").arg(number)) +
           attribute("href", href(target)) +
           QStringLiteral(" epub:type=\"noteref\" role=\"doc-noteref\" class=\"noteref\">") +
           xmlEscaped(piece.text) + QStringLiteral("</a></sup>");
}

QString EpubWriter::notesOf(const std::vector<WrittenNote>& notes) const {
    // The footnotes as notes the reader shows over the text (and here), the endnotes as a list
    const auto noteText = [this](const WrittenNote& note) {
        QString paragraphs;
        for (qsizetype i = 0; i < note.content.size(); ++i) {
            paragraphs += QStringLiteral("<p>");
            if (i == 0) {
                paragraphs += QStringLiteral("<a") + attribute("href", href(note.reference)) +
                              QStringLiteral(" role=\"doc-backlink\">") + xmlEscaped(note.number) +
                              QStringLiteral("</a> ");
            }
            paragraphs += note.content.at(i) + QStringLiteral("</p>");
        }
        return paragraphs;
    };
    QString footnotes;
    QString endnotes;
    for (const WrittenNote& note : notes) {
        if (note.kind == NoteKind::Footnote) {
            footnotes +=
                QStringLiteral("<aside") + attribute("id", note.id) +
                QStringLiteral(" epub:type=\"footnote\" role=\"doc-footnote\" class=\"note\">") +
                noteText(note) + QStringLiteral("</aside>\n");
        } else {
            endnotes += QStringLiteral("<li") + attribute("id", note.id) +
                        QStringLiteral(" epub:type=\"endnote\">") + noteText(note) +
                        QStringLiteral("</li>\n");
        }
    }
    QString out;
    if (!footnotes.isEmpty()) {
        out += QStringLiteral("<hr class=\"notes\"/>\n") + footnotes;
    }
    if (!endnotes.isEmpty()) {
        out += QStringLiteral(
                   "<section class=\"endnotes\" epub:type=\"endnotes\" "
                   "role=\"doc-endnotes\">\n<ol>\n") +
               endnotes + QStringLiteral("</ol>\n</section>\n");
    }
    return out;
}

QString EpubWriter::imageOf(const ImageProperties& image) {
    if (m_book.imagesFolder.isEmpty()) {
        ++m_missingImages;
        return QString();
    }
    ExportPicture picture = exportPicture(m_images, image, m_textWidth, 0.0);
    if (picture.missing || picture.data.isEmpty()) {
        ++m_missingImages;
        return QString();
    }
    if (picture.extension != QLatin1String("png") && picture.extension != QLatin1String("jpeg") &&
        picture.extension != QLatin1String("gif")) {
        // Not a picture every reader shows: as PNG
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!QImage::fromData(picture.data).save(&buffer, "PNG")) {
            ++m_missingImages;
            return QString();
        }
        picture.data = png;
        picture.extension = QStringLiteral("png");
        picture.contentType = QStringLiteral("image/png");
    }
    const QByteArray hash = QCryptographicHash::hash(picture.data, QCryptographicHash::Sha256);
    QString file = m_imageByHash.value(hash);
    if (file.isEmpty()) {
        const qsizetype number = m_imageByHash.size() + 1;
        file = QStringLiteral("images/image-%1.%2")
                   .arg(number, 3, 10, QLatin1Char('0'))
                   .arg(picture.extension == QLatin1String("jpeg") ? QStringLiteral("jpg")
                                                                   : picture.extension);
        m_files.emplace_back(file, picture.data);
        m_resources.push_back(
            {QStringLiteral("image-%1").arg(number), file, picture.contentType, QString()});
        m_imageByHash.insert(hash, file);
    }
    ++m_imagesWritten;
    const QString alt = image.decorative ? QString() : image.alt.simplified();
    if (!image.decorative && alt.isEmpty()) {
        m_imagesWithoutText = true;
    }
    const double percent = std::clamp(picture.points.width() / m_textWidth * 100.0, 1.0, 100.0);
    return QStringLiteral("<img") + attribute("src", href(file)) + attribute("alt", alt) +
           (image.decorative ? QStringLiteral(" role=\"presentation\"") : QString()) +
           attribute("style", QStringLiteral("width: ") + cssNumber(percent) + QLatin1Char('%')) +
           QStringLiteral("/>");
}

QString EpubWriter::inlineOf(const ExportText& text, const QTextBlock& block, double size,
                             bool code) {
    QString result;
    QString link;  // the address of the link open; empty: none
    const auto setLink = [&](const QString& address) {
        if (address == link) {
            return;
        }
        if (!link.isEmpty()) {
            result += QStringLiteral("</a>");
        }
        link = address;
        if (!link.isEmpty()) {
            result += QStringLiteral("<a") + attribute("href", link) + QLatin1Char('>');
        }
    };
    for (const Piece& piece : piecesOf(block, text)) {
        // A link holds no other link: the numbers of the notes and the cross-references
        // are outside it
        const bool ownLink =
            piece.kind == Piece::Kind::Note || (piece.reference && piece.reference->link &&
                                                m_captionFiles.contains(piece.reference->target));
        setLink(ownLink ? QString() : piece.link);
        switch (piece.kind) {
            case Piece::Kind::Text:
                result += runOf(piece.text, piece.format, size, code);
                break;
            case Piece::Kind::Image:
                result += imageOf(piece.image);
                break;
            case Piece::Kind::Note:
                result += noteReference(piece);
                break;
            case Piece::Kind::Field: {
                const QString shown = runOf(piece.text, piece.format, size, code);
                if (piece.captionNumber &&
                    m_captionFiles.value(piece.captionNumber->id) == m_file) {
                    const QString id = xmlName(QStringLiteral("cap-"), piece.captionNumber->id);
                    if (!m_ids.contains(id)) {
                        m_ids.insert(id);
                        result += QStringLiteral("<span") + attribute("id", id) + QLatin1Char('>') +
                                  shown + QStringLiteral("</span>");
                        break;
                    }
                } else if (ownLink && piece.reference) {
                    const QString target = m_captionFiles.value(piece.reference->target) +
                                           QLatin1Char('#') +
                                           xmlName(QStringLiteral("cap-"), piece.reference->target);
                    result += QStringLiteral("<a") + attribute("href", href(target)) +
                              QLatin1Char('>') + shown + QStringLiteral("</a>");
                    break;
                }
                result += shown;
                break;
            }
        }
    }
    setLink(QString());
    return result;
}

QString EpubWriter::paragraphOf(const ExportText& text, const QTextBlock& block, const QString& tag,
                                const QString& attributes, bool inList, const QString& lead) {
    const QString style = shownStyleOf(m_book.styles, block);
    const double size = styleSize(style);
    const bool code = isStyleBasedOn(m_book.styles, style, QStringLiteral("code"));
    QString content = inlineOf(text, block, size, code);
    if (content.isEmpty()) {
        content = QStringLiteral("<br/>");  // an empty paragraph keeps its line
    }
    content.prepend(lead);

    // The paragraph's own formatting over its style's (a list item: the indents of its list)
    const QTextBlockFormat format = block.blockFormat();
    const ParagraphFormat own = paragraphFormatOf(format);
    QStringList css;
    if (!inList && own.leftIndent) {
        css << QStringLiteral("margin-left: ") + emOf(*own.leftIndent, size);
    }
    if (!inList && own.firstLineIndent) {
        css << QStringLiteral("text-indent: ") + emOf(*own.firstLineIndent, size);
    }
    if (own.rightIndent) {
        css << QStringLiteral("margin-right: ") + emOf(*own.rightIndent, size);
    }
    // The room of an image around its paragraph and its caption, over the paragraph's own
    // (or its style's) room
    std::optional<double> spaceBefore = own.spaceBefore;
    std::optional<double> spaceAfter = own.spaceAfter;
    if (const ImageBlockSpace space = imageBlockSpace(block); !space.isEmpty()) {
        const QVariantMap& properties = m_book.styles.shownStyle(style).properties;
        const auto styleSpace = [&properties](const char* name) {
            return std::max(0.0, properties.value(QLatin1String(name)).toDouble());
        };
        if (space.before > 0.0) {
            spaceBefore = spaceBefore.value_or(styleSpace("spaceBefore")) + space.before;
        }
        if (space.after > 0.0) {
            spaceAfter = spaceAfter.value_or(styleSpace("spaceAfter")) + space.after;
        }
    }
    if (spaceBefore) {
        css << QStringLiteral("margin-top: ") + emOf(*spaceBefore, size);
    }
    if (spaceAfter) {
        css << QStringLiteral("margin-bottom: ") + emOf(*spaceAfter, size);
    }
    if (m_book.epub.bookStyles && own.lineSpacing) {
        css << QStringLiteral("line-height: ") + (own.lineSpacing->rule == LineSpacingRule::Multiple
                                                      ? cssNumber(own.lineSpacing->value)
                                                      : emOf(own.lineSpacing->value, size));
    }
    if (own.pageBreakBefore.value_or(false)) {
        css << QStringLiteral("page-break-before: always");
    }
    if (const QString alignment = styleAlignmentName(ownAlignment(format)); !alignment.isEmpty()) {
        css << QStringLiteral("text-align: ") + alignment;
    }
    return QLatin1Char('<') + tag + attributes +
           attribute("class", xmlName(QStringLiteral("s-"), style)) +
           (css.isEmpty() ? QString() : attribute("style", css.join(QStringLiteral("; ")))) +
           QLatin1Char('>') + content + QStringLiteral("</") + tag + QLatin1Char('>');
}

void EpubWriter::closeLists(QString& out) {
    while (!m_lists.empty()) {
        out += m_lists.back().numbered ? QStringLiteral("</li>\n</ol>\n")
                                       : QStringLiteral("</li>\n</ul>\n");
        m_lists.pop_back();
    }
}

void EpubWriter::writeListItem(const ExportText& text, const QTextBlock& block,
                               const ListItem& item, QString& out) {
    if (!item.numbered) {
        // Without a number: a paragraph of the item before, at its text
        out += paragraphOf(text, block, QStringLiteral("p"), QString(), !m_lists.empty()) +
               QLatin1Char('\n');
        return;
    }
    // The lists deeper than its level end, and one of its level of another list or kind
    const bool numbered = !item.levelFormat().isBullet();
    const bool ownMarks = writesOwnMarks(item.levelFormat());
    while (!m_lists.empty() &&
           (m_lists.back().level > item.level ||
            (m_lists.back().level == item.level &&
             (m_lists.back().numbered != numbered || m_lists.back().id != item.id)))) {
        out += m_lists.back().numbered ? QStringLiteral("</li>\n</ol>\n")
                                       : QStringLiteral("</li>\n</ul>\n");
        m_lists.pop_back();
    }
    if (!m_lists.empty() && m_lists.back().level == item.level) {
        out += QStringLiteral("</li>\n");
    } else {
        // A list of its own, in the item of the level above when one is open
        const QString type = listType(item.levelFormat().format);
        if (ownMarks) {
            out += (numbered ? QStringLiteral("<ol") : QStringLiteral("<ul")) +
                   attribute("class", QStringLiteral("marks")) + QStringLiteral(">\n");
        } else if (numbered) {
            out += QStringLiteral("<ol") + (type.isEmpty() ? QString() : attribute("type", type)) +
                   QStringLiteral(">\n");
        } else {
            out += QStringLiteral("<ul>\n");
        }
        m_lists.push_back({item.id, item.level, numbered});
    }
    // The mark written out, in the place of the list's own
    if (ownMarks) {
        out += QStringLiteral("<li>") +
               paragraphOf(text, block, QStringLiteral("p"), QString(), true,
                           QStringLiteral("<span class=\"list-mark\">") +
                               xmlEscaped(text.listMarker(block)) + QStringLiteral("</span> "));
        return;
    }
    out += numbered ? QStringLiteral("<li") +
                          attribute("value", QString::number(std::max(0, text.listNumber(block)))) +
                          QLatin1Char('>')
                    : QStringLiteral("<li>");
    out += paragraphOf(text, block, QStringLiteral("p"), QString(), true);
}

void EpubWriter::writeObject(const ExportText& text, TextObject object, QString& out) {
    closeLists(out);
    switch (object) {
        case TextObject::PageBreak:
            out += QStringLiteral("<div class=\"page-break\"></div>\n");
            return;
        case TextObject::HorizontalRule:
            out += QStringLiteral("<hr/>\n");
            return;
        case TextObject::SceneBreak: {
            if (m_book.sceneBreak == SceneBreakLook::Line) {
                out += QStringLiteral("<hr class=\"scene-break\"/>\n");
                return;
            }
            const QString symbol = sceneBreakText(m_book.sceneBreak);
            out += QStringLiteral("<p class=\"scene-break\">") +
                   (symbol.isEmpty() ? QStringLiteral("<br/>") : xmlEscaped(symbol)) +
                   QStringLiteral("</p>\n");
            return;
        }
        case TextObject::FigureList: {
            QString rows;
            for (const FigureListRow& row : text.captions().figureList()) {
                const QString file = m_captionFiles.value(row.id);
                rows +=
                    QStringLiteral("<li>") +
                    (file.isEmpty()
                         ? xmlEscaped(row.text)
                         : QStringLiteral("<a") +
                               attribute("href", href(file + QLatin1Char('#') +
                                                      xmlName(QStringLiteral("cap-"), row.id))) +
                               QLatin1Char('>') + xmlEscaped(row.text) + QStringLiteral("</a>")) +
                    QStringLiteral("</li>\n");
            }
            if (!rows.isEmpty()) {
                out += QStringLiteral("<ul class=\"figure-list\">\n") + rows +
                       QStringLiteral("</ul>\n");
            }
            return;
        }
        case TextObject::None:
            return;
    }
}

void EpubWriter::writeTable(const ExportText& text, size_t index, const QTextTable& table,
                            QString& out) {
    const TableShape shape = tableShapeOf(table);
    const TableProperties& properties = shape.properties;
    const double width = tableWidthPoints(shape, m_textWidth);
    QStringList css{QStringLiteral("width: ") +
                    cssNumber(std::min(100.0, width / m_textWidth * 100.0)) + QLatin1Char('%')};
    switch (properties.alignment) {
        case TableAlignment::Center:
            css << QStringLiteral("margin-left: auto") << QStringLiteral("margin-right: auto");
            break;
        case TableAlignment::Right:
            css << QStringLiteral("margin-left: auto");
            break;
        case TableAlignment::Left:
            if (properties.indent != 0.0) {
                css << QStringLiteral("margin-left: ") + emOf(properties.indent, m_normalSize);
            }
            break;
    }
    out += QStringLiteral("<table") + attribute("style", css.join(QStringLiteral("; "))) +
           QStringLiteral(">\n<colgroup>");
    for (const double column : columnWidthsPoints(shape, width)) {
        out += QStringLiteral("<col") +
               attribute("style", QStringLiteral("width: ") + cssNumber(column / width * 100.0) +
                                      QLatin1Char('%')) +
               QStringLiteral("/>");
    }
    out += QStringLiteral("</colgroup>\n");

    const TableBorders& borders = properties.borders;
    const QString line = borderCss(borders);
    const QTextDocument& document = text.document();
    const std::vector<OpenList> lists = m_lists;  // the lists of the cells are their own
    m_lists.clear();
    for (int row = 0; row < shape.rows; ++row) {
        if (row == 0 && shape.headerRows > 0) {
            out += QStringLiteral("<thead>\n");
        } else if (row == shape.headerRows) {
            out +=
                (row > 0 ? QStringLiteral("</thead>\n") : QString()) + QStringLiteral("<tbody>\n");
        }
        out += QStringLiteral("<tr>");
        for (int column = 0; column < shape.columns;) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (!cell.isValid()) {
                ++column;
                continue;
            }
            const int columnSpan = std::max(1, cell.columnSpan());
            if (cell.row() != row || cell.column() != column) {
                column = cell.column() + columnSpan;  // covered by a cell above it
                continue;
            }
            const int rowSpan = std::max(1, cell.rowSpan());
            const TableCellProperties cellProperties = cellPropertiesOf(cell);
            QStringList cellCss;
            const auto edge = [&](const char* side, bool outer, int outerFlag, int innerFlag) {
                const bool drawn = (borders.lines & (outer ? outerFlag : innerFlag)) != 0;
                cellCss << QStringLiteral("border-") + QLatin1String(side) + QStringLiteral(": ") +
                               (drawn ? line : QStringLiteral("none"));
            };
            edge("top", row == 0, BORDER_TOP, BORDER_ROWS);
            edge("bottom", row + rowSpan >= shape.rows, BORDER_BOTTOM, BORDER_ROWS);
            edge("left", column == 0, BORDER_LEFT, BORDER_COLUMNS);
            edge("right", column + columnSpan >= shape.columns, BORDER_RIGHT, BORDER_COLUMNS);
            if (cellProperties.background.isValid() && cellProperties.background.alpha() > 0) {
                cellCss << QStringLiteral("background-color: ") +
                               cellProperties.background.name(QColor::HexRgb);
            }
            if (cellProperties.alignment == CellAlignment::Middle) {
                cellCss << QStringLiteral("vertical-align: middle");
            } else if (cellProperties.alignment == CellAlignment::Bottom) {
                cellCss << QStringLiteral("vertical-align: bottom");
            }
            const QString tag =
                row < shape.headerRows ? QStringLiteral("th") : QStringLiteral("td");
            out +=
                QLatin1Char('<') + tag +
                (columnSpan > 1 ? attribute("colspan", QString::number(columnSpan)) : QString()) +
                (rowSpan > 1 ? attribute("rowspan", QString::number(rowSpan)) : QString()) +
                attribute("style", cellCss.join(QStringLiteral("; "))) + QLatin1Char('>');
            const int first = document.findBlock(cell.firstPosition()).blockNumber();
            const int last = document.findBlock(cell.lastPosition()).blockNumber();
            writeBlocks(text, index, first, last, false, out);
            closeLists(out);
            out += QStringLiteral("</") + tag + QLatin1Char('>');
            column += columnSpan;
        }
        out += QStringLiteral("</tr>\n");
    }
    if (shape.rows > 0) {
        out += shape.headerRows >= shape.rows ? QStringLiteral("</thead>\n")
                                              : QStringLiteral("</tbody>\n");
    }
    out += QStringLiteral("</table>\n");
    m_lists = lists;
}

void EpubWriter::writeBlocks(const ExportText& text, size_t index, int from, int to, bool topLevel,
                             QString& out) {
    const QTextDocument& document = text.document();
    for (QTextBlock block = document.findBlockByNumber(from);
         block.isValid() && block.blockNumber() <= to; block = block.next()) {
        if (topLevel) {
            if (const QTextTable* table = tableOf(block)) {
                closeLists(out);
                writeTable(text, index, *table, out);
                block = document.findBlock(table->lastPosition());  // its last paragraph
                continue;
            }
        }
        if (const TextObject object = blockObject(block); object != TextObject::None) {
            writeObject(text, object, out);
            continue;
        }
        if (const std::optional<ListItem> item = listItemOf(block.blockFormat())) {
            writeListItem(text, block, *item, out);
            continue;
        }
        closeLists(out);
        const int level = headingLevelOfStyle(m_book.styles, shownStyleOf(m_book.styles, block));
        const int heading =
            m_headingAt.value(qMakePair(static_cast<int>(index), block.blockNumber()), -1);
        if (topLevel && level > 0 && heading >= 0) {
            out += paragraphOf(text, block, QStringLiteral("h%1").arg(std::clamp(level, 1, 6)),
                               attribute("id", QStringLiteral("h%1").arg(heading + 1)), false) +
                   QLatin1Char('\n');
        } else {
            out +=
                paragraphOf(text, block, QStringLiteral("p"), QString(), false) + QLatin1Char('\n');
        }
    }
}

QString EpubWriter::itemDocument(size_t index) {
    const ExportItem& item = m_book.items[index];
    m_file = fileOf(index);
    m_ids.clear();
    m_notes.clear();
    m_lists.clear();

    QString type;
    switch (item.kind) {
        case ExportItemKind::Group:
            type = QStringLiteral(" epub:type=\"part\" role=\"doc-part\"");
            break;
        case ExportItemKind::Contents:
            type = QStringLiteral(" epub:type=\"toc\" role=\"doc-toc\"");
            break;
        case ExportItemKind::Text:
            if (!item.title.trimmed().isEmpty()) {
                type = QStringLiteral(" epub:type=\"chapter\" role=\"doc-chapter\"");
            }
            break;
    }
    QString body = QStringLiteral("<section") + type + QStringLiteral(">\n");
    if (m_withTitle[index]) {
        const QString style = titleStyleOf(item, m_book.styles);
        const int heading = m_headingAt.value(qMakePair(static_cast<int>(index), -1), -1);
        const int level =
            std::clamp(heading >= 0 ? m_headings[static_cast<size_t>(heading)].level : 1, 1, 6);
        body +=
            QStringLiteral("<h%1").arg(level) +
            (heading >= 0 ? attribute("id", QStringLiteral("h%1").arg(heading + 1)) : QString()) +
            attribute("class", xmlName(QStringLiteral("s-"), style)) + QLatin1Char('>') +
            xmlEscaped(item.title.simplified()) + QStringLiteral("</h%1>\n").arg(level);
    }
    if (item.kind == ExportItemKind::Text) {
        const ExportText& text = *m_texts[index];
        writeBlocks(text, index, 0, text.document().blockCount() - 1, true, body);
        closeLists(body);
    } else if (item.kind == ExportItemKind::Contents) {
        body += contentsList();
    }
    body += notesOf(m_notes);
    body += QStringLiteral("</section>\n");
    return document(item.title.simplified().isEmpty() ? m_title : item.title.simplified(), body,
                    m_file);
}

// =============================================================================
// The tables of contents, the cover, the fonts and the package
// =============================================================================

QString EpubWriter::contentsList() const {
    // The headings as lists in lists, by their levels
    if (m_headings.empty()) {
        const QString first = m_documents.empty() ? QString() : m_documents.front().second;
        return QStringLiteral("<ol>\n<li><a") + attribute("href", href(first)) + QLatin1Char('>') +
               xmlEscaped(m_title) + QStringLiteral("</a></li>\n</ol>\n");
    }
    QString out = QStringLiteral("<ol>\n");
    std::vector<int> open;  // the levels of the lists open, the outer one first
    for (const NavHeading& heading : m_headings) {
        const int level = std::clamp(heading.level, 1, HEADING_LEVELS);
        while (open.size() > 1 && open.back() > level) {
            out += QStringLiteral("</li>\n</ol>\n");
            open.pop_back();
        }
        if (!open.empty() && open.back() >= level) {
            out += QStringLiteral("</li>\n");
        } else if (!open.empty()) {
            out += QStringLiteral("\n<ol>\n");
            open.push_back(level);
        } else {
            open.push_back(level);
        }
        out += QStringLiteral("<li><a") + attribute("href", href(heading.target)) +
               QLatin1Char('>') + xmlEscaped(heading.text) + QStringLiteral("</a>");
    }
    while (open.size() > 1) {
        out += QStringLiteral("</li>\n</ol>\n");
        open.pop_back();
    }
    return out + QStringLiteral("</li>\n</ol>\n");
}

QString EpubWriter::navDocument() const {
    QString body;
    QString title = m_title;
    if (m_contentsItem >= 0) {
        const ExportItem& item = m_book.items[static_cast<size_t>(m_contentsItem)];
        title = item.title.simplified();
        body += QStringLiteral("<nav epub:type=\"toc\" role=\"doc-toc\" id=\"toc\">\n");
        if (!title.isEmpty() && m_book.titleHeadings) {
            body += QStringLiteral("<h1") +
                    attribute("class",
                              xmlName(QStringLiteral("s-"), titleStyleOf(item, m_book.styles))) +
                    QLatin1Char('>') + xmlEscaped(title) + QStringLiteral("</h1>\n");
        }
        if (title.isEmpty()) {
            title = m_title;
        }
    } else {
        // Not in the reading order: only the reader's own table of contents
        body += QStringLiteral("<nav epub:type=\"toc\" role=\"doc-toc\" id=\"toc\">\n<h1>") +
                xmlEscaped(m_title) + QStringLiteral("</h1>\n");
    }
    body += contentsList() + QStringLiteral("</nav>\n");

    // The places a reader opens: the cover, the table of contents, the start of the text
    QString landmarks;
    if (!m_coverFile.isEmpty()) {
        landmarks += QStringLiteral("<li><a epub:type=\"cover\"") + attribute("href", m_coverFile) +
                     QLatin1Char('>') + xmlEscaped(m_title) + QStringLiteral("</a></li>\n");
    }
    if (m_contentsItem >= 0) {
        landmarks += QStringLiteral("<li><a epub:type=\"toc\" href=\"#toc\">") + xmlEscaped(title) +
                     QStringLiteral("</a></li>\n");
    }
    const auto start =
        std::find_if(m_headings.cbegin(), m_headings.cend(), [](const NavHeading& heading) {
            return heading.target.startsWith(QLatin1String("text/"));
        });
    if (start != m_headings.cend()) {
        landmarks += QStringLiteral("<li><a epub:type=\"bodymatter\"") +
                     attribute("href", start->target) + QLatin1Char('>') + xmlEscaped(start->text) +
                     QStringLiteral("</a></li>\n");
    }
    if (!landmarks.isEmpty()) {
        body += QStringLiteral(
                    "<nav epub:type=\"landmarks\" id=\"landmarks\" hidden=\"hidden\">\n<ol>\n") +
                landmarks + QStringLiteral("</ol>\n</nav>\n");
    }
    return document(title, body, QStringLiteral("nav.xhtml"));
}

QString EpubWriter::ncx() const {
    QString points;
    std::vector<int> open;  // the levels of the points open
    int order = 0;
    int depth = 0;
    for (const NavHeading& heading : m_headings) {
        const int level = std::clamp(heading.level, 1, HEADING_LEVELS);
        while (!open.empty() && open.back() >= level) {
            points += QStringLiteral("</navPoint>\n");
            open.pop_back();
        }
        open.push_back(level);
        depth = std::max(depth, static_cast<int>(open.size()));
        ++order;
        points +=
            QStringLiteral("<navPoint id=\"np%1\" playOrder=\"%1\"><navLabel><text>").arg(order) +
            xmlEscaped(heading.text) + QStringLiteral("</text></navLabel><content") +
            attribute("src", heading.target) + QStringLiteral("/>\n");
    }
    while (!open.empty()) {
        points += QStringLiteral("</navPoint>\n");
        open.pop_back();
    }
    if (points.isEmpty() && !m_documents.empty()) {
        points = QStringLiteral("<navPoint id=\"np1\" playOrder=\"1\"><navLabel><text>") +
                 xmlEscaped(m_title) + QStringLiteral("</text></navLabel><content") +
                 attribute("src", m_documents.front().second) + QStringLiteral("/></navPoint>\n");
        depth = 1;
    }
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\"") +
           attribute("xml:lang", languageTag(m_book.language)) +
           QStringLiteral(">\n<head>\n<meta name=\"dtb:uid\"") +
           attribute("content", identifier()) + QStringLiteral("/>\n<meta name=\"dtb:depth\"") +
           attribute("content", QString::number(std::max(1, depth))) +
           QStringLiteral(
               "/>\n<meta name=\"dtb:totalPageCount\" content=\"0\"/>\n"
               "<meta name=\"dtb:maxPageNumber\" content=\"0\"/>\n</head>\n<docTitle><text>") +
           xmlEscaped(m_title) + QStringLiteral("</text></docTitle>\n<navMap>\n") + points +
           QStringLiteral("</navMap>\n</ncx>\n");
}

QString EpubWriter::identifier() const {
    if (isIsbn(m_book.epub.isbn)) {
        QString isbn;
        for (const QChar ch : m_book.epub.isbn) {
            if (ch.isDigit() || ch == u'X' || ch == u'x') {
                isbn += ch.toUpper();
            }
        }
        return QStringLiteral("urn:isbn:") + isbn;
    }
    QString own = m_book.epub.identifier.trimmed();
    if (const QUuid uuid(own); !uuid.isNull()) {
        return QStringLiteral("urn:uuid:") + uuid.toString(QUuid::WithoutBraces);
    }
    if (!own.isEmpty()) {
        return own;
    }
    // Without one: the same for the same title and author
    static const QUuid urlNamespace(QStringLiteral("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}"));
    return QStringLiteral("urn:uuid:") +
           QUuid::createUuidV5(urlNamespace, m_title + QLatin1Char('\n') + m_book.author)
               .toString(QUuid::WithoutBraces);
}

void EpubWriter::addCover() {
    const QString path = m_book.epub.coverImage;
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    QByteArray data = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    QBuffer buffer(&data);
    QImageReader reader(&buffer);
    QByteArray format = reader.format().toLower();
    const QImage image = QImage::fromData(data);
    if (image.isNull()) {
        m_coverMissing = true;
        return;
    }
    if (format != "jpeg" && format != "png" && format != "gif") {
        // Not a picture every reader shows: as PNG
        data.clear();
        QBuffer png(&data);
        png.open(QIODevice::WriteOnly);
        image.save(&png, "PNG");
        format = "png";
    }
    const QString extension =
        format == "jpeg" ? QStringLiteral("jpg") : QString::fromLatin1(format);
    const QString picture = QStringLiteral("images/cover.") + extension;
    m_files.emplace_back(picture, data);
    m_resources.push_back({QStringLiteral("cover-image"), picture,
                           QStringLiteral("image/") + QString::fromLatin1(format),
                           QStringLiteral("cover-image")});
    m_coverFile = QStringLiteral("text/cover.xhtml");
    const QString body = QStringLiteral("<section epub:type=\"cover\" class=\"cover\">\n<img") +
                         attribute("src", QStringLiteral("../") + picture) +
                         attribute("alt", m_title) +
                         QStringLiteral(" role=\"doc-cover\"/>\n</section>\n");
    m_files.emplace_back(m_coverFile, document(m_title, body, m_coverFile).toUtf8());
    m_documents.emplace_back(QStringLiteral("cover"), m_coverFile);
}

void EpubWriter::addFonts() {
    if (!m_book.epub.embedFont) {
        return;
    }
    const QString family = m_book.fontFamily.isEmpty() ? QFont().family() : m_book.fontFamily;
    // A font of the system's own windows (".AppleSystemUIFont" of macOS) is not for books: its
    // licence keeps it in the system, and its file names another family
    if (QFontDatabase::isPrivateFamily(family)) {
        m_fontNotEmbedded = true;
        return;
    }
    struct Variant {
        bool bold;
        bool italic;
        const char* name;
    };
    const std::array<Variant, 4> variants = {{{false, false, "regular"},
                                              {true, false, "bold"},
                                              {false, true, "italic"},
                                              {true, true, "bold-italic"}}};
    QSet<QByteArray> written;
    for (const Variant& variant : variants) {
        QFont font(family);
        font.setWeight(variant.bold ? QFont::Bold : QFont::Normal);
        font.setItalic(variant.italic);
        const QRawFont raw = QRawFont::fromFont(font);
        const bool regular = !variant.bold && !variant.italic;
        if (!raw.isValid() || raw.familyName().compare(family, Qt::CaseInsensitive) != 0 ||
            (variant.bold && raw.weight() < QFont::DemiBold) ||
            (variant.italic && raw.style() == QFont::StyleNormal)) {
            if (regular) {
                m_fontNotEmbedded = true;  // the font is not on the computer: the reader's own
                return;
            }
            continue;  // drawn from another of the family's, as the reader will
        }
        // The licence of the font: the regular one's is the family's
        const QByteArray os2 = raw.fontTable("OS/2");
        const quint16 permissions =
            os2.size() >= 10 ? qFromBigEndian<quint16>(os2.constData() + 8) : 0xFFFF;
        if ((permissions & FONT_NOT_EMBEDDABLE) != 0) {
            m_fontNotEmbedded = true;
            if (regular) {
                return;
            }
            continue;
        }
        bool cff = false;
        const QByteArray data = fontFileOf(raw, &cff);
        if (data.isEmpty() || written.contains(data)) {
            if (regular) {
                m_fontNotEmbedded = true;
                return;
            }
            continue;
        }
        written.insert(data);
        const QString file = QStringLiteral("fonts/") + xmlName(QString(), family) +
                             QLatin1Char('-') + QLatin1String(variant.name) +
                             (cff ? QStringLiteral(".otf") : QStringLiteral(".ttf"));
        m_files.emplace_back(file, data);
        m_resources.push_back({QStringLiteral("font-") + QLatin1String(variant.name), file,
                               cff ? QStringLiteral("font/otf") : QStringLiteral("font/ttf"),
                               QString()});
        m_fontCss += QStringLiteral("@font-face { font-family: ") + cssString(family) +
                     QStringLiteral("; font-weight: ") +
                     (variant.bold ? QStringLiteral("bold") : QStringLiteral("normal")) +
                     QStringLiteral("; font-style: ") +
                     (variant.italic ? QStringLiteral("italic") : QStringLiteral("normal")) +
                     QStringLiteral("; src: url(\"../") + file + QStringLiteral("\"); }\n");
    }
}

QString EpubWriter::packageDocument() const {
    const QString language = languageTag(m_book.language);
    QString metadata = QStringLiteral("<dc:identifier id=\"book-id\">") + xmlEscaped(identifier()) +
                       QStringLiteral("</dc:identifier>\n<dc:title>") + xmlEscaped(m_title) +
                       QStringLiteral("</dc:title>\n");
    if (!m_book.author.trimmed().isEmpty()) {
        metadata += QStringLiteral("<dc:creator>") + xmlEscaped(m_book.author.simplified()) +
                    QStringLiteral("</dc:creator>\n");
    }
    metadata += QStringLiteral("<dc:language>") + language + QStringLiteral("</dc:language>\n");
    if (!m_book.epub.description.trimmed().isEmpty()) {
        metadata += QStringLiteral("<dc:description>") +
                    xmlEscaped(m_book.epub.description.trimmed()) +
                    QStringLiteral("</dc:description>\n");
    }
    if (!m_book.epub.publisher.trimmed().isEmpty()) {
        metadata += QStringLiteral("<dc:publisher>") +
                    xmlEscaped(m_book.epub.publisher.simplified()) +
                    QStringLiteral("</dc:publisher>\n");
    }
    const QDateTime modified =
        m_book.epub.modified.isValid() ? m_book.epub.modified : QDateTime::currentDateTimeUtc();
    metadata += QStringLiteral("<meta property=\"dcterms:modified\">") +
                modified.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")) +
                QStringLiteral("</meta>\n");
    if (!m_coverFile.isEmpty()) {
        metadata += QStringLiteral("<meta name=\"cover\" content=\"cover-image\"/>\n");
    }
    // What the book offers a reader who does not see, or does not see well
    const bool pictures = m_imagesWritten > 0 || !m_coverFile.isEmpty();
    const auto meta = [&metadata](const char* property, const QString& value) {
        metadata += QStringLiteral("<meta property=\"schema:") + QLatin1String(property) +
                    QStringLiteral("\">") + value + QStringLiteral("</meta>\n");
    };
    meta("accessMode", QStringLiteral("textual"));
    if (pictures) {
        meta("accessMode", QStringLiteral("visual"));
    }
    meta("accessModeSufficient",
         m_imagesWithoutText ? QStringLiteral("textual,visual") : QStringLiteral("textual"));
    meta("accessibilityFeature", QStringLiteral("structuralNavigation"));
    meta("accessibilityFeature", QStringLiteral("tableOfContents"));
    meta("accessibilityFeature", QStringLiteral("readingOrder"));
    if (m_imagesWritten > 0 && !m_imagesWithoutText) {
        meta("accessibilityFeature", QStringLiteral("alternativeText"));
    }
    meta("accessibilityHazard", QStringLiteral("none"));

    QString manifest = QStringLiteral(
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" "
        "properties=\"nav\"/>\n"
        "<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n"
        "<item id=\"css\" href=\"styles/book.css\" media-type=\"text/css\"/>\n");
    for (const auto& [id, file] : m_documents) {
        if (id != QLatin1String("nav")) {
            manifest += QStringLiteral("<item") + attribute("id", id) + attribute("href", file) +
                        QStringLiteral(" media-type=\"application/xhtml+xml\"/>\n");
        }
    }
    for (const Resource& resource : m_resources) {
        manifest += QStringLiteral("<item") + attribute("id", resource.id) +
                    attribute("href", resource.href) + attribute("media-type", resource.mediaType) +
                    (resource.properties.isEmpty() ? QString()
                                                   : attribute("properties", resource.properties)) +
                    QStringLiteral("/>\n");
    }
    QString spine;
    for (const auto& [id, file] : m_documents) {
        spine += QStringLiteral("<itemref") + attribute("idref", id) + QStringLiteral("/>\n");
    }
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
               "unique-identifier=\"book-id\"") +
           attribute("xml:lang", language) +
           QStringLiteral(">\n<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n") +
           metadata + QStringLiteral("</metadata>\n<manifest>\n") + manifest +
           QStringLiteral("</manifest>\n<spine toc=\"ncx\">\n") + spine +
           QStringLiteral("</spine>\n</package>\n");
}

std::vector<std::pair<QString, QByteArray>> EpubWriter::build(ExportResult* result) {
    prepare();
    addFonts();
    addCover();
    const size_t count = m_book.items.size();
    for (size_t index = 0; index < count; ++index) {
        const ExportItem& item = m_book.items[index];
        if (static_cast<int>(index) == m_contentsItem) {
            m_documents.emplace_back(QStringLiteral("nav"), QStringLiteral("nav.xhtml"));
        } else if (item.kind != ExportItemKind::Group || m_withTitle[index]) {
            // A part without its title has nothing to show
            m_files.emplace_back(fileOf(index), itemDocument(index).toUtf8());
            m_documents.emplace_back(
                QStringLiteral("item-%1").arg(index + 1, 3, 10, QLatin1Char('0')), fileOf(index));
        }
        if (m_progress) {
            m_progress(static_cast<int>(TEXTS_SHARE * (index + 1) / std::max<size_t>(1, count)));
        }
    }
    if (!m_bookEndnotes.empty()) {
        m_file = QStringLiteral("text/endnotes.xhtml");
        const QString body = notesOf(m_bookEndnotes);
        m_files.emplace_back(m_file, document(m_title, body, m_file).toUtf8());
        m_documents.emplace_back(QStringLiteral("endnotes"), m_file);
    }
    m_file = QStringLiteral("nav.xhtml");
    m_files.emplace_back(m_file, navDocument().toUtf8());
    m_files.emplace_back(QStringLiteral("styles/book.css"), styleCss().toUtf8());
    m_files.emplace_back(QStringLiteral("toc.ncx"), ncx().toUtf8());
    m_files.emplace_back(QStringLiteral("content.opf"), packageDocument().toUtf8());

    // The archive: its type first and as it is, the container, the publication
    std::vector<std::pair<QString, QByteArray>> archive;
    archive.emplace_back(QStringLiteral("mimetype"), QByteArray("application/epub+zip"));
    archive.emplace_back(
        QStringLiteral("META-INF/container.xml"),
        QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<container version=\"1.0\" "
                   "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
                   "<rootfiles>\n<rootfile full-path=\"OEBPS/content.opf\" "
                   "media-type=\"application/oebps-package+xml\"/>\n</rootfiles>\n</container>\n"));
    for (auto& [name, data] : m_files) {
        archive.emplace_back(QStringLiteral("OEBPS/") + name, std::move(data));
    }
    if (result) {
        result->images = m_imagesWritten;
        result->missingImages = m_missingImages;
        result->coverMissing = m_coverMissing;
        result->fontNotEmbedded = m_fontNotEmbedded;
    }
    return archive;
}

}  // namespace

bool isIsbn(const QString& text) {
    QList<int> digits;
    const QString trimmed = text.trimmed();
    for (qsizetype i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch.isDigit() && ch.unicode() < 0x80) {
            digits << ch.digitValue();
        } else if ((ch == u'X' || ch == u'x') && i + 1 == trimmed.size()) {
            digits << 10;  // the check digit of an ISBN of 10 digits
        } else if (ch != u'-' && ch != u' ') {
            return false;
        }
    }
    if (digits.size() == 13) {
        // 978 or 979, the digits by 1 and 3 in turn: a sum divisible by 10
        int sum = 0;
        for (qsizetype i = 0; i < digits.size(); ++i) {
            if (digits.at(i) == 10) {
                return false;
            }
            sum += digits.at(i) * (i % 2 == 0 ? 1 : 3);
        }
        return sum % 10 == 0;
    }
    if (digits.size() == 10) {
        // The digits by 10, 9... 1: a sum divisible by 11; X (10) only as the last one
        int sum = 0;
        for (qsizetype i = 0; i < digits.size(); ++i) {
            if (digits.at(i) == 10 && i + 1 < digits.size()) {
                return false;
            }
            sum += digits.at(i) * static_cast<int>(10 - i);
        }
        return sum % 11 == 0;
    }
    return false;
}

bool exportEpub(const BookExport& book, const QString& path, const ExportProgress& progress,
                ExportResult* result, ExportProblem* problem) {
    ExportProblem ignored;
    ExportProblem* reported = problem ? problem : &ignored;
    *reported = ExportProblem();
    ExportResult written;
    EpubWriter writer(book, QFileInfo(path).completeBaseName(), progress);
    const QByteArray data = zipped(writer.build(&written), reported);
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
