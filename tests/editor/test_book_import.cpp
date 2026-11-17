// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_import.cpp
/// @brief Import of DOCX, Markdown and plain text into a book: the encodings, the readers, the
///        division into texts and parts, the book's styles, the cleanups and the KML, and the
///        files of the export read back

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/unicode_paths.h>
#include <kalahari/editor/book_export.h>
#include <kalahari/editor/book_import.h>
#include <kalahari/editor/kml_document_model.h>

#include <QBuffer>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <zip.h>

#include <algorithm>

using namespace kalahari::editor;

namespace {

kalahari::core::PackageParagraphStyle packageStyle(const QString& id, const QString& name, const QString& base = {},
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("pl"), name);
    style.baseStyle = base;
    style.properties = properties;
    return style;
}

/// The styles of a novel
ParagraphStyleSet novelStyles() {
    const QString normal = QStringLiteral("normal");
    const QString heading = QStringLiteral("heading_1");
    return ParagraphStyleSet::fromPackage(
        {packageStyle(normal, QStringLiteral("Normalny"), {}, {{QStringLiteral("alignment"), QStringLiteral("justify")}}),
         packageStyle(heading, QStringLiteral("Nagłówek 1"), normal, {{QStringLiteral("alignment"), QStringLiteral("left")}}),
         packageStyle(QStringLiteral("heading_2"), QStringLiteral("Nagłówek 2"), heading),
         packageStyle(QStringLiteral("heading_3"), QStringLiteral("Nagłówek 3"), heading),
         packageStyle(QStringLiteral("quote"), QStringLiteral("Cytat"), normal),
         packageStyle(QStringLiteral("dialogue"), QStringLiteral("Dialog"), normal),
         packageStyle(QStringLiteral("code"), QStringLiteral("Kod"), normal),
         packageStyle(QStringLiteral("caption"), QStringLiteral("Podpis ilustracji"), normal),
         packageStyle(QStringLiteral("footnote"), QStringLiteral("Przypis"), normal),
         packageStyle(QStringLiteral("motto"), QStringLiteral("Motto"), normal),
         packageStyle(QStringLiteral("part_title"), QStringLiteral("Tytuł części"), heading,
                      {{QStringLiteral("alignment"), QStringLiteral("center")}}),
         packageStyle(QStringLiteral("chapter_title"), QStringLiteral("Tytuł rozdziału"), heading,
                      {{QStringLiteral("alignment"), QStringLiteral("center")}})},
        QStringLiteral("pl"));
}

/// Options of the KML with the novel's styles and no cleanups
ImportKmlOptions plainOptions() {
    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    options.cleanup.typography = false;
    options.cleanup.doubleSpaces = false;
    options.cleanup.leadingSpaces = false;
    options.cleanup.sceneMarkers = false;
    options.cleanup.empty = EmptyParagraphs::Keep;
    return options;
}

/// A paragraph of one run of text
ImportBlock paragraph(const QString& text, const QString& style = QStringLiteral("normal"), int heading = 0) {
    ImportBlock block;
    block.style = style;
    block.headingLevel = heading;
    if (!text.isEmpty()) {
        ImportRun run;
        run.text = text;
        block.runs.push_back(run);
    }
    return block;
}

ImportBlock objectBlock(ImportBlock::Kind kind) {
    ImportBlock block;
    block.kind = kind;
    return block;
}

/// The KML of a whole document
QString wholeKml(const ImportDocument& document, const ImportKmlOptions& options) {
    ImportPiece piece;
    piece.from = 0;
    piece.to = document.blocks.size();
    return importKml(document, piece, options);
}

/// Whether KML is read whole by the editor's reader
bool readable(const QString& kml) {
    QXmlStreamReader reader(kml);
    while (!reader.atEnd()) {
        reader.readNext();
    }
    if (reader.hasError()) {
        return false;
    }
    KmlDocumentModel model;
    return model.loadKml(kml);
}

