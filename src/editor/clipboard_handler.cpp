/// @file clipboard_handler.cpp
/// @brief Clipboard operations implementation (OpenSpec #00042 Phase 4.13-4.16)

#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_tables.h>
#include "book_editor_internal.h"
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QClipboard>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QVariant>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief CSS for the inline style attributes (font, size, color, bg, caps, spacing) of a KML
///        tag
QString inlineStyleCss(const QXmlStreamAttributes& attrs)
{
    QStringList css;
    if (attrs.hasAttribute(QStringLiteral("font"))) {
        QString family = attrs.value(QStringLiteral("font")).toString();
        family.remove(QLatin1Char('\'')).remove(QLatin1Char(';'));
        if (!family.isEmpty()) {
            css << QStringLiteral("font-family:'%1'").arg(family);
        }
    }
    if (attrs.hasAttribute(QStringLiteral("size"))) {
        bool ok = false;
        const double size = attrs.value(QStringLiteral("size")).toDouble(&ok);
        if (ok && size > 0) {
            css << QStringLiteral("font-size:%1pt").arg(size);
        }
    }
    if (attrs.hasAttribute(QStringLiteral("color"))) {
        const QColor color(attrs.value(QStringLiteral("color")).toString());
        if (color.isValid()) {
            css << QStringLiteral("color:") + color.name();
        }
    }
    if (attrs.hasAttribute(QStringLiteral("bg"))) {
        const QColor color(attrs.value(QStringLiteral("bg")).toString());
        if (color.isValid()) {
            css << QStringLiteral("background-color:") + color.name();
        }
    }
    const QStringView caps = attrs.value(QStringLiteral("caps"));
    if (caps == u"small") {
        css << QStringLiteral("font-variant:small-caps");
    } else if (caps == u"all") {
        css << QStringLiteral("text-transform:uppercase");
    }
    if (attrs.hasAttribute(QStringLiteral("spacing"))) {
        bool ok = false;
        const double spacing = attrs.value(QStringLiteral("spacing")).toDouble(&ok);
        if (ok && spacing != 0.0) {
            css << QStringLiteral("letter-spacing:%1pt").arg(spacing);
        }
    }
    return css.join(QLatin1Char(';'));
}

/// @brief The rows or columns a cell of KML spans (its rowspan or colspan attribute)
int spanOf(const QXmlStreamAttributes& attrs, const QString& name)
{
    bool ok = false;
    const int span = attrs.value(name).toString().trimmed().toInt(&ok);
    return ok ? std::clamp(span, 1, MAX_TABLE_SPAN) : 1;
}

/// @brief A document that loads nothing an HTML text refers to: a style sheet or an image
///        of a pasted page may be a file of the computer or of a network share
class ResourcelessDocument : public QTextDocument {
protected:
    QVariant loadResource(int /*type*/, const QUrl& /*name*/) override { return {}; }
};

/// @brief A character the matching of a plain text with its HTML passes over: white space,
///        the marks of images and tables, and the invisible ones (a soft hyphen)
bool skippedInMatching(QChar ch)
{
    const char16_t code = ch.unicode();
    return ch.isSpace() || code == QChar::ObjectReplacementCharacter || code == 0xFDD0 ||
           code == 0xFDD1 || ch.category() == QChar::Other_Format;
}

/// @brief A character of the HTML's text that ends a paragraph: a block's end, or a table's
///        start or end (QTextBeginningOfFrame, QTextEndOfFrame)
bool endsParagraph(QChar ch)
{
    const char16_t code = ch.unicode();
    return code == QChar::ParagraphSeparator || code == 0xFDD0 || code == 0xFDD1;
}

/// @brief The white space between two paragraphs of a text with no more line ends than
///        @p kept: the last ones, with the white space after them (a CR LF is one line end)
///
/// The browsers leave a blank line between two paragraphs in the plain text, which their
/// HTML does not have; @p kept is at least 1.
QString withLastLineEnds(const QString& space, int kept)
{
    qsizetype keepFrom = space.size();
    int lineEnds = 0;
    for (qsizetype i = space.size() - 1; i >= 0; --i) {
        const QChar ch = space.at(i);
        if (ch == u'\r' && i + 1 < space.size() && space.at(i + 1) == u'\n') {
            keepFrom = i;  // the CR of a CR LF kept
        } else if (ch == u'\n' || ch == u'\r') {
            if (++lineEnds > kept) {
                return space.mid(keepFrom);
            }
            keepFrom = i;
        }
    }
    return space;
}

// =============================================================================
// Scene breaks, lines and page breaks for other programs
// =============================================================================

/// @brief A horizontal line in plain text
const QString HORIZONTAL_LINE_TEXT = QString(10, QChar(0x2500));  // ──────────

/// @brief The text of a scene break of a look ("asterisks", "asterism", "fleuron", "line",
///        "blank"; the asterisks for any other)
QString sceneBreakText(const QString& style)
{
    if (style == QStringLiteral("asterism")) {
        return QString(QChar(0x2042));  // ⁂
    }
    if (style == QStringLiteral("fleuron")) {
        return QString(QChar(0x2766));  // ❦
    }
    if (style == QStringLiteral("line")) {
        return QString(3, QChar(0x2500));  // ───, a short line
    }
    if (style == QStringLiteral("blank")) {
        return QString();
    }
    return QStringLiteral("* * *");
}

/// @brief Whether a paragraph of KML in a style is a quote in HTML
bool isQuoteStyle(const QString& style)
{
    return style == QStringLiteral("quote") || style == QStringLiteral("block_quote");
}

/// @brief The level of a KML heading element (h1-h3), 0 for any other element
int headingLevelOfTag(const QString& tag)
{
    if (tag.size() == 2 && tag.at(0) == u'h' && tag.at(1) >= u'1' && tag.at(1) <= u'3') {
        return tag.at(1).digitValue();
    }
    return 0;
}

// =============================================================================
// Reading HTML of other programs
// =============================================================================

/// @brief The block indent the marks give a quote: Qt's reading of HTML keeps no class
///        names (Word's quote styles), and a blockquote only on its first paragraph
constexpr int QUOTE_MARK = 9701;

/// @brief The font family the marks give a page break of Word, a <br> with
///        page-break-before: Qt reads the <br> as a line break
constexpr const char* PAGE_BREAK_MARK = "kalahari-page-break";

/// @brief The start of the font family the marks give the number or bullet of a list item of
///        Word: Word writes a list item as a paragraph with its list and level in its style
///        (mso-list:l0 level1 lfo1) and its number in a span of its own (mso-list:Ignore).
///        The span gets the family "kalahari-list-<lfo>-<level>": Qt's reading keeps a font
///        family, and not the styles of Word.
constexpr const char* WORD_LIST_MARK = "kalahari-list-";

