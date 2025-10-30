/// @file part.cpp
/// @brief Part implementation with chapter management

#include <kalahari/core/part.h>
#include <kalahari/core/logger.h>
#include <algorithm>

namespace kalahari {
namespace core {

// ===========================================================================
// Constructor
// ===========================================================================

Part::Part(std::string id, std::string title)
    : m_id(std::move(id))
    , m_title(std::move(title))
{
}

// ===========================================================================
// Setters
// ===========================================================================

void Part::setTitle(const std::string& title) {
    m_title = title;
}

// ===========================================================================
// Chapter Management
// ===========================================================================

void Part::addChapter(std::shared_ptr<BookElement> chapter) {
    if (!chapter) {
        Logger::getInstance().warn("Attempted to add null chapter to part '{}'", m_id);
        return;
    }

    m_chapters.push_back(std::move(chapter));
}

bool Part::removeChapter(const std::string& chapterId) {
    auto it = std::remove_if(m_chapters.begin(), m_chapters.end(),
        [&chapterId](const std::shared_ptr<BookElement>& ch) {
            return ch && ch->getId() == chapterId;
        });

    if (it != m_chapters.end()) {
        m_chapters.erase(it, m_chapters.end());
        return true;
    }

    return false;
}

std::shared_ptr<BookElement> Part::getChapter(const std::string& chapterId) const {
    auto it = std::find_if(m_chapters.begin(), m_chapters.end(),
        [&chapterId](const std::shared_ptr<BookElement>& ch) {
            return ch && ch->getId() == chapterId;
        });

    if (it != m_chapters.end()) {
        return *it;
    }

    return nullptr;
}

bool Part::moveChapter(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_chapters.size() || toIndex >= m_chapters.size()) {
        Logger::getInstance().warn("Invalid move indices for part '{}': from={}, to={}, size={}",
                                   m_id, fromIndex, toIndex, m_chapters.size());
        return false;
    }

    if (fromIndex == toIndex) {
        return true;  // No-op
    }

    // Move element
    auto chapter = m_chapters[fromIndex];
    m_chapters.erase(m_chapters.begin() + fromIndex);
    m_chapters.insert(m_chapters.begin() + toIndex, chapter);

    return true;
}

int Part::getWordCount() const {
    int total = 0;
    for (const auto& chapter : m_chapters) {
        if (chapter) {
            total += chapter->getWordCount();
        }
    }
    return total;
}

void Part::clearChapters() {
    m_chapters.clear();
}

// ===========================================================================
// JSON Serialization
// ===========================================================================

json Part::toJson() const {
    json j;

    // Required fields
    j["id"] = m_id;
    j["title"] = m_title;

    // Chapters array
    json chaptersArray = json::array();
    for (const auto& chapter : m_chapters) {
        if (chapter) {
            chaptersArray.push_back(chapter->toJson());
        }
    }
    j["chapters"] = chaptersArray;

    return j;
}

Part Part::fromJson(const json& j) {
    Part part;

    // Required fields - will throw if missing
    part.m_id = j.at("id").get<std::string>();
    part.m_title = j.at("title").get<std::string>();

    // Chapters array - optional
    if (j.contains("chapters") && j["chapters"].is_array()) {
        for (const auto& chapterJson : j["chapters"]) {
            try {
                auto chapter = std::make_shared<BookElement>(BookElement::fromJson(chapterJson));
                part.m_chapters.push_back(chapter);
            } catch (const json::exception& e) {
                Logger::getInstance().error("Failed to parse chapter in part '{}': {}",
                                           part.m_id, e.what());
                // Continue parsing other chapters
            }
        }
    }

    return part;
}

} // namespace core
} // namespace kalahari
