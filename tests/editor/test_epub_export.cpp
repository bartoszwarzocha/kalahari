// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_epub_export.cpp
/// @brief Export of a book to EPUB 3: the archive, the package with its data, the documents
///        of the texts, the navigation, the notes, the images, the cover, the look and the font
///
/// With the environment variable KALAHARI_EXPORT_SAMPLES set to a folder, the files are copied
/// there, to be opened in an e-book reader or checked with EPUBCheck.

#include "test_support/export_samples.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_export.h>

#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimeZone>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// The book exported to an EPUB, the files of the archive by name
QHash<QString, QByteArray> exported(const BookExport& book, const QString& path, ExportResult* result = nullptr,
                                    const QString& sample = QString()) {
    ExportResult written;
    ExportProblem problem;
    REQUIRE(exportEpub(book, path, {}, &written, &problem));
    CHECK(problem.kind == ExportProblem::Kind::None);
    keepSample(path, sample);
    if (result) {
        *result = written;
    }
    return zipEntries(path);
}

/// A file of the archive as text
QString text(const QHash<QString, QByteArray>& entries, const char* name) {
    REQUIRE(entries.contains(QString::fromLatin1(name)));
    return QString::fromUtf8(entries.value(QString::fromLatin1(name)));
}

/// The ids of the spine of a package, in their order
QStringList spineOf(const QString& package) {
    static const QRegularExpression itemref(QStringLiteral("<itemref idref=\"([^\"]+)\"/>"));
    QStringList ids;
    for (auto it = itemref.globalMatch(package); it.hasNext();) {
        ids << it.next().captured(1);
    }
    return ids;
}

}  // namespace

