/// @file plugin_archive.cpp
/// @brief Plugin archive implementation

#include <kalahari/core/plugin_archive.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>
#include <zip.h>
#include <QCoreApplication>
#include <QString>
#include <fstream>
#include <chrono>
#include <cctype>
#include <exception>
#include <memory>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <vector>

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

namespace {

/// @brief A message for the writer, translated (lupdate finds it in gui/utils/core_texts.cpp)
QString message(const char* source) {
    return QCoreApplication::translate("kalahari::core::PluginArchive", source);
}

/// @brief Path @p path as text, for a message
QString textOf(const std::filesystem::path& path) {
    return QString::fromStdString(pathText(path));
}

/// @brief Give the writer @p text in @p error, and log it
/// @return false: the archive was not unpacked
bool failed(std::string& error, const QString& text) {
    error = text.toStdString();
    Logger::getInstance().error("PluginArchive: {}", error);
    return false;
}

/// @brief Whether file @p name of an archive (not empty) is written in the folder the archive
/// is unpacked to: it has no root ("/x", "\\x"), no drive ("C:x") and no ".."
///
/// "\\" separates the folders of a path on Windows, so it counts as a separator on every
/// system: an archive unpacks the same everywhere.
bool staysInFolder(std::string_view name) {
    if (name.front() == '/' || name.front() == '\\') {
        return false;
    }
    if (name.size() >= 2 && name[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(name.front())) != 0) {
        return false;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t end = name.find_first_of("/\\", start);
        if (name.substr(start, end == std::string_view::npos ? end : end - start) == "..") {
            return false;
        }
        if (end == std::string_view::npos) {
            return true;
        }
        start = end + 1;
    }
}

/// @brief Closes an archive of libzip, or a file of it
struct ZipCloser {
    void operator()(zip_t* archive) const { zip_discard(archive); }
    void operator()(zip_file_t* file) const { zip_fclose(file); }
};

}  // namespace

PluginArchive::PluginArchive(const std::filesystem::path& plugin_path)
    : m_archive_path(plugin_path), m_is_valid(false) {
    try {
        // Generate unique subdirectory: <timestamp>_<random>_<plugin_filename>
        // The random part keeps directories distinct when the same plugin is extracted
        // twice within one second (reload, several app instances, parallel tests);
        // otherwise one archive's destructor would delete the other's files.
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::random_device randomDevice;
        const auto randomPart =
            (static_cast<std::uint64_t>(randomDevice()) << 32) | randomDevice();

        std::ostringstream oss;
        oss << timestamp << "_" << std::hex << randomPart << std::dec << "_";
        std::filesystem::path name = oss.str();
        name += plugin_path.stem();

        m_extracted_dir = getTempPluginDir() / name;

        // Extract archive (its folder is made with the folders above it)
        m_is_valid = extractArchive(m_archive_path, m_extracted_dir);
    } catch (const std::exception& e) {
        // The writer gets the reason as a message, as of every other archive not unpacked
        m_is_valid = failed(m_error, message("The plugin \"%1\" cannot be unpacked: %2")
                                         .arg(textOf(plugin_path.filename()),
                                              QString::fromLocal8Bit(e.what())));
    }

    if (m_is_valid) {
        Logger::getInstance().debug("PluginArchive: Extracted {} to {}",
                                   pathText(plugin_path.filename()),
                                   pathText(m_extracted_dir));
    } else if (!m_extracted_dir.empty()) {
        // Nothing of an archive that was not unpacked stays
        std::error_code ignored;
        std::filesystem::remove_all(m_extracted_dir, ignored);
    }
}

PluginArchive::~PluginArchive() {
    if (!m_extracted_dir.empty() && std::filesystem::exists(m_extracted_dir)) {
        try {
            std::filesystem::remove_all(m_extracted_dir);
            Logger::getInstance().debug("PluginArchive: Cleaned up temp directory: {}",
                                       pathText(m_extracted_dir));
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::getInstance().warn("PluginArchive: Failed to cleanup temp directory {}: {}",
                                      pathText(m_extracted_dir), e.what());
        }
    }
}

PluginArchive::PluginArchive(PluginArchive&& other) noexcept
    : m_archive_path(std::move(other.m_archive_path)),
      m_extracted_dir(std::move(other.m_extracted_dir)),
      m_is_valid(other.m_is_valid),
      m_error(std::move(other.m_error)) {
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
                Logger::getInstance().warn("PluginArchive: Unknown exception during move cleanup (ignored)");
            }
        }

        // Move from other
        m_archive_path = std::move(other.m_archive_path);
        m_extracted_dir = std::move(other.m_extracted_dir);
        m_is_valid = other.m_is_valid;
        m_error = std::move(other.m_error);

        // Invalidate other
        other.m_is_valid = false;
        other.m_extracted_dir.clear();
    }
    return *this;
}

