/// @file test_book_export.cpp
/// @brief Export of a book to DOCX and Markdown: the parts of the Word document, its styles,
///        lists, notes, comments, tables and images, and the Markdown file with its images
///
/// With the environment variable KALAHARI_EXPORT_SAMPLES set to a folder, the files of the
/// first test are copied there, to be opened in Word or LibreOffice.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/unicode_paths.h>
#include <kalahari/editor/book_export.h>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <zip.h>

using namespace kalahari::editor;

namespace {

/// A style of a package
kalahari::core::PackageParagraphStyle packageStyle(const QString& id, const QString& name,
                                                   const QString& base = {},
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("pl"), name);
    style.baseStyle = base;
    style.properties = properties;
    return style;
}

/// The styles of a novel, as its pack has them
ParagraphStyleSet novelStyles() {
    const QString normal = QStringLiteral("normal");
    const QString heading = QStringLiteral("heading_1");
    return ParagraphStyleSet::fromPackage(
        {packageStyle(normal, QStringLiteral("Normalny"), {},
                      {{QStringLiteral("fontSize"), 12}, {QStringLiteral("lineHeight"), 1.5},
                       {QStringLiteral("alignment"), QStringLiteral("justify")},
                       {QStringLiteral("firstLineIndent"), 18}}),
         packageStyle(heading, QStringLiteral("Nagłówek 1"), normal,
                      {{QStringLiteral("fontSize"), 20}, {QStringLiteral("bold"), true},
                       {QStringLiteral("alignment"), QStringLiteral("left")},
                       {QStringLiteral("firstLineIndent"), 0}, {QStringLiteral("keepWithNext"), true}}),
         packageStyle(QStringLiteral("heading_2"), QStringLiteral("Nagłówek 2"), heading,
                      {{QStringLiteral("fontSize"), 16}}),
         packageStyle(QStringLiteral("heading_3"), QStringLiteral("Nagłówek 3"), heading,
                      {{QStringLiteral("fontSize"), 14}}),
         packageStyle(QStringLiteral("quote"), QStringLiteral("Cytat"), normal,
                      {{QStringLiteral("italic"), true}, {QStringLiteral("leftMargin"), 36}}),
         packageStyle(QStringLiteral("dialogue"), QStringLiteral("Dialog"), normal),
         packageStyle(QStringLiteral("code"), QStringLiteral("Kod"), normal,
                      {{QStringLiteral("fontFamily"), QStringLiteral("Courier New")},
                       {QStringLiteral("spellCheck"), false}}),
         packageStyle(QStringLiteral("caption"), QStringLiteral("Podpis ilustracji"), normal,
                      {{QStringLiteral("italic"), true}}),
         packageStyle(QStringLiteral("footnote"), QStringLiteral("Przypis"), normal,
                      {{QStringLiteral("fontSize"), 10}}),
         packageStyle(QStringLiteral("part_title"), QStringLiteral("Tytuł części"), heading,
                      {{QStringLiteral("fontSize"), 24}, {QStringLiteral("alignment"), QStringLiteral("center")}}),
         packageStyle(QStringLiteral("chapter_title"), QStringLiteral("Tytuł rozdziału"), heading,
                      {{QStringLiteral("alignment"), QStringLiteral("center")}})},
        QStringLiteral("pl"));
}

