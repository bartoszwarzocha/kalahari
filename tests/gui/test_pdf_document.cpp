// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_pdf_document.cpp
/// @brief A PDF file read by PDFium: why a file does not load, the sizes, labels and data of
/// its pages, the pages drawn, their text, the search and the table of contents

#include "kalahari/gui/pdf_document.h"
#include "kalahari/gui/pdf_models.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QList>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>

#include <array>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace gui = kalahari::gui;
using gui::PdfDocument;

namespace {

/// A PDF file of @p objects (numbered from 1, the first the catalog) and @p trailer added to
/// its trailer
QByteArray pdfOf(const QList<QByteArray>& objects, const QByteArray& trailer = {}) {
    QByteArray pdf = "%PDF-1.4\n";
    QList<qsizetype> offsets;
    for (qsizetype i = 0; i < objects.size(); ++i) {
        offsets << pdf.size();
        pdf += QByteArray::number(i + 1) + " 0 obj\n" + objects.at(i) + "\nendobj\n";
    }
    const qsizetype xref = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const qsizetype offset : offsets) {
        pdf += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R " +
           trailer + " >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return pdf;
}

/// @p text as a PDF string in UTF-16 (big-endian, with its byte order mark), in hexadecimal
QByteArray utf16String(const QString& text) {
    QByteArray bytes("\xFE\xFF", 2);
    for (const QChar each : text) {
        bytes.append(static_cast<char>(each.row()));
        bytes.append(static_cast<char>(each.cell()));
    }
    return "<" + bytes.toHex() + ">";
}

/// A stream object of @p content
QByteArray streamOf(const QByteArray& content) {
    return "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content +
           "\nendstream";
}

/// Three pages of 300 by 400 points: "Wells of the desert" at the top of the first, "Oasis"
/// at the top of the second, turned a quarter to the right, and nothing on the third; the
/// pages labeled i, ii and 1; a table of contents that leads back to its first entry; and a
/// title, authors (in UTF-16), a subject and a date in its data
QByteArray atlasPdf() {
    const QByteArray font =
        "/Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> >> >>";
    return pdfOf(
        {QByteArray("<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R ") +
             "/PageLabels << /Nums [0 << /S /r >> 2 << /S /D >>] >> >>",
         "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
         "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] " + font +
             " /Contents 11 0 R >>",
         "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Rotate 90 " + font +
             " /Contents 12 0 R >>",
         "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] >>",
         "<< /Type /Outlines /First 7 0 R /Last 9 0 R /Count 3 >>",
         QByteArray("<< /Title (Wells) /Parent 6 0 R /Next 9 0 R /First 8 0 R /Last 8 0 R ") +
             "/Count 1 /Dest [3 0 R /Fit] >>",
         "<< /Title (Salt  roads) /Parent 7 0 R /A << /S /GoTo /D [4 0 R /Fit] >> >>",
         // Its next entry is the first one again
         "<< /Title (Oases) /Parent 6 0 R /Prev 7 0 R /Next 7 0 R /Dest [5 0 R /Fit] >>",
         "<< /Title (Wells on the Salt Roads) /Author " +
             utf16String(QStringLiteral("Ewa Zielińska; Jan Nowak")) +
             " /Subject (Travels) /CreationDate (D:20190301120000Z) >>",
         streamOf("BT /F1 12 Tf 20 350 Td (Wells of the desert) Tj ET"),
         streamOf("BT /F1 12 Tf 20 350 Td (Oasis) Tj ET")},
        "/Info 10 0 R");
}

/// RC4 of @p data with @p key, as the standard security of PDF files encrypts
QByteArray rc4(const QByteArray& key, const QByteArray& data) {
    std::array<unsigned char, 256> state{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        state.at(i) = static_cast<unsigned char>(i);
    }
    std::size_t j = 0;
    for (std::size_t i = 0; i < state.size(); ++i) {
        const auto keyByte =
            static_cast<unsigned char>(key.at(static_cast<qsizetype>(i) % key.size()));
        j = (j + state.at(i) + keyByte) % state.size();
        std::swap(state.at(i), state.at(j));
    }
    QByteArray result(data.size(), '\0');
    std::size_t i = 0;
    j = 0;
    for (qsizetype n = 0; n < data.size(); ++n) {
        i = (i + 1) % state.size();
        j = (j + state.at(i)) % state.size();
        std::swap(state.at(i), state.at(j));
        const unsigned char stream = state.at((state.at(i) + state.at(j)) % state.size());
        result[n] = static_cast<char>(static_cast<unsigned char>(data.at(n)) ^ stream);
    }
    return result;
}

/// A PDF file of one empty page that opens only with password "camel" (the standard
/// security of PDF 1.1: RC4 with a key of 40 bits)
QByteArray protectedPdf() {
    const QByteArray padding = QByteArray::fromHex(
        "28BF4E5E4E758A4164004E56FFFA01082E2E00B6D0683E802F0CA9FE6453697A");
    const auto padded = [&padding](const QByteArray& password) {
        return (password + padding).left(padding.size());
    };
    const QByteArray id = QByteArray::fromHex("00112233445566778899AABBCCDDEEFF");
    // The owner's entry: the user's password encrypted with the owner's
    const QByteArray ownerKey =
        QCryptographicHash::hash(padded("caravan"), QCryptographicHash::Md5).left(5);
    const QByteArray owner = rc4(ownerKey, padded("camel"));
    // The key of the file, and the user's entry: the padding encrypted with it
    const QByteArray permissions = QByteArray::fromHex("FCFFFFFF");  // -4, low byte first
    const QByteArray key = QCryptographicHash::hash(padded("camel") + owner + permissions + id,
                                                    QCryptographicHash::Md5)
                               .left(5);
    const QByteArray user = rc4(key, padding);
    return pdfOf({"<< /Type /Catalog /Pages 2 0 R >>",
                  "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
                  "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] >>",
                  "<< /Filter /Standard /V 1 /R 2 /Length 40 /O <" + owner.toHex() + "> /U <" +
                      user.toHex() + "> /P -4 >>"},
                 "/Encrypt 4 0 R /ID [<" + id.toHex() + "> <" + id.toHex() + ">]");
}

/// A PDF file of one A5 page without margins, at 72 points to the inch, black from 50 to 150
/// points from its top left corner
QByteArray blackSquarePdf() {
    QBuffer buffer;
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QPdfWriter writer(&buffer);
        writer.setPageSize(QPageSize(QPageSize::A5));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.fillRect(QRect(50, 50, 100, 100), Qt::black);
    }
    return buffer.data();
}

