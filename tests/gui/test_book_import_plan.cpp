// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_import_plan.cpp
/// @brief What an import will add to a book: the files in the order of their names, the texts
///        and parts with their titles and kinds, the kinds the book has no room for, and the
///        new styles of the book made of the files' styles

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/extension_points.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/book_import_plan.h>
#include <kalahari/gui/book_styles.h>
#include <catch2/catch_test_macros.hpp>
#include "test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// A novel "Szkło" open in the book of the tests: a title page and a chapter
class Novel {
   public:
    Novel() {
        REQUIRE(m_folder.isValid());
        test::book().loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(test::book().createProject(m_folder.path(), QStringLiteral("Szkło"),
                                           QStringLiteral("Anna"), QStringLiteral("pl"), true,
                                           QStringLiteral("kalahari.novel")));
    }
    ~Novel() { test::book().closeProject(false); }
    Novel(const Novel&) = delete;
    Novel& operator=(const Novel&) = delete;

    core::ProjectManager& book() { return test::book(); }

   private:
    QTemporaryDir m_folder;
};

/// A file of @p text named @p name in @p folder; its path
QString writeFile(const QTemporaryDir& folder, const QString& name, const QByteArray& text) {
    const QString path = QDir(folder.path()).filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(text);
    return path;
}

/// The Markdown files @p paths as the window reads them
std::vector<ImportFile> readMarkdown(const QStringList& paths) {
    ImportReading reading;
    reading.format = ImportFormat::Markdown;
    std::vector<ImportFile> files;
    for (const QString& path : paths) {
        files.push_back(readImportFile(path, reading));
        REQUIRE(files.back().isRead());
    }
    return files;
}

/// The division at the headings of level @p textLevel, a part at @p partLevel
editor::SplitOptions atHeadings(int textLevel, int partLevel = 0) {
    editor::SplitOptions split;
    split.mode = editor::SplitMode::Headings;
    split.textLevel = textLevel;
    split.partLevel = partLevel;
    return split;
}

/// The id of the kind of reference @p reference ("kalahari.novel:prologue" → "prologue")
QString kindId(const QString& reference) {
    return reference.section(QLatin1Char(':'), -1);
}

}  // namespace

TEST_CASE("Import plan: the files in the order of their names, the numbers as numbers",
          "[import][gui]") {
    CHECK(sortedByName({QStringLiteral("/a/Rozdział 10.md"), QStringLiteral("/b/rozdział 2.md"),
                        QStringLiteral("/a/Rozdział 1.md")}) ==
          QStringList({QStringLiteral("/a/Rozdział 1.md"), QStringLiteral("/b/rozdział 2.md"),
                       QStringLiteral("/a/Rozdział 10.md")}));
    CHECK(importExtensions(ImportFormat::Markdown) ==
          QStringList({QStringLiteral(".md"), QStringLiteral(".markdown")}));
    CHECK(importExtensions(ImportFormat::PlainText) == QStringList({QStringLiteral(".txt")}));
}