/// A ZIP package of these parts at @p path
void writeZip(const QString& path, const std::vector<std::pair<QString, QByteArray>>& parts) {
    int error = 0;
    zip_t* archive = kalahari::core::openZipFile(kalahari::core::pathOfText(path.toStdString()),
                                                 ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(archive != nullptr);
    std::vector<QByteArray> kept;
    kept.reserve(parts.size());
    for (const auto& [name, data] : parts) {
        kept.push_back(data);
        zip_source_t* source = zip_source_buffer(archive, kept.back().constData(),
                                                 static_cast<zip_uint64_t>(kept.back().size()), 0);
        REQUIRE(source != nullptr);
        REQUIRE(zip_file_add(archive, name.toUtf8().constData(), source, ZIP_FL_OVERWRITE) >= 0);
    }
    REQUIRE(zip_close(archive) == 0);
}

const QByteArray W_NS = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
                        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
                        "xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" "
                        "xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" "
                        "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
                        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                        "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"";

QByteArray part(const char* root, const QByteArray& content) {
    return QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:") + root + ' ' + W_NS + '>' +
           content + "</w:" + root + '>';
}

/// A red PNG of 10 pixels
QByteArray redPng() {
    QImage image(10, 10, QImage::Format_RGB32);
    image.fill(QColor(200, 0, 0));
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

/// A Word document with the parts a test needs: styles, numbering, notes, comments, an image
void writeSampleDocx(const QString& path) {
    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "</Relationships>";
    const QByteArray documentRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rS\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "<Relationship Id=\"rN\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/>"
        "<Relationship Id=\"rF\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes\" Target=\"footnotes.xml\"/>"
        "<Relationship Id=\"rC\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" Target=\"comments.xml\"/>"
        "<Relationship Id=\"rX\" Type=\"http://schemas.microsoft.com/office/2011/relationships/commentsExtended\" Target=\"commentsExtended.xml\"/>"
        "<Relationship Id=\"rI\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/image1.png\"/>"
        "<Relationship Id=\"rL\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" Target=\"https://example.com/\" TargetMode=\"External\"/>"
        "</Relationships>";
    const QByteArray core =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>Szklane miasto</dc:title><dc:creator>Anna Nowak</dc:creator>"
        "<dc:language>pl-PL</dc:language></cp:coreProperties>";
    const QByteArray styles = part("styles",
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Nagwek1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/>"
        "<w:pPr><w:outlineLvl w:val=\"0\"/></w:pPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Nagwek2\"><w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Cytat\"><w:name w:val=\"Quote\"/><w:basedOn w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"MojMotto\"><w:name w:val=\"Motto\"/><w:basedOn w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Wlasny\"><w:name w:val=\"Mój styl\"/><w:basedOn w:val=\"Cytat\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Akapitzlist\"><w:name w:val=\"List Paragraph\"/><w:basedOn w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"character\" w:styleId=\"Uwydatnienie\"><w:name w:val=\"Emphasis\"/><w:rPr><w:i/></w:rPr></w:style>");
    const QByteArray numbering = part("numbering",
        "<w:abstractNum w:abstractNumId=\"0\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/>"
        "<w:lvlText w:val=\"\xEF\x82\xB7\"/></w:lvl><w:lvl w:ilvl=\"1\"><w:numFmt w:val=\"lowerLetter\"/><w:lvlText w:val=\"%2)\"/></w:lvl></w:abstractNum>"
        "<w:abstractNum w:abstractNumId=\"1\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"3\"/><w:numFmt w:val=\"decimal\"/>"
        "<w:lvlText w:val=\"%1.\"/></w:lvl></w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num><w:num w:numId=\"2\"><w:abstractNumId w:val=\"1\"/></w:num>");
    const QByteArray footnotes = part("footnotes",
        "<w:footnote w:type=\"separator\" w:id=\"-1\"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>"
        "<w:footnote w:id=\"1\"><w:p><w:pPr><w:pStyle w:val=\"FootnoteText\"/></w:pPr><w:r><w:footnoteRef/></w:r>"
        "<w:r><w:t xml:space=\"preserve\"> Przypis </w:t></w:r><w:r><w:rPr><w:b/></w:rPr><w:t>gruby</w:t></w:r></w:p></w:footnote>");
    const QByteArray comments = part("comments",
        "<w:comment w:id=\"0\" w:author=\"Ewa\" w:date=\"2026-10-08T12:00:00Z\"><w:p w14:paraId=\"00000001\"><w:r><w:t>Sprawdź to.</w:t></w:r></w:p></w:comment>"
        "<w:comment w:id=\"1\" w:author=\"Anna\" w:date=\"2026-10-09T08:00:00Z\"><w:p w14:paraId=\"00000002\"><w:r><w:t>Sprawdzone.</w:t></w:r></w:p></w:comment>"
        "<w:comment w:id=\"2\" w:author=\"Ewa\"><w:p w14:paraId=\"00000003\"><w:r><w:t>Do zrobienia: dopisać scenę.</w:t></w:r></w:p></w:comment>");
    const QByteArray extended = part("commentsEx",
        "<w15:commentEx w15:paraId=\"00000001\" w15:done=\"1\"/><w15:commentEx w15:paraId=\"00000002\" w15:paraIdParent=\"00000001\" w15:done=\"0\"/>"
        "<w15:commentEx w15:paraId=\"00000003\" w15:done=\"0\"/>");
    const QByteArray drawing =
        "<w:r><w:drawing><wp:inline><wp:extent cx=\"1800000\" cy=\"1800000\"/><wp:docPr id=\"1\" name=\"Obraz 1\" descr=\"Czerwony kwadrat\"/>"
        "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><pic:pic><pic:blipFill>"
        "<a:blip r:embed=\"rI\"/><a:srcRect l=\"10000\" t=\"5000\"/></pic:blipFill><pic:spPr><a:xfrm rot=\"5400000\"/></pic:spPr></pic:pic>"
        "</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>";
    const QByteArray body =
        "<w:body>"
        "<w:sdt><w:sdtPr><w:docPartObj><w:docPartGallery w:val=\"Table of Contents\"/></w:docPartObj></w:sdtPr>"
        "<w:sdtContent><w:p><w:r><w:t>Spis treści</w:t></w:r></w:p></w:sdtContent></w:sdt>"
        "<w:p><w:r><w:t>Szklane miasto</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Nagwek1\"/></w:pPr><w:r><w:t>Rozdział 1</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:jc w:val=\"center\"/><w:ind w:firstLine=\"360\"/></w:pPr>"
        "<w:r><w:rPr><w:b/></w:rPr><w:t xml:space=\"preserve\">Gruby </w:t></w:r>"
        "<w:r><w:rPr><w:rStyle w:val=\"Uwydatnienie\"/></w:rPr><w:t>kursywa</w:t></w:r>"
        "<w:r><w:t xml:space=\"preserve\"> </w:t></w:r>"
        "<w:r><w:rPr><w:rFonts w:ascii=\"Georgia\"/><w:sz w:val=\"28\"/><w:color w:val=\"AA0000\"/></w:rPr><w:t>kolor</w:t></w:r>"
        "<w:r><w:t xml:space=\"preserve\"> i </w:t></w:r>"
        "<w:hyperlink r:id=\"rL\"><w:r><w:t>łącze</w:t></w:r></w:hyperlink>"
        "<w:r><w:footnoteReference w:id=\"1\"/></w:r>"
        "<w:r><w:br/><w:t>wiersz</w:t><w:tab/><w:t>tab</w:t></w:r>"
        "<w:r><w:rPr><w:vanish/></w:rPr><w:t>ukryty</w:t></w:r>"
        "</w:p>"
        "<w:p><w:commentRangeStart w:id=\"0\"/><w:r><w:t>Komentarz</w:t></w:r><w:commentRangeEnd w:id=\"0\"/>"
        "<w:r><w:commentReference w:id=\"0\"/></w:r><w:commentRangeStart w:id=\"1\"/><w:commentRangeEnd w:id=\"1\"/>"
        "<w:commentRangeStart w:id=\"2\"/><w:commentRangeEnd w:id=\"2\"/><w:r><w:commentReference w:id=\"2\"/></w:r>"
        "<w:ins w:id=\"5\" w:author=\"Ewa\"><w:r><w:t xml:space=\"preserve\"> nowe</w:t></w:r></w:ins>"
        "<w:del w:id=\"6\" w:author=\"Ewa\"><w:r><w:delText xml:space=\"preserve\"> stare</w:delText></w:r></w:del></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Akapitzlist\"/><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr><w:r><w:t>punkt</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Akapitzlist\"/><w:numPr><w:ilvl w:val=\"1\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr><w:r><w:t>podpunkt</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"2\"/></w:numPr></w:pPr><w:r><w:t>trzy</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Cytat\"/></w:pPr><w:r><w:t>Cytat.</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Wlasny\"/></w:pPr><w:r><w:t>Własny.</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"MojMotto\"/></w:pPr><w:r><w:t>Motto.</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>Przed</w:t></w:r><w:r><w:br w:type=\"page\"/></w:r><w:r><w:t>po stronie</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\"/></w:pBdr></w:pPr></w:p>"
        "<w:tbl><w:tblPr><w:jc w:val=\"center\"/></w:tblPr><w:tblGrid><w:gridCol w:w=\"2000\"/><w:gridCol w:w=\"6000\"/></w:tblGrid>"
        "<w:tr><w:trPr><w:tblHeader/></w:trPr><w:tc><w:p><w:r><w:t>A</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>B</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"restart\"/><w:shd w:fill=\"FFF2CC\"/></w:tcPr><w:p><w:r><w:t>dwa wiersze</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>1</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:vMerge/></w:tcPr><w:p/></w:tc><w:tc><w:p><w:r><w:t>2</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:gridSpan w:val=\"2\"/></w:tcPr><w:p><w:r><w:t>szeroka</w:t></w:r></w:p></w:tc></w:tr></w:tbl>"
        "<w:p><w:r><w:t xml:space=\"preserve\">Strona </w:t></w:r><w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
        "<w:r><w:instrText xml:space=\"preserve\"> PAGE </w:instrText></w:r><w:r><w:fldChar w:fldCharType=\"separate\"/></w:r>"
        "<w:r><w:t>7</w:t></w:r><w:r><w:fldChar w:fldCharType=\"end\"/></w:r></w:p>"
        "<w:p>" + drawing + "</w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Nagwek1\"/></w:pPr><w:r><w:t>Epilog</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>Koniec.</w:t></w:r></w:p>"
        "<w:sectPr><w:footerReference w:type=\"default\" r:id=\"rZ\"/></w:sectPr>"
        "</w:body>";
    writeZip(path, {{QStringLiteral("_rels/.rels"), rels},
                    {QStringLiteral("docProps/core.xml"), core},
                    {QStringLiteral("word/_rels/document.xml.rels"), documentRels},
                    {QStringLiteral("word/document.xml"), part("document", body)},
                    {QStringLiteral("word/styles.xml"), styles},
                    {QStringLiteral("word/numbering.xml"), numbering},
                    {QStringLiteral("word/footnotes.xml"), footnotes},
                    {QStringLiteral("word/comments.xml"), comments},
                    {QStringLiteral("word/commentsExtended.xml"), extended},
                    {QStringLiteral("word/media/image1.png"), redPng()}});
}

/// The texts of the runs of a block, joined
QString textOf(const ImportBlock& block) {
    return importPlainText(block);
}

}  // namespace

// =============================================================================
// Encodings
// =============================================================================

TEST_CASE("Import: the encodings of plain text", "[import][text]") {
    const QString polish = QStringLiteral("Zażółć gęślą jaźń. Śląsk, Łódź");
    // As Python's codecs cp1250 and iso8859_2 write it
    const QByteArray windows("Za\xBF\xF3\xB3\xE6 g\xEA\x9Cl\xB9 ja\x9F\xF1. \x8Cl\xB9sk, \xA3\xF3" "d\x9F");
    const QByteArray latin2("Za\xBF\xF3\xB3\xE6 g\xEA\xB6l\xB1 ja\xBC\xF1. \xA6l\xB1sk, \xA3\xF3" "d\xBC");

    SECTION("Windows-1250 and ISO 8859-2 are told apart by their letters") {
        CHECK(detectTextEncoding(windows) == TextEncoding::Windows1250);
        CHECK(detectTextEncoding(latin2) == TextEncoding::Iso8859_2);
        CHECK(decodeText(windows, TextEncoding::Windows1250) == polish);
        CHECK(decodeText(latin2, TextEncoding::Iso8859_2) == polish);
        CHECK(decodeText(QByteArray("\x84" "cytat\x94\x85 \x96"), TextEncoding::Windows1250) == QStringLiteral("„cytat”… –"));
    }

    SECTION("UTF-8, with and without its byte order mark, and UTF-16") {
        const QByteArray utf8 = polish.toUtf8();
        CHECK(detectTextEncoding(utf8) == TextEncoding::Utf8);
        CHECK(detectTextEncoding(QByteArray("\xEF\xBB\xBF") + utf8) == TextEncoding::Utf8);
        CHECK(decodeText(QByteArray("\xEF\xBB\xBF") + utf8 + "\r\nb\rc", TextEncoding::Utf8) ==
              polish + QStringLiteral("\nb\nc"));

        QByteArray little("\xFF\xFE");
        QByteArray big;
        QByteArray bare;
        for (const QChar ch : QStringLiteral("Łódź\n")) {
            little.append(static_cast<char>(ch.unicode() & 0xFF)).append(static_cast<char>(ch.unicode() >> 8));
            big.append(static_cast<char>(ch.unicode() >> 8)).append(static_cast<char>(ch.unicode() & 0xFF));
        }
        bare = little.mid(2);
        CHECK(detectTextEncoding(little) == TextEncoding::Utf16);
        CHECK(decodeText(little, TextEncoding::Utf16) == QStringLiteral("Łódź\n"));
        CHECK(decodeText(QByteArray("\xFE\xFF") + big, TextEncoding::Utf16) == QStringLiteral("Łódź\n"));
        CHECK(detectTextEncoding(QByteArray("a\0b\0c\0d\0", 8)) == TextEncoding::Utf16);
        CHECK(decodeText(bare, TextEncoding::Utf16) == QStringLiteral("Łódź\n"));
    }
}

// =============================================================================
// Plain text
// =============================================================================

TEST_CASE("Import: plain text, its paragraphs and titles", "[import][text]") {
    SECTION("Each line is a paragraph, an empty line an empty paragraph, a form feed a page break") {
        const ImportDocument document = readPlainText(QStringLiteral("\nJeden\n\tDwa\n\nTrzy\fCztery"), {});
        REQUIRE(document.blocks.size() == 6);
        CHECK(textOf(document.blocks[0]) == QStringLiteral("Jeden"));
        CHECK(document.blocks[1].runs.front().kind == ImportRun::Kind::Tab);
        CHECK(document.blocks[2].runs.empty());
        CHECK(textOf(document.blocks[3]) == QStringLiteral("Trzy"));
        CHECK(document.blocks[4].kind == ImportBlock::Kind::PageBreak);
        CHECK(textOf(document.blocks[5]) == QStringLiteral("Cztery"));
        REQUIRE(document.styles.size() == 1);
        CHECK(document.styles.front().key == QStringLiteral("normal"));
        CHECK(document.styles.front().paragraphs == 5);
    }

    SECTION("Paragraphs separated by empty lines join their lines") {
        const QString text = QStringLiteral("Pierwszy wiersz\nciąg dalszy.\n\nDrugi.\n\n\nTrzeci.\n");
        CHECK(paragraphsByEmptyLines(text));
        CHECK_FALSE(paragraphsByEmptyLines(QStringLiteral("a\nb\nc\n\nd")));
        TextReadOptions options;
        options.emptyLineParagraphs = true;
        const ImportDocument document = readPlainText(text, options);
        REQUIRE(document.blocks.size() == 4);
        CHECK(textOf(document.blocks[0]) == QStringLiteral("Pierwszy wiersz ciąg dalszy."));
        CHECK(textOf(document.blocks[1]) == QStringLiteral("Drugi."));
        CHECK(document.blocks[2].runs.empty());  // the second empty line
        CHECK(textOf(document.blocks[3]) == QStringLiteral("Trzeci."));
    }

    SECTION("The titles of texts and parts") {
        TextReadOptions options;
        options.titles = TitlePattern::Polish;
        options.parts = PartPattern::Polish;
        options.kindTitles = {QStringLiteral("Prolog")};
        const ImportDocument document = readPlainText(
            QStringLiteral("PROLOG\nTekst.\nCzęść pierwsza\nRozdział 1. Huta\nTekst.\nRozdział II\nRozdział dwudziesty pierwszy\n"
                           "Rozdziały są dwa.\nCzęść II – Miasto\nRozdziałek 3"),
            options);
        std::vector<int> levels;
        levels.reserve(document.blocks.size());
        for (const ImportBlock& block : document.blocks) {
            levels.push_back(block.headingLevel);
        }
        CHECK(levels == std::vector<int>{2, 0, 1, 2, 0, 2, 2, 0, 1, 0});
        CHECK(document.blocks[3].style == QStringLiteral("heading_2"));

        TextReadOptions english;
        english.titles = TitlePattern::English;
        const ImportDocument chapters =
            readPlainText(QStringLiteral("Chapter One\nChapter 12: Fire\nChapter Twenty-Two\nChapters"), english);
        CHECK(chapters.blocks[0].headingLevel == 1);  // no parts: the texts' titles are of level 1
        CHECK(chapters.blocks[1].headingLevel == 1);
        CHECK(chapters.blocks[2].headingLevel == 1);
        CHECK(chapters.blocks[3].headingLevel == 0);

        TextReadOptions numbers;
        numbers.titles = TitlePattern::Number;
        const ImportDocument numbered = readPlainText(QStringLiteral("1\nIV.\n12. To nie\nXIIV"), numbers);
        CHECK(numbered.blocks[0].headingLevel == 1);
        CHECK(numbered.blocks[1].headingLevel == 1);
        CHECK(numbered.blocks[2].headingLevel == 0);
        CHECK(numbered.blocks[3].headingLevel == 0);

        TextReadOptions capitals;
        capitals.titles = TitlePattern::Capitals;
        const ImportDocument caps = readPlainText(QStringLiteral("BURZA NAD MORZEM\nZwykły wiersz\nA\n123"), capitals);
        CHECK(caps.blocks[0].headingLevel == 1);
        CHECK(caps.blocks[1].headingLevel == 0);
        CHECK(caps.blocks[2].headingLevel == 0);
        CHECK(caps.blocks[3].headingLevel == 0);

        TextReadOptions prefix;
        prefix.titles = TitlePattern::Prefix;
        prefix.prefix = QStringLiteral("Odcinek");
        const ImportDocument prefixed = readPlainText(QStringLiteral("odcinek 5\nODCINEK specjalny\nOdcinki"), prefix);
        CHECK(prefixed.blocks[0].headingLevel == 1);
        CHECK(prefixed.blocks[1].headingLevel == 1);  // any letter case
        CHECK(prefixed.blocks[2].headingLevel == 0);  // a line that does not start with it
    }

    SECTION("Empty lines start texts") {
        TextReadOptions options;
        options.emptyLinesForText = 3;
        const ImportDocument document = readPlainText(QStringLiteral("Jeden\n\nDwa\n\n\n\nTrzy"), options);
        REQUIRE(document.blocks.size() == 4);
        CHECK_FALSE(document.blocks[0].startsText);
        CHECK(document.blocks[1].runs.empty());
        CHECK_FALSE(document.blocks[2].startsText);
        CHECK(document.blocks[3].startsText);
        CHECK(suggestedSplit(document).mode == SplitMode::Marks);
        const std::vector<ImportPiece> pieces = splitDocument(document, suggestedSplit(document));
        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0].title == QStringLiteral("Jeden"));
        CHECK(pieces[1].title == QStringLiteral("Trzy"));
        CHECK(pieces[1].words == 1);
    }
}

