/// @file document_archive.cpp
/// @brief Document archive implementation

#include <kalahari/core/document_archive.h>
#include <kalahari/core/logger.h>
#include <zip.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace kalahari {
namespace core {

// ===========================================================================
// RAII wrapper for libzip buffer management
// ===========================================================================

/// @brief RAII wrapper for malloc-allocated buffers used with libzip
///
/// libzip's zip_source_buffer() requires a malloc-allocated buffer when the
/// library should take ownership (freep=1). This wrapper ensures the buffer
/// is freed if an error occurs before ownership is transferred to libzip.
///
/// Usage:
/// @code
/// ZipSourceBuffer buffer(size);
/// if (!buffer.data()) return false;  // allocation failed
/// memcpy(buffer.data(), source, size);
/// zip_source_t* src = zip_source_buffer(archive, buffer.data(), size, 1);
/// if (src) buffer.release();  // libzip owns it now
/// @endcode
class ZipSourceBuffer {
public:
    /// @brief Allocate a buffer of the given size
    /// @param size Number of bytes to allocate
    explicit ZipSourceBuffer(size_t size)
        : m_data(static_cast<char*>(malloc(size))), m_size(size) {}

    /// @brief Free the buffer if still owned
    ~ZipSourceBuffer() {
        if (m_data) {
            free(m_data);
        }
    }

    // Non-copyable
    ZipSourceBuffer(const ZipSourceBuffer&) = delete;
    ZipSourceBuffer& operator=(const ZipSourceBuffer&) = delete;

    // Moveable
    ZipSourceBuffer(ZipSourceBuffer&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size) {
        other.m_data = nullptr;
        other.m_size = 0;
    }