/// @brief Word's list items with the marks of their numbers (WORD_LIST_MARK)
QString withWordListMarks(const QString& html)
{
    static const QRegularExpression paragraph(
        QStringLiteral("<p\\b[^>]*mso-list:\\s*l\\d+\\s+level(\\d+)\\s+lfo(\\d+)[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ignore(QStringLiteral("mso-list:\\s*Ignore"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString marked;
    qsizetype copied = 0;
    for (auto it = paragraph.globalMatch(html); it.hasNext();) {
        const QRegularExpressionMatch item = it.next();
        if (item.capturedStart() < copied) {
            continue;
        }
        const qsizetype end =
            html.indexOf(QStringLiteral("</p"), item.capturedEnd(), Qt::CaseInsensitive);
        const QRegularExpressionMatch number = ignore.match(html, item.capturedEnd());
        if (!number.hasMatch() || (end >= 0 && number.capturedStart() > end)) {
            continue;  // a paragraph of a list without its number
        }
        marked += html.mid(copied, number.capturedStart() - copied);
        marked += QStringLiteral("font-family:%1%2-%3")
                      .arg(QLatin1String(WORD_LIST_MARK), item.captured(2), item.captured(1));
        copied = number.capturedEnd();
    }
    if (copied == 0) {
        return html;
    }
    return marked + html.mid(copied);
}

/// @brief The HTML with the marks of what Qt's reading of it does not keep: the paragraphs
///        of quotes (in a blockquote, in Word's quote styles), Word's page breaks and the
///        numbers of Word's list items
/// @param styleSheets Qt reads the HTML's style sheets too (the source's styles, as Word
///        writes them): not the ones Qt's reading leaves out otherwise
QString withMarks(const QString& html, bool styleSheets)
{
    const QString marks =
        QStringLiteral("<style>blockquote, blockquote *, .MsoQuote, .MsoIntenseQuote "
                       "{-qt-block-indent: %1} br[style*=\"page-break-before:always\"], "
                       "br[style*=\"page-break-before: always\"] {font-family: %2}</style>")
            .arg(QUOTE_MARK)
            .arg(QLatin1String(PAGE_BREAK_MARK));
    QString marked = withWordListMarks(html);
    QString sheets;
    // Of HTML with <!--StartFragment--> (Word, the browsers on Windows) Qt reads only what
    // follows it, up to <!--EndFragment-->
    const QString fragmentStart = QStringLiteral("<!--StartFragment-->");
    if (styleSheets) {
        // Qt leaves out a style sheet in an HTML comment, as Word writes its styles; CSS
        // reads it without the comment's marks. A style sheet of the head goes after the
        // fragment's start.
        static const QRegularExpression styleSheet(
            QStringLiteral("(<style\\b[^>]*>)(.*?)(</style\\s*>)"),
            QRegularExpression::CaseInsensitiveOption |
                QRegularExpression::DotMatchesEverythingOption);
        const QString source = marked;
        const qsizetype start = source.indexOf(fragmentStart);
        marked.clear();
        qsizetype copied = 0;
        for (auto it = styleSheet.globalMatch(source); it.hasNext();) {
            const QRegularExpressionMatch match = it.next();
            QString sheet = match.captured(2);
            sheet.remove(QStringLiteral("<!--"));
            sheet.remove(QStringLiteral("-->"));
            sheet = match.captured(1) + sheet + match.captured(3);
            marked += source.mid(copied, match.capturedStart() - copied);
            if (match.capturedStart() < start) {
                sheets += sheet;
            } else {
                marked += sheet;
            }
            copied = match.capturedEnd();
        }
        marked += source.mid(copied);
    }
    const qsizetype start = marked.indexOf(fragmentStart);
    if (start < 0) {
        return sheets + marks + marked;
    }
    marked.insert(start + fragmentStart.size(), sheets + marks);
    return marked;
}

/// @brief The family of a CSS font list that a computer can have: the first that is not a
///        generic one (serif, monospace...) or a browser's name of the system's font
QString sourceFontFamily(const QStringList& families)
{
    static const QStringList generic = {
        QStringLiteral("serif"),         QStringLiteral("sans-serif"),
        QStringLiteral("monospace"),     QStringLiteral("cursive"),
        QStringLiteral("fantasy"),       QStringLiteral("system-ui"),
        QStringLiteral("ui-serif"),      QStringLiteral("ui-sans-serif"),
        QStringLiteral("ui-monospace"),  QStringLiteral("ui-rounded"),
        QStringLiteral("math"),          QStringLiteral("emoji"),
        QStringLiteral("fangsong"),      QStringLiteral("BlinkMacSystemFont"),
        QLatin1String(PAGE_BREAK_MARK)};
    for (const QString& family : families) {
        const QString name = family.trimmed();
        if (!name.isEmpty() && !name.startsWith(u'-') &&
            !name.startsWith(QLatin1String(WORD_LIST_MARK)) &&
            !generic.contains(name, Qt::CaseInsensitive)) {
            return name;
        }
    }
    return QString();
}

/// @brief The number or bullet a list item starts with: a list in a list has the circle and
///        then the square of a browser
QString listMarker(const QTextList& list, const QTextBlock& block)
{
    switch (list.format().style()) {
        case QTextListFormat::ListDisc:
            if (list.format().indent() == 2) {
                return QString(QChar(0x25E6));  // ◦
            }
            if (list.format().indent() > 2) {
                return QString(QChar(0x25AA));  // ▪
            }
            return QString(QChar(0x2022));  // •
        case QTextListFormat::ListCircle:
            return QString(QChar(0x25E6));  // ◦
        case QTextListFormat::ListSquare:
            return QString(QChar(0x25AA));  // ▪
        default:
            return list.itemText(block);  // "1.", "a.", "i."...
    }
}

/// @brief The format of a level of a list read from HTML: the format of a QTextList
ListLevel listLevelOf(const QTextListFormat& format, ListKind kind, int level)
{
    ListLevel result = ListDefinition::defaultLevel(kind, level);
    switch (format.style()) {
        case QTextListFormat::ListDisc:
            // A list in a list has the circle and then the square of a browser
            result.format = ListNumberFormat::Bullet;
            result.bullet = QString(QChar(format.indent() == 2  ? BULLET_CIRCLE
                                          : format.indent() > 2 ? BULLET_SQUARE
                                                                : BULLET_DISC));
            break;
        case QTextListFormat::ListCircle:
            result.format = ListNumberFormat::Bullet;
            result.bullet = QString(QChar(BULLET_CIRCLE));
            break;
        case QTextListFormat::ListSquare:
            result.format = ListNumberFormat::Bullet;
            result.bullet = QString(QChar(BULLET_SQUARE));
            break;
        case QTextListFormat::ListLowerAlpha:
            result.format = ListNumberFormat::LowerAlpha;
            break;
        case QTextListFormat::ListUpperAlpha:
            result.format = ListNumberFormat::UpperAlpha;
            break;
        case QTextListFormat::ListLowerRoman:
            result.format = ListNumberFormat::LowerRoman;
            break;
        case QTextListFormat::ListUpperRoman:
            result.format = ListNumberFormat::UpperRoman;
            break;
        default:
            result.format = ListNumberFormat::Decimal;
            break;
    }
    if (!result.isBullet()) {
        const QString suffix = format.numberSuffix();
        result.suffix = suffix == QStringLiteral(")") ? suffix : QStringLiteral(".");
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        result.start = std::clamp(format.start(), MIN_LIST_NUMBER, MAX_LIST_NUMBER);
#endif
    }
    return result;
}

/// @brief The format of a level of Word's list from the number or bullet of an item, and the
///        number: "1." "a)" "iv." or a bullet of Symbol, Wingdings or Courier New
/// @param known The format the level had at an item before, which tells "i." of roman
///        numbers from "i." of letters
std::pair<ListLevel, int> wordListLevelOf(const QString& mark, ListKind kind, int level,
                                          const std::optional<ListLevel>& known)
{
    ListLevel result = known.value_or(ListDefinition::defaultLevel(kind, level));
    QString core = mark.trimmed();
    QString suffix;
    if (core.endsWith(u'.') || core.endsWith(u')')) {
        suffix = core.right(1);
        core.chop(1);
    }
    if (core.startsWith(u'(')) {
        core.remove(0, 1);
    }
    const auto numbered = [&](ListNumberFormat format) -> std::optional<int> {
        if (const std::optional<int> number = listNumberFromText(core, format);
            number && listNumberText(*number, format) == core) {
            return number;
        }
        return std::nullopt;
    };
    if (!core.isEmpty() && !suffix.isEmpty()) {
        // Roman numbers where the level had them, or where it had no letters and the
        // number looks roman: "i", "ii", "iv" (a single "v" or "x" is a letter)
        std::vector<ListNumberFormat> formats = {ListNumberFormat::Decimal};
        const bool romanBefore = known && (known->format == ListNumberFormat::LowerRoman ||
                                           known->format == ListNumberFormat::UpperRoman);
        const bool lettersBefore = known && (known->format == ListNumberFormat::LowerAlpha ||
                                             known->format == ListNumberFormat::UpperAlpha);
        const bool romanLooks = core.size() > 1 || core.compare(u"i", Qt::CaseInsensitive) == 0;
        if (romanBefore || (!lettersBefore && romanLooks)) {
            formats.insert(formats.end(), {ListNumberFormat::LowerRoman, ListNumberFormat::UpperRoman,
                                           ListNumberFormat::LowerAlpha, ListNumberFormat::UpperAlpha});
        } else {
            formats.insert(formats.end(), {ListNumberFormat::LowerAlpha, ListNumberFormat::UpperAlpha,
                                           ListNumberFormat::LowerRoman, ListNumberFormat::UpperRoman});
        }
        for (const ListNumberFormat format : formats) {
            if (const std::optional<int> number = numbered(format)) {
                result.format = format;
                result.suffix = suffix;
                result.bullet.clear();
                return {result, *number};
            }
        }
    }
    // A bullet: Word writes the characters of Symbol and Wingdings in their fonts
    static const QHash<QString, QChar> bullets = {
        {QStringLiteral("·"), QChar(BULLET_DISC)},   {QStringLiteral("•"), QChar(BULLET_DISC)},
        {QStringLiteral("o"), QChar(BULLET_CIRCLE)}, {QStringLiteral("◦"), QChar(BULLET_CIRCLE)},
        {QStringLiteral("§"), QChar(BULLET_SQUARE)}, {QStringLiteral("▪"), QChar(BULLET_SQUARE)},
        {QStringLiteral("-"), QChar(BULLET_DASH)},   {QStringLiteral("–"), QChar(BULLET_DASH)},
        {QStringLiteral("ü"), QChar(BULLET_CHECK)},  {QStringLiteral("✓"), QChar(BULLET_CHECK)},
        {QStringLiteral("Ø"), QChar(BULLET_ARROW)},  {QStringLiteral("➢"), QChar(BULLET_ARROW)}};
    const QString bullet = mark.trimmed();
    result.format = ListNumberFormat::Bullet;
    result.suffix = QStringLiteral(".");
    result.bullet = bullets.contains(bullet) ? QString(bullets.value(bullet))
                    : bullet.isEmpty()       ? QString(QChar(BULLET_DISC))
                                             : bullet;
    return {result, -1};
}

/// @brief The text of a paragraph read from HTML, with the format of each character
struct PastedText {
    QString text;
    std::vector<QTextCharFormat> formats;  ///< One for each character of the text
    std::vector<qsizetype> pageBreaks;     ///< Where Word's page breaks were in the text
    QString wordMark;     ///< The number or bullet of Word's list item (WORD_LIST_MARK)
    int wordList = -1;    ///< Its list (lfo)
    int wordLevel = -1;   ///< Its level, from 0

    void append(QChar ch, const QTextCharFormat& format)
    {
        text += ch;
        formats.push_back(format);
    }

    void append(const QString& part, const QTextCharFormat& format)
    {
        for (const QChar ch : part) {
            append(ch, format);
        }
    }

    /// @brief The characters [from, to) of the text, with their formats
    PastedText mid(qsizetype from, qsizetype to) const
    {
        PastedText part;
        part.text = text.mid(from, to - from);
        part.formats.assign(formats.begin() + from, formats.begin() + to);
        return part;
    }

    /// @brief Whether a browser shows anything of the text: a character other than a
    ///        space, which HTML leaves out where nothing else shows
    bool shows() const
    {
        return std::any_of(text.cbegin(), text.cend(), [](QChar ch) { return ch != u' '; });
    }

    /// @brief Leave out what a browser does not show: the spaces at the start and the end of
    ///        each line, the line break that ends the text, and the no-break spaces that end
    ///        it (Word's and Kalahari's mark of an empty paragraph or last line)
    void trim()
    {
        std::vector<bool> kept(text.size(), true);
        qsizetype lineStart = 0;
        for (qsizetype i = 0; i <= text.size(); ++i) {
            if (i < text.size() && text.at(i) != QChar::LineSeparator) {
                continue;
            }
            for (qsizetype j = lineStart; j < i && text.at(j) == u' '; ++j) {
                kept[j] = false;
            }
            for (qsizetype j = i - 1; j >= lineStart && text.at(j) == u' '; --j) {
                kept[j] = false;
            }
            lineStart = i + 1;
        }
        keep(kept);

        if (text.endsWith(QChar(QChar::LineSeparator))) {
            text.chop(1);
            formats.pop_back();
        }
        while (!text.isEmpty() && (text.back() == u' ' || text.back() == QChar::Nbsp)) {
            text.chop(1);
            formats.pop_back();
        }
    }

private:
    void keep(const std::vector<bool>& kept)
    {
        QString keptText;
        std::vector<QTextCharFormat> keptFormats;
        for (qsizetype i = 0; i < text.size(); ++i) {
            if (kept[i]) {
                keptText += text.at(i);
                keptFormats.push_back(formats[i]);
            }
        }
        text = keptText;
        formats = std::move(keptFormats);
    }
};

/// @brief Whether XML, and so KML, can hold a character of a text read from HTML: not a
///        control character other than a tab, not a mark of Qt's document (an object, the
///        start or end of a frame), not U+FFFE or U+FFFF
bool holdable(QChar ch)
{
    const char16_t code = ch.unicode();
    return (code >= 0x20 || code == u'\t') && code != QChar::ObjectReplacementCharacter &&
           code != 0xFDD0 && code != 0xFDD1 && code != 0xFFFE && code != 0xFFFF;
}

/// @brief The KML of HTML that Qt has read into a document: its paragraphs and tables, as a
///        paste mode keeps them
class PastedKml {
public:
    /// @param inCell The KML of a cell of a table: only its text, without lines, page breaks
    ///        and tables (a table in it is its rows of text)
    explicit PastedKml(PasteMode mode, bool inCell = false)
        : m_mode(mode)
        , m_inCell(inCell)
        , m_cursor(&m_paragraphs)
    {
    }

    /// @brief Add the paragraphs and tables of a frame
    void addFrame(const QTextFrame& frame) { addContent(frame.begin()); }

    /// @brief Add the paragraphs of a cell of a table
    /// @param header The cell is in the table's header row, which is bold in the editor
    void addCell(const QTextTableCell& cell, bool header)
    {
        m_headerCell = header;
        addContent(cell.begin());
    }

    /// @brief The KML; empty without paragraphs and tables
    QString kml() const
    {
        const QString elements = body();
        return elements.isEmpty() ? QString()
                                  : QStringLiteral("<kml>") + elements + QStringLiteral("</kml>");
    }

    /// @brief The elements of the KML, without its root element; empty without paragraphs
    ///        and tables
    QString body() const
    {
        if (m_items.empty()) {
            return QString();
        }
        const KmlSerializer serializer;
        QString kml;
        QTextBlock block = m_paragraphs.begin();

        // The list elements open, from the first level, as KmlSerializer writes them
        struct OpenList {
            int level = 0;
            ListLevel format;
            bool itemOpen = false;
        };
        std::vector<OpenList> openLists;
        int openListId = 0;
        const auto closeList = [&]() {
            if (openLists.back().itemOpen) {
                kml += QStringLiteral("</li>");
            }
            kml += QStringLiteral("</") + kmlListElementOf(openLists.back().format) +
                   QLatin1Char('>');
            openLists.pop_back();
        };
        const auto closeLists = [&]() {
            while (!openLists.empty()) {
                closeList();
            }
        };

        for (const Item& item : m_items) {
            if (!item.list) {
                closeLists();
            }
            if (!item.paragraph) {
                kml += item.content.isEmpty()
                           ? QLatin1Char('<') + item.element + QStringLiteral("/>")
                           : item.content;
                continue;
            }
            if (item.list) {
                const ListItem& listItem = *item.list;
                const ListLevel& format = listItem.levelFormat();
                if (!openLists.empty() && openListId != listItem.id) {
                    closeLists();
                }
                while (!openLists.empty() && openLists.back().level > listItem.level) {
                    closeList();
                }
                if (!openLists.empty() && openLists.back().level == listItem.level) {
                    if (openLists.back().format != format) {
                        closeList();
                    } else if (openLists.back().itemOpen) {
                        kml += QStringLiteral("</li>");
                        openLists.back().itemOpen = false;
                    }
                }
                for (int level = openLists.empty() ? 0 : openLists.back().level + 1;
                     level <= listItem.level; ++level) {
                    const ListLevel& levelFormat =
                        listItem.definition.levels[static_cast<size_t>(level)];
                    kml += QLatin1Char('<') + kmlListElementOf(levelFormat) +
                           listLevelToKml(levelFormat, level) + QLatin1Char('>');
                    openLists.push_back({level, levelFormat, false});
                }
                openListId = listItem.id;
                kml += QStringLiteral("<li") + item.attributes;
                if (const std::optional<int> value = listItem.value) {
                    kml += QStringLiteral(" value=\"") + QString::number(*value) + QLatin1Char('"');
                }
                kml += QLatin1Char('>') + serializer.blockToKml(block);
                openLists.back().itemOpen = true;
            } else {
                kml += QLatin1Char('<') + item.element + item.attributes + QLatin1Char('>') +
                       serializer.blockToKml(block) + QStringLiteral("</") + item.element +
                       QLatin1Char('>');
            }
            block = block.next();
        }
        closeLists();
        return kml;
    }

private:
    /// @brief An element of the KML: a paragraph, whose text is the next block of
    ///        m_paragraphs, an empty element (a line, a page break), or a table
    struct Item {
        QString element;
        QString attributes;
        bool paragraph = false;
        std::optional<ListItem> list;  ///< A list item: its list, level and number
        QString content;               ///< The KML of a table, written whole
    };

    bool keepsStructure() const { return m_mode != PasteMode::Text; }

    /// @brief Add the paragraphs and tables from an iterator of a frame or a cell
    void addContent(QTextFrame::iterator it)
    {
        for (; !it.atEnd(); ++it) {
            if (QTextFrame* child = it.currentFrame()) {
                if (auto* table = qobject_cast<QTextTable*>(child)) {
                    addTable(*table);
                } else {
                    addFrame(*child);
                }
            } else if (it.currentBlock().isValid()) {
                addBlock(it.currentBlock());
            }
        }
    }

    void addBlock(const QTextBlock& block)
    {
        const QTextBlockFormat format = block.blockFormat();
        if (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore) {
            addPageBreak();
        }
        if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            if (keepsStructure() && !m_inCell) {
                m_items.push_back(
                    {QStringLiteral("hr"), QString(), false, std::nullopt, QString()});
            }
        } else {
            const int level = keepsStructure() ? std::min(format.headingLevel(), 3) : 0;
            QString element = QStringLiteral("p");
            QString attributes;
            if (level > 0) {
                element = QStringLiteral("h") + QString::number(level);
            } else if (keepsStructure() && format.indent() == QUOTE_MARK) {
                attributes = QStringLiteral(" style=\"quote\"");
            }
            attributes += alignmentAttribute(format);

            // Word's page breaks end a paragraph, which goes on after them
            const PastedText text = textOf(block, level > 0 || m_headerCell);
            const bool preformatted = format.nonBreakableLines();
            std::optional<ListItem> list = keepsStructure() ? listItemOf(block, text) : std::nullopt;
            if (list) {
                element = QStringLiteral("li");
                attributes = alignmentAttribute(format);
            }
            qsizetype from = 0;
            for (const qsizetype pageBreak : text.pageBreaks) {
                addParagraph(element, attributes, text.mid(from, pageBreak), false, list);
                addPageBreak();
                from = pageBreak;
                if (list) {
                    // The rest of the item, after the page, without its number
                    list->value.reset();
                    list->numbered = false;
                }
            }
            addParagraph(element, attributes, text.mid(from, text.text.size()), preformatted, list);
        }
        if (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysAfter) {
            addPageBreak();
        }
    }

    /// @brief Add a table: in PasteMode::Merge and PasteMode::Source a table of KML with its
    ///        columns, their widths, its header row and its merged cells, each cell with its
    ///        paragraphs as the mode keeps them; in PasteMode::Text, and in a cell, its rows
    ///        of text (addTableRows())
    void addTable(const QTextTable& table)
    {
        if (!keepsStructure() || m_inCell) {
            addTableRows(table);
            return;
        }
        const int headerRows = firstRowIsHeader(table) ? 1 : 0;
        QString kml = QStringLiteral("<table>");
        for (const double width : columnWidthsOf(table)) {
            kml += QStringLiteral("<col width=\"%1%\"/>")
                       .arg(QString::number(std::round(width * 100.0) / 100.0, 'g', 6));
        }
        for (int row = 0; row < table.rows(); ++row) {
            kml += QStringLiteral("<tr>");
            for (int column = 0; column < table.columns(); ++column) {
                const QTextTableCell cell = table.cellAt(row, column);
                if (cell.row() != row || cell.column() != column) {
                    continue;  // a part of a merged cell
                }
                const QString element =
                    row < headerRows ? QStringLiteral("th") : QStringLiteral("td");
                kml += QLatin1Char('<') + element;
                if (cell.columnSpan() > 1) {
                    kml += QStringLiteral(" colspan=\"%1\"").arg(cell.columnSpan());
                }
                if (cell.rowSpan() > 1) {
                    kml += QStringLiteral(" rowspan=\"%1\"").arg(cell.rowSpan());
                }
                PastedKml content(m_mode, true);
                content.addCell(cell, row < headerRows);
                const QString paragraphs = content.body();
                kml += QLatin1Char('>') +
                       (paragraphs.isEmpty() ? QStringLiteral("<p></p>") : paragraphs) +
                       QStringLiteral("</") + element + QLatin1Char('>');
            }
            kml += QStringLiteral("</tr>");
        }
        kml += QStringLiteral("</table>");
        m_items.push_back({QStringLiteral("table"), QString(), false, std::nullopt, kml});
    }

    /// @brief Whether the first row of a table is its header: in a thead, or of th cells,
    ///        whose paragraphs Qt centers both ways (Qt::AlignCenter), as no paragraph of a
    ///        page is
    static bool firstRowIsHeader(const QTextTable& table)
    {
        if (table.format().headerRowCount() > 0) {
            return true;
        }
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell cell = table.cellAt(0, column);
            const Qt::Alignment alignment =
                cell.firstCursorPosition().block().blockFormat().alignment();
            if ((alignment & Qt::AlignCenter) != Qt::AlignCenter) {
                return false;
            }
        }
        return true;
    }

    /// @brief The widths of a table's columns in percent, from those of the HTML: all of them
    ///        in percent or all in pixels; empty (equal columns) when one is not known
    static std::vector<double> columnWidthsOf(const QTextTable& table)
    {
        const QList<QTextLength> lengths = table.format().columnWidthConstraints();
        if (lengths.isEmpty() || lengths.size() != table.columns()) {
            return {};
        }
        const QTextLength::Type type = lengths.front().type();
        double sum = 0.0;
        for (const QTextLength& length : lengths) {
            if (length.type() != type || type == QTextLength::VariableLength ||
                length.rawValue() <= 0.0) {
                return {};
            }
            sum += length.rawValue();
        }
        std::vector<double> widths;
        for (const QTextLength& length : lengths) {
            widths.push_back(length.rawValue() * 100.0 / sum);
        }
        return widths;
    }

    /// @brief Add the rows of a table as text: each row a paragraph, its cells separated by
    ///        tabs, the paragraphs of a cell by line breaks
    void addTableRows(const QTextTable& table)
    {
        for (int row = 0; row < table.rows(); ++row) {
            PastedText text;
            for (int column = 0; column < table.columns(); ++column) {
                const QTextTableCell cell = table.cellAt(row, column);
                if (cell.row() != row || cell.column() != column) {
                    continue;  // a part of a merged cell
                }
                if (column > 0) {
                    text.append(QLatin1Char('\t'), QTextCharFormat());
                }
                const QTextBlock last = cell.lastCursorPosition().block();
                bool first = true;
                for (QTextBlock block = cell.firstCursorPosition().block(); block.isValid();
                     block = block.next()) {
                    PastedText line = textOf(block, false);
                    line.trim();
                    if (!first) {
                        text.append(QChar(QChar::LineSeparator), QTextCharFormat());
                    }
                    first = false;
                    text.text += line.text;
                    text.formats.insert(text.formats.end(), line.formats.begin(),
                                        line.formats.end());
                    if (block == last) {
                        break;
                    }
                }
            }
            addParagraph(QStringLiteral("p"), QString(), text, false);
        }
    }

    void addPageBreak()
    {
        // Two in a row (a paragraph's break after it, the next one's before it) are one
        if (keepsStructure() && !m_inCell && (m_items.empty() || m_items.back().paragraph ||
                                 m_items.back().element != QStringLiteral("page-break"))) {
            m_items.push_back(
                {QStringLiteral("page-break"), QString(), false, std::nullopt, QString()});
        }
    }

    /// @brief The list item a block of HTML is: an item of a list of HTML, or of Word's list
    ///        (PastedText::wordMark)
    std::optional<ListItem> listItemOf(const QTextBlock& block, const PastedText& text)
    {
        ListItem item;
        if (const QTextList* list = block.textList()) {
            // A list in an item of another one (indent 2 and more) is a level of that one
            const QTextListFormat format = list->format();
            item.level = std::clamp(format.indent() - 1, 0, LIST_LEVELS - 1);
            if (item.level == 0 || m_topListId == 0) {
                auto known = m_htmlListIds.find(list);
                if (known == m_htmlListIds.end()) {
                    known = m_htmlListIds.emplace(list, ++m_listCount).first;
                }
                m_topListId = known->second;
            }
            item.id = m_topListId;
            const ListKind kind = levelKindOf(format.style());
            ListDefinition& definition = definitionOf(item.id, item.level == 0 ? kind : std::optional<ListKind>());
            definition.levels[static_cast<size_t>(item.level)] =
                listLevelOf(format, definition.kind(), item.level);
            item.definition = definition;
            return item;
        }
        if (text.wordList < 0) {
            return std::nullopt;
        }
        // Word's list item: its format and number from its number or bullet
        item.level = std::clamp(text.wordLevel, 0, LIST_LEVELS - 1);
        auto known = m_wordListIds.find(text.wordList);
        if (known == m_wordListIds.end()) {
            known = m_wordListIds.emplace(text.wordList, ++m_listCount).first;
        }
        item.id = known->second;
        WordLevels& levels = m_wordLevels[item.id];
        const auto seen = levels.formats.find(item.level);
        const std::optional<ListLevel> before =
            seen != levels.formats.end() ? std::optional<ListLevel>(seen->second) : std::nullopt;
        const std::optional<ListKind> firstKind =
            m_definitions.count(item.id) > 0 ? std::nullopt
                                             : std::optional<ListKind>(ListKind::Numbered);
        ListDefinition& definition = definitionOf(item.id, firstKind);
        auto [format, number] = wordListLevelOf(text.wordMark, definition.kind(), item.level, before);
        // The items of the levels below start their numbers again
        for (auto it = levels.next.begin(); it != levels.next.end();) {
            it = it->first > item.level ? levels.next.erase(it) : std::next(it);
        }
        if (!before) {
            if (number >= 0) {
                format.start = number;
            }
            levels.formats[item.level] = format;
            definition.levels[static_cast<size_t>(item.level)] = format;
            if (item.level == 0) {
                // The kind of the list: that of its first level
                for (int level = 1; level < LIST_LEVELS; ++level) {
                    if (levels.formats.count(level) == 0) {
                        definition.levels[static_cast<size_t>(level)] =
                            ListDefinition::defaultLevel(format.isBullet() ? ListKind::Bulleted
                                                                           : ListKind::Numbered,
                                                         level);
                    }
                }
            }
        }
        const ListLevel& levelFormat = definition.levels[static_cast<size_t>(item.level)];
        if (number >= 0 && !levelFormat.isBullet()) {
            const auto next = levels.next.find(item.level);
            const int expected = next != levels.next.end() ? next->second : levelFormat.start;
            if (number != expected) {
                item.value = number;
            }
            levels.next[item.level] = number + 1;
        }
        item.definition = definition;
        return item;
    }

    /// @brief The kind of a level of a list of HTML
    static ListKind levelKindOf(QTextListFormat::Style style)
    {
        return style == QTextListFormat::ListDisc || style == QTextListFormat::ListCircle ||
                       style == QTextListFormat::ListSquare
                   ? ListKind::Bulleted
                   : ListKind::Numbered;
    }

    /// @brief The formats of the levels of a pasted list; a new list of @p kind (numbered
    ///        without one) gets the default formats of its kind
    ListDefinition& definitionOf(int id, std::optional<ListKind> kind)
    {
        auto known = m_definitions.find(id);
        if (known == m_definitions.end()) {
            known = m_definitions
                        .emplace(id, ListDefinition::ofKind(kind.value_or(ListKind::Numbered)))
                        .first;
        }
        return known->second;
    }

    /// @brief Add a paragraph of the text that a browser shows
    /// @param preformatted The text keeps its white space, and a line shows even when empty
    /// @param list The paragraph is a list item
    void addParagraph(const QString& element, const QString& attributes, PastedText text,
                      bool preformatted, const std::optional<ListItem>& list = std::nullopt)
    {
        if (!preformatted) {
            if (!text.shows()) {
                return;  // an empty element, or white space between elements
            }
            text.trim();
        }
        if (m_paragraphCount > 0) {
            m_cursor.insertBlock();
        }
        qsizetype runStart = 0;
        for (qsizetype i = 1; i <= text.text.size(); ++i) {
            if (i == text.text.size() || text.formats[i] != text.formats[runStart]) {
                m_cursor.insertText(text.text.mid(runStart, i - runStart),
                                    text.formats[runStart]);
                runStart = i;
            }
        }
        ++m_paragraphCount;
        m_items.push_back({element, attributes, true, list, QString()});
    }

    /// @brief The text of a block with the formats the mode keeps; a list item starts with
    ///        its number or bullet
    PastedText textOf(const QTextBlock& block, bool heading) const
    {
        PastedText text;
        if (QTextList* list = block.textList(); list != nullptr && !keepsStructure()) {
            text.append(listMarker(*list, block) + QLatin1Char(' '), QTextCharFormat());
        }
        bool afterWordMark = false;  // the white space after Word's number is left out
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat source = fragment.charFormat();
            if (source.isImageFormat()) {
                continue;  // until the editor has images
            }
            // The number or bullet of Word's list item
            const QStringList families = source.fontFamilies().toStringList();
            const auto wordMark = std::find_if(families.begin(), families.end(), [](const QString& family) {
                return family.startsWith(QLatin1String(WORD_LIST_MARK));
            });
            if (wordMark != families.end() && text.text.trimmed().isEmpty()) {
                const QStringList parts =
                    wordMark->mid(qsizetype(qstrlen(WORD_LIST_MARK))).split(QLatin1Char('-'));
                if (parts.size() == 2) {
                    text.wordList = parts.at(0).toInt();
                    text.wordLevel = parts.at(1).toInt() - 1;
                }
                text.wordMark += fragment.text();
                afterWordMark = true;
                continue;
            }
            if (afterWordMark) {
                QString rest = fragment.text();
                qsizetype space = 0;
                while (space < rest.size() && (rest.at(space).isSpace() || rest.at(space) == u'\t')) {
                    ++space;
                }
                if (space == rest.size()) {
                    continue;
                }
                afterWordMark = false;
                if (space > 0) {
                    // the text after the white space
                    const QTextCharFormat format = keptFormat(source, heading);
                    text.append(rest.mid(space).remove(QChar(QChar::ObjectReplacementCharacter)),
                                format);
                    continue;
                }
            }
            const bool pageBreak = source.fontFamilies().toStringList().contains(
                QLatin1String(PAGE_BREAK_MARK));
            const QTextCharFormat format = keptFormat(source, heading);
            const QString part = fragment.text();
            for (qsizetype i = 0; i < part.size(); ++i) {
                const QChar ch = part.at(i);
                if (pageBreak && ch == QChar::LineSeparator) {
                    text.pageBreaks.push_back(text.text.size());
                } else if (ch.isHighSurrogate() && i + 1 < part.size() &&
                           part.at(i + 1).isLowSurrogate()) {
                    text.append(ch, format);
                    text.append(part.at(++i), format);
                } else if (!ch.isSurrogate() && holdable(ch)) {
                    text.append(ch, format);
                }
            }
        }
        if (text.wordList >= 0 && !keepsStructure()) {
            // Only the text: the number stays, as a list of HTML keeps it, and a bullet of
            // Symbol or Wingdings is the character it shows
            const ListLevel shown =
                wordListLevelOf(text.wordMark, ListKind::Numbered, 0, std::nullopt).first;
            PastedText marked;
            marked.append((shown.isBullet() ? shown.bullet : text.wordMark.trimmed()) +
                              QLatin1Char(' '),
                          QTextCharFormat());
            marked.text += text.text;
            marked.formats.insert(marked.formats.end(), text.formats.begin(), text.formats.end());
            for (const qsizetype pageBreak : text.pageBreaks) {
                marked.pageBreaks.push_back(pageBreak + marked.text.size() - text.text.size());
            }
            return marked;
        }
        return text;
    }

    /// @brief The format the mode keeps of a fragment's format
    /// @param heading The fragment is in a heading, whose own bold is the look of a heading
    QTextCharFormat keptFormat(const QTextCharFormat& source, bool heading) const
    {
        QTextCharFormat format;
        if (m_mode == PasteMode::Text) {
            return format;
        }
        // The emphasis; a link's underline and color are the look of a link
        if (!heading && source.fontWeight() >= QFont::DemiBold) {
            format.setFontWeight(QFont::Bold);
        }
        if (source.fontItalic()) {
            format.setFontItalic(true);
        }
        if (source.fontUnderline() && !source.isAnchor()) {
            format.setFontUnderline(true);
        }
        if (source.fontStrikeOut()) {
            format.setFontStrikeOut(true);
        }
        const QTextCharFormat::VerticalAlignment vertical = source.verticalAlignment();
        if (vertical == QTextCharFormat::AlignSubScript ||
            vertical == QTextCharFormat::AlignSuperScript) {
            format.setVerticalAlignment(vertical);
        }
        if (source.fontCapitalization() == QFont::SmallCaps ||
            source.fontCapitalization() == QFont::AllUppercase) {
            format.setFontCapitalization(source.fontCapitalization());
        }
        // A link to a page or an e-mail address; not one to a place in the source page, nor
        // a script
        if (source.isAnchor() && isOpenableLinkAddress(source.anchorHref())) {
            setLinkAddress(format, source.anchorHref().trimmed());
        }
        if (m_mode != PasteMode::Source) {
            return format;
        }

        // The look of the source: its font, size and colors (KmlSerializer leaves out black
        // and transparent ones)
        const QString family = sourceFontFamily(source.fontFamilies().toStringList());
        if (!family.isEmpty()) {
            format.setFontFamilies({family});
        }
        if (source.hasProperty(QTextFormat::FontPointSize) && source.fontPointSize() > 0) {
            format.setFontPointSize(source.fontPointSize());
        } else if (source.hasProperty(QTextFormat::FontPixelSize) &&
                   source.intProperty(QTextFormat::FontPixelSize) > 0) {
            // CSS has 96 pixels in an inch, 72 points
            format.setFontPointSize(source.intProperty(QTextFormat::FontPixelSize) * 0.75);
        }
        if (source.hasProperty(QTextFormat::ForegroundBrush) && !source.isAnchor()) {
            format.setForeground(source.foreground().color());
        }
        if (source.hasProperty(QTextFormat::BackgroundBrush)) {
            format.setBackground(source.background().color());
        }
        if (source.hasProperty(QTextFormat::FontLetterSpacing) &&
            source.fontLetterSpacingType() == QFont::AbsoluteSpacing &&
            source.fontLetterSpacing() != 0.0) {
            // In CSS pixels, as the size above
            format.setProperty(KmlPropLetterSpacing,
                               std::clamp(source.fontLetterSpacing() * 0.75, MIN_LETTER_SPACING,
                                          MAX_LETTER_SPACING));
        }
        return format;
    }

    /// @brief The align attribute the mode keeps: Merge only center and right, as the book's
    ///        paragraphs are justified (left in the last line), but not the centering of a th
    ///        cell of the header row (Qt::AlignCenter), which is its look, as its bold
    QString alignmentAttribute(const QTextBlockFormat& format) const
    {
        const Qt::Alignment own = ownAlignment(format);
        if (!keepsStructure()) {
            return QString();
        }
        if (m_headerCell && m_mode == PasteMode::Merge &&
            (format.alignment() & Qt::AlignCenter) == Qt::AlignCenter) {
            return QString();
        }
        if (own & Qt::AlignHCenter) {
            return QStringLiteral(" align=\"center\"");
        }
        if (own & Qt::AlignRight) {
            return QStringLiteral(" align=\"right\"");
        }
        if (m_mode == PasteMode::Source) {
            if (own & Qt::AlignJustify) {
                return QStringLiteral(" align=\"justify\"");
            }
            if (own & Qt::AlignLeft) {
                return QStringLiteral(" align=\"left\"");
            }
        }
        return QString();
    }

    /// @brief What Word's list has had on each level: its format and the number its next
    ///        item has
    struct WordLevels {
        std::map<int, ListLevel> formats;
        std::map<int, int> next;
    };

    PasteMode m_mode;
    bool m_inCell = false;       ///< The KML of a cell of a table
    bool m_headerCell = false;   ///< Of a cell of the header row
    QTextDocument m_paragraphs;  ///< The text of the paragraphs, a block for each
    QTextCursor m_cursor;
    int m_paragraphCount = 0;
    std::vector<Item> m_items;
    int m_listCount = 0;                                ///< The lists so far: their ids
    int m_topListId = 0;                                ///< The list of HTML of the first level
    std::map<const QTextList*, int> m_htmlListIds;      ///< The ids of the lists of HTML
    std::map<int, int> m_wordListIds;                   ///< The ids of Word's lists, by lfo
    std::map<int, WordLevels> m_wordLevels;             ///< Of Word's lists, by id
    std::map<int, ListDefinition> m_definitions;        ///< The formats of the lists, by id
};

}  // namespace

