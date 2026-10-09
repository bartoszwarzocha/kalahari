/// @file kml_serializer.cpp
/// @brief KML Serializer implementation (OpenSpec #00043 Phase 11.2)
///
/// Serializes QTextDocument content to KML (Kalahari Markup Language) format.
/// This is the reverse operation of reading KML (KmlDocumentModel).

#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_format_registry.h>  // For KmlPropertyId enum and registry functions
#include <kalahari/core/logger.h>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextBlockFormat>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QVariantMap>
#include <algorithm>

namespace kalahari {
namespace editor {

namespace {

/// @brief Text as XML can hold it: escaped, without the characters XML has no place for
/// (control characters other than tab and line breaks, unpaired surrogates, U+FFFE, U+FFFF)
QString xmlText(const QString& text)
{
    QString kept;
    kept.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char16_t code = ch.unicode();
        if (ch.isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            kept += ch;
            kept += text.at(++i);
        } else if ((code >= 0x20 || code == u'\t' || code == u'\n' || code == u'\r') &&
                   !ch.isSurrogate() && code != 0xFFFE && code != 0xFFFF) {
            kept += ch;
        }
    }
    return KmlFormatRegistry::escapeXml(kept);
}

/// @brief An anchor of an annotation: its start tag, or the whole tag of an empty one
QString anchorTag(const QString& id, bool empty)
{
    return QStringLiteral("<anchor ref=\"") + KmlFormatRegistry::escapeXml(id) +
           (empty ? QStringLiteral("\"/>") : QStringLiteral("\">"));
}

/// @brief Remember an annotation anchored in the serialized text (once)
void noteAnnotation(AnnotationList* annotations, const Annotation& annotation)
{
    if (annotations == nullptr) {
        return;
    }
    const bool known = std::any_of(annotations->cbegin(), annotations->cend(),
                                   [&annotation](const Annotation& a) { return a.id == annotation.id; });
    if (!known) {
        annotations->append(annotation);
    }
}

/// @brief An annotation of the <annotations> section
QString annotationElement(const Annotation& annotation)
{
    QString element = QStringLiteral("<annotation id=\"") +
                      KmlFormatRegistry::escapeXml(annotation.id) + QStringLiteral("\" kind=\"") +
                      annotationKindName(annotation.kind) + QLatin1Char('"');
    auto attribute = [&element](const QString& name, const QString& value) {
        element += QLatin1Char(' ') + name + QStringLiteral("=\"") + xmlText(value) +
                   QLatin1Char('"');
    };
    if (!annotation.author.isEmpty()) {
        attribute(QStringLiteral("author"), annotation.author);
    }
    if (annotation.created.isValid()) {
        attribute(QStringLiteral("created"), annotation.created.toUTC().toString(Qt::ISODate));
    }
    if (annotation.done) {
        attribute(QStringLiteral("done"), QStringLiteral("true"));
    }
    for (auto it = annotation.otherAttributes.cbegin(); it != annotation.otherAttributes.cend();
         ++it) {
        const bool known = it.key() == QStringLiteral("id") || it.key() == QStringLiteral("kind") ||
                           it.key() == QStringLiteral("author") ||
                           it.key() == QStringLiteral("done") ||
                           (it.key() == QStringLiteral("created") && annotation.created.isValid());
        if (!known) {
            attribute(it.key(), it.value());
        }
    }
    return element + QLatin1Char('>') + xmlText(annotation.text) +
           QStringLiteral("</annotation>");
}

}  // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================

KmlSerializer::KmlSerializer()
    : m_indented(false)
{
}

// =============================================================================
// Public Serialization Methods
// =============================================================================

QString KmlSerializer::toKml(const QTextDocument* document) const
{
    if (!document) {
        return QString();
    }

    // The whole text, without the paragraph separator that ends the document
    return toKml(document, 0, document->characterCount() - 1);
}

QString KmlSerializer::toKml(const QTextDocument* document, int from, int to) const
{
    if (!document) {
        return QString();
    }

    const int end = document->characterCount() - 1;
    from = std::clamp(from, 0, end);
    to = std::clamp(to, from, end);

    const QString newline = m_indented ? QStringLiteral("\n") : QString();
    const QString indent = m_indented ? QStringLiteral("  ") : QString();

    // Every block (paragraph) from the one holding the range start to the one holding its end
    QString paragraphs;
    AnnotationList annotations;
    const QTextBlock last = document->findBlock(to);
    for (QTextBlock block = document->findBlock(from); block.isValid(); block = block.next()) {
        paragraphs += indent + QStringLiteral("<p");
        paragraphs += serializeBlockAttributes(block);
        paragraphs += QStringLiteral(">");
        paragraphs += serializeBlockContent(block, from, to, &annotations);
        paragraphs += QStringLiteral("</p>") + newline;

        if (block == last) {
            break;
        }
    }

    // The annotations anchored in the paragraphs come first, so a reader knows them
    // when it meets their anchors
    QString result = QStringLiteral("<kml>") + newline;
    if (!annotations.isEmpty()) {
        result += indent + QStringLiteral("<annotations>") + newline;
        for (const Annotation& annotation : annotations) {
            result += indent + indent + annotationElement(annotation) + newline;
        }
        result += indent + QStringLiteral("</annotations>") + newline;
    }
    result += paragraphs;
    result += QStringLiteral("</kml>");

    return result;
}

QString KmlSerializer::blockToKml(const QTextBlock& block) const
{
    if (!block.isValid()) {
        return QString();
    }

    return serializeBlockContent(block, block.position(), block.position() + block.length() - 1,
                                 nullptr);
}

// =============================================================================
// Options
// =============================================================================

void KmlSerializer::setIndented(bool indented)
{
    m_indented = indented;
}

bool KmlSerializer::isIndented() const
{
    return m_indented;
}

// =============================================================================
// Internal Serialization Methods
// =============================================================================

QString KmlSerializer::serializeBlockAttributes(const QTextBlock& block) const
{
    QString attrs;

    const Qt::Alignment align = ownAlignment(block.blockFormat());

    if (align & Qt::AlignHCenter) {
        attrs += QStringLiteral(" align=\"center\"");
    } else if (align & Qt::AlignRight) {
        attrs += QStringLiteral(" align=\"right\"");
    } else if (align & Qt::AlignJustify) {
        attrs += QStringLiteral(" align=\"justify\"");
    } else if (align & Qt::AlignLeft) {
        attrs += QStringLiteral(" align=\"left\"");
    }
    // No attribute: a paragraph without its own alignment, shown with the default

    return attrs;
}

QString KmlSerializer::serializeBlockContent(const QTextBlock& block, int from, int to,
                                            AnnotationList* annotations) const
{
    QString result;

    // The annotations on the paragraph's start, when the range holds it
    if (block.position() >= from) {
        for (const Annotation& annotation : annotationsOf(block.charFormat())) {
            if (annotation.point) {
                result += anchorTag(annotation.id, true);
                noteAnnotation(annotations, annotation);
            }
        }
    }

    // Iterate through all fragments in the block, each cut to the range. Neighbouring
    // fragments with the same format (an edit splits a run into several) make one run;
    // so do those that differ only by annotations on places, written inside the run.
    QString runText;
    QTextCharFormat runFormat;
    RunPlaces runPlaces;
    auto flush = [&]() {
        result += serializeRun(runText, runFormat, runPlaces, annotations);
        runText.clear();
        runPlaces.clear();
    };
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const int start = std::max(fragment.position(), from);
        const int stop = std::min(fragment.position() + fragment.length(), to);
        if (start >= stop) {
            continue;
        }

        // An annotation on a place is on the character before it
        QTextCharFormat format = fragment.charFormat();
        AnnotationList fragmentAnnotations = annotationsOf(format);
        AnnotationList places;
        fragmentAnnotations.removeIf([&places](const Annotation& a) {
            if (a.point) {
                places.append(a);
            }
            return a.point;
        });
        setAnnotations(format, fragmentAnnotations);

        if (!runText.isEmpty() && format != runFormat) {
            flush();
        }
        if (runText.isEmpty()) {
            runFormat = format;
        }
        runText += fragment.text().mid(start - fragment.position(), stop - start);
        if (!places.isEmpty() && stop == fragment.position() + fragment.length()) {
            runPlaces.emplace_back(runText.size(), places);
        }
    }
    if (!runText.isEmpty()) {
        flush();
    }

