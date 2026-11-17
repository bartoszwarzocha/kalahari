// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_plot_board.cpp
/// @brief The plot board and the outline of the book: the cards of the texts, the places they
/// move to, the cells of the outline, the keys, and what Ctrl+Z undoes

#include "context_menu_reader.h"
#include "document_window.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/panels/plot_fields_box.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_card_editor.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFont>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUndoStack>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

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
    KeptSetting variant{PlotBoard::SCENES_VARIANT_SETTING};
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

    /// @param scenesVariant Which scenes there are (PlotBoard::SCENES_TEXT...)
    explicit StormBoard(const char* scenesVariant = PlotBoard::SCENES_TEXT) {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<std::string>(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_COLUMNS);
        settings.set<std::string>(PlotBoard::SCENES_VARIANT_SETTING, scenesVariant);
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

/// What is said under the board after "Night" of "The Storm" moved to "The Calm"
const QString& nightToCalm() {
    static const QString TEXT = QStringLiteral("\"Night\" moved to \"The Calm\".");
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
        // Ctrl with the arrows across the texts moves a scene to the text beside
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(panel->message() == nightToCalm());
        CHECK(boardOfBook() == "Chapter 1, The Storm | Part One: The Calm");
        CHECK(scenes.words() == "2 1");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(scenes.words() == "2 3 1");
        REQUIRE(board->setCurrentCard(night));

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
        // On the card of another text: to that text
        emit panel->dropBesideRequested(s.night, calm, true);
        CHECK(panel->message() == nightToCalm());
        CHECK(s.words() == "2 1");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");

        // In the drawer of a grid the scenes move left and right, up and down to other texts
        core::SettingsManager::getInstance().set<std::string>(PlotBoard::LAYOUT_SETTING,
                                                              PlotBoard::LAYOUT_GRID);
        QCoreApplication::processEvents();
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(s.words() == "3 2 1");
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(panel->message() == nightToCalm());
        CHECK(s.words() == "2 1");
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
        // After the card of another text: to that text
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(calm));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(panel->message() == nightToCalm());
        CHECK(s.words() == "2 1");
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

namespace {

/// The places in the text of the scenes in plan @p plan: "0 1 1 3"
std::string aftersOf(const QList<core::PlannedScene>& plan) {
    QStringList shown;
    for (const core::PlannedScene& scene : plan) {
        shown << QString::number(scene.after);
    }
    return shown.join(QLatin1Char(' ')).toStdString();
}

/// A plan of scenes after @p afters scenes of their text, titled "P0", "P1"...
QList<core::PlannedScene> planOf(std::initializer_list<int> afters) {
    QList<core::PlannedScene> plan;
    for (const int after : afters) {
        core::PlannedScene scene;
        scene.after = after;
        scene.plot.title = QStringLiteral("P%1").arg(plan.size());
        plan << scene;
    }
    return plan;
}

/// The kinds of the scene cards of text @p textId on @p board ("Scene 1, In the Plan")
std::string shownKinds(const gui::PlotBoardView& board, const QString& textId) {
    QStringList shown;
    for (const PlotCard& card : PlotBoard::cardsOf(board.bands())) {
        for (const PlotCard& scene : card.id == textId ? card.scenes : QList<PlotCard>()) {
            shown << (scene.title.isEmpty() ? scene.kindName
                                            : QStringLiteral("%1 %2").arg(scene.kindName,
                                                                          scene.title));
        }
    }
    return shown.join(QStringLiteral(", ")).toStdString();
}

/// Give text @p textId of @p pm plan @p plan, as the .klh file has it
void setPlan(core::ProjectManager& pm, const QString& textId,
             const QList<core::PlannedScene>& plan) {
    const core::ProjectElement* element = pm.findElement(textId);
    REQUIRE(element != nullptr);
    core::ElementPlot plot = element->plot;
    plot.planned = plan;
    REQUIRE(pm.setPlot(textId, element->description, plot));
    // The board fills itself again a moment later
    QCoreApplication::processEvents();
}

}  // namespace

TEST_CASE("Plot board: the order of the scenes of a text and of those in its plan",
          "[gui][plotboard][plan]") {
    using Slot = PlotBoard::SceneSlot;
    // One at the start of the text, two after its first scene, one beyond its end
    const QList<core::PlannedScene> plan = planOf({1, 0, 1, 9});
    QList<Slot> order = PlotBoard::sceneOrder(3, plan);
    CHECK(order == QList<Slot>{{.planned = true, .index = 1}, {.planned = false, .index = 0},
                               {.planned = true, .index = 0}, {.planned = true, .index = 2},
                               {.planned = false, .index = 1}, {.planned = false, .index = 2},
                               {.planned = true, .index = 3}});
    // The plan in the order of the board: each scene after the scenes of the text before it
    CHECK(aftersOf(PlotBoard::planInOrder(order, plan)) == "0 1 1 3");
    order.move(6, 0);
    const QList<core::PlannedScene> moved = PlotBoard::planInOrder(order, plan);
    CHECK(aftersOf(moved) == "0 0 1 1");
    CHECK(moved.at(0).plot.title == QStringLiteral("P3"));

    // Without the scenes of the text, the places of the plan in their new order
    QList<Slot> planOnly = PlotBoard::sceneOrder(0, plan);
    CHECK(planOnly == QList<Slot>{{.planned = true, .index = 0}, {.planned = true, .index = 1},
                                  {.planned = true, .index = 2}, {.planned = true, .index = 3}});
    planOnly.move(3, 0);
    const QList<core::PlannedScene> reordered = PlotBoard::planInOrder(planOnly, plan);
    CHECK(aftersOf(reordered) == "0 1 1 9");
    CHECK(reordered.at(0).plot.title == QStringLiteral("P3"));

    // The ids of the cards
    const QString planned = PlotBoard::plannedId(QStringLiteral("t1"), 2);
    CHECK(PlotBoard::plannedOf(planned) == std::make_pair(QStringLiteral("t1"), 2));
    CHECK_FALSE(PlotBoard::sceneOf(planned).has_value());
    CHECK_FALSE(PlotBoard::plannedOf(PlotBoard::sceneId(QStringLiteral("t1"), 2)).has_value());
    CHECK(PlotBoard::textOf(planned) == QStringLiteral("t1"));
    CHECK(PlotBoard::textOf(QStringLiteral("t1")) == QStringLiteral("t1"));
    CHECK(PlotBoard::slotOf(planned) == Slot{.planned = true, .index = 2});
    CHECK(PlotBoard::slotOf(QStringLiteral("t1")).index == -1);
    CHECK(PlotBoard::slotId(QStringLiteral("t1"), Slot{.planned = true, .index = 2}) == planned);
}

TEST_CASE("Plot board: the scenes in the plans of the texts on the board",
          "[gui][plotboard][scenes][plan]") {
    gui::registerAllIcons();
    StormBoard s(PlotBoard::SCENES_BOTH);
    auto& pm = s.pm;
    auto& settings = core::SettingsManager::getInstance();
    const QString& storm = s.storm;
    const QString& calm = s.book.calm;
    gui::PlotBoardPanel* panel = s.panel;
    gui::PlotBoardView* board = s.board;
    const auto plan = [&pm, &storm]() { return pm.findElement(storm)->plot.planned; };
    const auto setVariant = [&settings](const char* variant) {
        settings.set<std::string>(PlotBoard::SCENES_VARIANT_SETTING, variant);
        // The board follows the setting a moment later
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    };

    SECTION("the variants show the scenes of the text, those in its plan, or both") {
        setPlan(pm, storm, planOf({1, 9}));
        CHECK(shownKinds(*board, storm) ==
              "Scene 1, In the Plan P0, Scene 2 Night, Scene 3, In the Plan P1");
        const PlotCard card = cardOf(storm);
        REQUIRE(card.scenes.size() == 5);
        CHECK(card.scenes.at(1).planned);
        CHECK(card.scenes.at(1).id == PlotBoard::plannedId(storm, 0));
        CHECK(card.scenes.at(1).words == 0);
        // A text of one scene shows it beside its plan
        setPlan(pm, calm, planOf({0}));
        CHECK(shownKinds(*board, calm) == "In the Plan P0, Scene 1");

        setVariant(PlotBoard::SCENES_TEXT);
        CHECK(shownKinds(*board, storm) == "Scene 1, Scene 2 Night, Scene 3");
        CHECK(shownKinds(*board, calm).empty());
        setVariant(PlotBoard::SCENES_PLAN);
        CHECK(shownKinds(*board, storm) == "Scene 1 P0, Scene 2 P1");
        CHECK(shownKinds(*board, calm) == "Scene 1 P0");
        setVariant(PlotBoard::SCENES_BOTH);
        CHECK(shownKinds(*board, storm) ==
              "Scene 1, In the Plan P0, Scene 2 Night, Scene 3, In the Plan P1");
    }

    SECTION("Insert adds a scene in the plan and edits its card, Ctrl+Z undoes it") {
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Insert);
        REQUIRE(plan().size() == 1);
        CHECK(plan().at(0).after == 2);
        const QString added = PlotBoard::plannedId(storm, 0);
        CHECK(board->currentCard() == added);
        CHECK(panel->message() == QStringLiteral("Added a scene in the plan after \"Night\"."));
        // The text stays as it is
        CHECK(s.words() == "2 3 1");
        const gui::PlotCardEditor* editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        editor->titleEdit()->setText(QStringLiteral("Dawn"));
        press(editor->titleEdit(), Qt::Key_Return, Qt::ControlModifier);
        CHECK(plan().at(0).plot.title == QStringLiteral("Dawn"));
        CHECK(shownKinds(*board, storm) == "Scene 1, Scene 2 Night, In the Plan Dawn, Scene 3");

        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(plan().at(0).plot.title.isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(plan().isEmpty());
        CHECK(panel->message() ==
              QStringLiteral("Undone: adding a scene to the plan of \"The Storm\"."));
        press(board, Qt::Key_Y, Qt::ControlModifier);
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(plan().size() == 1);
        CHECK(plan().at(0).plot.title == QStringLiteral("Dawn"));

        // Alt+Insert: at the end of the text; from the card of a text too
        REQUIRE(board->setCurrentCard(calm));
        press(board, Qt::Key_Insert, Qt::AltModifier);
        REQUIRE(pm.findElement(calm)->plot.planned.size() == 1);
        CHECK(pm.findElement(calm)->plot.planned.at(0).after == 1);
        CHECK(board->currentCard() == PlotBoard::plannedId(calm, 0));
        CHECK(panel->message() ==
              QStringLiteral("Added a scene in the plan at the end of \"The Calm\"."));
        REQUIRE(board->cardEditor() != nullptr);
        press(board->cardEditor()->titleEdit(), Qt::Key_Escape);
        CHECK(pm.scenesOf(calm).size() == 1);
    }

    SECTION("a scene in the plan moves among the scenes, to another text, and is deleted") {
        setPlan(pm, storm, planOf({1}));
        const QString p0 = PlotBoard::plannedId(storm, 0);
        REQUIRE(board->setCurrentCard(p0));
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "2");
        CHECK(s.words() == "2 3 1");
        CHECK(board->currentCard() == p0);
        CHECK(panel->message() == QStringLiteral("\"P0\" moved."));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "1");
        // A scene of the text moves past it: the text does not change
        REQUIRE(board->setCurrentCard(s.first));
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "0");
        CHECK(s.words() == "2 3 1");
        CHECK(shownKinds(*board, storm) == "In the Plan P0, Scene 1, Scene 2 Night, Scene 3");
        // and then past a scene of the text: the text changes, the plan keeps its place
        press(board, Qt::Key_Down, Qt::ControlModifier);
        CHECK(s.words() == "3 2 1");
        CHECK(aftersOf(plan()) == "0");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "1");

        // A drop and Ctrl+X with Ctrl+V among the scenes of its text
        emit panel->dropBesideRequested(p0, s.last, true);
        CHECK(aftersOf(plan()) == "3");
        REQUIRE(board->setCurrentCard(p0));
        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(storm));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "0");

        // Ctrl+Right in columns: to the start of the text after it
        REQUIRE(board->setCurrentCard(p0));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(plan().isEmpty());
        REQUIRE(pm.findElement(calm)->plot.planned.size() == 1);
        CHECK(pm.findElement(calm)->plot.planned.at(0).plot.title == QStringLiteral("P0"));
        CHECK(board->currentCard() == PlotBoard::plannedId(calm, 0));
        CHECK(panel->message() == QStringLiteral("\"P0\" moved to \"The Calm\"."));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(panel->message() == QStringLiteral("\"P0\" is in the last text of the board."));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(aftersOf(plan()) == "0");
        CHECK(pm.findElement(calm)->plot.planned.isEmpty());

        // Delete: at once, Ctrl+Z brings it back
        REQUIRE(board->setCurrentCard(p0));
        press(board, Qt::Key_Delete);
        CHECK(plan().isEmpty());
        CHECK(panel->message() == QStringLiteral("Deleted \"P0\" from the plan."));
        CHECK(board->currentCard() == s.first);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(plan().size() == 1);
    }

    SECTION("Enter writes a scene in the plan in its text and opens the text there") {
        setPlan(pm, storm, planOf({2}));
        const QString p0 = PlotBoard::plannedId(storm, 0);
        REQUIRE(board->setCurrentCard(p0));
        // The menu says so, and a scene in the plan has no state
        const QMenu* menu = panel->cardMenu(p0);
        REQUIRE(actionOf(*menu, QStringLiteral("&Write the Scene")) != nullptr);
        REQUIRE(actionOf(*menu, QStringLiteral("&Delete from the Plan")) != nullptr);
        CHECK_FALSE(actionOf(*menu, QStringLiteral("&State"))->isEnabled());
        emit panel->statusRequested(p0, QStringLiteral("final"));
        CHECK(panel->message() ==
              QStringLiteral("\"P0\" is in the plan: it gets a state once it is written."));

        press(board, Qt::Key_Return);
        CHECK(s.words() == "2 3 0 1");
        CHECK(s.titleOf(2) == QStringLiteral("P0"));
        CHECK(pm.scenesOf(storm).at(2).plot.status == QStringLiteral("draft"));
        CHECK(plan().isEmpty());
        auto* text = qobject_cast<gui::EditorPanel*>(s.window.tabs->currentWidget());
        REQUIRE(text != nullptr);
        CHECK(text->property("elementId").toString() == storm);

        // Ctrl+Z on the board takes the scene out of the text and back to the plan
        REQUIRE(s.boards.openBoard(QString(), true) == panel);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(wordsOf(core::ChapterDocument::kmlScenes(text->getBookEditor()->toKml())) ==
              "2 3 1");
        CHECK(aftersOf(plan()) == "2");
    }

    SECTION("the first scene in the plan of a text not written yet is the text") {
        setPlan(pm, calm, planOf({1, 1}));
        REQUIRE(board->setCurrentCard(PlotBoard::plannedId(calm, 0)));
        press(board, Qt::Key_Return);
        REQUIRE(pm.scenesOf(calm).size() == 1);
        CHECK(pm.findElement(calm)->plot.opening.title == QStringLiteral("P0"));
        REQUIRE(pm.findElement(calm)->plot.planned.size() == 1);
        CHECK(pm.findElement(calm)->plot.planned.at(0).plot.title == QStringLiteral("P1"));
        CHECK(pm.findElement(calm)->plot.planned.at(0).after == 1);
    }

    SECTION("in the plan only, Enter opens the text and the scenes move in the plan") {
        setPlan(pm, storm, planOf({1, 2}));
        setVariant(PlotBoard::SCENES_PLAN);
        const QString p1 = PlotBoard::plannedId(storm, 1);
        REQUIRE(board->setCurrentCard(p1));
        CHECK(actionOf(*panel->cardMenu(p1), QStringLiteral("&Open")) != nullptr);
        press(board, Qt::Key_Up, Qt::ControlModifier);
        CHECK(plan().at(0).plot.title == QStringLiteral("P1"));
        CHECK(aftersOf(plan()) == "1 2");
        CHECK(board->currentCard() == PlotBoard::plannedId(storm, 0));
        CHECK(s.words() == "2 3 1");
        press(board, Qt::Key_Return);
        CHECK(s.words() == "2 3 1");
        CHECK(qobject_cast<gui::EditorPanel*>(s.window.tabs->currentWidget()) != nullptr);
    }

    SECTION("the outline has a row of each scene in the plan, edited in its cells") {
        setPlan(pm, storm, planOf({1}));
        REQUIRE(s.boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_OUTLINE), true) == panel);
        gui::PlotOutlineModel* model = panel->outlineModel();
        const int row = model->rowOf(PlotBoard::plannedId(storm, 0));
        REQUIRE(row >= 0);
        using Model = gui::PlotOutlineModel;
        CHECK(model->index(row, Model::NUMBER_COLUMN).data().toString() == QStringLiteral("–"));
        CHECK(model->index(row, Model::WORDS_COLUMN).data().toString() == QStringLiteral("–"));
        CHECK(model->index(row, Model::STATUS_COLUMN).data().toString() ==
              QStringLiteral("In the Plan"));
        CHECK_FALSE(model->flags(model->index(row, Model::STATUS_COLUMN)) & Qt::ItemIsEditable);
        CHECK(model->index(row, Model::TITLE_COLUMN).data(Qt::FontRole).value<QFont>().italic());
        CHECK(model->setData(model->index(row, Model::PLACE_COLUMN),
                             QVariant(QStringLiteral("The well")), Qt::EditRole));
        CHECK(plan().at(0).plot.place == QStringLiteral("The well"));
    }
}