// =============================================================================
// Texts and parts
// =============================================================================

TEST_CASE("Import: the texts and parts of a document", "[import][split]") {
    ImportDocument document;
    document.blocks = {paragraph(QStringLiteral("Szklane miasto")),             // 0
                       paragraph(QStringLiteral("Prolog"), {}, 1),               // 1
                       paragraph(QStringLiteral("Noc.")),                         // 2
                       paragraph(QStringLiteral("Część pierwsza. Szkło"), {}, 1), // 3
                       paragraph(QStringLiteral("Motto części.")),                // 4
                       paragraph(QStringLiteral("Rozdział 1"), {}, 2),           // 5
                       paragraph(QStringLiteral("Raz dwa trzy.")),                // 6
                       paragraph(QStringLiteral("Scena"), {}, 3),                 // 7
                       paragraph(QStringLiteral("Cztery.")),                      // 8
                       paragraph(QStringLiteral("Rozdział 2"), {}, 2),           // 9
                       paragraph(QStringLiteral("Epilog"), {}, 1),               // 10
                       paragraph(QStringLiteral("Koniec.")),                      // 11
                       paragraph(QStringLiteral("Pusty"), {}, 1)};               // 12

    SECTION("The headings and the division they suggest") {
        CHECK(headingCounts(document) == std::map<int, int>{{1, 4}, {2, 2}, {3, 1}});
        const SplitOptions suggested = suggestedSplit(document);
        CHECK(suggested.mode == SplitMode::Headings);
        CHECK(suggested.partLevel == 1);
        CHECK(suggested.textLevel == 2);
    }

    SECTION("Parts of the first level with the texts of the second under them") {
        SplitOptions options;
        options.partLevel = 1;
        options.textLevel = 2;
        const std::vector<ImportPiece> pieces = splitDocument(document, options);
        REQUIRE(pieces.size() == 8);
        CHECK(pieces[0].beforeTitles);
        CHECK(pieces[0].title.isEmpty());
        CHECK(pieces[0].to == 1);
        CHECK(pieces[1].title == QStringLiteral("Prolog"));
        CHECK_FALSE(pieces[1].inPart);
        CHECK(pieces[1].from == 2);  // without its heading
        CHECK(pieces[2].part);
        CHECK(pieces[2].title == QStringLiteral("Część pierwsza. Szkło"));
        CHECK(pieces[3].title == QStringLiteral("Część pierwsza. Szkło"));  // the part's own text
        CHECK(pieces[3].inPart);
        CHECK(pieces[3].from == 4);
        CHECK(pieces[3].to == 5);
        CHECK(pieces[4].title == QStringLiteral("Rozdział 1"));
        CHECK(pieces[4].inPart);
        CHECK(pieces[4].to == 9);  // a heading of the third level stays in it
        CHECK(pieces[4].words == 5);
        CHECK(pieces[5].title == QStringLiteral("Rozdział 2"));  // a text of a heading alone
        CHECK(pieces[5].from == pieces[5].to);
        CHECK(pieces[6].title == QStringLiteral("Epilog"));
        CHECK_FALSE(pieces[6].inPart);  // a heading of a part without texts ends the part
        CHECK(pieces[7].title == QStringLiteral("Pusty"));
    }

    SECTION("The heading stays in its text") {
        SplitOptions options;
        options.textLevel = 1;
        options.headingAsTitle = false;
        const std::vector<ImportPiece> pieces = splitDocument(document, options);
        REQUIRE(pieces.size() == 5);
        CHECK(pieces[1].from == 1);
        CHECK(pieces[2].title == QStringLiteral("Część pierwsza. Szkło"));
        CHECK(pieces[2].to == 10);
    }

    SECTION("At the page breaks, or whole") {
        ImportDocument pages;
        pages.blocks = {paragraph(QStringLiteral("Tytuł książki")), objectBlock(ImportBlock::Kind::PageBreak),
                        paragraph(QStringLiteral("Rozdział"), {}, 1), paragraph(QStringLiteral("Tekst rozdziału.")),
                        objectBlock(ImportBlock::Kind::PageBreak), objectBlock(ImportBlock::Kind::PageBreak),
                        paragraph(QStringLiteral("Bardzo długi pierwszy akapit tekstu, który nie ma nagłówka i jest dłuższy niż tytuł."))};
        SplitOptions options;
        options.mode = SplitMode::PageBreaks;
        const std::vector<ImportPiece> pieces = splitDocument(pages, options);
        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0].beforeTitles);
        CHECK(pieces[0].title == QStringLiteral("Tytuł książki"));
        CHECK(pieces[1].title == QStringLiteral("Rozdział"));
        CHECK(pieces[1].from == 3);
        CHECK(pieces[2].title == QStringLiteral("Bardzo długi pierwszy akapit tekstu, który nie ma nagłówka i…"));
        CHECK(pieces[2].from == 6);  // the paragraph is not a title: it stays

        options.mode = SplitMode::Whole;
        const std::vector<ImportPiece> whole = splitDocument(pages, options);
        REQUIRE(whole.size() == 1);
        CHECK(whole[0].title.isEmpty());
        CHECK(whole[0].words == 19);
    }

    SECTION("The titles of parts") {
        for (const char* title : {"Część pierwsza", "CZĘŚĆ II. Miasto", "Księga 3", "Tom drugi", "Part One", "Book IV",
                                  "Volume 2: Fire"}) {
            INFO(title);
            CHECK(isPartTitle(QString::fromUtf8(title)));
        }
        for (const char* title : {"Częstochowa", "Tomasz", "Booking", "Część", "Rozdział 1", "Partia szachów"}) {
            INFO(title);
            CHECK_FALSE(isPartTitle(QString::fromUtf8(title)));
        }
    }

    SECTION("Chapters with the headings of their scenes are no parts") {
        ImportDocument scenes;
        scenes.blocks = {paragraph(QStringLiteral("Wiosna"), {}, 1), paragraph(QStringLiteral("Scena w ogrodzie"), {}, 2),
                         paragraph(QStringLiteral("Tekst.")), paragraph(QStringLiteral("Lato"), {}, 1),
                         paragraph(QStringLiteral("Scena nad wodą"), {}, 2), paragraph(QStringLiteral("Tekst."))};
        const SplitOptions suggested = suggestedSplit(scenes);
        CHECK(suggested.partLevel == 0);
        CHECK(suggested.textLevel == 1);
        scenes.fromKalahari = true;  // the program's own: the headings of the first level are its parts
        CHECK(suggestedSplit(scenes).partLevel == 1);
    }

    SECTION("The kinds of the titles") {
        const std::vector<KindTitle> kinds{{QStringLiteral("prologue"), {QStringLiteral("Prolog"), QStringLiteral("Prologue")}},
                                           {QStringLiteral("acknowledgments"), {QStringLiteral("Podziękowania")}}};
        CHECK(kindOfTitle(QStringLiteral("Prolog"), kinds) == QStringLiteral("prologue"));
        CHECK(kindOfTitle(QStringLiteral("  prolog. Noc nad miastem"), kinds) == QStringLiteral("prologue"));
        CHECK(kindOfTitle(QStringLiteral("PROLOGUE"), kinds) == QStringLiteral("prologue"));
        CHECK(kindOfTitle(QStringLiteral("Prologi"), kinds).isEmpty());
        CHECK(kindOfTitle(QStringLiteral("Podziękowania"), kinds) == QStringLiteral("acknowledgments"));
        CHECK(kindOfTitle(QStringLiteral("Rozdział 1"), kinds).isEmpty());
    }
}

