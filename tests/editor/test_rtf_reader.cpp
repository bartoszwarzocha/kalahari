// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_rtf_reader.cpp
/// @brief Import of RTF: the files of Word and WordPad, the texts of TextEdit and Scrivener

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/book_import.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QXmlStreamReader>

using namespace kalahari::editor;
using Catch::Matchers::WithinAbs;

namespace {

/// The bytes of a small PNG picture
QByteArray pngBytes() {
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QString textOf(const ImportBlock& block) {
    return importPlainText(block);
}

/// The text runs of a paragraph, without the others
std::vector<ImportRun> textRuns(const ImportBlock& block) {
    std::vector<ImportRun> runs;
    std::copy_if(block.runs.begin(), block.runs.end(), std::back_inserter(runs),
                 [](const ImportRun& run) { return run.kind == ImportRun::Kind::Text; });
    return runs;
}

/// The index of the first run of a kind in a paragraph; -1: none
int runOf(const ImportBlock& block, ImportRun::Kind kind) {
    for (size_t i = 0; i < block.runs.size(); ++i) {
        if (block.runs[i].kind == kind) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// The text of a note
QString noteText(const ImportNote& note) {
    QStringList paragraphs;
    for (const ImportBlock& block : note.paragraphs) {
        paragraphs << importPlainText(block);
    }
    return paragraphs.join(QLatin1Char('\n'));
}

/// A comment or a footnote of Scrivener of one paragraph
LinkedNote linkedNote(const QString& text, bool footnote) {
    LinkedNote note;
    note.footnote = footnote;
    ImportBlock block;
    ImportRun run;
    run.text = text;
    block.runs.push_back(run);
    note.paragraphs.push_back(block);
    return note;
}

bool readableKml(const QString& kml) {
    QXmlStreamReader reader(kml);
    while (!reader.atEnd()) {
        reader.readNext();
    }
    return !reader.hasError();
}

}  // namespace

TEST_CASE("Import RTF: a document of Word", "[import][rtf]") {
    const QByteArray rtf =
        QByteArray(R"RTF({\rtf1\ansi\ansicpg1250\deff0
{\fonttbl{\f0\froman\fcharset238 Times New Roman;}{\f1\fswiss\fcharset238{\*\panose 020b0604020202020204}Arial;}{\f2\fnil\fcharset2 Symbol;}}
{\colortbl;\red0\green0\blue0;\red255\green0\blue0;}
{\stylesheet{\ql\widctlpar\fs24 Normal;}{\s1\ql\keepn\outlinelevel0\b\fs32\sbasedon0\snext0 heading 1;}{\s15\ql\fs20\sbasedon0 footnote text;}{\*\cs16\super footnote reference;}}
{\info{\title Karawana}{\author Jan Kowalski}{\*\company Firma}{\creatim\yr2025\mo1\dy2}}
{\header\pard\plain Nag\'b3\'f3wek strony\par}
\pard\plain\s1\ql\keepn\outlinelevel0\b\fs32 Rozdzia\'b3 pierwszy\par
\pard\plain\qj\fi567\widctlpar\fs24 Zwyk\'b3y {\b pogrubiony}, {\i kursywa}, {\ul podkre\'9clenie} i {\cf2 kolor}{\cs16\super\chftn}{\footnote\pard\plain\s15\fs20 {\cs16\super\chftn} Tre\'9c\'e6 przypisu.}.\par
\pard\plain\fs24 {\*\atnid JK}{\*\atnauthor Kasia}Zdanie z uwag\'b9{\*\atnref 1}\chatn{\*\annotation{\*\atnref 1}\pard\plain\fs20 Sprawd\'9f to.} i {\field{\*\fldinst{HYPERLINK "https://example.com/"}}{\fldrslt{\ul\cf2 \'b3\'b9cze}}}; \u346\'8cmia\u322\'b3o{\super 2} i H{\sub 2}O.\par
\pard\plain\fs24 Akapit z\line nowym wierszem\tab i tabulatorem.\page Po podziale strony.\par
{\*\shppict{\pict\picscalex50\pngblip\picwgoal2268 )RTF") +
        pngBytes().toHex() +
        QByteArray(R"RTF(}}{\nonshppict{\pict\wmetafile8 0102}}\par
{\pict\wmetafile8\picwgoal100 0102}\par
{\field{\*\fldinst TOC \\o "1-3"}{\fldrslt\pard\plain Spis 1\par Spis 2\par}}
\pard\plain\fs24 Koniec{\footnote\ftnalt\pard\plain\s15 Przypis ko\'f1cowy.}.\par
})RTF");
    const ImportDocument read = readRtf(rtf);

    CHECK(read.title == QStringLiteral("Karawana"));
    CHECK(read.author == QStringLiteral("Jan Kowalski"));

    // The styles of its stylesheet, without the character styles
    REQUIRE(read.styles.size() == 3);
    const ImportStyle* normal = read.style(QStringLiteral("s0"));
    const ImportStyle* heading = read.style(QStringLiteral("s1"));
    const ImportStyle* footnote = read.style(QStringLiteral("s15"));
    REQUIRE(normal);
    REQUIRE(heading);
    REQUIRE(footnote);
    CHECK(normal->name == QStringLiteral("Normal"));
    CHECK(normal->properties.value(QStringLiteral("fontSize")).toDouble() == 12.0);
    CHECK(heading->name == QStringLiteral("heading 1"));
    CHECK(heading->headingLevel == 1);
    CHECK(heading->basedOn == QStringLiteral("s0"));
    CHECK(heading->next == QStringLiteral("s0"));
    CHECK(heading->paragraphs == 1);
    CHECK(heading->properties.value(QStringLiteral("bold")).toBool());
    CHECK(heading->properties.value(QStringLiteral("fontSize")).toDouble() == 16.0);
    CHECK(heading->properties.value(QStringLiteral("keepWithNext")).toBool());
    CHECK(footnote->name == QStringLiteral("footnote text"));

    REQUIRE(read.blocks.size() == 9);
    // The heading: the formatting of its style is not its own
    const ImportBlock& title = read.blocks[0];
    CHECK(textOf(title) == QStringLiteral("Rozdział pierwszy"));
    CHECK(title.style == QStringLiteral("s1"));
    CHECK(read.headingLevelOf(title) == 1);
    CHECK_FALSE(title.runs.front().format.bold);
    CHECK(title.runs.front().format.size == 0.0);
    CHECK(title.own.isEmpty());
    CHECK(title.align.isEmpty());

    // A paragraph with its own formatting and a footnote
    const ImportBlock& first = read.blocks[1];
    CHECK(textOf(first) == QStringLiteral("Zwykły pogrubiony, kursywa, podkreślenie i kolor."));
    CHECK(first.style == QStringLiteral("s0"));
    CHECK(first.align == QStringLiteral("justify"));
    REQUIRE(first.own.firstLineIndent);
    CHECK_THAT(*first.own.firstLineIndent, WithinAbs(28.35, 0.001));
    CHECK_FALSE(first.own.widowControl);
    const std::vector<ImportRun> runs = textRuns(first);
    REQUIRE(runs.size() == 9);
    CHECK(runs[0].text == QStringLiteral("Zwykły "));
    CHECK(runs[0].format == ImportCharFormat());
    CHECK(runs[1].format.bold);
    CHECK(runs[3].format.italic);
    CHECK(runs[5].text == QStringLiteral("podkreślenie"));
    CHECK(runs[5].format.underline);
    CHECK(runs[7].format.color == QStringLiteral("#ff0000"));
    const int note = runOf(first, ImportRun::Kind::Note);
    REQUIRE(note >= 0);
    CHECK_FALSE(first.runs[static_cast<size_t>(note)].format.superscript);
    REQUIRE(read.notes.size() == 2);
    const ImportNote& footnoteNote = read.notes[static_cast<size_t>(first.runs[static_cast<size_t>(note)].index)];
    CHECK_FALSE(footnoteNote.endnote);
    CHECK(noteText(footnoteNote) == QStringLiteral("Treść przypisu."));
    CHECK(footnoteNote.paragraphs.front().style == QStringLiteral("s15"));
    CHECK(footnoteNote.paragraphs.front().runs.front().format.size == 0.0);

    // A comment of Word, a link, Unicode, raised and lowered text
    const ImportBlock& second = read.blocks[2];
    CHECK(textOf(second) == QStringLiteral("Zdanie z uwagą i łącze; Śmiało2 i H2O."));
    const int comment = runOf(second, ImportRun::Kind::Annotation);
    REQUIRE(comment >= 0);
    REQUIRE(read.annotations.size() == 1);
    CHECK(read.annotations[0].author == QStringLiteral("Kasia"));
    CHECK(read.annotations[0].text == QStringLiteral("Sprawdź to."));
    const std::vector<ImportRun> secondRuns = textRuns(second);
    const auto link = std::find_if(secondRuns.begin(), secondRuns.end(),
                                   [](const ImportRun& run) { return !run.link.isEmpty(); });
    REQUIRE(link != secondRuns.end());
    CHECK(link->text == QStringLiteral("łącze"));
    CHECK(link->link == QStringLiteral("https://example.com/"));
    CHECK(link->format.underline);
    const auto raised = std::find_if(secondRuns.begin(), secondRuns.end(),
                                     [](const ImportRun& run) { return run.format.superscript; });
    REQUIRE(raised != secondRuns.end());
    CHECK(raised->text == QStringLiteral("2"));
    CHECK(std::any_of(secondRuns.begin(), secondRuns.end(),
                      [](const ImportRun& run) { return run.format.subscript; }));

    // A line break, a tab and a page break in a paragraph
    const ImportBlock& third = read.blocks[3];
    CHECK(runOf(third, ImportRun::Kind::LineBreak) >= 0);
    CHECK(runOf(third, ImportRun::Kind::Tab) >= 0);
    CHECK(read.blocks[4].kind == ImportBlock::Kind::PageBreak);
    CHECK(textOf(read.blocks[5]) == QStringLiteral("Po podziale strony."));

    // A picture in PNG; Word's picture for old programs and a metafile are not read
    const int image = runOf(read.blocks[6], ImportRun::Kind::Image);
    REQUIRE(image >= 0);
    REQUIRE(read.images.size() == 1);
    CHECK(read.images[0].data == pngBytes());
    CHECK(read.images[0].fileName == QStringLiteral("image1.png"));
    CHECK(read.images[0].look.unit == ImageWidthUnit::Centimeters);
    CHECK_THAT(read.images[0].look.width, WithinAbs(2.0, 0.001));
    CHECK(read.blocks[7].runs.empty());
    CHECK(read.omissions.at(ImportOmission::Image) == 1);

    // Word's table of contents and the headers are left out; an endnote
    CHECK(read.omissions.at(ImportOmission::TableOfContents) == 1);
    CHECK(read.omissions.at(ImportOmission::HeaderFooter) == 1);
    CHECK(textOf(read.blocks[8]) == QStringLiteral("Koniec."));
    CHECK(read.notes[1].endnote);
    CHECK(noteText(read.notes[1]) == QStringLiteral("Przypis końcowy."));

    // Its text written as KML
    ImportPiece piece;
    piece.to = read.blocks.size();
    ImportKmlOptions options;
    options.formatting = ImportFormatting::All;
    CHECK(readableKml(importKml(read, piece, options)));
}

TEST_CASE("Import RTF: tables", "[import][rtf]") {
    const QByteArray rtf = R"RTF({\rtf1\ansi{\colortbl;\red0\green128\blue0;}
\trowd\trhdr\trqc\trrh400\clcbpat1\cellx2000\clvmgf\clvertalc\cellx4000\cellx6000
\pard\intbl A\cell B1\par B2\cell C\cell\row
\trowd\cellx2000\clvmrg\cellx4000\cellx6000
\pard\intbl D\cell\cell E\cell\row
\trowd\clmgf\cellx2000\clmrg\cellx4000\cellx6000
\pard\intbl F\cell\cell G\cell
{\trowd\clmgf\cellx2000\clmrg\cellx4000\cellx6000\row}
\pard After.\par})RTF";
    const ImportDocument read = readRtf(rtf);

    REQUIRE(read.blocks.size() == 2);
    REQUIRE(read.blocks[0].kind == ImportBlock::Kind::Table);
    const ImportTable& table = *read.blocks[0].table;
    CHECK(table.align == QStringLiteral("center"));
    CHECK(table.columns == std::vector<double>{2000.0, 2000.0, 2000.0});
    REQUIRE(table.rows.size() == 3);

    const ImportRow& header = table.rows[0];
    CHECK(header.header);
    CHECK(header.height == 20.0);
    REQUIRE(header.cells.size() == 3);
    CHECK(header.cells[0].background == QStringLiteral("#008000"));
    CHECK(textOf(header.cells[0].blocks.front()) == QStringLiteral("A"));
    CHECK(header.cells[1].rowSpan == 2);
    CHECK(header.cells[1].valign == QStringLiteral("middle"));
    REQUIRE(header.cells[1].blocks.size() == 2);
    CHECK(textOf(header.cells[1].blocks[1]) == QStringLiteral("B2"));

    const ImportRow& merged = table.rows[1];
    CHECK_FALSE(merged.header);
    REQUIRE(merged.cells.size() == 2);
    CHECK(textOf(merged.cells[0].blocks.front()) == QStringLiteral("D"));
    CHECK(textOf(merged.cells[1].blocks.front()) == QStringLiteral("E"));

    const ImportRow& wide = table.rows[2];
    REQUIRE(wide.cells.size() == 2);
    CHECK(wide.cells[0].columnSpan == 2);
    CHECK(textOf(wide.cells[1].blocks.front()) == QStringLiteral("G"));

    CHECK(textOf(read.blocks[1]) == QStringLiteral("After."));
}

TEST_CASE("Import RTF: lists", "[import][rtf]") {
    const QByteArray rtf = R"RTF({\rtf1\ansi\ansicpg1250{\fonttbl{\f0 Times;}{\f1\fcharset2 Symbol;}{\f2 Courier New;}}
\pard\ls1\ilvl0\fi-360\li720 {\listtext\f1\'b7\tab}Pierwszy\par
{\listtext\f1\'b7\tab}Drugi\par
\pard\ls1\ilvl1\fi-360\li1440 {\listtext\f2 o\tab}Zagnie\'bfd\'bfony\par
\pard\ls2\ilvl0 {\listtext 3.\tab}Trzeci\par
{\listtext 4.\tab}Czwarty\par
\pard\ls3 {\listtext b)\tab}Litera\par
\pard\ls4 {\listtext IV.\tab}Rzymska\par
\pard {\pntext\f1\'b7\tab}{\*\pn\pnlvlblt\pnf1{\pntxtb\'b7}}WordPad\par
\pard\li720 Zwyk\'b3y\par})RTF";
    const ImportDocument read = readRtf(rtf);

    REQUIRE(read.blocks.size() == 9);
    REQUIRE(read.lists.size() == 5);
    const ImportBlock& first = read.blocks[0];
    CHECK(textOf(first) == QStringLiteral("Pierwszy"));
    CHECK(first.list == 0);
    CHECK(first.listLevel == 0);
    CHECK_FALSE(first.own.leftIndent);
    CHECK_FALSE(first.own.firstLineIndent);
    CHECK(read.blocks[1].list == 0);
    CHECK(read.lists[0].kind() == ListKind::Bulleted);
    CHECK(read.lists[0].levels[0].bullet == QString(QChar(BULLET_DISC)));

    CHECK(textOf(read.blocks[2]) == QStringLiteral("Zagnieżdżony"));
    CHECK(read.blocks[2].list == 0);
    CHECK(read.blocks[2].listLevel == 1);
    CHECK(read.lists[0].levels[1].bullet == QString(QChar(BULLET_CIRCLE)));

    CHECK(read.blocks[3].list == 1);
    CHECK(read.blocks[4].list == 1);
    CHECK(read.lists[1].kind() == ListKind::Numbered);
    CHECK(read.lists[1].levels[0].format == ListNumberFormat::Decimal);
    CHECK(read.lists[1].levels[0].start == 3);
    CHECK(read.lists[1].levels[0].suffix == QStringLiteral("."));

    CHECK(read.lists[2].levels[0].format == ListNumberFormat::LowerAlpha);
    CHECK(read.lists[2].levels[0].start == 2);
    CHECK(read.lists[2].levels[0].suffix == QStringLiteral(")"));
    CHECK(read.lists[3].levels[0].format == ListNumberFormat::UpperRoman);
    CHECK(read.lists[3].levels[0].start == 4);

    // WordPad's list items (\pntext without \ls)
    CHECK(textOf(read.blocks[7]) == QStringLiteral("WordPad"));
    CHECK(read.blocks[7].list == 4);
    CHECK(read.lists[4].kind() == ListKind::Bulleted);

    const ImportBlock& plain = read.blocks[8];
    CHECK(plain.list == -1);
    REQUIRE(plain.own.leftIndent);
    CHECK(*plain.own.leftIndent == 36.0);
}

TEST_CASE("Import RTF: sections and page breaks", "[import][rtf]") {
    const QByteArray rtf = R"RTF({\rtf1\ansi\sectd Jeden\par\sect\sectd\sbknone Dwa\par\sect\sectd Trzy\par
\pard\pagebb Nowa strona\par\sect})RTF";
    const ImportDocument read = readRtf(rtf);

    REQUIRE(read.blocks.size() == 6);
    CHECK(textOf(read.blocks[0]) == QStringLiteral("Jeden"));
    CHECK(textOf(read.blocks[1]) == QStringLiteral("Dwa"));
    CHECK(read.blocks[2].kind == ImportBlock::Kind::PageBreak);
    CHECK(textOf(read.blocks[3]) == QStringLiteral("Trzy"));
    CHECK(read.blocks[4].kind == ImportBlock::Kind::PageBreak);
    CHECK(textOf(read.blocks[5]) == QStringLiteral("Nowa strona"));
    CHECK_FALSE(read.blocks[5].own.pageBreakBefore);
}

TEST_CASE("Import RTF: a text of TextEdit and Scrivener (Cocoa)", "[import][rtf]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    QFile picture(QDir(folder.path()).filePath(QStringLiteral("palma.png")));
    REQUIRE(picture.open(QIODevice::WriteOnly));
    picture.write(pngBytes());
    picture.close();

    const QByteArray rtf = R"RTF({\rtf1\ansi\ansicpg1252\cocoartf2639
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fnil\fcharset0 Palatino-Roman;\f1\fnil\fcharset0 Palatino-Italic;}
{\colortbl;\red255\green255\blue255;\red200\green0\blue0;}
{\*\expandedcolortbl;;\cssrgb\c80000\c0\c0;}
\paperw11900\paperh16840\margl1440\margr1440\vieww12000\viewh15840\viewkind0
\pard\tx560\pardeftab720\fi720\sl288\slmult1\pardirnatural\partightenfactor0

\f0\fs24 \cf0 Pierwszy akapit z \'84cudzys\uc0\u322 owem\'94.\
Drugi akapit, \f1\i kursywa\f0\i0  i {\cf2 kolor}.\
\
\pard\pardeftab720\qc\partightenfactor0
\cf0 ***\
\pard\pardeftab720\fi720\partightenfactor0
\cf0 {{\NeXTGraphic palma.png \width1134 \height1134 \appleattachmentpadding0 \appleembedtype0 \appleaqc
}\'ac}{{\NeXTGraphic brak.png \width2268 \height1134
}\'ac}{{\NeXTGraphic plik.pdf
}\'ac}Ostatni bez ko\uc0\u324 ca})RTF";
    RtfReadOptions options;
    options.folder = folder.path();
    const ImportDocument read = readRtf(rtf, options);

    REQUIRE(read.blocks.size() == 5);
    CHECK(read.styles.empty());
    const ImportBlock& first = read.blocks[0];
    CHECK(textOf(first) == QStringLiteral("Pierwszy akapit z „cudzysłowem”."));
    CHECK(first.style.isEmpty());
    CHECK(first.align.isEmpty());
    REQUIRE(first.own.firstLineIndent);
    CHECK(*first.own.firstLineIndent == 36.0);
    REQUIRE(first.own.lineSpacing);
    CHECK(first.own.lineSpacing->rule == LineSpacingRule::Multiple);
    CHECK_THAT(first.own.lineSpacing->value, WithinAbs(1.2, 0.001));
    // The font and the size of most of the text are not its own
    REQUIRE(first.runs.size() == 1);
    CHECK(first.runs[0].format == ImportCharFormat());

    const std::vector<ImportRun> runs = textRuns(read.blocks[1]);
    REQUIRE(runs.size() == 5);
    CHECK(runs[1].text == QStringLiteral("kursywa"));
    CHECK(runs[1].format.italic);
    CHECK(runs[1].format.font.isEmpty());  // Palatino-Italic is Palatino
    CHECK(runs[2].text == QStringLiteral(" i "));
    CHECK(runs[3].format.color == QStringLiteral("#c80000"));

    CHECK(read.blocks[2].runs.empty());
    CHECK(textOf(read.blocks[3]) == QStringLiteral("***"));
    CHECK(read.blocks[3].align == QStringLiteral("center"));
    CHECK_FALSE(read.blocks[3].own.firstLineIndent);

    // TextEdit's attachments: a picture beside the text, a missing one, a file of another kind
    const ImportBlock& last = read.blocks[4];
    CHECK(textOf(last) == QStringLiteral("Ostatni bez końca"));
    REQUIRE(read.images.size() == 2);
    CHECK(read.images[0].data == pngBytes());
    CHECK_FALSE(read.images[0].missing);
    CHECK_THAT(read.images[0].look.width, WithinAbs(2.0, 0.001));
    CHECK(read.images[1].missing);
    CHECK(read.images[1].address == QStringLiteral("brak.png"));
    CHECK(read.omissions.at(ImportOmission::Image) == 1);
    CHECK(last.runs[0].kind == ImportRun::Kind::Image);
    CHECK(last.runs[1].kind == ImportRun::Kind::Image);
    CHECK_FALSE(textOf(last).contains(QChar(0x00AC)));
}

TEST_CASE("Import RTF: Scrivener's comments, footnotes and annotations", "[import][rtf]") {
    const QByteArray rtf = R"RTF({\rtf1\ansi\ansicpg1252\cocoartf2639
{\fonttbl\f0\fnil\fcharset0 Palatino-Roman;}
{\colortbl;\red255\green255\blue255;\red200\green0\blue0;}
\pard\f0\fs24 Tekst {\field{\*\fldinst{HYPERLINK "scrivcmt://C1"}}{\fldrslt komentowany}} i przypis{\field{\*\fldinst{HYPERLINK "scrivcmt://F1"}}{\fldrslt  tutaj}}.\
Adnotacja{\Scrv_annot \cf2 w grupie} i znacznik {\Scrv_annot\id3}w tek\uc0\u347 cie{\Scrv_annot_end} oraz {\Scrv_fn\id4}przypis{\Scrv_fn_end}.{\Scrv_ps1 nieznane}{\*\Scrv_meta x}\
Bez ko{\Scrv_comm\id5}\uc0\u324 ca})RTF";
    RtfReadOptions options;
    options.linked.insert(QStringLiteral("C1"), linkedNote(QStringLiteral("Komentarz."), false));
    options.linked.insert(QStringLiteral("F1"), linkedNote(QStringLiteral("Przypis połączony."), true));
    const ImportDocument read = readRtf(rtf, options);

