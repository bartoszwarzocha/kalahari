// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_book_plan_chronology.cpp
/// @brief The chronology of the Book Plan on its tab: the scenes of the book in the order of
/// the story, their times changed with the keys and undone, the events outside the text in the
/// file of the plan, and the times of the timelines of the Workshop offered to the cards

#include "kalahari_tests/gui/document_window.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plan_chronology.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/timeline/plan_chronology_layout.h"
#include "kalahari/timeline/plan_chronology_view.h"
#include "kalahari/timeline/story_time.h"
#include "kalahari/timeline/timeline.h"
#include "kalahari/timeline/timeline_module.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <functional>
#include <string>

#include "kalahari_tests/test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
namespace timeline = kalahari::timeline;
using gui::PlotBoard;
using Command = timeline::PlanChronologyView::Command;

namespace {

const qint64 DAY_ONE = timeline::CalendarDay{1931, 4, 14}.julianDay().value_or(0);

void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// Whether @p widget takes key @p key before the commands of the window
bool takesKey(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();
    QCoreApplication::sendEvent(widget, &event);
    return event.isAccepted();
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

/// The module of the timelines registered until the test ends
struct TimelineModule {
    TimelineModule() {
        gui::registerAllIcons();
        timeline::registerTimelineModule();
    }
    ~TimelineModule() {
        auto& registry = core::ExtensionPointRegistry::getInstance();
        registry.unregisterPlugin("kalahari.timelines");
        registry.unregisterPlugin("kalahari.timelines.plan");
    }
    TimelineModule(const TimelineModule&) = delete;
    TimelineModule& operator=(const TimelineModule&) = delete;
    TimelineModule(TimelineModule&&) = delete;
    TimelineModule& operator=(TimelineModule&&) = delete;
};

/// The window of a card or of the periods shown now; nullptr: none
gui::dialogs::KalahariDialog* shownDialog(const char* name) {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* dialog = qobject_cast<gui::dialogs::KalahariDialog*>(widget);
        if (dialog != nullptr && dialog->isVisible() &&
            dialog->objectName() == QLatin1String(name)) {
            return dialog;
        }
    }
    return nullptr;
}

/// Run @p command, fill the window @p name it shows with @p fill and press its Save
/// @return Whether the window was shown
bool filling(const char* name, const std::function<void(QWidget&)>& fill,
             const std::function<void()>& command) {
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &fill, name]() {
        gui::dialogs::KalahariDialog* dialog = shownDialog(name);
        if (dialog == nullptr) {
            return;
        }
        shown = true;
        fill(*dialog);
        dialog->acceptButton()->click();
    });
    timer.start();
    command();
    return shown;
}

/// Field @p name of window @p dialog
QLineEdit* fieldOf(QWidget& dialog, const char* name) {
    auto* field = dialog.findChild<QLineEdit*>(QString::fromLatin1(name));
    REQUIRE(field != nullptr);
    return field;
}

/// Run @p command and give the question it asks @p answer
/// @return Whether a question was asked
bool answering(const std::function<void()>& command, gui::dialogs::MessageDialog::Answer answer,
               QString* asked = nullptr) {
    using gui::dialogs::MessageDialog;
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, answer, asked]() {
        MessageDialog* message = nullptr;
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* found = qobject_cast<MessageDialog*>(widget);
                found != nullptr && found->isVisible()) {
                message = found;
            }
        }
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
        button->click();
    });
    timer.start();
    command();
    return shown;
}

/// The text of "The Storm": three scenes, the second titled "Night", the third on day 7
constexpr const char* STORM_SCENES = "<kml><p>One two</p><scene-break title=\"Night\"/>"
                                     "<p>Three four five</p><scene-break time=\"Day 7\"/>"
                                     "<p>Six</p></kml>";

