// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_plan_texts.cpp
/// @brief The Book Plan: a chapter in the plan without a file, its file created and taken back
/// to the Trash with Ctrl+Z, the bar of the commands, the pencil and the button of a card

#include "kalahari_tests/gui/document_window.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/file_trash.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QModelIndex>
#include <QMouseEvent>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdio>
#include <string>

#include "kalahari_tests/test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::PlotBoard;
using gui::PlotCard;
using Command = gui::PlotBoardPanel::Command;

namespace {

void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// A click of the left button at @p pos of the viewport of @p board
void click(const gui::PlotBoardView& board, const QPoint& pos) {
    QMouseEvent down(QEvent::MouseButtonPress, pos, board.viewport()->mapToGlobal(pos),
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(board.viewport(), &down);
    QMouseEvent up(QEvent::MouseButtonRelease, pos, board.viewport()->mapToGlobal(pos),
                   Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(board.viewport(), &up);
}

/// The setting @p key, put back when the test ends
class KeptSetting {
public:
    explicit KeptSetting(const char* key)
        : m_key(key),
          m_value(core::SettingsManager::getInstance().get<std::string>(key, std::string())) {}
    ~KeptSetting() {
        try {
            core::SettingsManager::getInstance().set<std::string>(m_key, m_value);
        } catch (...) {
            (void)std::fputs("KeptSetting: the setting of the test stays\n", stderr);
        }
    }
    KeptSetting(const KeptSetting&) = delete;
    KeptSetting& operator=(const KeptSetting&) = delete;
    KeptSetting(KeptSetting&&) = delete;
    KeptSetting& operator=(KeptSetting&&) = delete;

private:
    const char* m_key;
    std::string m_value;
};

/// A Trash of the test until it ends: the files it gets are gone and listed
struct TestTrash {
    QStringList files;
    TestTrash() {
        core::setTrashForTests([](const QString& /*path*/) { return true; },
                               [this](const QString& path) {
                                   files << path;
                                   return QFile::remove(path);
                               });
    }
    ~TestTrash() { core::setTrashForTests({}, {}); }
    TestTrash(const TestTrash&) = delete;
    TestTrash& operator=(const TestTrash&) = delete;
    TestTrash(TestTrash&&) = delete;
    TestTrash& operator=(TestTrash&&) = delete;
};

/// The row of the Navigator titled @p title; nullptr: none
QTreeWidgetItem* navigatorRow(gui::NavigatorPanel& navigator, const QString& title) {
    auto* tree = navigator.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == title) {
            return *it;
        }
    }
    return nullptr;
}

/// "Chapter 1" and "The Storm" of a novel on the board of the Book Plan, "The Storm" chosen
struct PlanBook {
    KeptSetting view{PlotBoard::VIEW_SETTING};
    KeptSetting variant{PlotBoard::SCENES_VARIANT_SETTING};
    TestTrash trash;
    test::OpenNovel novel{QStringLiteral("Rain")};
    core::ProjectManager& pm = novel.pm;
    QString storm = novel.chapterId;
    test::Window window;
    gui::PlotBoardCoordinator boards{pm, window.tabs, &window.navigator, &window.statusBar,
                                     [](bool /*keys*/) {}};
    gui::PlotBoardPanel* panel = nullptr;
    gui::PlotBoardView* board = nullptr;

    PlanBook() {
        core::SettingsManager::getInstance().set<std::string>(PlotBoard::SCENES_VARIANT_SETTING,
                                                              PlotBoard::SCENES_BOTH);
        window.window.resize(1200, 800);
        panel = boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_BOARD), true);
        REQUIRE(panel != nullptr);
        panel->resize(1200, 800);
        board = panel->board();
        REQUIRE(board->setCurrentCard(storm));
    }

    /// The card of text @p id on the board
    [[nodiscard]] PlotCard cardOf(const QString& id) const {
        const std::optional<PlotCard> found = panel->card(id);
        REQUIRE(found.has_value());
        return *found;
    }

    /// Ctrl+Shift+Insert on the board, and the card editor it opens closed
    QString addPlanned() {
        const QStringList before = ids();
        press(board, Qt::Key_Insert, Qt::ControlModifier | Qt::ShiftModifier);
        if (gui::PlotCardEditor* editor = board->cardEditor()) {
            press(editor->titleEdit(), Qt::Key_Escape);
        }
        QString added;
        for (const QString& id : ids()) {
            if (!before.contains(id)) {
                added = id;
            }
        }
        REQUIRE_FALSE(added.isEmpty());
        return added;
    }

    [[nodiscard]] QStringList ids() const {
        QStringList found;
        for (const PlotCard& card : PlotBoard::cardsOf(panel->bands())) {
            found << card.id;
        }
        return found;
    }
};

}  // namespace

