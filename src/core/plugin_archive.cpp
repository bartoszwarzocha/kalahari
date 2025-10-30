/// @file plugin_archive.cpp
/// @brief Plugin archive implementation

#include <kalahari/core/plugin_archive.h>
#include <kalahari/core/logger.h>
#include <zip.h>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

#ifdef __linux__
#include <cstdlib>
#elif __APPLE__
#include <cstdlib>
#elif _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace kalahari {
namespace core {

PluginArchive::PluginArchive(const std::filesystem::path& plugin_path)
    : m_archive_path(plugin_path), m_is_valid(false) {

    // Generate unique temporary directory
    auto temp_base = getTempPluginDir();

    // Create temp directory if it doesn't exist
    try {
        std::filesystem::create_directories(temp_base);
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::getInstance().error("PluginArchive: Failed to create temp directory {}: {}",
                                   temp_base.string(), e.what());
        return;
    }

    // Generate unique subdirectory: <timestamp>_<plugin_filename>
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << timestamp << "_" << plugin_path.stem().string();

    m_extracted_dir = temp_base / oss.str();

    // Extract archive
    m_is_valid = extractArchive(m_archive_path, m_extracted_dir);

    if (m_is_valid) {
        Logger::getInstance().debug("PluginArchive: Extracted {} to {}",
                                   plugin_path.filename().string(),
                                   m_extracted_dir.string());
    }
}

PluginArchive::~PluginArchive() {
    if (!m_extracted_dir.empty() && std::filesystem::exists(m_extracted_dir)) {
        try {
            std::filesystem::remove_all(m_extracted_dir);
            Logger::getInstance().debug("PluginArchive: Cleaned up temp directory: {}",
                                       m_extracted_dir.string());
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::getInstance().warn("PluginArchive: Failed to cleanup temp directory {}: {}",
                                      m_extracted_dir.string(), e.what());
        }
    }
}

PluginArchive::PluginArchive(PluginArchive&& other) noexcept
    : m_archive_path(std::move(other.m_archive_path)),
      m_extracted_dir(std::move(other.m_extracted_dir)),
      m_is_valid(other.m_is_valid) {
    other.m_is_valid = false;
    other.m_extracted_dir.clear();
}

PluginArchive& PluginArchive::operator=(PluginArchive&& other) noexcept {
    if (this != &other) {
        // Cleanup current resources
        if (!m_extracted_dir.empty() && std::filesystem::exists(m_extracted_dir)) {
            try {
                std::filesystem::remove_all(m_extracted_dir);
            } catch (...) {
                // Ignore cleanup errors during move
            }
        }

        // Move from other
        m_archive_path = std::move(other.m_archive_path);
        m_extracted_dir = std::move(other.m_extracted_dir);
        m_is_valid = other.m_is_valid;

        // Invalidate other
        other.m_is_valid = false;
        other.m_extracted_dir.clear();
    }
    return *this;
}

bool PluginArchive::isValid() const {
    return m_is_valid;
}

const std::filesystem::path& PluginArchive::getExtractedPath() const {
    return m_extracted_dir;
}

bool PluginArchive::hasManifest() const {
    if (!m_is_valid) {
        return false;
    }

    std::filesystem::path manifest_path = m_extracted_dir / "manifest.json";
    return std::filesystem::exists(manifest_path) && std::filesystem::is_regular_file(manifest_path);
}

const std::filesystem::path& PluginArchive::getArchivePath() const {
    return m_archive_path;
}

std::filesystem::path PluginArchive::getTempPluginDir() {
#ifdef __linux__
    // XDG Base Directory specification
    const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
    if (xdg_data_home && xdg_data_home[0] != '\0') {
        return std::filesystem::path(xdg_data_home) / "Kalahari" / "plugins" / "temp";
    }

    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".local" / "share" / "Kalahari" / "plugins" / "temp";
    }

