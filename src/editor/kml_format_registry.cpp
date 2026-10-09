/// @file kml_format_registry.cpp
/// @brief KML Format Registry implementation (OpenSpec #00043)
///
/// Centralized KML tag to QTextCharFormat mappings, providing a single
/// source of truth for both the KML reader (KmlDocumentModel) and KmlSerializer.

#include <kalahari/editor/kml_format_registry.h>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QSet>
#include <QXmlStreamAttributes>

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
        QStringLiteral("sup"), QStringLiteral("superscript"),
        // Span — inline style carrier (no formatting of its own)
        QStringLiteral("span")
    };
    return tags;
}

/// @brief Set of all known metadata tags
const QSet<QString>& metadataTagSet()
{
    static const QSet<QString> tags = {
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

/// @brief Metadata attributes stored as int
bool isNumberAttribute(const QString& name)
{
    return name == QStringLiteral("number");
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

void applyInlineStyleAttributes(const QXmlStreamAttributes& attrs,
                                QTextCharFormat& format)
{
    // Font family
    if (attrs.hasAttribute(QStringLiteral("font"))) {
        format.setFontFamilies({attrs.value(QStringLiteral("font")).toString()});
    }

    // Font size (point size)
    if (attrs.hasAttribute(QStringLiteral("size"))) {
        bool ok = false;
        const qreal size = attrs.value(QStringLiteral("size")).toDouble(&ok);
        if (ok && size > 0) {
            format.setFontPointSize(size);
        }
    }

    // Text color
    if (attrs.hasAttribute(QStringLiteral("color"))) {
        const QColor color(attrs.value(QStringLiteral("color")).toString());
        if (color.isValid()) {
            format.setForeground(QBrush(color));
        }
    }

    // Background color
    if (attrs.hasAttribute(QStringLiteral("bg"))) {
        const QColor bgColor(attrs.value(QStringLiteral("bg")).toString());
        if (bgColor.isValid()) {
            format.setBackground(QBrush(bgColor));
        }
    }
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

QVariantMap readMetadataAttributes(const QXmlStreamAttributes& attrs)
{
    QVariantMap metadata;
    for (const QXmlStreamAttribute& attr : attrs) {
        const QString name = attr.name().toString();
        const QString value = attr.value().toString();
        if (isNumberAttribute(name)) {
            bool ok = false;
            const int number = value.toInt(&ok);
            metadata[name] = ok ? QVariant(number) : QVariant(value);
        } else {
            metadata[name] = value;
        }
    }
    return metadata;
}

QString writeMetadataAttributes(const QString& tag, const QVariantMap& metadata)
{
    QString result;
    auto write = [&result](const QString& name, const QVariant& value) {
        const QString text = value.toString();
        if (text.isEmpty()) {
            return;
        }
        result += QLatin1Char(' ') + name + QStringLiteral("=\"") + escapeXml(text) +
                  QLatin1Char('"');
    };

    QStringList known;
    if (const MetadataTagDef* def = getMetadataTagDef(tag)) {
        known = def->knownAttributes;
    }
    for (const QString& name : known) {
        const auto it = metadata.constFind(name);
        if (it != metadata.constEnd()) {
            write(name, it.value());
        }
    }
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        if (!known.contains(it.key())) {
            write(it.key(), it.value());
        }
    }
    return result;
}

// =============================================================================
// Document Structure Implementation
// =============================================================================

QString withRootElement(const QString& kml)
{
    QString content = kml.trimmed();
    // "<doc" covers "<document" too
    if (!content.startsWith(QStringLiteral("<kml")) && !content.startsWith(QStringLiteral("<doc"))) {
        content = QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
    }
    return content;
}

bool isInlineTextTag(const QString& tag)
{
    return isFormattingTag(tag) || isMetadataTag(tag) || tag == QStringLiteral("anchor") ||
           tag == QStringLiteral("t") || tag == QStringLiteral("text");
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
