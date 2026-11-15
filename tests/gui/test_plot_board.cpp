// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_plot_board.cpp
/// @brief The plot board and the outline of the book: the cards of the texts, the places they
/// move to, the cells of the outline, the keys, and what Ctrl+Z undoes

#include "document_window.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>

#include "test_support/window_parts.h"

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
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
    const QList<PlotCard> cards = PlotBoard::cardsOf(PlotBoard::bandsOf(test::book()));
    const auto found =
        std::ranges::find_if(cards, [&id](const PlotCard& card) { return card.id == id; });
    REQUIRE(found != cards.cend());
    return *found;
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
    const QList<QAction*> actions = menu.actions();
    const auto found = std::ranges::find_if(
        actions, [&text](const QAction* action) { return action->text() == text; });
    return found != actions.cend() ? *found : nullptr;
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

/// The setting of the board @p key that is true or false, put back when the test ends
class KeptSwitch {
public:
    explicit KeptSwitch(const char* key)
        : m_key(key), m_value(core::SettingsManager::getInstance().get<bool>(key, true)) {}
    ~KeptSwitch() {
        try {
            core::SettingsManager::getInstance().set<bool>(m_key, m_value);
        } catch (...) {
            (void)std::fputs("KeptSwitch: the setting of the test stays\n", stderr);
        }
    }
    KeptSwitch(const KeptSwitch&) = delete;
    KeptSwitch& operator=(const KeptSwitch&) = delete;
    KeptSwitch(KeptSwitch&&) = delete;
    KeptSwitch& operator=(KeptSwitch&&) = delete;

private:
    const char* m_key;
    bool m_value;
};

/// The text of "The Storm": three scenes, the second titled "Night"
constexpr const char* STORM_SCENES = "<kml><p>One two</p><scene-break title=\"Night\"/>"
                                     "<p>Three four five</p><scene-break/><p>Six</p></kml>";

/// The words of each scene of @p scenes, in their order: "2 3 1"
std::string wordsOf(const QList<core::KmlScene>& scenes) {
    QStringList shown;
    for (const core::KmlScene& scene : scenes) {
        shown << QString::number(scene.words.words);
    }
    return shown.join(QLatin1Char(' ')).toStdString();
}

/// The words of each scene card of text @p textId on @p board, in their order: "2 3 1"
std::string shownWords(const gui::PlotBoardView& board, const QString& textId) {
    QStringList shown;
    for (const PlotCard& card : PlotBoard::cardsOf(board.bands())) {
        for (const PlotCard& scene : card.id == textId ? card.scenes : QList<PlotCard>()) {
            shown << QString::number(scene.words);
        }
    }
    return shown.join(QLatin1Char(' ')).toStdString();
}

/// The message window shown now; nullptr: none
gui::dialogs::MessageDialog* shownMessage() {
    using gui::dialogs::MessageDialog;
    if (auto* message = qobject_cast<MessageDialog*>(QApplication::activeModalWidget())) {
        return message;
    }
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* message = qobject_cast<MessageDialog*>(widget);
        if (message != nullptr && message->isVisible()) {
            return message;
        }
    }
    return nullptr;
}

/// Run @p command and give the message window it shows @p answer: the button of its action,
/// the other one, or Cancel; @p asked gets what it asks, with its title
/// @return Whether a message window was shown
bool answering(const std::function<void()>& command, gui::dialogs::MessageDialog::Answer answer,
               QString* asked = nullptr) {
    using gui::dialogs::MessageDialog;
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, answer, asked]() {
        MessageDialog* message = shownMessage();
        if (message == nullptr) {
            return;
        }
        shown = true;
        if (asked != nullptr) {
            *asked = message->clipboardText();
        }
        QPushButton* button = message->cancelButton();
        if (answer == MessageDialog::Answer::Accept) {
            button = message->acceptButton();
        } else if (answer == MessageDialog::Answer::Alternative) {
            button = message->alternativeButton();
        }
        if (button != nullptr) {
            button->click();
        } else {
            message->reject();
        }
    });
    timer.start();
    command();
    return shown;
}

