/// @file kml_element.h
/// @brief Base class for KML (Kalahari Markup Language) elements (OpenSpec #00042)
///
/// KmlElement provides the abstract base for all inline elements in the editor:
/// text runs, bold, italic, underline, strikethrough, and nested containers.

#pragma once

#include <QString>
#include <memory>

namespace kalahari::editor {

/// @brief Type of KML element
enum class ElementType {
    Text,           ///< Plain text run (KmlTextRun)
    Bold,           ///< Bold formatting (<b>)
    Italic,         ///< Italic formatting (<i>)
    Underline,      ///< Underline formatting (<u>)
    Strikethrough,  ///< Strikethrough formatting (<s>)
    Subscript,      ///< Subscript formatting (<sub>)
    Superscript,    ///< Superscript formatting (<sup>)
    Link,           ///< Hyperlink (<a>)
    CharacterStyle  ///< Custom character style (<span>)
};

/// @brief Convert ElementType to string for debugging/logging
QString elementTypeToString(ElementType type);

/// @brief Base class for all KML inline elements
///
/// KmlElement is the abstract base class for all inline content in KML documents.
/// Inline elements include text runs, formatting elements (bold, italic, etc.),
/// and containers that can hold nested elements.
///
/// Each element knows its type and can serialize itself to KML format.
/// Elements support cloning for copy operations and undo/redo.
class KmlElement {
public:
    /// @brief Virtual destructor for proper inheritance cleanup
    virtual ~KmlElement() = default;

    /// @brief Get the type of this element
    /// @return ElementType identifying the concrete element class
    virtual ElementType type() const = 0;

    /// @brief Serialize this element to KML format
    /// @return QString containing valid KML markup
    virtual QString toKml() const = 0;

    /// @brief Create a deep copy of this element
    /// @return unique_ptr to a new element with same content
    virtual std::unique_ptr<KmlElement> clone() const = 0;

    /// @brief Get plain text content (without markup)
    /// @return QString containing only text content
    virtual QString plainText() const = 0;

    /// @brief Get the character count of this element
    /// @return Number of characters in plain text
    virtual int length() const = 0;

    /// @brief Check if this element is empty (no content)
    /// @return true if the element contains no text
    bool isEmpty() const { return length() == 0; }

protected:
    /// @brief Protected constructor - only derived classes can instantiate
    KmlElement() = default;

    /// @brief Copy constructor - protected for clone() support
    KmlElement(const KmlElement&) = default;

    /// @brief Move constructor
    KmlElement(KmlElement&&) = default;

    /// @brief Copy assignment - protected for derived classes
    KmlElement& operator=(const KmlElement&) = default;

    /// @brief Move assignment
    KmlElement& operator=(KmlElement&&) = default;
};

}  // namespace kalahari::editor