// =============================================================================
// Paste Operations
// =============================================================================

bool ClipboardHandler::canPaste()
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return false;
    }

    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData == nullptr) {
        return false;
    }

    return mimeData->hasFormat(MIME_KML) ||
           mimeData->hasHtml() ||
           mimeData->hasText();
}

QString ClipboardHandler::pasteAsKml(PasteMode mode)
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return QString();
    }

    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData == nullptr) {
        return QString();
    }

    // Priority 1: Native KML format
    if (mimeData->hasFormat(MIME_KML)) {
        return QString::fromUtf8(mimeData->data(MIME_KML));
    }

    // Priority 2: HTML - convert to KML, unless only the text is wanted
    if (mimeData->hasHtml() && (mode != PasteMode::Text || !mimeData->hasText())) {
        const QString kml = htmlToKml(mimeData->html(), mode);
        if (!kml.isEmpty() || !mimeData->hasText()) {
            return kml;
        }
    }

    // Priority 3: Plain text - convert to KML, with the line breaks its HTML shows
    if (mimeData->hasText()) {
        return textToKml(withLineEndsOf(mimeData->text(),
                                        mimeData->hasHtml() ? mimeData->html() : QString()));
    }

    return QString();
}

QString ClipboardHandler::pasteAsText()
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return QString();
    }

    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData == nullptr) {
        return QString();
    }

    // Check for text directly
    if (mimeData->hasText()) {
        return mimeData->text();
    }

    // Extract text from HTML
    if (mimeData->hasHtml()) {
        // Simple HTML text extraction - strip tags
        QString html = mimeData->html();
        html.remove(QRegularExpression("<[^>]*>"));
        return html;
    }

    // Extract text from KML
    if (mimeData->hasFormat(MIME_KML)) {
        QString kml = QString::fromUtf8(mimeData->data(MIME_KML));
        return kmlToText(kml);
    }

    return QString();
}

