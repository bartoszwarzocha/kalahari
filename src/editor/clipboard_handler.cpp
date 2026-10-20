/// @file clipboard_handler.cpp
/// @brief Clipboard operations implementation (OpenSpec #00042 Phase 4.13-4.16)

#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/kml_serializer.h>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
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
#include <string>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief CSS for the inline style attributes (font, size, color, bg) of a KML tag
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
    return css.join(QLatin1Char(';'));
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
int headingLevelOf(const QString& tag)
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

/// @brief The HTML with the marks of what Qt's reading of it does not keep: the paragraphs
///        of quotes (in a blockquote, in Word's quote styles) and Word's page breaks
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
    QString marked = html;
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
        const qsizetype start = html.indexOf(fragmentStart);
        marked.clear();
        qsizetype copied = 0;
        for (auto it = styleSheet.globalMatch(html); it.hasNext();) {
            const QRegularExpressionMatch match = it.next();
            QString sheet = match.captured(2);
            sheet.remove(QStringLiteral("<!--"));
            sheet.remove(QStringLiteral("-->"));
            sheet = match.captured(1) + sheet + match.captured(3);
            marked += html.mid(copied, match.capturedStart() - copied);
            if (match.capturedStart() < start) {
                sheets += sheet;
            } else {
                marked += sheet;
            }
            copied = match.capturedEnd();
        }
        marked += html.mid(copied);
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

/// @brief The text of a paragraph read from HTML, with the format of each character
struct PastedText {
    QString text;
    std::vector<QTextCharFormat> formats;  ///< One for each character of the text
    std::vector<qsizetype> pageBreaks;     ///< Where Word's page breaks were in the text

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

/// @brief The KML of HTML that Qt has read into a document: its paragraphs, as a paste mode
///        keeps them
class PastedKml {
public:
    explicit PastedKml(PasteMode mode)
        : m_mode(mode)
        , m_cursor(&m_paragraphs)
    {
    }

    /// @brief Add the paragraphs and tables of a frame
    void addFrame(const QTextFrame& frame)
    {
        for (QTextFrame::iterator it = frame.begin(); !it.atEnd(); ++it) {
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

    /// @brief The KML; empty without paragraphs
    QString kml() const
    {
        if (m_items.empty()) {
            return QString();
        }
        const KmlSerializer serializer;
        QString kml = QStringLiteral("<kml>");
        QTextBlock block = m_paragraphs.begin();
        for (const Item& item : m_items) {
            if (!item.paragraph) {
                kml += QLatin1Char('<') + item.element + QStringLiteral("/>");
                continue;
            }
            kml += QLatin1Char('<') + item.element + item.attributes + QLatin1Char('>') +
                   serializer.blockToKml(block) + QStringLiteral("</") + item.element +
                   QLatin1Char('>');
            block = block.next();
        }
        return kml + QStringLiteral("</kml>");
    }

private:
    /// @brief An element of the KML: a paragraph, whose text is the next block of
    ///        m_paragraphs, or an empty element (a line, a page break)
    struct Item {
        QString element;
        QString attributes;
        bool paragraph = false;
    };

    bool keepsStructure() const { return m_mode != PasteMode::Text; }

    void addBlock(const QTextBlock& block)
    {
        const QTextBlockFormat format = block.blockFormat();
        if (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore) {
            addPageBreak();
        }
        if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            if (keepsStructure()) {
                m_items.push_back({QStringLiteral("hr"), QString(), false});
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
            const PastedText text = textOf(block, level > 0);
            const bool preformatted = format.nonBreakableLines();
            qsizetype from = 0;
            for (const qsizetype pageBreak : text.pageBreaks) {
                addParagraph(element, attributes, text.mid(from, pageBreak), false);
                addPageBreak();
                from = pageBreak;
            }
            addParagraph(element, attributes, text.mid(from, text.text.size()), preformatted);
        }
        if (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysAfter) {
            addPageBreak();
        }
    }

    /// @brief Add a table: each row a paragraph, its cells separated by tabs (until the editor
    ///        has tables), the paragraphs of a cell by line breaks
    void addTable(const QTextTable& table)
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
        if (keepsStructure() && (m_items.empty() || m_items.back().paragraph ||
                                 m_items.back().element != QStringLiteral("page-break"))) {
            m_items.push_back({QStringLiteral("page-break"), QString(), false});
        }
    }

    /// @brief Add a paragraph of the text that a browser shows
    /// @param preformatted The text keeps its white space, and a line shows even when empty
    void addParagraph(const QString& element, const QString& attributes, PastedText text,
                      bool preformatted)
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
        m_items.push_back({element, attributes, true});
    }

