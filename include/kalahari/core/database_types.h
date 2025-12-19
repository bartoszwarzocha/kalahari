/// @file database_types.h
/// @brief Data Transfer Objects (DTOs) for ProjectDatabase
///
/// Defines structures used to transfer data between ProjectDatabase and
/// the rest of the application. These are simple value types without
/// business logic - they map directly to database tables.
///
/// OpenSpec #00041: SQLite Project Database

#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>

namespace kalahari {
namespace core {

// =============================================================================
// Project Format Detection
// =============================================================================

/// @brief Project storage format
///
/// Used by ProjectFormatDetector to determine how to open a project.
enum class ProjectFormat {
    Unknown,   ///< Not recognized as a valid project
    Legacy,    ///< Old format (book.json, styles.json, etc.)
    SQLite     ///< New format (project.db)
};

// =============================================================================
// Chapter Data
// =============================================================================

/// @brief Chapter metadata (maps to 'chapters' table)
///
/// Contains metadata about a chapter. The actual content is stored
/// in .kchapter files, not in the database.
struct ChapterInfo {
    QString id;              ///< Unique chapter ID (UUID)
    QString path;            ///< Relative path to .kchapter file
    QString title;           ///< Chapter title
    QString status;          ///< Status: "draft", "revision", "final"
    int wordCount = 0;       ///< Word count (cached)
    int characterCount = 0;  ///< Character count (cached)
    int orderIndex = 0;      ///< Display order in navigator
    QDateTime createdAt;     ///< Creation timestamp
    QDateTime modifiedAt;    ///< Last modification timestamp

    /// @brief Check if chapter info is valid
    bool isValid() const { return !id.isEmpty() && !path.isEmpty(); }
};

/// @brief Chapter history entry (maps to 'chapter_history' table)
struct ChapterHistoryEntry {
    int id = 0;              ///< Auto-increment ID
    QString chapterId;       ///< Reference to chapter
    QString action;          ///< Action: "created", "edited", "reviewed"
    QString author;          ///< Who performed the action
    QDateTime timestamp;     ///< When action occurred
};

// =============================================================================
// Library Items (Characters, Locations, Items)
// =============================================================================

/// @brief Character from library (maps to 'characters' table)
struct CharacterInfo {
    QString id;              ///< Unique character ID (UUID)
    QString name;            ///< Character name
    QString description;     ///< Character description
    QString color;           ///< Hex color for UI (e.g., "#FF5733")
    QString notes;           ///< Additional notes
    QDateTime createdAt;     ///< Creation timestamp
    QDateTime modifiedAt;    ///< Last modification timestamp

    /// @brief Check if character info is valid
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

/// @brief Location from library (maps to 'locations' table)
struct LocationInfo {
    QString id;              ///< Unique location ID (UUID)
    QString name;            ///< Location name
    QString description;     ///< Location description
    QString notes;           ///< Additional notes
    QDateTime createdAt;     ///< Creation timestamp
    QDateTime modifiedAt;    ///< Last modification timestamp

    /// @brief Check if location info is valid
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

/// @brief Item from library (maps to 'items' table)
struct ItemInfo {
    QString id;              ///< Unique item ID (UUID)
    QString name;            ///< Item name
    QString description;     ///< Item description
    QString notes;           ///< Additional notes
    QDateTime createdAt;     ///< Creation timestamp
    QDateTime modifiedAt;    ///< Last modification timestamp

    /// @brief Check if item info is valid
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

// =============================================================================
// Statistics
// =============================================================================

/// @brief Writing session statistics (maps to 'session_stats' table)
///
/// Recorded during writing sessions for productivity analysis.
struct SessionStats {
    int id = 0;              ///< Auto-increment ID
    QDateTime timestamp;     ///< When stats were recorded
    QString documentId;      ///< Which document was edited
    int wordsWritten = 0;    ///< Words added in session
    int wordsDeleted = 0;    ///< Words removed in session
    int activeMinutes = 0;   ///< Minutes of active editing
    int hour = 0;            ///< Hour of day (0-23) for time analysis
};

/// @brief Aggregated statistics for dashboard/reports
struct AggregatedStats {
    int totalWords = 0;           ///< Total word count across all chapters
    int totalCharacters = 0;      ///< Total character count
    int totalChapters = 0;        ///< Number of chapters
    int totalSessions = 0;        ///< Number of writing sessions
    int totalActiveMinutes = 0;   ///< Total active writing time
    int averageWordsPerSession = 0;  ///< Average words per session
    QDateTime firstSession;       ///< First recorded session
    QDateTime lastSession;        ///< Most recent session
};

// =============================================================================
// Styles
// =============================================================================

/// @brief Paragraph style definition (maps to 'paragraph_styles' table)
struct ParagraphStyle {
    QString id;              ///< Unique style ID
    QString name;            ///< Display name
    QString baseStyle;       ///< Parent style ID for inheritance
    QVariantMap properties;  ///< Style properties (font, size, margins, etc.)

    /// @brief Check if style is valid
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

/// @brief Character (inline) style definition (maps to 'character_styles' table)
struct CharacterStyle {
    QString id;              ///< Unique style ID
    QString name;            ///< Display name
    QVariantMap properties;  ///< Style properties (bold, italic, color, etc.)

    /// @brief Check if style is valid
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

} // namespace core
} // namespace kalahari
