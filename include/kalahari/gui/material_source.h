/// @file material_source.h
/// @brief Where a material comes from: a file, a web page, a picture on the web or none, and
/// what comes to the Navigator from outside the program

#pragma once

#include "kalahari/core/book_project.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

class QMimeData;

namespace kalahari {
namespace gui {

/// @brief Where a material comes from, as the program shows and opens it
enum class MaterialSource {
    File,        ///< A copy of a file from the disk, in the Workshop's folder
    Page,        ///< A web page: its address
    WebPicture,  ///< A picture on the web: its address, and its copy in the book if it has one
    None,        ///< Neither a file nor an address: a paper book, a talk, a quotation
};

/// @brief The source of material @p element: a picture on the web when its address names a
/// picture or it has a copy of one
MaterialSource materialSourceOf(const core::ProjectElement& element);

/// @brief The web address @p text is, without spaces around it: an http or https address with
/// its host; an empty address for any other text
QUrl webAddress(const QString& text);

/// @brief Whether @p url names a picture by the end of its path (.jpg, .png...), which the
/// program tells without connecting to it; a page of a wiki about a picture
/// (".../wiki/File:Camel.jpg") is a page
bool isPictureAddress(const QUrl& url);

/// @brief The title the end of @p url gives: "Kalahari" from ".../wiki/Kalahari", "Camel on a
/// dune" from ".../Camel_on_a_dune.jpg"; its host, without "www.", when its path has none
QString titleOfAddress(const QUrl& url);

/// @brief A name for the copy of the picture at @p url: the end of its path, without the
/// characters a file name cannot have; "picture" when it has none. @p suffix ("jpg"), when
/// given, is the suffix of the name.
QString fileNameOfAddress(const QUrl& url, const QString& suffix = QString());

/// @brief What comes from outside the program, dropped on the Navigator or pasted in it: files,
/// web addresses, a picture without a file (a screenshot), or a text that is none of these
struct IncomingMaterials {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::IncomingMaterials)

public:
    QStringList files;      ///< Files on the disk, absolute
    QList<QUrl> addresses;  ///< Web addresses
    QImage picture;         ///< A picture without a file
    QString text;           ///< A text that is no file and no address

    /// @brief Whether it gives materials: files, addresses or a picture
    bool hasMaterials() const {
        return !files.isEmpty() || !addresses.isEmpty() || !picture.isNull();
    }

    /// @brief What @p data has: the files and web addresses of its list of addresses, else its
    /// picture, else its text, which can be a web address or the path of a file too ("Copy as
    /// path" puts it in quotes)
    static IncomingMaterials of(const QMimeData* data);

    /// @brief The title of a material from a picture without a file, pasted at @p time:
    /// "Picture from the clipboard 20.10.2026 17:05"
    static QString pictureTitle(const QDateTime& time);

    /// @brief The name of the PNG file of a picture without a file, pasted at @p time:
    /// "clipboard_20261020_170512.png"
    static QString pictureFileName(const QDateTime& time);
};

} // namespace gui
} // namespace kalahari
