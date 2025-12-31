/// @file kml_format_registry.cpp
/// @brief KML Format Registry implementation (OpenSpec #00043)
///
/// Centralized KML tag to QTextCharFormat mappings, providing a single
/// source of truth for both KmlParser and KmlSerializer.

#include <kalahari/editor/kml_format_registry.h>
#include <QFont>
#include <QSet>

namespace kalahari {
namespace editor {
namespace KmlFormatRegistry {

// =============================================================================
// Static Data
// =============================================================================

namespace {

/// @brief Set of all known formatting tags (all aliases)
const QSet<QString>& formattingTagSet()
{
    static const QSet<QString> tags = {
        // Bold variants
        QStringLiteral("b"), QStringLiteral("bold"), QStringLiteral("strong"),
        // Italic variants
        QStringLiteral("i"), QStringLiteral("italic"), QStringLiteral("em"),
        // Underline variants
        QStringLiteral("u"), QStringLiteral("underline"),
        // Strikethrough variants
        QStringLiteral("s"), QStringLiteral("strike"), QStringLiteral("strikethrough"),
        // Subscript variants
        QStringLiteral("sub"), QStringLiteral("subscript"),
        // Superscript variants
        QStringLiteral("sup"), QStringLiteral("superscript")
    };
    return tags;
}

/// @brief Set of all known metadata tags
const QSet<QString>& metadataTagSet()
{
    static const QSet<QString> tags = {
        QStringLiteral("comment"),
        QStringLiteral("todo"),
        QStringLiteral("footnote"),
        QStringLiteral("charref"),
        QStringLiteral("locref")
    };
    return tags;
}

/// @brief Static metadata tag definitions
const QVector<MetadataTagDef>& metadataDefinitions()
{
    static const QVector<MetadataTagDef> defs = {
        {
            QStringLiteral("comment"),
            KmlPropComment,
            {QStringLiteral("id"), QStringLiteral("author"),
             QStringLiteral("created"), QStringLiteral("resolved")}
        },
        {
            QStringLiteral("todo"),
            KmlPropTodo,
            {QStringLiteral("id"), QStringLiteral("completed"),
             QStringLiteral("priority")}
        },
        {
            QStringLiteral("footnote"),
            KmlPropFootnote,
            {QStringLiteral("id"), QStringLiteral("number")}
        },
        {
            QStringLiteral("charref"),
            KmlPropCharRef,
            {QStringLiteral("id"), QStringLiteral("target")}
        },
        {
            QStringLiteral("locref"),
            KmlPropLocRef,
            {QStringLiteral("id"), QStringLiteral("target")}
        }
    };
    return defs;
}

} // anonymous namespace

// =============================================================================
// Formatting Tags Implementation
// =============================================================================

bool isFormattingTag(const QString& tag)
{
    return formattingTagSet().contains(tag);
}

QTextCharFormat applyTagFormat(const QString& tag, const QTextCharFormat& base)
{
    QTextCharFormat format = base;

    // Bold variants
    if (tag == QStringLiteral("b") ||
        tag == QStringLiteral("bold") ||
        tag == QStringLiteral("strong")) {
        format.setFontWeight(QFont::Bold);
    }
    // Italic variants
    else if (tag == QStringLiteral("i") ||
             tag == QStringLiteral("italic") ||
             tag == QStringLiteral("em")) {
        format.setFontItalic(true);
    }
    // Underline variants
    else if (tag == QStringLiteral("u") ||
             tag == QStringLiteral("underline")) {
        format.setFontUnderline(true);
    }
    // Strikethrough variants
    else if (tag == QStringLiteral("s") ||
             tag == QStringLiteral("strike") ||
             tag == QStringLiteral("strikethrough")) {
        format.setFontStrikeOut(true);
    }
    // Subscript variants
    else if (tag == QStringLiteral("sub") ||
             tag == QStringLiteral("subscript")) {
        format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    }
    // Superscript variants
    else if (tag == QStringLiteral("sup") ||
             tag == QStringLiteral("superscript")) {
        format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    }

    return format;
}

QString canonicalFormattingTag(const QString& tag)
{
    // Bold variants -> "b"
    if (tag == QStringLiteral("b") ||
        tag == QStringLiteral("bold") ||
        tag == QStringLiteral("strong")) {
        return QStringLiteral("b");
    }
    // Italic variants -> "i"
    if (tag == QStringLiteral("i") ||
        tag == QStringLiteral("italic") ||
        tag == QStringLiteral("em")) {
        return QStringLiteral("i");
    }
    // Underline variants -> "u"
    if (tag == QStringLiteral("u") ||
        tag == QStringLiteral("underline")) {
        return QStringLiteral("u");
    }
    // Strikethrough variants -> "s"
    if (tag == QStringLiteral("s") ||
        tag == QStringLiteral("strike") ||
        tag == QStringLiteral("strikethrough")) {
        return QStringLiteral("s");
    }
    // Subscript variants -> "sub"
    if (tag == QStringLiteral("sub") ||
        tag == QStringLiteral("subscript")) {
        return QStringLiteral("sub");
    }
    // Superscript variants -> "sup"
    if (tag == QStringLiteral("sup") ||
        tag == QStringLiteral("superscript")) {
        return QStringLiteral("sup");
    }

    // Not a formatting tag
    return QString();
}

QString formatToOpenTags(const QTextCharFormat& format)
{
    QString tags;

    // Bold check
    if (format.fontWeight() >= QFont::Bold) {
        tags += QStringLiteral("<b>");
    }

    // Italic check
    if (format.fontItalic()) {
        tags += QStringLiteral("<i>");
    }

    // Underline check
    if (format.fontUnderline()) {
        tags += QStringLiteral("<u>");
    }

    // Strikethrough check
    if (format.fontStrikeOut()) {
        tags += QStringLiteral("<s>");
    }

    // Subscript/Superscript check
    if (format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
        tags += QStringLiteral("<sub>");
    } else if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript) {
        tags += QStringLiteral("<sup>");
    }

