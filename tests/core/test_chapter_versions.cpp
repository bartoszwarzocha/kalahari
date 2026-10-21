/// @file test_chapter_versions.cpp
/// @brief Versions of the texts of the chapter files: where they are kept, their list, their
/// texts, and what goes when versions are deleted

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_versions.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

#include <string>

using namespace kalahari::core;

namespace {

/// The chapter file whose versions the tests keep
const QString CHAPTER = QStringLiteral("book/chapter_002.kchapter");

/// A moment of the day of the tests, in UTC
QDateTime at(int hour, int minute, int second = 0, int msec = 0) {
    return QDateTime(QDate(2026, 10, 21), QTime(hour, minute, second, msec), QTimeZone::utc());
}

QString kmlWith(const QString& text) {
    return QStringLiteral("<kml><p>%1</p></kml>").arg(text);
}

QJsonObject readJson(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeFile(const QString& path, const QByteArray& data) {
    REQUIRE(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(data) == data.size());
}

QByteArray bytesOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// Files of the versions, newest first
std::string filesOf(const QList<ChapterVersion>& versions) {
    QStringList files;
    for (const ChapterVersion& version : versions) {
        files << version.file;
    }
    return files.join(QStringLiteral(", ")).toStdString();
}

/// The name of the file of a version kept at @p created, in the computer's time
QString fileAt(const QDateTime& created, const QString& suffix = QString()) {
    return created.toLocalTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")) + suffix +
           QStringLiteral(".kml");
}

}  // namespace

TEST_CASE("The versions of a file are in versions/ under its path", "[core][versions]") {
    const QString book = QStringLiteral("/books/Caravan");

    CHECK(ChapterVersions::folderOf(book, CHAPTER) == "/books/Caravan/versions/book/chapter_002");
    CHECK(ChapterVersions::folderOf(book, "workshop/note_001.kchapter") ==
          "/books/Caravan/versions/workshop/note_001");
    CHECK(ChapterVersions::folderOf(book, "book\\Chapter_003.KCHAPTER") ==
          "/books/Caravan/versions/book/Chapter_003");
    CHECK(ChapterVersions::folderOf(book, "./book//chapter_004.kchapter") ==
          "/books/Caravan/versions/book/chapter_004");
    CHECK(ChapterVersions::folderOf(book, "workshop/notes") ==
          "/books/Caravan/versions/workshop/notes");

    // A file outside the folder of the book has none
    CHECK(ChapterVersions::folderOf(book, "../other/chapter.kchapter").isEmpty());
    CHECK(ChapterVersions::folderOf(book, "book/../../chapter.kchapter").isEmpty());
    CHECK(ChapterVersions::folderOf(book, "/books/chapter.kchapter").isEmpty());
    CHECK(ChapterVersions::folderOf(book, "C:/books/chapter.kchapter").isEmpty());
    CHECK(ChapterVersions::folderOf(book, ".kchapter").isEmpty());
    CHECK(ChapterVersions::folderOf(book, QString()).isEmpty());
    CHECK(ChapterVersions::folderOf(QString(), CHAPTER).isEmpty());

    CHECK(ChapterVersions(book, CHAPTER).isValid());
    CHECK_FALSE(ChapterVersions(book, "../chapter.kchapter").isValid());
    CHECK(ChapterVersions(book, "../chapter.kchapter").list().isEmpty());
    CHECK_FALSE(ChapterVersions(book, "../chapter.kchapter")
                    .add(kmlWith("Text"), {.origin = VersionOrigin::Manual}));
}

TEST_CASE("Versions keep their texts, newest first, with index.json listing them",
          "[core][versions]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ChapterVersions versions(dir.path(), CHAPTER);
    CHECK(versions.list().isEmpty());
    CHECK_FALSE(versions.newest());
    CHECK_FALSE(QFileInfo::exists(versions.folder()));

    const QString first = kmlWith(QStringLiteral("Burza przyszła od zachodu."));
    const QString second =
        QStringLiteral("<kml><p>– Zawracamy – rzucił Tau.</p><p>Nadia milczała.</p></kml>");
    const QString third = kmlWith(QStringLiteral("Piasek uniósł się jak ściana."));

    const auto kept1 = versions.add(
        first, {.origin = VersionOrigin::Manual, .name = "Pierwsza wersja burzy"},
        at(9, 5, 12, 918));
    REQUIRE(kept1);
    const auto kept2 = versions.add(second,
                                    {.origin = VersionOrigin::ReplaceAll,
                                     .name = "Po zmianie imienia",
                                     .found = "Taun",
                                     .replacement = "Tau",
                                     .replaced = 12},
                                    at(11, 30));
    REQUIRE(kept2);
    const auto kept3 = versions.add(third, {.origin = VersionOrigin::Save}, at(10, 0));
    REQUIRE(kept3);

    // The file of each version is named after the time it was kept, in the computer's time
    CHECK(kept1->file == fileAt(at(9, 5, 12)));
    CHECK(kept1->created == at(9, 5, 12, 918));
    CHECK(kept1->origin == VersionOrigin::Manual);
    CHECK(kept1->name == "Pierwsza wersja burzy");
    CHECK(kept1->found.isEmpty());
    CHECK(kept1->replacement.isEmpty());
    CHECK(kept1->replaced == 0);
    CHECK_FALSE(kept1->restored.isValid());
    CHECK(kept1->words == WordTally{4, 0});
    CHECK_FALSE(kept1->isAutomatic());
    CHECK(kept2->name == "Po zmianie imienia");
    CHECK(kept2->found == "Taun");
    CHECK(kept2->replacement == "Tau");
    CHECK(kept2->replaced == 12);
    CHECK(kept2->words == WordTally{5, 2});
    CHECK(kept2->isAutomatic());

    // Newest first, whatever the order they were kept in
    const QList<ChapterVersion> listed = versions.list();
    CHECK(filesOf(listed) == filesOf({*kept2, *kept3, *kept1}));
    CHECK(listed == QList<ChapterVersion>{*kept2, *kept3, *kept1});
    CHECK(versions.count() == 3);
    CHECK(versions.newest() == kept2);
    CHECK(versions.find(kept3->file) == kept3);
    CHECK_FALSE(versions.find("2026-01-01_000000.kml"));
    CHECK(versions.lastCreated(VersionOrigin::Save) == at(10, 0));
    CHECK(versions.lastCreated(VersionOrigin::Manual) == at(9, 5, 12, 918));
    CHECK_FALSE(versions.lastCreated(VersionOrigin::Restore));

    // Their texts, as they were given
    CHECK(versions.text(kept1->file) == first);
    CHECK(versions.text(kept2->file) == second);
    CHECK(versions.text(kept3->file) == third);
    CHECK_FALSE(versions.text("missing.kml"));
    CHECK(versions.newestHas(second));
    CHECK_FALSE(versions.newestHas(third));
    CHECK(bytesOf(QDir(versions.folder()).filePath(kept1->file)) == first.toUtf8());

    // index.json says what the program knows about them, for a person too
    const QJsonObject index = readJson(QDir(versions.folder()).filePath("index.json"));
    CHECK(index["format"].toInt() == 1);
    CHECK(index["file"].toString() == CHAPTER);
    const QJsonArray entries = index["versions"].toArray();
    REQUIRE(entries.size() == 3);
    const QJsonObject newest = entries.at(0).toObject();
    CHECK(newest["file"].toString() == kept2->file);
    CHECK(newest["created"].toString() == "2026-10-21T11:30:00.000Z");
    CHECK(newest["origin"].toString() == "replaceAll");
    CHECK(newest["name"].toString() == "Po zmianie imienia");
    CHECK(newest["find"].toString() == "Taun");
    CHECK(newest["replace"].toString() == "Tau");
    CHECK(newest["replaced"].toInt() == 12);
    CHECK_FALSE(newest.contains("restored"));
    CHECK(newest["words"].toInt() == 5);
    CHECK(newest["dashes"].toInt() == 2);
    const QJsonObject oldest = entries.at(2).toObject();
    CHECK(oldest["created"].toString() == "2026-10-21T09:05:12.918Z");
    CHECK(oldest["origin"].toString() == "manual");
    CHECK(oldest["name"].toString() == "Pierwsza wersja burzy");
    CHECK_FALSE(oldest.contains("find"));
    CHECK_FALSE(oldest.contains("replace"));
    CHECK_FALSE(oldest.contains("replaced"));
    CHECK_FALSE(oldest.contains("restored"));
    CHECK_FALSE(entries.at(1).toObject().contains("name"));
    CHECK(entries.at(1).toObject()["origin"].toString() == "save");

    // Another file has its own versions
    ChapterVersions other(dir.path(), "book/chapter_003.kchapter");
    CHECK(other.list().isEmpty());
    REQUIRE(other.add(first, {.origin = VersionOrigin::Manual}));
    CHECK(other.count() == 1);
    CHECK(versions.count() == 3);
}

TEST_CASE("Versions kept in the same second get files of their own", "[core][versions]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ChapterVersions versions(dir.path(), CHAPTER);

    const auto one = versions.add(
        kmlWith("One"), {.origin = VersionOrigin::Restore, .restored = at(9, 5, 12, 918)},
        at(15, 12, 7, 100));
    const auto two =
        versions.add(kmlWith("Two"), {.origin = VersionOrigin::Manual}, at(15, 12, 7, 600));
    const auto three =
        versions.add(kmlWith("Three"), {.origin = VersionOrigin::Manual}, at(15, 12, 7, 900));
    REQUIRE(one);
    REQUIRE(two);
    REQUIRE(three);
    CHECK(one->restored == at(9, 5, 12, 918));
    CHECK(versions.find(one->file) == one);
    CHECK_FALSE(two->restored.isValid());
    CHECK(one->file == fileAt(at(15, 12, 7)));
    CHECK(two->file == fileAt(at(15, 12, 7), "_2"));
    CHECK(three->file == fileAt(at(15, 12, 7), "_3"));
    CHECK(filesOf(versions.list()) == filesOf({*three, *two, *one}));
    CHECK(versions.text(two->file) == kmlWith("Two"));

    // A file left in the folder is not written over
    const QDateTime later = at(16, 0);
    writeFile(QDir(versions.folder()).filePath(fileAt(later)), "the writer's own file");
    const auto four = versions.add(kmlWith("Four"), {.origin = VersionOrigin::Manual}, later);
    REQUIRE(four);
    CHECK(four->file == fileAt(later, "_2"));
    CHECK(bytesOf(QDir(versions.folder()).filePath(fileAt(later))) == "the writer's own file");
    CHECK(versions.count() == 4);  // the writer's file is not a version
}

TEST_CASE("A version gets the writer's description and can be deleted", "[core][versions]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ChapterVersions versions(dir.path(), CHAPTER);
    ChapterVersions other(dir.path(), "book/chapter_003.kchapter");
    const auto older = versions.add(kmlWith("Older"), {.origin = VersionOrigin::Save}, at(8, 0));
    const auto newer = versions.add(kmlWith("Newer"), {.origin = VersionOrigin::Manual}, at(9, 0));
    REQUIRE(older);
    REQUIRE(newer);
    REQUIRE(other.add(kmlWith("Other"), {.origin = VersionOrigin::Manual}));

    CHECK(versions.rename(older->file, QStringLiteral("Przed skróceniem dialogu z Tau")));
    CHECK(versions.find(older->file)->name == QStringLiteral("Przed skróceniem dialogu z Tau"));
    CHECK(versions.find(older->file)->origin == VersionOrigin::Save);
    CHECK(versions.rename(older->file, QString()));
    CHECK(versions.find(older->file)->name.isEmpty());
    CHECK_FALSE(versions.rename("missing.kml", "Name"));

    // The version goes with its file
    const QString olderPath = QDir(versions.folder()).filePath(older->file);
    CHECK(versions.remove(older->file));
    CHECK(filesOf(versions.list()) == filesOf({*newer}));
    CHECK_FALSE(QFileInfo::exists(olderPath));
    CHECK_FALSE(versions.remove(older->file));

    // The last one takes the folder with it; versions/ stays for the other file
    CHECK(versions.remove(newer->file));
    CHECK(versions.list().isEmpty());
    CHECK_FALSE(QFileInfo::exists(versions.folder()));
    CHECK(QFileInfo::exists(other.folder()));

    // versions/ goes too when no file has versions
    CHECK(other.remove(other.newest()->file));
    CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("versions")));
    CHECK(QFileInfo::exists(dir.path()));
}

