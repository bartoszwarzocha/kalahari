/// @file unicode_paths.h
/// @brief Paths with any characters: as UTF-8 text, from UTF-8 text, and the ZIP archives in
/// them
///
/// On Windows, std::filesystem::path::string() converts a path to the code page of the system
/// and throws when the code page has no place for a character of the path ("ś" in a code page
/// of Western Europe, "Ж" in a Polish one), and a path made from narrow text takes the text in
/// that code page. A folder of the user, of a book or of the program can have any name, so the
/// program takes the text of a path in UTF-8, the encoding of its log, of Qt and of libzip's
/// names of entries.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

struct zip;  // zip_t of libzip

namespace kalahari::core {

/// @brief Path @p path as UTF-8 text, for the log, the messages and Python
std::string pathText(const std::filesystem::path& path);

/// @brief The path of UTF-8 text @p text, e.g. of the name of an entry of a ZIP archive
std::filesystem::path pathOfText(std::string_view text);

/// @brief Open the ZIP archive @p path with libzip, as zip_open() does
///
/// zip_open() takes a narrow file name, which on Windows is not Unicode; this takes the path
/// itself, so an archive in a folder of any name opens.
/// @param flags Flags of zip_open(): ZIP_RDONLY, ZIP_CREATE...
/// @param error Gets the error of libzip (ZIP_ER_*) when the archive does not open; may be null
/// @return The archive, closed with zip_close() or zip_discard(); nullptr when it does not open
zip* openZipFile(const std::filesystem::path& path, int flags, int* error);

}  // namespace kalahari::core
