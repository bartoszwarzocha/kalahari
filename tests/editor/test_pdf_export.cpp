// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_pdf_export.cpp
/// @brief Export of a book to PDF: the pages of the page view (BookPages), the running heads
///        and the numbers, the table of contents, and the file with its bookmarks, links,
///        comments and cross-references
///
/// With the environment variable KALAHARI_EXPORT_SAMPLES set to a folder, the files are copied
/// there, to be opened in a PDF reader.

#include "test_support/export_samples.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/page_start.h>
#include <kalahari/editor/book_export.h>
#include <kalahari/editor/book_pages.h>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <vector>

#ifdef KALAHARI_HAS_PDF
#include <kalahari/gui/pdf_document.h>

#include <cstddef>
#endif

using namespace kalahari::editor;
using namespace kalahari::test;
#ifdef KALAHARI_HAS_PDF
using kalahari::gui::PdfContentsEntry;
using kalahari::gui::PdfDocument;
#endif

namespace {

/// A text string of PDF as the writer gives it: UTF-16BE in hex
QByteArray hexText(const QString& text) {
    QByteArray hex("<FEFF");
    for (const QChar ch : text) {
        hex += QByteArray::number(ch.unicode(), 16).rightJustified(4, '0').toUpper();
    }
    return hex + '>';
}

/// The contents of a file
QByteArray fileData(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// The cross-reference sections of a PDF, the last one first (it points to the one before it
/// with /Prev); 0 when an entry of one does not give the offset of its object
int crossReferenceSections(const QByteArray& file) {
    static const QRegularExpression startPattern(
        QStringLiteral("startxref\\s+(\\d+)\\s+%%EOF\\s*$"));
    const QRegularExpressionMatch start = startPattern.match(QString::fromLatin1(file.right(64)));
    if (!start.hasMatch()) {
        return 0;
    }
    static const QRegularExpression subsection(QStringLiteral("^(\\d+) (\\d+)$"));
    static const QRegularExpression previous(QStringLiteral("/Prev (\\d+)"));
    int sections = 0;
    qsizetype at = start.captured(1).toLongLong();
    while (at >= 0) {
        if (!file.mid(at, 5).startsWith("xref\n")) {
            return 0;
        }
        at += 5;
        while (!file.mid(at, 7).startsWith("trailer")) {
            const qsizetype end = file.indexOf('\n', at);
            const QRegularExpressionMatch head =
                subsection.match(QString::fromLatin1(file.mid(at, end - at)).trimmed());
            if (end < 0 || !head.hasMatch()) {
                return 0;
            }
            at = end + 1;
            const int first = head.captured(1).toInt();
            for (int i = 0; i < head.captured(2).toInt(); ++i, at += 20) {
                const QByteArray entry = file.mid(at, 20);
                if (entry.size() == 20 && entry.at(17) == 'n' &&
                    !file.mid(entry.left(10).toLongLong())
                         .startsWith(QByteArray::number(first + i) + " 0 obj")) {
                    return 0;
                }
            }
        }
        ++sections;
        const qsizetype trailerEnd = file.indexOf("startxref", at);
        const QRegularExpressionMatch prev =
            previous.match(QString::fromLatin1(file.mid(at, trailerEnd - at)));
        at = prev.hasMatch() ? prev.captured(1).toLongLong() : -1;
    }
    return sections;
}
#ifdef KALAHARI_HAS_PDF
/// The text of a page of a PDF
QString pageText(const PdfDocument& document, int page) {
    return document.getAllText(page).text();
}

/// @p text without its spaces and line breaks: a reader of PDF puts them by the gaps between
/// the letters, which differ with the fonts of the computer
QString withoutSpaces(QString text) {
    static const QRegularExpression spaces(QStringLiteral("\\s+"),
                                           QRegularExpression::UseUnicodePropertiesOption);
    return text.remove(spaces);
}

/// The entries of the table of contents @p contents one level under its entry @p parent (-1:
/// the entries at the top), as their places in it
std::vector<std::size_t> entriesUnder(const std::vector<PdfContentsEntry>& contents,
                                      int parent) {
    std::vector<std::size_t> entries;
    const auto first = parent < 0 ? std::size_t{0} : static_cast<std::size_t>(parent) + 1;
    const int level = parent < 0 ? 0 : contents[static_cast<std::size_t>(parent)].level + 1;
    for (std::size_t at = first; at < contents.size() && contents[at].level >= level; ++at) {
        if (contents[at].level == level) {
            entries.push_back(at);
        }
    }
    return entries;
}
#endif

}  // namespace

TEST_CASE("PDF: the pages of the page view, with bookmarks, links and comments", "[export][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    book.annotationsAsComments = true;
    book.runningHead = RunningHead::AuthorAndTitle;
    book.page.mirrored = true;
    const QString path = folder.filePath(QStringLiteral("Wiatr nad oazą.pdf"));

    std::vector<int> progress;
    ExportResult result;
    ExportProblem problem;
    REQUIRE(exportPdf(
        book, path, [&progress](int value) { progress.push_back(value); }, &result, &problem));
    keepSample(path);
    CHECK(problem.kind == ExportProblem::Kind::None);
    CHECK(result.file == path);
    CHECK(result.images == 2);
    CHECK(result.missingImages == 1);
    REQUIRE_FALSE(progress.empty());
    CHECK(std::is_sorted(progress.cbegin(), progress.cend()));
    CHECK(progress.back() == 100);

    const QByteArray data = fileData(path);
    SECTION("Qt's file and the update after it, each object where its cross-reference says") {
        CHECK(data.startsWith("%PDF-1.6"));
        CHECK(crossReferenceSections(data) == 2);
    }

    SECTION("The bookmarks of the titles and headings, open; the language of the book") {
        CHECK(data.contains("/PageMode /UseOutlines"));
        CHECK(data.contains("/Type /Outlines"));
        CHECK(data.contains("/Title " + hexText(QStringLiteral("Rozdział 1. Studnia"))));
        CHECK(data.contains("/Title " + hexText(QStringLiteral("Section"))));
        CHECK(data.contains("/Lang (pl)"));
        CHECK_FALSE(data.contains("/PageLayout"));
    }

    SECTION("Links to the headings (the table of contents) and to the caption, the web link Qt's") {
        static const QRegularExpression inside(QStringLiteral(
            "/Subtype /Link\\n/Rect [^\\n]+\\n/Border \\[0 0 0\\]\\n/Dest \\[\\d+ 0 R /XYZ "));
        CHECK(QString::fromLatin1(data).count(inside) == 5);
        CHECK(data.contains("/URI (https://example.com/a?b=1&c=2)"));
        CHECK(data.count("/URI (") == 1);
        CHECK_FALSE(data.contains("javascript"));
    }

    SECTION("The annotations as comments, with their authors, dates and kinds") {
        CHECK(data.count("/Subtype /Text") == 2);
        CHECK(data.contains("/Contents " + hexText(QStringLiteral("Sprawdź datę."))));
        CHECK(data.contains("/Contents " + hexText(QStringLiteral("Zadanie: Opisz pogodę."))));
        CHECK(data.contains("/T " + hexText(QStringLiteral("Anna Nowak"))));
        CHECK(data.contains("/M (D:20261008120000Z)"));
    }

#ifdef KALAHARI_HAS_PDF
    SECTION("A reader shows the pages, the running heads, the numbers and the bookmarks") {
        PdfDocument document(nullptr);
        REQUIRE(document.load(path) == PdfDocument::Error::None);
        // Each on a right page: the contents, the part, the first chapter (two pages), the
        // second; empty left pages before the part and the first chapter
        REQUIRE(document.pageCount() == 7);
        CHECK(document.pagePointSize(0).toSize() == QSize(595, 842));
        const QString contents = pageText(document, 0);
        CHECK(contents.contains(QStringLiteral("Spis treści")));
        CHECK(contents.count(QStringLiteral("Część pierwsza")) == 1);
        static const QRegularExpression contentsPages(QStringLiteral(
            "pierwsza\\s+3\\b[\\s\\S]*Studnia\\s+5\\b[\\s\\S]*Its own heading\\s+7\\b"));
        CHECK(contents.contains(contentsPages));
        CHECK(pageText(document, 1).trimmed().isEmpty());
        // The part's page: its title only, without its number
        CHECK(pageText(document, 2).trimmed() == QStringLiteral("Część pierwsza"));
        CHECK(pageText(document, 3).trimmed().isEmpty());
        // The first page of a text: its number, no running head
        const QString opening = pageText(document, 4);
        INFO(opening.toStdString());
        CHECK(withoutSpaces(opening).contains(QStringLiteral("Rozdział1.Studnia")));
        CHECK_FALSE(opening.contains(QStringLiteral("ANNA NOWAK")));
        CHECK(opening.trimmed().endsWith(QStringLiteral("5")));
        // A left page: the author above the text
        CHECK(pageText(document, 5).contains(QStringLiteral("ANNA NOWAK")));
        CHECK(pageText(document, 5).trimmed().endsWith(QStringLiteral("6")));
        CHECK(pageText(document, 6).contains(QStringLiteral("Its own heading")));

        const std::vector<PdfContentsEntry> bookmarks = document.contents();
        const std::vector<std::size_t> top = entriesUnder(bookmarks, -1);
        REQUIRE(top.size() == 3);
        const PdfContentsEntry& chapter = bookmarks[top[1]];
        CHECK(chapter.title == QStringLiteral("Rozdział 1. Studnia"));
        CHECK(chapter.page == 4);
        const std::vector<std::size_t> sections = entriesUnder(bookmarks, static_cast<int>(top[1]));
        REQUIRE(sections.size() == 1);
        CHECK(bookmarks[sections[0]].title == QStringLiteral("Section"));
        CHECK(bookmarks[top[2]].page == 6);
    }
#endif
}

TEST_CASE("PDF: the purposes of the file and the options", "[export][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    book.annotationsAsComments = true;
    const QString path = folder.filePath(QStringLiteral("book.pdf"));
    ExportResult result;
    ExportProblem problem;

    SECTION("To archive: PDF/A-1b, the annotations printed, the pages in pairs as columns") {
        book.pdf.purpose = PdfPurpose::Archive;
        book.pdf.facingPages = true;
        REQUIRE(exportPdf(book, path, {}, &result, &problem));
        keepSample(path, QStringLiteral("archive.pdf"));
        const QByteArray data = fileData(path);
        CHECK(data.startsWith("%PDF-1.4"));
        CHECK(data.contains("pdfaid:part"));
        CHECK(crossReferenceSections(data) == 2);
        CHECK(data.contains("/PageLayout /TwoColumnRight"));
        // Every annotation the update adds is printed: the links, the comments
        CHECK(data.count("/F 4\n") >= 7);
        // The update keeps the file's identifier
        const qsizetype update = data.lastIndexOf("trailer");
        CHECK(data.mid(update).contains("/ID"));
    }

    SECTION("To read: the pages in pairs; without bookmarks") {
        book.pdf.facingPages = true;
        book.pdf.bookmarks = false;
        REQUIRE(exportPdf(book, path, {}, &result, &problem));
        const QByteArray data = fileData(path);
        CHECK(data.contains("/PageLayout /TwoPageRight"));
        CHECK_FALSE(data.contains("/Outlines"));
        CHECK_FALSE(data.contains("/PageMode"));
        CHECK(data.contains("/Subtype /Text"));
    }

    SECTION("For a print shop: PDF/X-4 without links and comments, with Qt 6.8 or later") {
        book.pdf.purpose = PdfPurpose::PrintShop;
        if (!pdfPurposeSupported(PdfPurpose::PrintShop)) {
            CHECK_FALSE(exportPdf(book, path, {}, &result, &problem));
            CHECK(problem.kind == ExportProblem::Kind::Other);
            CHECK_FALSE(QFile::exists(path));
            return;
        }
        REQUIRE(exportPdf(book, path, {}, &result, &problem));
        keepSample(path, QStringLiteral("print-shop.pdf"));
        const QByteArray data = fileData(path);
        CHECK(data.contains("GTS_PDFX"));
        CHECK_FALSE(data.contains("/Subtype /Link"));
        CHECK_FALSE(data.contains("/Subtype /Text"));
        CHECK_FALSE(data.contains("/URI"));
        CHECK(data.contains("/Outlines"));
    }

    SECTION("Without the comments: no annotation of the text") {
        book.annotationsAsComments = false;
        REQUIRE(exportPdf(book, path, {}, &result, &problem));
        CHECK_FALSE(fileData(path).contains("/Subtype /Text"));
    }
}

TEST_CASE("PDF: the texts on the pages of the book", "[export][pdf]") {
    SECTION(
        "Each text on a new page; with mirror margins on a right page unless it starts on a left "
        "one") {
        ExportItem third = textItem(QStringLiteral("e3"), QStringLiteral("Trzy"), longKml(1));
        third.start = kalahari::core::PageStart::Left;
        BookExport book =
            bookOf({textItem(QStringLiteral("e1"), QStringLiteral("Jeden"), longKml(1)),
                    textItem(QStringLiteral("e2"), QStringLiteral("Dwa"), longKml(1)), third});
        book.page.mirrored = true;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        // Jeden (right), empty, Dwa (right), Trzy (left)
        REQUIRE(pages.pageCount() == 4);
        CHECK(pages.isBlank(1));
        CHECK_FALSE(pages.isBlank(3));
        CHECK(pages.pageAt(QStringLiteral("e2"), 0) == 2);
        CHECK(pages.pageAt(QStringLiteral("e3"), 0) == 3);
        CHECK(pages.pageAt(QStringLiteral("other"), 0) == -1);

        book.page.mirrored = false;
        BookPages plain(book, BookPages::Options{});
        plain.layOut();
        CHECK(plain.pageCount() == 3);
        CHECK_FALSE(plain.isBlank(1));
    }

    SECTION("With mirror margins a text that starts on the next page leaves no page empty") {
        ExportItem second = textItem(QStringLiteral("e2"), QStringLiteral("Dwa"), longKml(1));
        second.start = kalahari::core::PageStart::Next;
        ExportItem third = textItem(QStringLiteral("e3"), QStringLiteral("Trzy"), longKml(1));
        third.start = kalahari::core::PageStart::Next;
        BookExport book =
            bookOf({textItem(QStringLiteral("e1"), QStringLiteral("Jeden"), longKml(1)), second,
                    third, textItem(QStringLiteral("e4"), QStringLiteral("Cztery"), longKml(1))});
        book.page.mirrored = true;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        // Jeden (right), Dwa (left), Trzy (right), empty, Cztery (right)
        REQUIRE(pages.pageCount() == 5);
        CHECK(pages.pageAt(QStringLiteral("e2"), 0) == 1);
        CHECK(pages.pageAt(QStringLiteral("e3"), 0) == 2);
        CHECK(pages.isBlank(3));
        CHECK(pages.pageAt(QStringLiteral("e4"), 0) == 4);
    }

    SECTION("A text goes on after the one before it while its page has room") {
        BookExport book =
            bookOf({textItem(QStringLiteral("e1"), QStringLiteral("Jeden"), longKml(2)),
                    textItem(QStringLiteral("e2"), QStringLiteral("Dwa"), longKml(2)),
                    textItem(QStringLiteral("e3"), QStringLiteral("Trzy"), longKml(40))});
        book.textsOnNewPage = false;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        CHECK(pages.pageAt(QStringLiteral("e2"), 0) == 0);
        CHECK(pages.pageAt(QStringLiteral("e3"), 0) == 0);
        REQUIRE(pages.pageCount() > 1);
        // The headings below one another on the first page
        const std::vector<PageHeading>& headings = pages.headings();
        REQUIRE(headings.size() == 3);
        CHECK(headings[0].place.page == 0);
        CHECK(headings[1].place.page == 0);
        CHECK(headings[1].place.point.y() > headings[0].place.point.y() + 40.0);
        CHECK(headings[2].place.point.y() > headings[1].place.point.y() + 40.0);

        // A page with a footnote at its bottom: the next text starts on a new page
        book.items[0].kml =
            QStringLiteral("<kml><p>Short<footnote id=\"f1\">A note.</footnote>.</p></kml>");
        BookPages noted(book, BookPages::Options{});
        noted.layOut();
        CHECK(noted.pageAt(QStringLiteral("e2"), 0) == 1);
    }

    SECTION("A text without a title of its own: no running head, no number on its pages") {
        ExportItem titlePage;
        titlePage.elementId = QStringLiteral("t");
        titlePage.kml = QStringLiteral("<kml><p align=\"center\">Wiatr nad oazą</p></kml>");
        BookExport book = bookOf(
            {titlePage, textItem(QStringLiteral("e1"), QStringLiteral("Jeden"), longKml(60))});
        book.runningHead = RunningHead::Chapter;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        REQUIRE(pages.pageCount() >= 3);
        CHECK(pages.headings().size() == 1);

        // The top and the bottom margins of a page as drawn: something on them or not
        const auto margins = [&pages](int page) {
            const QSizeF size = pages.pageSize();
            QImage image(size.toSize(), QImage::Format_ARGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            pages.paint(&painter, page);
            painter.end();
            const auto inked = [&image](int top, int bottom) {
                for (int y = top; y < bottom; ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (qGray(image.pixel(x, y)) < 160) {
                            return true;
                        }
                    }
                }
                return false;
            };
            const int margin = static_cast<int>(25.4 / 25.4 * 72.0) - 4;  // 25.4 mm
            return std::make_pair(inked(0, margin), inked(image.height() - margin, image.height()));
        };
        CHECK(margins(0) == std::make_pair(false, false));  // the title page
        CHECK(margins(1) == std::make_pair(false, true));   // the first page of the text
        CHECK(margins(2) == std::make_pair(true, true));    // the next one
    }
}
