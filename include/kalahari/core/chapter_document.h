// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file chapter_document.h
/// @brief KChapter document format for rich chapter storage
///
/// ChapterDocument represents a single chapter stored in .kchapter format.
/// This JSON-based format stores KML content, plaintext backup, statistics,
/// and extensible metadata.
///
/// Example .kchapter file:
/// @code{.json}
/// {
///   "kalahari": { "version": "1.0", "type": "chapter" },
///   "content": {
///     "kml": "<kml><p>Chapter content...</p></kml>",
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

#ifndef KALAHARI_CORE_CHAPTER_DOCUMENT_H
#define KALAHARI_CORE_CHAPTER_DOCUMENT_H

#include <kalahari/core/book_project.h>
#include <kalahari/core/text_statistics.h>

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QColor>
#include <QList>
#include <optional>

namespace kalahari::core {

/// @brief A scene of a text: its text from its start or from a scene break (<scene-break/>,
///        a paragraph of the text) to the next break
struct KmlScene {
    ScenePlot plot;   ///< What its break says of it (ScenePlot::toAttributes()); the first
                      ///< scene has nothing here (ElementPlot::opening)
    WordTally words;  ///< The words of its text, with the texts of its notes

    bool operator==(const KmlScene&) const = default;
};

/// @brief A text and what its first scene says: what changes when the plot board moves, adds
///        or deletes a scene of the text
struct SceneText {
    QString kml;        ///< The text (KML)
    ScenePlot opening;  ///< Its first scene (ElementPlot::opening, in the .klh file)

    bool operator==(const SceneText&) const = default;
};

/// @brief Two texts after a scene of one of them moved to the other
struct SceneMove {
    SceneText from;  ///< The text the scene left
    SceneText to;    ///< The text it went to

    bool operator==(const SceneMove&) const = default;
};

/// @brief Chapter document with JSON+KML storage
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

    /// @brief Construct with initial KML content
    /// @param kml KML content string
    explicit ChapterDocument(const QString& kml);

    // =========================================================================
    // Content Access
    // =========================================================================

    /// @brief Get KML content
    /// @return KML string
    [[nodiscard]] QString kml() const;

    /// @brief Get plain text content (disaster recovery backup)
    /// @return Plain text string
    [[nodiscard]] QString plainText() const;

    /// @brief Set KML content
    /// @param kml KML content string
    ///
    /// Automatically updates plainText and recalculates statistics.
    void setKml(const QString& kml);

    /// @brief Set plain text content directly
    /// @param text Plain text string
    ///
    /// Normally auto-generated from KML, but can be set manually.
    void setPlainText(const QString& text);

    /// @brief Check if document has content
    /// @return true if KML is not empty
    [[nodiscard]] bool hasContent() const;

    // =========================================================================
    // Statistics (auto-calculated)
    // =========================================================================

    /// @brief Get word count
    /// @return Number of words in plain text, as counted by core::countText() under the rules
    ///         of the settings when the text was set, with the texts of the notes of the KML
    ///         when the rules count them: a file read keeps the number it was saved with,
    ///         whatever the rules are now (wordTally() follows them)
    [[nodiscard]] int wordCount() const;

    /// @brief The words of the text under any rules of counting (core::tallyWords()),
    ///        counted now, as the editor counts the text of the KML: with the texts of its
    ///        footnotes and endnotes (kmlWordTally())
    [[nodiscard]] WordTally wordTally() const;

    /// @brief Get character count
    /// @return Number of characters (excluding whitespace)
    [[nodiscard]] int characterCount() const;

    /// @brief Get paragraph count
    /// @return Number of paragraphs containing text, as counted by core::countText()
    [[nodiscard]] int paragraphCount() const;

    /// @brief Get last modification timestamp
    /// @return QDateTime of last modification
    [[nodiscard]] QDateTime lastModified() const;

    /// @brief Update last modified timestamp to now
    void touch();

    // =========================================================================
    // Metadata
    // =========================================================================

    /// @brief Get chapter title
    /// @return Title string
    [[nodiscard]] QString title() const;

    /// @brief Set chapter title
    /// @param title Title string
    void setTitle(const QString& title);

    /// @brief Get chapter status
    /// @return Status string (e.g., "draft", "revision", "final")
    [[nodiscard]] QString status() const;

    /// @brief Set chapter status
    /// @param status Status string
    void setStatus(const QString& status);

