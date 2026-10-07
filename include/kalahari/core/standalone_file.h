/// @file standalone_file.h
/// @brief Chapters and plain text files opened on their own, outside a book project

#pragma once

#include <kalahari/core/text_file.h>
#include <QString>
#include <optional>

namespace kalahari::core {

/// @brief File that the editor opens outside a book project, saved back in its own format
class StandaloneFile {
public:
    /// @brief Kind of file, by its extension
    enum class Type {
        Chapter,     ///< Kalahari chapter (.kchapter)
        PlainText,   ///< Plain text (.txt)
        Unsupported  ///< Any other file
    };

    /// @brief Kind of the file at @p path
    static Type typeOf(const QString& path);

    /// @brief Read a chapter or plain text file
    /// @param kml Set to the file's content as KML; the lines of a text file become paragraphs
    /// @param error Set to the reason when a text file cannot be read (a chapter's is logged)
    /// @return The file, or nullopt when it is not supported or cannot be read
    static std::optional<StandaloneFile> open(const QString& path, QString& kml,
                                              QString* error = nullptr);

    StandaloneFile() = default;

    QString path() const { return m_path; }  ///< Path of the file
    Type type() const { return m_type; }     ///< Kind of the file

    /// @brief Write the editor's content to the file in its own format
    ///
    /// A chapter keeps its other data (title, status, notes), a text file its encoding and
    /// line ends.
    /// @param kml Content as KML, written to a chapter
    /// @param text Content as plain text, paragraphs separated by '\n', written to a text file
    /// @param error Set to the reason when a text file cannot be written
    bool save(const QString& kml, const QString& text, QString* error = nullptr);

    /// @brief Write the editor's content to another file, which this file becomes
    ///
    /// The extension of @p path gives the format. A chapter saved as a chapter keeps its
    /// other data, a text file saved as a text file its encoding and line ends; a new text
    /// file is UTF-8.
    bool saveAs(const QString& path, const QString& kml, const QString& text,
                QString* error = nullptr);

private:
    QString m_path;
    Type m_type = Type::Unsupported;
    TextFileFormat m_textFormat;  ///< Encoding and line ends of a text file
};

}  // namespace kalahari::core