// =============================================================================
// Styles and KML
// =============================================================================

TEST_CASE("Import: the book's styles of the file's styles", "[import][styles]") {
    ImportDocument document;
    const auto addStyle = [&document](const QString& key, const QString& name, const QString& base = {}, int level = 0) {
        ImportStyle style;
        style.key = key;
        style.name = name;
        style.basedOn = base;
        style.headingLevel = level;
        document.styles.push_back(style);
    };
    addStyle(QStringLiteral("Normal"), QStringLiteral("Normal"));
    addStyle(QStringLiteral("Nagwek1"), QStringLiteral("heading 1"), {}, 1);
    addStyle(QStringLiteral("Nagwek5"), QStringLiteral("heading 5"), {}, 5);
    addStyle(QStringLiteral("Cytat"), QStringLiteral("Quote"));
    addStyle(QStringLiteral("Dialogi"), QStringLiteral("dialog"));
    addStyle(QStringLiteral("ChapterTitle"), QStringLiteral("Chapter title"));
    addStyle(QStringLiteral("Mine"), QStringLiteral("Mój"), QStringLiteral("Cytat"));
    addStyle(QStringLiteral("Other"), QStringLiteral("Inny"));
    addStyle(QStringLiteral("quote"), QStringLiteral("quote"));
    const ParagraphStyleSet book = novelStyles();
    CHECK(suggestedBookStyle(document, QStringLiteral("Normal"), book) == QStringLiteral("normal"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Nagwek1"), book) == QStringLiteral("heading_1"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Nagwek5"), book) == QStringLiteral("heading_3"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Cytat"), book) == QStringLiteral("quote"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Dialogi"), book) == QStringLiteral("dialogue"));
    CHECK(suggestedBookStyle(document, QStringLiteral("ChapterTitle"), book) == QStringLiteral("chapter_title"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Mine"), book) == QStringLiteral("quote"));
    CHECK(suggestedBookStyle(document, QStringLiteral("Other"), book) == QStringLiteral("normal"));
    CHECK(suggestedBookStyle(document, QStringLiteral("quote"), book) == QStringLiteral("quote"));
}

TEST_CASE("Import: the KML of a text", "[import][kml]") {
    ImportDocument document;
    ImportBlock rich = paragraph(QString());
    rich.align = QStringLiteral("center");
    rich.own.firstLineIndent = 18.0;
    ImportRun bold;
    bold.text = QStringLiteral("Gruby ");
    bold.format.bold = true;
    ImportRun styled;
    styled.text = QStringLiteral("kolor");
    styled.format.italic = true;
    styled.format.font = QStringLiteral("Georgia");
    styled.format.size = 14;
    styled.format.color = QStringLiteral("#aa0000");
    ImportRun link;
    link.text = QStringLiteral(" łącze & <znak>");
    link.link = QStringLiteral("https://example.com/?a=1&b=2");
    ImportRun script;
    script.text = QStringLiteral(" skrypt");
    script.link = QStringLiteral("javascript:alert(1)");
    ImportRun lineBreak;
    lineBreak.kind = ImportRun::Kind::LineBreak;
    ImportRun after;
    after.text = QStringLiteral("dalej");
    rich.runs = {bold, styled, link, script, lineBreak, after};
    document.blocks.push_back(rich);

    SECTION("Emphasis and alignment, by default") {
        const QString kml = wholeKml(document, plainOptions());
        CHECK(kml == QStringLiteral("<kml><p align=\"center\"><b>Gruby </b><i>kolor</i>"
                                    "<a href=\"https://example.com/?a=1&amp;b=2\"> łącze &amp; &lt;znak&gt;</a> skrypt<br/>dalej</p></kml>"));
        CHECK(readable(kml));
    }

    SECTION("The left and justified alignment of the body text is the style's") {
        ImportDocument aligned;
        aligned.blocks = {paragraph(QStringLiteral("Lewo")), paragraph(QStringLiteral("Obustronnie")),
                          paragraph(QStringLiteral("Prawo"))};
        aligned.blocks[0].align = QStringLiteral("left");
        aligned.blocks[1].align = QStringLiteral("justify");
        aligned.blocks[2].align = QStringLiteral("right");
        ImportKmlOptions options = plainOptions();
        CHECK(wholeKml(aligned, options) == QStringLiteral("<kml><p>Lewo</p><p>Obustronnie</p><p align=\"right\">Prawo</p></kml>"));
        options.formatting = ImportFormatting::All;
        CHECK(wholeKml(aligned, options) ==
              QStringLiteral("<kml><p align=\"left\">Lewo</p><p>Obustronnie</p><p align=\"right\">Prawo</p></kml>"));
    }

    SECTION("All the formatting, or only the styles") {
        ImportKmlOptions options = plainOptions();
        options.formatting = ImportFormatting::All;
        const QString all = wholeKml(document, options);
        CHECK(all.contains(QStringLiteral("<p align=\"center\" indent-first=\"18\">")));
        CHECK(all.contains(QStringLiteral("<i font=\"Georgia\" size=\"14\" color=\"#aa0000\">kolor</i>")));
        CHECK(readable(all));
        options.formatting = ImportFormatting::StylesOnly;
        CHECK(wholeKml(document, options) ==
              QStringLiteral("<kml><p>Gruby kolor<a href=\"https://example.com/?a=1&amp;b=2\"> łącze &amp; &lt;znak&gt;</a> skrypt<br/>dalej</p></kml>"));
    }

    SECTION("Headings, styles of the book and the choices of the writer") {
        ImportDocument withStyles;
        ImportStyle heading;
        heading.key = QStringLiteral("H");
        heading.name = QStringLiteral("heading 2");
        heading.headingLevel = 2;
        ImportStyle mine;
        mine.key = QStringLiteral("M");
        mine.name = QStringLiteral("Motto");
        withStyles.styles = {heading, mine};
        withStyles.blocks = {paragraph(QStringLiteral("Tytuł"), QStringLiteral("H")),
                             paragraph(QStringLiteral("Motto"), QStringLiteral("M")),
                             paragraph(QStringLiteral("Zarys"), QStringLiteral("Normal"), 4),
                             paragraph(QStringLiteral("Ręczny"), QStringLiteral("M"))};
        withStyles.blocks[3].style = QStringLiteral("X");
        ImportKmlOptions options = plainOptions();
        CHECK(wholeKml(withStyles, options) == QStringLiteral("<kml><h2>Tytuł</h2><p style=\"motto\">Motto</p><h3>Zarys</h3><p>Ręczny</p></kml>"));
        options.styles.insert(QStringLiteral("H"), QStringLiteral("chapter_title"));
        options.styles.insert(QStringLiteral("M"), QStringLiteral("quote"));
        CHECK(wholeKml(withStyles, options) ==
              QStringLiteral("<kml><p style=\"chapter_title\">Tytuł</p><p style=\"quote\">Motto</p><h3>Zarys</h3><p>Ręczny</p></kml>"));
    }

    SECTION("Lists, tables, lines and breaks") {
        ImportDocument blocks;
        ListDefinition bullets = ListDefinition::bulleted();
        bullets.levels[1] = ListDefinition::numbered().levels[1];
        blocks.lists = {bullets};
        const auto item = [](const QString& text, int level) {
            ImportBlock block = paragraph(text);
            block.list = 0;
            block.listLevel = level;
            return block;
        };
        auto table = std::make_shared<ImportTable>();
        table->columns = {1000.0, 3000.0};
        ImportRow header;
        header.header = true;
        header.cells.resize(2);
        header.cells[0].blocks = {paragraph(QStringLiteral("A"))};
        header.cells[1].blocks = {paragraph(QStringLiteral("B"))};
        ImportRow row;
        row.cells.resize(1);
        row.cells[0].columnSpan = 2;
        row.cells[0].background = QStringLiteral("#fff2cc");
        table->rows = {header, row};
        ImportBlock tableBlock = objectBlock(ImportBlock::Kind::Table);
        tableBlock.table = table;
        blocks.blocks = {item(QStringLiteral("jeden"), 0), item(QStringLiteral("a"), 3), item(QStringLiteral("dwa"), 0),
                         paragraph(QStringLiteral("między")), item(QStringLiteral("trzy"), 0),
                         objectBlock(ImportBlock::Kind::HorizontalLine), objectBlock(ImportBlock::Kind::SceneBreak),
                         objectBlock(ImportBlock::Kind::PageBreak), tableBlock, objectBlock(ImportBlock::Kind::PageBreak)};
        const QString kml = wholeKml(blocks, plainOptions());
        CHECK(kml == QStringLiteral(
                         "<kml><ul><li>jeden<ol type=\"a\" suffix=\")\"><li>a</li></ol></li><li>dwa</li></ul><p>między</p>"
                         "<ul continue=\"true\"><li>trzy</li></ul><hr/><scene-break/><page-break/>"
                         "<table><col width=\"25%\"/><col width=\"75%\"/><tr><th><p>A</p></th><th><p>B</p></th></tr>"
                         "<tr><td colspan=\"2\" background=\"#fff2cc\"><p></p></td></tr></table></kml>"));
        CHECK(readable(kml));

        // Another list at a deeper level is inside the item before it (Word's own lists of the
        // levels); at the first level it is a list of its own
        ImportDocument nested;
        nested.lists = {ListDefinition::numbered(), ListDefinition::bulleted()};
        const auto itemOf = [](const QString& text, int list, int level) {
            ImportBlock block = paragraph(text);
            block.list = list;
            block.listLevel = level;
            return block;
        };
        nested.blocks = {itemOf(QStringLiteral("jeden"), 0, 0), itemOf(QStringLiteral("pod"), 1, 1),
                         itemOf(QStringLiteral("dwa"), 0, 0), itemOf(QStringLiteral("osobna"), 1, 0)};
        const QString lists = wholeKml(nested, plainOptions());
        CHECK(lists == QStringLiteral("<kml><ol><li>jeden<ul type=\"circle\"><li>pod</li></ul></li><li>dwa</li></ol>"
                                      "<ul><li>osobna</li></ul></kml>"));
        CHECK(readable(lists));
    }

    SECTION("Notes, images and annotations") {
        ImportDocument objects;
        ImportNote note;
        note.paragraphs = {paragraph(QStringLiteral("Przypis.")), paragraph(QStringLiteral("Drugi akapit."))};
        ImportNote endnote;
        endnote.endnote = true;
        endnote.paragraphs = {paragraph(QStringLiteral("Końcowy"))};
        objects.notes = {note, endnote};
        ImportImage image;
        image.look.alt = QStringLiteral("Mapa");
        image.look.unit = ImageWidthUnit::Centimeters;
        image.look.width = 5.0;
        objects.images = {image, image};
        ImportAnnotation comment;
        comment.author = QStringLiteral("Ewa");
        comment.created = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
        comment.done = true;
        comment.text = QStringLiteral("Sprawdź & popraw");
        ImportAnnotation todo;
        todo.text = QStringLiteral("Do zrobienia: dopisać scenę");
        objects.annotations = {comment, todo};

        ImportBlock text = paragraph(QString());
        ImportRun words;
        words.text = QStringLiteral("Tekst");
        words.annotations = {0};
        ImportRun noteRun;
        noteRun.kind = ImportRun::Kind::Note;
        noteRun.index = 0;
        ImportRun endRun = noteRun;
        endRun.index = 1;
        ImportRun imageRun;
        imageRun.kind = ImportRun::Kind::Image;
        imageRun.index = 0;
        ImportRun missing = imageRun;
        missing.index = 1;
        ImportRun point;
        point.kind = ImportRun::Kind::Annotation;
        point.index = 1;
        ImportRun field;
        field.kind = ImportRun::Kind::Field;
        field.text = QStringLiteral("page");
        text.runs = {words, noteRun, endRun, imageRun, missing, point, field};
        objects.blocks = {text};

        ImportKmlOptions options = plainOptions();
        options.imageSources = {QStringLiteral("images/mapa.png")};
        options.annotationLabels = {{AnnotationKind::Todo, QStringLiteral("Do zrobienia")}};
        const QString kml = wholeKml(objects, options);
        INFO(kml.toStdString());
        CHECK(readable(kml));
        CHECK(kml.contains(QStringLiteral("kind=\"comment\" author=\"Ewa\" created=\"2026-10-08T12:00:00Z\" done=\"true\">Sprawdź &amp; popraw</annotation>")));
        CHECK(kml.contains(QStringLiteral("kind=\"todo\">dopisać scenę</annotation>")));
        CHECK(kml.contains(QStringLiteral("\">Tekst</anchor>")));
        CHECK(kml.contains(QStringLiteral("\"><p>Przypis.</p><p>Drugi akapit.</p></footnote>")));
        CHECK(kml.contains(QStringLiteral("\">Końcowy</endnote>")));
        CHECK(kml.contains(QStringLiteral("<img src=\"images/mapa.png\" alt=\"Mapa\" width=\"5cm\"/>")));
        CHECK(kml.contains(QStringLiteral("[Mapa]")));
        CHECK(kml.contains(QStringLiteral("<field type=\"page\"/>")));
        CHECK(kml.indexOf(QStringLiteral("<annotations>")) == 5);

        options.annotations = false;
        const QString without = wholeKml(objects, options);
        CHECK_FALSE(without.contains(QStringLiteral("annotation")));
        CHECK_FALSE(without.contains(QStringLiteral("anchor")));
    }
}

TEST_CASE("Import: the cleanups of the text", "[import][cleanup]") {
    ImportKmlOptions options = plainOptions();
    options.cleanup = ImportCleanup();
    options.cleanup.quoteMarks = QuoteMarks::forLanguage(QStringLiteral("pl"));

    SECTION("Typography as autocorrect makes it, spaces, scene marks and empty paragraphs") {
        ImportDocument document;
        ImportBlock split = paragraph(QStringLiteral("\t  \"Cytat\"  i "));
        ImportRun italic;
        italic.text = QStringLiteral("wielokropek...");
        italic.format.italic = true;
        split.runs.push_back(italic);
        document.blocks = {split,
                           paragraph(QStringLiteral("- Dialog - tak.")),
                           paragraph(QStringLiteral(" ")),
                           paragraph(QStringLiteral("* * *")),
                           paragraph(QString()),
                           paragraph(QStringLiteral("To \"kod\"  ...")),
                           paragraph(QStringLiteral("Rock'n'roll -- 1939--1945"))};
        document.blocks[5].style = QStringLiteral("code");
        const QString kml = wholeKml(document, options);
        INFO(kml.toStdString());
        CHECK(kml == QStringLiteral("<kml><p>„Cytat” i <i>wielokropek…</i></p><p>— Dialog — tak.</p><scene-break/>"
                                    "<p style=\"code\">To &quot;kod&quot;  ...</p><p>Rock’n’roll – 1939–1945</p></kml>"));
        CHECK(readable(kml));
    }

    SECTION("The empty paragraphs as scene breaks, and the dialogue's style") {
        ImportDocument document;
        document.blocks = {paragraph(QString()), paragraph(QStringLiteral("Raz.")), paragraph(QString()), paragraph(QString()),
                           paragraph(QStringLiteral("— Dwa.")), paragraph(QString())};
        options.cleanup.empty = EmptyParagraphs::SceneBreak;
        options.dialogueStyle = true;
        CHECK(wholeKml(document, options) ==
              QStringLiteral("<kml><p>Raz.</p><scene-break/><p style=\"dialogue\">— Dwa.</p></kml>"));
        options.cleanup.empty = EmptyParagraphs::Keep;
        CHECK(wholeKml(document, options).count(QStringLiteral("<p></p>")) == 4);
    }
}

// =============================================================================
// DOCX
// =============================================================================

TEST_CASE("Import: a Word document", "[import][docx]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = folder.filePath(QStringLiteral("Szklane miasto – rękopis.docx"));
    writeSampleDocx(path);

    ImportDocument read;
    ImportProblem problem;
    REQUIRE(readDocx(path, &read, &problem));
    CHECK(problem.kind == ImportProblem::Kind::None);
    CHECK(read.title == QStringLiteral("Szklane miasto"));
    CHECK(read.author == QStringLiteral("Anna Nowak"));
    CHECK(read.language == QStringLiteral("pl"));
    CHECK_FALSE(read.fromKalahari);
    CHECK(read.omissions.at(ImportOmission::TableOfContents) == 1);
    CHECK(read.omissions.at(ImportOmission::HeaderFooter) == 1);
    CHECK(read.trackedChanges == 2);

    SECTION("The paragraphs, styles and lists") {
        const std::vector<ImportBlock>& blocks = read.blocks;
        REQUIRE(blocks.size() >= 17);
        CHECK(textOf(blocks[0]) == QStringLiteral("Szklane miasto"));
        CHECK(read.headingLevelOf(blocks[1]) == 1);
        CHECK(blocks[2].align == QStringLiteral("center"));
        REQUIRE(blocks[2].own.firstLineIndent.has_value());
        CHECK(*blocks[2].own.firstLineIndent == 18.0);
        const std::vector<ImportRun>& runs = blocks[2].runs;
        REQUIRE(runs.size() >= 10);
        CHECK(runs[0].format.bold);
        CHECK(runs[1].format.italic);  // of its character style
        CHECK(runs[3].format.font == QStringLiteral("Georgia"));
        CHECK(runs[3].format.size == 14.0);
        CHECK(runs[3].format.color == QStringLiteral("#aa0000"));
        CHECK(runs[5].link == QStringLiteral("https://example.com/"));
        CHECK(runs[6].kind == ImportRun::Kind::Note);
        CHECK(runs[7].kind == ImportRun::Kind::LineBreak);
        CHECK(textOf(blocks[2]).endsWith(QStringLiteral("wiersz tab")));  // the hidden text left out
        REQUIRE(read.notes.size() == 1);
        CHECK(textOf(read.notes[0].paragraphs[0]) == QStringLiteral(" Przypis gruby"));

        CHECK(blocks[4].list == 0);
        CHECK(blocks[5].list == 0);
        CHECK(blocks[5].listLevel == 1);
        CHECK(blocks[6].list == 1);
        REQUIRE(read.lists.size() == 2);
        CHECK(read.lists[0].levels[0].format == ListNumberFormat::Bullet);
        CHECK(read.lists[0].levels[0].bullet == QStringLiteral("•"));
        CHECK(read.lists[0].levels[1].format == ListNumberFormat::LowerAlpha);
        CHECK(read.lists[0].levels[1].suffix == QStringLiteral(")"));
        CHECK(read.lists[1].levels[0].start == 3);

        CHECK(blocks[7].style == QStringLiteral("Cytat"));
        const ParagraphStyleSet book = novelStyles();
        CHECK(suggestedBookStyle(read, QStringLiteral("Wlasny"), book) == QStringLiteral("quote"));
        CHECK(suggestedBookStyle(read, QStringLiteral("MojMotto"), book) == QStringLiteral("motto"));
        CHECK(suggestedBookStyle(read, QStringLiteral("Akapitzlist"), book) == QStringLiteral("normal"));
        const ImportStyle* normal = read.style(QStringLiteral("Normal"));
        REQUIRE(normal != nullptr);
        CHECK(normal->paragraphs >= 5);

        CHECK(textOf(blocks[10]) == QStringLiteral("Przed"));
        CHECK(blocks[11].kind == ImportBlock::Kind::PageBreak);
        CHECK(textOf(blocks[12]) == QStringLiteral("po stronie"));
        CHECK(blocks[13].kind == ImportBlock::Kind::HorizontalLine);
    }

    SECTION("The table, the field, the image and the comments") {
        const auto table = std::find_if(read.blocks.begin(), read.blocks.end(),
                                        [](const ImportBlock& block) { return block.kind == ImportBlock::Kind::Table; });
        REQUIRE(table != read.blocks.end());
        const ImportTable& cells = *table->table;
        CHECK(cells.align == QStringLiteral("center"));
        CHECK(cells.columns == std::vector<double>{2000.0, 6000.0});
        REQUIRE(cells.rows.size() == 4);
        CHECK(cells.rows[0].header);
        CHECK_FALSE(cells.rows[1].header);
        CHECK(cells.rows[1].cells[0].rowSpan == 2);
        CHECK(cells.rows[1].cells[0].background == QStringLiteral("#fff2cc"));
        CHECK(cells.rows[2].cells.size() == 1);
        CHECK(cells.rows[3].cells[0].columnSpan == 2);

        const ImportBlock& page = *(table + 1);
        REQUIRE(page.runs.size() == 2);
        CHECK(page.runs[1].kind == ImportRun::Kind::Field);
        CHECK(page.runs[1].text == QStringLiteral("page"));

        REQUIRE(read.images.size() == 1);
        const ImportImage& image = read.images[0];
        CHECK_FALSE(image.data.isEmpty());
        CHECK(image.fileName == QStringLiteral("image1.png"));
        CHECK(image.look.alt == QStringLiteral("Czerwony kwadrat"));
        CHECK(image.look.name.isEmpty());  // "Obraz 1": Word's own name
        CHECK(image.look.width == 5.0);
        CHECK(image.look.crop.left == 10.0);
        CHECK(image.look.crop.top == 5.0);
        CHECK(image.look.rotation == 1);

        REQUIRE(read.annotations.size() == 3);
        CHECK(read.annotations[0].done);
        CHECK(read.annotations[0].text == QStringLiteral("Sprawdź to.\n\nAnna: Sprawdzone."));
        CHECK(read.annotations[0].created == QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc()));
        const ImportBlock& commented = read.blocks[3];
        CHECK(commented.runs[0].annotations == std::vector<int>{0});
        CHECK(commented.runs[1].kind == ImportRun::Kind::Annotation);
        CHECK(commented.runs[1].index == 2);
    }

    SECTION("The tracked changes accepted and rejected") {
        const ImportDocument accepted = withChangesResolved(read, false);
        const ImportDocument rejected = withChangesResolved(read, true);
        CHECK(textOf(accepted.blocks[3]) == QStringLiteral("Komentarz nowe"));
        CHECK(textOf(rejected.blocks[3]) == QStringLiteral("Komentarz stare"));
    }

    SECTION("Divided and written as KML") {
        const ImportDocument accepted = withChangesResolved(read, false);
        const SplitOptions split = suggestedSplit(accepted);
        CHECK(split.textLevel == 1);
        CHECK(split.partLevel == 0);
        const std::vector<ImportPiece> pieces = splitDocument(accepted, split);
        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0].beforeTitles);
        CHECK(pieces[1].title == QStringLiteral("Rozdział 1"));
        CHECK(pieces[2].title == QStringLiteral("Epilog"));

        ImportKmlOptions options;
        options.bookStyles = novelStyles();
        options.imageSources = {QStringLiteral("images/image1.png")};
        options.annotationLabels = {{AnnotationKind::Todo, QStringLiteral("Do zrobienia")}};
        const QString kml = importKml(accepted, pieces[1], options);
        INFO(kml.toStdString());
        CHECK(readable(kml));
        CHECK(kml.contains(QStringLiteral("<p align=\"center\"><b>Gruby </b><i>kursywa</i> kolor i <a href=\"https://example.com/\">łącze</a><footnote")));
        CHECK(kml.contains(QStringLiteral("kind=\"todo\" author=\"Ewa\">dopisać scenę.</annotation>")));
        CHECK(kml.contains(QStringLiteral("<p style=\"quote\">Cytat.</p><p style=\"quote\">Własny.</p><p style=\"motto\">Motto.</p>")));
        CHECK(kml.contains(QStringLiteral("<p>Przed</p><page-break/><p>po stronie</p><hr/><table align=\"center\">")));
        CHECK(kml.contains(QStringLiteral("<td rowspan=\"2\" background=\"#fff2cc\">")));
        CHECK(kml.contains(QStringLiteral("<p>Strona <field type=\"page\"/></p>")));
        CHECK(kml.contains(QStringLiteral("<img src=\"images/image1.png\" alt=\"Czerwony kwadrat\" width=\"5cm\"")));
    }

    SECTION("A file that is no Word document") {
        const QString text = folder.filePath(QStringLiteral("tekst.docx"));
        QFile file(text);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("To nie jest ZIP.");
        file.close();
        ImportProblem notDocx;
        CHECK_FALSE(readDocx(text, nullptr, &notDocx));
        CHECK(notDocx.kind == ImportProblem::Kind::NotDocx);
        ImportProblem missing;
        CHECK_FALSE(readDocx(folder.filePath(QStringLiteral("brak.docx")), nullptr, &missing));
        CHECK(missing.kind == ImportProblem::Kind::CannotOpen);
    }
}

