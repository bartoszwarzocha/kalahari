/// @file book.cpp
/// @brief Book implementation with 3-section management

#include <kalahari/core/book.h>
#include <kalahari/core/logger.h>
#include <algorithm>

namespace kalahari {
namespace core {

// ===========================================================================
// Section Management - Add
// ===========================================================================

void Book::addFrontMatter(std::shared_ptr<BookElement> element) {
    if (!element) {
        Logger::getInstance().warn("Attempted to add null element to front matter");
        return;
    }
    m_frontMatter.push_back(std::move(element));
}

void Book::addPart(std::shared_ptr<Part> part) {
    if (!part) {
        Logger::getInstance().warn("Attempted to add null part to body");
        return;
    }
    m_body.push_back(std::move(part));
}

void Book::addBackMatter(std::shared_ptr<BookElement> element) {
    if (!element) {
        Logger::getInstance().warn("Attempted to add null element to back matter");
        return;
    }
    m_backMatter.push_back(std::move(element));
}

// ===========================================================================
// Section Management - Remove
// ===========================================================================

bool Book::removeFrontMatter(const std::string& elementId) {
    auto it = std::remove_if(m_frontMatter.begin(), m_frontMatter.end(),
        [&elementId](const std::shared_ptr<BookElement>& el) {
            return el && el->getId() == elementId;
        });

    if (it != m_frontMatter.end()) {
        m_frontMatter.erase(it, m_frontMatter.end());
        return true;
    }
    return false;
}

bool Book::removePart(const std::string& partId) {
    auto it = std::remove_if(m_body.begin(), m_body.end(),
        [&partId](const std::shared_ptr<Part>& part) {
            return part && part->getId() == partId;
        });

    if (it != m_body.end()) {
        m_body.erase(it, m_body.end());
        return true;
    }
    return false;
}

bool Book::removeBackMatter(const std::string& elementId) {
    auto it = std::remove_if(m_backMatter.begin(), m_backMatter.end(),
        [&elementId](const std::shared_ptr<BookElement>& el) {
            return el && el->getId() == elementId;
        });

    if (it != m_backMatter.end()) {
        m_backMatter.erase(it, m_backMatter.end());
        return true;
    }
    return false;
}

bool Book::movePart(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_body.size() || toIndex >= m_body.size()) {
        Logger::getInstance().warn("Invalid move indices for parts: from={}, to={}, size={}",
                                   fromIndex, toIndex, m_body.size());
        return false;
    }

    if (fromIndex == toIndex) {
        return true;  // No-op
    }

    // Move element
    auto part = m_body[fromIndex];
    m_body.erase(m_body.begin() + fromIndex);
    m_body.insert(m_body.begin() + toIndex, part);

    return true;
}

// ===========================================================================
// Statistics
// ===========================================================================

size_t Book::getWordCount() const {
    // Body only (not front/back matter) - industry standard for novel word counts
    size_t total = 0;
    for (const auto& part : m_body) {
        if (part) {
            // Part::getWordCount() returns int, cast to size_t
            // Negative word counts from individual elements are clamped to 0
            int partWords = part->getWordCount();
            if (partWords > 0) {
                total += static_cast<size_t>(partWords);
            }
        }
    }
    return total;
}

size_t Book::getChapterCount() const {
    size_t total = 0;
    for (const auto& part : m_body) {
        if (part) {
            total += part->getChapterCount();
        }
    }
    return total;
}

bool Book::isEmpty() const {
    return m_frontMatter.empty() && m_body.empty() && m_backMatter.empty();
}

void Book::clearAll() {
    m_frontMatter.clear();
    m_body.clear();
    m_backMatter.clear();
}

// ===========================================================================
// JSON Serialization
// ===========================================================================

json Book::toJson() const {
    json j;

    // Front Matter array
    json frontMatterArray = json::array();
    for (const auto& element : m_frontMatter) {
        if (element) {
            frontMatterArray.push_back(element->toJson());
        }
    }
    j["frontMatter"] = frontMatterArray;

    // Body array (parts)
    json bodyArray = json::array();
    for (const auto& part : m_body) {
        if (part) {
            bodyArray.push_back(part->toJson());
        }
    }
    j["body"] = bodyArray;

    // Back Matter array
    json backMatterArray = json::array();
    for (const auto& element : m_backMatter) {
        if (element) {
            backMatterArray.push_back(element->toJson());
        }
    }
    j["backMatter"] = backMatterArray;

    return j;
}

Book Book::fromJson(const json& j) {
    Book book;

    // Front Matter - optional array
    if (j.contains("frontMatter") && j["frontMatter"].is_array()) {
        for (const auto& elementJson : j["frontMatter"]) {
            try {
                auto element = std::make_shared<BookElement>(BookElement::fromJson(elementJson));
                book.m_frontMatter.push_back(element);
            } catch (const json::exception& e) {
                Logger::getInstance().error("Failed to parse front matter element: {}", e.what());
                // Continue parsing other elements
            }
        }
    }

    // Body - optional array of parts
    if (j.contains("body") && j["body"].is_array()) {
        for (const auto& partJson : j["body"]) {
            try {
                auto part = std::make_shared<Part>(Part::fromJson(partJson));
                book.m_body.push_back(part);
            } catch (const json::exception& e) {
                Logger::getInstance().error("Failed to parse body part: {}", e.what());
                // Continue parsing other parts
            }
        }
    }

    // Back Matter - optional array
    if (j.contains("backMatter") && j["backMatter"].is_array()) {
        for (const auto& elementJson : j["backMatter"]) {
            try {
                auto element = std::make_shared<BookElement>(BookElement::fromJson(elementJson));
                book.m_backMatter.push_back(element);
            } catch (const json::exception& e) {
                Logger::getInstance().error("Failed to parse back matter element: {}", e.what());
                // Continue parsing other elements
            }
        }
    }

    return book;
}

} // namespace core
} // namespace kalahari