    return result;
}

QString KmlSerializer::buildInlineStyleAttributes(const QTextCharFormat& format) const
{
    QString attrs;

    // Font family — emit when explicitly set
    const QStringList families = format.fontFamilies().toStringList();
    if (!families.isEmpty()) {
        attrs += QStringLiteral(" font=\"") +
                 KmlFormatRegistry::escapeXml(families.first()) +
                 QStringLiteral("\"");
    }

    // Font size — emit when explicitly set (0 means "use document default")
    const qreal pointSize = format.fontPointSize();
    if (pointSize > 0) {
        attrs += QStringLiteral(" size=\"") +
                 QString::number(pointSize) +
                 QStringLiteral("\"");
    }

    // Text color — emit when explicitly set and not default black
    if (format.foreground().style() != Qt::NoBrush) {
        const QColor color = format.foreground().color();
        if (color.isValid() && color != QColor(Qt::black)) {
            attrs += QStringLiteral(" color=\"") +
                     color.name() +
                     QStringLiteral("\"");
        }
    }

    // Background color — emit when explicitly set and not transparent
    if (format.background().style() != Qt::NoBrush) {
        const QColor bgColor = format.background().color();
        if (bgColor.isValid() && bgColor.alpha() > 0) {
            attrs += QStringLiteral(" bg=\"") +
                     bgColor.name() +
                     QStringLiteral("\"");
        }
    }

    return attrs;
}