TEST_CASE("Import: the pictures a Word document links to", "[import][docx]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = folder.filePath(QStringLiteral("Łącza.docx"));
    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>";
    const QByteArray image = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";
    const QByteArray documentRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rW\" Type=\"" + image + "\" Target=\"https://example.com/obrazy/wydma.png\" TargetMode=\"External\"/>"
        "<Relationship Id=\"rF\" Type=\"" + image + "\" Target=\"file:///C:/Obrazy/mapa.png\" TargetMode=\"External\"/>"
        "<Relationship Id=\"rE\" Type=\"" + image + "\" Target=\"media/image1.png\"/>"
        "</Relationships>";
    const auto drawing = [](const QByteArray& blip, const QByteArray& alt) {
        return "<w:p><w:r><w:drawing><wp:inline><wp:extent cx=\"1800000\" cy=\"1800000\"/><wp:docPr id=\"1\" name=\"Obraz 1\" descr=\"" + alt +
               "\"/><a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><pic:pic><pic:blipFill>"
               "<a:blip " + blip + "/></pic:blipFill></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>";
    };
    // Link to File (to the web, to a file of the computer), and Insert and Link: its copy
    const QByteArray body = "<w:body>" + drawing("r:link=\"rW\"", "Wydma") + drawing("r:link=\"rF\"", "Mapa") +
                            drawing("r:embed=\"rE\" r:link=\"rW\"", "Kopia") + "</w:body>";
    writeZip(path, {{QStringLiteral("_rels/.rels"), rels},
                    {QStringLiteral("word/_rels/document.xml.rels"), documentRels},
                    {QStringLiteral("word/document.xml"), part("document", body)},
                    {QStringLiteral("word/media/image1.png"), redPng()}});

    ImportDocument read;
    ImportProblem problem;
    REQUIRE(readDocx(path, &read, &problem));
    CHECK(read.omissions.count(ImportOmission::Image) == 0);
    REQUIRE(read.images.size() == 3);
    CHECK(read.images[0].web);
    CHECK(read.images[0].address == QStringLiteral("https://example.com/obrazy/wydma.png"));
    CHECK(read.images[0].data.isEmpty());
    CHECK(read.images[0].look.alt == QStringLiteral("Wydma"));
    CHECK(read.images[1].missing);
    CHECK_FALSE(read.images[1].web);
    CHECK(read.images[1].address == QStringLiteral("file:///C:/Obrazy/mapa.png"));
    CHECK_FALSE(read.images[2].web);
    CHECK(read.images[2].data == redPng());
}