/// @p data as file @p name in @p folder
QString fileOf(const QTemporaryDir& folder, const QString& name, const QByteArray& data) {
    const QString path = folder.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data) == data.size());
    file.close();
    return path;
}

/// Events run until @p done, for 5 seconds at most
bool waitFor(const std::function<bool()>& done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

/// Whether @p color is white
bool isWhite(QRgb color) {
    return qRed(color) == 255 && qGreen(color) == 255 && qBlue(color) == 255 &&
           qAlpha(color) == 255;
}

/// Whether @p color is nearly black
bool isBlack(QRgb color) {
    return qRed(color) < 32 && qGreen(color) < 32 && qBlue(color) < 32 && qAlpha(color) == 255;
}

}  // namespace

TEST_CASE("PDF document: a file it cannot read says why", "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;

    SECTION("No file") {
        CHECK(document.load(folder.filePath(QStringLiteral("missing.pdf"))) ==
              PdfDocument::Error::FileNotFound);
        CHECK(document.load(QString()) == PdfDocument::Error::FileNotFound);
    }
    SECTION("An empty file, and one that is no PDF") {
        CHECK(document.load(fileOf(folder, QStringLiteral("empty.pdf"), {})) ==
              PdfDocument::Error::InvalidFileFormat);
        CHECK(document.load(fileOf(folder, QStringLiteral("notes.pdf"),
                                   "Notes on the caravans, not a PDF file")) ==
              PdfDocument::Error::InvalidFileFormat);
    }
    SECTION("A file protected with a password") {
        CHECK(document.load(fileOf(folder, QStringLiteral("locked.pdf"), protectedPdf())) ==
              PdfDocument::Error::IncorrectPassword);
    }
    CHECK_FALSE(document.isOpen());
    CHECK(document.pageCount() == 0);
    CHECK(document.render(0, QSize(10, 10)).isNull());
    CHECK(document.contents().empty());

    // A file that loads after one that did not
    REQUIRE(document.load(fileOf(folder, QStringLiteral("atlas.pdf"), atlasPdf())) ==
            PdfDocument::Error::None);
    CHECK(document.isOpen());
    CHECK(document.pageCount() == 3);
    document.close();
    CHECK_FALSE(document.isOpen());
    CHECK(document.pageCount() == 0);
}

TEST_CASE("PDF document: the sizes, labels and data of the pages", "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;
    REQUIRE(document.load(fileOf(folder, QStringLiteral("atlas.pdf"), atlasPdf())) ==
            PdfDocument::Error::None);

    // Sizes as the pages are shown: the second one turned
    CHECK(document.pagePointSize(0) == QSizeF(300, 400));
    CHECK(document.pagePointSize(1) == QSizeF(400, 300));
    CHECK(document.pagePointSize(3).isEmpty());
    CHECK(document.pagePointSize(-1).isEmpty());

    CHECK(document.pageLabel(0) == QStringLiteral("i"));
    CHECK(document.pageLabel(1) == QStringLiteral("ii"));
    CHECK(document.pageLabel(2) == QStringLiteral("1"));
    CHECK(document.pageLabel(3).isEmpty());

    CHECK(document.metaData(PdfDocument::MetaDataField::Title) ==
          QStringLiteral("Wells on the Salt Roads"));
    // Written in UTF-16 in the file
    CHECK(document.metaData(PdfDocument::MetaDataField::Author) ==
          QStringLiteral("Ewa Zielińska; Jan Nowak"));
    CHECK(document.metaData(PdfDocument::MetaDataField::Subject) == QStringLiteral("Travels"));
    CHECK(document.metaData(PdfDocument::MetaDataField::CreationDate) ==
          QStringLiteral("D:20190301120000Z"));
}

