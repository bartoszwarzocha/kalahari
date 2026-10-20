/// @file unicode_paths.cpp
/// @brief Paths with any characters: as UTF-8 text, from UTF-8 text, and the ZIP archives in
/// them

#include <kalahari/core/unicode_paths.h>

#include <zip.h>

namespace kalahari::core {

std::string pathText(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

std::filesystem::path pathOfText(std::string_view text) {
    return {std::u8string(text.begin(), text.end())};
}

zip* openZipFile(const std::filesystem::path& path, int flags, int* error) {
    zip_error_t zipError;
    zip_error_init(&zipError);
#ifdef _WIN32
    // Narrow file names are not Unicode on Windows
    zip_source_t* source = zip_source_win32w_create(path.c_str(), 0, -1, &zipError);
#else
    zip_source_t* source = zip_source_file_create(path.c_str(), 0, -1, &zipError);
#endif
    zip_t* archive = source ? zip_open_from_source(source, flags, &zipError) : nullptr;
    if (!archive) {
        if (source) {
            zip_source_free(source);
        }
        if (error) {
            *error = zip_error_code_zip(&zipError);
        }
    }
    zip_error_fini(&zipError);
    return archive;
}

}  // namespace kalahari::core