TEST_CASE("Import: the program's own Word document read back", "[import][docx][export]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    BookExport book;
    book.title = QStringLiteral("Wiatr nad oazą");
    book.author = QStringLiteral("Anna Nowak");
    book.language = QStringLiteral("pl");
    book.styles = novelStyles();
    book.annotationsAsComments = true;
    book.annotationLabels.insert(AnnotationKind::Todo, QStringLiteral("Do zrobienia"));
    book.annotationLabels.insert(AnnotationKind::Note, QStringLiteral("Notatka"));

    ExportItem contents;
    contents.kind = ExportItemKind::Contents;
    contents.title = QStringLiteral("Spis treści");
    ExportItem part;
    part.kind = ExportItemKind::Group;
    part.title = QStringLiteral("Część pierwsza");
    part.titleStyle = QStringLiteral("part_title");
    ExportItem first;
    first.title = QStringLiteral("Rozdział 1. Studnia");
    first.titleStyle = QStringLiteral("chapter_title");
    first.kml = QStringLiteral(
        "<kml><annotations><annotation id=\"a1\" kind=\"todo\" author=\"Anna\">Opisz pogodę.</annotation></annotations>"
        "<p>Zwykły <b>gruby</b> i <i>kursywa</i><footnote id=\"f1\">Przypis.</footnote>.</p>"
        "<p style=\"quote\">Cytat.</p><scene-break/><p><anchor ref=\"a1\">Pogoda</anchor> była.</p>"
        "<ol><li>Jeden</li><li>Dwa</li></ol></kml>");
    ExportItem second;
    second.title = QStringLiteral("Rozdział 2");
    second.titleStyle = QStringLiteral("chapter_title");
    second.kml = QStringLiteral("<kml><p>Drugi.</p></kml>");
    ExportItem epilogue;
    epilogue.title = QStringLiteral("Epilog");
    epilogue.titleStyle = QStringLiteral("chapter_title");
    epilogue.kml = QStringLiteral("<kml><p>Koniec.</p></kml>");
    book.items = {contents, part, first, second, epilogue};

    const QString path = folder.filePath(QStringLiteral("Wiatr.docx"));
    ExportProblem exportProblem;
    REQUIRE(exportDocx(book, path, {}, nullptr, &exportProblem));

    ImportDocument read;
    REQUIRE(readDocx(path, &read, nullptr));
    CHECK(read.fromKalahari);
    CHECK(read.title == QStringLiteral("Wiatr nad oazą"));
    const SplitOptions split = suggestedSplit(read);
    CHECK(split.partLevel == 1);
    CHECK(split.textLevel == 2);
    const std::vector<ImportPiece> pieces = splitDocument(read, split);
    REQUIRE(pieces.size() == 4);
    CHECK(pieces[0].part);
    CHECK(pieces[0].title == QStringLiteral("Część pierwsza"));
    CHECK(pieces[1].title == QStringLiteral("Rozdział 1. Studnia"));
    CHECK(pieces[1].inPart);
    CHECK(pieces[2].title == QStringLiteral("Rozdział 2"));
    CHECK(pieces[3].title == QStringLiteral("Epilog"));

    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    options.annotationLabels = {{AnnotationKind::Todo, QStringLiteral("Do zrobienia")},
                                {AnnotationKind::Note, QStringLiteral("Notatka")}};
    options.cleanup.typography = false;
    const QString kml = importKml(read, pieces[1], options);
    INFO(kml.toStdString());
    CHECK(readable(kml));
    CHECK(kml.contains(QStringLiteral("<p>Zwykły <b>gruby</b> i <i>kursywa</i><footnote")));
    CHECK(kml.contains(QStringLiteral("\">Przypis.</footnote>.</p><p style=\"quote\">Cytat.</p><scene-break/>")));
    CHECK(kml.contains(QStringLiteral("kind=\"todo\" author=\"Anna\">Opisz pogodę.</annotation>")));
    CHECK(kml.contains(QStringLiteral("\">Pogoda</anchor> była.</p><ol><li>Jeden</li><li>Dwa</li></ol>")));
    CHECK(importKml(read, pieces[2], options) == QStringLiteral("<kml><p>Drugi.</p></kml>"));
}