TEST_CASE("The oldest versions kept by the program go; the writer's own stay",
          "[core][versions]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ChapterVersions versions(dir.path(), CHAPTER);
    const auto save1 = versions.add(kmlWith("1"), {.origin = VersionOrigin::Save}, at(8, 0));
    const auto manual1 = versions.add(kmlWith("2"), {.origin = VersionOrigin::Manual}, at(8, 10));
    const auto replace =
        versions.add(kmlWith("3"), {.origin = VersionOrigin::ReplaceAll}, at(8, 20));
    const auto restore = versions.add(kmlWith("4"), {.origin = VersionOrigin::Restore}, at(8, 30));
    const auto manual2 = versions.add(kmlWith("5"), {.origin = VersionOrigin::Manual}, at(8, 40));
    const auto save2 = versions.add(kmlWith("6"), {.origin = VersionOrigin::Save}, at(8, 50));
    REQUIRE((save1 && manual1 && replace && restore && manual2 && save2));

    CHECK(versions.removeOldAutomatic(4) == 0);
    CHECK(versions.count() == 6);

    CHECK(versions.removeOldAutomatic(2) == 2);
    CHECK(filesOf(versions.list()) == filesOf({*save2, *manual2, *restore, *manual1}));
    CHECK_FALSE(QFileInfo::exists(QDir(versions.folder()).filePath(save1->file)));
    CHECK_FALSE(QFileInfo::exists(QDir(versions.folder()).filePath(replace->file)));

    CHECK(versions.removeOldAutomatic(0) == 2);
    CHECK(filesOf(versions.list()) == filesOf({*manual2, *manual1}));
    CHECK(versions.removeOldAutomatic(-1) == 0);
    CHECK(versions.count() == 2);
}