    ZipSourceBuffer& operator=(ZipSourceBuffer&& other) noexcept {
        if (this != &other) {
            if (m_data) {
                free(m_data);
            }
            m_data = other.m_data;
            m_size = other.m_size;
            other.m_data = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    /// @brief Get pointer to buffer data
    /// @return Pointer to buffer, or nullptr if allocation failed
    char* data() noexcept { return m_data; }

    /// @brief Get buffer size
    size_t size() const noexcept { return m_size; }

    /// @brief Release ownership of the buffer
    /// @return Pointer to buffer (caller is responsible for freeing)
    ///
    /// Call this after successfully passing the buffer to libzip
    /// (i.e., after zip_source_buffer() succeeds with freep=1)
    char* release() noexcept {
        char* p = m_data;
        m_data = nullptr;
        m_size = 0;
        return p;
    }

private:
    char* m_data;
    size_t m_size;
};

// ===========================================================================
// Public API - Save
// ===========================================================================

bool DocumentArchive::save(const Document& doc, const std::filesystem::path& archive_path) {
    Logger::getInstance().info("DocumentArchive: Saving document '{}' to {}",
                              doc.getTitle(), archive_path.string());

    int zip_error = 0;

    // Create/overwrite ZIP archive
    // ZIP_CREATE: create if doesn't exist
    // ZIP_TRUNCATE: overwrite if exists
    zip_t* archive = zip_open(archive_path.string().c_str(),
                             ZIP_CREATE | ZIP_TRUNCATE,
                             &zip_error);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("DocumentArchive: Failed to create archive {}: {}",
                                   archive_path.filename().string(),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return false;
    }

    // Serialize document to JSON
    json manifest;
    try {
        manifest = doc.toJson();
    } catch (const json::exception& e) {
        Logger::getInstance().error("DocumentArchive: Failed to serialize document: {}", e.what());
        zip_discard(archive);  // Close without saving
        return false;
    }

    // Write manifest.json to ZIP
    if (!writeManifest(archive, manifest)) {
        Logger::getInstance().error("DocumentArchive: Failed to write manifest.json");
        zip_discard(archive);
        return false;
    }

    // Phase 0 MVP: Skip RTF file copying
    // Phase 2 TODO: Iterate through book structure and copy RTF files
    // - doc.getBook().getFrontMatter() → content/frontmatter/*.rtf
    // - doc.getBook().getBody() → content/body/part_*/chapter_*.rtf
    // - doc.getBook().getBackMatter() → content/backmatter/*.rtf

    // Close and save ZIP
    if (zip_close(archive) < 0) {
        Logger::getInstance().error("DocumentArchive: Failed to close archive: {}",
                                   zip_strerror(archive));
        zip_discard(archive);
        return false;
    }

    Logger::getInstance().info("DocumentArchive: Successfully saved document to {}",
                              archive_path.string());
    return true;
}

// ===========================================================================
// Public API - Load
// ===========================================================================

std::optional<Document> DocumentArchive::load(const std::filesystem::path& archive_path) {
    Logger::getInstance().info("DocumentArchive: Loading document from {}",
                              archive_path.string());

    // Validate path before loading
    if (!validateArchivePath(archive_path)) {
        return std::nullopt;
    }

    int zip_error = 0;

    // Open ZIP archive (read-only)
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_RDONLY, &zip_error);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("DocumentArchive: Failed to open archive {}: {}",
                                   archive_path.filename().string(),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return std::nullopt;
    }

    // Read manifest.json from ZIP
    auto manifest_opt = readManifest(archive);

    if (!manifest_opt) {
        Logger::getInstance().error("DocumentArchive: Failed to read manifest.json");
        zip_close(archive);
        return std::nullopt;
    }

    const json& manifest = *manifest_opt;

    // Parse JSON to Document
    Document doc;
    try {
        doc = Document::fromJson(manifest);
    } catch (const json::exception& e) {
        Logger::getInstance().error("DocumentArchive: Failed to parse manifest.json: {}", e.what());
        zip_close(archive);
        return std::nullopt;
    }

    // Phase 0 MVP: Skip RTF file extraction
    // Phase 2 TODO: Extract RTF files to temp directory for lazy loading
    // - Create temp directory (similar to PluginArchive::getTempPluginDir())
    // - Extract content/*.rtf files
    // - Update BookElement file paths to point to extracted temp files

    zip_close(archive);

    Logger::getInstance().info("DocumentArchive: Successfully loaded document '{}'",
                              doc.getTitle());
    return doc;
}

// ===========================================================================
// Private Helpers - Manifest
// ===========================================================================

bool DocumentArchive::writeManifest(zip_t* archive, const json& manifest) {
    // Serialize JSON to string (4-space indent for readability)
    std::string manifest_str;
    try {
        manifest_str = manifest.dump(4);
    } catch (const json::exception& e) {
        Logger::getInstance().error("DocumentArchive: Failed to dump JSON: {}", e.what());
        return false;
    }

    // Use RAII wrapper for buffer management
    // CRITICAL: zip_source_buffer with freep=0 does NOT copy data!
    // Data must remain valid until zip_close() is called.
    // Solution: allocate with malloc() via ZipSourceBuffer, let libzip free it.
    ZipSourceBuffer buffer(manifest_str.size());
    if (!buffer.data()) {
        Logger::getInstance().error("DocumentArchive: Failed to allocate buffer for manifest");
        return false;
    }
    std::memcpy(buffer.data(), manifest_str.data(), manifest_str.size());

    // Create ZIP source from buffer
    // Pass 1 as freep so libzip will free the buffer when done
    zip_source_t* source = zip_source_buffer(archive,
                                             buffer.data(),
                                             buffer.size(),
                                             1);  // 1 = libzip will call free()

    if (!source) {
        Logger::getInstance().error("DocumentArchive: Failed to create ZIP source for manifest: {}",
                                   zip_strerror(archive));
        // RAII: buffer is automatically freed by ~ZipSourceBuffer()
        return false;
    }

    // Transfer ownership to libzip - buffer will NOT be freed by ~ZipSourceBuffer()
    buffer.release();

    // Add manifest.json to ZIP at root
    zip_int64_t index = zip_file_add(archive, "manifest.json", source, ZIP_FL_OVERWRITE);

    if (index < 0) {
        Logger::getInstance().error("DocumentArchive: Failed to add manifest.json to ZIP: {}",
                                   zip_strerror(archive));
        zip_source_free(source);  // This will also free the buffer (owned by libzip now)
        return false;
    }

    Logger::getInstance().debug("DocumentArchive: Wrote manifest.json ({} bytes)", manifest_str.size());
    return true;
}

std::optional<json> DocumentArchive::readManifest(zip_t* archive) {
    // Locate manifest.json in ZIP
    zip_int64_t index = zip_name_locate(archive, "manifest.json", 0);

    if (index < 0) {
        Logger::getInstance().error("DocumentArchive: manifest.json not found in archive");
        return std::nullopt;
    }

    // Get file info (size)
    zip_stat_t stat;
    if (zip_stat_index(archive, index, 0, &stat) < 0) {
        Logger::getInstance().error("DocumentArchive: Failed to stat manifest.json: {}",
                                   zip_strerror(archive));
        return std::nullopt;
    }

    // Open file in ZIP
    zip_file_t* file = zip_fopen_index(archive, index, 0);

    if (!file) {
        Logger::getInstance().error("DocumentArchive: Failed to open manifest.json: {}",
                                   zip_strerror(archive));
        return std::nullopt;
    }

    // Read file content
    std::vector<char> buffer(stat.size);
    zip_int64_t bytes_read = zip_fread(file, buffer.data(), stat.size);

    zip_fclose(file);

    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != stat.size) {
        Logger::getInstance().error("DocumentArchive: Failed to read manifest.json (expected {} bytes, got {})",
                                   stat.size, bytes_read);
        return std::nullopt;
    }