/// A text with every kind of element the export knows
const QString RICH_KML = QStringLiteral(
    "<kml><annotations>"
    "<annotation id=\"a1\" kind=\"comment\" author=\"Anna Nowak\" created=\"2026-10-08T12:00:00Z\">Sprawdź datę.</annotation>"
    "<annotation id=\"a2\" kind=\"todo\" author=\"Anna Nowak\" done=\"true\">Opisz pogodę.</annotation>"
    "</annotations>"
    "<p>Plain <b>bold</b>, <i>italic</i>, <u>under</u>, <s>strike</s>, H<sub>2</sub>O, x<sup>2</sup>, "
    "<span color=\"#aa0000\" font=\"Georgia\" size=\"14\" caps=\"small\" spacing=\"1\">styled</span> *stars* &amp; [brackets].</p>"
    "<p>A line<br/>break, a <a href=\"https://example.com/a?b=1&amp;c=2\">link</a> and "
    "<a href=\"javascript:alert(1)\">not a link</a>.</p>"
    "<p>Note here<footnote id=\"f1\">The <b>first</b> note.</footnote> and an endnote"
    "<endnote id=\"e1\">The end note.</endnote>. <anchor ref=\"a1\">Commented text</anchor>.<anchor ref=\"a2\"/></p>"
    "<scene-break/>"
    "<p style=\"quote\">A quote.</p>"
    "<p style=\"code\">int x = 1;</p><p style=\"code\">return x;</p>"
    "<h2>Section</h2>"
    "<ol><li>One</li><li>Two<ol type=\"a\"><li>Two a</li></ol></li><li value=\"7\">Seven</li></ol>"
    "<ul><li>Bullet</li></ul>"
    "<table><col width=\"40%\"/><col width=\"60%\"/>"
    "<tr><th><p>Who</p></th><th><p>Role</p></th></tr>"
    "<tr><td colspan=\"2\" background=\"#fff2cc\"><p>Both</p></td></tr></table>"
    "<p>After the table.</p>"
    "<page-break/>"
    "<p align=\"center\"><img src=\"images/red.png\" alt=\"Red square\" width=\"50%\"/></p>"
    "<p style=\"caption\">Figure <caption-number label=\"Figure\" id=\"c1\"/>. Red</p>"
    "<p>See figure <cross-reference to=\"c1\"/>.</p>"
    "<p><img src=\"images/red.png\" alt=\"Framed\" crop=\"10%\" border=\"solid\" shadow=\"true\"/></p>"
    "<p><img src=\"images/missing.png\" alt=\"Missing\"/></p>"
    "<hr/>"
    "<list-of-figures/>"
    "</kml>");

/// A book of a part and two chapters, its images in @p folder
BookExport sampleBook(const QString& folder) {
    BookExport book;
    book.title = QStringLiteral("Wiatr nad oazą");
    book.author = QStringLiteral("Anna Nowak");
    book.language = QStringLiteral("pl");
    book.styles = novelStyles();
    book.fontFamily = QStringLiteral("Literata");
    book.imagesFolder = folder;
    book.annotationLabels.insert(AnnotationKind::Todo, QStringLiteral("Zadanie"));

    ExportItem contents;
    contents.kind = ExportItemKind::Contents;
    contents.title = QStringLiteral("Spis treści");
    book.items.push_back(contents);

    ExportItem part;
    part.kind = ExportItemKind::Group;
    part.title = QStringLiteral("Część pierwsza");
    part.titleStyle = QStringLiteral("part_title");
    book.items.push_back(part);

    ExportItem first;
    first.elementId = QStringLiteral("e1");
    first.title = QStringLiteral("Rozdział 1. Studnia");
    first.titleStyle = QStringLiteral("chapter_title");
    first.kml = RICH_KML;
    book.items.push_back(first);

    ExportItem second;
    second.elementId = QStringLiteral("e2");
    second.title = QStringLiteral("Rozdział 2");
    second.titleStyle = QStringLiteral("chapter_title");
    second.kml = QStringLiteral("<kml><h1>Its own heading</h1><p>Second<footnote id=\"f2\">Second note.</footnote>.</p></kml>");
    second.footnotes.footnotesBefore = 0;  // anew in each chapter
    book.items.push_back(second);
    return book;
}

