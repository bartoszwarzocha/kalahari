/// @file material_kinds.cpp
/// @brief The kinds of materials of the base package, and the kind that the source of a
/// material gives it

#include "kalahari/core/material_kinds.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSet>

namespace kalahari {
namespace core {

namespace {

/// The suffixes of pictures, in small letters
const QSet<QString>& pictureSuffixes() {
    static const QSet<QString> suffixes{
        QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("jpe"),
        QStringLiteral("jfif"), QStringLiteral("png"),  QStringLiteral("gif"),
        QStringLiteral("webp"), QStringLiteral("svg"),  QStringLiteral("bmp"),
        QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("ico"),
        QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("heif")};
    return suffixes;
}

/// The types of files with a text: PDF, Word, OpenDocument, RTF, plain text, Markdown,
/// spreadsheets, presentations and e-books. Named one by one: a chapter file and a program's
/// source are kinds of plain text too, and are no documents.
const QSet<QString>& documentTypes() {
    static const QSet<QString> types{
        QStringLiteral("application/pdf"),
        QStringLiteral("text/plain"),
        QStringLiteral("text/markdown"),
        QStringLiteral("text/csv"),
        QStringLiteral("application/rtf"),
        QStringLiteral("text/rtf"),
        QStringLiteral("application/msword"),
        QStringLiteral("application/vnd.ms-excel"),
        QStringLiteral("application/vnd.ms-powerpoint"),
        QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"),
        QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
        QStringLiteral(
            "application/vnd.openxmlformats-officedocument.presentationml.presentation"),
        QStringLiteral("application/vnd.oasis.opendocument.text"),
        QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"),
        QStringLiteral("application/vnd.oasis.opendocument.presentation"),
        QStringLiteral("application/epub+zip")};
    return types;
}

/// The suffixes of the documents of documentTypes(), in small letters: the database of the
/// types that Qt brings on Windows and macOS does not take notes.md for Markdown
const QSet<QString>& documentSuffixes() {
    static const QSet<QString> suffixes{
        QStringLiteral("pdf"),  QStringLiteral("txt"),      QStringLiteral("md"),
        QStringLiteral("mkd"),  QStringLiteral("markdown"), QStringLiteral("csv"),
        QStringLiteral("rtf"),  QStringLiteral("doc"),      QStringLiteral("docx"),
        QStringLiteral("xls"),  QStringLiteral("xlsx"),     QStringLiteral("ppt"),
        QStringLiteral("pptx"), QStringLiteral("odt"),      QStringLiteral("ods"),
        QStringLiteral("odp"),  QStringLiteral("epub")};
    return suffixes;
}

/// The last part of the path of @p url that is not empty, decoded
QString lastPart(const QUrl& url) {
    const QStringList parts =
        url.path(QUrl::FullyDecoded).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.constLast();
}

/// The suffix of @p name in small letters; empty without one
QString suffixOf(const QString& name) {
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? name.mid(dot + 1).toLower() : QString();
}

} // namespace

const QStringList& MaterialKinds::all() {
    static const QStringList kinds{
        QString::fromLatin1(QUOTE),       QString::fromLatin1(WEB_PAGE),
        QString::fromLatin1(PICTURE),     QString::fromLatin1(DOCUMENT),
        QString::fromLatin1(PUBLICATION), QString::fromLatin1(RECORDING),
        QString::fromLatin1(OTHER_FILE)};
    return kinds;
}

QString MaterialKinds::ofFile(const QString& path) {
    const QMimeType type = QMimeDatabase().mimeTypeForFile(path);
    const QString name = type.name();
    if (name.startsWith(QLatin1String("image/"))) {
        return QString::fromLatin1(PICTURE);
    }
    if (name.startsWith(QLatin1String("audio/")) || name.startsWith(QLatin1String("video/"))) {
        return QString::fromLatin1(RECORDING);
    }
    const QSet<QString>& documents = documentTypes();
    if (documents.contains(name)) {
        return QString::fromLatin1(DOCUMENT);
    }
    for (const QString& alias : type.aliases()) {
        if (documents.contains(alias)) {
            return QString::fromLatin1(DOCUMENT);
        }
    }
    if (documentSuffixes().contains(suffixOf(QFileInfo(path).fileName()))) {
        return QString::fromLatin1(DOCUMENT);
    }
    return QString::fromLatin1(OTHER_FILE);
}

QString MaterialKinds::ofSource(const QUrl& url, const QString& path) {
    if (!url.isEmpty()) {
        return !path.isEmpty() || isPictureAddress(url) ? QString::fromLatin1(PICTURE)
                                                        : QString::fromLatin1(WEB_PAGE);
    }
    return path.isEmpty() ? QString() : ofFile(path);
}

bool MaterialKinds::isPictureName(const QString& name) {
    return pictureSuffixes().contains(suffixOf(name));
}

bool MaterialKinds::isPictureAddress(const QUrl& url) {
    const QString last = lastPart(url);
    return !last.contains(QLatin1Char(':')) && isPictureName(last);
}

} // namespace core
} // namespace kalahari
