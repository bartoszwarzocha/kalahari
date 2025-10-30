/// @file book_element.h
/// @brief Universal book element with flexible type system
///
/// BookElement represents any section in a book structure (chapters, front matter,
/// back matter) using a flexible string-based type system. This design allows
/// plugins and users to define custom section types without modifying core code.
///
/// Example JSON representation:
/// @code{.json}
/// {
///   "type": "chapter",
///   "id": "ch-001",
///   "title": "Chapter 1: The Adventure Begins",
///   "file": "content/body/part-001/chapter-001.rtf",
///   "wordCount": 2500,
///   "created": "2025-10-30T10:00:00Z",
///   "modified": "2025-10-30T14:30:00Z",
///   "metadata": {
///     "pov": "First Person",
///     "location": "London"
///   }
/// }
/// @endcode

#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>
#include <kalahari/core/book_constants.h>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Universal book element (chapter, front matter, back matter)
///
/// BookElement uses a flexible type system where the type is a string,
/// allowing unlimited extensibility. Known types are defined in BookElementTypes
/// namespace, but any string is valid.
///
/// The element stores a path to an RTF file containing the actual content.
/// This enables lazy loading (Phase 1) - metadata is loaded eagerly, content
/// is loaded on-demand.
///
/// Example usage:
/// @code{.cpp}
/// // Create a chapter
/// BookElement chapter(BookElementTypes::CHAPTER, generateId(), "Chapter 1",
///                     "content/body/part-001/chapter-001.rtf");
/// chapter.setWordCount(2500);
/// chapter.setMetadata("pov", "First Person");
/// chapter.setMetadata("location", "London");
///
/// // Serialize to JSON
/// json j = chapter.toJson();
///
/// // Deserialize from JSON
/// BookElement loaded = BookElement::fromJson(j);
/// @endcode
class BookElement {
public:
    /// @brief Construct a BookElement
    /// @param type Element type (e.g., "chapter", "title_page", custom type)
    /// @param id Unique identifier (UUID recommended)
    /// @param title Display title
    /// @param file Relative path to RTF content file
    BookElement(std::string type, std::string id, std::string title,
                std::filesystem::path file = "");

    /// @brief Default constructor (for deserialization)
    BookElement() = default;

    // Getters
    const std::string& getType() const { return m_type; }
    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::filesystem::path& getFile() const { return m_file; }
    int getWordCount() const { return m_wordCount; }
    const std::chrono::system_clock::time_point& getCreated() const { return m_created; }
    const std::chrono::system_clock::time_point& getModified() const { return m_modified; }

    /// @brief Check if this is a known type
    /// @return true if type is one of the predefined BookElementTypes constants
    bool isKnownType() const;

    // Setters
    void setType(const std::string& type);
    void setTitle(const std::string& title);
    void setFile(const std::filesystem::path& file);
    void setWordCount(int count);
    void setModified(const std::chrono::system_clock::time_point& time);

    /// @brief Update modified timestamp to now
    void touch();

    /// @brief Set custom metadata field
    /// @param key Metadata key
    /// @param value Metadata value
    ///
    /// Metadata is fully extensible - plugins and users can add custom fields.
    /// Examples: "pov", "location", "timeline", "citation_style", etc.
    void setMetadata(const std::string& key, const std::string& value);

    /// @brief Get custom metadata field
    /// @param key Metadata key
    /// @return Optional value if key exists, std::nullopt otherwise
    std::optional<std::string> getMetadata(const std::string& key) const;

    /// @brief Get all metadata
    /// @return Const reference to metadata map
    const std::map<std::string, std::string>& getAllMetadata() const { return m_metadata; }

    /// @brief Remove metadata field
    /// @param key Metadata key to remove
    void removeMetadata(const std::string& key);

    /// @brief Clear all metadata
    void clearMetadata();

    /// @brief Serialize to JSON
    /// @return JSON object with all fields
    json toJson() const;

    /// @brief Deserialize from JSON
    /// @param j JSON object
    /// @return BookElement with populated fields
    /// @throws json::exception if required fields are missing
    static BookElement fromJson(const json& j);

private:
    std::string m_type;           ///< Element type (flexible string, not enum)
    std::string m_id;             ///< Unique identifier (UUID)
    std::string m_title;          ///< Display title
    std::filesystem::path m_file; ///< Relative path to RTF content file
    int m_wordCount = 0;          ///< Cached word count (updated on save)
    std::chrono::system_clock::time_point m_created;   ///< Creation timestamp
    std::chrono::system_clock::time_point m_modified;  ///< Last modification timestamp
    std::map<std::string, std::string> m_metadata;     ///< Extensible custom metadata

    /// @brief Helper: Convert time_point to ISO 8601 string
    static std::string timeToString(const std::chrono::system_clock::time_point& time);

    /// @brief Helper: Convert ISO 8601 string to time_point
    static std::chrono::system_clock::time_point stringToTime(const std::string& str);
};

} // namespace core
} // namespace kalahari
