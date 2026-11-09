/// @file material_file.h
/// @brief The file of a material as the program shows it: its name, format, size and picture

#pragma once

#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief What the program knows of the file of a material: its name, format and size, and
/// whether it is a picture the program can show
///
/// A picture is a file of an image type ("image/png", "image/webp"...). The program shows those
/// whose format its image plugins read; for the others problem() says why it does not.
struct MaterialFile {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::MaterialFile)

public:
    QString path;           ///< Absolute path
    QString name;           ///< Name of the file: "mapa.png"
    QString format;         ///< Format in capitals: "PNG", "PDF"; empty without a suffix
    bool exists = false;    ///< Whether the file is on the disk
    bool picture = false;   ///< Whether it is a picture by its type
    bool readable = false;  ///< Whether it is a picture the program can show
    QSize size;             ///< Pixels of a picture the program can show
    qint64 bytes = 0;       ///< Size of the file
    QString readError;      ///< Why a picture of a known format cannot be read

    /// @brief The file @p path
    static MaterialFile of(const QString& path);

    /// @brief The picture could not be read after all, for @p reason (a damaged file whose
    /// beginning its image plugin knows): the program does not show it
    void unreadable(const QString& reason);

    /// @brief One line about the file: "mapa.png · PNG · 1600 × 1000 px · 245 KB"; the parts
    /// it does not have are left out
    QString summary() const;

    /// @brief Why the program does not show the picture, in a sentence; empty when it shows it
    /// or the file is no picture
    QString problem() const;
};

/// @brief Picture @p path turned by @p rotation degrees clockwise (0, 90, 180 or 270), at most
/// @p bounds large when they are valid; a null image when it cannot be read, and then why in
/// @p error when it is given
QImage readPicture(const QString& path, int rotation = 0, const QSize& bounds = QSize(),
                   QString* error = nullptr);

} // namespace gui
} // namespace kalahari
