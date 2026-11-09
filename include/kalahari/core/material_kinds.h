/// @file material_kinds.h
/// @brief The kinds of materials of the base package, and the kind that the source of a
/// material gives it

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace kalahari {
namespace core {

/// @brief Ids of the kinds of materials of the base package (ProjectManager::BASE_PACKAGE),
/// and the kind that a file or a web address of a material gives it
///
/// The Materials of the Workshop have the working notes and the kinds of materials; a book
/// saved before the kinds of materials has its materials of one kind, OLD_MATERIAL, which
/// ProjectManager converts when it opens the book.
struct MaterialKinds {
    static constexpr const char* WORK_NOTE = "work_note";      ///< A note written in the program
    static constexpr const char* QUOTE = "quote";              ///< Someone else's words
    static constexpr const char* WEB_PAGE = "web_page";        ///< The address of a page
    static constexpr const char* PICTURE = "picture";          ///< A picture, also on the web
    static constexpr const char* DOCUMENT = "document";        ///< A file with a text
    static constexpr const char* PUBLICATION = "publication";  ///< A book or an article
    static constexpr const char* RECORDING = "recording";      ///< A sound or a film
    static constexpr const char* OTHER_FILE = "other_file";    ///< Any other file
    /// The one kind of materials of earlier versions, retired (ElementKind::retired)
    static constexpr const char* OLD_MATERIAL = "material";

    /// @brief The kinds of materials, in the order of the packages, without the working notes
    static const QStringList& all();

    /// @brief Kind of a material with the file @p path, by the type of the file: a picture, a
    /// recording (sound or film), a document (PDF, Word, ODT, RTF, TXT, Markdown, a
    /// spreadsheet, a presentation, an e-book), or another file
    ///
    /// The type comes from the contents of the file and from its name; of a file that is not
    /// there, from its name alone.
    static QString ofFile(const QString& path);

    /// @brief Kind of a material with the web address @p url and the file @p path (absolute;
    /// empty: none): with an address, a picture when the address names one or the material has
    /// a copy of it, else a web page; without one, the kind of its file (ofFile()); without
    /// both, an empty id
    static QString ofSource(const QUrl& url, const QString& path);

    /// @brief Whether @p name, the name of a file, has the suffix of a picture (.jpg, .png...)
    static bool isPictureName(const QString& name);

    /// @brief Whether @p url names a picture by the end of its path (.jpg, .png...), which the
    /// program tells without connecting to it; a page of a wiki about a picture
    /// (".../wiki/File:Camel.jpg") is a page
    static bool isPictureAddress(const QUrl& url);
};

} // namespace core
} // namespace kalahari