TEST_CASE("EPUB: an archive readers open, with the book's data and its reading order", "[export][epub]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    QImage cover(60, 90, QImage::Format_RGB32);
    cover.fill(QColor(20, 60, 120));
    const QString coverPath = folder.filePath(QStringLiteral("cover.jpg"));
    REQUIRE(cover.save(coverPath, "JPEG"));
    book.epub.coverImage = coverPath;
    book.epub.isbn = QStringLiteral("978-83-01-00000-1");
    book.epub.publisher = QStringLiteral("Wydawnictwo Oaza");
    book.epub.description = QStringLiteral("Karawana & burza.");
    book.epub.modified = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::utc());
    const QString path = folder.filePath(QStringLiteral("Wiatr nad oazą.epub"));
    ExportResult result;
    const QHash<QString, QByteArray> entries = exported(book, path, &result);
    CHECK(result.file == path);
    CHECK(result.images == 2);
    CHECK(result.missingImages == 1);
    CHECK_FALSE(result.coverMissing);

    SECTION("The type of the archive first, stored as it is, without extra fields") {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray head = file.read(58);
        CHECK(head.startsWith("PK\x03\x04"));
        CHECK(head.at(8) == 0);   // stored
        CHECK(head.at(28) == 0);  // no extra field
        CHECK(head.mid(30) == "mimetypeapplication/epub+zip");
        CHECK(text(entries, "META-INF/container.xml").contains(QStringLiteral("full-path=\"OEBPS/content.opf\"")));
    }

    SECTION("Every document is XML, every file of the archive in the manifest") {
        const QString package = text(entries, "OEBPS/content.opf");
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            INFO(it.key().toStdString());
            if (it.key().endsWith(QLatin1String(".xhtml")) || it.key().endsWith(QLatin1String(".opf")) ||
                it.key().endsWith(QLatin1String(".ncx")) || it.key().endsWith(QLatin1String(".xml"))) {
                CHECK(wellFormed(it.value()));
            }
            if (it.key().startsWith(QLatin1String("OEBPS/")) && it.key() != QLatin1String("OEBPS/content.opf")) {
                CHECK(package.contains(QStringLiteral("href=\"") + it.key().mid(6) + QLatin1Char('"')));
            }
        }
    }

    SECTION("The package: the book's data, its identifier, the cover, what it offers to every reader") {
        const QString package = text(entries, "OEBPS/content.opf");
        CHECK(package.contains(QStringLiteral("<dc:identifier id=\"book-id\">urn:isbn:9788301000001</dc:identifier>")));
        CHECK(package.contains(QStringLiteral("<dc:title>Wiatr nad oazą</dc:title>")));
        CHECK(package.contains(QStringLiteral("<dc:creator>Anna Nowak</dc:creator>")));
        CHECK(package.contains(QStringLiteral("<dc:language>pl</dc:language>")));
        CHECK(package.contains(QStringLiteral("<dc:description>Karawana &amp; burza.</dc:description>")));
        CHECK(package.contains(QStringLiteral("<dc:publisher>Wydawnictwo Oaza</dc:publisher>")));
        CHECK(package.contains(QStringLiteral("<meta property=\"dcterms:modified\">2026-11-01T10:00:00Z</meta>")));
        CHECK(package.contains(QStringLiteral("href=\"images/cover.jpg\" media-type=\"image/jpeg\" properties=\"cover-image\"")));
        CHECK(package.contains(QStringLiteral("<meta name=\"cover\" content=\"cover-image\"/>")));
        CHECK(package.contains(QStringLiteral("<meta property=\"schema:accessibilityFeature\">alternativeText</meta>")));
        CHECK(package.contains(QStringLiteral("<meta property=\"schema:accessModeSufficient\">textual</meta>")));
        CHECK(package.contains(QStringLiteral("<meta property=\"schema:accessibilityHazard\">none</meta>")));
        // The cover, the table of contents where the book has it, the part, the chapters
        CHECK(spineOf(package) == QStringList{QStringLiteral("cover"), QStringLiteral("nav"), QStringLiteral("item-002"),
                                              QStringLiteral("item-003"), QStringLiteral("item-004")});
        CHECK(text(entries, "OEBPS/text/cover.xhtml").contains(QStringLiteral("<img src=\"../images/cover.jpg\" alt=\"Wiatr nad oazą\" role=\"doc-cover\"/>")));
    }

    SECTION("The table of contents: the titles and headings, by their levels, for EPUB 3 and EPUB 2") {
        const QString nav = text(entries, "OEBPS/nav.xhtml");
        CHECK(nav.contains(QStringLiteral("<nav epub:type=\"toc\" role=\"doc-toc\" id=\"toc\">")));
        CHECK(nav.contains(QStringLiteral(">Spis treści</h1>")));
        CHECK(nav.contains(QStringLiteral("<li><a href=\"text/item-003.xhtml#h2\">Rozdział 1. Studnia</a>\n<ol>\n"
                                          "<li><a href=\"text/item-003.xhtml#h3\">Section</a></li>\n</ol>\n</li>")));
        CHECK(nav.contains(QStringLiteral("<a epub:type=\"cover\" href=\"text/cover.xhtml\">")));
        CHECK(count(nav, QStringLiteral("<li><a href=")) == 4);
        const QString ncx = text(entries, "OEBPS/toc.ncx");
        CHECK(ncx.contains(QStringLiteral("<meta name=\"dtb:uid\" content=\"urn:isbn:9788301000001\"/>")));
        CHECK(count(ncx, QStringLiteral("<navPoint ")) == 4);
        CHECK(ncx.contains(QStringLiteral("<meta name=\"dtb:depth\" content=\"2\"/>")));
    }
}

