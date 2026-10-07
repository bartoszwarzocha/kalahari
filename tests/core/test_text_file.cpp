/// @file test_text_file.cpp
/// @brief Plain text files read and written in their own encoding and line ends

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/text_file.h>
#include <QFile>
#include <QStringEncoder>
#include <QTemporaryDir>

using namespace kalahari::core;

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

/// Reads @p data as a text file and writes the text back to another file, unchanged
QByteArray roundTrip(const QTemporaryDir& dir, const QByteArray& data, TextFileContent& content) {
    writeBytes(dir.filePath("in.txt"), data);
    const auto read = readTextFile(dir.filePath("in.txt"));
    REQUIRE(read.has_value());
    content = *read;
    TextFileFormat format = content.format;
    REQUIRE(writeTextFile(dir.filePath("out.txt"), content.text, format));
    CHECK(format == content.format);
    return bytesOf(dir.filePath("out.txt"));
}

const QString POLISH = QStringLiteral("Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144");

}  // namespace

TEST_CASE("A UTF-8 text file is written back as it was", "[core][text_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    TextFileContent content;

    SECTION("Windows line ends") {
        const QByteArray data = "One\r\n" + POLISH.toUtf8() + "\r\n";
        CHECK(roundTrip(dir, data, content) == data);
        CHECK(content.text == "One\n" + POLISH + "\n");
        CHECK(content.format.encoding == QStringConverter::Utf8);
        CHECK_FALSE(content.format.byteOrderMark);
        CHECK(content.format.crlf);
    }

    SECTION("Unix line ends, no line end at the end") {
        const QByteArray data = "One\n" + POLISH.toUtf8();
        CHECK(roundTrip(dir, data, content) == data);
        CHECK(content.text == "One\n" + POLISH);
        CHECK_FALSE(content.format.crlf);
    }

    SECTION("Byte order mark") {
        const QByteArray data = "\xEF\xBB\xBF" + POLISH.toUtf8() + "\n";
        CHECK(roundTrip(dir, data, content) == data);
        CHECK(content.text == POLISH + "\n");  // without the mark
        CHECK(content.format.byteOrderMark);
    }

    SECTION("Empty file") {
        CHECK(roundTrip(dir, QByteArray(), content).isEmpty());
        CHECK(content.text.isEmpty());
    }
}

TEST_CASE("A byte order mark gives the encoding of a text file", "[core][text_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    TextFileContent content;

    QStringEncoder encoder(QStringConverter::Utf16LE, QStringConverter::Flag::WriteBom);
    const QByteArray data = encoder(POLISH + "\r\nTwo");
    CHECK(roundTrip(dir, data, content) == data);
    CHECK(content.text == POLISH + "\nTwo");
    CHECK(content.format.encoding == QStringConverter::Utf16LE);
    CHECK(content.format.byteOrderMark);
    CHECK(content.format.crlf);
}

TEST_CASE("A text file that is not UTF-8 is read in an 8-bit encoding", "[core][text_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    TextFileContent content;

    // "Caf\u00E9" with a Latin-1 / Windows-1250 / Windows-1252 e acute
    const QByteArray data = "Caf\xE9\nTwo\n";
    CHECK(roundTrip(dir, data, content) == data);
    CHECK(content.text == QStringLiteral("Caf\u00E9\nTwo\n"));
    CHECK(content.format.encoding != QStringConverter::Utf8);

    SECTION("Text the encoding cannot hold is written as UTF-8") {
        TextFileFormat format = content.format;
        const QString text = content.text + QStringLiteral("\u4E2D");  // in no 8-bit code page
        REQUIRE(writeTextFile(dir.filePath("out.txt"), text, format));
        CHECK(format.encoding == QStringConverter::Utf8);
        CHECK(bytesOf(dir.filePath("out.txt")) == text.toUtf8());
    }
}

TEST_CASE("Old Mac line ends are read as line ends", "[core][text_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeBytes(dir.filePath("mac.txt"), "One\rTwo\r");

    const auto content = readTextFile(dir.filePath("mac.txt"));
    REQUIRE(content.has_value());
    CHECK(content->text == "One\nTwo\n");
    CHECK_FALSE(content->format.crlf);
}

TEST_CASE("A new text file is UTF-8 with the system's line ends", "[core][text_file]") {
    const TextFileFormat format = TextFileFormat::newFile();
    CHECK(format.encoding == QStringConverter::Utf8);
    CHECK_FALSE(format.byteOrderMark);
#ifdef Q_OS_WIN
    CHECK(format.crlf);
#else
    CHECK_FALSE(format.crlf);
#endif
}

TEST_CASE("Text files that cannot be read or written give the reason", "[core][text_file]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString error;

    CHECK_FALSE(readTextFile(dir.filePath("missing.txt"), &error).has_value());
    CHECK_FALSE(error.isEmpty());

    error.clear();
    TextFileFormat format;
    CHECK_FALSE(writeTextFile(dir.filePath("no/such/folder/file.txt"), "text", format, &error));
    CHECK_FALSE(error.isEmpty());
}