// =============================================================================
// Markdown
// =============================================================================

TEST_CASE("Import: Markdown", "[import][markdown]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    QDir().mkpath(folder.filePath(QStringLiteral("obrazy")));
    QFile png(folder.filePath(QStringLiteral("obrazy/mapa.png")));
    REQUIRE(png.open(QIODevice::WriteOnly));
    png.write(redPng());
    png.close();

    const QString text = QStringLiteral(
        "---\ntitle: \"Szklane miasto\"\nauthor: Anna Nowak\nlang: pl-PL\n---\n\n"
        "# Rozdział 1\n\n"
        "Tekst **gruby**, *kursywa*, _też kursywa_, ~~skreślony~~, <u>podkreślony</u>, H<sub>2</sub>O, "
        "[łącze](https://example.com) i `kod [^1]`.\\\nNowy wiersz  \ntrzeci<br>czwarty.\n\n"
        "Przypis[^n1] i drugi[^n2].\n\n"
        "> Cytat.\n\n"
        "* * *\n\n"
        "&nbsp;\n\n"
        "---\n\n"
        "1. jeden\n2. dwa\n   - pod\n\n"
        "- [x] zrobione\n\n"
        "```\nkod  x\n```\n\n"
        "| A | B |\n|---|---|\n| 1 | 2 |\n\n"
        "![Mapa](obrazy/mapa.png) ![Brak](obrazy/brak.png) ![Sieć](https://example.com/a.png)\n\n"
        "#### Czwarty\n\n"
        "## Rozdział 2\n\nKoniec.\n\n"
        "[^n1]: Treść *przypisu*.\n"
        "[^n2]: Pierwszy akapit.\n\n    Drugi akapit.\n");
    const ImportDocument read = readMarkdown(text, folder.path());
    CHECK(read.title == QStringLiteral("Szklane miasto"));
    CHECK(read.author == QStringLiteral("Anna Nowak"));
    CHECK(read.language == QStringLiteral("pl"));

    const std::vector<ImportBlock>& blocks = read.blocks;
    REQUIRE(blocks.size() >= 16);
    CHECK(blocks[0].style == QStringLiteral("heading_1"));
    const std::vector<ImportRun>& runs = blocks[1].runs;
    QStringList italic;
    for (const ImportRun& run : runs) {
        if (run.format.italic) italic << run.text;
    }
    CHECK(italic == QStringList{QStringLiteral("kursywa"), QStringLiteral("też kursywa")});
    CHECK(std::any_of(runs.begin(), runs.end(), [](const ImportRun& run) { return run.format.underline && run.text == QStringLiteral("podkreślony"); }));
    CHECK(std::any_of(runs.begin(), runs.end(), [](const ImportRun& run) { return run.format.strike; }));
    CHECK(std::any_of(runs.begin(), runs.end(), [](const ImportRun& run) { return run.format.subscript && run.text == QStringLiteral("2"); }));
    CHECK(std::any_of(runs.begin(), runs.end(), [](const ImportRun& run) { return run.link == QStringLiteral("https://example.com"); }));
    CHECK(std::count_if(runs.begin(), runs.end(), [](const ImportRun& run) { return run.kind == ImportRun::Kind::LineBreak; }) == 3);
    CHECK(textOf(blocks[1]).contains(QStringLiteral("kod [^1]")));  // in code, no note

    const std::vector<ImportRun>& notes = blocks[2].runs;
    CHECK(std::count_if(notes.begin(), notes.end(), [](const ImportRun& run) { return run.kind == ImportRun::Kind::Note; }) == 2);
    REQUIRE(read.notes.size() == 2);
    CHECK(textOf(read.notes[0].paragraphs[0]) == QStringLiteral("Treść przypisu."));
    CHECK(read.notes[0].paragraphs[0].runs[1].format.italic);
    CHECK(read.notes[1].paragraphs.size() == 2);

    CHECK(blocks[3].style == QStringLiteral("quote"));
    CHECK(blocks[4].kind == ImportBlock::Kind::SceneBreak);
    CHECK(blocks[5].kind == ImportBlock::Kind::SceneBreak);
    CHECK(blocks[6].kind == ImportBlock::Kind::HorizontalLine);
    CHECK(blocks[7].list == blocks[8].list);
    CHECK(blocks[9].list == blocks[7].list);
    CHECK(blocks[9].listLevel == 1);
    CHECK(read.lists[static_cast<size_t>(blocks[7].list)].levels[0].format == ListNumberFormat::Decimal);
    CHECK(read.lists[static_cast<size_t>(blocks[7].list)].levels[1].format == ListNumberFormat::Bullet);
    CHECK(blocks[10].list != blocks[7].list);
    CHECK(textOf(blocks[10]) == QStringLiteral("☑ zrobione"));
    CHECK(blocks[11].style == QStringLiteral("code"));
    CHECK(textOf(blocks[11]) == QStringLiteral("kod  x"));
    REQUIRE(blocks[12].kind == ImportBlock::Kind::Table);
    CHECK(blocks[12].table->rows.size() == 2);
    CHECK(blocks[12].table->rows[0].header);

    REQUIRE(read.images.size() == 3);
    CHECK_FALSE(read.images[0].data.isEmpty());
    CHECK(read.images[0].look.alt == QStringLiteral("Mapa"));
    CHECK(read.images[1].missing);
    CHECK(read.images[2].web);

    CHECK(blocks[14].style == QStringLiteral("heading_3"));
    CHECK(blocks[14].headingLevel == 4);
    const SplitOptions split = suggestedSplit(read);
    CHECK(split.textLevel == 2);  // one heading of the first level: the book's title
    const std::vector<ImportPiece> pieces = splitDocument(read, {SplitMode::Headings, 0, 1, true});
    REQUIRE(pieces.size() == 1);
    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    options.imageSources = {QStringLiteral("images/mapa.png")};
    const QString kml = importKml(read, pieces[0], options);
    INFO(kml.toStdString());
    CHECK(readable(kml));
    CHECK(kml.contains(QStringLiteral("<p style=\"code\">kod  x</p>")));
    CHECK(kml.contains(QStringLiteral("<img src=\"images/mapa.png\" alt=\"Mapa\"/> [Brak] [Sieć]")));
}

TEST_CASE("Import: Markdown's code and deep headings", "[import][markdown]") {
    const ImportDocument read =
        readMarkdown(QStringLiteral("Tekst.\n\n    kod  wcięty\n\n##### Piąty\n\n~~~python\nx = 1\n~~~\n"), QString());
    REQUIRE(read.blocks.size() == 4);
    CHECK(read.blocks[1].style == QStringLiteral("code"));
    CHECK(textOf(read.blocks[1]) == QStringLiteral("kod  wcięty"));
    CHECK(read.blocks[2].style == QStringLiteral("heading_3"));
    CHECK(read.blocks[2].headingLevel == 5);
    CHECK(read.blocks[3].style == QStringLiteral("code"));
    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    CHECK(wholeKml(read, options) ==
          QStringLiteral("<kml><p>Tekst.</p><p style=\"code\">kod  wcięty</p><h3>Piąty</h3><p style=\"code\">x = 1</p></kml>"));
}

TEST_CASE("Import: the program's own Markdown read back", "[import][markdown][export]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    BookExport book;
    book.title = QStringLiteral("Wiatr");
    book.styles = novelStyles();
    ExportItem part;
    part.kind = ExportItemKind::Group;
    part.title = QStringLiteral("Część pierwsza");
    ExportItem first;
    first.title = QStringLiteral("Rozdział 1");
    first.kml = QStringLiteral("<kml><p>Zwykły <b>gruby</b>*gwiazdki*<footnote id=\"f1\">Przypis.</footnote>.</p>"
                               "<scene-break/><p style=\"quote\">Cytat.</p><p style=\"code\">kod</p><hr/></kml>");
    ExportItem second;
    second.title = QStringLiteral("Rozdział 2");
    second.kml = QStringLiteral("<kml><p>Drugi.</p></kml>");
    book.items = {part, first, second};
    const QString path = folder.filePath(QStringLiteral("Wiatr.md"));
    REQUIRE(exportMarkdown(book, path, QStringLiteral("Wiatr – obrazy"), {}, nullptr, nullptr));
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    const ImportDocument read = readMarkdown(decodeText(data, detectTextEncoding(data)), folder.path());
    const std::vector<ImportPiece> pieces = splitDocument(read, suggestedSplit(read));
    REQUIRE(pieces.size() == 3);
    // The part and the chapters are headings of the first level, as in the book's styles
    CHECK(pieces[0].title == QStringLiteral("Część pierwsza"));
    CHECK(pieces[0].from == pieces[0].to);
    CHECK(pieces[1].title == QStringLiteral("Rozdział 1"));
    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    options.cleanup.typography = false;
    const QString kml = importKml(read, pieces[1], options);
    INFO(kml.toStdString());
    CHECK(readable(kml));
    CHECK(kml.contains(QStringLiteral("<p>Zwykły <b>gruby</b>*gwiazdki*<footnote")));
    CHECK(kml.contains(QStringLiteral("\">Przypis.</footnote>.</p><scene-break/><p style=\"quote\">Cytat.</p><p style=\"code\">kod</p><hr/>")));
}

// =============================================================================
// Files of other programs
// =============================================================================
//
// tests/data/import holds files that other programs made, not written by hand:
// - pandoc.docx: Pandoc 3.1.3 from a Markdown book (a title block, two parts, a footnote,
//   "* * *", a list with a list inside it, a table): pandoc sample.md -o pandoc.docx
// - pandoc_gfm.md: Pandoc's GitHub Markdown of pandoc.docx: pandoc pandoc.docx -t gfm
// - word_template.docx: python-docx 1.2.0 on Word's own template (its styles: Title,
//   Heading 1-2, List Bullet, List Number, Quote; a table, a page break in a paragraph of its
//   own, a comment)
// - libreoffice.docx: LibreOffice 24.2 from word_template.docx through ODT
// - windows-1250.txt: a plain text in Windows-1250 with CR LF line ends