TEST_CASE("EPUB: the texts as the reader shows them", "[export][epub]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    book.annotationsAsComments = true;  // no comments in an EPUB all the same
    const QHash<QString, QByteArray> entries = exported(book, folder.filePath(QStringLiteral("book.epub")), nullptr,
                                                        QStringLiteral("sample.epub"));
    const QString chapter = text(entries, "OEBPS/text/item-003.xhtml");
    const QString second = text(entries, "OEBPS/text/item-004.xhtml");

    SECTION("The title of the Navigator; a text with its own heading keeps it") {
        CHECK(chapter.contains(QStringLiteral("<section epub:type=\"chapter\" role=\"doc-chapter\">")));
        CHECK(chapter.contains(QStringLiteral("<h1 id=\"h2\" class=\"s-chapter_title\">Rozdział 1. Studnia</h1>")));
        CHECK(chapter.contains(QStringLiteral("<h2 id=\"h3\" class=\"s-heading_2\">Section</h2>")));
        CHECK(second.contains(QStringLiteral("<h1 id=\"h4\" class=\"s-heading_1\">Its own heading</h1>")));
        CHECK_FALSE(second.contains(QStringLiteral("s-chapter_title")));
        CHECK(text(entries, "OEBPS/text/item-002.xhtml").contains(QStringLiteral("epub:type=\"part\"")));
    }

    SECTION("The formatting of the text, the links the program opens") {
        CHECK(chapter.contains(QStringLiteral("Plain <b>bold</b>, <i>italic</i>, <u>under</u>, <s>strike</s>, H<sub>2</sub>O, x<sup>2</sup>")));
        CHECK(chapter.contains(QStringLiteral("color: #aa0000")));
        CHECK(chapter.contains(QStringLiteral("font-variant: small-caps")));
        CHECK(chapter.contains(QStringLiteral("A line<br/>break, a <a href=\"https://example.com/a?b=1&amp;c=2\">link</a> and not a link.")));
        CHECK_FALSE(chapter.contains(QStringLiteral("javascript")));
        CHECK(chapter.contains(QStringLiteral("<p class=\"s-quote\">A quote.</p>")));
        CHECK(chapter.contains(QStringLiteral("<p class=\"scene-break\">") + sceneBreakText(SceneBreakLook::Asterisks) +
                               QStringLiteral("</p>")));
        CHECK(chapter.contains(QStringLiteral("<div class=\"page-break\"></div>")));
        CHECK_FALSE(chapter.contains(QStringLiteral("Sprawdź datę")));
    }

    SECTION("The notes: shown over the text, listed after it, each with its way back") {
        CHECK(chapter.contains(QStringLiteral("<sup><a id=\"r1\" href=\"#n1\" epub:type=\"noteref\" role=\"doc-noteref\" class=\"noteref\">1</a></sup>")));
        CHECK(chapter.contains(QStringLiteral("<aside id=\"n1\" epub:type=\"footnote\" role=\"doc-footnote\" class=\"note\"><p><a href=\"#r1\" role=\"doc-backlink\">1</a> The <b>first</b> note.</p></aside>")));
        CHECK(chapter.contains(QStringLiteral("<section class=\"endnotes\" epub:type=\"endnotes\" role=\"doc-endnotes\">")));
        CHECK(chapter.contains(QStringLiteral("<li id=\"n2\" epub:type=\"endnote\"><p><a href=\"#r2\" role=\"doc-backlink\">i</a> The end note.</p></li>")));
        // The numbers of the second chapter anew, its ids of the book
        CHECK(second.contains(QStringLiteral("<a id=\"r3\" href=\"#n3\"")));
        CHECK(second.contains(QStringLiteral(">1</a></sup>")));
    }

    SECTION("Lists with their numbers, a table with its header and cells, images with their texts") {
        CHECK(chapter.contains(QStringLiteral("<li value=\"2\"><p class=\"s-normal\">Two</p><ol type=\"a\">\n<li value=\"1\"><p class=\"s-normal\">Two a</p></li>\n</ol>\n</li>")));
        CHECK(chapter.contains(QStringLiteral("<li value=\"7\">")));
        CHECK(chapter.contains(QStringLiteral("<ul>\n<li><p class=\"s-normal\">Bullet</p></li>\n</ul>")));
        CHECK(chapter.contains(QStringLiteral("<colgroup><col style=\"width: 40%\"/><col style=\"width: 60%\"/></colgroup>")));
        CHECK(chapter.contains(QStringLiteral("<thead>\n<tr><th ")));
        CHECK(chapter.contains(QStringLiteral("<td colspan=\"2\" style=\"")));
        CHECK(chapter.contains(QStringLiteral("background-color: #fff2cc")));
        CHECK(chapter.contains(QStringLiteral("<img src=\"../images/image-001.png\" alt=\"Red square\" style=\"width: 50%\"/>")));
        CHECK(chapter.contains(QStringLiteral("alt=\"Framed\"")));
        CHECK_FALSE(chapter.contains(QStringLiteral("Missing")));
        CHECK(entries.contains(QStringLiteral("OEBPS/images/image-002.png")));
    }

    SECTION("The caption's number, the cross-reference and the list of figures go to it") {
        CHECK(chapter.contains(QStringLiteral("Figure <span id=\"cap-c1\">1</span>. Red")));
        CHECK(chapter.contains(QStringLiteral("See figure <a href=\"#cap-c1\">1</a>.")));
        CHECK(chapter.contains(QStringLiteral("<ul class=\"figure-list\">\n<li><a href=\"#cap-c1\">Figure 1. Red</a></li>")));
    }
}