    REQUIRE(read.blocks.size() == 3);
    REQUIRE(read.annotations.size() == 3);
    REQUIRE(read.notes.size() == 2);

    // The comment and the footnote linked to the text
    const ImportBlock& first = read.blocks[0];
    CHECK(textOf(first) == QStringLiteral("Tekst komentowany i przypis tutaj."));
    const std::vector<ImportRun> runs = textRuns(first);
    const auto commented = std::find_if(runs.begin(), runs.end(),
                                        [](const ImportRun& run) { return !run.annotations.empty(); });
    REQUIRE(commented != runs.end());
    CHECK(commented->text == QStringLiteral("komentowany"));
    CHECK(read.annotations[static_cast<size_t>(commented->annotations.front())].text ==
          QStringLiteral("Komentarz."));
    const int note = runOf(first, ImportRun::Kind::Note);
    REQUIRE(note > 0);
    CHECK(first.runs[static_cast<size_t>(note) - 1].text.endsWith(QStringLiteral("przypis tutaj")));
    CHECK(noteText(read.notes[static_cast<size_t>(first.runs[static_cast<size_t>(note)].index)]) ==
          QStringLiteral("Przypis połączony."));

    // Inline annotations and footnotes: in a group, between the groups that start and end
    // them; Scrivener's other groups are left out
    const ImportBlock& second = read.blocks[1];
    CHECK(textOf(second) == QStringLiteral("Adnotacja i znacznik  oraz ."));
    std::vector<QString> annotations;
    for (const ImportRun& run : second.runs) {
        if (run.kind == ImportRun::Kind::Annotation) {
            annotations.push_back(read.annotations[static_cast<size_t>(run.index)].text);
        }
    }
    CHECK(annotations == std::vector<QString>{QStringLiteral("w grupie"), QStringLiteral("w tekście")});
    const int inlineNote = runOf(second, ImportRun::Kind::Note);
    REQUIRE(inlineNote >= 0);
    CHECK(noteText(read.notes[static_cast<size_t>(second.runs[static_cast<size_t>(inlineNote)].index)]) ==
          QStringLiteral("przypis"));

