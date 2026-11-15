// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_plot_board.cpp
/// @brief The plot board and the outline of the book: the cards of the texts, the places they
/// move to, the cells of the outline, the keys, and what Ctrl+Z undoes

#include "document_window.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTableView>
#include <QUndoStack>

#include <cstdio>
#include <optional>
#include <string>

#include "test_support/window_parts.h"

using namespace kalahari;
using gui::PlotBoard;
using gui::PlotCard;

namespace {

/// Kind @p kindId of the base package
core::KindRef baseKind(const char* kindId) {
    return test::book().bookTypes().findKind(QStringLiteral("kalahari.base"),
                                             QString::fromLatin1(kindId));
}

/// What @p bands show: the titles of the texts, each part with its title before them, the
/// bands parted with "|": "Chapter 1, The Storm | Part One: The Calm"
std::string boardOf(const QList<gui::PlotBand>& bands) {
    QStringList shown;
    for (const gui::PlotBand& band : bands) {
        QStringList titles;
        for (const PlotCard& card : band.cards) {
            titles << card.title;
        }
        const QString texts = titles.join(QStringLiteral(", "));
        shown << (band.partId.isEmpty() ? texts : band.title + QStringLiteral(": ") + texts);
    }
    return shown.join(QStringLiteral(" | ")).toStdString();
}

/// What the plot board of the open book shows
std::string boardOfBook() {
    return boardOf(PlotBoard::bandsOf(test::book()));
}

/// The card of text @p id of the open book
PlotCard cardOf(const QString& id) {
    for (const PlotCard& card : PlotBoard::cardsOf(PlotBoard::bandsOf(test::book()))) {
        if (card.id == id) {
            return card;
        }
    }
    FAIL("No card of " << id.toStdString());
    return {};
}

/// Move text @p id of the open book @p steps places, as the board does
bool step(const QString& id, int steps) {
    auto& pm = test::book();
    const std::optional<core::ElementPlace> place = PlotBoard::stepPlace(pm, id, steps);
    return place && pm.moveElementTo(id, *place);
}

/// Key @p key with @p modifiers sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(widget, &event);
}