namespace {

/// "The Storm" with a note on "Night", its second scene
constexpr const char* ANNOTATED_STORM =
    "<kml><annotations><annotation id=\"n1\" kind=\"note\">Rain</annotation></annotations>"
    "<p>One two</p><scene-break title=\"Night\"/><p><anchor ref=\"n1\">Three</anchor> four "
    "five</p><scene-break/><p>Six</p></kml>";

/// "The Calm" with two scenes, the second "Dawn"
constexpr const char* CALM_SCENES =
    "<kml><p>Calm</p><scene-break title=\"Dawn\"/><p>Seven eight</p></kml>";

/// Whether text @p kml has the note on "Night"
bool hasNote(const QString& kml) {
    return kml.contains(QStringLiteral("<annotation id=\"n1\""));
}

}  // namespace

TEST_CASE("Plot board: a scene of a text moved to another text with its annotations",
          "[gui][plotboard][scenes]") {
    gui::registerAllIcons();
    StormBoard s;
    auto& pm = s.pm;
    auto& settings = core::SettingsManager::getInstance();
    const QString& storm = s.storm;
    const QString& calm = s.book.calm;
    gui::PlotBoardPanel* panel = s.panel;
    gui::PlotBoardView* board = s.board;
    pm.setChapterContent(storm, QString::fromLatin1(ANNOTATED_STORM));
    REQUIRE(pm.saveChapterContent(storm));
    pm.setChapterContent(calm, QString::fromLatin1(CALM_SCENES));
    REQUIRE(pm.saveChapterContent(calm));
    // The board fills itself again a moment later
    s.boards.refresh();
    QCoreApplication::processEvents();
    const auto calmWords = [&pm, &calm]() { return wordsOf(pm.scenesOf(calm)); };
    const auto planOfText = [&pm](const QString& textId) {
        return aftersOf(pm.findElement(textId)->plot.planned);
    };
    REQUIRE(s.words() == "2 3 1");
    REQUIRE(calmWords() == "1 2");

    SECTION("Ctrl with the arrows across the texts moves it with its note, Ctrl+Z both back") {
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(panel->message() == nightToCalm());
        CHECK(s.words() == "2 1");
        CHECK(calmWords() == "3 1 2");
        CHECK(pm.findElement(calm)->plot.opening.title == QStringLiteral("Night"));
        CHECK(pm.scenesOf(calm).at(1).plot.title.isEmpty());
        CHECK(hasNote(pm.chapterText(calm)));
        CHECK_FALSE(hasNote(pm.chapterText(storm)));
        CHECK(board->currentCard() == PlotBoard::sceneId(calm, 0));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(panel->message() == QStringLiteral("Undone: moving \"Night\"."));
        CHECK(s.words() == "2 3 1");
        CHECK(calmWords() == "1 2");
        CHECK(hasNote(pm.chapterText(storm)));
        CHECK_FALSE(hasNote(pm.chapterText(calm)));
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(calmWords() == "3 1 2");

        // And back, to the end of the text before
        REQUIRE(board->setCurrentCard(PlotBoard::sceneId(calm, 0)));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(s.words() == "2 1 3");
        CHECK(calmWords() == "1 2");
        CHECK(pm.findElement(calm)->plot.opening.title.isEmpty());
        CHECK(pm.scenesOf(storm).at(2).plot.title == QStringLiteral("Night"));
        CHECK(hasNote(pm.chapterText(storm)));
        CHECK(board->currentCard() == PlotBoard::sceneId(storm, 2));
    }

    SECTION("dropped beside a scene or on the card of another text, or pasted after it") {
        // Before "Dawn"
        emit panel->dropBesideRequested(s.night, PlotBoard::sceneId(calm, 1), false);
        CHECK(s.words() == "2 1");
        CHECK(calmWords() == "1 3 2");
        CHECK(pm.scenesOf(calm).at(1).plot.title == QStringLiteral("Night"));
        CHECK(board->currentCard() == PlotBoard::sceneId(calm, 1));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(calmWords() == "1 2");

        // Ctrl+X, and Ctrl+V after "Dawn": at the end
        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_X, Qt::ControlModifier);
        REQUIRE(board->setCurrentCard(PlotBoard::sceneId(calm, 1)));
        press(board, Qt::Key_V, Qt::ControlModifier);
        CHECK(calmWords() == "1 2 3");
        CHECK(board->markedCard().isEmpty());
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(calmWords() == "1 2");

        // A scene goes beside a scene or on a text, not in a part
        emit panel->dropInPartRequested(s.night, s.book.part);
        CHECK(panel->message() == QStringLiteral("\"Night\" cannot be there."));
        CHECK(s.words() == "2 3 1");
    }

    SECTION("to an empty text it takes the place of its empty scene; the only scene stays") {
        // "Chapter 1" before "The Storm" has no words
        REQUIRE(board->setCurrentCard(s.first));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(s.words() == "3 1");
        CHECK(wordsOf(pm.scenesOf(s.book.first)) == "2");
        CHECK(pm.findElement(storm)->plot.opening.title == QStringLiteral("Night"));
        CHECK(board->currentCard() == s.book.first);
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(s.words() == "2 3 1");
        CHECK(wordsOf(pm.scenesOf(s.book.first)) == "0");

        // The only scene of a text, shown beside its plan
        settings.set<std::string>(PlotBoard::SCENES_VARIANT_SETTING, PlotBoard::SCENES_BOTH);
        pm.setChapterContent(calm, QStringLiteral("<kml><p>Calm</p></kml>"));
        REQUIRE(pm.saveChapterContent(calm));
        setPlan(pm, calm, planOf({1}));
        s.boards.refresh();
        QCoreApplication::processEvents();
        REQUIRE(board->setCurrentCard(PlotBoard::sceneId(calm, 0)));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(panel->message() ==
              QStringLiteral("\"The Calm, Scene 1\" is the only scene of its text."));
        CHECK(s.words() == "2 3 1");
    }

    SECTION("the scenes in the plans keep their places among the scenes of the texts") {
        setPlan(pm, storm, planOf({2}));
        setPlan(pm, calm, planOf({1}));
        // The plans not shown: the scenes after the place move with it
        press(board, Qt::Key_Down);
        press(board, Qt::Key_Down);
        REQUIRE(board->currentCard() == s.night);
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(planOfText(storm) == "1");
        CHECK(planOfText(calm) == "2");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(planOfText(storm) == "2");
        CHECK(planOfText(calm) == "1");

        // Shown: before or after a scene in the plan
        settings.set<std::string>(PlotBoard::SCENES_VARIANT_SETTING, PlotBoard::SCENES_BOTH);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        emit panel->dropBesideRequested(s.night, PlotBoard::plannedId(calm, 0), false);
        CHECK(shownKinds(*board, calm) ==
              "Scene 1, Scene 2 Night, In the Plan P0, Scene 3 Dawn");
        CHECK(planOfText(calm) == "2");
        CHECK(planOfText(storm) == "1");
        press(board, Qt::Key_Z, Qt::ControlModifier);
        emit panel->dropBesideRequested(s.night, PlotBoard::plannedId(calm, 0), true);
        CHECK(shownKinds(*board, calm) ==
              "Scene 1, In the Plan P0, Scene 2 Night, Scene 3 Dawn");
        CHECK(planOfText(calm) == "1");
    }

    SECTION("an open text changes in its editor, the other in its file, one Ctrl+Z undoes both") {
        press(board, Qt::Key_Return);
        auto* text = qobject_cast<gui::EditorPanel*>(s.window.tabs->currentWidget());
        REQUIRE(text != nullptr);
        const editor::BookEditor* editor = text->getBookEditor();
        REQUIRE(editor != nullptr);
        REQUIRE(s.boards.openBoard(QString(), true) == panel);
        const QByteArray saved = s.book.novel.chapterFile();
        const auto edited = [editor]() {
            return wordsOf(core::ChapterDocument::kmlScenes(editor->toKml()));
        };

        REQUIRE(board->setCurrentCard(s.night));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(edited() == "2 1");
        CHECK(editor->isModified());
        CHECK_FALSE(hasNote(editor->toKml()));
        CHECK(s.book.novel.chapterFile() == saved);
        CHECK(calmWords() == "3 1 2");
        CHECK(hasNote(pm.chapterText(calm)));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(edited() == "2 3 1");
        CHECK(hasNote(editor->toKml()));
        CHECK(calmWords() == "1 2");
    }
}

