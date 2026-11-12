/// @file image_store.h
/// @brief The image files of a book: where they are, their sizes and their pictures
///
/// The images of a text name their files relative to the book's main folder
/// ("images/map.png"). The store finds the files there, keeps the size of each and the
/// pictures drawn lately (scaled to the pixels they are drawn in, so a page of photos does
/// not keep the photos whole), and copies new images into the book's folder of images,
/// where a name taken by another file gets a number ("map-2.png") and a file with the same
/// content is used again.

#pragma once

#include <QCache>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QString>

#include <optional>

#include "kalahari/editor/text_images.h"

namespace kalahari::editor {

/// @brief The image files of a book: their place, sizes and pictures
class ImageStore {
public:
    /// @brief The most memory the pictures drawn lately take, in kilobytes
    static constexpr int PICTURE_CACHE_KB = 96 * 1024;

    ImageStore();

    /// @brief Set the book's main folder, the one the sources are relative to; an empty one:
    ///        none (every image is missing, and none can be added)
    void setFolder(const QString& folder);

    /// @brief The book's main folder; empty: none
    const QString& folder() const { return m_folder; }

    /// @brief Whether the store has a folder: images can be shown and added
    bool hasFolder() const { return !m_folder.isEmpty(); }

    /// @brief The file of a source, a path on the disk; empty when the store has no folder
    ///        or the source is not one of the book's folder (isSafeImageSource())
    QString filePath(const QString& source) const;

    /// @brief The size of the image of a source in its pixels (an SVG: its own size);
    ///        std::nullopt: the file is missing or is not an image the editor reads
    std::optional<QSize> pixelSize(const QString& source) const;

    /// @brief The size of the image of a file in its pixels (an SVG: its own size);
    ///        std::nullopt: it is missing or is not an image the editor reads
    static std::optional<QSize> fileSize(const QString& path);

    /// @brief The picture of a source in these pixels; null: the file is missing or is not
    ///        an image
    QImage picture(const QString& source, const QSize& pixels) const;

    /// @brief The picture of an image as it is shown, in these pixels: its file's picture
    ///        cropped, turned, mirrored and recolored (shownPicture()); null: the file is
    ///        missing or is not an image
    QImage shownImage(const ImageProperties& image, const QSize& pixels) const;

    /// @brief The pixels an image has of its own: those of its file cropped and turned;
    ///        std::nullopt: the file is missing or is not an image
    std::optional<QSize> shownPixelSize(const ImageProperties& image) const;

    /// @brief Copy a file into the book's folder of images
    /// @return The source of the copy ("images/map.png"): the file itself when it is there
    ///         already, a file of the same content when the folder has one, else a copy, its
    ///         name with a number when another file has its name; empty: it could not be
    ///         copied, or the store has no folder
    QString addFile(const QString& path);

    /// @brief Save a picture (pasted, a screenshot) in the book's folder of images as PNG
    /// @param baseName Its name without the extension; a taken one gets a number
    /// @return Its source; empty: it could not be saved, or the store has no folder
    QString addPicture(const QImage& picture, const QString& baseName);

    /// @brief The source in this book of an image of another book's folder: the same one
    ///        when the folders are the same or the file is missing, else that of its copy
    QString adoptSource(const QString& otherFolder, const QString& source);

    /// @brief Read the file of a source again the next time it is asked for (it changed)
    void forget(const QString& source);

private:
    /// @brief Write data into the folder of images as a file named after @p name: the name
    ///        of a file with the same content, else the first free one of name, name-2...
    QString storeData(const QByteArray& data, const QString& name);

    QString m_folder;  ///< The book's main folder, without a slash at its end
    mutable QHash<QString, std::optional<QSize>> m_sizes;  ///< By path
    mutable QCache<QString, QImage> m_pictures;            ///< By path and pixels
};

}  // namespace kalahari::editor
