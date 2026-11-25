// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file material_file.h
/// @brief The file of a material as the program shows it: its name, format, size and picture

#ifndef KALAHARI_GUI_MATERIAL_FILE_H
#define KALAHARI_GUI_MATERIAL_FILE_H

#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <QString>

namespace kalahari::gui {

/// @brief What the program knows of the file of a material: its name, format and size, and
/// whether it is a picture the program can show
///
/// A picture is a file of an image type ("image/png", "image/webp"...). The program shows those
/// whose format its image plugins read; for the others problem() says why it does not. A PDF
/// file is no picture: the program shows its pages when it is built with their preview
/// (PDFium), and its first page where it shows a picture (readPicture()).
struct MaterialFile {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::MaterialFile)

public:
    QString path;           ///< Absolute path
    QString name;           ///< Name of the file: "mapa.png"
    QString format;         ///< Format in capitals: "PNG", "PDF"; empty without a suffix
    bool exists = false;    ///< Whether the file is on the disk
    bool picture = false;   ///< Whether it is a picture by its type
    bool pdf = false;       ///< Whether it is a PDF file
    bool readable = false;  ///< Whether it is a picture, or a PDF file, the program can show
    int pages = 0;          ///< Pages of a PDF file the program can show
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
    [[nodiscard]] QString summary() const;

    /// @brief Why the program does not show the picture, in a sentence; empty when it shows it
    /// or the file is no picture
    [[nodiscard]] QString problem() const;
};

/// @brief Picture @p path turned by @p rotation degrees clockwise (0, 90, 180 or 270), at most
/// @p bounds large when they are valid; a null image when it cannot be read, and then why in
/// @p error when it is given. Of a PDF file, its first page.
QImage readPicture(const QString& path, int rotation = 0, const QSize& bounds = QSize(),
                   QString* error = nullptr);

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_MATERIAL_FILE_H
