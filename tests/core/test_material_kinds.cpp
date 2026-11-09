/// @file test_material_kinds.cpp
/// @brief The kind that the file or the web address of a material gives it

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/material_kinds.h>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

using namespace kalahari::core;

namespace {

/// Writes @p data to @p path
void writeBytes(const QString& path, const QByteArray& data) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data) == data.size());
}

/// A small PNG picture
QByteArray png() {
    QImage image(4, 3, QImage::Format_RGB32);
    image.fill(Qt::darkYellow);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    REQUIRE(image.save(&buffer, "PNG"));
    return bytes;
}

} // namespace

TEST_CASE("Material kinds: the kind of a file by its type", "[core][materials]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    // By the name of a file that is not there
    CHECK(MaterialKinds::ofFile(dir.filePath("map.jpg")) == "picture");
    CHECK(MaterialKinds::ofFile(dir.filePath("Map.PNG")) == "picture");
    CHECK(MaterialKinds::ofFile(dir.filePath("plan.svg")) == "picture");
    CHECK(MaterialKinds::ofFile(dir.filePath("talk.mp3")) == "recording");
    CHECK(MaterialKinds::ofFile(dir.filePath("dune.mp4")) == "recording");
    CHECK(MaterialKinds::ofFile(dir.filePath("atlas.pdf")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("letter.docx")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("letter.doc")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("letter.odt")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("letter.rtf")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("notes.txt")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("notes.md")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("costs.xlsx")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("costs.ods")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("talk.pptx")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("book.epub")) == "document");
    CHECK(MaterialKinds::ofFile(dir.filePath("route.zip")) == "other_file");
    CHECK(MaterialKinds::ofFile(dir.filePath("route.kml")) == "other_file");
    CHECK(MaterialKinds::ofFile(dir.filePath("tool.cpp")) == "other_file");
    CHECK(MaterialKinds::ofFile(dir.filePath("without suffix")) == "other_file");

    // By the contents of a file that is there: a picture under another name is a picture
    const QString picture = dir.filePath("scan.dat");
    writeBytes(picture, png());
    CHECK(MaterialKinds::ofFile(picture) == "picture");
    const QString text = dir.filePath("notes.txt");
    writeBytes(text, QByteArrayLiteral("The well is two days from Ghanzi.\n"));
    CHECK(MaterialKinds::ofFile(text) == "document");
}

TEST_CASE("Material kinds: the kind of a source, and a picture by its address",
          "[core][materials]") {
    // A picture by the end of its path; a page of a wiki about a picture is a page
    CHECK(MaterialKinds::isPictureAddress(QUrl("https://example.com/a/Camel.JPG")));
    CHECK(MaterialKinds::isPictureAddress(QUrl("https://example.com/dune.webp?w=800")));
    CHECK_FALSE(
        MaterialKinds::isPictureAddress(QUrl("https://commons.wikimedia.org/wiki/File:Camel.jpg")));
    CHECK_FALSE(MaterialKinds::isPictureAddress(QUrl("https://example.com/kalahari")));
    CHECK_FALSE(MaterialKinds::isPictureAddress(QUrl("https://example.com/")));
    CHECK(MaterialKinds::isPictureName("Camel.jpeg"));
    CHECK_FALSE(MaterialKinds::isPictureName("jpeg"));

    // An address: a picture when it names one or has its copy; else a page
    CHECK(MaterialKinds::ofSource(QUrl("https://example.com/camel.png"), QString()) == "picture");
    CHECK(MaterialKinds::ofSource(QUrl("https://example.com/photo?id=7"), "/book/photo.png") ==
          "picture");
    CHECK(MaterialKinds::ofSource(QUrl("https://en.wikipedia.org/wiki/Kalahari"), QString()) ==
          "web_page");

    // A file alone: its kind; nothing: no kind
    CHECK(MaterialKinds::ofSource(QUrl(), "/book/workshop/atlas.pdf") == "document");
    CHECK(MaterialKinds::ofSource(QUrl(), QString()).isEmpty());

    // The kinds, in the order of the packages
    CHECK(MaterialKinds::all() == QStringList{"quote", "web_page", "picture", "document",
                                              "publication", "recording", "other_file"});
}