    /// @brief The text of a block with the formats the mode keeps; a list item starts with
    ///        its number or bullet
    PastedText textOf(const QTextBlock& block, bool heading) const
    {
        PastedText text;
        if (QTextList* list = block.textList()) {
            text.append(listMarker(*list, block) + QLatin1Char(' '), QTextCharFormat());
        }
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat source = fragment.charFormat();
            if (source.isImageFormat()) {
                continue;  // until the editor has images
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
        return format;
    }

    /// @brief The align attribute the mode keeps: Merge only center and right, as the book's
    ///        paragraphs are justified (left in the last line)
    QString alignmentAttribute(const QTextBlockFormat& format) const
    {
        const Qt::Alignment own = ownAlignment(format);
        if (!keepsStructure()) {
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

    PasteMode m_mode;
    QTextDocument m_paragraphs;  ///< The text of the paragraphs, a block for each
    QTextCursor m_cursor;
    int m_paragraphCount = 0;
    std::vector<Item> m_items;
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

QString ClipboardHandler::kmlToHtml(const QString& kml, const QString& sceneBreakStyle)
{
    if (kml.isEmpty()) {
        return QString();
    }

    // Plain HTML that other programs paste: paragraphs and headings with their alignment,
    // quotes, lines and page breaks, the basic formatting tags and the explicit inline
    // styles. Metadata (annotation anchors, footnotes), text runs and unknown elements keep
    // only their text; the annotations themselves stay out.
    struct OpenElement {
        bool written = false;    ///< The element wrote an HTML element to close
        bool paragraph = false;  ///< The element is a paragraph or a heading
    };
    std::vector<OpenElement> open;
    bool emptyParagraph = false;
    bool endsWithBreak = false;  ///< The paragraph's text so far ends with a line break
    bool inQuote = false;        ///< A blockquote is open, for paragraphs in a quote style
    bool pageBreak = false;      ///< A page break comes before the next element
    QString breakStyle = sceneBreakStyle;

    QString html;
    QXmlStreamWriter writer(&html);
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

    // The start of an element of the text's level: in a blockquote for a quote, after the
    // page break before it
    const auto startElement = [&](const QString& name, bool quote, const QString& align) {
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
                const int level = headingLevelOf(tag);
                if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph") ||
                    level > 0) {
                    startElement(level > 0 ? tag : QStringLiteral("p"),
                                 level == 0 &&
                                     isQuoteStyle(attrs.value(QStringLiteral("style")).toString()),
                                 attrs.value(QStringLiteral("align")).toString().toLower());
                    element = {true, true};
                    emptyParagraph = true;
                    endsWithBreak = false;
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
                    if (element.written) {
                        writer.writeEndElement();
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

QString ClipboardHandler::kmlToText(const QString& kml, const QString& sceneBreakStyle)
{
    if (kml.isEmpty()) {
        return QString();
    }

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
                    text += QLatin1Char('\n');
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
                    }
                }
                break;
            }

            case QXmlStreamReader::EndElement:
                --depth;
                break;

            case QXmlStreamReader::Characters: {
                // Whitespace between paragraphs is only their indentation
                if (depth >= 2) {
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

}  // namespace kalahari::editor
