/// @file plugin_archive.h
/// @brief ZIP plugin archive extraction and management
///
/// This file provides the PluginArchive class, a RAII wrapper for extracting
/// .kplugin ZIP archives to a temporary directory. The archive is automatically
/// cleaned up when the object is destroyed.
///
/// Example usage:
/// @code
/// PluginArchive archive("/path/to/plugin.kplugin");
/// if (archive.isValid()) {
///     auto extracted_path = archive.getExtractedPath();
///     // Work with extracted files...
/// }
/// // Automatic cleanup on destruction
/// @endcode

#pragma once

#include <string>
#include <filesystem>

namespace kalahari {
namespace core {

/// @brief RAII wrapper for ZIP plugin archive extraction
///
/// Extracts .kplugin files to a temporary directory and ensures automatic
/// cleanup on destruction. Thread-safe for individual instances (not shared).
class PluginArchive {
public:
    /// @brief Extract .kplugin file to temporary directory
    /// @param plugin_path Path to .kplugin file
    ///
    /// Extracts the ZIP archive to ~/.local/share/Kalahari/plugins/temp/<unique_id>/
    /// The extraction happens in the constructor. Use isValid() to check success.
    explicit PluginArchive(const std::filesystem::path& plugin_path);

    /// @brief Cleanup extracted files on destruction
    ///
    /// Removes the temporary directory and all extracted files.
    /// Errors during cleanup are logged but don't throw exceptions.
    ~PluginArchive();

    // Prevent copying (move is allowed)
    PluginArchive(const PluginArchive&) = delete;
    PluginArchive& operator=(const PluginArchive&) = delete;

    // Allow moving
    PluginArchive(PluginArchive&& other) noexcept;
    PluginArchive& operator=(PluginArchive&& other) noexcept;

    /// @brief Check if extraction was successful
    /// @return true if archive was extracted successfully
    bool isValid() const;

    /// @brief Get path to extracted directory
    /// @return Path to temporary directory containing extracted files
    ///
    /// Returns empty path if extraction failed. Check isValid() first.
    const std::filesystem::path& getExtractedPath() const;

    /// @brief Check if manifest.json exists in extracted files
    /// @return true if manifest.json is present
    bool hasManifest() const;

    /// @brief Get the original .kplugin file path
    /// @return Path to source .kplugin file
    const std::filesystem::path& getArchivePath() const;

private:
    /// @brief Get platform-specific temporary plugin directory
    /// @return Path to ~/.local/share/Kalahari/plugins/temp/ (or platform equivalent)
    static std::filesystem::path getTempPluginDir();

    /// @brief Extract ZIP archive to target directory
    /// @param archive_path Path to .kplugin file
    /// @param target_dir Directory to extract to
    /// @return true if extraction succeeded
    bool extractArchive(const std::filesystem::path& archive_path,
                       const std::filesystem::path& target_dir);

    std::filesystem::path m_archive_path;    ///< Original .kplugin file path
    std::filesystem::path m_extracted_dir;   ///< Temporary directory with extracted files
    bool m_is_valid;                         ///< Extraction success flag
};

} // namespace core
} // namespace kalahari