/// The files of a ZIP archive by name
QHash<QString, QByteArray> zipEntries(const QString& path) {
    QHash<QString, QByteArray> entries;
    int error = 0;
    // Not zip_open(): its narrow name is not Unicode on Windows, and the book's name has "ą"
    zip_t* archive = kalahari::core::openZipFile(kalahari::core::pathOfText(path.toStdString()),
                                                 ZIP_RDONLY, &error);
    REQUIRE(archive != nullptr);
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        zip_stat_t stat;
        zip_stat_index(archive, static_cast<zip_uint64_t>(i), 0, &stat);
        zip_file_t* file = zip_fopen_index(archive, static_cast<zip_uint64_t>(i), 0);
        REQUIRE(file != nullptr);
        QByteArray data(static_cast<qsizetype>(stat.size), '\0');
        zip_fread(file, data.data(), stat.size);
        zip_fclose(file);
        entries.insert(QString::fromUtf8(stat.name), data);
    }
    zip_close(archive);
    return entries;
}

/// Whether the XML is well-formed
bool wellFormed(const QByteArray& xml) {
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
    }
    return !reader.hasError();
}

/// A red square of 40 pixels in images/red.png of @p folder
void writeRedSquare(const QString& folder) {
    QDir().mkpath(folder + QStringLiteral("/images"));
    QImage image(40, 40, QImage::Format_RGB32);
    image.fill(QColor(200, 0, 0));
    REQUIRE(image.save(folder + QStringLiteral("/images/red.png")));
}

/// Copy a file to the folder of the samples, when the environment names one
void keepSample(const QString& path, const QString& name = QString()) {
    const QString folder = qEnvironmentVariable("KALAHARI_EXPORT_SAMPLES");
    if (!folder.isEmpty()) {
        QDir().mkpath(folder);
        const QString target = folder + QLatin1Char('/') + (name.isEmpty() ? QFileInfo(path).fileName() : name);
        QFile::remove(target);
        QFile::copy(path, target);
    }
}

int count(const QString& text, const QString& part) {
    return static_cast<int>(text.count(part));
}

}  // namespace