namespace {

/// The item of element @p elementId (empty: of the type @p type) in the tree of @p navigator
QTreeWidgetItem* navigatorItem(const gui::NavigatorPanel& navigator, const QString& elementId,
                               const QString& type = QString()) {
    auto* tree = navigator.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->expandAll();
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        const bool found = elementId.isEmpty()
                               ? (*it)->data(0, Qt::UserRole + 1).toString() == type
                               : (*it)->data(0, Qt::UserRole).toString() == elementId;
        if (found) {
            return *it;
        }
    }
    return nullptr;
}

/// The texts of the menu of @p item of @p navigator; with @p choose, the item beginning with
/// it is chosen
QStringList navigatorMenu(gui::NavigatorPanel& navigator, QTreeWidgetItem* item,
                          const QString& choose = QString()) {
    REQUIRE(item != nullptr);
    auto* tree = navigator.findChild<QTreeWidget*>();
    tree->scrollToItem(item);
    test::ContextMenuReader reader(&navigator);
    if (!choose.isEmpty()) {
        reader.chooseItem(choose);
    }
    emit tree->customContextMenuRequested(tree->visualItemRect(item).center());
    REQUIRE(reader.menusRead() == 1);
    QStringList texts;
    for (const test::MenuItem& entry : reader.items()) {
        texts << entry.text;
    }
    return texts;
}