namespace {

QString importData(const char* name) {
    return QStringLiteral(KALAHARI_SOURCE_DIR "/tests/data/import/") + QString::fromUtf8(name);
}

/// The KML of the novel's styles with the cleanups for Polish
ImportKmlOptions polishOptions() {
    ImportKmlOptions options;
    options.bookStyles = novelStyles();
    options.cleanup.quoteMarks = QuoteMarks::forLanguage(QStringLiteral("pl"));
    return options;
}

ImportDocument readDocxData(const char* name) {
    ImportDocument document;
    ImportProblem problem;
    REQUIRE(readDocx(importData(name), &document, &problem));
    CHECK(problem.kind == ImportProblem::Kind::None);
    return document;
}

QByteArray dataOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

}  // namespace

TEST_CASE("Import: the files of other programs", "[import][docx][markdown][text]") {
    SECTION("Pandoc's Word document of a book with parts") {
        const ImportDocument read = readDocxData("pandoc.docx");
        CHECK(read.title == QStringLiteral("Szklane miasto"));
        CHECK(read.author == QStringLiteral("Anna Nowak"));
        CHECK(read.language == QStringLiteral("pl"));
        CHECK_FALSE(read.fromKalahari);
        CHECK(suggestedBookStyle(read, QStringLiteral("BlockText"), novelStyles()) == QStringLiteral("quote"));
        CHECK(suggestedBookStyle(read, QStringLiteral("FirstParagraph"), novelStyles()) == QStringLiteral("normal"));

        const SplitOptions split = suggestedSplit(read);
        CHECK(split.partLevel == 1);
        CHECK(split.textLevel == 2);
        const std::vector<ImportPiece> pieces = splitDocument(read, split);
        REQUIRE(pieces.size() == 6);
        CHECK(pieces[0].beforeTitles);  // the title and the author
        CHECK(pieces[1].part);
        CHECK(pieces[1].title == QStringLiteral("Część pierwsza"));
        CHECK(pieces[2].title == QStringLiteral("Rozdział 1. Huta"));
        CHECK(pieces[2].inPart);
        CHECK(pieces[4].part);
        CHECK(pieces[5].title == QStringLiteral("Rozdział 3"));

        const QString kml = importKml(read, pieces[2], polishOptions());
        INFO(kml.toStdString());
        CHECK(readable(kml));
        CHECK(kml.contains(QStringLiteral("<p>Pierwszy akapit z <b>pogrubieniem</b>, <i>kursywą</i> i przypisem<footnote")));
        CHECK(kml.contains(QStringLiteral("\">Treść przypisu.</footnote>.</p>")));
        // Pandoc's quotation marks are English ones: the book's are Polish
        CHECK(kml.contains(QStringLiteral("<p>„Cytat” w cudzysłowie – i myślnik.</p><p style=\"quote\">Cytat blokowy.</p><hr/>")));
        CHECK(kml.contains(QStringLiteral("<ol><li>jeden</li><li>dwa<ul type=\"dash\"><li>pod</li></ul></li></ol>")));
        CHECK(kml.contains(QStringLiteral("<tr><th><p>A</p></th><th><p>B</p></th></tr><tr><td><p>1</p></td>")));
    }

    SECTION("Pandoc's GitHub Markdown of the same book") {
        const QString path = importData("pandoc_gfm.md");
        const QByteArray data = dataOf(path);
        CHECK(detectTextEncoding(data) == TextEncoding::Utf8);
        const ImportDocument read = readMarkdown(decodeText(data, TextEncoding::Utf8), QFileInfo(path).absolutePath());
        const SplitOptions split = suggestedSplit(read);
        CHECK(split.partLevel == 1);
        const std::vector<ImportPiece> pieces = splitDocument(read, split);
        REQUIRE(pieces.size() == 5);
        CHECK(pieces[0].part);
        const QString kml = importKml(read, pieces[1], polishOptions());
        INFO(kml.toStdString());
        CHECK(readable(kml));
        CHECK(kml.contains(QStringLiteral("<p>„Cytat” w cudzysłowie – i myślnik.</p>")));
        CHECK(kml.contains(QStringLiteral("<ol><li>jeden</li><li>dwa<ul><li>pod</li></ul></li></ol><table>")));
        CHECK(kml.contains(QStringLiteral("\">Treść przypisu.</footnote>.</p>")));
    }

    SECTION("A document of Word's own template, and LibreOffice's copy of it") {
        const ImportDocument word = readDocxData("word_template.docx");
        const ImportDocument libreOffice = readDocxData("libreoffice.docx");
        for (const ImportDocument* read : {&word, &libreOffice}) {
            CHECK(read->title == QStringLiteral("Dom nad rzeką"));
            CHECK(read->author == QStringLiteral("Jan Kowalski"));
            // The chapters have the heading of a scene: they are no parts
            const SplitOptions split = suggestedSplit(*read);
            CHECK(split.partLevel == 0);
            CHECK(split.textLevel == 1);
            // The paragraph of a page break alone is the break: no empty paragraphs around it
            const auto table = std::find_if(read->blocks.begin(), read->blocks.end(), [](const ImportBlock& block) {
                return block.kind == ImportBlock::Kind::Table;
            });
            REQUIRE(std::distance(table, read->blocks.end()) > 2);
            CHECK(std::next(table)->kind == ImportBlock::Kind::PageBreak);
            CHECK(textOf(*std::next(table, 2)) == QStringLiteral("Rozdział 2"));
        }
        // The formatting of Word's styles, for a new style of the book
        const ImportStyle* heading = word.style(QStringLiteral("Heading1"));
        REQUIRE(heading != nullptr);
        CHECK(heading->next == QStringLiteral("Normal"));
        CHECK(heading->properties.value(QStringLiteral("bold")).toBool());
        CHECK(heading->properties.value(QStringLiteral("fontSize")).toDouble() == 14.0);
        CHECK(heading->properties.value(QStringLiteral("spaceBefore")).toDouble() == 24.0);
        CHECK(heading->properties.value(QStringLiteral("spaceAfter")).toDouble() == 0.0);
        CHECK(heading->properties.value(QStringLiteral("keepWithNext")).toBool());
        CHECK(heading->properties.value(QStringLiteral("keepTogether")).toBool());
        CHECK_FALSE(heading->properties.contains(QStringLiteral("fontFamily")));  // a font of the theme
        const ImportStyle* quote = word.style(QStringLiteral("Quote"));
        REQUIRE(quote != nullptr);
        CHECK(quote->properties == QVariantMap{{QStringLiteral("italic"), true}});
        const ImportStyle* title = word.style(QStringLiteral("Title"));
        REQUIRE(title != nullptr);
        CHECK(title->properties.value(QStringLiteral("fontSize")).toDouble() == 26.0);
        CHECK(title->properties.value(QStringLiteral("lineHeight")).toDouble() == 1.0);
        CHECK(word.style(QStringLiteral("Normal"))->properties.isEmpty());

        const std::vector<ImportPiece> pieces = splitDocument(word, suggestedSplit(word));
        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0].beforeTitles);
        CHECK(pieces[1].title == QStringLiteral("Rozdział 1"));
        CHECK(pieces[2].title == QStringLiteral("Rozdział 2"));
        const QString first = importKml(word, pieces[1], polishOptions());
        INFO(first.toStdString());
        CHECK(readable(first));
        CHECK(first == QStringLiteral(
                           "<kml><p>Zwykły <b>gruby</b> i <i>kursywa</i>.</p><ul><li>Punkt pierwszy</li><li>Punkt drugi</li></ul>"
                           "<ol><li>Krok jeden</li></ol><p style=\"quote\">Cytat z książki.</p><table><col width=\"50%\"/>"
                           "<col width=\"50%\"/><tr><td><p>A</p></td><td><p>B</p></td></tr><tr><td><p>1</p></td><td><p>2</p></td></tr>"
                           "</table></kml>"));
        const QString second = importKml(word, pieces[2], polishOptions());
        CHECK(second.contains(QStringLiteral("kind=\"comment\" author=\"Ewa\"")));
        CHECK(second.contains(QStringLiteral(">Sprawdź daty.</annotation></annotations><p><anchor ref=\"")));
        CHECK(second.endsWith(QStringLiteral("<h2>Scena</h2><p>Ostatni akapit.</p></kml>")));

        // LibreOffice writes the left alignment of the paragraphs: the book's style aligns them
        const std::vector<ImportPiece> copies = splitDocument(libreOffice, suggestedSplit(libreOffice));
        REQUIRE(copies.size() == 3);
        CHECK(importKml(libreOffice, copies[1], polishOptions()) == first);
        CHECK(importKml(libreOffice, copies[2], polishOptions()).endsWith(QStringLiteral("<h2>Scena</h2><p>Ostatni akapit.</p></kml>")));
    }

    SECTION("A plain text in Windows-1250") {
        const QByteArray data = dataOf(importData("windows-1250.txt"));
        REQUIRE(detectTextEncoding(data) == TextEncoding::Windows1250);
        const QString text = decodeText(data, TextEncoding::Windows1250);
        CHECK(text.contains(QStringLiteral("Zażółć gęślą jaźń.")));
        CHECK_FALSE(text.contains(QLatin1Char('\r')));
        REQUIRE(paragraphsByEmptyLines(text));
        TextReadOptions options;
        options.emptyLineParagraphs = true;
        options.titles = TitlePattern::Polish;
        options.parts = PartPattern::Polish;
        const ImportDocument read = readPlainText(text, options);
        REQUIRE_FALSE(read.blocks.empty());
        CHECK(read.blocks[0].headingLevel == 1);  // no part titles: the chapters are of the first level
        const std::vector<ImportPiece> pieces = splitDocument(read, suggestedSplit(read));
        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0].title == QStringLiteral("ROZDZIAŁ PIERWSZY"));
        CHECK(importKml(read, pieces[0], polishOptions()) ==
              QStringLiteral("<kml><p>Zażółć gęślą jaźń. „Cytat” – tak…</p><p>Drugi akapit w dwóch wierszach.</p></kml>"));
        CHECK(pieces[1].title == QStringLiteral("Rozdział 2"));
    }
}