/// "The Storm" of a novel with three scenes (STORM_SCENES), on the plot board in columns with
/// the scenes shown and the card of "The Storm" chosen, until the test ends
struct StormBoard {
    KeptSetting view{PlotBoard::VIEW_SETTING};
    KeptSetting layout{PlotBoard::LAYOUT_SETTING};
    KeptSwitch scenes{PlotBoard::SCENES_SETTING};
    PlotNovel book;
    core::ProjectManager& pm = book.pm;
    QString storm = book.storm;
    QString first = PlotBoard::sceneId(storm, 0);
    QString night = PlotBoard::sceneId(storm, 1);
    QString last = PlotBoard::sceneId(storm, 2);
    test::Window window;
    gui::PlotBoardCoordinator boards{pm, window.tabs, &window.navigator, &window.statusBar,
                                     [](bool /*keys*/) {}};
    gui::PlotBoardPanel* panel = nullptr;
    gui::PlotBoardView* board = nullptr;

    StormBoard() {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<std::string>(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_COLUMNS);
        settings.set<bool>(PlotBoard::SCENES_SETTING, true);
        pm.setChapterContent(storm, QString::fromLatin1(STORM_SCENES));
        REQUIRE(pm.saveChapterContent(storm));
        panel = boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_BOARD), true);
        REQUIRE(panel != nullptr);
        board = panel->board();
        REQUIRE(board->setCurrentCard(storm));
    }

    /// The words of each scene of "The Storm" in its file, in their order: "2 3 1"
    [[nodiscard]] std::string words() const { return wordsOf(pm.scenesOf(storm)); }

    /// The title of scene @p scene of "The Storm"
    [[nodiscard]] QString titleOf(int scene) const {
        return pm.scenesOf(storm).at(scene).plot.title;
    }
};

/// What is said under the board when a scene was to go to another text
const QString& toAnotherText() {
    static const QString TEXT = QStringLiteral(
        "A scene moves to another text in the editor: cut it there and paste it in the other "
        "text.");
    return TEXT;
}

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
    CHECK(model.index(2, Column::TITLE_COLUMN).data().toString() == QStringLiteral("Part One"));
    CHECK(model.index(2, Column::TITLE_COLUMN).data(Column::PART_ROLE).toBool());
    CHECK(model.index(3, Column::NUMBER_COLUMN).data().toInt() == 3);
    CHECK(model.index(3, Column::STATUS_COLUMN).data().toString() == QStringLiteral("Draft"));

    // A part's row and the counted cells cannot be edited
    CHECK_FALSE(model.flags(model.index(2, Column::TITLE_COLUMN)) & Qt::ItemIsEditable);
    CHECK_FALSE(model.flags(model.index(3, Column::WORDS_COLUMN)) & Qt::ItemIsEditable);
    CHECK(model.flags(model.index(3, Column::POV_COLUMN)) & Qt::ItemIsEditable);

    // An edit asks for the change and keeps the cell as the book has it
    QStringList edited;
    QObject::connect(&model, &gui::PlotOutlineModel::cellEdited,
                     [&edited](const QString& id, int column, const QString& value) {
                         edited << QStringLiteral("%1 %2 %3").arg(id).arg(column).arg(value);
                     });
    CHECK(model.setData(model.index(3, Column::POV_COLUMN), QStringLiteral(" Anna "),
                        Qt::EditRole));
    CHECK(edited ==
          QStringList{QStringLiteral("%1 %2 Anna").arg(book.calm).arg(Column::POV_COLUMN)});
    CHECK(model.index(3, Column::POV_COLUMN).data().toString().isEmpty());

    // A text keeps its title, and the same text is no change
    CHECK_FALSE(model.setData(model.index(3, Column::TITLE_COLUMN), QString(), Qt::EditRole));
    CHECK_FALSE(model.setData(model.index(3, Column::TITLE_COLUMN), QStringLiteral("The Calm"),
                              Qt::EditRole));
    CHECK(edited.size() == 1);

    model.setReadOnly(true);
    CHECK_FALSE(model.flags(model.index(3, Column::POV_COLUMN)) & Qt::ItemIsEditable);
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
        const gui::PlotCardEditor* editor = board->cardEditor();
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
        const QMenu* menu = panel->cardMenu(storm);
        const QAction* states = actionOf(*menu, QStringLiteral("&State"));
        REQUIRE(states != nullptr);
        QAction* revision = actionOf(*states->menu(), QStringLiteral("Revision"));
        REQUIRE(revision != nullptr);
        revision->trigger();
        CHECK(core::ProjectManager::statusOf(*pm.findElement(storm)) ==
              QStringLiteral("revision"));
        CHECK(panel->message() ==
              QStringLiteral("\"The Storm\" is now in the state: Revision."));

        menu = panel->cardMenu(storm);
        const QAction* colors = actionOf(*menu, QStringLiteral("Card &Color"));
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
        const QModelIndex place = model->index(row, gui::PlotOutlineModel::PLACE_COLUMN);
        REQUIRE(model->setData(place, QStringLiteral("The lighthouse"), Qt::EditRole));
        CHECK(pm.findElement(calm)->plot.place == QStringLiteral("The lighthouse"));
        CHECK(model->index(row, gui::PlotOutlineModel::PLACE_COLUMN).data().toString() ==
              QStringLiteral("The lighthouse"));
    }
}