// =============================================================================
// Format Conversion
// =============================================================================

PasteMode ClipboardHandler::pasteModeFromId(const QString& id)
{
    if (id == QLatin1String(PASTE_MODE_SOURCE)) {
        return PasteMode::Source;
    }
    if (id == QLatin1String(PASTE_MODE_TEXT)) {
        return PasteMode::Text;
    }
    return PasteMode::Merge;
}

QString ClipboardHandler::sceneBreakStyle()
{
    return QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
        "editor.sceneBreak.style", std::string("asterisks")));
}

QString ClipboardHandler::htmlDocument(const QString& fragment)
{
    return QStringLiteral("<html><head><meta charset=\"utf-8\"></head><body>"
                          "<!--StartFragment-->") +
           fragment + QStringLiteral("<!--EndFragment--></body></html>");
}

QString ClipboardHandler::kmlToHtml(const QString& kml, const QString& sceneBreakStyle,
                                    const ListMarks* marks)
{
    if (kml.isEmpty()) {
        return QString();
    }
    const ListMarks counted = marks != nullptr ? ListMarks{} : listMarksOf(kml);
    const ListMarks& items = marks != nullptr ? *marks : counted;
    size_t itemIndex = 0;  // of the next <li>

    // Plain HTML that other programs paste: paragraphs and headings with their alignment,
    // quotes, lines and page breaks, the basic formatting tags and the explicit inline
    // styles. Metadata (annotation anchors, footnotes), text runs and unknown elements keep
    // only their text; the annotations themselves stay out.
    struct OpenElement {
        bool written = false;    ///< The element wrote an HTML element to close
        bool paragraph = false;  ///< The element is a paragraph, a heading or a list item
        bool list = false;       ///< The element is a list (ol or ul)
        bool numbered = false;   ///< A list of numbers
        int nextNumber = -1;     ///< The number its next item has without a value of its own
        bool row = false;        ///< A row of a table (its tr is written with its first cell)
        bool cell = false;       ///< A cell of a table
        bool table = false;      ///< A table
    };
    std::vector<OpenElement> open;
    bool emptyParagraph = false;
    bool endsWithBreak = false;  ///< The paragraph's text so far ends with a line break
    bool inQuote = false;        ///< A blockquote is open, for paragraphs in a quote style
    bool pageBreak = false;      ///< A page break comes before the next element
    QString breakStyle = sceneBreakStyle;

    // The table written: the widths of its columns (in percent), its rows so far, the column
    // of the next cell of its first row, and whether its header (thead) is open
    struct OpenTable {
        std::vector<double> widths;
        int rows = 0;
        int column = 0;
        bool head = false;
    };
    std::optional<OpenTable> table;

    QString html;
    QXmlStreamWriter writer(&html);
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

    // The start of an element of the text's level: in a blockquote for a quote, after the
    // page break before it
    const auto startElement = [&](const QString& name, bool quote, const QString& align,
                                  const QString& style = QString()) {
        if (inQuote && !quote) {
            writer.writeEndElement();  // blockquote
            inQuote = false;
        }
        if (quote && !inQuote) {
            writer.writeStartElement(QStringLiteral("blockquote"));
            inQuote = true;
        }
        writer.writeStartElement(name);
        QStringList css;
        if (!style.isEmpty()) {
            css << style;
        }
        if (pageBreak) {
            css << QStringLiteral("page-break-before:always");
            pageBreak = false;
        }
        if (align == QStringLiteral("center") || align == QStringLiteral("right") ||
            align == QStringLiteral("justify")) {
            css << QStringLiteral("text-align:") + align;
        }
        if (!css.isEmpty()) {
            writer.writeAttribute(QStringLiteral("style"), css.join(QLatin1Char(';')));
        }
    };
    // An empty line on a page of its own, for a page break that nothing follows
    const auto writePendingPageBreak = [&]() {
        if (pageBreak) {
            startElement(QStringLiteral("p"), false, QString());
            writer.writeCharacters(QString(QChar(QChar::Nbsp)));
            writer.writeEndElement();
        }
    };

    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString tag = reader.name().toString();
                if (tag == QStringLiteral("annotations")) {
                    reader.skipCurrentElement();
                    break;
                }
                if (tag == QStringLiteral("br")) {
                    // A line break in the paragraph
                    writer.writeEmptyElement(QStringLiteral("br"));
                    reader.skipCurrentElement();
                    emptyParagraph = false;
                    endsWithBreak = true;
                    break;
                }
                const QXmlStreamAttributes attrs = reader.attributes();
                // The elements of the text's level: children of the root element
                if (open.size() == 1) {
                    if (tag == QStringLiteral("page-break")) {
                        writePendingPageBreak();  // two in a row: an empty page between
                        pageBreak = true;
                        reader.skipCurrentElement();
                        break;
                    }
                    if (tag == QStringLiteral("hr")) {
                        startElement(QStringLiteral("hr"), false, QString());
                        writer.writeEndElement();
                        reader.skipCurrentElement();
                        break;
                    }
                    if (tag == QStringLiteral("scene-break")) {
                        // As it looks in the editor: a short line, an empty line, a symbol
                        if (breakStyle.isEmpty()) {
                            breakStyle = ClipboardHandler::sceneBreakStyle();
                        }
                        if (breakStyle == QStringLiteral("line")) {
                            startElement(QStringLiteral("hr"), false, QString());
                            writer.writeAttribute(QStringLiteral("width"), QStringLiteral("30%"));
                        } else {
                            const QString text = sceneBreakText(breakStyle);
                            startElement(QStringLiteral("p"), false,
                                         text.isEmpty() ? QString() : QStringLiteral("center"));
                            writer.writeCharacters(text.isEmpty() ? QString(QChar(QChar::Nbsp))
                                                                  : text);
                        }
                        writer.writeEndElement();
                        reader.skipCurrentElement();
                        break;
                    }
                }
                OpenElement element;
                if (tag == QStringLiteral("table")) {
                    if (table) {
                        reader.skipCurrentElement();  // a table holds none
                        break;
                    }
                    // As wide as the text, with a thin grid: Word, LibreOffice and the
                    // browsers show it as the editor does
                    startElement(QStringLiteral("table"), false, QString(),
                                 QStringLiteral("border-collapse:collapse"));
                    writer.writeAttribute(QStringLiteral("border"), QStringLiteral("1"));
                    writer.writeAttribute(QStringLiteral("cellspacing"), QStringLiteral("0"));
                    writer.writeAttribute(QStringLiteral("cellpadding"), QStringLiteral("4"));
                    writer.writeAttribute(QStringLiteral("width"), QStringLiteral("100%"));
                    table = OpenTable{};
                    element.written = true;
                    element.table = true;
                    open.push_back(element);
                    break;
                }
                if (table && tag == QStringLiteral("col")) {
                    const QString width = attrs.value(QStringLiteral("width")).toString().trimmed();
                    bool ok = false;
                    const double percent =
                        width.endsWith(QLatin1Char('%')) ? width.chopped(1).toDouble(&ok) : 0.0;
                    table->widths.push_back(ok && percent > 0.0 ? percent : 0.0);
                    reader.skipCurrentElement();
                    break;
                }
                if (table && tag == QStringLiteral("tr")) {
                    ++table->rows;
                    table->column = 0;
                    element.row = true;
                    open.push_back(element);
                    break;
                }
                if (table && (tag == QStringLiteral("th") || tag == QStringLiteral("td")) &&
                    open.back().row) {
                    // The row starts with its first cell: the header rows at the top are the
                    // table's thead
                    const bool header = tag == QStringLiteral("th");
                    if (!open.back().written) {
                        if (header && table->rows == 1) {
                            writer.writeStartElement(QStringLiteral("thead"));
                            table->head = true;
                        } else if (!header && table->head) {
                            writer.writeEndElement();  // thead
                            table->head = false;
                        }
                        writer.writeStartElement(QStringLiteral("tr"));
                        open.back().written = true;
                    }
                    writer.writeStartElement(tag);
                    const int columns = spanOf(attrs, QStringLiteral("colspan"));
                    const int rows = spanOf(attrs, QStringLiteral("rowspan"));
                    if (columns > 1) {
                        writer.writeAttribute(QStringLiteral("colspan"), QString::number(columns));
                    }
                    if (rows > 1) {
                        writer.writeAttribute(QStringLiteral("rowspan"), QString::number(rows));
                    }
                    // The widths of the columns on the cells of the first row, where all the
                    // programs read them
                    const auto first = static_cast<size_t>(table->column);
                    const size_t end = first + static_cast<size_t>(columns);
                    if (table->rows == 1 && end <= table->widths.size()) {
                        double width = 0.0;
                        for (size_t column = first; column < end; ++column) {
                            width += table->widths[column];
                        }
                        if (width > 0.0) {
                            writer.writeAttribute(
                                QStringLiteral("width"),
                                QString::number(std::round(width * 100.0) / 100.0, 'g', 6) +
                                    QLatin1Char('%'));
                        }
                    }
                    writer.writeAttribute(QStringLiteral("valign"), QStringLiteral("top"));
                    table->column += columns;
                    element.written = true;
                    element.cell = true;
                    open.push_back(element);
                    break;
                }
                const int level = headingLevelOfTag(tag);
                if (tag == QStringLiteral("ol") || tag == QStringLiteral("ul")) {
                    // A list: of the text's level, or of a lower level in an item
                    const int listLevel = static_cast<int>(std::count_if(
                        open.begin(), open.end(), [](const OpenElement& e) { return e.list; }));
                    const bool numbered = tag == QStringLiteral("ol");
                    const ListLevel format = listLevelFromKml(
                        numbered, attrs, std::min(listLevel, LIST_LEVELS - 1));
                    if (open.size() == 1) {
                        startElement(format.isBullet() ? QStringLiteral("ul") : QStringLiteral("ol"),
                                     false, QString());
                    } else {
                        writer.writeStartElement(format.isBullet() ? QStringLiteral("ul")
                                                                   : QStringLiteral("ol"));
                    }
                    element = {true, false, true, !format.isBullet(), -1};
                    if (format.isBullet()) {
                        const QChar bullet = format.bullet.isEmpty() ? QChar() : format.bullet.at(0);
                        if (bullet == QChar(BULLET_CIRCLE)) {
                            writer.writeAttribute(QStringLiteral("type"), QStringLiteral("circle"));
                        } else if (bullet == QChar(BULLET_SQUARE)) {
                            writer.writeAttribute(QStringLiteral("type"), QStringLiteral("square"));
                        } else if (bullet != QChar(BULLET_DISC)) {
                            writer.writeAttribute(QStringLiteral("style"),
                                                  QStringLiteral("list-style-type:'%1 '")
                                                      .arg(format.bullet));
                        }
                    } else {
                        static const QHash<ListNumberFormat, QString> types = {
                            {ListNumberFormat::LowerAlpha, QStringLiteral("a")},
                            {ListNumberFormat::UpperAlpha, QStringLiteral("A")},
                            {ListNumberFormat::LowerRoman, QStringLiteral("i")},
                            {ListNumberFormat::UpperRoman, QStringLiteral("I")}};
                        if (types.contains(format.format)) {
                            writer.writeAttribute(QStringLiteral("type"), types.value(format.format));
                        }
                        // The number of its first item, as it is shown
                        const int first = itemIndex < items.numbers.size()
                                              ? items.numbers[itemIndex]
                                              : format.start;
                        element.nextNumber = first >= 0 ? first : format.start;
                        if (element.nextNumber != 1) {
                            writer.writeAttribute(QStringLiteral("start"),
                                                  QString::number(element.nextNumber));
                        }
                    }
                    open.push_back(element);
                    break;
                }
                if (tag == QStringLiteral("li")) {
                    // An item: its number when it does not go on from the one before it
                    writer.writeStartElement(QStringLiteral("li"));
                    const QString mark =
                        itemIndex < items.marks.size() ? items.marks[itemIndex] : QString();
                    const int number =
                        itemIndex < items.numbers.size() ? items.numbers[itemIndex] : -1;
                    ++itemIndex;
                    QStringList css;
                    if (mark.isEmpty() && itemIndex <= items.marks.size()) {
                        css << QStringLiteral("list-style-type:none");
                    }
                    const QString align = attrs.value(QStringLiteral("align")).toString().toLower();
                    if (align == QStringLiteral("center") || align == QStringLiteral("right") ||
                        align == QStringLiteral("justify")) {
                        css << QStringLiteral("text-align:") + align;
                    }
                    const auto list = std::find_if(open.rbegin(), open.rend(),
                                                   [](const OpenElement& e) { return e.list; });
                    if (list != open.rend() && list->numbered && number >= 0) {
                        if (number != list->nextNumber) {
                            writer.writeAttribute(QStringLiteral("value"), QString::number(number));
                        }
                        list->nextNumber = number + 1;
                    }
                    if (!css.isEmpty()) {
                        writer.writeAttribute(QStringLiteral("style"), css.join(QLatin1Char(';')));
                    }
                    element = {true, true};
                    emptyParagraph = true;
                    endsWithBreak = false;
                    open.push_back(element);
                    break;
                }
                if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph") ||
                    level > 0) {
                    startElement(level > 0 ? tag : QStringLiteral("p"),
                                 level == 0 &&
                                     isQuoteStyle(attrs.value(QStringLiteral("style")).toString()),
                                 attrs.value(QStringLiteral("align")).toString().toLower());
                    element = {true, true};
                    emptyParagraph = true;
                    endsWithBreak = false;
                } else if (tag == QStringLiteral("a")) {
                    // A link the other program may open: to a page or an e-mail address
                    const QString address = attrs.value(QStringLiteral("href")).toString().trimmed();
                    if (isOpenableLinkAddress(address)) {
                        writer.writeStartElement(QStringLiteral("a"));
                        writer.writeAttribute(QStringLiteral("href"), address);
                        element.written = true;
                    }
                } else if (KmlFormatRegistry::isFormattingTag(tag)) {
                    const QString name = KmlFormatRegistry::canonicalFormattingTag(tag);
                    const QString style = inlineStyleCss(attrs);
                    if (!name.isEmpty() || !style.isEmpty()) {
                        writer.writeStartElement(name.isEmpty() ? QStringLiteral("span") : name);
                        if (!style.isEmpty()) {
                            writer.writeAttribute(QStringLiteral("style"), style);
                        }
                        element.written = true;
                    }
                }
                open.push_back(element);
                break;
            }

            case QXmlStreamReader::EndElement:
                if (!open.empty()) {
                    const OpenElement element = open.back();
                    open.pop_back();
                    if (element.paragraph && (emptyParagraph || endsWithBreak)) {
                        // Most programs drop an empty paragraph, and the empty line after a
                        // line break at a paragraph's end - a no-break space keeps the line
                        writer.writeCharacters(QString(QChar(QChar::Nbsp)));
                    }
                    if (element.cell && inQuote) {
                        writer.writeEndElement();  // the blockquote of the cell's last paragraph
                        inQuote = false;
                    }
                    if (element.table && table && table->head) {
                        writer.writeEndElement();  // thead: a table of header rows only
                    }
                    if (element.written) {
                        writer.writeEndElement();
                    }
                    if (element.table) {
                        table.reset();
                    }
                    if (open.empty()) {
                        // The end of the text
                        writePendingPageBreak();
                        if (inQuote) {
                            writer.writeEndElement();  // blockquote
                            inQuote = false;
                        }
                    }
                }
                break;

            case QXmlStreamReader::Characters: {
                // Whitespace outside paragraphs is only the indentation between them
                const bool inParagraph = std::any_of(open.begin(), open.end(),
                                                     [](const OpenElement& e) { return e.paragraph; });
                if (!reader.text().isEmpty() && (inParagraph || !reader.isWhitespace())) {
                    writer.writeCharacters(reader.text().toString());
                    emptyParagraph = false;
                    endsWithBreak = false;
                }
                break;
            }

            default:
                break;
        }
    }

    return html;
}