/// The text of Show on the Plot Board in the Navigator
const QString& showOnBoardText() {
    static const QString TEXT = QStringLiteral("Show on the Plot &Board");
    return TEXT;
}

}  // namespace

TEST_CASE("Plot board: the Edit menu works on the board and in the outline",
          "[gui][plotboard][edit]") {
    using Edit = gui::EditCoordinator::Edit;
    gui::registerAllIcons();
    StormBoard s;
    gui::PlotBoardView* board = s.board;
    gui::PlotBoardPanel* panel = s.panel;
    gui::EditCoordinator edits(test::commands(), []() -> editor::BookEditor* { return nullptr; });
    const auto shown = [board]() { return boardOf(board->bands()); };
    REQUIRE(shown() == "Chapter 1, The Storm | Part One: The Calm");

    SECTION("Undo and Redo undo and redo a change of the board, as Ctrl+Z and Ctrl+Y") {
        CHECK_FALSE(edits.canDo(Edit::Undo, board));
        CHECK_FALSE(edits.canDo(Edit::Redo, board));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        CHECK(edits.canDo(Edit::Undo, board));
        CHECK(edits.execute(Edit::Undo, board));
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
        CHECK(panel->message() == QStringLiteral("Undone: moving \"The Storm\"."));
        CHECK_FALSE(edits.canDo(Edit::Undo, board));
        CHECK(edits.canDo(Edit::Redo, board));
        CHECK(edits.execute(Edit::Redo, board));
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        // Also from the switches over the board
        auto* switchButton = panel->findChild<QToolButton*>();
        REQUIRE(switchButton != nullptr);
        CHECK(edits.execute(Edit::Undo, switchButton));
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
    }

    SECTION("Cut marks the card, Paste moves it after the card chosen, Delete deletes it") {
        CHECK_FALSE(edits.canDo(Edit::Paste, board));
        CHECK(edits.canDo(Edit::Cut, board));
        CHECK(edits.execute(Edit::Cut, board));
        CHECK(board->markedCard() == s.storm);
        // Not after itself
        CHECK_FALSE(edits.canDo(Edit::Paste, board));
        REQUIRE(board->setCurrentCard(s.book.calm));
        CHECK(edits.canDo(Edit::Paste, board));
        CHECK(edits.execute(Edit::Paste, board));
        CHECK(shown() == "Chapter 1 | Part One: The Calm, The Storm");
        CHECK(board->markedCard().isEmpty());

        // A scene asks first
        using gui::dialogs::MessageDialog;
        REQUIRE(board->setCurrentCard(s.night));
        QString asked;
        CHECK(answering([&edits, board]() { edits.execute(Edit::Delete, board); },
                        MessageDialog::Answer::Accept, &asked));
        CHECK(asked.contains(QStringLiteral("Delete the scene \"Night\" with its text?")));
        CHECK(s.words() == "2 1");
    }

    SECTION("a book open read-only: Undo and Redo only") {
        panel->setReadOnly(true);
        CHECK_FALSE(edits.canDo(Edit::Cut, board));
        CHECK_FALSE(edits.canDo(Edit::Delete, board));
        board->setMarkedCard(s.book.calm);
        CHECK_FALSE(edits.canDo(Edit::Paste, board));
        panel->setReadOnly(false);
        CHECK(edits.canDo(Edit::Paste, board));
    }

    SECTION("Copy and Select All are not the board's; a field of the card edited has its own") {
        CHECK_FALSE(edits.canDo(Edit::Copy, board));
        CHECK_FALSE(edits.canDo(Edit::SelectAll, board));
        press(board, Qt::Key_Right, Qt::ControlModifier);
        press(board, Qt::Key_F2);
        const gui::PlotCardEditor* editor = board->cardEditor();
        REQUIRE(editor != nullptr);
        QLineEdit* title = editor->titleEdit();
        CHECK_FALSE(edits.canDo(Edit::Undo, title));
        title->insert(QStringLiteral("!"));
        CHECK(edits.execute(Edit::Undo, title));
        CHECK(title->text() == QStringLiteral("The Storm"));
        // The board's change stays
        CHECK(shown() == "Chapter 1 | Part One: The Storm, The Calm");
        CHECK(edits.canDo(Edit::SelectAll, title));
    }

    SECTION("in the outline: Undo, Cut and Paste as on the board, Copy copies the cell") {
        REQUIRE(s.boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_OUTLINE), true) == panel);
        QTableView* outline = panel->outline();
        press(board, Qt::Key_Right, Qt::ControlModifier);
        REQUIRE(panel->setCurrentCard(s.storm));
        CHECK(edits.canDo(Edit::Undo, outline));
        CHECK(edits.canDo(Edit::Copy, outline));
        CHECK(edits.execute(Edit::Undo, outline));
        CHECK(shown() == "Chapter 1, The Storm | Part One: The Calm");
        CHECK(edits.execute(Edit::Cut, outline));
        CHECK(board->markedCard() == s.storm);
        REQUIRE(panel->setCurrentCard(s.book.calm));
        CHECK(edits.execute(Edit::Paste, outline));
        CHECK(shown() == "Chapter 1 | Part One: The Calm, The Storm");
    }
}

