/// @file kml_format_registry.h
/// @brief KML Format Registry - Single source of truth for KML tag mappings (OpenSpec #00043)
///
/// Provides centralized definitions for KML tag to QTextCharFormat mappings,
/// used by both KmlParser and KmlSerializer to ensure consistency.

#pragma once

#include <QString>
#include <QStringList>
#include <QTextCharFormat>
#include <QVector>

namespace kalahari {
namespace editor {

/// @brief Custom property IDs for KML metadata in QTextCharFormat
///
/// These properties allow storing KML-specific data (comments, todos, etc.)
/// inline with the text formatting, enabling round-trip serialization.
/// Property IDs start at QTextFormat::UserProperty + 100 to avoid conflicts.
///
/// @note Names use "Prop" prefix to avoid conflict with existing KmlComment class
enum KmlPropertyId {
    /// @brief Comment annotation attached to text range
    KmlPropComment = QTextFormat::UserProperty + 100,

    /// @brief TODO marker at text position
    KmlPropTodo = QTextFormat::UserProperty + 101,

    /// @brief Footnote reference
    KmlPropFootnote = QTextFormat::UserProperty + 102,

    /// @brief Character reference (link to Character entity)
    KmlPropCharRef = QTextFormat::UserProperty + 103,

    /// @brief Location reference (link to Location entity)
    KmlPropLocRef = QTextFormat::UserProperty + 104,
};

/// @brief Metadata tag definition
struct MetadataTagDef {
    QString tagName;              ///< Tag name in KML (e.g., "comment", "todo")
    KmlPropertyId propertyId;     ///< QTextFormat property ID
    QStringList knownAttributes;  ///< Known attributes for this tag
};

/// @brief KML Format Registry - centralized tag/format mappings
///
/// This namespace provides functions for converting between KML tags and
/// QTextCharFormat properties. Both KmlParser and KmlSerializer should use
/// these functions to ensure consistent behavior.
namespace KmlFormatRegistry {

// =============================================================================
// Formatting Tags
// =============================================================================

/// @brief Check if tag is a known formatting tag
/// @param tag The tag name (e.g., "b", "bold", "strong", "i", "italic", etc.)
/// @return true if tag is a recognized formatting tag
bool isFormattingTag(const QString& tag);

/// @brief Apply formatting tag to a base format
/// @param tag The formatting tag name
/// @param base Base format to apply modifications to (default: empty format)
/// @return Modified format with tag's styling applied
QTextCharFormat applyTagFormat(const QString& tag,
                                const QTextCharFormat& base = QTextCharFormat());

/// @brief Get canonical tag name for a formatting tag
/// @param tag Any valid formatting tag (e.g., "bold", "strong", "b")
/// @return Canonical short form (e.g., "b", "i", "u", "s", "sub", "sup")
///         or empty string if not a formatting tag
QString canonicalFormattingTag(const QString& tag);

/// @brief Get opening tags for format properties
/// @param format The text format to serialize
/// @return Opening tags string (e.g., "<b><i>")
QString formatToOpenTags(const QTextCharFormat& format);

/// @brief Get closing tags for format properties (reverse order)
/// @param format The text format to serialize
/// @return Closing tags string in reverse order (e.g., "</i></b>")
QString formatToCloseTags(const QTextCharFormat& format);

// =============================================================================
// Metadata Tags
// =============================================================================

/// @brief Check if tag is a known metadata tag
/// @param tag The tag name (e.g., "comment", "todo", "footnote")
/// @return true if tag is a recognized metadata tag
bool isMetadataTag(const QString& tag);

/// @brief Get all metadata tag definitions
/// @return Vector of all supported metadata tag definitions
const QVector<MetadataTagDef>& metadataTagDefinitions();

/// @brief Get metadata tag definition by tag name
/// @param tag The tag name
/// @return Pointer to definition, or nullptr if not found
const MetadataTagDef* getMetadataTagDef(const QString& tag);

/// @brief Get metadata tag definition by property ID
/// @param propId The KmlPropertyId
/// @return Pointer to definition, or nullptr if not found
const MetadataTagDef* getMetadataTagDefByProperty(KmlPropertyId propId);

// =============================================================================
// Utilities
// =============================================================================

/// @brief Escape XML special characters
/// @param text Input text
/// @return Text with &, <, >, ", ' escaped to XML entities
QString escapeXml(const QString& text);

/// @brief Unescape XML entities
/// @param text Input text with XML entities
/// @return Text with entities converted back to characters
QString unescapeXml(const QString& text);

} // namespace KmlFormatRegistry

} // namespace editor
} // namespace kalahari