TEST_CASE("EPUB: the look of the book's styles or a simple one", "[export][epub]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    BookExport book = sampleBook(folder.path());
    const QString path = folder.filePath(QStringLiteral("book.epub"));

    SECTION("The book's styles: the font, the sizes of the letters relative to the reader's, the spacing") {
        const QHash<QString, QByteArray> entries = exported(book, path);
        const QString css = text(entries, "OEBPS/styles/book.css");
        CHECK(css.contains(QStringLiteral("body { margin: 0; padding: 0; font-family: \"Literata\", serif }")));
        CHECK(css.contains(QStringLiteral(".s-normal { font-size: 1em; line-height: 1.5; text-align: justify; text-indent: 1.5em }")));
        CHECK(css.contains(QStringLiteral(".s-heading_1 { font-size: 1.667em;")));
        CHECK(css.contains(QStringLiteral(".s-quote { font-size: 1em; line-height: 1.5; font-style: italic; text-align: justify; text-indent: 1.5em; margin-left: 3em }")));
        CHECK(css.contains(QStringLiteral("font-family: \"Courier New\"")));
        CHECK(text(entries, "OEBPS/text/item-003.xhtml").contains(QStringLiteral("font-family: &quot;Georgia&quot;; font-size: 1.167em")));
    }

    SECTION("Simple: the reader's font and size; the indents, the spacing and the colors stay") {
        book.epub.bookStyles = false;
        const QHash<QString, QByteArray> entries = exported(book, path, nullptr, QStringLiteral("simple.epub"));
        const QString css = text(entries, "OEBPS/styles/book.css");
        CHECK(css.contains(QStringLiteral("body { margin: 0; padding: 0 }")));
        CHECK(css.contains(QStringLiteral(".s-normal { text-align: justify; text-indent: 1.5em }")));
        CHECK(css.contains(QStringLiteral(".s-quote { font-style: italic; text-align: justify; text-indent: 1.5em; margin-left: 3em }")));
        CHECK_FALSE(css.contains(QStringLiteral("font-size: 1.667em")));
        CHECK_FALSE(css.contains(QStringLiteral("line-height")));
        CHECK_FALSE(css.contains(QStringLiteral("Courier New")));
        const QString chapter = text(entries, "OEBPS/text/item-003.xhtml");
        CHECK(chapter.contains(QStringLiteral("<span style=\"color: #aa0000; letter-spacing: 0.083em; font-variant: small-caps\">styled</span>")));
        CHECK_FALSE(chapter.contains(QStringLiteral("Georgia")));
    }
}