QString KmlSerializer::serializeRun(const QString& text, const QTextCharFormat& format,
                                   const RunPlaces& places, AnnotationList* annotations) const
{
    // Handle special case: paragraph separator (0x2029)
    // These are inserted by Qt between blocks - skip them
    if (text == QString(QChar(0x2029))) {
        return QString();
    }

    // The fragments the run belongs to: their anchors enclose everything else
    const AnnotationList fragments = annotationsOf(format);
    for (const Annotation& annotation : fragments) {
        noteAnnotation(annotations, annotation);
    }

    // Escape XML special characters in text content; the annotations on places inside the
    // run go after their characters
    QString escapedText;
    qsizetype written = 0;
    for (const auto& [offset, placeAnnotations] : places) {
        escapedText += KmlFormatRegistry::escapeXml(text.mid(written, offset - written));
        written = offset;
        for (const Annotation& annotation : placeAnnotations) {
            escapedText += anchorTag(annotation.id, true);
            noteAnnotation(annotations, annotation);
        }
    }
    escapedText += KmlFormatRegistry::escapeXml(text.mid(written));

    // Build inline style attributes (font, size, color, bg)
    QString inlineAttrs = buildInlineStyleAttributes(format);

    // Build the result with formatting tags
    QString result;

    for (const Annotation& annotation : fragments) {
        result += anchorTag(annotation.id, false);
    }

    // Check for metadata first (wraps around formatting)
    bool hasMeta = hasMetadata(format);
    if (hasMeta) {
        result += metadataToOpenTag(format);
    }

    // Get formatting tags
    QString openTags = KmlFormatRegistry::formatToOpenTags(format);
    QString closeTags = KmlFormatRegistry::formatToCloseTags(format);

    if (!openTags.isEmpty() && !inlineAttrs.isEmpty()) {
        // Inject attributes into the first formatting tag: <b> → <b font="...">
        int firstClose = openTags.indexOf(QLatin1Char('>'));
        if (firstClose >= 0) {
            openTags.insert(firstClose, inlineAttrs);
        }
        result += openTags;
        result += escapedText;
        result += closeTags;
    } else if (openTags.isEmpty() && !inlineAttrs.isEmpty()) {
        // No formatting tags but has style overrides — wrap with <span>
        result += QStringLiteral("<span") + inlineAttrs + QStringLiteral(">");
        result += escapedText;
        result += QStringLiteral("</span>");
    } else {
        // Standard case: formatting tags without style attributes, or plain text
        result += openTags;
        result += escapedText;
        result += closeTags;
    }

    // Close metadata tag if present
    if (hasMeta) {
        result += metadataToCloseTag(format);
    }

    for (qsizetype i = 0; i < fragments.size(); ++i) {
        result += QStringLiteral("</anchor>");
    }

    return result;
}

// Note: formatToOpenTags and formatToCloseTags are now provided by KmlFormatRegistry

bool KmlSerializer::hasMetadata(const QTextCharFormat& format) const
{
    // Check if any metadata property is set
    return format.property(KmlPropFootnote).isValid() ||
           format.property(KmlPropCharRef).isValid() ||
           format.property(KmlPropLocRef).isValid();
}

QString KmlSerializer::metadataToOpenTag(const QTextCharFormat& format) const
{
    // Every metadata property present opens its own tag - nested in definition order -
    // with all of its attributes (KmlFormatRegistry escapes the values)
    QString tags;
    for (const auto& def : KmlFormatRegistry::metadataTagDefinitions()) {
        const QVariant metadata = format.property(def.propertyId);
        if (metadata.isValid()) {
            tags += QLatin1Char('<') + def.tagName +
                    KmlFormatRegistry::writeMetadataAttributes(def.tagName, metadata.toMap()) +
                    QLatin1Char('>');
        }
    }
    return tags;
}

QString KmlSerializer::metadataToCloseTag(const QTextCharFormat& format) const
{
    // Close the tags opened by metadataToOpenTag(), innermost first
    QString tags;
    const auto& defs = KmlFormatRegistry::metadataTagDefinitions();
    for (auto it = defs.crbegin(); it != defs.crend(); ++it) {
        if (format.property(it->propertyId).isValid()) {
            tags += QStringLiteral("</") + it->tagName + QLatin1Char('>');
        }
    }
    return tags;
}

// Note: escapeXml is now provided by KmlFormatRegistry::escapeXml()

} // namespace editor
} // namespace kalahari
