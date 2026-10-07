/// @file clipboard_handler.cpp
/// @brief Clipboard operations implementation (OpenSpec #00042 Phase 4.13-4.16)

#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_format_registry.h>
#include <QColor>
#include <QGuiApplication>
#include <QClipboard>
#include <QRegularExpression>
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
    // formatting tags and the explicit inline styles. Metadata (comments, TODO markers,
    // footnotes), text runs and unknown elements keep only their text.
    struct OpenElement {
        bool written = false;    ///< The element wrote an HTML element to close
        bool paragraph = false;  ///< The element is a paragraph
    };
    std::vector<OpenElement> open;
    bool emptyParagraph = false;

    QString html;
    QXmlStreamWriter writer(&html);
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString tag = reader.name().toString();
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
                    if (element.paragraph && emptyParagraph) {
                        // Most programs drop an empty paragraph - a no-break space keeps the line
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
        // XML has no place for control characters other than tab, and a CR left by a CR LF
        // line end would come back as one more paragraph break
        para.removeIf([](QChar ch) {
            return (ch.unicode() < 0x20 && ch != u'\t') || ch.unicode() == 0xFFFE ||
                   ch.unicode() == 0xFFFF;
        });
        writer.writeStartElement("p");
        writer.writeStartElement("text");
        writer.writeCharacters(para);
        writer.writeEndElement();  // text
        writer.writeEndElement();  // p
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
                if (tagName == "p") {
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
