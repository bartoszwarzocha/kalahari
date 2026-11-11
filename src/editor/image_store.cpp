/// @file image_store.cpp
/// @brief The image files of a book: where they are, their sizes and their pictures

#include "kalahari/editor/image_store.h"
#include "kalahari/editor/text_images.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// The most files of one name with numbers the folder of images is searched for
constexpr int MAX_NUMBERED_NAMES = 10000;

/// Whether the file is an SVG drawing (drawn by QSvgRenderer, whatever plugins Qt has)
bool isSvgFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("svg") || suffix == QStringLiteral("svgz");
}

/// Whether the reader turns the picture by a quarter (its EXIF orientation)
bool turnsQuarter(const QImageReader& reader)
{
    return (reader.transformation() & QImageIOHandler::TransformationRotate90) != 0;
}

/// The bytes of a file; empty when it cannot be read
QByteArray fileData(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

}  // namespace

ImageStore::ImageStore()
{
    m_pictures.setMaxCost(PICTURE_CACHE_KB);
}

void ImageStore::setFolder(const QString& folder)
{
    QString cleaned = folder.isEmpty() ? QString() : QDir::cleanPath(folder);
    if (cleaned == m_folder) {
        return;
    }
    m_folder = cleaned;
    m_sizes.clear();
    m_pictures.clear();
}

QString ImageStore::filePath(const QString& source) const
{
    if (m_folder.isEmpty() || !isSafeImageSource(source)) {
        return QString();
    }
    return QDir::cleanPath(m_folder + QLatin1Char('/') +
                           QString(source).replace(QLatin1Char('\\'), QLatin1Char('/')));
}

std::optional<QSize> ImageStore::fileSize(const QString& path)
{
    if (isSvgFile(path)) {
        const QSvgRenderer renderer(path);
        if (renderer.isValid() && !renderer.defaultSize().isEmpty()) {
            return renderer.defaultSize();
        }
        return std::nullopt;
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize read = reader.size();
    if (read.isEmpty() && reader.canRead()) {
        read = reader.read().size();  // a format that tells its size only once read
    }
    if (read.isEmpty()) {
        return std::nullopt;
    }
    if (turnsQuarter(reader)) {
        read.transpose();
    }
    return read;
}

std::optional<QSize> ImageStore::pixelSize(const QString& source) const
{
    const QString path = filePath(source);
    if (path.isEmpty()) {
        return std::nullopt;
    }
    if (const auto known = m_sizes.constFind(path); known != m_sizes.cend()) {
        return *known;
    }
    const std::optional<QSize> size = fileSize(path);
    m_sizes.insert(path, size);
    return size;
}

QImage ImageStore::picture(const QString& source, const QSize& pixels) const
{
    const QString path = filePath(source);
    if (path.isEmpty() || pixels.isEmpty() || !pixelSize(source)) {
        return QImage();
    }
    const QString key = path + QLatin1Char('@') + QString::number(pixels.width()) +
                        QLatin1Char('x') + QString::number(pixels.height());
    if (const QImage* cached = m_pictures.object(key)) {
        return *cached;
    }

    QImage image;
    if (isSvgFile(path)) {
        QSvgRenderer renderer(path);
        image = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(pixels)));
    } else {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize decoded = pixels;
        if (turnsQuarter(reader)) {
            decoded.transpose();  // the size before the picture is turned
        }
        reader.setScaledSize(decoded);
        image = reader.read();
        if (!image.isNull() && image.size() != pixels) {
            image = image.scaled(pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }
    if (image.isNull()) {
        return QImage();
    }
    const auto kilobytes = static_cast<int>(std::max<qsizetype>(1, image.sizeInBytes() / 1024));
    m_pictures.insert(key, new QImage(image), kilobytes);
    return image;
}

QString ImageStore::addFile(const QString& path)
{
    if (m_folder.isEmpty()) {
        return QString();
    }
    const QFileInfo file(path);
    if (!file.isFile()) {
        return QString();
    }
    // A file of the book's folder of images is used as it is
    const QString imagesFolder = m_folder + QLatin1Char('/') + QLatin1String(IMAGES_FOLDER);
    const QFileInfo folder(imagesFolder);
    if (folder.isDir() && file.absoluteDir().canonicalPath() == folder.canonicalFilePath()) {
        return QLatin1String(IMAGES_FOLDER) + QLatin1Char('/') + file.fileName();
    }
    const QByteArray data = fileData(file.absoluteFilePath());
    if (data.isEmpty()) {
        return QString();
    }
    return storeData(data, file.fileName());
}

QString ImageStore::addPicture(const QImage& picture, const QString& baseName)
{
    if (m_folder.isEmpty() || picture.isNull()) {
        return QString();
    }
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    if (!picture.save(&buffer, "PNG")) {
        return QString();
    }
    return storeData(data, baseName + QStringLiteral(".png"));
}

QString ImageStore::adoptSource(const QString& otherFolder, const QString& source)
{
    if (m_folder.isEmpty() || otherFolder.isEmpty() || !isSafeImageSource(source)) {
        return source;
    }
    const QString other = QDir::cleanPath(otherFolder);
    if (QFileInfo(other).canonicalFilePath() == QFileInfo(m_folder).canonicalFilePath() ||
        other == m_folder) {
        return source;  // the same book
    }
    const QString otherPath = QDir::cleanPath(
        other + QLatin1Char('/') + QString(source).replace(QLatin1Char('\\'), QLatin1Char('/')));
    if (!QFileInfo(otherPath).isFile()) {
        return source;  // missing there too: it stays missing
    }
    const QString adopted = addFile(otherPath);
    return adopted.isEmpty() ? source : adopted;
}

void ImageStore::forget(const QString& source)
{
    const QString path = filePath(source);
    if (path.isEmpty()) {
        return;
    }
    m_sizes.remove(path);
    const QString prefix = path + QLatin1Char('@');
    for (const QString& key : m_pictures.keys()) {
        if (key.startsWith(prefix)) {
            m_pictures.remove(key);
        }
    }
}

QString ImageStore::storeData(const QByteArray& data, const QString& name)
{
    const QString imagesFolder = m_folder + QLatin1Char('/') + QLatin1String(IMAGES_FOLDER);
    if (!QDir().mkpath(imagesFolder)) {
        return QString();
    }
    const QFileInfo named(name);
    const QString base = named.completeBaseName().isEmpty() ? QStringLiteral("image")
                                                            : named.completeBaseName();
    const QString suffix = named.suffix().isEmpty() ? QString()
                                                    : QLatin1Char('.') + named.suffix();
    for (int number = 1; number <= MAX_NUMBERED_NAMES; ++number) {
        const QString fileName =
            number == 1 ? base + suffix
                        : base + QLatin1Char('-') + QString::number(number) + suffix;
        const QString target = imagesFolder + QLatin1Char('/') + fileName;
        const QString source = QLatin1String(IMAGES_FOLDER) + QLatin1Char('/') + fileName;
        const QFileInfo existing(target);
        if (existing.exists()) {
            if (existing.isFile() && existing.size() == data.size() && fileData(target) == data) {
                return source;  // the same picture, added before
            }
            continue;
        }
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            file.remove();
            return QString();
        }
        file.close();
        forget(source);
        return source;
    }
    return QString();
}

}  // namespace kalahari::editor