TEST_CASE("Plot board: Show on the Plot Board in the Navigator", "[gui][plotboard]") {
    gui::registerAllIcons();
    StormBoard s;
    auto& pm = s.pm;
    const QList<core::KindRef> backKinds = pm.textKindsFor(core::BookPlace::Back);
    REQUIRE_FALSE(backKinds.isEmpty());
    const QString afterword =
        pm.addElement(backKinds.first(), QStringLiteral("Afterword"), core::BookPlace::Back);
    REQUIRE_FALSE(afterword.isEmpty());
    s.window.navigator.refreshNavigator();
    gui::NavigatorPanel& navigator = *s.window.navigatorPanel;
    QObject::connect(&navigator, &gui::NavigatorPanel::requestShowOnPlotBoard, &s.boards,
                     &gui::PlotBoardCoordinator::showOnBoard);
    s.window.window.setAttribute(Qt::WA_DontShowOnScreen);
    s.window.window.resize(600, 900);
    s.window.window.show();
    REQUIRE(s.board->setCurrentCard(s.book.first));

    SECTION("a text: its card") {
        const QStringList texts =
            navigatorMenu(navigator, navigatorItem(navigator, s.storm), QStringLiteral("Show on"));
        CHECK(texts.contains(showOnBoardText()));
        CHECK(s.board->currentCard() == s.storm);
        CHECK(s.window.tabs->currentWidget() == s.panel);
    }

    SECTION("a part: the card of its first text") {
        CHECK(navigatorMenu(navigator, navigatorItem(navigator, s.book.part),
                            QStringLiteral("Show on"))
                  .contains(showOnBoardText()));
        CHECK(s.board->currentCard() == s.book.calm);
    }

    SECTION("the book: the board as it was") {
        CHECK(navigatorMenu(navigator, navigatorItem(navigator, QString(), QStringLiteral("book")),
                            QStringLiteral("Show on"))
                  .contains(showOnBoardText()));
        CHECK(s.board->currentCard() == s.book.first);
        CHECK(s.window.tabs->currentWidget() == s.panel);
    }

    SECTION("a text outside the main section has no card") {
        const QStringList texts = navigatorMenu(navigator, navigatorItem(navigator, afterword));
        CHECK_FALSE(texts.contains(showOnBoardText()));
    }
}

