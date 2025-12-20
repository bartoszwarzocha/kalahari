/// @file kml_inline_elements.h
/// @brief KML Inline formatting elements (OpenSpec #00042 Phase 1.5)
///
/// Inline elements are container elements that wrap text runs or other inline
/// elements to apply formatting. They support nesting (e.g., bold inside italic).
///
/// Supported elements:
/// - KmlBold (<b>) - Bold text
/// - KmlItalic (<i>) - Italic text
/// - KmlUnderline (<u>) - Underlined text
/// - KmlStrikethrough (<s>) - Strikethrough text
/// - KmlSubscript (<sub>) - Subscript text
/// - KmlSuperscript (<sup>) - Superscript text

#pragma once

#include <kalahari/editor/kml_element.h>
#include <vector>
#include <memory>

namespace kalahari::editor {

/// @brief Base class for inline container elements
///
/// KmlInlineContainer is the base class for all inline formatting elements
/// (bold, italic, underline, etc.). It manages a list of child elements
/// and provides common functionality for serialization and content access.
///
/// Inline containers can hold:
/// - KmlTextRun (plain text)
/// - Other inline containers (for nested formatting)
///
/// Example KML:
/// @code
/// <b>Bold text</b>
/// <b><i>Bold and italic</i></b>
/// <b>Normal <i>italic</i> bold</b>
/// @endcode
class KmlInlineContainer : public KmlElement {
public:
    /// @brief Virtual destructor
    ~KmlInlineContainer() override = default;

    // =========================================================================
    // KmlElement interface implementation
    // =========================================================================

    /// @brief Get plain text content from all children
    /// @return Concatenated plain text from all child elements
    QString plainText() const override;

    /// @brief Get the total character count
    /// @return Sum of character counts from all children
    int length() const override;

    // =========================================================================
    // Child element management
    // =========================================================================

    /// @brief Get the number of child elements
    /// @return Number of children in this container
    int childCount() const;

    /// @brief Get a child element by index
    /// @param index The index (0-based)
    /// @return Pointer to the child element, or nullptr if index out of range
    const KmlElement* childAt(int index) const;

    /// @brief Get a mutable child element by index
    /// @param index The index (0-based)
    /// @return Pointer to the child element, or nullptr if index out of range
    KmlElement* childAt(int index);

    /// @brief Add a child element to the end
    /// @param child The element to add (ownership transferred)
    void appendChild(std::unique_ptr<KmlElement> child);

    /// @brief Insert a child element at a specific index
    /// @param index The insertion position (0 = before first)
    /// @param child The element to insert (ownership transferred)
    void insertChild(int index, std::unique_ptr<KmlElement> child);

    /// @brief Remove a child element by index
    /// @param index The index to remove
    /// @return The removed element, or nullptr if index out of range
    std::unique_ptr<KmlElement> removeChild(int index);

    /// @brief Remove all child elements
    void clearChildren();

    /// @brief Get direct access to children (for iteration)
    /// @return Const reference to the child vector
    const std::vector<std::unique_ptr<KmlElement>>& children() const;

protected:
    /// @brief Protected constructor - only derived classes can instantiate
    KmlInlineContainer();

    /// @brief Protected copy constructor for clone() support
    KmlInlineContainer(const KmlInlineContainer& other);

    /// @brief Protected move constructor
    KmlInlineContainer(KmlInlineContainer&& other) noexcept;

    /// @brief Protected copy assignment
    KmlInlineContainer& operator=(const KmlInlineContainer& other);

    /// @brief Protected move assignment
    KmlInlineContainer& operator=(KmlInlineContainer&& other) noexcept;

    /// @brief Helper to serialize children to KML
    /// @return KML string of all children concatenated
    QString childrenToKml() const;