QString ClipboardHandler::htmlToKml(const QString& html, PasteMode mode)
{
    if (html.isEmpty()) {
        return QString();
    }

    // Qt reads the HTML as a browser shows it, without what it refers to: a style sheet or
    // an image of a pasted page may be a file of the computer or of a network share
    const ResourcelessDocument resources;
    const QTextDocumentFragment fragment =
        QTextDocumentFragment::fromHtml(withMarks(html, mode == PasteMode::Source), &resources);
    ResourcelessDocument document;
    document.setUndoRedoEnabled(false);
    QTextCursor(&document).insertFragment(fragment);

    PastedKml kml(mode);
    kml.addFrame(*document.rootFrame());
    return kml.kml();
}

QString ClipboardHandler::textToKml(const QString& text)
{
    if (text.isEmpty()) {
        return QString();
    }

    QString result;
    QXmlStreamWriter writer(&result);

    // Split text into paragraphs by newlines
    QStringList paragraphs = text.split('\n');

    for (QString& para : paragraphs) {
        // A line break in the paragraph: U+2028, or a vertical tab, as Word writes it
        para.replace(QLatin1Char('\v'), QChar(QChar::LineSeparator));
        // XML has no place for control characters other than tab, and a CR left by a CR LF
        // line end would come back as one more paragraph break
        para.removeIf([](QChar ch) {
            return (ch.unicode() < 0x20 && ch != u'\t') || ch.unicode() == 0xFFFE ||
                   ch.unicode() == 0xFFFF;
        });
        writer.writeStartElement("p");
        writer.writeStartElement("text");
        const QStringList parts = para.split(QChar(QChar::LineSeparator));
        for (qsizetype i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                writer.writeEmptyElement("br");
            }
            writer.writeCharacters(parts.at(i));
        }
        writer.writeEndElement();  // text
        writer.writeEndElement();  // p
    }

    return result;
}