/// The plan of the time of the book as its file keeps it; empty: no file
QJsonObject planFile(const gui::PlotBoardCoordinator& boards) {
    QFile file(boards.planPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

/// The labels of the columns of the chronology, a gap in brackets
std::string columnsOf(const timeline::PlanChronologyView& view) {
    QStringList labels;
    for (const timeline::ChronologyColumn& column : view.grid()->chronology()->columns()) {
        labels << (column.gap ? QStringLiteral("[%1]").arg(column.label) : column.label);
    }
    return labels.join(QStringLiteral(", ")).toStdString();
}

/// A novel of "Chapter 1" on day 5 told by Amara, and "The Storm" on day 6 told by Selim with
/// three scenes: the first on day 2, "Night" without a time of its own, the last on day 7
struct ChronologyBook {
    KeptSetting view{PlotBoard::VIEW_SETTING};
    KeptSetting variant{PlotBoard::SCENES_VARIANT_SETTING};
    TimelineModule module;
    test::OpenNovel novel{QStringLiteral("Rain")};
    core::ProjectManager& pm = novel.pm;
    QString first;
    QString storm = novel.chapterId;
    test::Window window;
    gui::PlotBoardCoordinator boards{pm, window.tabs, &window.navigator, &window.statusBar,
                                     [](bool /*keys*/) {}};
    gui::PlotBoardPanel* panel = nullptr;
    timeline::PlanChronologyView* chronology = nullptr;

    explicit ChronologyBook(bool times = true) {
        core::SettingsManager::getInstance().set<std::string>(PlotBoard::SCENES_VARIANT_SETTING,
                                                              PlotBoard::SCENES_TEXT);
        const QList<gui::PlotCard> cards = PlotBoard::cardsOf(PlotBoard::bandsOf(pm));
        REQUIRE(cards.size() == 2);
        first = cards.at(0).id;
        pm.setChapterContent(storm, QString::fromLatin1(STORM_SCENES));
        REQUIRE(pm.saveChapterContent(storm));
        if (times) {
            core::ElementPlot plot;
            plot.time = QStringLiteral("Day 5");
            plot.pov = QStringLiteral("Amara");
            REQUIRE(pm.setPlot(first, QString(), plot));
            plot.time = QStringLiteral("Day 6");
            plot.pov = QStringLiteral("Selim");
            plot.opening.time = QStringLiteral("Day 2");
            REQUIRE(pm.setPlot(storm, QString(), plot));
        }
    }

    void open() {
        panel = boards.openBoard(QString::fromLatin1(PlotBoard::VIEW_CHRONOLOGY), true);
        REQUIRE(panel != nullptr);
        chronology = qobject_cast<timeline::PlanChronologyView*>(panel->chronology());
        REQUIRE(chronology != nullptr);
    }

    [[nodiscard]] core::ScenePlot sceneOf(int scene) const {
        return pm.scenesOf(storm).at(scene).plot;
    }
};

}  // namespace

TEST_CASE("Book plan: the chronology shows the scenes of the book in the order of the story",
          "[gui][plotboard][chronology]") {
    ChronologyBook book;
    book.open();
    gui::PlotBoardPanel* panel = book.panel;
    timeline::PlanChronologyView* chronology = book.chronology;
    const QString opening = PlotBoard::sceneId(book.storm, 0);
    const QString night = PlotBoard::sceneId(book.storm, 1);
    const QString last = PlotBoard::sceneId(book.storm, 2);

    // The third view of the Book Plan, with its own options and commands
    CHECK(panel->view() == QLatin1String(PlotBoard::VIEW_CHRONOLOGY));
    CHECK(panel->findChild<QWidget*>(QStringLiteral("plotBoardCommands"))->isHidden());
    CHECK(core::SettingsManager::getInstance().get<std::string>(PlotBoard::VIEW_SETTING, "") ==
          PlotBoard::VIEW_CHRONOLOGY);

    // A text without scene breaks is one card, a text with them its scenes, numbered in the
    // order of reading; a scene without a time of its own has its text's
    const QList<gui::PlanChronologyCard> cards = panel->chronologyCards();
    REQUIRE(cards.size() == 4);
    CHECK(cards.at(0).id == book.first);
    CHECK(cards.at(0).number == QStringLiteral("1"));
    CHECK(cards.at(1).id == opening);
    CHECK(cards.at(1).number == QStringLiteral("2.1"));
    CHECK(cards.at(1).time == QStringLiteral("Day 2"));
    CHECK(cards.at(2).id == night);
    CHECK(cards.at(2).title == QStringLiteral("Night"));
    CHECK(cards.at(2).time == QStringLiteral("Day 6"));
    CHECK(cards.at(2).pov == QStringLiteral("Selim"));
    CHECK(cards.at(2).textTime);
    CHECK(cards.at(3).number == QStringLiteral("2.3"));
    CHECK_FALSE(cards.at(3).textTime);

    // The days in columns; the first scene of "The Storm", told after day 5, is a flashback
    CHECK(columnsOf(*chronology) == "Day 2, [2 days], Day 5, Day 6, Day 7");
    const timeline::ChronologyLayout& layout = *chronology->grid()->chronology();
    CHECK(layout.items().at(layout.indexOf(opening)).flashback);
    CHECK_FALSE(layout.items().at(layout.indexOf(night)).flashback);

    SECTION("a text chosen elsewhere is its first scene here; the line under it says what it is") {
        REQUIRE(panel->setCurrentCard(book.storm));
        CHECK(panel->currentCard() == opening);
        CHECK(panel->hint().contains(QStringLiteral("flashback")));
        REQUIRE(panel->setCurrentCard(night));
        CHECK(panel->hint().contains(QStringLiteral("the time of the text")));
        // The board shows the scene chosen here
        panel->showView(QString::fromLatin1(PlotBoard::VIEW_BOARD));
        CHECK(panel->currentCard() == night);
        CHECK_FALSE(panel->findChild<QWidget*>(QStringLiteral("plotBoardCommands"))->isHidden());
    }

    SECTION("Ctrl+Right gives the scene a day later, Ctrl+Z undoes it and Ctrl+Y does it again") {
        REQUIRE(panel->setCurrentCard(last));
        QWidget* grid = chronology->grid();
        // The keys of the chronology are its own, not the commands of the window
        CHECK(takesKey(grid, Qt::Key_Right, Qt::ControlModifier));
        CHECK(takesKey(grid, Qt::Key_Z, Qt::ControlModifier));
        CHECK(takesKey(grid, Qt::Key_Delete));
        press(grid, Qt::Key_Right, Qt::ControlModifier);
        CHECK(book.sceneOf(2).time == QStringLiteral("Day 8"));
        CHECK_FALSE(panel->message().isEmpty());
        CHECK(panel->currentCard() == last);
        press(grid, Qt::Key_Z, Qt::ControlModifier);
        CHECK(book.sceneOf(2).time == QStringLiteral("Day 7"));
        press(grid, Qt::Key_Y, Qt::ControlModifier);
        CHECK(book.sceneOf(2).time == QStringLiteral("Day 8"));
        // Delete takes its time of the action away: it has its text's then
        press(grid, Qt::Key_Delete);
        CHECK(book.sceneOf(2).time.isEmpty());
        CHECK(panel->chronologyCards().at(3).textTime);
    }

    SECTION("a scene with the time of its text gets its own; the rest stays its text's") {
        REQUIRE(panel->setCurrentCard(night));
        CHECK_FALSE(chronology->command(Command::Delete)->isEnabled());
        press(chronology->grid(), Qt::Key_Left, Qt::ControlModifier);
        CHECK(book.sceneOf(1).time == QStringLiteral("Day 5"));
        CHECK(book.sceneOf(1).pov.isEmpty());
        CHECK(book.pm.findElement(book.storm)->plot.time == QStringLiteral("Day 6"));
        CHECK(chronology->command(Command::Delete)->isEnabled());
    }

    SECTION("F2 edits the card of a text in the window Card") {
        REQUIRE(panel->setCurrentCard(book.first));
        CHECK(filling(
            "planCardDialog",
            [](QWidget& dialog) {
                fieldOf(dialog, "planCardTime")->setText(QStringLiteral("Day 9, night"));
                fieldOf(dialog, "planCardPov")->setText(QStringLiteral("Nadia"));
            },
            [&chronology]() { press(chronology->grid(), Qt::Key_F2); }));
        const core::ProjectElement* text = book.pm.findElement(book.first);
        REQUIRE(text != nullptr);
        CHECK(text->plot.time == QStringLiteral("Day 9, night"));
        CHECK(text->plot.pov == QStringLiteral("Nadia"));
        CHECK(text->title == QStringLiteral("Chapter 1"));
    }

    SECTION("Insert adds an event outside the text to the file of the plan, Ctrl+Z takes it") {
        REQUIRE(panel->setCurrentCard(book.first));
        CHECK(planFile(book.boards).isEmpty());
        CHECK(filling(
            "planCardDialog",
            [](QWidget& dialog) {
                fieldOf(dialog, "planCardTitle")->setText(QStringLiteral("Selim draws the map"));
                fieldOf(dialog, "planCardTime")->setText(QStringLiteral("Day 1"));
            },
            [&chronology]() { press(chronology->grid(), Qt::Key_Insert); }));
        const QJsonArray events = planFile(book.boards).value(QStringLiteral("events")).toArray();
        REQUIRE(events.size() == 1);
        CHECK(events.at(0).toObject().value(QStringLiteral("title")).toString() ==
              QStringLiteral("Selim draws the map"));
        // In the band of the card chosen, before the scenes of the book
        CHECK(columnsOf(*chronology).starts_with("Day 1, Day 2"));
        CHECK(panel->currentCard().startsWith(QLatin1String(gui::PlanChronology::EVENT_PREFIX)));
        CHECK(panel->message().contains(QStringLiteral("Selim draws the map")));

        // How the chronology shows the plan is kept, but Undo leaves it as it is
        auto* scale = chronology->findChild<QComboBox*>(QStringLiteral("planChronologyScale"));
        REQUIRE(scale != nullptr);
        scale->setCurrentIndex(1);
        const QJsonValue view = planFile(book.boards).value(QStringLiteral("view"));
        press(chronology->grid(), Qt::Key_Z, Qt::ControlModifier);
        CHECK(planFile(book.boards).value(QStringLiteral("events")).toArray().isEmpty());
        CHECK(planFile(book.boards).value(QStringLiteral("view")) == view);
        press(chronology->grid(), Qt::Key_Y, Qt::ControlModifier);
        CHECK(planFile(book.boards).value(QStringLiteral("events")).toArray().size() == 1);
    }
}

TEST_CASE("Book plan: the times of the timelines of the Workshop offered to the cards",
          "[gui][plotboard][chronology]") {
    ChronologyBook book(false);
    // "Expedition" in the Workshop tells when "The Storm" happens: its earliest event, on day
    // 1; "Chapter 1" has a time of its own
    core::ElementPlot plot;
    plot.time = QStringLiteral("Day 9");
    REQUIRE(book.pm.setPlot(book.first, QString(), plot));
    const core::KindRef kind = book.pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                            QStringLiteral("timeline"));
    REQUIRE(kind);
    const QString id =
        book.pm.addElement(kind, QStringLiteral("Expedition"), core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const core::ProjectElement* element = book.pm.findElement(id);
    REQUIRE(element != nullptr);
    REQUIRE_FALSE(element->file.isEmpty());
    timeline::Timeline expedition;
    expedition.setDayOne(DAY_ONE);
    timeline::TimelineView view;
    view.scale = timeline::TimelineView::Scale::Days;
    expedition.setView(view);
    const auto addEvent = [&expedition](const QString& title, const QString& time,
                                        const QStringList& chapters) {
        timeline::TimelineEvent event;
        event.title = title;
        event.chapters = chapters;
        event.characters = {QStringLiteral("Amara")};
        event.place = QStringLiteral("Cairo");
        expedition.setTimeOf(event, time);
        expedition.addEvent(event);
    };
    addEvent(QStringLiteral("The storm comes"), QStringLiteral("Day 3"), {book.storm});
    addEvent(QStringLiteral("The camels run"), QStringLiteral("Day 1"), {book.storm});
    addEvent(QStringLiteral("The map"), QStringLiteral("Day 4"), {book.first});
    REQUIRE(expedition.save(book.pm.filePathOf(*element)));

    using gui::dialogs::MessageDialog;
    SECTION("Copy the Times gives them to the cards without a time, in one step of Undo") {
        QString asked;
        CHECK(answering([&book]() { book.open(); }, MessageDialog::Answer::Accept, &asked));
        CHECK(asked.contains(QStringLiteral("Expedition")));
        const core::ProjectElement* storm = book.pm.findElement(book.storm);
        CHECK(storm->plot.time == QStringLiteral("Day 1"));
        CHECK(storm->plot.pov == QStringLiteral("Amara"));
        CHECK(storm->plot.place == QStringLiteral("Cairo"));
        CHECK(book.pm.findElement(book.first)->plot.time == QStringLiteral("Day 9"));
        // The plan counts the days from day one of the timeline
        const std::optional<timeline::StoryTime> dayOne = timeline::StoryTime::fromJson(
            QJsonObject{{QStringLiteral("from"), planFile(book.boards).value("dayOne")}});
        REQUIRE(dayOne);
        CHECK(dayOne->first == DAY_ONE);
        CHECK(book.panel->message().contains(QStringLiteral("timelines")));
        press(book.chronology->grid(), Qt::Key_Z, Qt::ControlModifier);
        CHECK(book.pm.findElement(book.storm)->plot.time.isEmpty());
        // The timeline stays as it was
        timeline::Timeline kept;
        REQUIRE(kept.load(book.pm.filePathOf(*book.pm.findElement(id))));
        CHECK(kept.events().size() == 3);
    }

    SECTION("Don't Copy leaves the cards, and the question is not asked again") {
        CHECK(answering([&book]() { book.open(); }, MessageDialog::Answer::Alternative));
        CHECK(book.pm.findElement(book.storm)->plot.time.isEmpty());
        CHECK_FALSE(planFile(book.boards).isEmpty());
        book.panel->showView(QString::fromLatin1(PlotBoard::VIEW_BOARD));
        CHECK_FALSE(answering(
            [&book]() { book.panel->showView(QString::fromLatin1(PlotBoard::VIEW_CHRONOLOGY)); },
            MessageDialog::Answer::Accept));
    }
}