    // Fallback
    return std::filesystem::temp_directory_path() / "Kalahari" / "plugins";

#elif __APPLE__
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "Kalahari" / "plugins" / "temp";
    }

    // Fallback
    return std::filesystem::temp_directory_path() / "Kalahari" / "plugins";

#elif _WIN32
    // Use LOCALAPPDATA
    wchar_t* local_app_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data))) {
        std::filesystem::path result = local_app_data;
        CoTaskMemFree(local_app_data);
        return result / "Kalahari" / "plugins" / "temp";
    }

    // Fallback
    return std::filesystem::temp_directory_path() / "Kalahari" / "plugins";

#else
    // Generic fallback
    return std::filesystem::temp_directory_path() / "Kalahari" / "plugins";
#endif
}

bool PluginArchive::extractArchive(const std::filesystem::path& archive_path,
                                   const std::filesystem::path& target_dir) {
    int zip_error = 0;

    // Open ZIP archive
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_RDONLY, &zip_error);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("PluginArchive: Failed to open archive {}: {}",
                                   archive_path.filename().string(),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return false;
    }

    // Get number of entries
    zip_int64_t num_entries = zip_get_num_entries(archive, 0);

    if (num_entries < 0) {
        Logger::getInstance().error("PluginArchive: Failed to get entry count from {}",
                                   archive_path.filename().string());
        zip_close(archive);
        return false;
    }

    // Create target directory
    try {
        std::filesystem::create_directories(target_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::getInstance().error("PluginArchive: Failed to create extraction directory {}: {}",
                                   target_dir.string(), e.what());
        zip_close(archive);
        return false;
    }

    // Extract each entry
    for (zip_int64_t i = 0; i < num_entries; ++i) {
        const char* entry_name = zip_get_name(archive, i, 0);

        if (!entry_name) {
            Logger::getInstance().warn("PluginArchive: Failed to get name for entry {}", i);
            continue;
        }

        std::filesystem::path entry_path = target_dir / entry_name;

        // Check if entry is a directory (ends with /)
        std::string entry_name_str(entry_name);
        if (entry_name_str.back() == '/') {
            // Create directory
            try {
                std::filesystem::create_directories(entry_path);
            } catch (const std::filesystem::filesystem_error& e) {
                Logger::getInstance().warn("PluginArchive: Failed to create directory {}: {}",
                                          entry_path.string(), e.what());
            }
            continue;
        }

        // Ensure parent directory exists
        try {
            std::filesystem::create_directories(entry_path.parent_path());
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::getInstance().warn("PluginArchive: Failed to create parent directory for {}: {}",
                                      entry_path.string(), e.what());
            continue;
        }

        // Open file in archive
        zip_file_t* zip_file = zip_fopen_index(archive, i, 0);

        if (!zip_file) {
            Logger::getInstance().warn("PluginArchive: Failed to open entry {} in archive",
                                      entry_name);
            continue;
        }

        // Create output file
        std::ofstream output_file(entry_path, std::ios::binary);

        if (!output_file) {
            Logger::getInstance().warn("PluginArchive: Failed to create output file: {}",
                                      entry_path.string());
            zip_fclose(zip_file);
            continue;
        }

        // Read and write file content
        char buffer[8192];
        zip_int64_t bytes_read = 0;

        while ((bytes_read = zip_fread(zip_file, buffer, sizeof(buffer))) > 0) {
            output_file.write(buffer, bytes_read);
        }

        output_file.close();
        zip_fclose(zip_file);

        if (bytes_read < 0) {
            Logger::getInstance().warn("PluginArchive: Error reading entry {} from archive",
                                      entry_name);
        }
    }

    zip_close(archive);

    Logger::getInstance().info("PluginArchive: Extracted {} entries from {} to {}",
                              num_entries,
                              archive_path.filename().string(),
                              target_dir.string());

    return true;
}

} // namespace core
} // namespace kalahari
