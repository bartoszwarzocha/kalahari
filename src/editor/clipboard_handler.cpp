/// @file clipboard_handler.cpp
/// @brief Clipboard operations implementation (OpenSpec #00042 Phase 4.13-4.16)
/// Phase 11: Removed KmlDocument-dependent methods (use BookEditor API instead)

#include <kalahari/editor/clipboard_handler.h>
#include <QGuiApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace kalahari::editor {

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

    // Wrap in root element to handle multiple paragraphs at top level
    QString wrappedKml = "<root>" + kml + "</root>";

    QString html;
    QXmlStreamReader reader(wrappedKml);
    QXmlStreamWriter writer(&html);

    // Simple conversion without document declaration
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        switch (reader.tokenType()) {
            case QXmlStreamReader::StartElement: {
                const QString tagName = reader.name().toString();

                if (tagName == "p") {
                    writer.writeStartElement("p");
                } else if (tagName == "bold" || tagName == "b") {
                    writer.writeStartElement("b");
                } else if (tagName == "italic" || tagName == "i") {
                    writer.writeStartElement("i");
                } else if (tagName == "underline" || tagName == "u") {
                    writer.writeStartElement("u");
                } else if (tagName == "strike" || tagName == "s") {
                    writer.writeStartElement("s");
                } else if (tagName == "span") {
                    writer.writeStartElement("span");
                    // Copy style attribute if present
                    if (reader.attributes().hasAttribute("style")) {
                        writer.writeAttribute("style",
                                             reader.attributes().value("style").toString());
                    }
                } else if (tagName == "br") {
                    writer.writeEmptyElement("br");
                } else if (tagName == "text") {
                    // Text runs are just containers, skip the tag
                } else if (tagName == "document" || tagName == "root") {
                    // Skip document wrapper and our temporary root
                } else {
                    // Pass through unknown elements
                    writer.writeStartElement(tagName);
                }
                break;
            }

            case QXmlStreamReader::EndElement: {
                const QString tagName = reader.name().toString();
                if (tagName != "text" && tagName != "document" && tagName != "root") {
                    writer.writeEndElement();
                }
                break;
            }

            case QXmlStreamReader::Characters: {
                writer.writeCharacters(reader.text().toString());
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

    for (const QString& para : paragraphs) {
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