TEST_CASE("Plot board: the scenes of a text under its card, in a drawer and in the outline",
          "[gui][plotboard][scenes]") {
    gui::registerAllIcons();
    StormBoard scenes;
    auto& settings = core::SettingsManager::getInstance();
    const PlotNovel& book = scenes.book;
    auto& pm = scenes.pm;
    const QString& storm = scenes.storm;
    const QString& first = scenes.first;
    const QString& night = scenes.night;
    const QString& last = scenes.last;
    gui::PlotBoardCoordinator& boards = scenes.boards;
    gui::PlotBoardPanel* panel = scenes.panel;
    gui::PlotBoardView* board = scenes.board;
    const auto plotOf = [&pm, &storm](int scene) { return pm.scenesOf(storm).at(scene).plot; };

    SECTION("a text with scene breaks has a card of each scene") {
        const PlotCard card = cardOf(storm);
        REQUIRE(card.scenes.size() == 3);
        CHECK(card.scenes.at(1).id == night);
        CHECK(card.scenes.at(1).kindName == QStringLiteral("Scene 2"));
        CHECK(card.scenes.at(1).title == QStringLiteral("Night"));
        CHECK(card.scenes.at(1).textId == storm);
        CHECK(card.scenes.at(1).number == card.number);
        CHECK(card.scenes.at(0).words == 2);
        CHECK(card.scenes.at(1).words == 3);
        CHECK(card.scenes.at(2).words == 1);
        CHECK(card.scenes.at(2).status == card.status);
        // A text without scene breaks has none
        CHECK(cardOf(book.calm).scenes.isEmpty());
        // The texts are counted without their scenes
        CHECK(PlotBoard::cardsOf(PlotBoard::bandsOf(pm)).size() == 3);
        CHECK(PlotBoard::allCardsOf(PlotBoard::bandsOf(pm)).size() == 6);

        const auto scene = PlotBoard::sceneOf(night);
        REQUIRE(scene.has_value());
        CHECK(scene->first == storm);
        CHECK(scene->second == 1);
        CHECK_FALSE(PlotBoard::sceneOf(storm).has_value());
        CHECK(PlotBoard::scenesText(1) == QStringLiteral("1 scene"));
        CHECK(PlotBoard::scenesText(3) == QStringLiteral("3 scenes"));

        // An open text with changes shows its own scenes
        const QHash<QString, QString> unsaved{
            {storm, QStringLiteral("<kml><p>a</p><scene-break/><p>b</p></kml>")}};
        bool found = false;
        for (const PlotCard& each : PlotBoard::cardsOf(PlotBoard::bandsOf(pm, unsaved))) {
            if (each.id == storm) {
                found = true;
                CHECK(each.scenes.size() == 2);
            }
        }
        CHECK(found);
    }

    SECTION("in columns the scenes are under the card of their text, and the keys go to them") {
        CHECK(board->showsScenes());
        CHECK(board->cardRect(night).isValid());
        CHECK(board->cardRect(night).top() > board->cardRect(storm).bottom());
        press(board, Qt::Key_Down);
        CHECK(board->currentCard() == first);
        press(board, Qt::Key_Down);
        CHECK(board->currentCard() == night);
        CHECK(board->spokenText(night).startsWith(QStringLiteral("Scene 2: Night. Draft.")));
        // A scene moves to another text in the editor
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(panel->message() == toAnotherText());
        CHECK(boardOfBook() == "Chapter 1, The Storm | Part One: The Calm");
        CHECK(scenes.words() == "2 3 1");

        // Scenes off: the scene chosen gives its place to its text
        settings.set<bool>(PlotBoard::SCENES_SETTING, false);
        QCoreApplication::processEvents();
        CHECK_FALSE(board->showsScenes());
        CHECK_FALSE(board->cardRect(night).isValid());
        CHECK(board->currentCard() == storm);
        CHECK(panel->outlineModel()->rowOf(night) < 0);
    }

    SECTION("in a grid the scenes are in a drawer under the row, which Space opens and closes") {
        settings.set<std::string>(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_GRID);
        QCoreApplication::processEvents();
        REQUIRE(board->arrangement() == gui::PlotBoardView::Arrangement::Grid);
        CHECK(board->scenesShown(storm));
        CHECK(board->cardRect(night).isValid());
        CHECK(takesKey(board, Qt::Key_Space));
        REQUIRE(board->setCurrentCard(night));
        press(board, Qt::Key_Space);
        CHECK_FALSE(board->scenesShown(storm));
        CHECK_FALSE(board->cardRect(night).isValid());
        CHECK(board->currentCard() == storm);
        press(board, Qt::Key_Space);
        CHECK(board->scenesShown(storm));
        // Right goes through the scenes of the drawer
        press(board, Qt::Key_Right);
        CHECK(board->currentCard() == first);
        // A text without scenes has no drawer
        CHECK_FALSE(board->toggleScenes(book.calm));
    }

    SECTION("F2 edits a scene, the menu gives it a state, Ctrl+Z undoes it") {
        REQUIRE(board->setCurrentCard(night));
        press(board, Qt::Key_F2);
        const gui::PlotCardEditor* editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        CHECK(editor->titleEdit()->text() == QStringLiteral("Night"));
        editor->descriptionEdit()->setPlainText(QStringLiteral("The storm comes."));
        editor->povEdit()->setText(QStringLiteral("Anna"));
        press(editor->titleEdit(), Qt::Key_Return, Qt::ControlModifier);
        CHECK(plotOf(1).description == QStringLiteral("The storm comes."));
        CHECK(plotOf(1).pov == QStringLiteral("Anna"));
        CHECK(plotOf(1).title == QStringLiteral("Night"));
        CHECK(pm.chapterText(storm).contains(QStringLiteral("pov=\"Anna\"")));
        CHECK(panel->message() == QStringLiteral("The card \"Night\" changed."));
        CHECK(board->currentCard() == night);
        // The text shows the points of view of its scenes
        CHECK(cardOf(storm).scenePovs() == QStringList{QStringLiteral("Anna")});

        // The first scene keeps what it says in the .klh file
        const QMenu* menu = panel->cardMenu(first);
        const auto enabled = [menu](const char* text) {
            const QAction* action = actionOf(*menu, QString::fromLatin1(text));
            return action != nullptr && action->isEnabled();
        };
        CHECK(enabled("&New Scene After It"));
        CHECK(enabled("New Scene &at the End of the Text"));
        CHECK(enabled("Cu&t"));
        CHECK(enabled("&Delete Scene..."));
        CHECK(actionOf(*menu, QStringLiteral("&New Card After It")) == nullptr);
        CHECK(actionOf(*menu, QStringLiteral("&Delete from Book...")) == nullptr);
        const QAction* states = actionOf(*menu, QStringLiteral("&State"));
        REQUIRE(states != nullptr);
        QAction* finished = actionOf(*states->menu(), QStringLiteral("Final"));
        REQUIRE(finished != nullptr);
        finished->trigger();
        CHECK(pm.findElement(storm)->plot.opening.status == QStringLiteral("final"));
        CHECK(panel->message() ==
              QStringLiteral("\"The Storm, Scene 1\" is now in the state: Final."));
        CHECK(core::ProjectManager::statusOf(*pm.findElement(storm)) == QStringLiteral("draft"));

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(pm.findElement(storm)->plot.opening.status.isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(plotOf(1).description.isEmpty());
        CHECK(plotOf(1).pov.isEmpty());
        CHECK(panel->message() == QStringLiteral("Undone: change of the card \"Night\"."));
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(plotOf(1).pov == QStringLiteral("Anna"));
    }

    SECTION("the outline has a row of each scene under its text") {
        CHECK(boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_OUTLINE), true) == panel);
        gui::PlotOutlineModel* model = panel->outlineModel();
        const int row = model->rowOf(storm);
        REQUIRE(row >= 0);
        CHECK(model->rowOf(first) == row + 1);
        CHECK(model->rowOf(night) == row + 2);
        CHECK(model->cardIdAt(row + 3) == last);
        CHECK(model->index(row + 2, gui::PlotOutlineModel::NUMBER_COLUMN).data().toString() ==
              QStringLiteral("2.2"));
        // A scene without a title shows its name
        const QModelIndex unnamed = model->index(row + 1, gui::PlotOutlineModel::TITLE_COLUMN);
        CHECK(unnamed.data().toString() == QStringLiteral("Scene 1"));
        CHECK(unnamed.data(Qt::EditRole).toString().isEmpty());

        const QModelIndex title = model->index(row + 3, gui::PlotOutlineModel::TITLE_COLUMN);
        REQUIRE(model->setData(title, QStringLiteral("Morning"), Qt::EditRole));
        CHECK(plotOf(2).title == QStringLiteral("Morning"));
        // A scene may have no title
        REQUIRE(model->setData(
            model->index(model->rowOf(last), gui::PlotOutlineModel::TITLE_COLUMN), QString(),
            Qt::EditRole));
        CHECK(plotOf(2).title.isEmpty());
        const QModelIndex state =
            model->index(model->rowOf(night), gui::PlotOutlineModel::STATUS_COLUMN);
        REQUIRE(model->setData(state, QStringLiteral("revision"), Qt::EditRole));
        CHECK(plotOf(1).status == QStringLiteral("revision"));
    }

    SECTION("Enter opens the text at the scene, and the board changes the open text") {
        REQUIRE(board->setCurrentCard(last));
        press(board, Qt::Key_Return);
        auto* text = qobject_cast<gui::EditorPanel*>(scenes.window.tabs->currentWidget());
        REQUIRE(text != nullptr);
        CHECK(text->property("elementId").toString() == storm);
        editor::BookEditor* editor = text->getBookEditor();
        REQUIRE(editor != nullptr);
        CHECK(editor->cursorPosition().paragraph == 4);

        // An open text takes the change in its editor, to be saved with it
        REQUIRE(boards.openBoard(QString(), true) == panel);
        const QByteArray saved = book.novel.chapterFile();
        REQUIRE(board->setCurrentCard(night));
        emit panel->colorRequested(night, QStringLiteral("teal"));
        CHECK(editor->sceneAttributes(1).value(QStringLiteral("color")) ==
              QStringLiteral("teal"));
        CHECK(editor->isModified());
        CHECK(book.novel.chapterFile() == saved);
        const auto shownColor = [board, &night]() {
            for (const PlotCard& each : PlotBoard::allCardsOf(board->bands())) {
                if (each.id == night) {
                    return each.plot.color;
                }
            }
            return QString();
        };
        CHECK(shownColor() == QStringLiteral("teal"));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(editor->sceneAttributes(1).value(QStringLiteral("color")).isEmpty());
        CHECK(shownColor().isEmpty());

        // A scene changed in the editor since is not undone on the board
        press(board, Qt::Key_Y, Qt::ControlModifier);
        editor::SceneBreakAttributes changed = editor->sceneAttributes(1);
        changed.insert(QStringLiteral("color"), QStringLiteral("rose"));
        REQUIRE(editor->setSceneAttributes(1, changed));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(editor->sceneAttributes(1).value(QStringLiteral("color")) ==
              QStringLiteral("rose"));
        CHECK(panel->message() ==
              QStringLiteral("The scene changed in the editor: the board cannot undo that."));
        CHECK_FALSE(boards.undoStack()->canUndo());
    }
}