TEST_CASE("Import plan: the texts of the files, their titles and kinds, and the kinds without room",
          "[import][gui]") {
    Novel novel;
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString first = writeFile(
        folder, QStringLiteral("Rozdział 2.md"),
        "Szkic.\n\n# Prolog\n\nNoc była ciemna.\n\n# Rozdział pierwszy\n\n"
        "Rano przyszedł list.\n");
    const QString second = writeFile(folder, QStringLiteral("Rozdział 10.md"),
                                     "# Prolog\n\nDrugi prolog.\n\n# Burza\n\nWiatr.\n");
    const std::vector<ImportFile> files = readMarkdown({first, second});

    // The book has its title page; one prologue, which it does not have yet
    const ImportKinds kinds = importKindsOf(novel.book());
    REQUIRE(kinds.main);
    CHECK(kinds.main.kind->id == QStringLiteral("chapter"));
    REQUIRE(kinds.part);
    REQUIRE(kinds.titlePage);
    REQUIRE(kinds.find(kinds.titlePage.reference()) != nullptr);
    CHECK(kinds.find(kinds.titlePage.reference())->room == 0);
    const ImportKind* prologue = nullptr;
    for (const ImportKind& kind : kinds.kinds) {
        if (kind.kind.kind->id == QStringLiteral("prologue")) {
            prologue = &kind;
        }
    }
    REQUIRE(prologue != nullptr);
    CHECK(prologue->room == 1);
    CHECK(kinds.find(kinds.main.reference())->room == -1);

    SECTION("At the headings: the kinds the titles name, a title page before them") {
        std::vector<ImportRow> rows = importRows(files, atHeadings(1), false, kinds);
        REQUIRE(rows.size() == 5);
        // The short text before the first title of the first file: a title page, which the
        // book has, so it is unchecked
        CHECK(rows[0].file == 0);
        CHECK(kindId(rows[0].kind) == QStringLiteral("title_page"));
        CHECK(rows[0].title == kinds.find(rows[0].kind)->name);
        CHECK_FALSE(rows[0].checked);
        CHECK(rows[0].full);
        CHECK(rows[1].title == QStringLiteral("Prolog"));
        CHECK(kindId(rows[1].kind) == QStringLiteral("prologue"));
        CHECK(rows[1].checked);
        CHECK(rows[2].title == QStringLiteral("Rozdział pierwszy"));
        CHECK(rows[2].kind == kinds.main.reference());
        // The second prologue: the book can have one
        CHECK(rows[3].file == 1);
        CHECK(kindId(rows[3].kind) == QStringLiteral("prologue"));
        CHECK_FALSE(rows[3].checked);
        CHECK(rows[3].full);
        CHECK(rows[4].title == QStringLiteral("Burza"));
        CHECK(fullKinds(rows, kinds).isEmpty());
        CHECK(checkedWords(rows) == rows[1].piece.words + rows[2].piece.words + rows[4].piece.words);

        // Checked again by the writer: the book cannot have both
        rows[3].checked = true;
        CHECK(fullKinds(rows, kinds) == QStringList{rows[3].kind});
        rows[1].checked = false;
        CHECK(fullKinds(rows, kinds).isEmpty());

        // The texts of the checked rows, of their kinds, with their KML
        const std::vector<ImportedText> texts = importedTexts(
            files, rows, kinds, std::vector<editor::ImportKmlOptions>(files.size()));
        REQUIRE(texts.size() == 3);
        CHECK(texts[0].title == QStringLiteral("Rozdział pierwszy"));
        CHECK(texts[0].kml.contains(QStringLiteral("Rano przyszedł list.")));
        CHECK(texts[1].title == QStringLiteral("Prolog"));
        CHECK(texts[1].kind.kind->id == QStringLiteral("prologue"));
        CHECK(texts[1].kml.contains(QStringLiteral("Drugi prolog.")));
        CHECK(texts[2].title == QStringLiteral("Burza"));
    }

    SECTION("Each file as one text, named after its file") {
        const std::vector<ImportRow> rows = importRows(files, atHeadings(1), true, kinds);
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].title == QStringLiteral("Rozdział 2"));
        CHECK(rows[0].kind == kinds.main.reference());
        CHECK(rows[0].checked);
        // A file that starts with its heading takes its title
        CHECK(rows[1].title == QStringLiteral("Prolog"));
        CHECK(kindId(rows[1].kind) == QStringLiteral("prologue"));
    }

    SECTION("A new book: the room of each kind is its limit") {
        const ImportKinds made = importKindsOf(novel.book().bookTypes(),
                                               QStringLiteral("kalahari.novel"));
        REQUIRE(made.titlePage);
        CHECK(made.find(made.titlePage.reference())->room == 1);
        CHECK(made.main.kind->id == QStringLiteral("chapter"));
        const std::vector<ImportRow> rows = importRows(files, atHeadings(1), false, made);
        REQUIRE(rows.size() == 5);
        CHECK(rows[0].checked);
        CHECK(kindId(rows[0].kind) == QStringLiteral("title_page"));
        CHECK_FALSE(rows[3].checked);
    }
}

