// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_join.cpp
/// @brief A series: a volume taken out as a book of its own, with what it keeps of the Workshop,
///        its files, their versions and its goals; a book joined as a volume, its Workshop
///        joined to the series'

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/volume_copies.h>
#include <kalahari/core/writing_goals.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

using kalahari::core::BookDictionary;
using kalahari::core::BookGoals;
using kalahari::core::BookPlace;
using kalahari::core::BookProject;
using kalahari::core::CardPair;
using kalahari::core::CardPortrait;
using kalahari::core::ChapterDocument;
using kalahari::core::ChapterVersions;
using kalahari::core::CoverMaterial;
using kalahari::core::DictionaryName;
using kalahari::core::ElementLink;
using kalahari::core::ElementPlace;
using kalahari::core::JoinProblem;
using kalahari::core::JoinRequest;
using kalahari::core::KindReference;
using kalahari::core::KindRef;
using kalahari::core::NewBook;
using kalahari::core::NewMaterial;
using kalahari::core::OwnRelation;
using kalahari::core::ProjectBook;
using kalahari::core::ProjectDatabase;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;
using kalahari::core::VersionOrigin;
using kalahari::core::VolumeCopies;

namespace {

ProjectManager& projects() {
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    return pm;
}

/// Kind @p kindId of the base package
KindRef base(const ProjectManager& pm, const char* kindId) {
    return pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
}

/// A description of the back cover, in the current volume, chosen
QString addBlurb(ProjectManager& pm, const char* title) {
    NewMaterial blurb;
    blurb.title = QString::fromUtf8(title);
    blurb.description = QString::fromUtf8(title);
    blurb.cover.part = QString::fromLatin1(CoverMaterial::BLURB);
    blurb.cover.place = QString::fromLatin1(CoverMaterial::BACK);
    const QString id = pm.addMaterial(blurb, base(pm, "cover"));
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(pm.setCoverState(id, QString::fromLatin1(CoverMaterial::CHOSEN)));
    return id;
}

/// The element @p id of @p elements and of their groups; nullptr: none
const ProjectElement* findIn(const QList<ProjectElement>& elements, const QString& id) {
    QList<const QList<ProjectElement>*> lists{&elements};
    while (!lists.isEmpty()) {
        for (const ProjectElement& element : *lists.takeLast()) {
            if (element.id == id) {
                return &element;
            }
            lists << &element.elements;
        }
    }
    return nullptr;
}

/// The text of the chapter file @p file of the project in @p projectDir
QString textIn(const QString& projectDir, const QString& file) {
    const std::optional<ChapterDocument> chapter =
        ChapterDocument::load(QDir(projectDir).filePath(file));
    REQUIRE(chapter.has_value());
    return chapter->kml();
}

/// Write the text @p text to chapter @p elementId of the open book
void writeText(ProjectManager& pm, const QString& elementId, const char* text) {
    const ProjectElement* element = pm.findElement(elementId);
    REQUIRE(element != nullptr);
    std::optional<ChapterDocument> chapter = ChapterDocument::load(pm.filePathOf(*element));
    REQUIRE(chapter.has_value());
    chapter->setKml(QStringLiteral("<kml><p>%1</p></kml>").arg(QString::fromUtf8(text)));
    REQUIRE(chapter->save(pm.filePathOf(*element)));
}

/// The files of the folder @p folder and of its folders with what each has, to see that it
/// does not change
QHash<QString, QByteArray> filesIn(const QString& folder) {
    QHash<QString, QByteArray> files;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        files.insert(QDir(folder).relativeFilePath(path),
                     QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256));
    }
    return files;
}

/// A name of the book dictionary: @p text, of card @p card
DictionaryName nameOf(const char* text, const QString& card) {
    DictionaryName name;
    name.card = card;
    name.variants.append({QString::fromUtf8(text), {}, {}, {}, false});
    return name;
}

/// An element of kind @p kindId of the base package, for the projects made in the tests
ProjectElement elementOf(const char* id, const char* kindId, const char* title) {
    ProjectElement element;
    element.id = QString::fromLatin1(id);
    element.kind = KindReference{QStringLiteral("kalahari.base"), QString::fromLatin1(kindId)};
    element.title = QString::fromUtf8(title);
    return element;
}

