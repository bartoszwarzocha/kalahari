/// @file book.h
/// @brief Book class - complete book structure with 3 sections
///
/// Book represents a complete book with professional structure:
/// - Front Matter: title page, copyright, dedication, preface, etc.
/// - Body: Parts containing Chapters (main content)
/// - Back Matter: epilogue, glossary, bibliography, about author, etc.
///
/// This 3-section structure follows publishing industry standards and supports
/// all common book elements while remaining flexible for custom types.
///
/// Example JSON representation:
/// @code{.json}
/// {
///   "frontMatter": [
///     {"type": "title_page", "id": "fm-001", "title": "Title Page", ...},
///     {"type": "dedication", "id": "fm-002", "title": "Dedication", ...}
///   ],
///   "body": [
///     {
///       "id": "part-001",
///       "title": "Part I: The Beginning",
///       "chapters": [...]
///     }
///   ],
///   "backMatter": [
///     {"type": "epilogue", "id": "bm-001", "title": "Epilogue", ...},
///     {"type": "bibliography", "id": "bm-002", "title": "Bibliography", ...}
///   ]
/// }
/// @endcode

#pragma once

#include <kalahari/core/book_element.h>
#include <kalahari/core/part.h>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Book - 3-section structure (Front Matter + Body + Back Matter)
///
/// Book provides the complete hierarchical structure for a book project:
/// - **Front Matter**: Introductory sections (title page, copyright, dedication, preface)
/// - **Body**: Main content organized as Parts → Chapters
/// - **Back Matter**: Closing sections (epilogue, glossary, bibliography, about author)
///
/// Example usage:
/// @code{.cpp}
/// Book book;
///
/// // Add front matter
/// auto titlePage = std::make_shared<BookElement>(
///     BookElementTypes::TITLE_PAGE, generateId(), "Title Page",
///     "content/frontmatter/title_page.rtf");
/// book.addFrontMatter(titlePage);
///
/// // Add body (parts with chapters)
/// Part part1("part-001", "Part I: The Beginning");
/// // ... add chapters to part1
/// book.addPart(std::make_shared<Part>(part1));
///
/// // Add back matter
/// auto epilogue = std::make_shared<BookElement>(
///     BookElementTypes::EPILOGUE, generateId(), "Epilogue",
///     "content/backmatter/epilogue.rtf");
/// book.addBackMatter(epilogue);
///
/// // Get total word count (body only)
/// int totalWords = book.getWordCount();
///
/// // Serialize
/// json j = book.toJson();
/// @endcode
class Book {
public:
    /// @brief Default constructor
    Book() = default;

    // Getters (non-const for modification)
    std::vector<std::shared_ptr<BookElement>>& getFrontMatter() { return m_frontMatter; }
    std::vector<std::shared_ptr<Part>>& getBody() { return m_body; }
    std::vector<std::shared_ptr<BookElement>>& getBackMatter() { return m_backMatter; }

    // Const getters
    const std::vector<std::shared_ptr<BookElement>>& getFrontMatter() const { return m_frontMatter; }
    const std::vector<std::shared_ptr<Part>>& getBody() const { return m_body; }
    const std::vector<std::shared_ptr<BookElement>>& getBackMatter() const { return m_backMatter; }

    /// @brief Add element to front matter
    /// @param element BookElement (typically title_page, copyright, dedication, preface)
    void addFrontMatter(std::shared_ptr<BookElement> element);

    /// @brief Add part to body
    /// @param part Part containing chapters
    void addPart(std::shared_ptr<Part> part);

    /// @brief Add element to back matter
    /// @param element BookElement (typically epilogue, glossary, bibliography, about_author)
    void addBackMatter(std::shared_ptr<BookElement> element);

    /// @brief Remove front matter element by ID
    /// @param elementId Element identifier
    /// @return true if found and removed
    bool removeFrontMatter(const std::string& elementId);

    /// @brief Remove part by ID
    /// @param partId Part identifier
    /// @return true if found and removed
    bool removePart(const std::string& partId);

    /// @brief Remove back matter element by ID
    /// @param elementId Element identifier
    /// @return true if found and removed
    bool removeBackMatter(const std::string& elementId);

    /// @brief Get total word count (body only)
    /// @return Sum of all chapter word counts across all parts
    ///
    /// Note: Front matter and back matter are excluded from the count.
    /// This follows industry standard (novel word counts don't include prelims).
    int getWordCount() const;

    /// @brief Get total chapter count across all parts
    /// @return Number of chapters in body
    size_t getChapterCount() const;

    /// @brief Get number of parts in body
    /// @return Part count
    size_t getPartCount() const { return m_body.size(); }

    /// @brief Check if book is empty (no content)
    /// @return true if all three sections are empty
    bool isEmpty() const;

    /// @brief Clear all content (front matter, body, back matter)
    ///
    /// Removes all sections. Use with caution - this is irreversible.
    void clearAll();

    /// @brief Serialize to JSON
    /// @return JSON object with 3 section arrays
    json toJson() const;

    /// @brief Deserialize from JSON
    /// @param j JSON object
    /// @return Book with populated sections
    /// @throws json::exception if JSON structure is invalid
    static Book fromJson(const json& j);

private:
    std::vector<std::shared_ptr<BookElement>> m_frontMatter;  ///< Front matter sections
    std::vector<std::shared_ptr<Part>> m_body;                ///< Parts containing chapters
    std::vector<std::shared_ptr<BookElement>> m_backMatter;   ///< Back matter sections
};

} // namespace core
} // namespace kalahari