TEST_CASE("Import plan: a part with its texts, and a book without parts", "[import][gui]") {
    Novel novel;
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = writeFile(folder, QStringLiteral("Szkło.md"),
                                   "# Część pierwsza\n\n## Huta\n\nPiec.\n\n## Wiatr\n\nPiasek.\n");
    const std::vector<ImportFile> files = readMarkdown({path});
    const ImportKinds kinds = importKindsOf(novel.book());
    std::vector<ImportRow> rows = importRows(files, atHeadings(2, 1), false, kinds);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].piece.part);
    CHECK(rows[0].kind == kinds.part.reference());
    CHECK(rows[0].title == QStringLiteral("Część pierwsza"));
    CHECK(rows[1].piece.inPart);
    CHECK(rows[2].piece.inPart);
    const std::vector<editor::ImportKmlOptions> options(1);

    SECTION("The part goes with its checked texts") {
        rows[1].checked = false;
        CHECK(isChecked(rows, 0));
        const std::vector<ImportedText> texts = importedTexts(files, rows, kinds, options);
        REQUIRE(texts.size() == 1);
        CHECK(texts[0].title == QStringLiteral("Część pierwsza"));
        CHECK(texts[0].kind.reference() == kinds.part.reference());
        REQUIRE(texts[0].texts.size() == 1);
        CHECK(texts[0].texts[0].title == QStringLiteral("Wiatr"));
    }

    SECTION("A part without a checked text does not go") {
        rows[1].checked = false;
        rows[2].checked = false;
        CHECK_FALSE(isChecked(rows, 0));
        CHECK(importedTexts(files, rows, kinds, options).empty());
    }

    SECTION("A book without parts: the texts of the part one after another") {
        ImportKinds flat = kinds;
        flat.part = core::KindRef();
        const std::vector<ImportedText> texts = importedTexts(files, rows, flat, options);
        REQUIRE(texts.size() == 2);
        CHECK(texts[0].title == QStringLiteral("Huta"));
        CHECK(texts[1].title == QStringLiteral("Wiatr"));
    }
}

TEST_CASE("Import plan: the styles of the files and the new styles of the book", "[import][gui]") {
    Novel novel;
    // Two files with the style "Tytuł rozdziału", in the second in other letters
    auto first = std::make_shared<editor::ImportDocument>();
    editor::ImportStyle heading;
    heading.key = QStringLiteral("Tytu");
    heading.name = QStringLiteral("Tytuł rozdziału");
    heading.paragraphs = 2;
    heading.properties.insert(QStringLiteral("fontSize"), 18.0);
    editor::ImportStyle unused;
    unused.key = QStringLiteral("Unused");
    unused.name = QStringLiteral("Unused");
    first->styles = {heading, unused};
    auto second = std::make_shared<editor::ImportDocument>();
    editor::ImportStyle same = heading;
    same.key = QStringLiteral("TytulRozdzialu");
    same.name = QStringLiteral("TYTUŁ ROZDZIAŁU");
    same.paragraphs = 1;
    second->styles = {same};
    std::vector<ImportFile> files(2);
    files[0].path = QStringLiteral("a.docx");
    files[0].read = files[0].document = first;
    files[1].path = QStringLiteral("b.docx");
    files[1].read = files[1].document = second;

    // The list of the window: the style once, with the paragraphs of both files
    const std::vector<ImportFileStyle> styles = importFileStyles(files);
    REQUIRE(styles.size() == 1);
    CHECK(styles[0].name == QStringLiteral("Tytuł rozdziału"));
    CHECK(styles[0].paragraphs == 3);

    // The ids of new styles: Latin letters, another one when the book has it
    CHECK(newStyleId(QStringLiteral("Tytuł rozdziału"), {}) ==
          QStringLiteral("imported_tytul_rozdzialu"));
    CHECK(newStyleId(QStringLiteral("Tytuł rozdziału"),
                     {QStringLiteral("imported_tytul_rozdzialu")}) ==
          QStringLiteral("imported_tytul_rozdzialu_2"));
    CHECK(newStyleId(QStringLiteral("§§"), {}) == QStringLiteral("imported_style"));

    // A new style of the book for the style of the files, with its own formatting
    const editor::ParagraphStyleSet book = bookParagraphStyles(novel.book());
    QHash<QString, QString> mapping{
        {QStringLiteral("Tytuł rozdziału"),
         QLatin1String(NEW_STYLE_PREFIX) + QStringLiteral("Tytuł rozdziału")},
        {QStringLiteral("Brak"), QLatin1String(NEW_STYLE_PREFIX) + QStringLiteral("Brak")}};
    const QList<core::BookParagraphStyle> made = newBookStyles(files, &mapping, book);
    REQUIRE(made.size() == 1);
    CHECK(made[0].id == QStringLiteral("imported_tytul_rozdzialu"));
    CHECK(made[0].name == QStringLiteral("Tytuł rozdziału"));
    CHECK(book.find(made[0].baseStyle) != nullptr);
    CHECK(made[0].properties.value(QStringLiteral("fontSize")).toDouble() == 18.0);
    CHECK(mapping.value(QStringLiteral("Tytuł rozdziału")) == made[0].id);
    CHECK(mapping.value(QStringLiteral("Brak")).isEmpty());  // no file has it: the suggestion

    // Each file maps its own key
    CHECK(fileMapping(*first, mapping) ==
          QHash<QString, QString>{{QStringLiteral("Tytu"), made[0].id}});
    CHECK(fileMapping(*second, mapping) ==
          QHash<QString, QString>{{QStringLiteral("TytulRozdzialu"), made[0].id}});
}