    /// @brief Helper to clone all children
    /// @param target The container to clone children into
    void cloneChildrenTo(KmlInlineContainer& target) const;

private:
    std::vector<std::unique_ptr<KmlElement>> m_children;  ///< Child elements
};

// =============================================================================
// Concrete Inline Element Classes
// =============================================================================

/// @brief Bold text element (<b>)
///
/// Wraps content in bold formatting.
///
/// Example KML:
/// @code
/// <b>Bold text</b>
/// <b>Multiple <i>nested</i> elements</b>
/// @endcode
class KmlBold : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlBold();

    /// @brief Destructor
    ~KmlBold() override = default;

    /// @brief Copy constructor
    KmlBold(const KmlBold& other);

    /// @brief Move constructor
    KmlBold(KmlBold&& other) noexcept;

    /// @brief Copy assignment
    KmlBold& operator=(const KmlBold& other);

    /// @brief Move assignment
    KmlBold& operator=(KmlBold&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

/// @brief Italic text element (<i>)
///
/// Wraps content in italic formatting.
class KmlItalic : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlItalic();

    /// @brief Destructor
    ~KmlItalic() override = default;

    /// @brief Copy constructor
    KmlItalic(const KmlItalic& other);

    /// @brief Move constructor
    KmlItalic(KmlItalic&& other) noexcept;

    /// @brief Copy assignment
    KmlItalic& operator=(const KmlItalic& other);

    /// @brief Move assignment
    KmlItalic& operator=(KmlItalic&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

/// @brief Underline text element (<u>)
///
/// Wraps content in underline formatting.
class KmlUnderline : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlUnderline();

    /// @brief Destructor
    ~KmlUnderline() override = default;

    /// @brief Copy constructor
    KmlUnderline(const KmlUnderline& other);

    /// @brief Move constructor
    KmlUnderline(KmlUnderline&& other) noexcept;

    /// @brief Copy assignment
    KmlUnderline& operator=(const KmlUnderline& other);

    /// @brief Move assignment
    KmlUnderline& operator=(KmlUnderline&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

/// @brief Strikethrough text element (<s>)
///
/// Wraps content in strikethrough formatting.
class KmlStrikethrough : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlStrikethrough();

    /// @brief Destructor
    ~KmlStrikethrough() override = default;

    /// @brief Copy constructor
    KmlStrikethrough(const KmlStrikethrough& other);

    /// @brief Move constructor
    KmlStrikethrough(KmlStrikethrough&& other) noexcept;

    /// @brief Copy assignment
    KmlStrikethrough& operator=(const KmlStrikethrough& other);

    /// @brief Move assignment
    KmlStrikethrough& operator=(KmlStrikethrough&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

/// @brief Subscript text element (<sub>)
///
/// Wraps content in subscript formatting (below baseline).
class KmlSubscript : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlSubscript();

    /// @brief Destructor
    ~KmlSubscript() override = default;

    /// @brief Copy constructor
    KmlSubscript(const KmlSubscript& other);

    /// @brief Move constructor
    KmlSubscript(KmlSubscript&& other) noexcept;

    /// @brief Copy assignment
    KmlSubscript& operator=(const KmlSubscript& other);

    /// @brief Move assignment
    KmlSubscript& operator=(KmlSubscript&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

/// @brief Superscript text element (<sup>)
///
/// Wraps content in superscript formatting (above baseline).
class KmlSuperscript : public KmlInlineContainer {
public:
    /// @brief Default constructor
    KmlSuperscript();

    /// @brief Destructor
    ~KmlSuperscript() override = default;

    /// @brief Copy constructor
    KmlSuperscript(const KmlSuperscript& other);

    /// @brief Move constructor
    KmlSuperscript(KmlSuperscript&& other) noexcept;

    /// @brief Copy assignment
    KmlSuperscript& operator=(const KmlSuperscript& other);

    /// @brief Move assignment
    KmlSuperscript& operator=(KmlSuperscript&& other) noexcept;

    // KmlElement interface
    ElementType type() const override;
    QString toKml() const override;
    std::unique_ptr<KmlElement> clone() const override;
};

}  // namespace kalahari::editor
