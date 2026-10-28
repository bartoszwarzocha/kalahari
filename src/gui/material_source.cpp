/// @file material_source.cpp
/// @brief Where a material comes from, and what comes to the Navigator from outside the program

#include "kalahari/gui/material_source.h"

#include "kalahari/core/material_kinds.h"

#include <QFileInfo>
#include <QLocale>
#include <QMimeData>

namespace kalahari {
namespace gui {

namespace {

/// Longest name of the copy of a picture, before its suffix
constexpr qsizetype MAXIMUM_NAME = 100;

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

MaterialSource materialSourceOf(const core::ProjectElement& element) {
    if (!element.url.isEmpty()) {
        return core::MaterialKinds::ofSource(QUrl(element.url), element.file) ==
                       QLatin1String(core::MaterialKinds::PICTURE)
                   ? MaterialSource::WebPicture
                   : MaterialSource::Page;
    }
    return element.file.isEmpty() ? MaterialSource::None : MaterialSource::File;
}

QUrl webAddress(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.contains(QLatin1Char('\n')) || trimmed.contains(QLatin1Char(' '))) {
        return {};
    }
    QUrl url(trimmed, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() ||
        (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        return {};
    }
    return url;
}

QString titleOfAddress(const QUrl& url) {
    QString title = lastPart(url);
    if (core::MaterialKinds::isPictureAddress(url)) {
        title.truncate(title.lastIndexOf(QLatin1Char('.')));
    }
    title.replace(QLatin1Char('_'), QLatin1Char(' '));
    title = title.simplified();
    if (title.isEmpty()) {
        title = url.host();
        if (title.startsWith(QLatin1String("www."))) {
            title.remove(0, 4);
        }
    }
    return title;
}

QString fileNameOfAddress(const QUrl& url, const QString& suffix) {
    QString name = lastPart(url);
    QString ownSuffix = suffixOf(name);
    if (!ownSuffix.isEmpty()) {
        name.truncate(name.size() - ownSuffix.size() - 1);
    }
    // The characters no file name can have on one of the systems
    static const QString forbidden = QStringLiteral("<>:\"/\\|?*");
    for (QChar& character : name) {
        if (character.unicode() < 0x20 || forbidden.contains(character)) {
            character = QLatin1Char('_');
        }
    }
    name = name.trimmed();
    while (name.endsWith(QLatin1Char('.'))) {
        name.chop(1);
    }
    name.truncate(MAXIMUM_NAME);
    if (name.isEmpty()) {
        name = QStringLiteral("picture");
    }
    const QString chosen = suffix.isEmpty() ? ownSuffix : suffix.toLower();
    return chosen.isEmpty() ? name : name + QLatin1Char('.') + chosen;
}

IncomingMaterials IncomingMaterials::of(const QMimeData* data) {
    IncomingMaterials incoming;
    if (data == nullptr) {
        return incoming;
    }
    for (const QUrl& url : data->urls()) {
        if (url.isLocalFile()) {
            const QFileInfo file(url.toLocalFile());
            if (file.isFile()) {
                incoming.files << file.absoluteFilePath();
            }
        } else if (const QUrl address = webAddress(url.toString()); !address.isEmpty()) {
            incoming.addresses << address;
        }
    }
    if (incoming.hasMaterials()) {
        return incoming;
    }
    if (data->hasImage()) {
        incoming.picture = qvariant_cast<QImage>(data->imageData());
        if (!incoming.picture.isNull()) {
            return incoming;
        }
    }
    QString text = data->text().trimmed();
    if (const QUrl address = webAddress(text); !address.isEmpty()) {
        incoming.addresses << address;
        return incoming;
    }
    // A path copied as text, in quotes
    if (text.size() > 1 && text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"'))) {
        text = text.mid(1, text.size() - 2);
    }
    const QFileInfo file(text);
    if (!text.isEmpty() && !text.contains(QLatin1Char('\n')) && file.isAbsolute() &&
        file.isFile()) {
        incoming.files << file.absoluteFilePath();
        return incoming;
    }
    incoming.text = data->text();
    return incoming;
}

QString IncomingMaterials::pictureTitle(const QDateTime& time) {
    //: The title of a material from a picture on the clipboard, e.g. a screenshot; %1: when it
    //: was pasted. In Polish: Obraz ze schowka %1
    return tr("Picture from the clipboard %1").arg(QLocale().toString(time, QLocale::ShortFormat));
}

QString IncomingMaterials::pictureFileName(const QDateTime& time) {
    return QStringLiteral("clipboard_%1.png").arg(time.toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

} // namespace gui
} // namespace kalahari
