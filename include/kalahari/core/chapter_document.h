/// @file chapter_document.h
/// @brief KChapter document format for rich chapter storage
///
/// ChapterDocument represents a single chapter stored in .kchapter format.
/// This JSON-based format stores HTML content, plaintext backup, statistics,
/// and extensible metadata.
///
/// Example .kchapter file:
/// @code{.json}
/// {
///   "kalahari": { "version": "1.0", "type": "chapter" },
///   "content": {
///     "html": "<p>Chapter content...</p>",
///     "plainText": "Chapter content..."
///   },
///   "statistics": {
///     "wordCount": 1523,
///     "characterCount": 8945,
///     "paragraphCount": 42,
///     "lastModified": "2025-01-15T10:30:00Z"
///   },
///   "metadata": {
///     "title": "Chapter 1",
///     "status": "draft",
///     "notes": ""
///   },
///   "annotations": {
///     "comments": [],
///     "highlights": []
///   }
/// }
/// @endcode
///
/// OpenSpec #00035: KChapter Document Format

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QColor>
#include <optional>

namespace kalahari {
namespace core {

/// @brief Chapter document with JSON+HTML storage
///
/// Provides rich metadata, statistics auto-calculation, and disaster
/// recovery via plaintext backup. Designed for extensibility with
/// future annotations and template support.
class ChapterDocument {
public:
    /// @brief Format version for compatibility checking
    static constexpr const char* FORMAT_VERSION = "1.0";

    /// @brief Document type identifier
    static constexpr const char* FORMAT_TYPE = "chapter";

    // =========================================================================
    // Construction
    // =========================================================================

    /// @brief Default constructor (empty document)
    ChapterDocument();

    /// @brief Construct with initial HTML content
    /// @param html HTML content string
    explicit ChapterDocument(const QString& html);

    // =========================================================================
    // Content Access
    // =========================================================================

    /// @brief Get HTML content
    /// @return HTML string
    QString html() const;

    /// @brief Get plain text content (disaster recovery backup)
    /// @return Plain text string
    QString plainText() const;

    /// @brief Set HTML content
    /// @param html HTML content string
    ///
    /// Automatically updates plainText and recalculates statistics.
    void setHtml(const QString& html);

    /// @brief Set plain text content directly
    /// @param text Plain text string
    ///
    /// Normally auto-generated from HTML, but can be set manually.
    void setPlainText(const QString& text);

    /// @brief Check if document has content
    /// @return true if HTML is not empty
    bool hasContent() const;

    // =========================================================================
    // Statistics (auto-calculated)
    // =========================================================================

    /// @brief Get word count
    /// @return Number of words in plain text
    int wordCount() const;

    /// @brief Get character count
    /// @return Number of characters (excluding whitespace)
    int characterCount() const;

    /// @brief Get paragraph count
    /// @return Number of paragraphs
    int paragraphCount() const;

    /// @brief Get last modification timestamp
    /// @return QDateTime of last modification
    QDateTime lastModified() const;

    /// @brief Update last modified timestamp to now
    void touch();

    // =========================================================================
    // Metadata
    // =========================================================================

    /// @brief Get chapter title
    /// @return Title string
    QString title() const;

    /// @brief Set chapter title
    /// @param title Title string
    void setTitle(const QString& title);

    /// @brief Get chapter status
    /// @return Status string (e.g., "draft", "revision", "final")
    QString status() const;

    /// @brief Set chapter status
    /// @param status Status string
    void setStatus(const QString& status);

    /// @brief Get chapter notes
    /// @return Notes string
    QString notes() const;

    /// @brief Set chapter notes
    /// @param notes Notes string
    void setNotes(const QString& notes);

    /// @brief Get chapter color (for visual organization)
    /// @return Optional color
    std::optional<QColor> color() const;

    /// @brief Set chapter color
    /// @param color Color value
    void setColor(const QColor& color);

    /// @brief Clear chapter color
    void clearColor();

    // =========================================================================
    // Annotations (Phase 3 - extensibility point)
    // =========================================================================

    /// @brief Get comments array
    /// @return JSON array of comments
    QJsonArray comments() const;

    /// @brief Set comments array
    /// @param comments JSON array of comments
    void setComments(const QJsonArray& comments);

    /// @brief Get highlights array
    /// @return JSON array of highlights
    QJsonArray highlights() const;

    /// @brief Set highlights array
    /// @param highlights JSON array of highlights
    void setHighlights(const QJsonArray& highlights);

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize to JSON object
    /// @return Complete JSON representation
    QJsonObject toJson() const;

    /// @brief Deserialize from JSON object
    /// @param json JSON object
    /// @return ChapterDocument with populated fields
    static ChapterDocument fromJson(const QJsonObject& json);

    // =========================================================================
    // File I/O
    // =========================================================================

    /// @brief Load document from .kchapter file
    /// @param path File path
    /// @return ChapterDocument if successful, nullopt on error
    static std::optional<ChapterDocument> load(const QString& path);

    /// @brief Save document to .kchapter file
    /// @param path File path
    /// @return true if saved successfully
    bool save(const QString& path) const;

    // =========================================================================
    // Migration Helpers
    // =========================================================================

    /// @brief Create ChapterDocument from RTF/HTML content
    /// @param content HTML or RTF content string
    /// @param title Optional title
    /// @return New ChapterDocument
    ///
    /// Used for migrating existing RTF chapters to .kchapter format.
    static ChapterDocument fromHtmlContent(const QString& content,
                                            const QString& title = QString());

    /// @brief Extract plain text from HTML
    /// @param html HTML string
    /// @return Plain text with tags stripped
    static QString htmlToPlainText(const QString& html);

private:
    /// @brief Recalculate statistics from current content
    void recalculateStatistics();

    /// @brief Calculate word count from plain text
    /// @param text Plain text string
    /// @return Word count
    static int calculateWordCount(const QString& text);

    /// @brief Calculate character count from plain text
    /// @param text Plain text string
    /// @return Character count (excluding whitespace)
    static int calculateCharacterCount(const QString& text);

    /// @brief Calculate paragraph count from plain text
    /// @param text Plain text string
    /// @return Paragraph count
    static int calculateParagraphCount(const QString& text);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Content
    QString m_html;
    QString m_plainText;

    // Statistics
    int m_wordCount = 0;
    int m_characterCount = 0;
    int m_paragraphCount = 0;
    QDateTime m_lastModified;

    // Metadata
    QString m_title;
    QString m_status = "draft";
    QString m_notes;
    std::optional<QColor> m_color;

    // Annotations (Phase 3)
    QJsonArray m_comments;
    QJsonArray m_highlights;
};

} // namespace core
} // namespace kalahari
