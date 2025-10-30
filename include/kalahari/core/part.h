/// @file part.h
/// @brief Part class - container for book chapters
///
/// A Part represents a major division in a book's body structure, containing
/// multiple chapters. Parts are optional - books can have a single default part,
/// or multiple parts for complex structures.
///
/// Example JSON representation:
/// @code{.json}
/// {
///   "id": "part-001",
///   "title": "Part I: The Beginning",
///   "chapters": [
///     {
///       "type": "chapter",
///       "id": "ch-001",
///       "title": "Chapter 1: The Journey Starts",
///       "file": "content/body/part-001/chapter-001.rtf",
///       "wordCount": 2500
///     },
///     {
///       "type": "chapter",
///       "id": "ch-002",
///       "title": "Chapter 2: The First Challenge",
///       "file": "content/body/part-001/chapter-002.rtf",
///       "wordCount": 3200
///     }
///   ]
/// }
/// @endcode

#pragma once

#include <kalahari/core/book_element.h>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Part - container for chapters in book body structure
///
/// Part provides logical grouping for chapters (e.g., "Part I", "Part II").
/// Books can have:
/// - Single part (default) - GUI hides part structure
/// - Multiple parts - GUI shows expandable tree structure
///
/// Example usage:
/// @code{.cpp}
/// // Create a part
/// Part part("part-001", "Part I: The Beginning");
///
/// // Add chapters
/// auto ch1 = std::make_shared<BookElement>(
///     BookElementTypes::CHAPTER, "ch-001", "Chapter 1",
///     "content/body/part-001/chapter-001.rtf");
/// ch1->setWordCount(2500);
/// part.addChapter(ch1);
///
/// // Get aggregate word count
/// int totalWords = part.getWordCount();  // Sum of all chapters
///
/// // Serialize to JSON
/// json j = part.toJson();
/// @endcode
class Part {
public:
    /// @brief Construct a Part
    /// @param id Unique identifier (UUID recommended)
    /// @param title Display title (e.g., "Part I: The Beginning")
    Part(std::string id, std::string title);

    /// @brief Default constructor (for deserialization)
    Part() = default;

    // Getters
    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }
    const std::vector<std::shared_ptr<BookElement>>& getChapters() const { return m_chapters; }

    // Setters
    void setTitle(const std::string& title);

    /// @brief Add a chapter to this part
    /// @param chapter BookElement with type="chapter"
    ///
    /// Note: This method does not validate that the element is a chapter.
    /// It's the caller's responsibility to ensure correct type.
    void addChapter(std::shared_ptr<BookElement> chapter);

    /// @brief Remove a chapter by ID
    /// @param chapterId Chapter identifier to remove
    /// @return true if chapter was found and removed, false otherwise
    bool removeChapter(const std::string& chapterId);

    /// @brief Get a chapter by ID
    /// @param chapterId Chapter identifier
    /// @return Shared pointer to chapter if found, nullptr otherwise
    std::shared_ptr<BookElement> getChapter(const std::string& chapterId) const;

    /// @brief Move a chapter from one position to another
    /// @param fromIndex Source index
    /// @param toIndex Destination index
    /// @return true if move succeeded, false if indices invalid
    ///
    /// Used for drag-and-drop reordering in GUI.
    bool moveChapter(size_t fromIndex, size_t toIndex);

    /// @brief Get total word count (sum of all chapters)
    /// @return Aggregate word count across all chapters
    int getWordCount() const;

    /// @brief Get number of chapters
    /// @return Chapter count
    size_t getChapterCount() const { return m_chapters.size(); }

    /// @brief Check if part is empty
    /// @return true if part has no chapters
    bool isEmpty() const { return m_chapters.empty(); }

    /// @brief Clear all chapters
    ///
    /// Removes all chapters from this part. Use with caution.
    void clearChapters();

    /// @brief Serialize to JSON
    /// @return JSON object with id, title, and chapters array
    json toJson() const;

    /// @brief Deserialize from JSON
    /// @param j JSON object
    /// @return Part with populated fields and chapters
    /// @throws json::exception if required fields are missing
    static Part fromJson(const json& j);

private:
    std::string m_id;      ///< Unique identifier (UUID)
    std::string m_title;   ///< Display title (e.g., "Part I")
    std::vector<std::shared_ptr<BookElement>> m_chapters;  ///< Chapters in this part
};

} // namespace core
} // namespace kalahari
