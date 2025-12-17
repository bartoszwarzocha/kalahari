/// @file document_archive.h
/// @brief Document archive (.klh) save/load operations
///
/// DocumentArchive provides static methods for saving and loading Document
/// objects to/from .klh (Kalahari) ZIP archives. The .klh format is a ZIP
/// container with the following structure:
///
/// @code
/// my_document.klh/
/// ├── manifest.json          # Document metadata + book structure
/// └── content/               # RTF content files (Phase 2+)
///     ├── frontmatter/
///     │   ├── 001_title.rtf
///     │   └── 002_dedication.rtf
///     ├── body/
///     │   ├── part_001/
///     │   │   ├── chapter_001.rtf
///     │   │   └── chapter_002.rtf
///     │   └── part_002/
///     │       └── chapter_001.rtf
///     └── backmatter/
///         └── 001_epilogue.rtf
/// @endcode
///
/// Phase 0 MVP: Only manifest.json is saved/loaded (RTF paths stored, files not copied)
/// Phase 2+: Full implementation with RTF file archiving and lazy loading
///
/// Example usage:
/// @code{.cpp}
/// // Save document
/// Document doc("My Novel", "John Doe", "en");
/// // ... populate document ...
/// if (DocumentArchive::save(doc, "my_novel.klh")) {
///     std::cout << "Document saved!\n";
/// }
///
/// // Load document
/// auto loaded = DocumentArchive::load("my_novel.klh");
/// if (loaded) {
///     std::cout << "Loaded: " << loaded->getTitle() << "\n";
/// }
/// @endcode

#pragma once

#include <kalahari/core/document.h>
#include <filesystem>
#include <optional>

// Forward declaration for libzip types
struct zip;
typedef struct zip zip_t;

namespace kalahari {
namespace core {

/// @brief Static utility class for .klh archive operations
///
/// DocumentArchive provides save/load operations for Document objects.
/// Uses ZIP compression (libzip) to create .klh archive files.
///
/// All methods are static - no state is maintained.
class DocumentArchive {
public:
    /// @brief Save document to .klh archive file
    /// @param doc Document to save
    /// @param archive_path Path to .klh file (will be created/overwritten)
    /// @return true if save succeeded, false otherwise
    ///
    /// Phase 0 MVP: Saves only manifest.json
    /// Phase 2+: Saves manifest.json + all RTF content files
    ///
    /// Errors are logged via Logger. Common failures:
    /// - Cannot create/open ZIP archive (permissions, disk full)
    /// - Cannot write manifest.json (serialization error)
    /// - Cannot copy RTF files (Phase 2+)
    static bool save(const Document& doc, const std::filesystem::path& archive_path);

    /// @brief Load document from .klh archive file
    /// @param archive_path Path to .klh file
    /// @return Optional Document if load succeeded, std::nullopt otherwise
    ///
    /// Phase 0 MVP: Loads only manifest.json (RTF paths preserved but files not extracted)
    /// Phase 2+: Extracts RTF files to temp directory for lazy loading
    ///
    /// Errors are logged via Logger. Common failures:
    /// - Cannot open ZIP archive (file not found, corrupted, permissions)
    /// - Cannot read manifest.json (missing, invalid JSON)
    /// - Cannot extract RTF files (Phase 2+)
    static std::optional<Document> load(const std::filesystem::path& archive_path);

private:
    /// @brief Write manifest.json to ZIP archive
    /// @param archive Open ZIP archive handle
    /// @param manifest JSON object (Document::toJson() output)
    /// @return true if write succeeded
    ///
    /// Creates manifest.json at root of ZIP with formatted JSON (4-space indent).
    static bool writeManifest(zip_t* archive, const json& manifest);

    /// @brief Read manifest.json from ZIP archive
    /// @param archive Open ZIP archive handle
    /// @return Optional JSON object if read succeeded
    ///
    /// Reads manifest.json from root of ZIP and parses JSON.
    static std::optional<json> readManifest(zip_t* archive);

    // Phase 2+ methods (stubs for now):

    /// @brief Copy RTF file from filesystem to ZIP archive
    /// @param archive Open ZIP archive handle
    /// @param source_rtf Path to source RTF file
    /// @param zip_path Path inside ZIP (e.g., "content/body/part_001/chapter_001.rtf")
    /// @return true if copy succeeded
    ///
    /// Phase 0 MVP: Not implemented (returns false)
    /// Phase 2+: Reads source_rtf and adds to ZIP at zip_path
    static bool writeRTFFile(zip_t* archive,
                            const std::filesystem::path& source_rtf,
                            const std::string& zip_path);

    /// @brief Extract RTF file from ZIP archive to filesystem
    /// @param archive Open ZIP archive handle
    /// @param zip_path Path inside ZIP
    /// @param target_path Path to extract to
    /// @return true if extraction succeeded
    ///
    /// Phase 0 MVP: Not implemented (returns false)
    /// Phase 2+: Extracts zip_path from ZIP to target_path
    static bool extractRTFFile(zip_t* archive,
                              const std::string& zip_path,
                              const std::filesystem::path& target_path);

    /// @brief Validate archive path before loading
    /// @param path Path to validate
    /// @return true if path is valid for loading
    ///
    /// Validates:
    /// - File exists
    /// - Is a regular file (not directory, symlink, etc.)
    /// - Has valid extension (.kdoc or .kbackup)
    /// - Path can be canonicalized (no path traversal)
    static bool validateArchivePath(const std::filesystem::path& path);
};

} // namespace core
} // namespace kalahari