bool PluginArchive::isValid() const {
    return m_is_valid;
}

const std::string& PluginArchive::errorMessage() const {
    return m_error;
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
    // Tests unpack into their own temporary folder, never into the user's profile
    if (SettingsManager::isTestMode()) {
        return std::filesystem::temp_directory_path() / "Kalahari" / "plugins";
    }

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
    const QString plugin = textOf(archive_path.filename());
    const QString cannotOpen = message("The plugin \"%1\" cannot be opened: its file is not a ZIP "
                                       "archive, it is damaged or it cannot be read.")
                                   .arg(plugin);

    // Open ZIP archive
    int zip_error = 0;
    const std::unique_ptr<zip_t, ZipCloser> archive(
        openZipFile(archive_path, ZIP_RDONLY, &zip_error));

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("PluginArchive: Failed to open archive {}: {}",
                                   pathText(archive_path.filename()),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return failed(m_error, cannotOpen);
    }

    // Get number of entries
    const zip_int64_t num_entries = zip_get_num_entries(archive.get(), 0);

    if (num_entries < 0) {
        return failed(m_error, cannotOpen);
    }

    // The names first: nothing of an archive with a file that has no name, or that would be
    // written outside the plugin's folder, is written anywhere
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(num_entries));
    for (zip_int64_t i = 0; i < num_entries; ++i) {
        // libzip gives the names of the entries in UTF-8
        const char* entry_name = zip_get_name(archive.get(), static_cast<zip_uint64_t>(i), 0);

        if (!entry_name) {
            return failed(m_error, cannotOpen);
        }
        const std::string_view name(entry_name);
        if (name.empty()) {
            return failed(m_error, message("The plugin \"%1\" was not unpacked: one of its files "
                                           "has no name.")
                                       .arg(plugin));
        }
        if (!staysInFolder(name)) {
            return failed(m_error, message("The plugin \"%1\" was not unpacked: its file \"%2\" "
                                           "would be written outside the plugin's folder.")
                                       .arg(plugin, QString::fromUtf8(name.data(),
                                                                      static_cast<qsizetype>(
                                                                          name.size()))));
        }
        names.emplace_back(name);
    }

    // Extract each entry
    const auto cannotCreate = [&](const std::filesystem::path& folder) {
        return failed(m_error, message("The plugin \"%1\" cannot be unpacked: the folder \"%2\" "
                                       "cannot be created.")
                                   .arg(plugin, textOf(folder)));
    };
    const auto cannotRead = [&](const std::string& name) {
        return failed(m_error, message("The plugin \"%1\" cannot be unpacked: its file \"%2\" "
                                       "cannot be read from the archive, which may be damaged.")
                                   .arg(plugin, QString::fromStdString(name)));
    };
    const auto cannotWrite = [&](const std::filesystem::path& file) {
        return failed(m_error, message("The plugin \"%1\" cannot be unpacked: the file \"%2\" "
                                       "cannot be written.")
                                   .arg(plugin, textOf(file)));
    };

    std::error_code ec;
    std::filesystem::create_directories(target_dir, ec);
    if (ec || !std::filesystem::is_directory(target_dir)) {
        return cannotCreate(target_dir);
    }

    for (zip_int64_t i = 0; i < num_entries; ++i) {
        const std::string& name = names[static_cast<std::size_t>(i)];
        const std::filesystem::path entry_path = (target_dir / pathOfText(name)).make_preferred();

        // The folder of the entry; a folder of the archive ends with "/", so it is its own
        const std::filesystem::path folder = entry_path.parent_path();
        std::filesystem::create_directories(folder, ec);
        if (ec || !std::filesystem::is_directory(folder)) {
            return cannotCreate(folder);
        }
        if (name.back() == '/') {
            continue;
        }

        // Open file in archive
        const std::unique_ptr<zip_file_t, ZipCloser> zip_file(
            zip_fopen_index(archive.get(), static_cast<zip_uint64_t>(i), 0));

        if (!zip_file) {
            return cannotRead(name);
        }

        // Create output file
        std::ofstream output_file(entry_path, std::ios::binary);

        if (!output_file) {
            return cannotWrite(entry_path);
        }

        // Read and write file content
        char buffer[8192];
        zip_int64_t bytes_read = 0;

        while ((bytes_read = zip_fread(zip_file.get(), buffer, sizeof(buffer))) > 0) {
            output_file.write(buffer, bytes_read);
        }

        if (bytes_read < 0) {
            return cannotRead(name);
        }
        output_file.close();
        if (!output_file) {
            return cannotWrite(entry_path);
        }
    }

    Logger::getInstance().info("PluginArchive: Extracted {} entries from {} to {}",
                              num_entries,
                              pathText(archive_path.filename()),
                              pathText(target_dir));

    return true;
}

} // namespace core
} // namespace kalahari
