// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_join.cpp
/// @brief A series: a volume taken out as a book of its own, with what it keeps of the Workshop,
///        its files, their versions and its goals

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/volume_copies.h>
#include <kalahari/core/writing_goals.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <optional>

using kalahari::core::BookGoals;
using kalahari::core::BookPlace;
using kalahari::core::BookProject;
using kalahari::core::ChapterDocument;
using kalahari::core::ChapterVersions;
using kalahari::core::CoverMaterial;
using kalahari::core::ElementPlace;
using kalahari::core::KindRef;
using kalahari::core::NewBook;
using kalahari::core::NewMaterial;
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
