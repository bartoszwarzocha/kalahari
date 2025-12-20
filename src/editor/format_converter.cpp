/// @file format_converter.cpp
/// @brief Implementation of FormatConverter (OpenSpec #00042 Phase 2.2)

#include <kalahari/editor/format_converter.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <kalahari/editor/kml_text_run.h>

namespace kalahari::editor {

// =============================================================================
// Public Methods
// =============================================================================

QList<QTextLayout::FormatRange> FormatConverter::buildFormatRanges(
    const KmlParagraph& paragraph,
    const QFont& baseFont)
{
    QList<QTextLayout::FormatRange> result;
    QList<ElementType> activeTypes;

    int offset = 0;
    for (const auto& element : paragraph.elements()) {
        offset = processElement(element.get(), offset, activeTypes, baseFont, result);
    }

    return result;
}

QTextCharFormat FormatConverter::elementTypeToFormat(ElementType type, const QFont& baseFont)
{
    QTextCharFormat format;
    applyElementType(format, type, baseFont);
    return format;
}

QTextCharFormat FormatConverter::combineFormats(
    const QList<ElementType>& types,
    const QFont& baseFont)
{
    QTextCharFormat format;
    for (ElementType type : types) {
        applyElementType(format, type, baseFont);
    }
    return format;
}

void FormatConverter::applyElementType(
    QTextCharFormat& format,
    ElementType type,
    const QFont& baseFont)
{
    switch (type) {
        case ElementType::Bold:
            format.setFontWeight(QFont::Bold);
            break;

        case ElementType::Italic:
            format.setFontItalic(true);
            break;

        case ElementType::Underline:
            format.setFontUnderline(true);
            break;

        case ElementType::Strikethrough:
            format.setFontStrikeOut(true);
            break;

        case ElementType::Subscript: {
            // Reduce font size
            qreal newSize = baseFont.pointSizeF() * SCRIPT_SIZE_FACTOR;
            if (newSize > 0) {
                format.setFontPointSize(newSize);
            }
            // Set vertical alignment for subscript
            format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
            break;
        }

        case ElementType::Superscript: {
            // Reduce font size
            qreal newSize = baseFont.pointSizeF() * SCRIPT_SIZE_FACTOR;
            if (newSize > 0) {
                format.setFontPointSize(newSize);
            }
            // Set vertical alignment for superscript
            format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            break;
        }

        case ElementType::Text:
        case ElementType::Link:
        case ElementType::CharacterStyle:
            // Text has no additional formatting
            // Link and CharacterStyle would need additional handling
            break;
    }
}

// =============================================================================
// Private Methods
// =============================================================================

int FormatConverter::processElement(
    const KmlElement* element,
    int offset,
    QList<ElementType>& activeTypes,
    const QFont& baseFont,
    QList<QTextLayout::FormatRange>& result)
{
    if (!element) {
        return offset;
    }

    ElementType type = element->type();

    // Handle text run - this is the leaf element that produces actual characters
    if (type == ElementType::Text) {
        int length = element->length();
        if (length > 0 && !activeTypes.isEmpty()) {
            // Create format range for this text with all active formatting
            QTextLayout::FormatRange range;
            range.start = offset;
            range.length = length;
            range.format = combineFormats(activeTypes, baseFont);
            result.append(range);
        }
        return offset + length;
    }

    // Handle inline container elements (Bold, Italic, etc.)
    // Check if this element is an inline container
    const auto* container = dynamic_cast<const KmlInlineContainer*>(element);
    if (container) {
        // Push this element's type onto the active stack
        activeTypes.append(type);

        // Process all children
        for (const auto& child : container->children()) {
            offset = processElement(child.get(), offset, activeTypes, baseFont, result);
        }

        // Pop this element's type
        activeTypes.removeLast();
    }

    return offset;
}

}  // namespace kalahari::editor