TEST_CASE("DOCX: a package Word reads, with the book's structure", "[export][docx]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    book.annotationsAsComments = true;
    const QString path = folder.filePath(QStringLiteral("Wiatr nad oazą.docx"));

    std::vector<int> progress;
    ExportResult result;
    ExportProblem problem;
    REQUIRE(exportDocx(book, path, [&progress](int value) { progress.push_back(value); }, &result, &problem));
    keepSample(path);
    CHECK(problem.kind == ExportProblem::Kind::None);
    CHECK(result.file == path);
    CHECK(result.images == 2);
    CHECK(result.missingImages == 1);
    REQUIRE_FALSE(progress.empty());
    CHECK(progress.back() == 100);

    const QHash<QString, QByteArray> entries = zipEntries(path);
    for (const char* part : {"[Content_Types].xml", "_rels/.rels", "docProps/core.xml", "docProps/app.xml",
                             "word/document.xml", "word/_rels/document.xml.rels", "word/styles.xml",
                             "word/settings.xml", "word/footnotes.xml", "word/endnotes.xml", "word/numbering.xml",
                             "word/comments.xml", "word/commentsExtended.xml", "word/footer1.xml"}) {
        INFO(part);
        REQUIRE(entries.contains(QString::fromLatin1(part)));
        CHECK(wellFormed(entries.value(QString::fromLatin1(part))));
    }
    const QString document = QString::fromUtf8(entries.value(QStringLiteral("word/document.xml")));
    const QString styles = QString::fromUtf8(entries.value(QStringLiteral("word/styles.xml")));

    SECTION("The titles of the Navigator are headings; a text with its own heading keeps it") {
        CHECK(document.contains(QStringLiteral("<w:pStyle w:val=\"PartTitle\"/>")));
        CHECK(count(document, QStringLiteral("<w:pStyle w:val=\"ChapterTitle\"/>")) == 1);
        CHECK(document.contains(QStringLiteral("Rozdział 1. Studnia")));
        CHECK_FALSE(document.contains(QStringLiteral(">Rozdział 2<")));
        CHECK(document.contains(QStringLiteral("Its own heading")));
        CHECK(document.contains(QStringLiteral("<w:pStyle w:val=\"Heading2\"/>")));
    }

    SECTION("The book's styles: Word's own names for Normal, the headings, Quote and Caption") {
        CHECK(styles.contains(QStringLiteral("w:styleId=\"Normal\"><w:name w:val=\"Normal\"/>")));
        CHECK(styles.contains(QStringLiteral("w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/>")));
        CHECK(styles.contains(QStringLiteral("w:styleId=\"Quote\"><w:name w:val=\"Quote\"/>")));
        CHECK(styles.contains(QStringLiteral("w:styleId=\"Caption\"><w:name w:val=\"caption\"/>")));
        CHECK(styles.contains(QStringLiteral("w:styleId=\"Dialogue\"><w:name w:val=\"Dialog\"/><w:basedOn w:val=\"Normal\"/>")));
        CHECK(styles.contains(QStringLiteral("w:styleId=\"ChapterTitle\"><w:name w:val=\"Tytuł rozdziału\"/><w:basedOn w:val=\"Heading1\"/>")));
        // Normal: the export's font in the defaults, the book's size, spacing and indent
        CHECK(styles.contains(QStringLiteral("<w:rFonts w:ascii=\"Literata\"")));
        CHECK(styles.contains(QStringLiteral("<w:lang w:val=\"pl-PL\"")));
        CHECK(styles.contains(QStringLiteral("<w:spacing w:line=\"360\" w:lineRule=\"auto\"/><w:ind w:firstLine=\"360\"/><w:jc w:val=\"both\"/>")));
        // Code: its own font, not checked
        CHECK(styles.contains(QStringLiteral("<w:rFonts w:ascii=\"Courier New\"")));
        CHECK(styles.contains(QStringLiteral("<w:noProof/>")));
        CHECK(styles.contains(QStringLiteral("<w:outlineLvl w:val=\"0\"/>")));
    }

    SECTION("The formatting of the text") {
        CHECK(document.contains(QStringLiteral("<w:b/><w:bCs/>")));
        CHECK(document.contains(QStringLiteral("<w:vertAlign w:val=\"subscript\"/>")));
        CHECK(document.contains(QStringLiteral("<w:vertAlign w:val=\"superscript\"/>")));
        CHECK(document.contains(QStringLiteral("<w:smallCaps/>")));
        CHECK(document.contains(QStringLiteral("<w:color w:val=\"AA0000\"/><w:spacing w:val=\"20\"/><w:sz w:val=\"28\"/>")));
        CHECK(document.contains(QStringLiteral("A line</w:t><w:br/>")));
        // A line ended by a line break is not justified, as in the editor
        CHECK(QString::fromUtf8(entries.value(QStringLiteral("word/settings.xml")))
                  .contains(QStringLiteral("<w:compat><w:doNotExpandShiftReturn/>")));
        CHECK(document.contains(QStringLiteral("*stars* &amp; [brackets]")));
    }

    SECTION("Links: Word's links to pages, any other address only text") {
        const QString rels = QString::fromUtf8(entries.value(QStringLiteral("word/_rels/document.xml.rels")));
        CHECK(rels.contains(QStringLiteral("Target=\"https://example.com/a?b=1&amp;c=2\" TargetMode=\"External\"")));
        CHECK_FALSE(rels.contains(QStringLiteral("javascript")));
        CHECK(document.contains(QStringLiteral("<w:rStyle w:val=\"Hyperlink\"/>")));
    }

    SECTION("Notes: Word's footnotes and endnotes, numbered anew in the second chapter") {
        const QString footnotes = QString::fromUtf8(entries.value(QStringLiteral("word/footnotes.xml")));
        const QString endnotes = QString::fromUtf8(entries.value(QStringLiteral("word/endnotes.xml")));
        CHECK(document.contains(QStringLiteral("<w:footnoteReference w:id=")));
        CHECK(document.contains(QStringLiteral("<w:endnoteReference w:id=")));
        CHECK(footnotes.contains(QStringLiteral("first</w:t>")));
        CHECK(footnotes.contains(QStringLiteral("Second note.")));
        CHECK(endnotes.contains(QStringLiteral("The end note.")));
        // The second chapter starts its footnotes at 1 again: its section restarts them
        CHECK(document.contains(QStringLiteral("<w:footnotePr><w:numFmt w:val=\"decimal\"/><w:numStart w:val=\"1\"/>"
                                               "<w:numRestart w:val=\"eachSect\"/></w:footnotePr>")));
        CHECK(QString::fromUtf8(entries.value(QStringLiteral("word/settings.xml")))
                  .contains(QStringLiteral("<w:pos w:val=\"sectEnd\"/>")));
    }

    SECTION("Lists: Word's lists with the item's own number") {
        const QString numbering = QString::fromUtf8(entries.value(QStringLiteral("word/numbering.xml")));
        CHECK(numbering.contains(QStringLiteral("<w:numFmt w:val=\"lowerLetter\"/>")));
        CHECK(numbering.contains(QStringLiteral("<w:numFmt w:val=\"bullet\"/>")));
        CHECK(numbering.contains(QStringLiteral("<w:startOverride w:val=\"7\"/>")));
        CHECK(count(document, QStringLiteral("<w:numPr>")) == 5);
    }

    SECTION("A table: its columns, merged cells, header row and background") {
        CHECK(document.contains(QStringLiteral("<w:tblHeader/>")));
        CHECK(document.contains(QStringLiteral("<w:gridSpan w:val=\"2\"/>")));
        CHECK(document.contains(QStringLiteral("w:fill=\"FFF2CC\"")));
        CHECK(count(document, QStringLiteral("<w:gridCol ")) == 2);
    }

    SECTION("Images: the file as it is, a cropped and framed one drawn, a missing one left out") {
        int media = 0;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            media += it.key().startsWith(QLatin1String("word/media/")) ? 1 : 0;
        }
        CHECK(media == 2);
        QFile red(folder.filePath(QStringLiteral("images/red.png")));
        REQUIRE(red.open(QIODevice::ReadOnly));
        CHECK(entries.value(QStringLiteral("word/media/image1.png")) == red.readAll());
        CHECK(document.contains(QStringLiteral("descr=\"Red square\"")));
        CHECK(count(document, QStringLiteral("<w:drawing>")) == 2);
    }

    SECTION("Captions, cross-references and the list of figures: the program's text") {
        CHECK(document.contains(QStringLiteral("w:name=\"_KalC_c1\"")));
        CHECK(document.contains(QStringLiteral("<w:hyperlink w:anchor=\"_KalC_c1\">")));
        CHECK(document.contains(QStringLiteral("<w:pStyle w:val=\"TableofFigures\"/>")));
        CHECK(document.contains(QStringLiteral("Figure 1. Red")));
    }

    SECTION("Comments: the annotations with their authors, a TODO done is resolved") {
        const QString comments = QString::fromUtf8(entries.value(QStringLiteral("word/comments.xml")));
        CHECK(comments.contains(QStringLiteral("w:author=\"Anna Nowak\" w:date=\"2026-10-08T12:00:00Z\" w:initials=\"AN\"")));
        CHECK(comments.contains(QStringLiteral("Sprawdź datę.")));
        CHECK(comments.contains(QStringLiteral("Zadanie: Opisz pogodę.")));
        CHECK(count(document, QStringLiteral("<w:commentRangeStart ")) == 2);
        CHECK(count(document, QStringLiteral("<w:commentRangeEnd ")) == 2);
        CHECK(count(document, QStringLiteral("<w:commentReference ")) == 2);
        const QString extended = QString::fromUtf8(entries.value(QStringLiteral("word/commentsExtended.xml")));
        CHECK(extended.contains(QStringLiteral("w15:done=\"1\"")));
    }

    SECTION("The table of contents: Word's field, filled with the headings") {
        CHECK(document.contains(QStringLiteral("TOC \\o \"1-3\" \\h \\z \\u")));
        CHECK(document.contains(QStringLiteral("<w:pStyle w:val=\"TOCHeading\"/>")));
        CHECK(QString::fromUtf8(entries.value(QStringLiteral("word/settings.xml"))).contains(QStringLiteral("<w:updateFields")));
        CHECK(document.contains(QStringLiteral("<w:hyperlink w:anchor=\"_KalH1\"")));
    }

    SECTION("Sections: each item from a new page, the page of the settings, page numbers") {
        CHECK(count(document, QStringLiteral("<w:sectPr>")) == 4);
        CHECK(count(document, QStringLiteral("<w:type w:val=\"nextPage\"/>")) == 3);
        CHECK(document.contains(QStringLiteral("<w:pgSz w:w=\"11906\" w:h=\"16838\"/>")));
        CHECK(count(document, QStringLiteral("<w:footerReference ")) == 4);
        CHECK(document.contains(QStringLiteral("<w:pageBreakBefore/>")));
    }

    SECTION("The book's title, author and language in the properties of the document") {
        const QString core = QString::fromUtf8(entries.value(QStringLiteral("docProps/core.xml")));
        CHECK(core.contains(QStringLiteral("<dc:title>Wiatr nad oazą</dc:title>")));
        CHECK(core.contains(QStringLiteral("<dc:creator>Anna Nowak</dc:creator>")));
        CHECK(core.contains(QStringLiteral("<dc:language>pl-PL</dc:language>")));
    }
}

