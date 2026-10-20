/// @file material_file.cpp
/// @brief The file of a material as the program shows it

#include "kalahari/gui/material_file.h"

#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QStringList>
#include <QTransform>

namespace kalahari {
namespace gui {

namespace {

/// Bytes in a megabyte, from which the size has one decimal place
constexpr qint64 MEGABYTE = 1024 * 1024;

} // namespace

MaterialFile MaterialFile::of(const QString& path) {
    MaterialFile file;
    file.path = path;
    const QFileInfo info(path);
    file.name = info.fileName();
    file.format = info.suffix().toUpper();
    file.exists = info.isFile();
    if (!file.exists) {
        return file;
    }
    file.bytes = info.size();

    const QMimeType type = QMimeDatabase().mimeTypeForFile(info);
    file.picture = type.name().startsWith(QLatin1String("image/"));
    QImageReader reader(path);
    if (reader.canRead()) {
        // A file of another type that an image plugin reads is a picture too
        file.picture = true;
        file.readable = true;
        file.format = QString::fromLatin1(reader.format()).toUpper();
        file.size = reader.size();
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
            file.size.transpose();
        }
    } else if (file.picture) {
        file.readError = reader.errorString();
    }
    if (file.format.isEmpty() && file.picture) {
        file.format = type.preferredSuffix().toUpper();
    }
    return file;
}

void MaterialFile::unreadable(const QString& reason) {
    readable = false;
    size = QSize();
    readError = reason;
}

QString MaterialFile::summary() const {
    QStringList parts{name};
    if (!format.isEmpty()) {
        parts << format;
    }
    if (size.isValid()) {
        //: The size of a picture in pixels: %1 the width, %2 the height. In Polish: %1 × %2 px
        parts << tr("%1 × %2 px").arg(size.width()).arg(size.height());
    }
    if (exists) {
        parts << QLocale().formattedDataSize(bytes, bytes < MEGABYTE ? 0 : 1,
                                             QLocale::DataSizeTraditionalFormat);
    }
    return parts.join(QStringLiteral(" · "));
}

QString MaterialFile::problem() const {
    if (!exists) {
        //: In Polish: Pliku nie ma w folderze książki.
        return tr("The file is not in the book's folder.");
    }
    if (!picture || readable) {
        return {};
    }
    // A known suffix, or a format an image plugin found in the beginning of the file
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();
    if (supported.contains(QFileInfo(path).suffix().toLower().toLatin1()) ||
        supported.contains(format.toLower().toLatin1())) {
        //: A picture of a format the program shows, which it cannot read; %1: the reason in
        //: English, from Qt. In Polish: Nie da się odczytać obrazu (%1). Może być uszkodzony.
        return tr("The picture cannot be read (%1). It may be damaged.").arg(readError);
    }
    //: %1: the format, e.g. WEBP. In Polish: Program nie pokazuje obrazów %1. Otwórz plik w
    //: programie systemowym.
    return tr("The program does not show %1 pictures. Open the file in the system program.")
        .arg(format);
}

QImage readPicture(const QString& path, int rotation, const QSize& bounds, QString* error) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // The size of the picture as it is shown, turned by its own metadata and by @p rotation
    QSize shown = reader.size();
    const bool sideways = (reader.transformation() & QImageIOHandler::TransformationRotate90) != 0;
    if (sideways) {
        shown.transpose();
    }
    const bool turned = rotation == 90 || rotation == 270;
    if (turned) {
        shown.transpose();
    }
    // A large picture is read smaller at once, where its plugin can do it; one that its
    // metadata turns is read whole and made smaller after
    QSize wanted;
    if (bounds.isValid() && shown.isValid() &&
        (shown.width() > bounds.width() || shown.height() > bounds.height())) {
        wanted = shown.scaled(bounds, Qt::KeepAspectRatio);
        if (!sideways) {
            reader.setScaledSize(turned ? wanted.transposed() : wanted);
        }
    }
    QImage image = reader.read();
    if (image.isNull()) {
        if (error != nullptr) {
            *error = reader.errorString();
        }
        return image;
    }
    if (rotation != 0) {
        image = image.transformed(QTransform().rotate(rotation), Qt::SmoothTransformation);
    }
    if (wanted.isValid() && image.size() != wanted) {
        image = image.scaled(wanted, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace gui
} // namespace kalahari
