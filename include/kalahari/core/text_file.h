/// @file text_file.h
/// @brief Plain text files read and written in their own encoding and line ends

#pragma once

#include <QString>
#include <QStringConverter>
#include <optional>

namespace kalahari::core {

/// @brief Encoding and line ends of a plain text file
struct TextFileFormat {
    QStringConverter::Encoding encoding = QStringConverter::Utf8;  ///< Encoding of the text
    bool byteOrderMark = false;  ///< The file starts with a byte order mark
    bool crlf = false;           ///< Lines end with CR LF (Windows) instead of LF

    /// @brief Format of a new text file: UTF-8 without a byte order mark, the system's line ends
    static TextFileFormat newFile();

    bool operator==(const TextFileFormat& other) const = default;
};

/// @brief Text of a plain text file, lines separated by '\n', with the file's format
struct TextFileContent {
    QString text;           ///< Text of the file
    TextFileFormat format;  ///< How the file stores it
};

/// @brief Read a plain text file
///
/// A byte order mark gives the encoding. Without one the text is UTF-8 when it is valid
/// UTF-8, otherwise the system's 8-bit encoding (Latin-1 where the system uses UTF-8).
/// Line ends (CR LF, LF or CR) become '\n'.
/// @param error Set to the reason when the file cannot be read
std::optional<TextFileContent> readTextFile(const QString& path, QString* error = nullptr);

/// @brief Write @p text (lines separated by '\n') to a file in @p format, replacing it in one step
///
/// Text that the format's encoding cannot hold is written as UTF-8, and @p format is
/// updated to say so.
/// @param error Set to the reason when the file cannot be written
bool writeTextFile(const QString& path, const QString& text, TextFileFormat& format,
                   QString* error = nullptr);

}  // namespace kalahari::core
