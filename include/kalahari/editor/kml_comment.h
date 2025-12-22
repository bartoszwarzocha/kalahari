/// @file kml_comment.h
/// @brief KML Comment element - annotation attached to text range (OpenSpec #00042 Phase 7.8)
///
/// KmlComment represents an annotation/comment attached to a specific text range
/// within a paragraph. Comments are rendered as markers in the margin and can be
/// expanded to show their content.
///
/// Key features:
/// - Attached to a text range (startPos, endPos)
/// - Contains text content and metadata (author, timestamp)
/// - Can be marked as resolved
/// - Serialized in KML format

#pragma once

#include <QString>
#include <QDateTime>

class QDomElement;

namespace kalahari::editor {

/// @brief A comment annotation attached to a text range
///
/// Comments are used to add notes and annotations to specific portions of text.
/// They are displayed as markers in the margin and the commented text range
/// can be highlighted with a subtle background color.
///
/// Example KML:
/// @code
/// <comment id="c-uuid" start="5" end="10" author="John" created="2025-01-15T10:30:00" resolved="false">
///   This is a comment about the word "hello"
/// </comment>
/// @endcode
class KmlComment {
public:
    /// @brief Construct an empty comment
    KmlComment();

    /// @brief Construct a comment with position and text
    /// @param startPos Start position in paragraph (inclusive)
    /// @param endPos End position in paragraph (exclusive)
    /// @param text Comment text content
    KmlComment(int startPos, int endPos, const QString& text);

    /// @brief Destructor
    ~KmlComment() = default;

    /// @brief Copy constructor
    KmlComment(const KmlComment& other) = default;

    /// @brief Move constructor
    KmlComment(KmlComment&& other) noexcept = default;

    /// @brief Copy assignment
    KmlComment& operator=(const KmlComment& other) = default;

    /// @brief Move assignment
    KmlComment& operator=(KmlComment&& other) noexcept = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /// @brief Get the unique comment ID
    /// @return The comment ID (UUID-based)
    const QString& id() const { return m_id; }

    /// @brief Set the comment ID
    /// @param id The new ID
    void setId(const QString& id) { m_id = id; }

    /// @brief Generate a new unique ID for this comment
    /// @note Uses QUuid::createUuid() internally
    void generateId();

    // =========================================================================
    // Position
    // =========================================================================

    /// @brief Get the start position in the paragraph
    /// @return Start character position (inclusive, 0-based)
    int startPos() const { return m_startPos; }

    /// @brief Set the start position
    /// @param pos New start position
    void setStartPos(int pos) { m_startPos = pos; }

    /// @brief Get the end position in the paragraph
    /// @return End character position (exclusive)
    int endPos() const { return m_endPos; }

    /// @brief Set the end position
    /// @param pos New end position
    void setEndPos(int pos) { m_endPos = pos; }

    /// @brief Get the length of the commented text range
    /// @return Number of characters in the range
    int length() const { return m_endPos - m_startPos; }

    /// @brief Check if the comment range is valid
    /// @return true if startPos < endPos
    bool isValidRange() const { return m_startPos >= 0 && m_startPos < m_endPos; }

    // =========================================================================
    // Content
    // =========================================================================

    /// @brief Get the comment text content
    /// @return The comment text
    const QString& text() const { return m_text; }

    /// @brief Set the comment text content
    /// @param text New comment text
    void setText(const QString& text) { m_text = text; }

    // =========================================================================
    // Metadata
    // =========================================================================

    /// @brief Get the author name
    /// @return Author name (empty if not set)
    const QString& author() const { return m_author; }

    /// @brief Set the author name
    /// @param author New author name
    void setAuthor(const QString& author) { m_author = author; }

    /// @brief Get the creation timestamp
    /// @return Creation date/time
    const QDateTime& createdAt() const { return m_createdAt; }

    /// @brief Set the creation timestamp
    /// @param dt New timestamp
    void setCreatedAt(const QDateTime& dt) { m_createdAt = dt; }

    /// @brief Check if the comment is resolved/closed
    /// @return true if resolved
    bool isResolved() const { return m_resolved; }

    /// @brief Set the resolved status
    /// @param resolved New resolved status
    void setResolved(bool resolved) { m_resolved = resolved; }

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize this comment to KML XML format
    /// @return QString containing valid KML markup
    QString toKml() const;

    /// @brief Parse a comment from KML XML element
    /// @param element The DOM element to parse
    /// @return Parsed KmlComment (may have default values on error)
    static KmlComment fromKml(const QDomElement& element);

private:
    QString m_id;             ///< Unique comment identifier
    int m_startPos{0};        ///< Start position in paragraph (inclusive)
    int m_endPos{0};          ///< End position in paragraph (exclusive)
    QString m_text;           ///< Comment text content
    QString m_author;         ///< Author name
    QDateTime m_createdAt;    ///< Creation timestamp
    bool m_resolved{false};   ///< Whether comment is resolved
};

}  // namespace kalahari::editor
