/// @file kml_element.cpp
/// @brief Implementation of KML element utilities (OpenSpec #00042)

#include <kalahari/editor/kml_element.h>

namespace kalahari::editor {

QString elementTypeToString(ElementType type) {
    switch (type) {
        case ElementType::Text:
            return QStringLiteral("Text");
        case ElementType::Bold:
            return QStringLiteral("Bold");
        case ElementType::Italic:
            return QStringLiteral("Italic");
        case ElementType::Underline:
            return QStringLiteral("Underline");
        case ElementType::Strikethrough:
            return QStringLiteral("Strikethrough");
        case ElementType::Subscript:
            return QStringLiteral("Subscript");
        case ElementType::Superscript:
            return QStringLiteral("Superscript");
        case ElementType::Link:
            return QStringLiteral("Link");
        case ElementType::CharacterStyle:
            return QStringLiteral("CharacterStyle");
    }
    return QStringLiteral("Unknown");
}

}  // namespace kalahari::editor