/// Whether the window's command of @p key waits while @p widget has the keys: the widget takes
/// the key for itself
bool takesKey(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();
    QCoreApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// The item of @p menu whose text is @p text; nullptr: none
QAction* actionOf(const QMenu& menu, const QString& text) {
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

/// A character or a place of the Workshop titled @p title, as the cards of the Workshop are
void addWorkshopCard(const char* kindId, const QString& title) {
    core::BookProject* project = test::book().project();
    REQUIRE(project != nullptr);
    core::ProjectElement card;
    card.id = QStringLiteral("card-") + title;
    card.kind.packageId = QStringLiteral("kalahari.base");
    card.kind.kindId = QString::fromLatin1(kindId);
    card.title = title;
    project->workshop.elements.append(card);
}

/// A novel open until the test ends: "Chapter 1" and "The Storm" outside the parts, then
/// "Part One" with "The Calm"
struct PlotNovel {
    test::OpenNovel novel{QStringLiteral("Rain")};
    core::ProjectManager& pm = novel.pm;
    QString first;
    QString storm = novel.chapterId;
    QString part;
    QString calm;

    PlotNovel() {
        const QList<PlotCard> cards = PlotBoard::cardsOf(PlotBoard::bandsOf(pm));
        REQUIRE(cards.size() == 2);
        first = cards.at(0).id;
        REQUIRE(cards.at(1).id == storm);
        part = pm.addElement(baseKind("part"), QStringLiteral("Part One"), core::BookPlace::Main);
        REQUIRE_FALSE(part.isEmpty());
        calm = pm.addElement(pm.chapterKindFor(core::BookPlace::Main, part),
                             QStringLiteral("The Calm"), core::BookPlace::Main, part);
        REQUIRE_FALSE(calm.isEmpty());
        REQUIRE(boardOfBook() == "Chapter 1, The Storm | Part One: The Calm");
    }
};

/// The setting of the board @p key, put back when the test ends
class KeptSetting {
public:
    explicit KeptSetting(const char* key)
        : m_key(key),
          m_value(core::SettingsManager::getInstance().get<std::string>(key, std::string())) {}
    ~KeptSetting() {
        try {
            core::SettingsManager::getInstance().set<std::string>(m_key, m_value);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("KeptSetting: the setting of the test stays\n", stderr);
        }
    }
    KeptSetting(const KeptSetting&) = delete;
    KeptSetting& operator=(const KeptSetting&) = delete;

private:
    const char* m_key;
    std::string m_value;
};

}  // namespace

TEST_CASE("Plot board: the texts of the main section in bands, and the places a card moves to",
          "[gui][plotboard]") {
    PlotNovel book;
    auto& pm = book.pm;
    const QString& storm = book.storm;
    core::ElementPlot plot;
    plot.pov = QStringLiteral("Anna");
    plot.place = QStringLiteral("The harbour");
    REQUIRE(pm.setPlot(storm, QStringLiteral("The storm comes."), plot));

    SECTION("a band of the texts outside the parts, and one of each part") {
        const QList<gui::PlotBand> bands = PlotBoard::bandsOf(pm);
        REQUIRE(bands.size() == 2);
        CHECK(bands.at(0).partId.isEmpty());
        CHECK(bands.at(1).partId == book.part);
        const PlotCard card = cardOf(storm);
        CHECK(card.kindName == QStringLiteral("Chapter"));
        CHECK(card.description == QStringLiteral("The storm comes."));
        CHECK(card.plot.pov == QStringLiteral("Anna"));
        CHECK(card.plot.place == QStringLiteral("The harbour"));
        CHECK(card.status == QStringLiteral("draft"));
        CHECK(card.number == 2);
        CHECK(card.partId.isEmpty());
        CHECK(cardOf(book.calm).number == 3);
        CHECK(cardOf(book.calm).partId == book.part);
    }

    SECTION("a step goes into the part, through it, and out of it") {
        REQUIRE(step(storm, 1));
        CHECK(boardOfBook() == "Chapter 1 | Part One: The Storm, The Calm");
        REQUIRE(step(storm, 1));
        CHECK(boardOfBook() == "Chapter 1 | Part One: The Calm, The Storm");
        REQUIRE(step(storm, 1));
        CHECK(boardOfBook() == "Chapter 1 | Part One: The Calm | The Storm");
        CHECK_FALSE(PlotBoard::stepPlace(pm, storm, 1));

        // Many steps go as far as they can
        REQUIRE(step(storm, -10));
        CHECK(boardOfBook() == "The Storm, Chapter 1 | Part One: The Calm");
        CHECK_FALSE(PlotBoard::stepPlace(pm, storm, -1));
    }

    SECTION("a card dropped beside another or on the title of a part") {
        std::optional<core::ElementPlace> place =
            PlotBoard::placeBeside(pm, storm, book.calm, true);
        REQUIRE(place);
        REQUIRE(pm.moveElementTo(storm, *place));
        CHECK(boardOfBook() == "Chapter 1 | Part One: The Calm, The Storm");

        place = PlotBoard::placeBeside(pm, storm, book.first, false);
        REQUIRE(place);
        REQUIRE(pm.moveElementTo(storm, *place));
        CHECK(boardOfBook() == "The Storm, Chapter 1 | Part One: The Calm");

        place = PlotBoard::placeInPart(pm, storm, book.part, false);
        REQUIRE(place);
        REQUIRE(pm.moveElementTo(storm, *place));
        CHECK(boardOfBook() == "Chapter 1 | Part One: The Storm, The Calm");

        CHECK_FALSE(PlotBoard::placeBeside(pm, storm, storm, true));
        CHECK_FALSE(PlotBoard::placeBeside(pm, storm, QStringLiteral("no such text"), true));
    }

    SECTION("the characters and the places of the Workshop are suggested") {
        addWorkshopCard("character", QStringLiteral("Tom"));
        addWorkshopCard("character", QStringLiteral("Anna"));
        addWorkshopCard("location", QStringLiteral("The harbour"));
        CHECK(PlotBoard::povSuggestions(pm) ==
              QStringList{QStringLiteral("Anna"), QStringLiteral("Tom")});
        CHECK(PlotBoard::placeSuggestions(pm) == QStringList{QStringLiteral("The harbour")});
    }

    SECTION("the colors of the cards") {
        gui::PlotColors colors;
        colors.draft = QColor(Qt::gray);
        colors.revision = QColor(Qt::blue);
        colors.finished = QColor(Qt::green);
        colors.own = {QColor(Qt::red), QColor(Qt::yellow)};
        PlotCard card = cardOf(storm);
        const QStringList povs = gui::povsOf(PlotBoard::cardsOf(PlotBoard::bandsOf(pm)));
        CHECK(povs == QStringList{QStringLiteral("Anna")});
        const auto colorOf = [&](const char* colorBy) {
            return colors.ofCard(card, QString::fromLatin1(colorBy), povs);
        };
        CHECK(colorOf(PlotBoard::COLOR_STATUS) == QColor(Qt::gray));
        CHECK(colorOf(PlotBoard::COLOR_POV) == QColor(Qt::red));
        CHECK_FALSE(colorOf(PlotBoard::COLOR_OWN).isValid());
        card.plot.color = QStringLiteral("teal");
        CHECK(colorOf(PlotBoard::COLOR_OWN) == QColor(Qt::yellow));
        CHECK_FALSE(colorOf(PlotBoard::COLOR_NONE).isValid());
    }

    SECTION("one text and one word, without the plural") {
        CHECK(PlotBoard::textsText(1).toStdString() == "1 text");
        CHECK(PlotBoard::textsText(3).toStdString() == "3 texts");
        CHECK(PlotBoard::wordsText(1).toStdString() == "1 word");
        CHECK(PlotBoard::wordsText(0).toStdString() == "0 words");
    }
}

TEST_CASE("Plot board: the outline shows the parts over their texts and asks for the changes",
          "[gui][plotboard]") {
    PlotNovel book;
    using Column = gui::PlotOutlineModel;

    gui::PlotOutlineModel model;
    model.setBands(PlotBoard::bandsOf(book.pm));
    REQUIRE(model.rowCount() == 4);
    CHECK(model.cardIdAt(1) == book.storm);
    CHECK(model.cardIdAt(2).isEmpty());
    CHECK(model.rowOf(book.part) == 2);
    CHECK(model.rowOf(book.calm) == 3);
    CHECK(model.index(2, Column::TitleColumn).data().toString() == QStringLiteral("Part One"));
    CHECK(model.index(2, Column::TitleColumn).data(Column::PART_ROLE).toBool());
    CHECK(model.index(3, Column::NumberColumn).data().toInt() == 3);
    CHECK(model.index(3, Column::StatusColumn).data().toString() == QStringLiteral("Draft"));

    // A part's row and the counted cells cannot be edited
    CHECK_FALSE(model.flags(model.index(2, Column::TitleColumn)) & Qt::ItemIsEditable);
    CHECK_FALSE(model.flags(model.index(3, Column::WordsColumn)) & Qt::ItemIsEditable);
    CHECK(model.flags(model.index(3, Column::PovColumn)) & Qt::ItemIsEditable);

    // An edit asks for the change and keeps the cell as the book has it
    QStringList edited;
    QObject::connect(&model, &gui::PlotOutlineModel::cellEdited,
                     [&edited](const QString& id, int column, const QString& value) {
                         edited << QStringLiteral("%1 %2 %3").arg(id).arg(column).arg(value);
                     });
    CHECK(model.setData(model.index(3, Column::PovColumn), QStringLiteral(" Anna "),
                        Qt::EditRole));
    CHECK(edited ==
          QStringList{QStringLiteral("%1 %2 Anna").arg(book.calm).arg(Column::PovColumn)});
    CHECK(model.index(3, Column::PovColumn).data().toString().isEmpty());

    // A text keeps its title, and the same text is no change
    CHECK_FALSE(model.setData(model.index(3, Column::TitleColumn), QString(), Qt::EditRole));
    CHECK_FALSE(model.setData(model.index(3, Column::TitleColumn), QStringLiteral("The Calm"),
                              Qt::EditRole));
    CHECK(edited.size() == 1);

    model.setReadOnly(true);
    CHECK_FALSE(model.flags(model.index(3, Column::PovColumn)) & Qt::ItemIsEditable);
}

TEST_CASE("Plot board: on its tab the keys move and edit the cards, and Ctrl+Z undoes it",
          "[gui][plotboard]") {
    gui::registerAllIcons();
    const KeptSetting view(PlotBoard::VIEW_SETTING);
    const KeptSetting layout(PlotBoard::LAYOUT_SETTING);
    core::SettingsManager::getInstance().set<std::string>(PlotBoard::LAYOUT_SETTING,
                                                          PlotBoard::LAYOUT_COLUMNS);
    PlotNovel book;
    auto& pm = book.pm;
    const QString& storm = book.storm;
    const QString& calm = book.calm;

    test::Window window;
    QList<bool> propertiesShown;
    gui::PlotBoardCoordinator boards{pm, window.tabs, &window.navigator, &window.statusBar,
                                     [&propertiesShown](bool keys) { propertiesShown << keys; }};
    gui::PlotBoardPanel* panel =
        boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_BOARD), true);
    REQUIRE(panel != nullptr);
    CHECK(window.tabs->currentWidget() == panel);
    CHECK(window.tabs->tabText(window.tabs->indexOf(panel)) == QStringLiteral("Plot Board"));
    CHECK(panel->view() == QLatin1String(PlotBoard::VIEW_BOARD));
    // One tab of the board
    CHECK(boards.openBoard(QString(), false) == panel);
    CHECK(window.tabs->count() == 1);

    gui::PlotBoardView* board = panel->board();
    const auto shown = [board]() { return boardOf(board->bands()); };
    CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
    REQUIRE(board->setCurrentCard(storm));

    SECTION("Ctrl with an arrow moves the card, Ctrl+Z and Ctrl+Y undo and redo it") {
        // The keys of the board, not the commands of the window
        CHECK(takesKey(board, Qt::Key_Right, Qt::ControlModifier));
        CHECK(takesKey(board, Qt::Key_Z, Qt::ControlModifier));
        CHECK(takesKey(board, Qt::Key_Y, Qt::ControlModifier));
        CHECK(takesKey(board, Qt::Key_F2));
        CHECK(takesKey(board, Qt::Key_Insert));
        CHECK_FALSE(takesKey(board, Qt::Key_S, Qt::ControlModifier));

        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        CHECK(board->currentCard() == storm);
        CHECK(panel->message() == QStringLiteral("\"The Storm\" moved."));

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
        CHECK(panel->message() == QStringLiteral("Undone: moving \"The Storm\"."));
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        CHECK(panel->message() == QStringLiteral("Redone: moving \"The Storm\"."));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(panel->message() == QStringLiteral("Nothing to undo on the board."));

        // At the start of the book a card goes no further
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(shown() == "The Storm, Chapter 1 | Part One: The Calm");
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(shown() == "The Storm, Chapter 1 | Part One: The Calm");
        CHECK(panel->message() == QStringLiteral("\"The Storm\" cannot move further."));
    }

    SECTION("F2 edits the card on the board, Ctrl+Enter keeps it, Ctrl+Z undoes it") {
        press(board, Qt::Key_F2);
        gui::PlotCardEditor* editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        CHECK(editor->titleEdit()->text() == QStringLiteral("The Storm"));
        editor->titleEdit()->setText(QStringLiteral("The Great Storm"));
        editor->descriptionEdit()->setPlainText(QStringLiteral("Everything breaks."));
        editor->povEdit()->setText(QStringLiteral("Anna"));
        press(editor->titleEdit(), Qt::Key_Return, Qt::ControlModifier);
        CHECK(board->cardEditor() == nullptr);
        const core::ProjectElement* element = pm.findElement(storm);
        REQUIRE(element != nullptr);
        CHECK(element->title == QStringLiteral("The Great Storm"));
        CHECK(element->description == QStringLiteral("Everything breaks."));
        CHECK(element->plot.pov == QStringLiteral("Anna"));
        CHECK(panel->message() == QStringLiteral("The card \"The Great Storm\" changed."));
        CHECK(shown() == "Chapter 1, The Great Storm | Part One: The Calm");

        press(board, Qt::Key_Z, Qt::ControlModifier);
        element = pm.findElement(storm);
        CHECK(element->title == QStringLiteral("The Storm"));
        CHECK(element->description.isEmpty());
        CHECK(element->plot.pov.isEmpty());

        // Esc leaves the card as it was
        press(board, Qt::Key_F2);
        editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        editor->titleEdit()->setText(QStringLiteral("Something else"));
        press(editor->titleEdit(), Qt::Key_Escape);
        CHECK(board->cardEditor() == nullptr);
        CHECK(pm.findElement(storm)->title == QStringLiteral("The Storm"));
    }

    SECTION("the menu of the card gives it a state and a color") {
        QMenu* menu = panel->cardMenu(storm);
        QAction* states = actionOf(*menu, QStringLiteral("&State"));
        REQUIRE(states != nullptr);
        QAction* revision = actionOf(*states->menu(), QStringLiteral("Revision"));
        REQUIRE(revision != nullptr);
        revision->trigger();
        CHECK(core::ProjectManager::statusOf(*pm.findElement(storm)) ==
              QStringLiteral("revision"));
        CHECK(panel->message() ==
              QStringLiteral("\"The Storm\" is now in the state: Revision."));

        menu = panel->cardMenu(storm);
        QAction* colors = actionOf(*menu, QStringLiteral("Card &Color"));
        REQUIRE(colors != nullptr);
        QAction* teal = actionOf(*colors->menu(), QStringLiteral("Teal"));
        REQUIRE(teal != nullptr);
        teal->trigger();
        CHECK(pm.findElement(storm)->plot.color == QStringLiteral("teal"));

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(pm.findElement(storm)->plot.color.isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(core::ProjectManager::statusOf(*pm.findElement(storm)) == QStringLiteral("draft"));
    }

    SECTION("Ctrl+X marks the card, Ctrl+V moves it after another, Esc forgets it") {
        press(board, Qt::Key_X, Qt::ControlModifier);
        CHECK(board->markedCard() == storm);
        CHECK(boards.markedCard() == storm);
        CHECK(takesKey(board, Qt::Key_Escape));
        press(board, Qt::Key_Escape);
        CHECK(board->markedCard().isEmpty());
        CHECK(panel->message().isEmpty());

        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(calm));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(board->markedCard().isEmpty());
        CHECK(shown() == "Chapter 1 | Part One: The Calm, The Storm");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
    }

    SECTION("a change of the book elsewhere shows on the board and forgets what can be undone") {
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(boards.undoStack()->canUndo());
        REQUIRE(pm.renameElement(calm, QStringLiteral("The Quiet")));
        CHECK_FALSE(boards.undoStack()->canUndo());
        QCoreApplication::processEvents();
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Quiet");
    }

    SECTION("Insert adds a text of the kind of the card right after it and edits its card") {
        REQUIRE(board->setCurrentCard(calm));
        press(board, Qt::Key_Insert);
        const QList<PlotCard> cards = PlotBoard::cardsOf(board->bands());
        REQUIRE(cards.size() == 4);
        const QString added = cards.at(3).id;
        CHECK(cards.at(3).partId == book.part);
        CHECK(board->currentCard() == added);
        REQUIRE(board->cardEditor() != nullptr);
        CHECK(board->cardEditor()->elementId() == added);
        CHECK(panel->message() ==
              QStringLiteral("Added \"%1\" after \"The Calm\".").arg(cards.at(3).title));
        press(board->cardEditor()->titleEdit(), Qt::Key_Escape);
    }

    SECTION("Enter opens the text, Alt+Enter shows it in the Properties panel with the keys") {
        press(board, Qt::Key_Return, Qt::AltModifier);
        CHECK(propertiesShown == QList<bool>{true});
        press(board, Qt::Key_Return);
        auto* editor = qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget());
        REQUIRE(editor != nullptr);
        CHECK(editor->property("elementId").toString() == storm);

        // The board comes back to the front as it was
        CHECK(boards.openBoard(QString(), true) == panel);
        CHECK(window.tabs->currentWidget() == panel);
        CHECK(board->currentCard() == storm);
    }

    SECTION("the outline: Book > Outline shows it, and its keys are those of the board") {
        CHECK(boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_OUTLINE), true) == panel);
        CHECK(panel->view() == QLatin1String(PlotBoard::VIEW_OUTLINE));
        CHECK(core::SettingsManager::getInstance().get<std::string>(PlotBoard::VIEW_SETTING,
                                                                    "") ==
              PlotBoard::VIEW_OUTLINE);
        QTableView* outline = panel->outline();
        gui::PlotOutlineModel* model = panel->outlineModel();
        CHECK(model->cardIdAt(outline->currentIndex().row()) == storm);
        CHECK(takesKey(outline, Qt::Key_Down, Qt::ControlModifier));
        CHECK(takesKey(outline, Qt::Key_Z, Qt::ControlModifier));
        CHECK_FALSE(takesKey(outline, Qt::Key_Down));

        press(outline, Qt::Key_Down, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        CHECK(model->cardIdAt(outline->currentIndex().row()) == storm);
        press(outline, Qt::Key_Z, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");

        // A cell edited asks for the change of the book
        const int row = model->rowOf(calm);
        const QModelIndex place = model->index(row, gui::PlotOutlineModel::PlaceColumn);
        REQUIRE(model->setData(place, QStringLiteral("The lighthouse"), Qt::EditRole));
        CHECK(pm.findElement(calm)->plot.place == QStringLiteral("The lighthouse"));
        CHECK(model->index(row, gui::PlotOutlineModel::PlaceColumn).data().toString() ==
              QStringLiteral("The lighthouse"));
    }
}