TEST_CASE("Plot board: the plot of a text in the Properties panel", "[gui][plotboard]") {
    gui::registerAllIcons();
    StormBoard s;
    auto& pm = s.pm;
    addWorkshopCard("character", QStringLiteral("Anna"));
    gui::PropertiesPanel& properties = *s.window.properties;
    auto* box = properties.findChild<gui::PlotFieldsBox*>();
    REQUIRE(box != nullptr);
    auto* description = box->findChild<QPlainTextEdit*>(QStringLiteral("plotDescriptionEdit"));
    auto* pov = box->findChild<QLineEdit*>(QStringLiteral("plotPovEdit"));
    auto* place = box->findChild<QLineEdit*>(QStringLiteral("plotPlaceEdit"));
    auto* time = box->findChild<QLineEdit*>(QStringLiteral("plotTimeEdit"));
    auto* color = box->findChild<QComboBox*>(QStringLiteral("plotColorCombo"));
    REQUIRE(description != nullptr);
    REQUIRE(pov != nullptr);
    REQUIRE(place != nullptr);
    REQUIRE(time != nullptr);
    REQUIRE(color != nullptr);

    // The card chosen on the board shows in the Properties panel
    press(s.board, Qt::Key_Right);
    const QString chosen = PlotBoard::textOf(s.board->currentCard());
    REQUIRE_FALSE(chosen.isEmpty());
    CHECK(box->elementId() == chosen);
    CHECK_FALSE(box->isHidden());

    properties.showChapterProperties(s.storm);
    CHECK(box->elementId() == s.storm);
    REQUIRE(pov->completer() != nullptr);
    CHECK(pov->completer()->model()->rowCount() == 1);

    SECTION("a field keeps what was written when the keys leave it, and the board shows it") {
        pov->setText(QStringLiteral("Anna"));
        emit pov->editingFinished();
        place->setText(QStringLiteral(" The harbour "));
        emit place->editingFinished();
        time->setText(QStringLiteral("Day 3, dawn"));
        emit time->editingFinished();
        description->setPlainText(QStringLiteral("Everything breaks."));
        QFocusEvent out(QEvent::FocusOut);
        QCoreApplication::sendEvent(description, &out);
        color->setCurrentIndex(color->findData(QStringLiteral("rose")));
        emit color->activated(color->currentIndex());

        const core::ProjectElement* element = pm.findElement(s.storm);
        REQUIRE(element != nullptr);
        CHECK(element->description == QStringLiteral("Everything breaks."));
        CHECK(element->plot.pov == QStringLiteral("Anna"));
        CHECK(element->plot.place == QStringLiteral("The harbour"));
        CHECK(element->plot.time == QStringLiteral("Day 3, dawn"));
        CHECK(element->plot.color == QStringLiteral("rose"));
        QCoreApplication::processEvents();
        CHECK(s.panel->card(s.storm)->plot.pov == QStringLiteral("Anna"));
    }

    SECTION("a change on the board shows in the fields") {
        REQUIRE(s.board->setCurrentCard(s.storm));
        press(s.board, Qt::Key_F2);
        const gui::PlotCardEditor* editor = s.board->cardEditor();
        REQUIRE(editor != nullptr);
        editor->povEdit()->setText(QStringLiteral("Ben"));
        editor->descriptionEdit()->setPlainText(QStringLiteral("Calm before."));
        press(editor->titleEdit(), Qt::Key_Return, Qt::ControlModifier);
        CHECK(pov->text() == QStringLiteral("Ben"));
        CHECK(description->toPlainText() == QStringLiteral("Calm before."));
        press(s.board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(pov->text().isEmpty());
    }

    SECTION("only a text of the main section has them") {
        const QList<core::KindRef> backKinds = pm.textKindsFor(core::BookPlace::Back);
        REQUIRE_FALSE(backKinds.isEmpty());
        const QString afterword =
            pm.addElement(backKinds.first(), QStringLiteral("Afterword"), core::BookPlace::Back);
        properties.showChapterProperties(afterword);
        CHECK(box->isHidden());
        CHECK(box->elementId().isEmpty());
        properties.showChapterProperties(s.storm);
        CHECK_FALSE(box->isHidden());
    }
}

namespace {

/// The color of the pixel of the board at @p pos of its viewport
QColor pixelOf(const gui::PlotBoardView* board, const QPoint& pos) {
    return board->viewport()->grab().toImage().pixelColor(pos);
}

/// Show the window of @p scenes large enough for the cards, to see their pixels
void showBoard(StormBoard& scenes) {
    constexpr int WIDTH = 1000;
    constexpr int HEIGHT = 700;
    scenes.window.window.setCentralWidget(scenes.window.tabs);
    scenes.window.window.resize(WIDTH, HEIGHT);
    scenes.window.window.show();
    QCoreApplication::processEvents();
    REQUIRE(scenes.board->setCurrentCard(scenes.storm));
    REQUIRE(scenes.board->viewport()->rect().contains(scenes.board->cardRect(scenes.storm)));
}

}  // namespace

TEST_CASE("Plot board: the look of the board and the outline from Settings > Plot Board",
          "[gui][plotboard]") {
    const KeptSetting colorBy{PlotBoard::COLOR_SETTING};
    const KeptSetting colorPlace{PlotBoard::COLOR_PLACE_SETTING};
    const KeptSetting background{PlotBoard::BACKGROUND_SETTING};
    const KeptSetting startView{PlotBoard::START_VIEW_SETTING};
    const std::string descriptionKey = PlotBoard::fieldSetting(PlotBoard::FIELD_DESCRIPTION);
    const std::string timeKey = PlotBoard::fieldSetting(PlotBoard::FIELD_TIME);
    const std::string wordsKey = PlotBoard::outlineSetting(
        gui::PlotOutlineModel::columnName(gui::PlotOutlineModel::WORDS_COLUMN));
    const KeptSwitch description{descriptionKey.c_str()};
    const KeptSwitch time{timeKey.c_str()};
    const std::string descriptionColumnKey = PlotBoard::outlineSetting(
        gui::PlotOutlineModel::columnName(gui::PlotOutlineModel::DESCRIPTION_COLUMN));
    const KeptSwitch words{wordsKey.c_str()};
    const KeptSwitch descriptionColumn{descriptionColumnKey.c_str()};
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>(PlotBoard::COLOR_SETTING, PlotBoard::COLOR_STATUS);
    settings.set<std::string>(PlotBoard::COLOR_PLACE_SETTING, PlotBoard::COLOR_STRIP);
    settings.set<std::string>(PlotBoard::BACKGROUND_SETTING, PlotBoard::BACKGROUND_THEME);
    settings.set<bool>(descriptionKey, true);
    settings.set<bool>(timeKey, false);
    settings.set<bool>(wordsKey, true);
    settings.set<bool>(descriptionColumnKey, true);
    StormBoard scenes;
    gui::PlotBoardPanel* panel = scenes.panel;
    const gui::PlotBoardView* board = scenes.board;
    QCoreApplication::processEvents();

    SECTION("the fields of the cards") {
        CHECK(board->fields().description);
        CHECK_FALSE(board->fields().time);
        const int tall = board->cardRect(scenes.storm).height();
        settings.set<bool>(descriptionKey, false);
        settings.set<bool>(timeKey, true);
        QCoreApplication::processEvents();
        CHECK_FALSE(board->fields().description);
        CHECK(board->fields().time);
        // Without the lines of the description the cards are lower
        CHECK(board->cardRect(scenes.storm).height() < tall);
    }

    SECTION("the color of a card in a strip at its top or as its background") {
        showBoard(scenes);
        const QRect rect = board->cardRect(scenes.storm);
        REQUIRE(rect.isValid());
        const QColor base = board->palette().color(QPalette::Base);
        const QColor draft = gui::PlotColors::current().ofStatus(QStringLiteral("draft"));
        const QPoint strip(rect.center().x(), rect.top() + 2);
        const QPoint inside(rect.right() - 4, rect.center().y());
        CHECK_FALSE(board->tints());
        CHECK(pixelOf(board, strip) == draft);
        CHECK(pixelOf(board, inside) == base);

        settings.set<std::string>(PlotBoard::COLOR_PLACE_SETTING, PlotBoard::COLOR_TINT);
        QCoreApplication::processEvents();
        CHECK(board->tints());
        const QColor tinted = pixelOf(board, inside);
        CHECK(tinted != base);
        CHECK(pixelOf(board, strip) == tinted);
    }

    SECTION("the background of the board: the theme's, cork or the writer's own") {
        showBoard(scenes);
        const QColor window = board->palette().color(QPalette::Window);
        CHECK(board->background() == gui::PlotBoardView::Background::Theme);
        CHECK(board->viewport()->palette().color(QPalette::Window) == window);

        settings.set<std::string>(PlotBoard::BACKGROUND_SETTING, PlotBoard::BACKGROUND_CORK);
        QCoreApplication::processEvents();
        CHECK(board->background() == gui::PlotBoardView::Background::Cork);
        CHECK_FALSE(board->viewport()->palette().brush(QPalette::Window).texture().isNull());

        settings.set<std::string>(PlotBoard::BACKGROUND_SETTING, PlotBoard::BACKGROUND_OWN);
        QCoreApplication::processEvents();
        CHECK(board->background() == gui::PlotBoardView::Background::Own);
        CHECK(board->viewport()->palette().color(QPalette::Window) ==
              gui::PlotColors::current().background);
        // Under the cards, beyond the last of them
        CHECK(pixelOf(board, QPoint(board->viewport()->width() - 2,
                                    board->viewport()->height() - 2)) ==
              gui::PlotColors::current().background);

        settings.set<std::string>(PlotBoard::BACKGROUND_SETTING, PlotBoard::BACKGROUND_THEME);
        QCoreApplication::processEvents();
        CHECK(board->viewport()->palette().color(QPalette::Window) == window);
    }

    SECTION("the columns of the outline, also from the button over it") {
        using Model = gui::PlotOutlineModel;
        auto* columns = panel->findChild<QToolButton*>(QStringLiteral("plotBoardOutlineColumns"));
        REQUIRE(columns != nullptr);
        CHECK(columns->isHidden());
        panel->showView(QString::fromLatin1(PlotBoard::VIEW_OUTLINE));
        CHECK_FALSE(columns->isHidden());
        QTableView* outline = panel->outline();
        REQUIRE(panel->setCurrentCard(scenes.storm));
        const int row = outline->currentIndex().row();
        outline->setCurrentIndex(panel->outlineModel()->index(row, Model::WORDS_COLUMN));
        CHECK_FALSE(outline->isColumnHidden(Model::WORDS_COLUMN));

        settings.set<bool>(wordsKey, false);
        QCoreApplication::processEvents();
        CHECK(outline->isColumnHidden(Model::WORDS_COLUMN));
        CHECK_FALSE(outline->isColumnHidden(Model::TITLE_COLUMN));
        // The cell chosen was in the column hidden: the title of its row
        CHECK(outline->currentIndex().column() == Model::TITLE_COLUMN);
        CHECK(outline->currentIndex().row() == row);

        // The menu of the button has the columns but the title, as the settings say
        const QList<QAction*> actions = columns->menu()->actions();
        CHECK(actions.size() == Model::COLUMN_COUNT - 1);
        const auto wordsAction = std::ranges::find_if(actions, [](const QAction* action) {
            return action->data().toInt() == Model::WORDS_COLUMN;
        });
        REQUIRE(wordsAction != actions.cend());
        CHECK_FALSE((*wordsAction)->isChecked());
        (*wordsAction)->trigger();
        QCoreApplication::processEvents();
        CHECK(settings.get<bool>(wordsKey));
        CHECK_FALSE(outline->isColumnHidden(Model::WORDS_COLUMN));

        // The description takes the room left; without it the last column
        CHECK_FALSE(outline->horizontalHeader()->stretchLastSection());
        settings.set<bool>(descriptionColumnKey, false);
        QCoreApplication::processEvents();
        CHECK(outline->isColumnHidden(Model::DESCRIPTION_COLUMN));
        CHECK(outline->horizontalHeader()->stretchLastSection());
    }

    SECTION("the view a new tab opens on") {
        settings.set<std::string>(PlotBoard::VIEW_SETTING, PlotBoard::VIEW_OUTLINE);
        settings.set<std::string>(PlotBoard::START_VIEW_SETTING, PlotBoard::START_LAST);
        CHECK(PlotBoard::startView() == QLatin1String(PlotBoard::VIEW_OUTLINE));
        const gui::PlotBoardPanel last;
        CHECK(last.view() == QLatin1String(PlotBoard::VIEW_OUTLINE));
        settings.set<std::string>(PlotBoard::START_VIEW_SETTING, PlotBoard::VIEW_BOARD);
        const gui::PlotBoardPanel onBoard;
        CHECK(onBoard.view() == QLatin1String(PlotBoard::VIEW_BOARD));
        settings.set<std::string>(PlotBoard::VIEW_SETTING, PlotBoard::VIEW_BOARD);
        settings.set<std::string>(PlotBoard::START_VIEW_SETTING, PlotBoard::VIEW_OUTLINE);
        CHECK(PlotBoard::startView() == QLatin1String(PlotBoard::VIEW_OUTLINE));
    }
}

TEST_CASE("Plot board: the gear over the board and the outline opens Settings > Plot Board",
          "[gui][plotboard]") {
    gui::registerAllIcons();
    const StormBoard scenes;
    gui::PlotBoardPanel* panel = scenes.panel;
    int asked = 0;
    QObject::connect(&scenes.boards, &gui::PlotBoardCoordinator::settingsRequested,
                     [&asked]() { ++asked; });
    auto* gear = panel->findChild<QToolButton*>(QStringLiteral("plotBoardSettings"));
    REQUIRE(gear != nullptr);
    CHECK_FALSE(gear->icon().isNull());
    CHECK(gear->toolTip() == QStringLiteral("Plot Board Settings"));
    CHECK(gear->accessibleName() == gear->toolTip());
    CHECK(gear->focusPolicy() == Qt::TabFocus);
    CHECK_FALSE(gear->isHidden());
    gear->click();
    CHECK(asked == 1);
    // Over the outline too
    panel->showView(QString::fromLatin1(PlotBoard::VIEW_OUTLINE));
    CHECK_FALSE(gear->isHidden());
    gear->click();
    CHECK(asked == 2);
}

TEST_CASE("Plot board: the front and back sections shown as a preview", "[gui][plotboard]") {
    const KeptSwitch sections{PlotBoard::SECTIONS_SETTING};
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>(PlotBoard::SECTIONS_SETTING, false);
    StormBoard scenes;
    core::ProjectManager& pm = scenes.pm;
    gui::PlotBoardView* board = scenes.board;
    const QString dedication = pm.addElement(baseKind("dedication"), QStringLiteral("For Leila"),
                                             core::BookPlace::Front);
    const QString thanks = pm.addElement(baseKind("acknowledgments"), QStringLiteral("Thanks"),
                                         core::BookPlace::Back);
    REQUIRE_FALSE(dedication.isEmpty());
    REQUIRE_FALSE(thanks.isEmpty());
    scenes.boards.refresh();
    QCoreApplication::processEvents();

    SECTION("the texts of a section in a band with its name, without scenes and numbers") {
        const gui::PlotBand front = PlotBoard::sectionBand(pm, core::BookPlace::Front);
        const gui::PlotBand back = PlotBoard::sectionBand(pm, core::BookPlace::Back);
        CHECK(front.title == pm.book()->sectionName(core::BookPlace::Front));
        CHECK(back.title == pm.book()->sectionName(core::BookPlace::Back));
        CHECK(front.partId.isEmpty());
        const auto has = [](const gui::PlotBand& band, const QString& id) {
            return std::ranges::any_of(band.cards,
                                       [&id](const PlotCard& card) { return card.id == id; });
        };
        CHECK(has(front, dedication));
        CHECK_FALSE(has(front, thanks));
        CHECK(has(back, thanks));
        for (const PlotCard& card : front.cards) {
            CHECK(card.scenes.isEmpty());
            CHECK(card.number == 0);
        }
        // The main section as it was
        CHECK(boardOfBook() == "Chapter 1, The Storm | Part One: The Calm");
    }

    SECTION("before and after the main section, in columns and in the grid; not chosen") {
        CHECK_FALSE(board->showsSections());
        CHECK_FALSE(board->sectionCardRect(dedication).isValid());
        settings.set<bool>(PlotBoard::SECTIONS_SETTING, true);
        QCoreApplication::processEvents();
        CHECK(board->showsSections());
        const QRect front = board->sectionCardRect(dedication);
        const QRect back = board->sectionCardRect(thanks);
        REQUIRE(front.isValid());
        REQUIRE(back.isValid());
        CHECK(front.right() < board->cardRect(scenes.book.first).left());
        CHECK(back.left() > board->cardRect(scenes.book.calm).right());
        // A preview: its cards are not the board's
        CHECK_FALSE(board->setCurrentCard(dedication));
        CHECK(board->currentCard() == scenes.storm);
        press(board, Qt::Key_Home);
        CHECK(board->currentCard() == scenes.book.first);

        settings.set<std::string>(PlotBoard::LAYOUT_SETTING, PlotBoard::LAYOUT_GRID);
        QCoreApplication::processEvents();
        CHECK(board->sectionCardRect(dedication).bottom() <
              board->cardRect(scenes.book.first).top());
        CHECK(board->sectionCardRect(thanks).top() > board->cardRect(scenes.book.calm).bottom());
    }
}

TEST_CASE("Plot board: the numbers in the titles the program gave follow the moves",
          "[gui][plotboard]") {
    const core::KindRef chapter = baseKind("chapter");
    const core::KindRef part = baseKind("part");
    const core::KindRef dedication = baseKind("dedication");
    REQUIRE(chapter);
    REQUIRE(part);
    REQUIRE(dedication);
    const auto numbered = [](const core::KindRef& kind, const char* title, const char* language,
                             int number) {
        return PlotBoard::numberedTitle(QString::fromUtf8(title), *kind.kind,
                                        QString::fromLatin1(language), number)
            .value_or(QStringLiteral("-"))
            .toStdString();
    };

    SECTION("only the number of a title the program gave changes") {
        CHECK(numbered(chapter, "Chapter 3", "en", 1) == "Chapter 1");
        CHECK(numbered(chapter, "Chapter 3. Storm", "en", 12) == "Chapter 12. Storm");
        CHECK(numbered(chapter, "Chapter 12: Calm", "en", 2) == "Chapter 2: Calm");
        CHECK(numbered(chapter, "Rozdział 3. Burza", "pl", 4) == "Rozdział 4. Burza");
        CHECK(numbered(chapter, "Chapter 3", "en", 3) == "Chapter 3");
        // The writer's own titles stay
        CHECK(numbered(chapter, "The Storm", "en", 1) == "-");
        CHECK(numbered(chapter, "Chapter 3a", "en", 1) == "-");
        CHECK(numbered(chapter, "Chapters 3", "en", 1) == "-");
        CHECK(numbered(chapter, "Chapter three", "en", 1) == "-");
        CHECK(numbered(chapter, "Rozdział 3", "en", 1) == "-");
        // Roman numbers, as the kind has them
        CHECK(numbered(part, "Part II", "en", 3) == "Part III");
        CHECK(numbered(part, "Part IV. The Sea", "en", 1) == "Part I. The Sea");
        CHECK(numbered(part, "Part Ivy", "en", 1) == "-");
        // A kind without a number in its titles
        CHECK(numbered(dedication, "Dedication", "en", 1) == "-");
    }

    const KeptSetting renumber{PlotBoard::RENUMBER_SETTING};
    auto& settings = core::SettingsManager::getInstance();
    StormBoard scenes;
    core::ProjectManager& pm = scenes.pm;
    gui::PlotBoardView* board = scenes.board;
    gui::PlotBoardPanel* panel = scenes.panel;
    const QString& first = scenes.book.first;
    const QString& storm = scenes.storm;
    const QString& calm = scenes.book.calm;
    REQUIRE(pm.renameElement(storm, QStringLiteral("Chapter 2. The Storm")));
    REQUIRE(pm.renameElement(calm, QStringLiteral("Chapter 3")));
    scenes.boards.refresh();
    QCoreApplication::processEvents();
    CHECK(PlotBoard::renumbered(pm).isEmpty());
    const auto titles = [&pm, &first, &storm, &calm]() {
        return QStringList{pm.findElement(first)->title, pm.findElement(storm)->title,
                           pm.findElement(calm)->title}
            .join(QStringLiteral(", "))
            .toStdString();
    };
    const auto barButton = [panel]() {
        auto* bar = panel->findChild<QWidget*>(QStringLiteral("plotBoardInfoBar"));
        REQUIRE(bar != nullptr);
        auto* button = bar->findChild<QPushButton*>();
        REQUIRE(button != nullptr);
        return button;
    };
    REQUIRE(board->setCurrentCard(calm));

    SECTION("the texts of a kind numbered in the order of the book") {
        REQUIRE(pm.renameElement(first, QStringLiteral("Chapter 5")));
        const QList<PlotBoard::TitleChange> changes = PlotBoard::renumbered(pm);
        REQUIRE(changes.size() == 1);
        CHECK(changes.constFirst().id == first);
        CHECK(changes.constFirst().title == QStringLiteral("Chapter 1"));
    }

    SECTION("offered on the bar: Renumber is a step of its own") {
        settings.set<std::string>(PlotBoard::RENUMBER_SETTING, PlotBoard::RENUMBER_ASK);
        press(board, Qt::Key_Left, Qt::ControlModifier);
        press(board, Qt::Key_Left, Qt::ControlModifier);
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(boardOf(board->bands()) == "Chapter 3, Chapter 1, Chapter 2. The Storm | Part One: ");
        CHECK(titles() == "Chapter 1, Chapter 2. The Storm, Chapter 3");
        CHECK(panel->message() == QStringLiteral("\"Chapter 3\" moved. The numbers in the titles "
                                                 "no longer follow the order."));
        CHECK(barButton()->text() == QStringLiteral("&Renumber"));

        barButton()->click();
        CHECK(titles() == "Chapter 2, Chapter 3. The Storm, Chapter 1");
        CHECK(panel->message() ==
              QStringLiteral("3 titles renumbered; only the numbers changed."));
        CHECK(barButton()->text() == QStringLiteral("&Undo"));
        barButton()->click();
        CHECK(titles() == "Chapter 1, Chapter 2. The Storm, Chapter 3");
        CHECK(boardOf(board->bands()) == "Chapter 3, Chapter 1, Chapter 2. The Storm | Part One: ");
    }

    SECTION("at once, in the same step") {
        settings.set<std::string>(PlotBoard::RENUMBER_SETTING, PlotBoard::RENUMBER_AUTO);
        REQUIRE(board->setCurrentCard(storm));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(boardOf(board->bands()) == "Chapter 1. The Storm, Chapter 2 | Part One: Chapter 3");
        CHECK(titles() == "Chapter 2, Chapter 1. The Storm, Chapter 3");
        CHECK(panel->message() ==
              QStringLiteral("\"Chapter 2. The Storm\" moved. The titles were renumbered."));
        press(board, Qt::Key_Z, Qt::ControlModifier);
        CHECK(boardOf(board->bands()) == "Chapter 1, Chapter 2. The Storm | Part One: Chapter 3");
        CHECK(titles() == "Chapter 1, Chapter 2. The Storm, Chapter 3");
        press(board, Qt::Key_Y, Qt::ControlModifier);
        CHECK(titles() == "Chapter 2, Chapter 1. The Storm, Chapter 3");
    }

    SECTION("never") {
        settings.set<std::string>(PlotBoard::RENUMBER_SETTING, PlotBoard::RENUMBER_NEVER);
        REQUIRE(board->setCurrentCard(storm));
        press(board, Qt::Key_Left, Qt::ControlModifier);
        CHECK(titles() == "Chapter 1, Chapter 2. The Storm, Chapter 3");
        CHECK(panel->message() == QStringLiteral("\"Chapter 2. The Storm\" moved."));
        CHECK(barButton()->text() == QStringLiteral("&Undo"));
    }
}
