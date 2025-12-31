/// @file kml_serializer.h
/// @brief KML Serializer - serializes QTextDocument back to KML (OpenSpec #00043 Phase 11.2)
///
/// KmlSerializer converts QTextDocument content to KML (Kalahari Markup Language) format.
/// This is the reverse operation of KmlParser.
///
/// Key design principles (Phase 11 Architecture Correction):
/// - Direct QTextDocument -> KML serialization
/// - Standard formatting via QTextCharFormat (bold, italic, underline, strikethrough)
/// - Metadata (comments, todos, footnotes) via QTextFormat::UserProperty
///
/// Output KML structure:
/// @code
/// <kml>
///   <p>Paragraph with <b>bold</b> and <i>italic</i> text.</p>
///   <p>Formula: H<sub>2</sub>O and x<sup>2</sup></p>
///   <p>Text with <comment id="c1">annotated</comment> word.</p>
/// </kml>
/// @endcode

#pragma once

#include <kalahari/editor/kml_format_registry.h>  // For KmlPropertyId and format functions
#include <QString>
#include <QTextCharFormat>

class QTextDocument;
class QTextBlock;
class QTextFragment;

namespace kalahari {
namespace editor {

/// @brief Serializer for converting QTextDocument to KML format
///
/// Converts QTextDocument content back to KML markup. Uses QTextCharFormat
/// properties for both formatting (bold, italic, etc.) and metadata
/// (comments, todos, footnotes).
///
/// Example usage:
/// @code
/// KmlSerializer serializer;
///
/// // Basic serialization
/// QString kml = serializer.toKml(document);
///
/// // Pretty-printed output
/// serializer.setIndented(true);
/// QString prettyKml = serializer.toKml(document);
/// @endcode
class KmlSerializer {
public:
    /// @brief Default constructor
    explicit KmlSerializer();

    /// @brief Destructor
    ~KmlSerializer() = default;

    // =========================================================================
    // Main Serialization Methods
    // =========================================================================

    /// @brief Serialize QTextDocument to KML string
    /// @param document The document to serialize
    /// @return KML markup string
    QString toKml(const QTextDocument* document) const;

    /// @brief Serialize a single block (paragraph) to KML
    /// @param block The text block to serialize
    /// @return KML paragraph content (without <p> wrapper)
    QString blockToKml(const QTextBlock& block) const;

    // =========================================================================
    // Options
    // =========================================================================

    /// @brief Enable or disable indented (pretty-printed) output
    /// @param indented true for indented output, false for compact
    void setIndented(bool indented);

    /// @brief Check if indented output is enabled
    /// @return true if indented output is enabled
    bool isIndented() const;

private:
    // =========================================================================
    // Internal Serialization Methods
    // =========================================================================

    /// @brief Serialize a text block's inline content
    /// @param block The text block to serialize
    /// @return KML markup for the block content
    QString serializeBlockContent(const QTextBlock& block) const;

    /// @brief Serialize a text fragment with its format
    /// @param fragment The text fragment to serialize
    /// @return KML markup for the fragment
    /// @note Uses KmlFormatRegistry for format serialization
    QString serializeFragment(const QTextFragment& fragment) const;

    // Note: formatToOpenTags and formatToCloseTags are now provided by KmlFormatRegistry

    /// @brief Check if format has metadata properties
    /// @param format The character format to check
    /// @return true if format contains any metadata property
    bool hasMetadata(const QTextCharFormat& format) const;

    /// @brief Generate metadata opening tag with attributes
    /// @param format The character format containing metadata
    /// @return Opening metadata tag (e.g., "<comment id=\"c1\">")
    QString metadataToOpenTag(const QTextCharFormat& format) const;

    /// @brief Generate metadata closing tag
    /// @param format The character format containing metadata
    /// @return Closing metadata tag (e.g., "</comment>")
    QString metadataToCloseTag(const QTextCharFormat& format) const;

    // Note: escapeXml is now provided by KmlFormatRegistry::escapeXml()

    // =========================================================================
    // Members
    // =========================================================================

    bool m_indented;  ///< Whether to produce indented (pretty-printed) output
};

} // namespace editor
} // namespace kalahari