QString ClipboardHandler::withLineEndsOf(const QString& text, const QString& html)
{
    if (html.isEmpty() || (!text.contains(u'\n') && !text.contains(u'\r'))) {
        return text;
    }
    ResourcelessDocument document;
    document.setHtml(html);
    const QString structure = document.toRawText();

    // The characters of the text and of the HTML's text, one by one: between two of them
    // the line ends of the text are line breaks where the HTML has one and no paragraph end,
    // and no more than the HTML's line ends where it has a paragraph end
    QString result;
    result.reserve(text.size());
    qsizetype inText = 0;
    qsizetype inHtml = 0;
    while (inText < text.size()) {
        // The white space of the text up to its next character...
        const qsizetype spaceStart = inText;
        while (inText < text.size() && skippedInMatching(text.at(inText))) {
            ++inText;
        }
        // ... and of the HTML's text
        bool lineBreak = false;
        bool paragraphEnd = false;
        int lineEnds = 0;  // the line breaks and paragraph ends of the HTML
        while (inHtml < structure.size() && skippedInMatching(structure.at(inHtml))) {
            const QChar ch = structure.at(inHtml);
            lineBreak = lineBreak || ch == QChar::LineSeparator;
            paragraphEnd = paragraphEnd || endsParagraph(ch);
            lineEnds += (ch == QChar::LineSeparator || endsParagraph(ch)) ? 1 : 0;
            ++inHtml;
        }
        QString space = text.mid(spaceStart, inText - spaceStart);
        if (lineBreak && !paragraphEnd) {
            space.replace(QStringLiteral("\r\n"), QString(QChar(QChar::LineSeparator)));
            space.replace(QLatin1Char('\n'), QChar(QChar::LineSeparator));
            space.replace(QLatin1Char('\r'), QChar(QChar::LineSeparator));
        } else if (paragraphEnd && spaceStart > 0 && inText < text.size()) {
            space = withLastLineEnds(space, lineEnds);
        }
        result += space;
        if (inText == text.size()) {
            break;
        }
        if (inHtml == structure.size() || structure.at(inHtml) != text.at(inText)) {
            return text;  // another text: its HTML says nothing about its lines
        }
        result += text.at(inText);
        ++inText;
        ++inHtml;
    }
    return result;
}