TEST_CASE("DOCX: the options of the export", "[export][docx]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path());
    BookExport book = sampleBook(folder.path());
    const QString path = folder.filePath(QStringLiteral("book.docx"));

    SECTION("Without the comments, no comments part") {
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-1.docx"));
        const QHash<QString, QByteArray> entries = zipEntries(path);
        CHECK_FALSE(entries.contains(QStringLiteral("word/comments.xml")));
        CHECK_FALSE(QString::fromUtf8(entries.value(QStringLiteral("word/document.xml")))
                        .contains(QStringLiteral("commentRangeStart")));
    }

    SECTION("Without the titles, the texts go without the Navigator's titles") {
        book.titleHeadings = false;
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-2.docx"));
        const QString document = QString::fromUtf8(zipEntries(path).value(QStringLiteral("word/document.xml")));
        CHECK_FALSE(document.contains(QStringLiteral("Rozdział 1. Studnia")));
        CHECK_FALSE(document.contains(QStringLiteral("PartTitle\"/>")));
        CHECK(document.contains(QStringLiteral("Spis treści")));  // the title of the contents stays
    }

    SECTION("Texts one after another, mirror margins, no page numbers") {
        book.textsOnNewPage = false;
        book.page.mirrored = true;
        book.page.pageNumbers = false;
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-3.docx"));
        const QHash<QString, QByteArray> entries = zipEntries(path);
        const QString document = QString::fromUtf8(entries.value(QStringLiteral("word/document.xml")));
        CHECK(count(document, QStringLiteral("<w:type w:val=\"continuous\"/>")) == 4);
        CHECK_FALSE(document.contains(QStringLiteral("footerReference")));
        CHECK_FALSE(entries.contains(QStringLiteral("word/footer1.xml")));
        CHECK(QString::fromUtf8(entries.value(QStringLiteral("word/settings.xml"))).contains(QStringLiteral("<w:mirrorMargins/>")));
        // The inner margin at the binding: 30 mm
        CHECK(document.contains(QStringLiteral("w:left=\"1701\"")));
    }

    SECTION("Mirror margins: a text starts on a right page, or on a left one of its own") {
        book.page.mirrored = true;
        book.items[3].firstPageLeft = true;
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-4.docx"));
        const QString document = QString::fromUtf8(zipEntries(path).value(QStringLiteral("word/document.xml")));
        CHECK(count(document, QStringLiteral("<w:type w:val=\"oddPage\"/>")) == 2);
        CHECK(count(document, QStringLiteral("<w:type w:val=\"evenPage\"/>")) == 1);
    }

    SECTION("Notes numbered through the book: the second chapter goes on") {
        book.items[3].footnotes.footnotesBefore = 1;
        book.endnotesAtBookEnd = true;
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-5.docx"));
        const QHash<QString, QByteArray> entries = zipEntries(path);
        const QString document = QString::fromUtf8(entries.value(QStringLiteral("word/document.xml")));
        CHECK(count(document, QStringLiteral("<w:numRestart w:val=\"eachSect\"/>")) == 0);
        CHECK(QString::fromUtf8(entries.value(QStringLiteral("word/settings.xml"))).contains(QStringLiteral("<w:pos w:val=\"docEnd\"/>")));
    }

    SECTION("Only some texts: the numbers of their notes are the book's") {
        book.items.erase(book.items.begin(), book.items.begin() + 3);  // only the second chapter
        book.items[0].footnotes.footnotesBefore = 4;
        REQUIRE(exportDocx(book, path, {}, nullptr, nullptr));
        keepSample(path, QStringLiteral("options-6.docx"));
        const QString document = QString::fromUtf8(zipEntries(path).value(QStringLiteral("word/document.xml")));
        CHECK(document.contains(QStringLiteral("<w:numStart w:val=\"5\"/><w:numRestart w:val=\"eachSect\"/>")));
    }

    SECTION("A folder that cannot be written: a problem of writing") {
        ExportProblem problem;
        CHECK_FALSE(exportDocx(book, folder.filePath(QStringLiteral("missing/folder/book.docx")), {}, nullptr, &problem));
        CHECK(problem.kind == ExportProblem::Kind::CannotWrite);
        CHECK_FALSE(problem.lines.isEmpty());
    }
}

