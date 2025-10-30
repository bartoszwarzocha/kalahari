/// @file document.h
/// @brief Document class - top-level wrapper for book project with metadata
///
/// Document represents a complete book project, wrapping the Book structure
/// with additional metadata (author, language, timestamps, etc.).
///
/// In Phase 0 MVP: Document = Project (1:1 mapping)
/// In Phase 2+: Project can contain multiple Documents
///
/// Example JSON representation (manifest.json inside .klh):
/// @code{.json}
/// {
///   "version": "1.0.0",
///   "document": {
///     "id": "550e8400-e29b-41d4-a716-446655440000",
///     "title": "The Great Adventure",
///     "author": "Jane Doe",
///     "language": "en",
///     "genre": "fiction",
///     "created": "2025-10-30T10:00:00Z",
///     "modified": "2025-10-30T15:30:00Z"
///   },
///   "book": {
///     "frontMatter": [...],
///     "body": [...],
///     "backMatter": [...]
///   }
/// }
/// @endcode

#pragma once

#include <kalahari/core/book.h>
#include <string>
#include <chrono>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Document - top-level wrapper with project metadata
///
/// Document represents a complete book project with:
/// - Project metadata (title, author, language, genre)
/// - The book structure (front/body/back matter)
/// - Timestamps (created, modified)
/// - Save/load operations (.klh files)
///
/// Example usage:
/// @code{.cpp}
/// // Create new document
/// Document doc("My Novel", "John Doe", "en");
///
/// // Add content to book
/// Book& book = doc.getBook();
/// // ... populate book structure
///
/// // Save to .klh file
/// doc.save("my_novel.klh");
///
/// // Load from .klh file
/// auto loaded = Document::load("my_novel.klh");
/// if (loaded) {
///     Document& doc = *loaded;
///     // ... work with loaded document
/// }
/// @endcode
class Document {
public:
    /// @brief Construct a new document
    /// @param title Project title
    /// @param author Author name
    /// @param language ISO 639-1 language code (e.g., "en", "pl", "de")
    Document(std::string title, std::string author, std::string language = "en");

    /// @brief Default constructor (for deserialization)
    Document() = default;

    // Getters
    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::string& getAuthor() const { return m_author; }
    const std::string& getLanguage() const { return m_language; }
    const std::string& getGenre() const { return m_genre; }
    const std::chrono::system_clock::time_point& getCreated() const { return m_created; }
    const std::chrono::system_clock::time_point& getModified() const { return m_modified; }

    // Setters
    void setTitle(const std::string& title);
    void setAuthor(const std::string& author);
    void setLanguage(const std::string& language);
    void setGenre(const std::string& genre);

    /// @brief Get book structure (mutable)
    /// @return Reference to book
    Book& getBook() { return m_book; }

    /// @brief Get book structure (const)
    /// @return Const reference to book
    const Book& getBook() const { return m_book; }

    /// @brief Update modified timestamp to now
    void touch();

    /// @brief Save document to .klh file
    /// @param path Path to .klh file (will be created/overwritten)
    /// @return true if save succeeded, false otherwise
    ///
    /// Phase 0 MVP: Basic implementation (stub)
    /// Phase 2: Full DocumentArchive implementation with ZIP
    bool save(const std::filesystem::path& path);

    /// @brief Load document from .klh file
    /// @param path Path to .klh file
    /// @return Optional Document if load succeeded, std::nullopt otherwise
    ///
    /// Phase 0 MVP: Basic implementation (stub)
    /// Phase 2: Full DocumentArchive implementation with ZIP
    static std::optional<Document> load(const std::filesystem::path& path);

    /// @brief Serialize to JSON
    /// @return JSON object with document metadata and book structure
    ///
    /// Returns the complete manifest.json structure that goes inside .klh ZIP.
    json toJson() const;

    /// @brief Deserialize from JSON
    /// @param j JSON object (manifest.json structure)
    /// @return Document with populated fields
    /// @throws json::exception if required fields are missing
    static Document fromJson(const json& j);

    /// @brief Generate a new UUID v4 identifier
    /// @return UUID string (format: "timestamp-random")
    ///
    /// Simple UUID generation for Phase 0:
    /// - Based on timestamp (milliseconds) + random 4-digit hex
    /// - Collision-resistant for single-user application
    /// - Example: "1730281234567-a3f2"
    static std::string generateId();

private:
    std::string m_id;        ///< Unique identifier (UUID)
    std::string m_title;     ///< Project title
    std::string m_author;    ///< Author name
    std::string m_language;  ///< ISO 639-1 language code (en, pl, de, etc.)
    std::string m_genre;     ///< Genre (fiction, non-fiction, etc.)
    Book m_book;             ///< The book structure
    std::chrono::system_clock::time_point m_created;   ///< Creation timestamp
    std::chrono::system_clock::time_point m_modified;  ///< Last modification timestamp

    /// @brief Helper: Convert time_point to ISO 8601 string
    static std::string timeToString(const std::chrono::system_clock::time_point& time);

    /// @brief Helper: Convert ISO 8601 string to time_point
    static std::chrono::system_clock::time_point stringToTime(const std::string& str);
};

} // namespace core
} // namespace kalahari