TEST_CASE("Import plan: the start of a text file in its encoding", "[import][gui]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    // "Żółć" in Windows-1250, after an empty line, with a long line
    QByteArray text("\n\xAF\xF3\xB3\xE6\n");
    text += QByteArray(200, 'a') + "\nthird\n";
    const QString path = writeFile(folder, QStringLiteral("ansi.txt"), text);
    const QString preview = textPreview(path, editor::TextEncoding::Windows1250, 2);
    const QStringList lines = preview.split(QLatin1Char('\n'));
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == QStringLiteral("Żółć"));
    CHECK(lines[1].size() == 161);
    CHECK(lines[1].endsWith(QChar(0x2026)));
    CHECK(textPreview(QDir(folder.path()).filePath(QStringLiteral("none.txt")),
                      editor::TextEncoding::Utf8, 2)
              .isEmpty());
}

TEST_CASE("Import plan: the texts a plugin read, with their titles, as one document",
          "[import][gui][plugins]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    {
        QFile image(QDir(folder.path()).filePath(QStringLiteral("map.png")));
        REQUIRE(image.open(QIODevice::WriteOnly));
        image.write("\x89PNG\r\n\x1a\n");
    }
    const std::vector<core::ImportTextInfo> texts{
        {"", "Szklane miasto\n\nAutor: Anna"},
        {"  INT. DOM – NOC ", "Anna *czyta* list.[^1]\n\n[^1]: Od brata.\n\n# Uwaga\n\nCisza."},
        {"", "Wiatr.[^1] ![Mapa](map.png)\n\n[^1]: Gorący.\n\n### Głęboko\n\nPiasek."},
    };
    const editor::ImportDocument document =
        pluginDocument(texts, folder.path(), QStringLiteral("Szkic"));

    // Each text after the first one starts with its title, a heading of the first level; the
    // headings of the texts go a level lower
    std::vector<std::pair<int, QString>> headings;
    for (const editor::ImportBlock& block : document.blocks) {
        if (const int level = document.headingLevelOf(block); level > 0) {
            headings.emplace_back(level, editor::importPlainText(block));
        }
    }
    CHECK(headings == std::vector<std::pair<int, QString>>{{1, QStringLiteral("INT. DOM – NOC")},
                                                          {2, QStringLiteral("Uwaga")},
                                                          {1, QStringLiteral("Szkic")},
                                                          {4, QStringLiteral("Głęboko")}});
    // The notes and the images of each text are its own
    REQUIRE(document.notes.size() == 2);
    REQUIRE(document.images.size() == 1);
    CHECK_FALSE(document.images[0].missing);
    std::vector<int> notes;
    std::vector<int> images;
    for (const editor::ImportBlock& block : document.blocks) {
        for (const editor::ImportRun& run : block.runs) {
            if (run.kind == editor::ImportRun::Kind::Note) {
                notes.push_back(run.index);
            } else if (run.kind == editor::ImportRun::Kind::Image) {
                images.push_back(run.index);
            }
        }
    }
    CHECK(notes == std::vector<int>{0, 1});
    CHECK(images == std::vector<int>{0});
    CHECK(editor::importPlainText(document.notes[1].paragraphs.front()) ==
          QStringLiteral("Gorący."));
    // The styles of the paragraphs, counted in the whole document
    QStringList styles;
    for (const editor::ImportStyle& style : document.styles) {
        styles << QStringLiteral("%1 %2").arg(style.key).arg(style.paragraphs);
    }
    CHECK(styles == QStringList({QStringLiteral("normal 6"), QStringLiteral("heading_1 2"),
                                 QStringLiteral("heading_2 1"), QStringLiteral("heading_3 1")}));

    // Divided at the titles: the text before them, then the plugin's texts with their titles
    const std::vector<editor::ImportPiece> pieces =
        editor::splitDocument(document, editor::SplitOptions());
    REQUIRE(pieces.size() == 3);
    CHECK(pieces[0].beforeTitles);
    CHECK(pieces[1].title == QStringLiteral("INT. DOM – NOC"));
    CHECK(pieces[2].title == QStringLiteral("Szkic"));
    CHECK(pieces[1].words == 7);  // "Anna czyta list." "Od brata." "Uwaga" "Cisza."

    // No texts: an empty document
    CHECK(pluginDocument({}, folder.path(), QStringLiteral("Szkic")).blocks.empty());
}