    return tags;
}

QString formatToCloseTags(const QTextCharFormat& format)
{
    QString tags;

    // Close in reverse order for proper nesting

    // Subscript/Superscript close
    if (format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
        tags += QStringLiteral("</sub>");
    } else if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript) {
        tags += QStringLiteral("</sup>");
    }

    // Strikethrough close
    if (format.fontStrikeOut()) {
        tags += QStringLiteral("</s>");
    }

    // Underline close
    if (format.fontUnderline()) {
        tags += QStringLiteral("</u>");
    }

    // Italic close
    if (format.fontItalic()) {
        tags += QStringLiteral("</i>");
    }

    // Bold close
    if (format.fontWeight() >= QFont::Bold) {
        tags += QStringLiteral("</b>");
    }

    return tags;
}

// =============================================================================
// Metadata Tags Implementation
// =============================================================================

bool isMetadataTag(const QString& tag)
{
    return metadataTagSet().contains(tag);
}

const QVector<MetadataTagDef>& metadataTagDefinitions()
{
    return metadataDefinitions();
}

const MetadataTagDef* getMetadataTagDef(const QString& tag)
{
    const auto& defs = metadataDefinitions();
    for (const auto& def : defs) {
        if (def.tagName == tag) {
            return &def;
        }
    }
    return nullptr;
}

const MetadataTagDef* getMetadataTagDefByProperty(KmlPropertyId propId)
{
    const auto& defs = metadataDefinitions();
    for (const auto& def : defs) {
        if (def.propertyId == propId) {
            return &def;
        }
    }
    return nullptr;
}

// =============================================================================
// Utilities Implementation
// =============================================================================

QString escapeXml(const QString& text)
{
    QString result = text;

    // Order matters: & must be replaced first to avoid double-escaping
    result.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    result.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    result.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    result.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    result.replace(QLatin1Char('\''), QStringLiteral("&apos;"));

    return result;
}

QString unescapeXml(const QString& text)
{
    QString result = text;

    // Order matters: &amp; must be replaced last to avoid incorrect unescaping
    result.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    result.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    result.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    result.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    result.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    return result;
}

} // namespace KmlFormatRegistry
} // namespace editor
} // namespace kalahari
