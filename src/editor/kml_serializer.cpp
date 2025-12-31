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
#include <QFont>
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
        result += indent + QStringLiteral("<p>");
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

    // Build the result with formatting tags
    QString result;

    // Check for metadata first (wraps around formatting)
    bool hasMeta = hasMetadata(format);
    if (hasMeta) {
        result += metadataToOpenTag(format);
    }

    // Add formatting opening tags (using registry)
    result += KmlFormatRegistry::formatToOpenTags(format);

    // Add the text content
    result += escapedText;

    // Add formatting closing tags (reverse order, using registry)
    result += KmlFormatRegistry::formatToCloseTags(format);

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