QString ClipboardHandler::kmlToText(const QString& kml, const QString& sceneBreakStyle,
                                    const ListMarks* marks)
{
    if (kml.isEmpty()) {
        return QString();
    }
    const ListMarks counted = marks != nullptr ? ListMarks{} : listMarksOf(kml);
    const ListMarks& items = marks != nullptr ? *marks : counted;
    size_t itemIndex = 0;  // of the next <li>
    int listLevel = 0;     // the lists open

    QString text;
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));
    QString breakStyle = sceneBreakStyle;
    int depth = 0;           // of the element read: 1 for the root element
    bool firstLine = true;   // nothing of the text's level written yet
    const auto startLine = [&]() {
        if (!firstLine) {
            text += QLatin1Char('\n');
        }
        firstLine = false;
    };
    // A table: a tab before each cell but those of the first column, its rows on lines of
    // their own, the paragraphs of a cell separated by spaces (a line end would end the row)
    int tableDepth = 0;          // of the table read; 0 outside tables
    int cellDepth = 0;           // of its cell read; 0 outside cells
    int row = -1;                // of the row read
    int column = 0;              // of the next cell of the row
    std::vector<int> takenUntil;  // for each column, the row a cell above spans until
    bool firstInCell = true;     // no paragraph of the cell written yet

    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        switch (reader.tokenType()) {
            case QXmlStreamReader::StartElement: {
                ++depth;
                const QString tagName = reader.name().toString();
                if (tagName == QStringLiteral("annotations")) {
                    reader.skipCurrentElement();  // Not part of the text
                    --depth;
                } else if (tagName == QStringLiteral("br")) {
                    text += tableDepth > 0 ? QLatin1Char(' ') : QLatin1Char('\n');
                } else if (tagName == QStringLiteral("ol") || tagName == QStringLiteral("ul")) {
                    ++listLevel;
                } else if (tableDepth > 0) {
                    if (tagName == QStringLiteral("tr")) {
                        if (++row > 0) {
                            text += QLatin1Char('\n');
                        }
                        column = 0;
                    } else if (tagName == QStringLiteral("th") || tagName == QStringLiteral("td")) {
                        // In the first column the cells above do not span into
                        while (static_cast<size_t>(column) < takenUntil.size() &&
                               takenUntil[static_cast<size_t>(column)] > row) {
                            ++column;
                        }
                        if (column > 0) {
                            text += QLatin1Char('\t');
                        }
                        const QXmlStreamAttributes attrs = reader.attributes();
                        const int columns = spanOf(attrs, QStringLiteral("colspan"));
                        const size_t end = static_cast<size_t>(column) + static_cast<size_t>(columns);
                        if (takenUntil.size() < end) {
                            takenUntil.resize(end, 0);
                        }
                        for (size_t taken = static_cast<size_t>(column); taken < end; ++taken) {
                            takenUntil[taken] = row + spanOf(attrs, QStringLiteral("rowspan"));
                        }
                        column += columns;
                        firstInCell = true;
                        cellDepth = depth;
                    } else if (cellDepth > 0 &&
                               (depth == cellDepth + 1 || tagName == QStringLiteral("li"))) {
                        // A paragraph or a list item of the cell
                        if (!firstInCell) {
                            text += QLatin1Char(' ');
                        }
                        firstInCell = false;
                        if (tagName == QStringLiteral("li")) {
                            if (itemIndex < items.marks.size() &&
                                !items.marks[itemIndex].isEmpty()) {
                                text += items.marks[itemIndex] + QLatin1Char(' ');
                            }
                            ++itemIndex;
                        }
                    }
                } else if (tagName == QStringLiteral("li")) {
                    // A list item: its number or bullet, after a tab for each lower level
                    startLine();
                    text += QString(std::max(0, listLevel - 1), QLatin1Char('\t'));
                    if (itemIndex < items.marks.size() && !items.marks[itemIndex].isEmpty()) {
                        text += items.marks[itemIndex] + QLatin1Char(' ');
                    }
                    ++itemIndex;
                } else if (depth == 2) {
                    // The elements of the text's level: each starts a line
                    startLine();
                    if (tagName == QStringLiteral("hr")) {
                        text += HORIZONTAL_LINE_TEXT;
                    } else if (tagName == QStringLiteral("scene-break")) {
                        if (breakStyle.isEmpty()) {
                            breakStyle = ClipboardHandler::sceneBreakStyle();
                        }
                        text += sceneBreakText(breakStyle);
                    } else if (tagName == QStringLiteral("page-break")) {
                        text += QLatin1Char('\f');  // the page break of plain text
                    } else if (tagName == QStringLiteral("table")) {
                        tableDepth = depth;
                        row = -1;
                        takenUntil.clear();
                    }
                }
                break;
            }

            case QXmlStreamReader::EndElement:
                --depth;
                if (reader.name() == QStringLiteral("ol") || reader.name() == QStringLiteral("ul")) {
                    --listLevel;
                }
                if (depth < cellDepth) {
                    cellDepth = 0;
                }
                if (depth < tableDepth) {
                    tableDepth = 0;
                }
                break;

            case QXmlStreamReader::Characters: {
                // Whitespace between paragraphs is only their indentation, and so is any
                // text of a table outside its cells' paragraphs
                const bool inTable = tableDepth > 0;
                const bool inCellText =
                    cellDepth > 0 && (depth > cellDepth || !reader.isWhitespace());
                if (depth >= 2 && (!inTable || inCellText)) {
                    text += reader.text().toString();
                }
                break;
            }

            default:
                break;
        }
    }

    return text;
}

ListMarks ClipboardHandler::listMarksOf(const QString& kml)
{
    ListMarks result;
    if (kml.isEmpty()) {
        return result;
    }
    // The items in a document of their own, as the editor counts them
    KmlDocumentModel model;
    model.loadKml(kml);
    QTextDocument document;
    QTextCursor cursor(&document);
    appendParagraphs(cursor, model);
    const std::vector<QString> marks = listMarkers(document);
    const std::vector<int> numbers = listNumbers(document);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (isListItem(block.blockFormat())) {
            const auto index = static_cast<size_t>(block.blockNumber());
            result.marks.push_back(index < marks.size() ? marks[index] : QString());
            result.numbers.push_back(index < numbers.size() ? numbers[index] : -1);
        }
    }
    return result;
}

}  // namespace kalahari::editor
