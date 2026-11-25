// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file volume_copies.cpp
/// @brief A volume of a series copied out as a book of its own, a book copied into a series as
///        a volume, and the files that go with them

#include <kalahari/core/volume_copies.h>

#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/card_relations.h>
#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/volume_materials.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <utility>

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

/// A path for @p wanted that @p taken does not have, nor the disk (@p onDisk):
/// "<name>_2.<suffix>", "_3"...
QString freePath(const QString& wanted, QSet<QString>& taken,
                 const std::function<bool(const QString&)>& onDisk = {}) {
    QString path = wanted;
    const QFileInfo info(wanted);
    const QString folder = info.path() == QLatin1String(".") ? QString() : info.path() + u'/';
    const auto used = [&taken, &onDisk](const QString& each) {
        return taken.contains(keyOf(each)) || (onDisk && onDisk(each));
    };
    for (int number = 2; used(path); ++number) {
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

/// The files of @p book, whose folder was @p folder, go to the folder @p targetFolder: those
/// of @p folder keep their names, the others get free ones, which @p taken then has
QList<ProjectCopy> moveBookFiles(ProjectBook& book, const QString& folder,
                                 const QString& targetFolder, QSet<QString>& taken) {
    QList<ProjectElement*> elements;
    const std::function<void(ProjectElement&)> collect = [&elements](ProjectElement& element) {
        if (!element.file.isEmpty()) {
            elements << &element;
        }
    };
    for (QList<ProjectElement>* list :
         {&book.frontElements, &book.mainElements, &book.backElements, &book.files}) {
        forEachIn(*list, collect);
    }
    const QString prefix = folder + u'/';
    const QString target = targetFolder + u'/';
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

/// Copy the files of folder @p from, and of its folders, to @p to, those @p written lists;
/// a file that @p to has already stays
bool copyFolder(const QString& from, const QString& to, QStringList& problems,
                QStringList& written) {
    QDirIterator files(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    const QDir source(from);
    while (files.hasNext()) {
        const QString file = files.next();
        const QString target = QDir(to).filePath(source.relativeFilePath(file));
        if (QFileInfo::exists(target)) {
            continue;
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(file, target)) {
            problems.append(QStringLiteral("%1: the file cannot be copied")
                                .arg(QDir::toNativeSeparators(file)));
            return false;
        }
        written << target;
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

/// Copy file @p copy from the project in @p fromDir to that in @p toDir, with its versions,
/// those @p written lists
bool copyFile(const QString& fromDir, const QString& toDir, const ProjectCopy& copy,
              QStringList& problems, QStringList& written) {
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
    written << to;
    const QString versions = ChapterVersions::folderOf(fromDir, copy.from);
    const QString target = ChapterVersions::folderOf(toDir, copy.to);
    if (versions.isEmpty() || target.isEmpty() || !QFileInfo(versions).isDir()) {
        return true;
    }
    if (!copyFolder(versions, target, problems, written)) {
        return false;
    }
    if (!renameVersions(target, copy.to)) {
        problems.append(QStringLiteral("%1: the versions cannot be written")
                            .arg(QDir::toNativeSeparators(target)));
        return false;
    }
    return true;
}

// =============================================================================
// A book joined to a series
// =============================================================================

/// The elements of @p elements and of their groups, a group before its elements
QList<const ProjectElement*> allIn(const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> all;
    QList<const ProjectElement*> waiting;
    const auto wait = [&waiting](const QList<ProjectElement>& list) {
        for (const ProjectElement& element : std::views::reverse(list)) {
            waiting << &element;
        }
    };
    wait(elements);
    while (!waiting.isEmpty()) {
        const ProjectElement* element = waiting.takeLast();
        all << element;
        wait(element->elements);
    }
    return all;
}

/// The elements of the books of @p project: of their texts and their files outside them
QList<const ProjectElement*> bookElementsOf(const BookProject& project) {
    QList<const ProjectElement*> all;
    for (const ProjectBook& book : project.books) {
        for (const QList<ProjectElement>* list :
             {&book.frontElements, &book.mainElements, &book.backElements, &book.files}) {
            all << allIn(*list);
        }
    }
    return all;
}

/// The files of the elements of @p project, as keyOf() gives them
QSet<QString> filesOf(const BookProject& project) {
    QSet<QString> files;
    QList<const ProjectElement*> elements = bookElementsOf(project);
    elements << allIn(project.workshop.elements);
    for (const ProjectElement* element : std::as_const(elements)) {
        if (!element->file.isEmpty()) {
            files.insert(keyOf(element->file));
        }
    }
    return files;
}

/// The ids of the series that the ids of a book joined to it become, and its volume
struct JoinIds {
    QHash<QString, QString> elements;  ///< The cards of the book that are cards of the series
    QHash<QString, QString> roles;     ///< The relations of the book: those of the series
    QString volume;                    ///< The id of the new volume

    /// The id that element @p id of the book has in the series
    [[nodiscard]] QString element(const QString& id) const { return elements.value(id, id); }
};

/// The links, portraits and values of the fields of @p element name what @p ids gives, each
/// element once
void remap(ProjectElement& element, const JoinIds& ids) {
    QList<ElementLink> links;
    for (ElementLink link : std::as_const(element.links)) {
        link.to = ids.element(link.to);
        link.role = ids.roles.value(link.role, link.role);
        if (std::none_of(links.cbegin(), links.cend(),
                         [&link](const ElementLink& each) { return each.to == link.to; })) {
            links << link;
        }
    }
    element.links = links;
    for (CardPortrait& portrait : element.card.portraits) {
        portrait.image = ids.element(portrait.image);
    }
    // A field of the type "card" names a card
    for (QString& value : element.card.values) {
        value = ids.element(value);
    }
}

/// @p own of the series gains the links of @p element of the book that it does not have
void gainLinks(ProjectElement& own, const ProjectElement& element, const JoinIds& ids) {
    ProjectElement linked;
    linked.links = element.links;
    remap(linked, ids);
    for (const ElementLink& link : std::as_const(linked.links)) {
        if (link.to != own.id && own.linkTo(link.to) == nullptr) {
            own.links << link;
        }
    }
}

/// The elements of @p workshop, the Workshop of a book, join the Workshop of @p project: an
/// element of the series (its id, or the card of a pair) gains the links of the book's one,
/// and the elements inside the book's one join those inside it; the others come at the end
/// of their lists, with the ids of the series in their links
/// @return The ids of those that came
QSet<QString> joinWorkshop(BookProject& project, const QList<ProjectElement>& workshop,
                           const JoinIds& ids) {
    QSet<QString> added;
    // The lists of the book still to join, each with the id of the element of the series whose
    // list it joins (empty: the Workshop's own): ids, as a list of the series can move
    QList<std::pair<const QList<ProjectElement>*, QString>> lists{{&workshop, QString()}};
    while (!lists.isEmpty()) {
        const auto [from, parentId] = lists.takeLast();
        ProjectElement* parent = parentId.isEmpty() ? nullptr : project.findElement(parentId);
        QList<ProjectElement>& into = parent != nullptr ? parent->elements
                                                         : project.workshop.elements;
        for (const ProjectElement& element : *from) {
            if (ProjectElement* own = project.findElement(ids.element(element.id))) {
                gainLinks(*own, element, ids);
                lists.append({&element.elements, own->id});
                continue;
            }
            ProjectElement copy = element;
            copy.elements.clear();
            remap(copy, ids);
            if (VolumeMaterials::isVolumeMaterial(copy)) {
                copy.bookId = ids.volume;
            }
            added.insert(copy.id);
            into.append(copy);
            lists.append({&element.elements, copy.id});
        }
    }
    return added;
}

/// The relations between cards of @p relations, those of a book, join @p into, those of the
/// series: a relation of the same kinds and names is the series' one
/// @return The ids of the relations of the book, and the ids they have in the series
QHash<QString, QString> joinRelations(QList<OwnRelation>& into,
                                      const QList<OwnRelation>& relations) {
    QHash<QString, QString> roles;
    for (const OwnRelation& relation : relations) {
        const auto inverse = std::find_if(
            relations.cbegin(), relations.cend(),
            [&relation](const OwnRelation& each) { return each.id == relation.inverse; });
        const QString inverseName = inverse != relations.cend() ? inverse->name : relation.name;
        roles.insert(relation.id, card_links::ownRelation(into, relation.kind, relation.otherKind,
                                                          relation.name, inverseName));
    }
    return roles;
}

/// The paragraph styles, the fields of the cards and the kinds of @p book that @p project does
/// not have go to it
void joinSettings(BookProject& project, const BookProject& book) {
    for (const BookParagraphStyle& style : book.paragraphStyles) {
        if (std::none_of(project.paragraphStyles.cbegin(), project.paragraphStyles.cend(),
                         [&style](const BookParagraphStyle& own) { return own.id == style.id; })) {
            project.paragraphStyles << style;
        }
    }
    for (auto fields = book.workshop.cardFields.cbegin();
         fields != book.workshop.cardFields.cend(); ++fields) {
        if (!project.workshop.cardFields.contains(fields.key())) {
            project.workshop.cardFields.insert(fields.key(), fields.value());
        }
    }
    for (const KindReference& kind : book.kinds) {
        if (!project.kinds.contains(kind)) {
            project.kinds << kind;
        }
    }
}

/// The files of the elements of the Workshop that came (@p added) get free names among
/// @p taken and the files of the folder of the series (@p onDisk)
QList<ProjectCopy> moveWorkshopFiles(BookProject& project, const QSet<QString>& added,
                                     QSet<QString>& taken,
                                     const std::function<bool(const QString&)>& onDisk) {
    QList<ProjectCopy> copies;
    forEachIn(project.workshop.elements,
              [&added, &taken, &onDisk, &copies](ProjectElement& element) {
                  if (added.contains(element.id) && !element.file.isEmpty()) {
                      const QString to = freePath(element.file, taken, onDisk);
                      copies.append({element.file, to});
                      element.file = to;
                  }
              });
    return copies;
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

    QSet<QString> files;
    taken.files = moveBookFiles(project.books.first(), folder,
                                QString::fromLatin1(BookProject::BOOK_FOLDER), files);
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
                        QStringList& problems, QStringList* written) {
    QStringList made;
    const auto copyOne = [&fromDir, &toDir, &problems, &made](const ProjectCopy& file) {
        return copyFile(fromDir, toDir, file, problems, made);
    };
    const auto copyAll = [&fromDir, &toDir, &problems, &made](const ProjectCopy& folder) {
        const QString from = QDir(fromDir).filePath(folder.from);
        return !QFileInfo(from).isDir() ||
               copyFolder(from, QDir(toDir).filePath(folder.to), problems, made);
    };
    const bool copied = std::all_of(files.cbegin(), files.cend(), copyOne) &&
                        std::all_of(folders.cbegin(), folders.cend(), copyAll);
    if (written != nullptr) {
        written->append(made);
    }
    return copied;
}

JoinProblem VolumeCopies::joinProblem(const BookProject& series, const BookProject& book) {
    if (book.books.size() != 1) {
        return JoinProblem::Series;
    }
    if (series.type.has_value() != book.type.has_value() ||
        (series.type && series.type->id != book.type->id)) {
        return JoinProblem::OtherType;
    }
    const QList<const ProjectElement*> texts = bookElementsOf(book);
    const bool shared =
        series.bookIndex(book.books.first().id) >= 0 ||
        std::any_of(texts.cbegin(), texts.cend(), [&series](const ProjectElement* element) {
            return series.findElement(element->id) != nullptr;
        });
    return shared ? JoinProblem::VolumeOfSeries : JoinProblem::None;
}

QList<CardPair> VolumeCopies::sameCards(const BookProject& series, const BookProject& book,
                                        const std::function<bool(const ProjectElement&)>& isCard) {
    // The cards of the series that are no card of the book already
    QList<const ProjectElement*> free;
    for (const ProjectElement* own : allIn(series.workshop.elements)) {
        if (isCard(*own) && book.findElement(own->id) == nullptr) {
            free << own;
        }
    }
    QList<CardPair> pairs;
    for (const ProjectElement* card : allIn(book.workshop.elements)) {
        if (!isCard(*card) || series.findElement(card->id) != nullptr) {
            continue;
        }
        const QString title = card->title.trimmed();
        const auto same = std::find_if(free.begin(), free.end(), [card, &title](const auto* own) {
            return own->kind == card->kind &&
                   own->title.trimmed().compare(title, Qt::CaseInsensitive) == 0;
        });
        if (same != free.end()) {
            pairs.append({card->id, (*same)->id});
            free.erase(same);
        }
    }
    return pairs;
}

std::optional<JoinedVolume> VolumeCopies::join(const BookProject& series, const BookProject& book,
                                               const JoinRequest& request) {
    if (joinProblem(series, book) != JoinProblem::None) {
        return std::nullopt;
    }
    JoinedVolume joined;
    BookProject& project = joined.project;
    project = series;
    ProjectBook volume = book.books.first();
    const QString folder = volume.folder;
    volume.folder = request.folder;
    joined.bookId = volume.id;

    JoinIds ids;
    ids.volume = volume.id;
    for (const CardPair& pair : request.sameCards) {
        ids.elements.insert(pair.card, pair.seriesCard);
    }
    joined.ids = ids.elements;
    ids.roles = joinRelations(project.workshop.relations, book.workshop.relations);

    // A book that becomes a series gives its materials of the cover and its texts for
    // publishers to its first volume, then the Workshop of the book joins
    VolumeMaterials::toFirstVolume(project);
    const QSet<QString> added = joinWorkshop(project, book.workshop.elements, ids);
    joinSettings(project, book);

    // The volume names the cards of the series
    for (QList<ProjectElement>* list :
         {&volume.frontElements, &volume.mainElements, &volume.backElements, &volume.files}) {
        forEachIn(*list, [&ids](ProjectElement& element) { remap(element, ids); });
    }
    if (volume.publishingSet) {
        for (QString& pick : volume.publishingSet->picks) {
            pick = ids.element(pick);
        }
    }

    // Its files in its folder, those of the Workshop under free names
    QSet<QString> taken = filesOf(series);
    joined.files = moveBookFiles(volume, folder, request.folder, taken);
    joined.files << moveWorkshopFiles(project, added, taken, request.onDisk);
    const QString images = QStringLiteral("/images");
    const QString types = QString::fromLatin1(BookProject::TYPES_FOLDER);
    joined.folders = {{folder + images, request.folder + images}, {types, types}};

    const qsizetype at = request.index < 0 ? project.books.size()
                                           : std::min(request.index, project.books.size());
    project.books.insert(at, volume);
    return joined;
}

void VolumeCopies::joinDictionary(BookDictionary& into, const BookDictionary& other,
                                  const QHash<QString, QString>& cards) {
    for (DictionaryName name : other.names()) {
        name.card = cards.value(name.card, name.card);
        const bool known = into.findName(name.id) != nullptr ||
                           (!name.card.isEmpty() && into.findNameOfCard(name.card) != nullptr) ||
                           into.findNameWithText(name.text()) != nullptr;
        if (!known) {
            into.putName(name);
        }
    }
    for (const SpellingRule& rule : other.rules()) {
        if (into.findRule(rule.id) == nullptr) {
            into.putRule(rule);
        }
    }
    for (const QString& word : other.words()) {
        into.addWord(word);
    }
}

}  // namespace kalahari::core