TEST_CASE("index.json written elsewhere is read with care", "[core][versions]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ChapterVersions versions(dir.path(), CHAPTER);
    const QDir folder(versions.folder());
    writeFile(folder.filePath("a.kml"), kmlWith("A").toUtf8());
    writeFile(folder.filePath("b.kml"), kmlWith("B").toUtf8());
    writeFile(folder.filePath("orphan.kml"), kmlWith("Orphan").toUtf8());
    writeFile(dir.filePath("secret.kml"), kmlWith("Secret").toUtf8());

    SECTION("Listed versions without their files, and paths, are left out") {
        const QJsonArray entries{
            QJsonObject{{"file", "a.kml"}, {"created", "2026-10-21T10:00:00.000Z"},
                        {"origin", "save"}, {"words", 1}},
            QJsonObject{{"file", "gone.kml"}, {"created", "2026-10-21T11:00:00.000Z"},
                        {"origin", "manual"}},
            QJsonObject{{"file", "../../../secret.kml"},
                        {"created", "2026-10-21T12:00:00.000Z"}, {"origin", "manual"}},
            QJsonObject{{"file", "b.kml"}, {"created", "2026-10-21T09:00:00Z"},
                        {"origin", "somethingNew"}, {"words", -5}}};
        writeFile(folder.filePath("index.json"),
                  QJsonDocument(QJsonObject{{"format", 1}, {"file", CHAPTER},
                                            {"versions", entries}})
                      .toJson());

        const QList<ChapterVersion> listed = versions.list();
        CHECK(filesOf(listed) == "a.kml, b.kml");
        CHECK(listed.at(0).origin == VersionOrigin::Save);
        CHECK(listed.at(0).words.words == 1);
        // An origin the program does not know is the writer's: it does not delete it
        CHECK(listed.at(1).origin == VersionOrigin::Manual);
        CHECK(listed.at(1).created == at(9, 0));
        CHECK(listed.at(1).words.words == 0);
        CHECK_FALSE(versions.text("../../../secret.kml"));
        CHECK_FALSE(versions.text("orphan.kml"));
        CHECK(versions.removeOldAutomatic(0) == 1);
        CHECK(filesOf(versions.list()) == "b.kml");
    }

    SECTION("A file that is not a list of versions is not written over") {
        writeFile(folder.filePath("index.json"), "{ not json");
        CHECK(versions.list().isEmpty());
        CHECK_FALSE(versions.add(kmlWith("New"), {.origin = VersionOrigin::Manual}));
        CHECK(bytesOf(folder.filePath("index.json")) == "{ not json");
        CHECK(folder.entryList(QDir::Files).size() == 4);  // a, b, orphan and index.json
    }

    SECTION("A list of a newer format is read but not written over") {
        const QByteArray newer = QJsonDocument(
            QJsonObject{{"format", 2}, {"file", CHAPTER},
                        {"versions", QJsonArray{QJsonObject{
                                         {"file", "a.kml"},
                                         {"created", "2026-10-21T10:00:00.000Z"},
                                         {"origin", "save"},
                                         {"branch", "draft 2"}}}}}).toJson();
        writeFile(folder.filePath("index.json"), newer);
        CHECK(filesOf(versions.list()) == "a.kml");
        CHECK(versions.text("a.kml") == kmlWith("A"));
        CHECK_FALSE(versions.add(kmlWith("New"), {.origin = VersionOrigin::Manual}));
        CHECK_FALSE(versions.rename("a.kml", "Name"));
        CHECK_FALSE(versions.remove("a.kml"));
        CHECK(versions.removeOldAutomatic(0) == -1);
        CHECK(bytesOf(folder.filePath("index.json")) == newer);
        CHECK(QFileInfo::exists(folder.filePath("a.kml")));
    }

    SECTION("A list that cannot be read is not written over") {
        REQUIRE(folder.mkpath("index.json"));  // a folder where the file should be
        CHECK(versions.list().isEmpty());
        CHECK_FALSE(versions.add(kmlWith("New"), {.origin = VersionOrigin::Manual}));
        CHECK(folder.entryList(QDir::Files).size() == 3);  // a, b and orphan
    }
}

TEST_CASE("The names of the origins of versions", "[core][versions]") {
    for (const VersionOrigin origin : {VersionOrigin::Manual, VersionOrigin::Save,
                                       VersionOrigin::ReplaceAll, VersionOrigin::Restore}) {
        CHECK(versionOriginNamed(versionOriginName(origin)) == origin);
    }
    CHECK(versionOriginName(VersionOrigin::ReplaceAll) == "replaceAll");
    CHECK_FALSE(versionOriginNamed("Manual"));
    CHECK_FALSE(versionOriginNamed(QString()));
}