    /// @brief Get chapter notes
    /// @return Notes string
    [[nodiscard]] QString notes() const;

    /// @brief Set chapter notes
    /// @param notes Notes string
    void setNotes(const QString& notes);

    /// @brief Get chapter color (for visual organization)
    /// @return Optional color
    [[nodiscard]] std::optional<QColor> color() const;

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
    [[nodiscard]] QJsonArray comments() const;

    /// @brief Set comments array
    /// @param comments JSON array of comments
    void setComments(const QJsonArray& comments);

    /// @brief Get highlights array
    /// @return JSON array of highlights
    [[nodiscard]] QJsonArray highlights() const;

    /// @brief Set highlights array
    /// @param highlights JSON array of highlights
    void setHighlights(const QJsonArray& highlights);

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize to JSON object
    /// @return Complete JSON representation
    [[nodiscard]] QJsonObject toJson() const;

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
    ///
    /// The file is replaced only when the whole document is written; a failed save leaves
    /// the previous file as it was.
    /// @param path File path
    /// @return true if saved successfully
    [[nodiscard]] bool save(const QString& path) const;

    // =========================================================================
    // Migration Helpers
    // =========================================================================

    /// @brief Create ChapterDocument from KML content
    /// @param content KML content string
    /// @param title Optional title
    /// @return New ChapterDocument
    static ChapterDocument fromKmlContent(const QString& content,
                                          const QString& title = QString());

    /// @brief Extract plain text from KML
    /// @param kml KML string
    /// @return Text of the paragraphs, one per line, with entities decoded and the line
    ///         breaks in them as QChar::LineSeparator - the same text the editor shows
    static QString kmlToPlainText(const QString& kml);

    /// @brief The words of a text's KML under any rules of counting, as the editor counts
    ///        them: of its paragraphs (kmlToPlainText()) and of the texts of its footnotes and
    ///        endnotes (WordTally::noteWords)
    static WordTally kmlWordTally(const QString& kml);

    /// @brief The scenes of a text's KML in their order, with their words: the scenes of a
    ///        text add up to its words (kmlWordTally()); a text without scene breaks is one
    ///        scene
    static QList<KmlScene> kmlScenes(const QString& kml);

    /// @brief The KML with the break of scene @p scene (from 1: the first break) saying
    ///        @p plot of it; the rest of the text as it was
    /// @return Empty when the text has no such break
    static QString withScenePlot(const QString& kml, int scene, const ScenePlot& plot);

    /// @name The scenes of a text moved, added and deleted
    /// A scene goes with its text and what it says: the first scene of the text has no break,
    /// and what it says is SceneText::opening, so what comes first takes its break off and
    /// gives what it says to opening, and what was first gets a break. The annotations of the
    /// text and the headings at its start stay at its start; a scene moved to another text
    /// takes the annotations anchored in it along.
    /// @return nullopt for a KML that cannot be read, and for a scene the text does not have
    /// @{

    /// @brief @p text with scene @p from at place @p to of its scenes without it (from 0)
    static std::optional<SceneText> withSceneMoved(const SceneText& text, int from, int to);

    /// @brief Scene @p scene of @p from moved to place @p at of the scenes of @p to (0: the
    ///        first, the number of its scenes: the last), with its text, what it says and the
    ///        annotations anchored in it; an annotation anchored also in what stays of @p from
    ///        goes as a copy with a new id
    /// @return nullopt also for @p from of one scene
    static std::optional<SceneMove> withSceneMovedTo(const SceneText& from, int scene,
                                                     const SceneText& to, int at);

    /// @brief @p text with a new scene with an empty paragraph at place @p at of its scenes
    ///        (0: the first, the number of its scenes: the last), saying @p plot
    static std::optional<SceneText> withSceneAdded(const SceneText& text, int at,
                                                   const ScenePlot& plot = {});

    /// @brief @p text without scene @p scene: with its text, or (@p keepText) without its
    ///        break, its text joined to the scene before it (the first scene: the next one
    ///        joined to it), what the break said lost
    /// @return nullopt also for a text of one scene
    static std::optional<SceneText> withoutScene(const SceneText& text, int scene,
                                                 bool keepText);
    /// @}

private:
    /// @brief Recalculate statistics from current content
    void recalculateStatistics();

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Content
    QString m_kml;
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

} // namespace kalahari::core

#endif  // KALAHARI_CORE_CHAPTER_DOCUMENT_H
