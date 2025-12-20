/// @file kml_text_run.h
/// @brief KML Text Run element - plain text with optional style (OpenSpec #00042 Phase 1.4)
///
/// KmlTextRun represents a contiguous run of text with a single style.
/// It is the leaf element in the KML tree - it contains only text, no children.

#pragma once

#include <kalahari/editor/kml_element.h>
#include <QString>

namespace kalahari::editor {

/// @brief A contiguous run of plain text with optional style
///
/// KmlTextRun is the most basic content element in KML. It represents
/// a sequence of characters that all share the same formatting.
///
/// Text runs are the leaf nodes of the KML tree - they contain
/// only text content, never nested elements.
///
/// Example KML:
/// @code
/// <t>Hello, world!</t>
/// <t style="emphasis">Important text</t>
/// @endcode
class KmlTextRun : public KmlElement {
public:
    /// @brief Construct an empty text run
    KmlTextRun();

    /// @brief Construct a text run with content
    /// @param text The text content
    explicit KmlTextRun(const QString& text);

    /// @brief Construct a text run with content and style
    /// @param text The text content
    /// @param styleId The character style ID (empty for default)
    KmlTextRun(const QString& text, const QString& styleId);

    /// @brief Destructor
    ~KmlTextRun() override = default;

    /// @brief Copy constructor
    KmlTextRun(const KmlTextRun& other);

    /// @brief Move constructor
    KmlTextRun(KmlTextRun&& other) noexcept;

    /// @brief Copy assignment
    KmlTextRun& operator=(const KmlTextRun& other);

    /// @brief Move assignment
    KmlTextRun& operator=(KmlTextRun&& other) noexcept;

    // =========================================================================
    // KmlElement interface implementation
    // =========================================================================

    /// @brief Get the element type
    /// @return ElementType::Text
    ElementType type() const override;

    /// @brief Serialize to KML format
    /// @return KML representation: <t>text</t> or <t style="id">text</t>
    QString toKml() const override;

    /// @brief Create a deep copy
    /// @return New KmlTextRun with same content
    std::unique_ptr<KmlElement> clone() const override;

    /// @brief Get plain text content
    /// @return The text content (same as text())
    QString plainText() const override;

    /// @brief Get character count
    /// @return Number of characters in text
    int length() const override;

    // =========================================================================
    // KmlTextRun-specific methods
    // =========================================================================

    /// @brief Get the text content
    /// @return The text content
    const QString& text() const;

    /// @brief Set the text content
    /// @param text The new text content
    void setText(const QString& text);

    /// @brief Get the character style ID
    /// @return The style ID (empty string for default style)
    const QString& styleId() const;

    /// @brief Set the character style ID
    /// @param styleId The new style ID (empty for default)
    void setStyleId(const QString& styleId);

    /// @brief Check if this text run has a custom style
    /// @return true if styleId is not empty
    bool hasStyle() const;

    // =========================================================================
    // Static factory methods
    // =========================================================================

    /// @brief Parse a text run from KML
    /// @param kml The KML string (e.g., "<t>Hello</t>")
    /// @return Parsed text run, or nullptr if parsing fails
    /// @note This is a simple parser for single <t> elements only
    static std::unique_ptr<KmlTextRun> fromKml(const QString& kml);

private:
    QString m_text;     ///< The text content
    QString m_styleId;  ///< Character style ID (empty for default)
};

}  // namespace kalahari::editor
