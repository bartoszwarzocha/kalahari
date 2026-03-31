/// @file kml_serializer.cpp
/// @brief KML Serializer implementation (OpenSpec #00043 Phase 11.2)
///
/// Serializes QTextDocument content to KML (Kalahari Markup Language) format.
/// This is the reverse operation of KmlParser.

#include <kalahari/editor/kml_serializer.h>
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

namespace kalahari {
namespace editor {

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

    QString result;
    const QString newline = m_indented ? QStringLiteral("\n") : QString();
    const QString indent = m_indented ? QStringLiteral("  ") : QString();

    // Start document
    result += QStringLiteral("<kml>") + newline;

    // Iterate through all blocks (paragraphs)
    QTextBlock block = document->begin();
    while (block.isValid()) {
        result += indent + QStringLiteral("<p");
        result += serializeBlockAttributes(block);
        result += QStringLiteral(">");
        result += serializeBlockContent(block);
        result += QStringLiteral("</p>") + newline;

        block = block.next();
    }

    // End document
    result += QStringLiteral("</kml>");

    return result;
}

QString KmlSerializer::blockToKml(const QTextBlock& block) const
{
    if (!block.isValid()) {
        return QString();
    }

    return serializeBlockContent(block);
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

    QTextBlockFormat blockFormat = block.blockFormat();
    Qt::Alignment align = blockFormat.alignment();

    if (align & Qt::AlignHCenter) {
        attrs += QStringLiteral(" align=\"center\"");
    } else if (align & Qt::AlignRight) {
        attrs += QStringLiteral(" align=\"right\"");
    } else if (align & Qt::AlignJustify) {
        attrs += QStringLiteral(" align=\"justify\"");
    }
    // Left alignment is default — no attribute emitted

    return attrs;
}

QString KmlSerializer::serializeBlockContent(const QTextBlock& block) const
{
    QString result;

    // Iterate through all fragments in the block
    QTextBlock::iterator it;
    for (it = block.begin(); !it.atEnd(); ++it) {
        QTextFragment fragment = it.fragment();
        if (fragment.isValid()) {
            result += serializeFragment(fragment);
        }
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

QString KmlSerializer::serializeFragment(const QTextFragment& fragment) const
{
    if (!fragment.isValid()) {
        return QString();
    }

    QString text = fragment.text();
    QTextCharFormat format = fragment.charFormat();

    // Handle special case: paragraph separator (0x2029)
    // These are inserted by Qt between blocks - skip them
    if (text == QString(QChar(0x2029))) {
        return QString();
    }

    // Escape XML special characters in text content
    QString escapedText = KmlFormatRegistry::escapeXml(text);

    // Build inline style attributes (font, size, color, bg)
    QString inlineAttrs = buildInlineStyleAttributes(format);

    // Build the result with formatting tags
    QString result;

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

    return result;
}

// Note: formatToOpenTags and formatToCloseTags are now provided by KmlFormatRegistry

bool KmlSerializer::hasMetadata(const QTextCharFormat& format) const
{
    // Check if any metadata property is set
    return format.property(KmlPropComment).isValid() ||
           format.property(KmlPropTodo).isValid() ||
           format.property(KmlPropFootnote).isValid() ||
           format.property(KmlPropCharRef).isValid() ||
           format.property(KmlPropLocRef).isValid();
}

QString KmlSerializer::metadataToOpenTag(const QTextCharFormat& format) const
{
    QString tag;

    // Check each metadata type and build appropriate tag
    // Using KmlFormatRegistry::escapeXml for attribute value escaping

    // Comment
    QVariant commentVar = format.property(KmlPropComment);
    if (commentVar.isValid()) {
        QVariantMap meta = commentVar.toMap();
        tag = QStringLiteral("<comment");

        if (meta.contains(QStringLiteral("id"))) {
            tag += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("id")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("author"))) {
            tag += QStringLiteral(" author=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("author")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("created"))) {
            tag += QStringLiteral(" created=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("created")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("resolved")) && meta[QStringLiteral("resolved")].toBool()) {
            tag += QStringLiteral(" resolved=\"true\"");
        }

        tag += QStringLiteral(">");
        return tag;
    }

    // Todo
    QVariant todoVar = format.property(KmlPropTodo);
    if (todoVar.isValid()) {
        QVariantMap meta = todoVar.toMap();
        tag = QStringLiteral("<todo");

        if (meta.contains(QStringLiteral("id"))) {
            tag += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("id")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("completed")) && meta[QStringLiteral("completed")].toBool()) {
            tag += QStringLiteral(" completed=\"true\"");
        }
        if (meta.contains(QStringLiteral("priority"))) {
            tag += QStringLiteral(" priority=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("priority")].toString()) + QStringLiteral("\"");
        }

        tag += QStringLiteral(">");
        return tag;
    }

    // Footnote
    QVariant footnoteVar = format.property(KmlPropFootnote);
    if (footnoteVar.isValid()) {
        QVariantMap meta = footnoteVar.toMap();
        tag = QStringLiteral("<footnote");

        if (meta.contains(QStringLiteral("id"))) {
            tag += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("id")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("number"))) {
            tag += QStringLiteral(" number=\"") + QString::number(meta[QStringLiteral("number")].toInt()) + QStringLiteral("\"");
        }

        tag += QStringLiteral(">");
        return tag;
    }

    // Character reference
    QVariant charRefVar = format.property(KmlPropCharRef);
    if (charRefVar.isValid()) {
        QVariantMap meta = charRefVar.toMap();
        tag = QStringLiteral("<charref");

        if (meta.contains(QStringLiteral("id"))) {
            tag += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("id")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("target"))) {
            tag += QStringLiteral(" target=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("target")].toString()) + QStringLiteral("\"");
        }

        tag += QStringLiteral(">");
        return tag;
    }

    // Location reference
    QVariant locRefVar = format.property(KmlPropLocRef);
    if (locRefVar.isValid()) {
        QVariantMap meta = locRefVar.toMap();
        tag = QStringLiteral("<locref");

        if (meta.contains(QStringLiteral("id"))) {
            tag += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("id")].toString()) + QStringLiteral("\"");
        }
        if (meta.contains(QStringLiteral("target"))) {
            tag += QStringLiteral(" target=\"") + KmlFormatRegistry::escapeXml(meta[QStringLiteral("target")].toString()) + QStringLiteral("\"");
        }

        tag += QStringLiteral(">");
        return tag;
    }

    return tag;
}

QString KmlSerializer::metadataToCloseTag(const QTextCharFormat& format) const
{
    // Return appropriate closing tag based on which metadata is present

    if (format.property(KmlPropComment).isValid()) {
        return QStringLiteral("</comment>");
    }
    if (format.property(KmlPropTodo).isValid()) {
        return QStringLiteral("</todo>");
    }
    if (format.property(KmlPropFootnote).isValid()) {
        return QStringLiteral("</footnote>");
    }
    if (format.property(KmlPropCharRef).isValid()) {
        return QStringLiteral("</charref>");
    }
    if (format.property(KmlPropLocRef).isValid()) {
        return QStringLiteral("</locref>");
    }

    return QString();
}

// Note: escapeXml is now provided by KmlFormatRegistry::escapeXml()

} // namespace editor
} // namespace kalahari
