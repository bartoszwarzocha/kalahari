/// @file kml_paragraph.h
/// @brief KML Paragraph element - block container for inline elements (OpenSpec #00042 Phase 1.6)
///
/// KmlParagraph is a block-level element that contains inline elements (text runs,
/// bold, italic, etc.). It represents a single paragraph in the document.
///
/// Key responsibilities:
/// - Container for inline elements (KmlElement children)
/// - Paragraph-level styling (styleId)
/// - Text extraction and serialization

#pragma once

#include <kalahari/editor/kml_element.h>
#include <kalahari/editor/kml_comment.h>
#include <vector>
#include <memory>
#include <QString>
#include <QList>

namespace kalahari::editor {

/// @brief A paragraph containing inline elements
///
/// KmlParagraph is the fundamental block-level element in KML documents.
/// It contains a sequence of inline elements (text runs, formatting elements)
/// and can have a paragraph style applied.
///
/// Unlike inline containers (KmlBold, etc.), KmlParagraph represents a block
/// that starts on a new line. In the document model, paragraphs are separated
/// by line breaks.
///
/// Example KML:
/// @code
/// <p>Simple paragraph with plain text</p>
/// <p style="heading1">Chapter heading</p>
/// <p>Text with <b>bold</b> and <i>italic</i> formatting</p>
/// @endcode
class KmlParagraph {
public:
    /// @brief Construct an empty paragraph
    KmlParagraph();

    /// @brief Construct a paragraph with initial text
    /// @param text Initial plain text content
    explicit KmlParagraph(const QString& text);

    /// @brief Construct a paragraph with text and style
    /// @param text Initial plain text content
    /// @param styleId Paragraph style ID
    KmlParagraph(const QString& text, const QString& styleId);

    /// @brief Destructor
    ~KmlParagraph();

    /// @brief Copy constructor
    KmlParagraph(const KmlParagraph& other);

    /// @brief Move constructor
    KmlParagraph(KmlParagraph&& other) noexcept;

    /// @brief Copy assignment
    KmlParagraph& operator=(const KmlParagraph& other);

    /// @brief Move assignment
    KmlParagraph& operator=(KmlParagraph&& other) noexcept;

    // =========================================================================
    // Element container methods
    // =========================================================================

    /// @brief Get the number of child elements
    /// @return Number of inline elements in this paragraph
    int elementCount() const;

    /// @brief Get a child element by index
    /// @param index The index (0-based)
    /// @return Pointer to the element, or nullptr if index out of range
    const KmlElement* elementAt(int index) const;

    /// @brief Get a mutable child element by index
    /// @param index The index (0-based)
    /// @return Pointer to the element, or nullptr if index out of range
    KmlElement* elementAt(int index);

    /// @brief Add an element to the end of the paragraph
    /// @param element The element to add (ownership transferred)
    void addElement(std::unique_ptr<KmlElement> element);

    /// @brief Insert an element at a specific index
    /// @param index The insertion position (0 = before first)
    /// @param element The element to insert (ownership transferred)
    void insertElement(int index, std::unique_ptr<KmlElement> element);

    /// @brief Remove an element by index
    /// @param index The index to remove
    /// @return The removed element, or nullptr if index out of range
    std::unique_ptr<KmlElement> removeElement(int index);

    /// @brief Remove all elements
    void clearElements();

    /// @brief Get direct access to elements (for iteration)
    /// @return Const reference to the element vector
    const std::vector<std::unique_ptr<KmlElement>>& elements() const;

    // =========================================================================
    // Content methods
    // =========================================================================

    /// @brief Get the plain text content (without markup)
    /// @return Concatenated plain text from all elements
    QString plainText() const;

    /// @brief Get the total character count
    /// @return Sum of character counts from all elements
    int length() const;

    /// @brief Check if the paragraph is empty (no content)
    /// @return true if no elements or all elements are empty
    bool isEmpty() const;

    /// @brief Get the total character count (alias for length())
    /// @return Sum of character counts from all elements
    int characterCount() const;

    // =========================================================================
    // Text manipulation methods (Phase 1.7)
    // =========================================================================

    /// @brief Insert plain text at a character offset
    /// @param offset The character position (0 = before first character)
    /// @param text The text to insert
    /// @return true if successful, false if offset out of range
    /// @note If offset is at an element boundary, text is inserted into the preceding element.
    ///       For empty paragraphs, a new KmlTextRun is created.
    bool insertText(int offset, const QString& text);

    /// @brief Delete text between two character offsets
    /// @param start The start character position (inclusive)
    /// @param end The end character position (exclusive)
    /// @return true if successful, false if range invalid
    /// @note Elements that become empty after deletion are removed.
    bool deleteText(int start, int end);

    /// @brief Split this paragraph at a character offset
    /// @param offset The character position to split at
    /// @return A new paragraph containing content from offset to end,
    ///         or nullptr if offset out of range (or at position 0)
    /// @note This paragraph is modified to contain only content before offset.
    ///       The new paragraph inherits the same style as this paragraph.
    std::unique_ptr<KmlParagraph> splitAt(int offset);

    /// @brief Merge another paragraph into this one
    /// @param other The paragraph to merge (its content is appended)
    /// @note After merging, all elements from 'other' are moved to this paragraph.
    ///       The 'other' paragraph will be empty after this operation.
    void mergeWith(KmlParagraph& other);

    // =========================================================================
    // Style methods
    // =========================================================================

    /// @brief Get the paragraph style ID
    /// @return The style ID (empty string for default style)
    const QString& styleId() const;

    /// @brief Set the paragraph style ID
    /// @param styleId The new style ID (empty for default)
    void setStyleId(const QString& styleId);

    /// @brief Check if this paragraph has a custom style
    /// @return true if styleId is not empty
    bool hasStyle() const;

    // =========================================================================
    // Comments (Phase 7.8)
    // =========================================================================

    /// @brief Get all comments attached to this paragraph
    /// @return List of comments
    const QList<KmlComment>& comments() const;

    /// @brief Get the number of comments
    /// @return Number of comments
    int commentCount() const;

    /// @brief Add a comment to this paragraph
    /// @param comment The comment to add
    void addComment(const KmlComment& comment);

    /// @brief Remove a comment by ID
    /// @param commentId The ID of the comment to remove
    /// @return true if comment was found and removed
    bool removeComment(const QString& commentId);

    /// @brief Find a comment by ID
    /// @param id The comment ID
    /// @return Pointer to comment, or nullptr if not found
    KmlComment* commentById(const QString& id);

    /// @brief Find a comment by ID (const version)
    /// @param id The comment ID
    /// @return Const pointer to comment, or nullptr if not found
    const KmlComment* commentById(const QString& id) const;

    /// @brief Check if paragraph has any comments
    /// @return true if there are comments
    bool hasComments() const;

    /// @brief Get comments overlapping a text range
    /// @param start Start position (inclusive)
    /// @param end End position (exclusive)
    /// @return List of comments overlapping the range
    QList<KmlComment> commentsInRange(int start, int end) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize this paragraph to KML format
    /// @return QString containing valid KML markup
    QString toKml() const;

    /// @brief Create a deep copy of this paragraph
    /// @return unique_ptr to a new paragraph with same content
    std::unique_ptr<KmlParagraph> clone() const;

private:
    std::vector<std::unique_ptr<KmlElement>> m_elements;  ///< Child elements
    QString m_styleId;  ///< Paragraph style ID (empty for default)
    QList<KmlComment> m_comments;  ///< Comments attached to this paragraph
};

}  // namespace kalahari::editor