TEST_CASE("PDF document: a page drawn on white paper, and a part of it as in the whole",
          "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;
    REQUIRE(document.load(fileOf(folder, QStringLiteral("square.pdf"), blackSquarePdf())) ==
            PdfDocument::Error::None);
    const QSizeF points = document.pagePointSize(0);
    REQUIRE_FALSE(points.isEmpty());

    // Two pixels a point
    const QSize size = (points * 2).toSize();
    const QImage page = document.render(0, size);
    REQUIRE(page.size() == size);
    CHECK(isWhite(page.pixel(20, 20)));
    CHECK(isBlack(page.pixel(200, 200)));
    CHECK(isWhite(page.pixel(size.width() - 1, size.height() - 1)));

    // A part across the edge of the square, as the whole page has it
    const QRect part(80, 120, 160, 90);
    const QImage piece = document.render(0, size, part);
    REQUIRE(piece.size() == part.size());
    CHECK(piece == page.copy(part));
    CHECK(document.render(0, QSize()).isNull());
    CHECK(document.render(5, size).isNull());
}

TEST_CASE("PDF document: the text of a page, where it is and what a search finds",
          "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;
    REQUIRE(document.load(fileOf(folder, QStringLiteral("atlas.pdf"), atlasPdf())) ==
            PdfDocument::Error::None);

    const gui::PdfSelection all = document.getAllText(0);
    CHECK(all.text() == QStringLiteral("Wells of the desert"));
    CHECK(all.startIndex() == 0);
    CHECK(all.endIndex() == 19);
    // At the top left of the page, from 20 points of its left edge
    const QRectF bounds = all.boundingRectangle();
    CHECK(bounds.left() > 19.0);
    CHECK(bounds.left() < 22.0);
    CHECK(bounds.top() > 30.0);
    CHECK(bounds.bottom() < 55.0);
    CHECK_FALSE(document.getAllText(2).isValid());

    SECTION("A selection with the mouse, from letter to letter") {
        const double middle = bounds.center().y();
        // "Wells": from the middle of its W to past the middle of its s
        const gui::PdfSelection wells = document.getSelectionAtIndex(0, 0, 5);
        REQUIRE(wells.isValid());
        const QRectF word = wells.boundingRectangle();
        const gui::PdfSelection selected =
            document.getSelection(0, QPointF(word.left() + 1, middle),
                                  QPointF(word.right() - 1, middle));
        CHECK(selected.text() == QStringLiteral("Wells"));
        // From the right to the left, the same
        CHECK(document.getSelection(0, QPointF(word.right() - 1, middle),
                                    QPointF(word.left() + 1, middle))
                  .text() == QStringLiteral("Wells"));
        // Far from the text, nothing
        CHECK_FALSE(document.getSelection(0, QPointF(150, 300), QPointF(250, 380)).isValid());
    }
    SECTION("The characters from an index") {
        const gui::PdfSelection desert = document.getSelectionAtIndex(0, 13, 100);
        CHECK(desert.text() == QStringLiteral("desert"));
        CHECK(desert.startIndex() == 13);
        CHECK(desert.endIndex() == 19);
        CHECK_FALSE(document.getSelectionAtIndex(0, 19, 3).isValid());
        CHECK_FALSE(document.getSelectionAtIndex(0, -1, 3).isValid());
    }
    SECTION("A search in any case, with the words around the hit") {
        const QList<gui::PdfHit> hits = document.find(0, QStringLiteral("OF THE"));
        REQUIRE(hits.size() == 1);
        const gui::PdfHit& hit = hits.first();
        CHECK(hit.page == 0);
        CHECK(hit.startIndex == 6);
        CHECK(hit.length == 6);
        REQUIRE(hit.rectangles.size() == 1);
        CHECK(bounds.contains(hit.rectangles.first().center()));
        CHECK(hit.contextBefore == QStringLiteral("Wells "));
        CHECK(hit.contextAfter == QStringLiteral(" desert"));
        CHECK(document.find(0, QStringLiteral("camel")).isEmpty());
        CHECK(document.find(0, QString()).isEmpty());
    }
    SECTION("A page turned a quarter: the text at its top right") {
        const QList<gui::PdfHit> hits = document.find(1, QStringLiteral("oasis"));
        REQUIRE(hits.size() == 1);
        REQUIRE(hits.first().rectangles.size() == 1);
        const QRectF oasis = hits.first().rectangles.first();
        // 20 points from the left edge of the page, which is now its top
        CHECK(oasis.top() > 15.0);
        CHECK(oasis.top() < 25.0);
        CHECK(oasis.left() > 330.0);
        CHECK(oasis.right() < 400.0);
        // Its letters taken with the mouse where the page shows them
        const QPointF top(oasis.center().x(), oasis.top() + 1);
        const QPointF bottom(oasis.center().x(), oasis.bottom() - 1);
        CHECK_FALSE(document.getSelection(1, top, bottom).text().isEmpty());
    }
}