TEST_CASE("Markdown: a file GitHub shows, and that reads as text", "[export][markdown]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path() + QStringLiteral("/book"));
    BookExport book = sampleBook(folder.path() + QStringLiteral("/book"));
    const QString path = folder.filePath(QStringLiteral("Wiatr nad oazą.md"));
    const QString imagesName = markdownImagesFolderName(QStringLiteral("Wiatr nad oazą"), QStringLiteral("obrazy"));
    CHECK(imagesName == QStringLiteral("Wiatr nad oazą – obrazy"));

    ExportResult result;
    REQUIRE(exportMarkdown(book, path, imagesName, {}, &result, nullptr));
    keepSample(path);
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QString markdown = QString::fromUtf8(file.readAll());

    SECTION("Headings: the titles and the headings of the texts") {
        CHECK(markdown.contains(QStringLiteral("# Część pierwsza\n")));
        CHECK(markdown.contains(QStringLiteral("# Rozdział 1. Studnia\n")));
        CHECK(markdown.contains(QStringLiteral("## Section\n")));
        CHECK(markdown.contains(QStringLiteral("# Its own heading\n")));
        CHECK_FALSE(markdown.contains(QStringLiteral("# Rozdział 2\n")));
    }

    SECTION("The table of contents: the headings with their anchors") {
        CHECK(markdown.contains(QStringLiteral("- [Rozdział 1. Studnia](#rozdział-1-studnia)")));
        CHECK(markdown.contains(QStringLiteral("  - [Section](#section)")));
        CHECK_FALSE(markdown.contains(QStringLiteral("[Spis treści]")));
    }

    SECTION("The formatting of the text, the characters of Markdown escaped") {
        CHECK(markdown.contains(QStringLiteral("**bold**")));
        CHECK(markdown.contains(QStringLiteral("*italic*")));
        CHECK(markdown.contains(QStringLiteral("<u>under</u>")));
        CHECK(markdown.contains(QStringLiteral("~~strike~~")));
        CHECK(markdown.contains(QStringLiteral("H<sub>2</sub>O")));
        CHECK(markdown.contains(QStringLiteral("\\*stars\\* & \\[brackets\\]")));
        CHECK(markdown.contains(QStringLiteral("A line\\\nbreak")));
        CHECK(markdown.contains(QStringLiteral("[link](https://example.com/a?b=1&c=2)")));
        CHECK(markdown.contains(QStringLiteral(" and not a link.")));
    }

    SECTION("Quotes, code, lists and tables") {
        CHECK(markdown.contains(QStringLiteral("> A quote.")));
        CHECK(markdown.contains(QStringLiteral("```\nint x = 1;\nreturn x;\n```")));
        // Markdown numbers a list on from its first number: the item of its own number starts
        // another list
        CHECK(markdown.contains(QStringLiteral("1. One\n2. Two\n   1. Two a\n\n<!-- -->\n\n7. Seven")));
        CHECK(markdown.contains(QStringLiteral("- Bullet")));
        CHECK(markdown.contains(QStringLiteral("| Who | Role |\n| --- | --- |\n| Both |  |")));
    }

    SECTION("Notes at the end of the file, the labels unique in it") {
        CHECK(markdown.contains(QStringLiteral("Note here[^3-1] and an endnote[^3-i].")));
        CHECK(markdown.contains(QStringLiteral("[^3-1]: The **first** note.")));
        CHECK(markdown.contains(QStringLiteral("[^4-1]: Second note.")));
    }

    SECTION("Images: copied to their folder, a drawn one for a cropped image") {
        CHECK(result.images == 2);
        CHECK(result.missingImages == 1);
        CHECK(result.imagesFolder == folder.filePath(imagesName));
        CHECK(markdown.contains(QStringLiteral("![Red square](<Wiatr nad oazą – obrazy/red.png>)")));
        CHECK(markdown.contains(QStringLiteral("![Framed](<Wiatr nad oazą – obrazy/red-2.png>)")));
        CHECK(QFileInfo::exists(folder.filePath(imagesName + QStringLiteral("/red.png"))));
        CHECK(QFileInfo::exists(folder.filePath(imagesName + QStringLiteral("/red-2.png"))));
    }

    SECTION("Scene breaks, lines, captions and the list of figures") {
        CHECK(markdown.contains(QStringLiteral("\\*\u2003\\*\u2003\\*")));
        CHECK(markdown.contains(QStringLiteral("\n---\n")));
        CHECK(markdown.contains(QStringLiteral("Figure 1. Red")));
        CHECK(markdown.contains(QStringLiteral("- Figure 1. Red")));
    }
}

TEST_CASE("Markdown: images named in the book's folder without a copy", "[export][markdown]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    writeRedSquare(folder.path() + QStringLiteral("/book"));
    BookExport book = sampleBook(folder.path() + QStringLiteral("/book"));
    book.copyImages = false;
    const QString path = folder.filePath(QStringLiteral("out/book.md"));
    QDir().mkpath(folder.filePath(QStringLiteral("out")));
    ExportResult result;
    REQUIRE(exportMarkdown(book, path, QStringLiteral("book – images"), {}, &result, nullptr));
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QString markdown = QString::fromUtf8(file.readAll());
    CHECK(markdown.contains(QStringLiteral("![Red square](../book/images/red.png)")));
    CHECK(result.imagesFolder.isEmpty());
    CHECK_FALSE(QFileInfo::exists(folder.filePath(QStringLiteral("out/book – images"))));
}