TEST_CASE("EPUB: the endnotes at the end of the book, the identifier without an ISBN", "[export][epub]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    BookExport book = sampleBook(folder.path());
    const QString path = folder.filePath(QStringLiteral("book.epub"));

    SECTION("The endnotes in a document of their own after the last text") {
        book.endnotesAtBookEnd = true;
        const QHash<QString, QByteArray> entries = exported(book, path);
        CHECK(spineOf(text(entries, "OEBPS/content.opf")).constLast() == QStringLiteral("endnotes"));
        const QString chapter = text(entries, "OEBPS/text/item-003.xhtml");
        CHECK(chapter.contains(QStringLiteral("href=\"endnotes.xhtml#n2\"")));
        CHECK_FALSE(chapter.contains(QStringLiteral("The end note.")));
        const QString endnotes = text(entries, "OEBPS/text/endnotes.xhtml");
        CHECK(endnotes.contains(QStringLiteral("<li id=\"n2\" epub:type=\"endnote\"><p><a href=\"item-003.xhtml#r2\" role=\"doc-backlink\">i</a> The end note.</p></li>")));
    }

    SECTION("The book's own identifier; without one, the same for the same title and author") {
        book.epub.identifier = QStringLiteral("{9c1e4b62-4f4a-4c39-9a51-5f0d1f3c2a10}");
        CHECK(text(exported(book, path), "OEBPS/content.opf")
                  .contains(QStringLiteral(">urn:uuid:9c1e4b62-4f4a-4c39-9a51-5f0d1f3c2a10</dc:identifier>")));
        book.epub.identifier.clear();
        static const QRegularExpression uuid(QStringLiteral(">urn:uuid:([0-9a-f-]{36})</dc:identifier>"));
        const QString first = uuid.match(text(exported(book, path), "OEBPS/content.opf")).captured(1);
        const QString again = uuid.match(text(exported(book, path), "OEBPS/content.opf")).captured(1);
        CHECK_FALSE(first.isEmpty());
        CHECK(first == again);
        book.title = QStringLiteral("Inna książka");
        CHECK(uuid.match(text(exported(book, path), "OEBPS/content.opf")).captured(1) != first);
    }

    SECTION("Without a table of contents in the book: the reader's own, not in the reading order") {
        book.items.erase(book.items.begin());
        const QHash<QString, QByteArray> entries = exported(book, path);
        const QString package = text(entries, "OEBPS/content.opf");
        CHECK_FALSE(spineOf(package).contains(QStringLiteral("nav")));
        CHECK(package.contains(QStringLiteral("properties=\"nav\"")));
        CHECK(text(entries, "OEBPS/nav.xhtml").contains(QStringLiteral("<h1>Wiatr nad oazą</h1>")));
    }

    SECTION("A cover that cannot be read: the book without it") {
        book.epub.coverImage = folder.filePath(QStringLiteral("no-cover.jpg"));
        ExportResult result;
        const QHash<QString, QByteArray> entries = exported(book, path, &result);
        CHECK(result.coverMissing);
        CHECK_FALSE(entries.contains(QStringLiteral("OEBPS/text/cover.xhtml")));
    }
}

TEST_CASE("EPUB: the font of the text in the file, when its licence allows it", "[export][epub]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    BookExport book = sampleBook(folder.path());
    book.epub.embedFont = true;
    const QString path = folder.filePath(QStringLiteral("book.epub"));

    SECTION("A font the computer does not have: the reader's own") {
        book.fontFamily = QStringLiteral("Kalahari No Such Font");
        ExportResult result;
        const QHash<QString, QByteArray> entries = exported(book, path, &result);
        CHECK(result.fontNotEmbedded);
        CHECK_FALSE(text(entries, "OEBPS/content.opf").contains(QStringLiteral("fonts/")));
    }

    SECTION("The font of the computer: a font file a program reads, or none when its licence forbids it") {
        book.fontFamily = QFontInfo(QFont()).family();
        ExportResult result;
        const QHash<QString, QByteArray> entries = exported(book, path, &result);
        QStringList fonts;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            if (it.key().startsWith(QLatin1String("OEBPS/fonts/"))) {
                fonts << it.key();
            }
        }
        INFO(book.fontFamily.toStdString());
        if (result.fontNotEmbedded) {
            CHECK(fonts.isEmpty());
            return;
        }
        REQUIRE_FALSE(fonts.isEmpty());
        const QString css = text(entries, "OEBPS/styles/book.css");
        CHECK(css.contains(QStringLiteral("@font-face { font-family: \"") + book.fontFamily +
                           QStringLiteral("\"; font-weight: normal; font-style: normal; src: url(\"../fonts/")));
        for (const QString& font : std::as_const(fonts)) {
            INFO(font.toStdString());
            const int id = QFontDatabase::addApplicationFontFromData(entries.value(font));
            CHECK(id >= 0);
            CHECK(QFontDatabase::applicationFontFamilies(id).contains(book.fontFamily));
            QFontDatabase::removeApplicationFont(id);
        }
    }
}
