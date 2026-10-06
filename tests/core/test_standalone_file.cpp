/// @file test_standalone_file.cpp
/// @brief Chapters and plain text files opened on their own, outside a book project

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/standalone_file.h>
#include <kalahari/editor/kml_document_model.h>
#include <QFile>
#include <QTemporaryDir>

using namespace kalahari::core;
using kalahari::editor::KmlDocumentModel;

namespace {

QByteArray bytesOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

void writeBytes(const QString& path, const QByteArray& data) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data) == data.size());
}

/// Paragraph texts of KML, read as the editor reads it
QStringList paragraphsOf(const QString& kml) {
    KmlDocumentModel model;
    REQUIRE(model.loadKml(kml));  // the whole KML is readable
    QStringList paragraphs;
    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        paragraphs << model.paragraphText(i);
    }
    return paragraphs;
}

/// A chapter file with data besides its content
void writeChapter(const QString& path, const QString& kml) {
    ChapterDocument chapter;
    chapter.setTitle("The Title");
    chapter.setStatus("revision");
    chapter.setNotes("Some notes");
    chapter.setKml(kml);
    REQUIRE(chapter.save(path));
}

}  // namespace

TEST_CASE("Standalone files are chapters or text files, by extension", "[core][standalone_file]") {
    CHECK(StandaloneFile::typeOf("/a/b/Chapter.kchapter") == StandaloneFile::Type::Chapter);
    CHECK(StandaloneFile::typeOf("C:/Books/CHAPTER.KCHAPTER") == StandaloneFile::Type::Chapter);
    CHECK(StandaloneFile::typeOf("notes.txt") == StandaloneFile::Type::PlainText);
    CHECK(StandaloneFile::typeOf("notes.TXT") == StandaloneFile::Type::PlainText);
    CHECK(StandaloneFile::typeOf("old.rtf") == StandaloneFile::Type::Unsupported);
    CHECK(StandaloneFile::typeOf("map.kmap") == StandaloneFile::Type::Unsupported);
    CHECK(StandaloneFile::typeOf("README") == StandaloneFile::Type::Unsupported);
}

TEST_CASE("A text file opens as paragraphs and is saved in its own format",
          "[core][standalone_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("notes.txt");
    writeBytes(path, "\xEF\xBB\xBFOne\r\nTwo & <three>\r\n");

    QString kml;
    auto file = StandaloneFile::open(path, kml);
    REQUIRE(file.has_value());
    CHECK(file->type() == StandaloneFile::Type::PlainText);
    CHECK(file->path() == path);
    CHECK(paragraphsOf(kml) == QStringList{"One", "Two & <three>", ""});

    REQUIRE(file->save("<p><t>unused</t></p>", "One\nTwo & <three>\nFour"));
    CHECK(bytesOf(path) == "\xEF\xBB\xBFOne\r\nTwo & <three>\r\nFour");

    SECTION("Control characters do not break its paragraphs") {
        writeBytes(path, QByteArray("a\fb\0c\nd\te", 11));
        REQUIRE(StandaloneFile::open(path, kml).has_value());
        CHECK(paragraphsOf(kml) == QStringList{"abc", "d\te"});
    }
}

TEST_CASE("A chapter file keeps its other data when saved", "[core][standalone_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("Chapter.kchapter");
    writeChapter(path, "<p><t>Old text</t></p>");

    QString kml;
    auto file = StandaloneFile::open(path, kml);
    REQUIRE(file.has_value());
    CHECK(file->type() == StandaloneFile::Type::Chapter);
    CHECK(kml == "<p><t>Old text</t></p>");

    REQUIRE(file->save("<p><t>New <b>text</b></t></p>", "New text"));
    const auto saved = ChapterDocument::load(path);
    REQUIRE(saved.has_value());
    CHECK(saved->kml() == "<p><t>New <b>text</b></t></p>");
    CHECK(saved->title() == "The Title");
    CHECK(saved->status() == "revision");
    CHECK(saved->notes() == "Some notes");
    CHECK(saved->wordCount() == 2);
}

TEST_CASE("Save As writes the format of the new extension", "[core][standalone_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString chapterPath = dir.filePath("Chapter.kchapter");
    writeChapter(chapterPath, "<p><t>Text</t></p>");
    QString kml;
    auto file = StandaloneFile::open(chapterPath, kml);
    REQUIRE(file.has_value());

    SECTION("A chapter saved as a chapter keeps its data") {
        const QString copyPath = dir.filePath("Copy.kchapter");
        REQUIRE(file->saveAs(copyPath, "<p><t>Changed</t></p>", "Changed"));
        CHECK(file->path() == copyPath);
        const auto copy = ChapterDocument::load(copyPath);
        REQUIRE(copy.has_value());
        CHECK(copy->kml() == "<p><t>Changed</t></p>");
        CHECK(copy->title() == "The Title");
        CHECK(copy->status() == "revision");
        CHECK(ChapterDocument::load(chapterPath)->kml() == "<p><t>Text</t></p>");  // unchanged
    }

    SECTION("A chapter saved as text, then as a chapter again") {
        const QString textPath = dir.filePath("Plain.txt");
        REQUIRE(file->saveAs(textPath, "<p><t>Plain</t></p><p><t>text</t></p>", "Plain\ntext"));
        CHECK(file->type() == StandaloneFile::Type::PlainText);
        CHECK(file->path() == textPath);
#ifdef Q_OS_WIN
        CHECK(bytesOf(textPath) == "Plain\r\ntext");
#else
        CHECK(bytesOf(textPath) == "Plain\ntext");
#endif

        const QString newChapterPath = dir.filePath("New Chapter.kchapter");
        REQUIRE(file->saveAs(newChapterPath, "<p><t>Plain</t></p>", "Plain"));
        CHECK(file->type() == StandaloneFile::Type::Chapter);
        const auto chapter = ChapterDocument::load(newChapterPath);
        REQUIRE(chapter.has_value());
        CHECK(chapter->title() == "New Chapter");  // a new chapter, named after its file
        CHECK(chapter->status() == "draft");
        CHECK(chapter->kml() == "<p><t>Plain</t></p>");
    }

    SECTION("Not to a file of another type") {
        CHECK_FALSE(file->saveAs(dir.filePath("Chapter.rtf"), kml, "Text"));
        CHECK(file->path() == chapterPath);
        CHECK_FALSE(QFile::exists(dir.filePath("Chapter.rtf")));
    }
}

TEST_CASE("Unsupported and unreadable files do not open", "[core][standalone_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString kml;
    QString error;

    writeBytes(dir.filePath("old.rtf"), "{\\rtf1 text}");
    CHECK_FALSE(StandaloneFile::open(dir.filePath("old.rtf"), kml, &error).has_value());

    CHECK_FALSE(StandaloneFile::open(dir.filePath("missing.txt"), kml, &error).has_value());
    CHECK_FALSE(error.isEmpty());

    writeBytes(dir.filePath("broken.kchapter"), "{ not json");
    CHECK_FALSE(StandaloneFile::open(dir.filePath("broken.kchapter"), kml, &error).has_value());
}
