// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file volume_copies.cpp
/// @brief A volume of a series copied out as a book of its own, and the files that go with it

#include <kalahari/core/volume_copies.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/volume_materials.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>

namespace kalahari::core {

namespace {

/// Each element of @p elements and of its groups, a group before its elements; @p visit may
/// change the element, but not the lists it is in
void forEachIn(QList<ProjectElement>& elements,
               const std::function<void(ProjectElement&)>& visit) {
    // Depth first, in their order: the elements still to visit, the next one last
    QList<ProjectElement*> waiting;
    const auto wait = [&waiting](QList<ProjectElement>& list) {
        for (ProjectElement& element : std::views::reverse(list)) {
            waiting << &element;
        }
    };
    wait(elements);
    while (!waiting.isEmpty()) {
        ProjectElement* element = waiting.takeLast();
        visit(*element);
        wait(element->elements);
    }
}

/// Each element of the book of @p project, of its files outside its text and of the Workshop
void forEachOf(BookProject& project, const std::function<void(ProjectElement&)>& visit) {
    for (ProjectBook& book : project.books) {
        forEachIn(book.frontElements, visit);
        forEachIn(book.mainElements, visit);
        forEachIn(book.backElements, visit);
        forEachIn(book.files, visit);
    }
    forEachIn(project.workshop.elements, visit);
}

/// @p path as the file system tells files apart: without the case of the letters
QString keyOf(const QString& path) {
    return path.toLower();
}

/// A path for @p wanted that @p taken does not have: "<name>_2.<suffix>", "_3"...
QString freePath(const QString& wanted, QSet<QString>& taken) {
    QString path = wanted;
    const QFileInfo info(wanted);
    const QString folder = info.path() == QLatin1String(".") ? QString() : info.path() + u'/';
    for (int number = 2; taken.contains(keyOf(path)); ++number) {
        path = folder + info.completeBaseName() + u'_' + QString::number(number) +
               (info.suffix().isEmpty() ? QString() : u'.' + info.suffix());
    }
    taken.insert(keyOf(path));
    return path;
}

/// The materials of the Workshop of @p series for another volume than @p bookId leave
/// @p elements, and the others are the book's own; the ids of those that left go to @p gone
void keepVolumeMaterials(const BookProject& series, const QString& bookId,
                         QList<ProjectElement>& elements, QSet<QString>& gone) {
    const auto other = [&series, &bookId, &gone](const ProjectElement& element) {
        const QString volume = VolumeMaterials::volumeOf(series, element);
        if (volume.isEmpty() || volume == bookId) {
            return false;
        }
        gone.insert(element.id);
        return true;
    };
    // The lists still to look at: those of the groups that stay
    QList<QList<ProjectElement>*> lists{&elements};
    while (!lists.isEmpty()) {
        QList<ProjectElement>* list = lists.takeLast();
        list->removeIf(other);
        for (ProjectElement& element : *list) {
            element.bookId.clear();
            lists << &element.elements;
        }
    }
}

/// The files of the book of @p project, which was the folder @p folder of the series, go to
/// BookProject::BOOK_FOLDER: those of @p folder keep their names, the others get free ones
QList<ProjectCopy> moveBookFiles(BookProject& project, const QString& folder) {
    QList<ProjectElement*> elements;
    const std::function<void(ProjectElement&)> collect = [&elements](ProjectElement& element) {
        if (!element.file.isEmpty()) {
            elements << &element;
        }
    };
    ProjectBook& book = project.books.first();
    for (QList<ProjectElement>* list :
         {&book.frontElements, &book.mainElements, &book.backElements, &book.files}) {
        forEachIn(*list, collect);
    }
    const QString prefix = folder + u'/';
    const QString target = QString::fromLatin1(BookProject::BOOK_FOLDER) + u'/';
    QSet<QString> taken;
    QList<ProjectCopy> copies;
    // Its own files first, so that they keep their names
    std::stable_partition(elements.begin(), elements.end(),
                          [&prefix](const ProjectElement* element) {
                              return element->file.startsWith(prefix);
                          });
    for (ProjectElement* element : elements) {
        const bool own = element->file.startsWith(prefix);
        const QString rest =
            own ? element->file.mid(prefix.size()) : QFileInfo(element->file).fileName();
        const QString to = freePath(target + rest, taken);
        copies.append({element->file, to});
        element->file = to;
    }
    return copies;
}

/// The links and portraits of the elements of @p project that name what it does not have
/// leave them
void dropLostLinks(BookProject& project) {
    const BookProject& shown = project;
    forEachOf(project, [&shown](ProjectElement& element) {
        element.links.removeIf(
            [&shown](const ElementLink& link) { return !shown.hasLinkTarget(link.to); });
        element.card.portraits.removeIf([&shown](const CardPortrait& portrait) {
            return !portrait.image.isEmpty() && shown.findElement(portrait.image) == nullptr;
        });
    });
}

/// Copy the files of folder @p from, and of its folders, to @p to
bool copyFolder(const QString& from, const QString& to, QStringList& problems) {
    QDirIterator files(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    const QDir source(from);
    while (files.hasNext()) {
        const QString file = files.next();
        const QString target = QDir(to).filePath(source.relativeFilePath(file));
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(file, target)) {
            problems.append(QStringLiteral("%1: the file cannot be copied")
                                .arg(QDir::toNativeSeparators(file)));
            return false;
        }
    }
    return true;
}

/// The versions of a file copied to the folder @p folder name the file @p file
bool renameVersions(const QString& folder, const QString& file) {
    const QString indexPath =
        QDir(folder).filePath(QString::fromLatin1(ChapterVersions::INDEX_FILE));
    QFile index(indexPath);
    if (!index.exists()) {
        return true;
    }
    if (!index.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonObject root = QJsonDocument::fromJson(index.readAll()).object();
    index.close();
    root.insert(QStringLiteral("file"), file);
    QSaveFile saved(indexPath);
    return saved.open(QIODevice::WriteOnly) &&
           saved.write(QJsonDocument(root).toJson()) >= 0 && saved.commit();
}

/// Copy file @p copy from the project in @p fromDir to that in @p toDir, with its versions
bool copyFile(const QString& fromDir, const QString& toDir, const ProjectCopy& copy,
              QStringList& problems) {
    const QString from = QDir(fromDir).filePath(copy.from);
    const QString to = QDir(toDir).filePath(copy.to);
    if (!QFileInfo::exists(from)) {
        return true;  // a text never saved has no file yet, and nothing to copy
    }
    if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QFile::copy(from, to)) {
        problems.append(QStringLiteral("%1: the file cannot be copied")
                            .arg(QDir::toNativeSeparators(from)));
        return false;
    }
    const QString versions = ChapterVersions::folderOf(fromDir, copy.from);
    const QString target = ChapterVersions::folderOf(toDir, copy.to);
    if (versions.isEmpty() || target.isEmpty() || !QFileInfo(versions).isDir()) {
        return true;
    }
    if (!copyFolder(versions, target, problems)) {
        return false;
    }
    if (!renameVersions(target, copy.to)) {
        problems.append(QStringLiteral("%1: the versions cannot be written")
                            .arg(QDir::toNativeSeparators(target)));
        return false;
    }
    return true;
}

}  // namespace

std::optional<TakenOutVolume> VolumeCopies::takeOut(const BookProject& series,
                                                    const QString& bookId) {
    const qsizetype index = series.bookIndex(bookId);
    if (index < 0 || series.books.size() < 2) {
        return std::nullopt;
    }
    TakenOutVolume taken;
    BookProject& project = taken.project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.type = series.type;
    project.kinds = series.kinds;
    project.paragraphStyles = series.paragraphStyles;
    project.extra = series.extra;
    project.workshop = series.workshop;
    project.books.append(series.books.at(index));
    const QString folder = series.books.at(index).folder;
    project.books.first().folder = QString::fromLatin1(BookProject::BOOK_FOLDER);

    // The Workshop without what is for the other volumes
    QSet<QString> gone;
    keepVolumeMaterials(series, bookId, project.workshop.elements, gone);
    dropLostLinks(project);
    if (ProjectBook& book = project.books.first(); book.publishingSet) {
        book.publishingSet->picks.removeIf([&gone](QMap<QString, QString>::iterator pick) {
            return gone.contains(pick.value());
        });
    }

    taken.files = moveBookFiles(project, folder);
    forEachIn(project.workshop.elements, [&taken](ProjectElement& element) {
        if (!element.file.isEmpty()) {
            taken.files.append({element.file, element.file});
        }
    });
    // The images of its texts (editor::IMAGES_FOLDER, in the folder of the book)
    const QString images = QStringLiteral("/images");
    taken.folders.append({folder + images, QString::fromLatin1(BookProject::BOOK_FOLDER) + images});
    return taken;
}

bool VolumeCopies::copy(const QString& fromDir, const QString& toDir,
                        const QList<ProjectCopy>& files, const QList<ProjectCopy>& folders,
                        QStringList& problems) {
    for (const ProjectCopy& file : files) {
        if (!copyFile(fromDir, toDir, file, problems)) {
            return false;
        }
    }
    for (const ProjectCopy& folder : folders) {
        const QString from = QDir(fromDir).filePath(folder.from);
        if (QFileInfo(from).isDir() &&
            !copyFolder(from, QDir(toDir).filePath(folder.to), problems)) {
            return false;
        }
    }
    return true;
}

}  // namespace kalahari::core
