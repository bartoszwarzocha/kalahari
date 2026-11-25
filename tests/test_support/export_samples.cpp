// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file export_samples.cpp
/// @brief The sample book of the export tests and the helpers that read what they write

#include "export_samples.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/unicode_paths.h>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QXmlStreamReader>

#include <zip.h>

using namespace kalahari::editor;

namespace kalahari::test {

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

}  // namespace

/// The styles of a novel, as its pack has them
ParagraphStyleSet novelStyles() {
    const QString normal = QStringLiteral("normal");
    const QString heading = QStringLiteral("heading_1");
    return ParagraphStyleSet::fromPackage(
        {packageStyle(normal, QStringLiteral("Normalny"), {},
                      {{QStringLiteral("fontSize"), 12},
                       {QStringLiteral("lineHeight"), 1.5},
                       {QStringLiteral("alignment"), QStringLiteral("justify")},
                       {QStringLiteral("firstLineIndent"), 18}}),
         packageStyle(heading, QStringLiteral("Nagłówek 1"), normal,
                      {{QStringLiteral("fontSize"), 20},
                       {QStringLiteral("bold"), true},
                       {QStringLiteral("alignment"), QStringLiteral("left")},
                       {QStringLiteral("firstLineIndent"), 0},
                       {QStringLiteral("keepWithNext"), true}}),
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
                      {{QStringLiteral("fontSize"), 24},
                       {QStringLiteral("alignment"), QStringLiteral("center")}}),
         packageStyle(QStringLiteral("chapter_title"), QStringLiteral("Tytuł rozdziału"), heading,
                      {{QStringLiteral("alignment"), QStringLiteral("center")}})},
        QStringLiteral("pl"));
}

QString richKml() {
    return QStringLiteral(
        "<kml><annotations>"
        "<annotation id=\"a1\" kind=\"comment\" author=\"Anna Nowak\" "
        "created=\"2026-10-08T12:00:00Z\">Sprawdź datę.</annotation>"
        "<annotation id=\"a2\" kind=\"todo\" author=\"Anna Nowak\" done=\"true\">Opisz "
        "pogodę.</annotation>"
        "</annotations>"
        "<p>Plain <b>bold</b>, <i>italic</i>, <u>under</u>, <s>strike</s>, H<sub>2</sub>O, "
        "x<sup>2</sup>, "
        "<span color=\"#aa0000\" font=\"Georgia\" size=\"14\" caps=\"small\" "
        "spacing=\"1\">styled</span> *stars* &amp; [brackets].</p>"
        "<p>A line<br/>break, a <a href=\"https://example.com/a?b=1&amp;c=2\">link</a> and "
        "<a href=\"javascript:alert(1)\">not a link</a>.</p>"
        "<p>Note here<footnote id=\"f1\">The <b>first</b> note.</footnote> and an endnote"
        "<endnote id=\"e1\">The end note.</endnote>. <anchor ref=\"a1\">Commented "
        "text</anchor>.<anchor ref=\"a2\"/></p>"
        "<scene-break/>"
        "<p style=\"quote\">A quote.</p>"
        "<p style=\"code\">int x = 1;</p><p style=\"code\">return x;</p>"
        "<h2>Section</h2>"
        "<ol><li>One</li><li>Two<ol type=\"a\"><li>Two a</li></ol></li><li "
        "value=\"7\">Seven</li></ol>"
        "<ul><li>Bullet</li></ul>"
        "<table><col width=\"40%\"/><col width=\"60%\"/>"
        "<tr><th><p>Who</p></th><th><p>Role</p></th></tr>"
        "<tr><td colspan=\"2\" background=\"#fff2cc\"><p>Both</p></td></tr></table>"
        "<p>After the table.</p>"
        "<page-break/>"
        "<p align=\"center\"><img src=\"images/red.png\" alt=\"Red square\" width=\"50%\"/></p>"
        "<p style=\"caption\">Figure <caption-number label=\"Figure\" id=\"c1\"/>. Red</p>"
        "<p>See figure <cross-reference to=\"c1\"/>.</p>"
        "<p><img src=\"images/red.png\" alt=\"Framed\" crop=\"10%\" border=\"solid\" "
        "shadow=\"true\"/></p>"
        "<p><img src=\"images/missing.png\" alt=\"Missing\"/></p>"
        "<hr/>"
        "<list-of-figures/>"
        "</kml>");
}

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
    first.kml = richKml();
    book.items.push_back(first);

    ExportItem second;
    second.elementId = QStringLiteral("e2");
    second.title = QStringLiteral("Rozdział 2");
    second.titleStyle = QStringLiteral("chapter_title");
    second.kml = QStringLiteral(
        "<kml><h1>Its own heading</h1><p>Second<footnote id=\"f2\">Second "
        "note.</footnote>.</p></kml>");
    second.footnotes.footnotesBefore = 0;  // anew in each chapter
    book.items.push_back(second);
    return book;
}

/// The files of a ZIP archive by name
QString longKml(int paragraphs) {
    QString kml = QStringLiteral("<kml>");
    for (int i = 0; i < paragraphs; ++i) {
        kml += QStringLiteral("<p>Paragraph %1: the wind over the oasis moved the sand from one "
                              "dune to another, and the caravan waited for the evening to go "
                              "on.</p>")
                   .arg(i + 1);
    }
    return kml + QStringLiteral("</kml>");
}

ExportItem textItem(const QString& id, const QString& title, const QString& kml) {
    ExportItem item;
    item.elementId = id;
    item.title = title;
    item.titleStyle = QStringLiteral("chapter_title");
    item.kml = kml;
    return item;
}

BookExport bookOf(std::vector<ExportItem> items) {
    BookExport book;
    book.title = QStringLiteral("Wiatr nad oazą");
    book.author = QStringLiteral("Anna Nowak");
    book.language = QStringLiteral("pl");
    book.styles = novelStyles();
    book.items = std::move(items);
    return book;
}

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
void keepSample(const QString& path, const QString& name) {
    const QString folder = qEnvironmentVariable("KALAHARI_EXPORT_SAMPLES");
    if (!folder.isEmpty()) {
        QDir().mkpath(folder);
        const QString target =
            folder + QLatin1Char('/') + (name.isEmpty() ? QFileInfo(path).fileName() : name);
        QFile::remove(target);
        QFile::copy(path, target);
    }
}

int count(const QString& text, const QString& part) {
    return static_cast<int>(text.count(part));
}


}  // namespace kalahari::test
