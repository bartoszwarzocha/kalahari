/// @file book_element.cpp
/// @brief BookElement implementation with JSON serialization

#include <kalahari/core/book_element.h>
#include <kalahari/core/logger.h>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace kalahari {
namespace core {

// ===========================================================================
// Constructor
// ===========================================================================

BookElement::BookElement(std::string type, std::string id, std::string title,
                         std::filesystem::path file)
    : m_type(std::move(type))
    , m_id(std::move(id))
    , m_title(std::move(title))
    , m_file(std::move(file))
    , m_wordCount(0)
    , m_created(std::chrono::system_clock::now())
    , m_modified(std::chrono::system_clock::now())
{
}

// ===========================================================================
// Type Check
// ===========================================================================

bool BookElement::isKnownType() const {
    // Use utility function from book_constants.h
    return kalahari::core::isKnownType(m_type);
}

// ===========================================================================
// Setters
// ===========================================================================

void BookElement::setType(const std::string& type) {
    m_type = type;
    m_modified = std::chrono::system_clock::now();
}

void BookElement::setTitle(const std::string& title) {
    m_title = title;
    m_modified = std::chrono::system_clock::now();
}

void BookElement::setFile(const std::filesystem::path& file) {
    m_file = file;
    m_modified = std::chrono::system_clock::now();
}

void BookElement::setWordCount(int count) {
    m_wordCount = count;
    m_modified = std::chrono::system_clock::now();
}

void BookElement::setModified(const std::chrono::system_clock::time_point& time) {
    m_modified = time;
}

void BookElement::touch() {
    m_modified = std::chrono::system_clock::now();
}

// ===========================================================================
// Metadata Management
// ===========================================================================

void BookElement::setMetadata(const std::string& key, const std::string& value) {
    m_metadata[key] = value;
    m_isDirty = true;
    m_modified = std::chrono::system_clock::now();
}

std::optional<std::string> BookElement::getMetadata(const std::string& key) const {
    auto it = m_metadata.find(key);
    if (it != m_metadata.end()) {
        return it->second;
    }
    return std::nullopt;
}

void BookElement::removeMetadata(const std::string& key) {
    m_metadata.erase(key);
    m_modified = std::chrono::system_clock::now();
}

void BookElement::clearMetadata() {
    m_metadata.clear();
    m_modified = std::chrono::system_clock::now();
}

// ===========================================================================
// Dirty Tracking and Content Cache
// ===========================================================================

bool BookElement::isDirty() const noexcept {
    return m_isDirty;
}

void BookElement::setDirty(bool dirty) {
    m_isDirty = dirty;
}

bool BookElement::isContentLoaded() const noexcept {
    return !m_content.isEmpty();
}

const QString& BookElement::getContent() const {
    return m_content;
}

void BookElement::setContent(const QString& content) {
    m_content = content;
    m_isDirty = true;
    m_modified = std::chrono::system_clock::now();
}

void BookElement::unloadContent() {
    m_content.clear();
    // Note: m_isDirty is NOT changed - if content was dirty, it should be saved first
}

// ===========================================================================
// JSON Serialization
// ===========================================================================

json BookElement::toJson() const {
    json j;

    // Required fields
    j["type"] = m_type;
    j["id"] = m_id;
    j["title"] = m_title;
    j["file"] = m_file.generic_string();  // Convert path to generic string for cross-platform
    j["wordCount"] = m_wordCount;

    // Timestamps (ISO 8601 format)
    j["created"] = timeToString(m_created);
    j["modified"] = timeToString(m_modified);

    // Metadata map
    j["metadata"] = m_metadata;

    return j;
}

BookElement BookElement::fromJson(const json& j) {
    BookElement element;

    // Required fields - will throw if missing
    element.m_type = j.at("type").get<std::string>();
    element.m_id = j.at("id").get<std::string>();
    element.m_title = j.at("title").get<std::string>();

    // File path - handle as string, convert to path
    std::string filePath = j.at("file").get<std::string>();
    element.m_file = std::filesystem::path(filePath);

    // Word count - optional, default to 0
    element.m_wordCount = j.value("wordCount", 0);

    // Timestamps - optional, default to now
    if (j.contains("created") && j["created"].is_string()) {
        element.m_created = stringToTime(j["created"].get<std::string>());
    } else {
        element.m_created = std::chrono::system_clock::now();
    }

    if (j.contains("modified") && j["modified"].is_string()) {
        element.m_modified = stringToTime(j["modified"].get<std::string>());
    } else {
        element.m_modified = std::chrono::system_clock::now();
    }

    // Metadata - optional map
    if (j.contains("metadata") && j["metadata"].is_object()) {
        element.m_metadata = j["metadata"].get<std::map<std::string, std::string>>();
    }

    return element;
}

// ===========================================================================
// Time Conversion Helpers (ISO 8601)
// ===========================================================================

std::string BookElement::timeToString(const std::chrono::system_clock::time_point& time) {
    // Convert time_point to time_t
    auto timeT = std::chrono::system_clock::to_time_t(time);

    // Convert to tm struct (UTC)
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &timeT);
#else
    gmtime_r(&timeT, &tm);
#endif

    // Format as ISO 8601: YYYY-MM-DDTHH:MM:SSZ
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::chrono::system_clock::time_point BookElement::stringToTime(const std::string& str) {
    // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    std::tm tm = {};
    std::istringstream iss(str);

    // Parse the timestamp
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (iss.fail()) {
        // Failed to parse - log warning and return current time
        Logger::getInstance().warn("Failed to parse timestamp: {}, using current time", str);
        return std::chrono::system_clock::now();
    }

    // Convert tm to time_t (UTC)
#ifdef _WIN32
    auto timeT = _mkgmtime(&tm);
#else
    auto timeT = timegm(&tm);
#endif

    if (timeT == -1) {
        Logger::getInstance().warn("Invalid timestamp: {}, using current time", str);
        return std::chrono::system_clock::now();
    }

    // Convert time_t to time_point
    return std::chrono::system_clock::from_time_t(timeT);
}

} // namespace core
} // namespace kalahari
