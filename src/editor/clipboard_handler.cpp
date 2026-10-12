/// @file clipboard_handler.cpp
/// @brief Clipboard operations implementation (OpenSpec #00042 Phase 4.13-4.16)

#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_format_registry.h>
#include <QColor>
#include <QGuiApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>
#include <QVariant>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
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

QString ClipboardHandler::pasteAsKml()
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

    // Priority 2: HTML - convert to KML
    if (mimeData->hasHtml()) {
        return htmlToKml(mimeData->html());
    }

    // Priority 3: Plain text - convert to KML
    if (mimeData->hasText()) {
        return textToKml(mimeData->text());
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

QString ClipboardHandler::kmlToHtml(const QString& kml)
{
    if (kml.isEmpty()) {
        return QString();
    }

    // Plain HTML that other programs paste: paragraphs with their alignment, the basic
    // formatting tags and the explicit inline styles. Metadata (annotation anchors,
    // footnotes), text runs and unknown elements keep only their text; the annotations
    // themselves stay out.
    struct OpenElement {
        bool written = false;    ///< The element wrote an HTML element to close
        bool paragraph = false;  ///< The element is a paragraph
    };
    std::vector<OpenElement> open;
    bool emptyParagraph = false;
    bool endsWithBreak = false;  ///< The paragraph's text so far ends with a line break

    QString html;
    QXmlStreamWriter writer(&html);
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

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
                OpenElement element;
                if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph")) {
                    writer.writeStartElement(QStringLiteral("p"));
                    const QString align = attrs.value(QStringLiteral("align")).toString().toLower();
                    if (align == QStringLiteral("center") || align == QStringLiteral("right") ||
                        align == QStringLiteral("justify")) {
                        writer.writeAttribute(QStringLiteral("style"),
                                              QStringLiteral("text-align:") + align);
                    }
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

QString ClipboardHandler::htmlToKml(const QString& html)
{
    if (html.isEmpty()) {
        return QString();
    }

    QString kml;
    QXmlStreamWriter writer(&kml);

    // Parse HTML (lenient parsing)
    // Note: Qt's QXmlStreamReader is strict XML, so we need to handle HTML quirks

    // Simple regex-based conversion for common tags
    QString result = html;

    // Convert HTML tags to KML equivalents
    result.replace(QRegularExpression("<b\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<bold>");
    result.replace(QRegularExpression("</b>", QRegularExpression::CaseInsensitiveOption), "</bold>");
    result.replace(QRegularExpression("<strong\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<bold>");
    result.replace(QRegularExpression("</strong>", QRegularExpression::CaseInsensitiveOption), "</bold>");

    result.replace(QRegularExpression("<i\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<italic>");
    result.replace(QRegularExpression("</i>", QRegularExpression::CaseInsensitiveOption), "</italic>");
    result.replace(QRegularExpression("<em\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<italic>");
    result.replace(QRegularExpression("</em>", QRegularExpression::CaseInsensitiveOption), "</italic>");

    result.replace(QRegularExpression("<u\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<underline>");
    result.replace(QRegularExpression("</u>", QRegularExpression::CaseInsensitiveOption), "</underline>");

    result.replace(QRegularExpression("<s\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<strike>");
    result.replace(QRegularExpression("</s>", QRegularExpression::CaseInsensitiveOption), "</strike>");
    result.replace(QRegularExpression("<strike\\b[^>]*>", QRegularExpression::CaseInsensitiveOption), "<strike>");
    result.replace(QRegularExpression("</strike>", QRegularExpression::CaseInsensitiveOption), "</strike>");

    // Convert line breaks
    result.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "<br/>");

    // Convert paragraphs (already same tag name)
    // Handle <p> with attributes by stripping attributes
    result.replace(QRegularExpression("<p\\s+[^>]*>", QRegularExpression::CaseInsensitiveOption), "<p>");

    // Remove other HTML tags (head, body, html, div, span without relevant attributes)
    result.replace(QRegularExpression("</?html[^>]*>", QRegularExpression::CaseInsensitiveOption), "");
    result.replace(QRegularExpression("</?head[^>]*>", QRegularExpression::CaseInsensitiveOption), "");
    result.replace(QRegularExpression("</?body[^>]*>", QRegularExpression::CaseInsensitiveOption), "");
    result.replace(QRegularExpression("</?div[^>]*>", QRegularExpression::CaseInsensitiveOption), "");
    result.replace(QRegularExpression("</?span[^>]*>", QRegularExpression::CaseInsensitiveOption), "");

    // Decode HTML entities
    result.replace("&nbsp;", " ");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&amp;", "&");
    result.replace("&quot;", "\"");
    result.replace("&apos;", "'");

    // Trim whitespace
    result = result.trimmed();

    // Wrap in paragraph if not already
    if (!result.startsWith("<p>") && !result.isEmpty()) {
        result = "<p>" + result + "</p>";
    }

    return result;
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

QString ClipboardHandler::kmlToText(const QString& kml)
{
    if (kml.isEmpty()) {
        return QString();
    }

    // Wrap in root element to handle multiple paragraphs at top level
    QString wrappedKml = "<root>" + kml + "</root>";

    QString text;
    QXmlStreamReader reader(wrappedKml);
    bool firstParagraph = true;

    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        switch (reader.tokenType()) {
            case QXmlStreamReader::StartElement: {
                const QString tagName = reader.name().toString();
                // Add newline before new paragraphs (except first)
                if (tagName == "annotations") {
                    reader.skipCurrentElement();  // Not part of the text
                } else if (tagName == "p") {
                    if (!firstParagraph) {
                        text += '\n';
                    }
                    firstParagraph = false;
                } else if (tagName == "br") {
                    text += '\n';
                }
                break;
            }

            case QXmlStreamReader::Characters: {
                text += reader.text().toString();
                break;
            }

            default:
                break;
        }
    }

    return text;
}

}  // namespace kalahari::editor