TEST_CASE("Plot board: the scenes of a text moved, added and deleted on the board",
          "[gui][plotboard][scenes]") {
    using gui::dialogs::MessageDialog;
    gui::registerAllIcons();
    StormBoard s;
    auto& pm = s.pm;
    const QString& storm = s.storm;
    const QString& calm = s.book.calm;
    gui::PlotBoardPanel* panel = s.panel;
    gui::PlotBoardView* board = s.board;
    const auto deleting = [board](MessageDialog::Answer answer, QString* asked = nullptr) {
        return answering([board]() { press(board, Qt::Key_Delete); }, answer, asked);
    };

    SECTION("Ctrl+Up and Ctrl+Down move a scene among the scenes of its text, Ctrl+Z undoes it") {
        REQUIRE(board->setCurrentCard(s.night));
        CHECK(takesKey(board, Qt::Key_Up, Qt::ControlModifier));
        press(board, Qt::Key_Up, Qt::ControlModifier);
        CHECK(s.words() == "3 2 1");
        // The first scene keeps what it says in the .klh file
        CHECK(pm.findElement(storm)->plot.opening.title == QStringLiteral("Night"));
        CHECK(s.titleOf(1).isEmpty());
        CHECK(pm.chapterText(storm).startsWith(QStringLiteral("<kml><p>Three four five</p>")));
        CHECK(board->currentCard() == s.first);
        CHECK(panel->message() == QStringLiteral("\"Night\" moved."));
        press(board, Qt::Key_Up, Qt::ControlModifier);
        CHECK(panel->message() == QStringLiteral("\"Night\" is the first scene of its text."));
        CHECK(s.words() == "3 2 1");

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(pm.findElement(storm)->plot.opening.title.isEmpty());
        CHECK(s.titleOf(1) == QStringLiteral("Night"));
        CHECK(panel->message() == QStringLiteral("Undone: moving \"Night\"."));
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(s.words() == "3 2 1");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");

        // The last scene goes no further; dropped after a scene of its text, it is there
        REQUIRE(board->setCurrentCard(s.last));
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(panel->message() ==
              QStringLiteral("\"The Storm, Scene 3\" is the last scene of its text."));
        press(board, Qt::Key_Up, Qt::ControlModifier);
        CHECK(s.words() == "2 1 3");
        emit panel->dropBesideRequested(s.night, s.last, true);
        CHECK(s.words() == "2 3 1");
        CHECK(board->currentCard() == s.last);
        // Beside a scene of another text, in the editor
        emit panel->dropBesideRequested(s.night, calm, false);
        CHECK(panel->message() == toAnotherText());
        CHECK(s.words() == "2 3 1");

        // In the drawer of a grid the scenes move left and right, up and down to other texts
        core::SettingsManager::getInstance().set<std::string>(PlotBoard::LAYOUT_SETTING,
                                                              PlotBoard::LAYOUT_GRID);
        QCoreApplication::processEvents();
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(s.words() == "3 2 1");
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(panel->message() == toAnotherText());
        CHECK(s.words() == "3 2 1");
    }

    SECTION("Insert adds a scene after the scene and edits its card, Alt+Insert one at the end") {
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Insert);
        CHECK(s.words() == "2 3 0 1");
        const QString added = PlotBoard::sceneId(storm, 2);
        CHECK(board->currentCard() == added);
        CHECK(panel->message() == QStringLiteral("Added a scene after \"Night\"."));
        const gui::PlotCardEditor* editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        editor->titleEdit()->setText(QStringLiteral("Dawn"));
        press(editor->titleEdit(), Qt::Key_Return, Qt::ControlModifier);
        CHECK(s.titleOf(2) == QStringLiteral("Dawn"));
        CHECK(s.titleOf(3).isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.titleOf(2).isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(panel->message() == QStringLiteral("Undone: adding a scene to \"The Storm\"."));

        // Alt+Insert on the card of a text without scene breaks: its second scene
        REQUIRE(board->setCurrentCard(calm));
        CHECK(takesKey(board, Qt::Key_Insert, Qt::AltModifier));
        press(board, Qt::Key_Insert, Qt::AltModifier);
        CHECK(pm.scenesOf(calm).size() == 2);
        CHECK(board->currentCard() == PlotBoard::sceneId(calm, 1));
        CHECK(panel->message() == QStringLiteral("Added a scene at the end of \"The Calm\"."));
        REQUIRE(board->cardEditor() != nullptr);
        press(board->cardEditor()->titleEdit(), Qt::Key_Escape);
        CHECK(board->cardEditor() == nullptr);
        // From a scene: at the end of its text
        REQUIRE(board->setCurrentCard(s.first));
        press(board, Qt::Key_Insert, Qt::AltModifier);
        CHECK(s.words() == "2 3 1 0");
        CHECK(board->currentCard() == PlotBoard::sceneId(storm, 3));
    }

    SECTION("Delete asks whether to delete the scene with its text or its break only") {
        REQUIRE(board->setCurrentCard(s.night));
        QString asked;
        CHECK(deleting(MessageDialog::Answer::Cancel, &asked));
        CHECK(asked.contains(QStringLiteral("Delete the scene \"Night\" with its text?")));
        CHECK(s.words() == "2 3 1");

        CHECK(deleting(MessageDialog::Answer::Accept));
        CHECK(s.words() == "2 1");
        // The scene after it takes its place
        CHECK(board->currentCard() == s.night);
        CHECK(panel->message() == QStringLiteral("Deleted \"Night\"."));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(s.titleOf(1) == QStringLiteral("Night"));

        // Keep the Text: its text joins the scene before it
        REQUIRE(board->setCurrentCard(s.night));
        CHECK(deleting(MessageDialog::Answer::Alternative));
        CHECK(s.words() == "5 1");
        CHECK(board->currentCard() == s.first);
        CHECK(panel->message() ==
              QStringLiteral("\"Night\" is joined with the scene beside it."));
        // The last break gone, the text has no scene cards
        REQUIRE(board->setCurrentCard(s.night));
        CHECK(deleting(MessageDialog::Answer::Alternative));
        CHECK(s.words() == "6");
        CHECK(board->currentCard() == storm);
        CHECK(cardOf(storm).scenes.isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");

        // The first scene deleted: the next one is the first and says what it said
        REQUIRE(board->setCurrentCard(s.first));
        CHECK(deleting(MessageDialog::Answer::Accept));
        CHECK(s.words() == "3 1");
        CHECK(pm.findElement(storm)->plot.opening.title == QStringLiteral("Night"));
    }

    SECTION("Ctrl+X and Ctrl+V move a scene after another scene of its text") {
        REQUIRE(board->setCurrentCard(s.first));
        press(board, Qt::Key_X, Qt::ControlModifier);
        CHECK(board->markedCard() == s.first);
        REQUIRE(board->setCurrentCard(s.last));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(s.words() == "3 1 2");
        CHECK(pm.findElement(storm)->plot.opening.title == QStringLiteral("Night"));
        CHECK(board->markedCard().isEmpty());
        CHECK(board->currentCard() == s.last);
        // After the card of its text: its first scene
        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(storm));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(board->currentCard() == s.first);
        // After another text: in the editor
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(calm));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(panel->message() == toAnotherText());
        CHECK(s.words() == "2 3 1");
    }

    SECTION("a scene of an open text moves in its editor, to be saved with the text") {
        press(board, Qt::Key_Return);
        auto* text = qobject_cast<gui::EditorPanel*>(s.window.tabs->currentWidget());
        REQUIRE(text != nullptr);
        editor::BookEditor* editor = text->getBookEditor();
        REQUIRE(editor != nullptr);
        REQUIRE(s.boards.openBoard(QString(), true) == panel);
        const QByteArray saved = s.book.novel.chapterFile();
        const auto edited = [editor]() {
            return wordsOf(core::ChapterDocument::kmlScenes(editor->toKml()));
        };

        REQUIRE(board->setCurrentCard(s.last));
        press(board, Qt::Key_Up, Qt::ControlModifier);
        CHECK(edited() == "2 1 3");
        CHECK(shownWords(*board, storm) == "2 1 3");
        CHECK(editor->isModified());
        CHECK(s.book.novel.chapterFile() == saved);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(edited() == "2 3 1");
        CHECK(shownWords(*board, storm) == "2 3 1");

        // Deleted on the board, Ctrl+Z of the editor brings it back
        REQUIRE(board->setCurrentCard(s.night));
        CHECK(deleting(MessageDialog::Answer::Accept));
        CHECK(edited() == "2 1");
        editor->undo();
        CHECK(edited() == "2 3 1");
        // The board cannot undo it then
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(edited() == "2 3 1");
        CHECK(s.book.novel.chapterFile() == saved);
    }

    SECTION("the outline adds and deletes the scenes with the keys of the board") {
        REQUIRE(s.boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_OUTLINE), true) == panel);
        QTableView* outline = panel->outline();
        const gui::PlotOutlineModel* model = panel->outlineModel();
        outline->setCurrentIndex(
            model->index(model->rowOf(s.night), gui::PlotOutlineModel::TITLE_COLUMN));
        CHECK(takesKey(outline, Qt::Key_Insert, Qt::AltModifier));
        press(outline, Qt::Key_Insert, Qt::AltModifier);
        CHECK(s.words() == "2 3 1 0");
        CHECK(model->rowOf(PlotBoard::sceneId(storm, 3)) >= 0);
        QCoreApplication::processEvents();
        outline->setCurrentIndex(
            model->index(model->rowOf(s.night), gui::PlotOutlineModel::TITLE_COLUMN));
        CHECK(answering([outline]() { press(outline, Qt::Key_Delete); },
                        MessageDialog::Answer::Accept));
        CHECK(s.words() == "2 1 0");
    }
}