    // Parse JSON
    json manifest;
    try {
        manifest = json::parse(buffer.begin(), buffer.end());
    } catch (const json::exception& e) {
        Logger::getInstance().error("DocumentArchive: Failed to parse manifest.json: {}", e.what());
        return std::nullopt;
    }

    Logger::getInstance().debug("DocumentArchive: Read manifest.json ({} bytes)", stat.size);
    return manifest;
}

// ===========================================================================
// Private Helpers - RTF Files (Phase 2 stubs)
// ===========================================================================

bool DocumentArchive::writeRTFFile([[maybe_unused]] zip_t* archive,
                                   [[maybe_unused]] const std::filesystem::path& source_rtf,
                                   [[maybe_unused]] const std::string& zip_path) {
    // Phase 0 MVP: Not implemented
    Logger::getInstance().warn("DocumentArchive::writeRTFFile() is a stub in Phase 0 MVP");

    // Phase 2 TODO:
    // 1. Check if source_rtf exists
    // 2. Create zip_source_file() from source_rtf
    // 3. zip_file_add() to add to ZIP at zip_path
    // 4. Return success/failure

    return false;
}

bool DocumentArchive::extractRTFFile([[maybe_unused]] zip_t* archive,
                                     [[maybe_unused]] const std::string& zip_path,
                                     [[maybe_unused]] const std::filesystem::path& target_path) {
    // Phase 0 MVP: Not implemented
    Logger::getInstance().warn("DocumentArchive::extractRTFFile() is a stub in Phase 0 MVP");

    // Phase 2 TODO:
    // 1. zip_name_locate() to find zip_path
    // 2. zip_fopen_index() to open file
    // 3. Read chunks and write to target_path (similar to PluginArchive::extractArchive)
    // 4. Return success/failure

    return false;
}

// ===========================================================================
// Private Helpers - Path Validation
// ===========================================================================

bool DocumentArchive::validateArchivePath(const std::filesystem::path& path) {
    auto& logger = Logger::getInstance();

    // Check file exists
    if (!std::filesystem::exists(path)) {
        logger.error("DocumentArchive: File does not exist: {}", path.string());
        return false;
    }

    // Check is regular file (not directory, symlink, etc.)
    if (!std::filesystem::is_regular_file(path)) {
        logger.error("DocumentArchive: Not a regular file: {}", path.string());
        return false;
    }

    // Validate extension (.klh is the main format, .kbackup for backups)
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".klh" && ext != ".kbackup") {
        logger.error("DocumentArchive: Invalid extension '{}' - expected .klh or .kbackup", ext);
        return false;
    }

    // Canonicalize path to prevent path traversal
    std::error_code ec;
    auto canonical = std::filesystem::canonical(path, ec);
    if (ec) {
        logger.error("DocumentArchive: Failed to canonicalize path: {}", ec.message());
        return false;
    }

    return true;
}

} // namespace core
} // namespace kalahari
