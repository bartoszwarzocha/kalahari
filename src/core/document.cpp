/// @file document.cpp
/// @brief Document implementation with JSON serialization

#include <kalahari/core/document.h>
#include <kalahari/core/document_archive.h>
#include <kalahari/core/logger.h>
#include <random>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace kalahari {
namespace core {

// ===========================================================================
// Constructor
// ===========================================================================

Document::Document(std::string title, std::string author, std::string language)
    : m_id(generateId())
    , m_title(std::move(title))
    , m_author(std::move(author))
    , m_language(std::move(language))
    , m_genre("")
    , m_created(std::chrono::system_clock::now())
    , m_modified(std::chrono::system_clock::now())
{
}

// ===========================================================================
// Setters
// ===========================================================================

void Document::setTitle(const std::string& title) {
    m_title = title;
    touch();
}

void Document::setAuthor(const std::string& author) {
    m_author = author;
    touch();
}

void Document::setLanguage(const std::string& language) {
    m_language = language;
    touch();
}

void Document::setGenre(const std::string& genre) {
    m_genre = genre;
    touch();
}

void Document::touch() {
    m_modified = std::chrono::system_clock::now();
}

// ===========================================================================
// Save/Load (DocumentArchive integration)
// ===========================================================================

bool Document::save(const std::filesystem::path& path) {
    // Use DocumentArchive for ZIP operations
    // Phase 0 MVP: Saves manifest.json only
    // Phase 2: Full implementation with RTF files
    return DocumentArchive::save(*this, path);
}

std::optional<Document> Document::load(const std::filesystem::path& path) {
    // Use DocumentArchive for ZIP operations
    // Phase 0 MVP: Loads manifest.json only
    // Phase 2: Full implementation with RTF file extraction
    return DocumentArchive::load(path);
}

// ===========================================================================
// JSON Serialization
// ===========================================================================

json Document::toJson() const {
    json j;

    // Version (manifest format version)
    j["version"] = "1.0.0";

    // Document metadata
    json docMetadata;
    docMetadata["id"] = m_id;
    docMetadata["title"] = m_title;
    docMetadata["author"] = m_author;
    docMetadata["language"] = m_language;

    // Optional fields
    if (!m_genre.empty()) {
        docMetadata["genre"] = m_genre;
    }

    // Timestamps
    docMetadata["created"] = timeToString(m_created);
    docMetadata["modified"] = timeToString(m_modified);

    j["document"] = docMetadata;

    // Book structure
    j["book"] = m_book.toJson();

    return j;
}

Document Document::fromJson(const json& j) {
    Document doc;

    // Version check (optional - for future compatibility)
    if (j.contains("version")) {
        std::string version = j["version"].get<std::string>();
        Logger::getInstance().debug("Loading document with manifest version: {}", version);
    }

    // Document metadata - required
    const json& docMetadata = j.at("document");
    doc.m_id = docMetadata.at("id").get<std::string>();
    doc.m_title = docMetadata.at("title").get<std::string>();
    doc.m_author = docMetadata.at("author").get<std::string>();
    doc.m_language = docMetadata.at("language").get<std::string>();

    // Optional fields
    doc.m_genre = docMetadata.value("genre", "");

    // Timestamps - optional, default to now
    if (docMetadata.contains("created") && docMetadata["created"].is_string()) {
        doc.m_created = stringToTime(docMetadata["created"].get<std::string>());
    } else {
        doc.m_created = std::chrono::system_clock::now();
    }

    if (docMetadata.contains("modified") && docMetadata["modified"].is_string()) {
        doc.m_modified = stringToTime(docMetadata["modified"].get<std::string>());
    } else {
        doc.m_modified = std::chrono::system_clock::now();
    }

    // Book structure - required
    const json& bookJson = j.at("book");
    doc.m_book = Book::fromJson(bookJson);

    return doc;
}

// ===========================================================================
// UUID Generation (Simple Phase 0 Implementation)
// ===========================================================================

std::string Document::generateId() {
    // Simple UUID v4 alternative for Phase 0:
    // Format: "timestamp-random" (e.g., "1730281234567-a3f2")
    // Collision-resistant for single-user application

    // Get current timestamp in milliseconds
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Generate 4-digit random hex (0000-ffff)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);
    unsigned int randomHex = dis(gen);

    // Format: timestamp-hex
    std::ostringstream oss;
    oss << ms << "-" << std::hex << std::setw(4) << std::setfill('0') << randomHex;

    return oss.str();
}

// ===========================================================================
// Time Conversion Helpers (ISO 8601)
// ===========================================================================

std::string Document::timeToString(const std::chrono::system_clock::time_point& time) {
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

std::chrono::system_clock::time_point Document::stringToTime(const std::string& str) {
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