TEST_CASE("Book plan: a chapter in the plan has no file until Create the Chapter File",
          "[gui][plotboard][plan]") {
    PlanBook book;
    gui::PlotBoardPanel* panel = book.panel;
    gui::PlotBoardView* board = book.board;
    core::ProjectManager& pm = book.pm;

    const QString planned = book.addPlanned();
    // After "The Storm", without a file, words and a state
    CHECK(book.ids().indexOf(planned) == book.ids().indexOf(book.storm) + 1);
    const core::ProjectElement* element = pm.findElement(planned);
    REQUIRE(element != nullptr);
    const QString title = element->title;
    CHECK(pm.isPlannedText(planned));
    CHECK(element->file.isEmpty());
    CHECK(pm.wordCount(planned) == 0);
    CHECK(book.cardOf(planned).planned);
    CHECK(panel->message().contains(QStringLiteral("without a file yet")));
    CHECK(panel->currentCard() == planned);

    // The bar names what Enter does, the state waits for the text
    CHECK(panel->command(Command::Open)->text() == QStringLiteral("Create the Chapter File"));
    CHECK_FALSE(panel->command(Command::State)->isEnabled());
    CHECK(panel->command(Command::Delete)->text() == QStringLiteral("Delete from the Plan"));
    // The line under the board says what it is, once the bar of what was done is gone
    CHECK(panel->hint().isEmpty());
    panel->showMessage(QString(), false);
    CHECK(panel->hint().contains(QStringLiteral("in the plan")));
    // The card has a button at its bottom that does it
    CHECK(gui::PlotBoardView::planButtonText(book.cardOf(planned)) == QStringLiteral("Create"));
    CHECK_FALSE(board->planButtonRect(planned).isEmpty());

    // The Navigator shows it in italics, "in the plan"; the outline says so in its state
    book.window.navigator.refreshNavigator();
    const QTreeWidgetItem* row = navigatorRow(*book.window.navigatorPanel, title);
    REQUIRE(row != nullptr);
    CHECK(row->font(0).italic());
    CHECK(row->toolTip(0).contains(QStringLiteral("in the plan")));
    gui::PlotOutlineModel* outline = panel->outlineModel();
    const QModelIndex state =
        outline->index(outline->rowOf(planned), gui::PlotOutlineModel::STATUS_COLUMN);
    CHECK(outline->data(state, Qt::DisplayRole).toString() == PlotBoard::plannedName());

    SECTION("the button of the card creates the file, Ctrl+Z puts it back into the plan") {
        click(*board, board->planButtonRect(planned).center());
        element = pm.findElement(planned);
        REQUIRE(element != nullptr);
        REQUIRE_FALSE(element->file.isEmpty());
        const QString file = pm.filePathOf(*element);
        CHECK(QFile::exists(file));
        CHECK_FALSE(pm.isPlannedText(planned));
        CHECK(panel->message().contains(QStringLiteral("Created the chapter file")));
        CHECK(panel->command(Command::Open)->text() == QStringLiteral("Open"));

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(pm.isPlannedText(planned));
        CHECK_FALSE(QFile::exists(file));
        CHECK(book.trash.files == QStringList{file});
        CHECK(pm.findElement(planned)->title == title);

        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK_FALSE(pm.isPlannedText(planned));
        CHECK(QFile::exists(pm.filePathOf(*pm.findElement(planned))));
    }

    SECTION("Enter creates it too; Delete takes a chapter in the plan out without a question") {
        REQUIRE(board->setCurrentCard(planned));
        press(board, Qt::Key_Return);
        CHECK_FALSE(pm.isPlannedText(planned));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        REQUIRE(pm.isPlannedText(planned));
        REQUIRE(board->setCurrentCard(planned));
        press(board, Qt::Key_Delete);
        CHECK(pm.findElement(planned) == nullptr);
        CHECK(panel->message().contains(QStringLiteral("from the plan")));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(pm.isPlannedText(planned));
    }
}

TEST_CASE("Book plan: the pencil edits a card, the bar adds texts and scenes",
          "[gui][plotboard][plan]") {
    PlanBook book;
    gui::PlotBoardPanel* panel = book.panel;
    gui::PlotBoardView* board = book.board;

    // The pencil on every card edits it, as F2
    const QRect pencil = board->pencilRect(book.storm);
    REQUIRE_FALSE(pencil.isEmpty());
    click(*board, pencil.center());
    REQUIRE(board->cardEditor() != nullptr);
    press(board->cardEditor()->titleEdit(), Qt::Key_Escape);
    CHECK(board->cardEditor() == nullptr);

    // The commands of the bar, with their keys in their tips
    // As the system writes the keys (on macOS ⇧⌘Ins)
    CHECK(panel->command(Command::PlanText)
              ->toolTip()
              .contains(gui::ShortcutRules::keysText(
                  QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Insert))));
    const QStringList before = book.ids();
    panel->command(Command::NewText)->trigger();
    if (gui::PlotCardEditor* editor = board->cardEditor()) {
        press(editor->titleEdit(), Qt::Key_Escape);
    }
    CHECK(book.ids().size() == before.size() + 1);
    CHECK_FALSE(book.pm.isPlannedText(panel->currentCard()));

    // A scene in the plan of "The Storm", then one written: a scene break in its text
    REQUIRE(board->setCurrentCard(book.storm));
    panel->command(Command::PlanScene)->trigger();
    if (gui::PlotCardEditor* editor = board->cardEditor()) {
        press(editor->titleEdit(), Qt::Key_Escape);
    }
    CHECK(book.pm.findElement(book.storm)->plot.planned.size() == 1);
    REQUIRE(board->setCurrentCard(book.storm));
    panel->command(Command::NewScene)->trigger();
    if (gui::PlotCardEditor* editor = board->cardEditor()) {
        press(editor->titleEdit(), Qt::Key_Escape);
    }
    CHECK(book.pm.chapterText(book.storm).contains(QStringLiteral("scene-break")));
}
