/// @file document_archive.cpp
/// @brief Document archive implementation

#include <kalahari/core/document_archive.h>
#include <kalahari/core/logger.h>
#include <zip.h>
#include <fstream>
#include <sstream>

namespace kalahari {
namespace core {

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

    // Check if file exists
    if (!std::filesystem::exists(archive_path)) {
        Logger::getInstance().error("DocumentArchive: File not found: {}",
                                   archive_path.string());
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

    // Allocate memory that libzip will own and free
    // CRITICAL: zip_source_buffer with freep=0 does NOT copy data!
    // Data must remain valid until zip_close() is called.
    // Solution: allocate with malloc() and let libzip free it.
    size_t size = manifest_str.size();
    char* buffer = static_cast<char*>(malloc(size));
    if (!buffer) {
        Logger::getInstance().error("DocumentArchive: Failed to allocate buffer for manifest");
        return false;
    }
    memcpy(buffer, manifest_str.data(), size);

    // Create ZIP source from buffer
    // Pass 'free' as freep so libzip will free the buffer when done
    zip_source_t* source = zip_source_buffer(archive,
                                             buffer,
                                             size,
                                             1);  // 1 = libzip will call free()

    if (!source) {
        Logger::getInstance().error("DocumentArchive: Failed to create ZIP source for manifest: {}",
                                   zip_strerror(archive));
        free(buffer);  // Free manually if source creation failed
        return false;
    }

    // Add manifest.json to ZIP at root
    zip_int64_t index = zip_file_add(archive, "manifest.json", source, ZIP_FL_OVERWRITE);

    if (index < 0) {
        Logger::getInstance().error("DocumentArchive: Failed to add manifest.json to ZIP: {}",
                                   zip_strerror(archive));
        zip_source_free(source);  // This will also free the buffer
        return false;
    }

    Logger::getInstance().debug("DocumentArchive: Wrote manifest.json ({} bytes)", size);
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

} // namespace core
} // namespace kalahari