    // A start without its end: the text stays in the text
    CHECK(textOf(read.blocks[2]) == QStringLiteral("Bez końca"));
}

TEST_CASE("Import RTF: damaged and unusual files", "[import][rtf]") {
    SECTION("Nothing") {
        CHECK(readRtf(QByteArray()).blocks.empty());
    }
    SECTION("Groups not closed") {
        const ImportDocument read = readRtf(R"RTF({\rtf1 {\b A\par B)RTF");
        REQUIRE(read.blocks.size() == 2);
        CHECK(textOf(read.blocks[1]) == QStringLiteral("B"));
        CHECK(read.blocks[1].runs.front().format.bold);
    }
    SECTION("Text after the end") {
        const ImportDocument read = readRtf(R"RTF({\rtf1 A\par}}} junk)RTF");
        REQUIRE(read.blocks.size() == 1);
        CHECK(textOf(read.blocks[0]) == QStringLiteral("A"));
    }
    SECTION("Groups too deep") {
        const QByteArray rtf = "{\\rtf1 " + QByteArray(600, '{') + "Deep" + QByteArray(600, '}') + "\\par}";
        const ImportDocument read = readRtf(rtf);
        REQUIRE(read.blocks.size() == 1);
        CHECK(textOf(read.blocks[0]) == QStringLiteral("Deep"));
    }
    SECTION("Binary data and a cut escape") {
        const ImportDocument read = readRtf(R"RTF({\rtf1 {\*\data\bin4 {{}}}After\par Cut\'4)RTF");
        REQUIRE(read.blocks.size() == 2);
        CHECK(textOf(read.blocks[0]) == QStringLiteral("After"));
        CHECK(textOf(read.blocks[1]) == QStringLiteral("Cut"));
    }
    SECTION("Symbols, characters with names, Unicode beyond its first plane, huge numbers") {
        const ImportDocument read = readRtf(
            R"RTF({\rtf1\uc1 a\~b\_c\-d \{\}\\ \emdash\lquote x\rquote  \u-10179?\u-8704?\fs99999999999 \u0?e\par})RTF");
        REQUIRE(read.blocks.size() == 1);
        CHECK(textOf(read.blocks[0]) ==
              QStringLiteral("a\u00A0b\u2011cd {}\\ \u2014\u2018x\u2019 \U0001F600e"));
    }
    SECTION("Unknown destinations without their text, Word's \\upr") {
        const ImportDocument read =
            readRtf(R"RTF({\rtf1 {\*\unknown hidden}{\info{\upr{\title Tytu\'b3}{\*\ud{\title Tytu\u322 ?}}}}Text\par})RTF");
        REQUIRE(read.blocks.size() == 1);
        CHECK(textOf(read.blocks[0]) == QStringLiteral("Text"));
    }
}

TEST_CASE("Import RTF: code pages of the fonts", "[import][rtf]") {
    const ImportDocument read = readRtf(
        R"RTF({\rtf1\ansi\ansicpg1252{\fonttbl{\f0 Times;}{\f1\fcharset238 Arial CE;}{\f2\fcharset0\cpg1250 Tahoma;}}
\f0 Caf\'e9 {\f1 \'b3\'9c} {\f2 \'bf}\par})RTF");
    REQUIRE(read.blocks.size() == 1);
    CHECK(textOf(read.blocks[0]) == QStringLiteral("Café łś ż"));
}