TEST_CASE("PDF document: the table of contents, also one that leads back", "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;
    REQUIRE(document.load(fileOf(folder, QStringLiteral("atlas.pdf"), atlasPdf())) ==
            PdfDocument::Error::None);

    // Each entry once, followed by the entries under it
    const std::vector<gui::PdfContentsEntry> contents = document.contents();
    REQUIRE(contents.size() == 3);
    CHECK(contents.at(0).title == QStringLiteral("Wells"));
    CHECK(contents.at(0).page == 0);
    CHECK(contents.at(0).level == 0);
    CHECK(contents.at(1).title == QStringLiteral("Salt  roads"));
    CHECK(contents.at(1).page == 1);
    CHECK(contents.at(1).level == 1);
    CHECK(contents.at(2).title == QStringLiteral("Oases"));
    CHECK(contents.at(2).page == 2);
    CHECK(contents.at(2).level == 0);

    // As a tree
    gui::PdfContentsModel model;
    model.setDocument(&document);
    REQUIRE(model.rowCount() == 2);
    const QModelIndex wells = model.index(0, 0);
    CHECK(wells.data().toString() == QStringLiteral("Wells"));
    REQUIRE(model.rowCount(wells) == 1);
    const QModelIndex roads = model.index(0, 0, wells);
    CHECK(roads.data().toString() == QStringLiteral("Salt roads"));
    CHECK(roads.data(static_cast<int>(gui::PdfContentsModel::Role::Page)).toInt() == 1);
    CHECK(model.parent(roads) == wells);
    CHECK_FALSE(model.parent(wells).isValid());
    CHECK(model.index(1, 0).data().toString() == QStringLiteral("Oases"));
    CHECK_FALSE(model.index(2, 0).isValid());
    CHECK_FALSE(model.index(0, 1).isValid());
    model.setDocument(nullptr);
    CHECK(model.rowCount() == 0);
}

TEST_CASE("PDF search: the hits of the pages, found a few pages at a time", "[gui][pdf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    PdfDocument document;
    REQUIRE(document.load(fileOf(folder, QStringLiteral("atlas.pdf"), atlasPdf())) ==
            PdfDocument::Error::None);

    gui::PdfSearchModel model;
    model.setDocument(&document);
    int added = 0;
    QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model,
                     [&added](const QModelIndex&, int first, int last) {
                         added += last - first + 1;
                     });
    model.setSearchString(QStringLiteral("s"));
    CHECK(model.searchString() == QStringLiteral("s"));
    REQUIRE(waitFor([&model]() { return !model.isSearching(); }));
    // "Wells of the desert" has two, "Oasis" two
    REQUIRE(model.rowCount() == 4);
    CHECK(added == 4);
    CHECK(model.resultsOnPage(0).size() == 2);
    CHECK(model.resultsOnPage(1).size() == 2);
    CHECK(model.resultsOnPage(2).isEmpty());
    CHECK(model.resultAtIndex(2).page == 1);
    CHECK(model.resultAtIndex(4).page == -1);
    const QModelIndex first = model.index(0, 0);
    CHECK(first.data(static_cast<int>(gui::PdfSearchModel::Role::Page)).toInt() == 0);
    CHECK(first.data(static_cast<int>(gui::PdfSearchModel::Role::ContextBefore)).toString() ==
          QStringLiteral("Well"));
    CHECK(first.data(static_cast<int>(gui::PdfSearchModel::Role::ContextAfter)).toString() ==
          QStringLiteral(" of the desert"));

    // Another text starts again; none, and no document, find nothing
    model.setSearchString(QStringLiteral("oasis"));
    REQUIRE(waitFor([&model]() { return !model.isSearching(); }));
    CHECK(model.rowCount() == 1);
    model.setSearchString(QString());
    CHECK(model.rowCount() == 0);
    CHECK_FALSE(model.isSearching());
    model.setSearchString(QStringLiteral("wells"));
    model.setDocument(nullptr);
    CHECK(model.rowCount() == 0);
    CHECK_FALSE(model.isSearching());
}
