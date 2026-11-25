// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_book_page_numbers.cpp
/// @brief The pages of the texts in the whole book: the first page of each text, its number
///        and its side, in the editors and in the status bar

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/page_start.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/editor/text_places.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/gui/book_page_numbers.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/document_window.h"

#include <QApplication>
#include <QFile>
#include <QTextDocument>

#include <optional>
#include <string>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::BookPageNumbers;
using gui::TextFirstPage;
using test::OpenNovel;
using test::Window;

namespace {

core::SettingsManager& settings() {
    return core::SettingsManager::getInstance();
}

/// Gives back the settings of the pages as they were before the test (their defaults when
/// they had none)
struct KeptPageSettings {
    test::KeptSetting<bool> mirror{"editor.margins.mirrorEnabled", false};
    test::KeptSetting<std::string> numbering{BookPageNumbers::NUMBERING_KEY,
                                             BookPageNumbers::NUMBERING_BOOK};
    test::KeptSetting<std::string> start{core::TEXTS_START_KEY, core::PAGE_START_RIGHT};
};

/// A text of many pages: paragraphs of many words
QString longKml(int paragraphs) {
    QString kml = QStringLiteral("<kml>");
    for (int i = 0; i < paragraphs; ++i) {
        kml += QStringLiteral("<p>") +
               QStringLiteral("Words that go on and on across the page of the book. ").repeated(8) +
               QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

/// The book of the test only: its own chapter, the texts the book type adds taken out
void keepOnly(OpenNovel& novel) {
    core::ProjectManager& pm = novel.pm;
    const core::ProjectBook* book = pm.book();
    REQUIRE(book != nullptr);
    QStringList others;
    for (const QList<core::ProjectElement>* list :
         {&book->frontElements, &book->mainElements, &book->backElements}) {
        for (const core::ProjectElement& element : *list) {
            if (element.id != novel.chapterId) {
                others << element.id;
            }
        }
    }
    for (const QString& id : others) {
        const std::optional<core::ProjectElement> removed = pm.removeElement(id);
        REQUIRE(removed.has_value());
        for (const QString& file : pm.filesOf(*removed)) {
            QFile::remove(file);
        }
    }
}

/// Add a chapter with its text saved
QString addChapter(core::ProjectManager& pm, const QString& title, const QString& kml,
                   const QString& groupId = QString()) {
    const QString id = pm.addElement(pm.chapterKindFor(core::BookPlace::Main, groupId), title,
                                     core::BookPlace::Main, groupId);
    REQUIRE_FALSE(id.isEmpty());
    pm.setChapterContent(id, kml);
    REQUIRE(pm.saveChapterContent(id));
    return id;
}

/// The pages of an editor's text, every line laid out
int pagesOf(gui::EditorPanel& panel) {
    editor::BookEditor* editor = panel.getBookEditor();
    if (auto* layout = qobject_cast<editor::KalahariTextDocumentLayout*>(
            editor->textDocument()->documentLayout())) {
        layout->layoutPendingBlocks();
    }
    return editor->totalPages();
}

/// The first page of a text, as counted
TextFirstPage firstPage(const BookPageNumbers& numbers, const QString& id) {
    const std::optional<TextFirstPage> first = numbers.firstPageOf(id);
    REQUIRE(first.has_value());
    return *first;
}

}  // namespace

TEST_CASE("Book pages: the texts numbered through the book, with mirror margins on the page "
          "their start says",
          "[gui][pages]") {
    const KeptPageSettings kept;
    settings().set<bool>("editor.margins.mirrorEnabled", true);
    settings().set<std::string>(BookPageNumbers::NUMBERING_KEY, BookPageNumbers::NUMBERING_BOOK);
    settings().set<std::string>(core::TEXTS_START_KEY, core::PAGE_START_RIGHT);
    QApplication::processEvents();

    OpenNovel novel(QStringLiteral("Pages"));
    keepOnly(novel);
    core::ProjectManager& pm = novel.pm;
    pm.setChapterContent(novel.chapterId, QStringLiteral("<kml><p>A short storm.</p></kml>"));
    REQUIRE(pm.saveChapterContent(novel.chapterId));
    const QString road = addChapter(pm, QStringLiteral("The Road"), longKml(40));
    const QString end = addChapter(pm, QStringLiteral("The End"),
                                   QStringLiteral("<kml><p>The end.</p></kml>"));

    Window window;
    BookPageNumbers& numbers = window.navigator.pageNumbers();

    // Nothing counted while no editor shows pages: page 1 on its side
    window.navigator.onElementSelected(road, QStringLiteral("The Road"));
    gui::EditorPanel* panel = window.editor(window.tabs->currentIndex());
    numbers.countNow();
    CHECK_FALSE(numbers.firstPageOf(road).has_value());
    CHECK(panel->getBookEditor()->firstPageNumber() == 1);

    // The Page Layout view: the texts counted
    panel->getBookEditor()->setViewMode(editor::ViewMode::Page);
    numbers.countNow();
    const int roadPages = pagesOf(*panel);
    REQUIRE(roadPages > 2);
    // The Storm on page 1, a page left empty, The Road on page 3: a right one
    CHECK(firstPage(numbers, novel.chapterId) == TextFirstPage{0, 1, false});
    CHECK(firstPage(numbers, road) == TextFirstPage{2, 3, false});
    int endIndex = 2 + roadPages;
    endIndex += endIndex % 2;  // on a right page
    CHECK(firstPage(numbers, end) == TextFirstPage{endIndex, endIndex + 1, false});
    CHECK(panel->getBookEditor()->firstPageNumber() == 3);
    CHECK_FALSE(panel->getBookEditor()->appearance().pageMargins.firstPageLeft);
    panel->getBookEditor()->setCursorPosition({0, 0});
    CHECK(panel->pageStatus() == QStringLiteral("Page 3 (1 of %1)").arg(roadPages));

    // Go To: the number the page shows
    const editor::GoToOutcome outcome =
        panel->getBookEditor()->goToPlace(editor::PlaceKind::Page, QStringLiteral("4"));
    CHECK(outcome.result == editor::GoToResult::Done);
    CHECK(outcome.ordinal == 2);
    CHECK(outcome.firstNumber == 3);
    CHECK(panel->getBookEditor()->currentPage() == 2);
    CHECK(panel->getBookEditor()->goToPlace(editor::PlaceKind::Page, QStringLiteral("1")).result ==
          editor::GoToResult::NoNumber);

    // The next page, left or right: no empty pages
    settings().set<std::string>(core::TEXTS_START_KEY, core::PAGE_START_NEXT);
    QApplication::processEvents();
    numbers.countNow();
    CHECK(firstPage(numbers, road) == TextFirstPage{1, 2, true});
    CHECK(firstPage(numbers, end).index == 1 + roadPages);
    CHECK(panel->getBookEditor()->firstPageNumber() == 2);
    CHECK(panel->getBookEditor()->appearance().pageMargins.firstPageLeft);

    // A text of its own on a left page, the others on right ones
    settings().set<std::string>(core::TEXTS_START_KEY, core::PAGE_START_RIGHT);
    QApplication::processEvents();
    REQUIRE(pm.setFirstPage(end, core::PageStart::Left));
    window.navigator.refreshFirstPage(end);
    endIndex = 2 + roadPages;
    endIndex += 1 - endIndex % 2;  // on a left page
    CHECK(firstPage(numbers, end) == TextFirstPage{endIndex, endIndex + 1, true});

    // Each text from 1: the sides stay
    settings().set<std::string>(BookPageNumbers::NUMBERING_KEY, BookPageNumbers::NUMBERING_TEXT);
    QApplication::processEvents();
    numbers.countNow();
    CHECK(firstPage(numbers, road) == TextFirstPage{2, 1, false});
    CHECK(panel->getBookEditor()->firstPageNumber() == 1);
    CHECK(panel->pageStatus().startsWith(QStringLiteral("Page ")));
    CHECK_FALSE(panel->pageStatus().contains(QLatin1Char('(')));

    // Without mirror margins: no empty pages, no left pages
    settings().set<std::string>(BookPageNumbers::NUMBERING_KEY, BookPageNumbers::NUMBERING_BOOK);
    settings().set<bool>("editor.margins.mirrorEnabled", false);
    QApplication::processEvents();
    numbers.countNow();
    CHECK(firstPage(numbers, road) == TextFirstPage{1, 2, false});
    CHECK(firstPage(numbers, end) == TextFirstPage{1 + pagesOf(*panel), 2 + pagesOf(*panel),
                                                   false});
}

TEST_CASE("Book pages: a part with a text has a page for its title; the writing counts its "
          "text again",
          "[gui][pages]") {
    const KeptPageSettings kept;
    settings().set<bool>("editor.margins.mirrorEnabled", true);
    settings().set<std::string>(BookPageNumbers::NUMBERING_KEY, BookPageNumbers::NUMBERING_BOOK);
    settings().set<std::string>(core::TEXTS_START_KEY, core::PAGE_START_RIGHT);
    QApplication::processEvents();

    OpenNovel novel(QStringLiteral("Parts"));
    keepOnly(novel);
    core::ProjectManager& pm = novel.pm;
    const QString part = pm.addElement(pm.partKind(), QStringLiteral("Part One"),
                                       core::BookPlace::Main, QString(), -1, {novel.chapterId});
    REQUIRE_FALSE(part.isEmpty());
    const QString second =
        addChapter(pm, QStringLiteral("The Calm"), QStringLiteral("<kml><p>Calm.</p></kml>"));
    const QString empty =
        pm.addElement(pm.partKind(), QStringLiteral("Part Two"), core::BookPlace::Main);
    REQUIRE_FALSE(empty.isEmpty());

    Window window;
    BookPageNumbers& numbers = window.navigator.pageNumbers();
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    gui::EditorPanel* storm = window.editor(window.tabs->currentIndex());
    storm->getBookEditor()->setViewMode(editor::ViewMode::Page);
    numbers.countNow();

    // The title of the part on page 1, an empty page, The Storm on page 3, an empty page
    CHECK(firstPage(numbers, part) == TextFirstPage{0, 1, false});
    CHECK(firstPage(numbers, novel.chapterId) == TextFirstPage{2, 3, false});
    CHECK(firstPage(numbers, second) == TextFirstPage{4, 5, false});
    CHECK(storm->getBookEditor()->firstPageNumber() == 3);
    // A part with no text has no page
    CHECK_FALSE(numbers.firstPageOf(empty).has_value());

    // The open text written longer: counted again from its editor, a moment after
    storm->setContent(longKml(40));
    const int stormPages = pagesOf(*storm);
    REQUIRE(stormPages > 1);
    CHECK(test::waitUntil(
        [&numbers, &second, stormPages]() {
            const std::optional<TextFirstPage> first = numbers.firstPageOf(second);
            return first && first->index == 2 + stormPages + stormPages % 2;
        },
        10000));
}