/// A project of one book with id @p bookId, of the novel type
BookProject projectOf(const char* bookId) {
    BookProject project;
    project.id = QStringLiteral("project-") + QString::fromLatin1(bookId);
    project.type = kalahari::core::ProjectType{QStringLiteral("kalahari.novel"), QString(), {}};
    ProjectBook book;
    book.id = QString::fromLatin1(bookId);
    book.title = QString::fromLatin1(bookId);
    book.folder = QStringLiteral("book");
    project.books.append(book);
    return project;
}

}  // namespace

TEST_CASE("Series: a volume taken out is a book of its own, with what it has of the Workshop",
          "[core][series][join]") {
    const QTemporaryDir dir;
    const QTemporaryDir out;
    REQUIRE(dir.isValid());
    REQUIRE(out.isValid());
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString first = pm.book()->id;
    const QString sandChapter = pm.book()->mainElements.first().id;
    const QString sandBlurb = addBlurb(pm, "Opis tomu 1");
    const QString anna =
        pm.addElement(base(pm, "character"), QStringLiteral("Anna"), BookPlace::Workshop);
    REQUIRE_FALSE(anna.isEmpty());

    // The second volume, current, with its chapter, a description, a version and its goals
    const QString second =
        pm.addBook(NewBook{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()});
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE(pm.setCurrentBook(second));
    const QString dunesChapter = pm.book()->mainElements.first().id;
    writeText(pm, dunesChapter, "Wiatr.");
    const QString dunesBlurb = addBlurb(pm, "Opis tomu 2");
    REQUIRE(pm.keepVersion(dunesChapter, QStringLiteral("<kml><p>Cisza.</p></kml>"),
                           {.origin = VersionOrigin::Manual}) ==
            ProjectManager::VersionKept::Added);
    REQUIRE(pm.getDatabase() != nullptr);
    const QString goals = QStringLiteral("{\"targetWords\":50000}");
    pm.getDatabase()->setSetting(BookGoals::settingKeyOf(*pm.book()), goals);
    // The chapter of the first volume moved to the second one keeps its file in book/
    writeText(pm, sandChapter, "Piasek.");
    REQUIRE(pm.moveElementTo(sandChapter, ElementPlace{BookPlace::Main, QString(), 1}, second));
    REQUIRE(pm.findElement(sandChapter)->file.startsWith(QStringLiteral("book/")));
    // The card is about both chapters
    REQUIRE(pm.setLinks(anna, QStringList{sandChapter, dunesChapter}));

    const BookProject series = *pm.project();
    QStringList problems;
    const QString manifest = pm.takeOutBook(second, out.path(), true, &problems);
    INFO(problems.join(QLatin1Char('\n')).toStdString());
    REQUIRE_FALSE(manifest.isEmpty());
    CHECK(QFileInfo(manifest).fileName() == QStringLiteral("Wydmy.klh"));
    const QString bookDir = QFileInfo(manifest).absolutePath();
    CHECK(QDir(bookDir).dirName() == QStringLiteral("Wydmy"));
    // The series stays as it was
    CHECK(*pm.project() == series);

    QStringList loadProblems;
    const std::optional<BookProject> book = BookProject::load(manifest, loadProblems);
    REQUIRE(book.has_value());
    REQUIRE(book->books.size() == 1);
    const ProjectBook& volume = book->books.first();
    CHECK(volume.id == second);
    CHECK(volume.title == QStringLiteral("Wydmy"));
    CHECK(volume.folder == QStringLiteral("book"));
    CHECK(book->type == series.type);
    CHECK(book->id != series.id);
    CHECK(book->series.isEmpty());

    // Its texts in book/: its own file keeps its name, the other one gets a free one
    REQUIRE(volume.mainElements.size() == 2);
    const ProjectElement* own = findIn(volume.mainElements, dunesChapter);
    const ProjectElement* moved = findIn(volume.mainElements, sandChapter);
    REQUIRE(own != nullptr);
    REQUIRE(moved != nullptr);
    CHECK(own->file == QStringLiteral("book/chapter_001.kchapter"));
    CHECK(moved->file == QStringLiteral("book/chapter_001_2.kchapter"));
    CHECK(textIn(bookDir, own->file).contains(QStringLiteral("Wiatr.")));
    CHECK(textIn(bookDir, moved->file).contains(QStringLiteral("Piasek.")));
    // ... with the versions of its texts
    const ChapterVersions versions(bookDir, own->file);
    REQUIRE(versions.count() == 1);
    CHECK(versions.list().first().words.words == 1);

    // The Workshop: the card about its chapters only, its own description, not the other one
    const ProjectElement* card = findIn(book->workshop.elements, anna);
    REQUIRE(card != nullptr);
    CHECK(card->linkTargets() == QStringList({sandChapter, dunesChapter}));
    CHECK(findIn(book->workshop.elements, sandBlurb) == nullptr);
    const ProjectElement* blurb = findIn(book->workshop.elements, dunesBlurb);
    REQUIRE(blurb != nullptr);
    CHECK(blurb->bookId.isEmpty());
    CHECK(blurb->cover.state == QLatin1String(CoverMaterial::CHOSEN));
    if (!blurb->file.isEmpty()) {
        CHECK(QFileInfo::exists(QDir(bookDir).filePath(blurb->file)));
    }

    // Its goals
    ProjectDatabase database;
    REQUIRE(database.open(bookDir));
    CHECK(database.getSetting(QString::fromLatin1(BookGoals::SETTING_KEY)).toString() == goals);
    database.close();

    // The first volume taken out: the description of the second one stays in the series
    const std::optional<kalahari::core::TakenOutVolume> sand =
        VolumeCopies::takeOut(series, first);
    REQUIRE(sand.has_value());
    CHECK(findIn(sand->project.workshop.elements, sandBlurb) != nullptr);
    CHECK(findIn(sand->project.workshop.elements, dunesBlurb) == nullptr);
    const ProjectElement* sandCard = findIn(sand->project.workshop.elements, anna);
    REQUIRE(sandCard != nullptr);
    CHECK(sandCard->linkTargets().isEmpty());

    // A folder that is not empty takes no book, and nothing is written in it
    problems.clear();
    CHECK(pm.takeOutBook(second, out.path(), false, &problems).isEmpty());
    CHECK_FALSE(problems.isEmpty());
    CHECK(QDir(out.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries) ==
          QStringList{QStringLiteral("Wydmy")});
    // ... nor a book the series does not have
    CHECK(VolumeCopies::takeOut(series, QStringLiteral("none")) == std::nullopt);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: a book joined as a volume brings its texts, files and goals, and its Workshop "
          "joins the series'",
          "[core][series][join]") {
    const QTemporaryDir bookFolder;
    const QTemporaryDir seriesFolder;
    REQUIRE(bookFolder.isValid());
    REQUIRE(seriesFolder.isValid());
    auto& pm = projects();

    // The book: a chapter with a version, cards, a working note, a description of the back
    // cover, its dictionary and its goals
    REQUIRE(pm.createProject(bookFolder.path(), "Wydmy", "Ewa Lis", "pl", false,
                             "kalahari.novel"));
    const QString bookId = pm.book()->id;
    const QString chapter = pm.book()->mainElements.first().id;
    writeText(pm, chapter, "Wiatr.");
    REQUIRE(pm.keepVersion(chapter, QStringLiteral("<kml><p>Cisza.</p></kml>"),
                           {.origin = VersionOrigin::Manual}) ==
            ProjectManager::VersionKept::Added);
    const QString bookAnna =
        pm.addElement(base(pm, "character"), QStringLiteral("anna"), BookPlace::Workshop);
    const QString ola =
        pm.addElement(base(pm, "character"), QStringLiteral("Ola"), BookPlace::Workshop);
    const QString note =
        pm.addElement(base(pm, "work_note"), QStringLiteral("Pomysły"), BookPlace::Workshop);
    REQUIRE_FALSE(bookAnna.isEmpty());
    REQUIRE_FALSE(ola.isEmpty());
    REQUIRE_FALSE(note.isEmpty());
    REQUIRE(pm.setLinks(bookAnna, QStringList{chapter}));
    REQUIRE(pm.setLinks(ola, QStringList{chapter, bookAnna}));
    const QString blurb = addBlurb(pm, "Opis Wydm");
    REQUIRE(pm.changeBookDictionary([&bookAnna](BookDictionary& dictionary) {
        dictionary.putName(nameOf("Anna", bookAnna));
        dictionary.addWord(QStringLiteral("wydmowy"));
    }));
    const QString goals = QStringLiteral("{\"targetWords\":40000}");
    pm.getDatabase()->setSetting(BookGoals::settingKeyOf(*pm.book()), goals);
    const QString noteFile = pm.findElement(note)->file;
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    const QHash<QString, QByteArray> bookFiles = filesIn(bookFolder.path());

    // The series, of one book: its own Anna, a working note of the same name, its own
    // description of the back cover
    REQUIRE(pm.createProject(seriesFolder.path(), "Piasek", "Ewa Lis", "pl", false,
                             "kalahari.novel"));
    const QString first = pm.book()->id;
    const QString anna =
        pm.addElement(base(pm, "character"), QStringLiteral("Anna"), BookPlace::Workshop);
    const QString ownNote =
        pm.addElement(base(pm, "work_note"), QStringLiteral("Pomysły"), BookPlace::Workshop);
    REQUIRE(pm.findElement(ownNote)->file == noteFile);
    const QString ownBlurb = addBlurb(pm, "Opis Piasku");
    REQUIRE(pm.changeBookDictionary([&anna](BookDictionary& dictionary) {
        dictionary.putName(nameOf("Anna", anna));
    }));
    const qsizetype workshopBefore = pm.project()->workshop.elements.size();

    std::optional<BookProject> book;
    {
        QStringList problems;
        book = BookProject::load(manifest, problems);
        REQUIRE(book.has_value());
    }
    CHECK(pm.joinProblemOf(*book) == JoinProblem::None);
    const QList<CardPair> pairs = pm.sameCardsOf(*book);
    REQUIRE(pairs == QList<CardPair>{{bookAnna, anna}});

    int linksChanged = 0;
    const QMetaObject::Connection counting = QObject::connect(
        &pm, &ProjectManager::linksChanged, [&linksChanged]() { ++linksChanged; });
    QStringList problems;
    const QString volumeId = pm.joinBook(manifest, -1, pairs, &problems);
    QObject::disconnect(counting);
    INFO(problems.join(QLatin1Char('\n')).toStdString());
    REQUIRE(volumeId == bookId);
    REQUIRE(pm.project()->books.size() == 2);
    CHECK(pm.book()->id == first);
    const ProjectBook& volume = pm.project()->books.at(1);
    CHECK(volume.title == QStringLiteral("Wydmy"));
    CHECK(volume.folder == QStringLiteral("book2"));
    // Its text in its folder, with its version
    const ProjectElement* joinedChapter = findIn(volume.mainElements, chapter);
    REQUIRE(joinedChapter != nullptr);
    CHECK(joinedChapter->file == QStringLiteral("book2/chapter_001.kchapter"));
    CHECK(textIn(seriesFolder.path(), joinedChapter->file).contains(QStringLiteral("Wiatr.")));
    CHECK(ChapterVersions(seriesFolder.path(), joinedChapter->file).count() == 1);
    CHECK(pm.wordCount(chapter) == 1);

    // The Workshop: Anna of the series gains the link of the book's anna, Ola and the note
    // come at the end, the note under a free name, Ola linked with the series' Anna
    const QList<ProjectElement>& workshop = pm.project()->workshop.elements;
    CHECK(workshop.size() == workshopBefore + 3);
    CHECK(findIn(workshop, bookAnna) == nullptr);
    CHECK(pm.findElement(anna)->linkTargets() == QStringList{chapter});
    CHECK(linksChanged == 1);
    const ProjectElement* joinedOla = findIn(workshop, ola);
    REQUIRE(joinedOla != nullptr);
    CHECK(joinedOla->linkTargets() == QStringList({chapter, anna}));
    const ProjectElement* joinedNote = findIn(workshop, note);
    REQUIRE(joinedNote != nullptr);
    CHECK(joinedNote->file != noteFile);
    CHECK(QFileInfo(joinedNote->file).path() == QFileInfo(noteFile).path());
    CHECK(QFileInfo::exists(QDir(seriesFolder.path()).filePath(joinedNote->file)));
    // Each description of the cover for its own volume
    CHECK(pm.findElement(blurb)->bookId == volumeId);
    CHECK(pm.findElement(ownBlurb)->bookId == first);

    // The dictionary: the word of the book; the series' Anna keeps her name, once
    const BookDictionary& dictionary = pm.bookDictionary();
    CHECK(dictionary.hasWord(QStringLiteral("wydmowy")));
    CHECK(dictionary.names().size() == 1);
    // Its goals are those of the volume
    CHECK(pm.getDatabase()->getSetting(BookGoals::settingKeyOf(volume)).toString() == goals);

    // The book stays as it was, and it cannot join again
    CHECK(filesIn(bookFolder.path()) == bookFiles);
    CHECK(pm.joinProblemOf(*book) == JoinProblem::VolumeOfSeries);
    problems.clear();
    CHECK(pm.joinBook(manifest, -1, {}, &problems).isEmpty());
    CHECK(pm.project()->books.size() == 2);
    // A series is no book to join, nor a book of another type
    const BookProject series = *pm.project();
    CHECK(kalahari::core::VolumeCopies::joinProblem(projectOf("other"), series) ==
          JoinProblem::Series);
    BookProject poems = projectOf("poems");
    poems.type->id = QStringLiteral("kalahari.poetry");
    CHECK(kalahari::core::VolumeCopies::joinProblem(series, poems) == JoinProblem::OtherType);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: a volume taken out and deleted joins back with the same Workshop",
          "[core][series][join]") {
    const QTemporaryDir dir;
    const QTemporaryDir out;
    REQUIRE(dir.isValid());
    REQUIRE(out.isValid());
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString anna =
        pm.addElement(base(pm, "character"), QStringLiteral("Anna"), BookPlace::Workshop);
    const QString second =
        pm.addBook(NewBook{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()});
    REQUIRE(pm.setCurrentBook(second));
    const QString chapter = pm.book()->mainElements.first().id;
    REQUIRE(pm.setLinks(anna, QStringList{chapter}));
    const QString manifest = pm.takeOutBook(second, out.path(), true);
    REQUIRE_FALSE(manifest.isEmpty());
    REQUIRE(pm.removeBook(second).has_value());
    CHECK(pm.findElement(anna)->links.isEmpty());

    std::optional<BookProject> book;
    {
        QStringList problems;
        book = BookProject::load(manifest, problems);
        REQUIRE(book.has_value());
    }
    CHECK(pm.joinProblemOf(*book) == JoinProblem::None);
    // The same Anna: no pair to choose
    CHECK(pm.sameCardsOf(*book).isEmpty());
    const qsizetype workshop = pm.project()->workshop.elements.size();
    REQUIRE(pm.joinBook(manifest, 0, {}) == second);
    CHECK(pm.project()->books.first().id == second);
    CHECK(pm.project()->workshop.elements.size() == workshop);
    // Anna has her link back
    CHECK(pm.findElement(anna)->linkTargets() == QStringList{chapter});
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: a book joined brings its relations between cards, its styles and the fields "
          "of its cards, and its cards name those of the series",
          "[core][series][join]") {
    BookProject series = projectOf("sand");
    series.workshop.elements << elementOf("anna", "character", "Anna");
    series.workshop.relations << OwnRelation{QStringLiteral("own-1"),
                                             QStringLiteral("kalahari.base:character"),
                                             QStringLiteral("kalahari.base:character"),
                                             QStringLiteral("siostra"), QStringLiteral("own-1"),
                                             {}};
    series.paragraphStyles << kalahari::core::BookParagraphStyle{
        QStringLiteral("letter"), QStringLiteral("List"), {}, {}, {}, {}};

    BookProject book = projectOf("dunes");
    ProjectElement chapter = elementOf("chapter", "chapter", "Rozdział 1");
    chapter.links << ElementLink{QStringLiteral("anna2"), {}, 0, {}, {}, {}};
    book.books.first().mainElements << chapter;
    const ProjectElement bookAnna = elementOf("anna2", "character", "ANNA ");
    ProjectElement ola = elementOf("ola", "character", "Ola");
    ola.links << ElementLink{QStringLiteral("anna2"), QStringLiteral("own-1"), 0, {}, {}, {}};
    ola.card.values.insert(QStringLiteral("friend"), QStringLiteral("anna2"));
    CardPortrait portrait;
    portrait.image = QStringLiteral("anna2");
    ola.card.portraits << portrait;
    book.workshop.elements << bookAnna << ola;
    // "Mentor" and its inverse, under the ids the series has for others
    book.workshop.relations
        << OwnRelation{QStringLiteral("own-1"), QStringLiteral("kalahari.base:character"),
                       QStringLiteral("kalahari.base:character"), QStringLiteral("mentor"),
                       QStringLiteral("own-2"), {}}
        << OwnRelation{QStringLiteral("own-2"), QStringLiteral("kalahari.base:character"),
                       QStringLiteral("kalahari.base:character"), QStringLiteral("uczeń"),
                       QStringLiteral("own-1"), {}};
    book.paragraphStyles << kalahari::core::BookParagraphStyle{
        QStringLiteral("letter"), QStringLiteral("Letter"), {}, {}, {}, {}}
                         << kalahari::core::BookParagraphStyle{
                                QStringLiteral("song"), QStringLiteral("Piosenka"), {}, {}, {}, {}};
    book.workshop.cardFields.insert(QStringLiteral("kalahari.base:item"), {});

    const auto isCard = [](const ProjectElement& element) {
        return element.kind.kindId == QLatin1String("character");
    };
    const QList<CardPair> pairs = kalahari::core::VolumeCopies::sameCards(series, book, isCard);
    REQUIRE(pairs == QList<CardPair>{{QStringLiteral("anna2"), QStringLiteral("anna")}});
    JoinRequest request;
    request.folder = QStringLiteral("book2");
    request.sameCards = pairs;
    const std::optional<kalahari::core::JoinedVolume> joined =
        kalahari::core::VolumeCopies::join(series, book, request);
    REQUIRE(joined.has_value());
    const BookProject& project = joined->project;
    REQUIRE(project.books.size() == 2);

    // The chapter and Ola name the series' Anna, by her own relation of "mentor"
    CHECK(project.books.at(1).mainElements.first().linkTargets() ==
          QStringList{QStringLiteral("anna")});
    const ProjectElement* joinedOla = project.findElement(QStringLiteral("ola"));
    REQUIRE(joinedOla != nullptr);
    REQUIRE(joinedOla->links.size() == 1);
    CHECK(joinedOla->links.first().to == QStringLiteral("anna"));
    const QString mentor = joinedOla->links.first().role;
    CHECK(mentor != QStringLiteral("own-1"));
    CHECK(joinedOla->card.values.value(QStringLiteral("friend")) == QStringLiteral("anna"));
    CHECK(joinedOla->card.portraits.first().image == QStringLiteral("anna"));
    CHECK(project.findElement(QStringLiteral("anna2")) == nullptr);
    // The relations: the series' one, and "mentor" with its inverse
    REQUIRE(project.workshop.relations.size() == 3);
    const auto relation =
        std::ranges::find_if(project.workshop.relations,
                             [&mentor](const OwnRelation& each) { return each.id == mentor; });
    REQUIRE(relation != project.workshop.relations.cend());
    CHECK(relation->name == QStringLiteral("mentor"));
    // The series' style of the id stays, the other one comes; so do the fields of the items
    REQUIRE(project.paragraphStyles.size() == 2);
    CHECK(project.paragraphStyles.first().name == QStringLiteral("List"));
    CHECK(project.paragraphStyles.at(1).id == QStringLiteral("song"));
    CHECK(project.workshop.cardFields.contains(QStringLiteral("kalahari.base:item")));
}

