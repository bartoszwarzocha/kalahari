/// @file format_converter.h
/// @brief Utilities to convert KML elements to Qt text formats (OpenSpec #00042 Phase 2.2)
///
/// FormatConverter provides functions to convert KML inline elements (bold, italic, etc.)
/// to QTextCharFormat for use with QTextLayout. This bridges the KML model layer
/// with the Qt layout engine.
///
/// Key responsibilities:
/// - Convert KmlElement formatting to QTextCharFormat
/// - Build format ranges from KmlParagraph elements
/// - Support nested formatting (bold inside italic, etc.)

#pragma once

#include <kalahari/editor/kml_element.h>
#include <kalahari/editor/kml_paragraph.h>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QList>
#include <QFont>

namespace kalahari::editor {

/// @brief Utilities for converting KML formatting to Qt text formats
///
/// FormatConverter provides static functions to translate KML inline elements
/// (bold, italic, underline, strikethrough, sub/superscript) into
/// QTextCharFormat objects that can be applied to QTextLayout.
///
/// Usage:
/// @code
/// KmlParagraph paragraph("Hello <b>world</b>");
/// QFont baseFont("Serif", 12);
/// auto formats = FormatConverter::buildFormatRanges(paragraph, baseFont);
/// layout.setFormats(formats);
/// @endcode
class FormatConverter {
public:
    /// @brief Build format ranges for a paragraph's inline elements
    /// @param paragraph The KML paragraph to analyze
    /// @param baseFont The base font to apply formatting on top of
    /// @return List of format ranges suitable for QTextLayout::setFormats()
    ///
    /// This function traverses all elements in the paragraph and creates
    /// format ranges for each styled element. Nested elements accumulate
    /// their formatting (e.g., bold inside italic = bold+italic).
    static QList<QTextLayout::FormatRange> buildFormatRanges(
        const KmlParagraph& paragraph,
        const QFont& baseFont);

    /// @brief Convert an ElementType to a QTextCharFormat
    /// @param type The element type (Bold, Italic, etc.)
    /// @param baseFont The base font to modify
    /// @return QTextCharFormat with the appropriate styling applied
    ///
    /// This function creates a format for a single element type:
    /// - Bold: Sets font weight to Bold
    /// - Italic: Sets font italic
    /// - Underline: Sets font underline
    /// - Strikethrough: Sets font strikeout
    /// - Subscript: Reduces font size and lowers baseline
    /// - Superscript: Reduces font size and raises baseline
    static QTextCharFormat elementTypeToFormat(ElementType type, const QFont& baseFont);

    /// @brief Create a combined format from multiple element types
    /// @param types List of element types to combine
    /// @param baseFont The base font to modify
    /// @return QTextCharFormat with all styles applied
    ///
    /// Use this for nested formatting like <b><i>bold italic</i></b>
    static QTextCharFormat combineFormats(
        const QList<ElementType>& types,
        const QFont& baseFont);

    /// @brief Apply ElementType formatting to an existing format
    /// @param format The format to modify (in place)
    /// @param type The element type to apply
    /// @param baseFont The base font (used for size calculations)
    static void applyElementType(
        QTextCharFormat& format,
        ElementType type,
        const QFont& baseFont);

private:
    /// @brief Recursive helper to traverse elements and build format ranges
    /// @param element The element to process
    /// @param offset Current character offset in the paragraph
    /// @param activeTypes Stack of currently active formatting types
    /// @param baseFont The base font for format calculations
    /// @param result Output list of format ranges
    /// @return Updated offset after processing this element
    static int processElement(
        const KmlElement* element,
        int offset,
        QList<ElementType>& activeTypes,
        const QFont& baseFont,
        QList<QTextLayout::FormatRange>& result);

    /// @brief Scaling factor for subscript/superscript font size
    static constexpr qreal SCRIPT_SIZE_FACTOR = 0.7;

    /// @brief Vertical offset factor for superscript (relative to font size)
    static constexpr qreal SUPERSCRIPT_OFFSET = -0.4;

    /// @brief Vertical offset factor for subscript (relative to font size)
    static constexpr qreal SUBSCRIPT_OFFSET = 0.2;
};

}  // namespace kalahari::editor
