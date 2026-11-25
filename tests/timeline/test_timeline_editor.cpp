// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_timeline_editor.cpp
/// @brief The editor of timelines: the timeline open on its tab with Undo, where the axis
/// draws the events, the changes the views share, the axis and the list worked with the keys,
/// the time and the book, and the module the program knows

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/extension_points.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/window_editor_provider.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/settings/settings_page.h>
#include <kalahari/gui/widgets/card_bar.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_axis_view.h>
#include <kalahari/timeline/timeline_book_view.h>
#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_editor.h>
#include <kalahari/timeline/timeline_edits.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_list.h>
#include <kalahari/timeline/timeline_look.h>
#include <kalahari/timeline/timeline_module.h>
#include <kalahari/timeline/timeline_scene.h>
#include <kalahari/timeline/timeline_settings_page.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <cstdio>
#include <memory>
#include <string>

using namespace kalahari::timeline;

namespace {

const qint64 APRIL_14 = CalendarDay{1931, 4, 14}.julianDay().value_or(0);

/// A key press sent to a widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &event);
}

/// Whether a widget with the keys keeps a key from the window's shortcuts
bool keepsFromShortcuts(QWidget* widget, int key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();
    QApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// The program in Polish while it lives: the translation it ships and the language of the times
class Polish {
public:
    Polish()
        : m_before(kalahari::core::SettingsManager::getInstance().get<std::string>("ui.language",
                                                                                  "en")) {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
        kalahari::core::SettingsManager::getInstance().set<std::string>("ui.language", "pl");
    }
    ~Polish() {
        QCoreApplication::removeTranslator(&m_translator);
        try {
            kalahari::core::SettingsManager::getInstance().set<std::string>("ui.language",
                                                                            m_before);
        } catch (...) {
            // A destructor may not throw: the later tests then write the times in Polish
            static_cast<void>(std::fputs("Polish: the language of the test stays\n", stderr));
        }
    }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;
    Polish(Polish&&) = delete;
    Polish& operator=(Polish&&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    std::string m_before;
    bool m_loaded = false;
};

/// An event of thread @p thread at day @p day (none: without a time)
TimelineEvent eventOf(const QString& title, const QString& thread, std::optional<qint64> day) {
    TimelineEvent event;
    event.title = title;
    event.thread = thread;
    if (day) {
        event.time = StoryTime::day(*day);
    }
    return event;
}

/// A timeline open on its tab, its file in a folder of its own, with two threads
struct OpenTimeline {
    QTemporaryDir dir;
    QString journey;
    QString well;
    TimelineDocument document{dir.filePath(QStringLiteral("timeline.ktl"))};

    OpenTimeline() {
        Timeline timeline;
        timeline.setDayOne(APRIL_14);
        journey = timeline.addThread(QStringLiteral("Wyprawa"), QStringLiteral("1"));
        well = timeline.addThread(QStringLiteral("Studnia"), QStringLiteral("2"));
        timeline.save(document.path());
        document.load();
    }

    /// Event @p title of thread @p thread at day @p day (none: without a time)
    QString add(const QString& title, const QString& thread, std::optional<qint64> day) {
        QString added;
        document.change(QStringLiteral("adding"), [&](Timeline& timeline) {
            added = timeline.addEvent(eventOf(title, thread, day));
            return true;
        });
        return added;
    }

    [[nodiscard]] const TimelineEvent& event(const QString& id) const {
        return *document.timeline().event(id);
    }
};

}  // namespace

TEST_CASE("Timeline document: each change is a step of Undo, saved to the file at once",
          "[timeline][document]") {
    OpenTimeline open;
    TimelineDocument& document = open.document;
    CHECK_FALSE(document.canUndo());

    int changes = 0;
    QObject::connect(&document, &TimelineDocument::changed, [&changes]() { ++changes; });
    const QString compass = open.add(QStringLiteral("Kompas"), open.journey, APRIL_14 + 2);
    REQUIRE_FALSE(compass.isEmpty());
    CHECK(changes == 1);
    CHECK(document.undoText() == QStringLiteral("adding"));

    Timeline saved;
    REQUIRE(saved.load(document.path()));
    REQUIRE(saved.event(compass) != nullptr);
    CHECK(saved.event(compass)->title == QStringLiteral("Kompas"));

    // A change that changes nothing is no step
    CHECK_FALSE(document.changeEvent(compass, QStringLiteral("nothing"),
                                     [](TimelineEvent& event) { event.title = "Kompas"; }));
    CHECK(document.undoText() == QStringLiteral("adding"));

    REQUIRE(document.changeEvent(compass, QStringLiteral("renaming"), [](TimelineEvent& event) {
        event.title = QStringLiteral("Kompas przy studni");
    }));
    document.undo();
    CHECK(open.event(compass).title == QStringLiteral("Kompas"));
    CHECK(document.redoText() == QStringLiteral("renaming"));
    document.redo();
    CHECK(open.event(compass).title == QStringLiteral("Kompas przy studni"));

    // The view is saved, but it is no step of Undo, and Undo keeps it
    document.changeView([](TimelineView& view) { view.zoom = 2; });
    CHECK(document.undoText() == QStringLiteral("renaming"));
    REQUIRE(saved.load(document.path()));
    CHECK(saved.view().zoom == 2);
    document.undo();
    CHECK(document.timeline().view().zoom == 2);
}

TEST_CASE("Timeline scene: a lane for each thread, rows that do not overlap, the tray",
          "[timeline][scene]") {
    OpenTimeline open;
    const QString compass = open.add(QStringLiteral("Kompas"), open.journey, APRIL_14);
    const QString storm = open.add(QStringLiteral("Burza"), open.journey, APRIL_14);
    open.add(QStringLiteral("Selim rysuje mapę"), open.journey, std::nullopt);
    const TimelineIndex index;
    const TimelineLook look = TimelineLook::current();
    TimelineScene scene(open.document.timeline(), index, look, QApplication::font());
    scene.layout(900.0);

    // Without events without a thread, no lane for them
    REQUIRE(scene.lanes().size() == 2);
    CHECK(scene.lanes().at(0).name == QStringLiteral("Wyprawa"));
    CHECK(scene.lanes().at(0).count == 2);
    CHECK(scene.lanes().at(1).count == 0);

    // Two events of the same day in one lane: two rows
    const TimelineItem* first = scene.itemOf(compass, open.journey);
    const TimelineItem* second = scene.itemOf(storm, open.journey);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK_FALSE(first->rect.intersects(second->rect));
    CHECK(scene.itemAt(first->rect.center()) == first);
    CHECK(scene.lane(open.journey)->height > first->rect.height() * 2);

    // An event without a time waits in the tray, not on the axis
    REQUIRE(scene.trayItems().size() == 1);
    CHECK(scene.trayHeight() > 0.0);
    CHECK(scene.onAxis().size() == 2);

    // An event without a thread brings its lane
    open.add(QStringLiteral("Ktoś zasypuje źródło"), QString(), APRIL_14 + 5);
    TimelineScene again(open.document.timeline(), index, look, QApplication::font());
    again.layout(900.0);
    REQUIRE(again.lanes().size() == 3);
    CHECK(again.lanes().at(2).key.isEmpty());
}

TEST_CASE("Timeline edits: add after an event, move it in time and between lanes",
          "[timeline][edits]") {
    OpenTimeline open;
    TimelineDocument& document = open.document;
    const TimelineLook look = TimelineLook::current();
    const QString compass = open.add(QStringLiteral("Kompas"), open.journey, APRIL_14 + 2);
    QString message;

    // A new event: the day after, in the lane of the event before
    const QString added = TimelineEdits::addAfter(document, compass, std::nullopt, &message);
    REQUIRE_FALSE(added.isEmpty());
    CHECK(document.selected() == added);
    CHECK(open.event(added).thread == open.journey);
    REQUIRE(open.event(added).time.has_value());
    CHECK(open.event(added).time->first == APRIL_14 + 3);
    CHECK_FALSE(message.isEmpty());

    // At 30 pixels a day a step is a day; at 1 pixel a day it is a year
    REQUIRE(TimelineEdits::shift(document, compass, 1, 30.0, look, &message));
    CHECK(open.event(compass).time->first == APRIL_14 + 3);
    REQUIRE(TimelineEdits::shift(document, compass, -1, 1.0, look, &message));
    CHECK(CalendarDay::of(open.event(compass).time->first).year == 1930);
    document.undo();
    CHECK(open.event(compass).time->first == APRIL_14 + 3);

    // Without a time it cannot move, and it says why
    const QString drawing = open.add(QStringLiteral("Selim rysuje mapę"), QString(), std::nullopt);
    message.clear();
    CHECK_FALSE(TimelineEdits::shift(document, drawing, 1, 30.0, look, &message));
    CHECK_FALSE(message.isEmpty());

    // In the order of the story a step is a place
    document.changeView([](TimelineView& view) { view.scale = TimelineView::Scale::Order; });
    const QList<const TimelineEvent*> before = document.timeline().eventsInOrder();
    REQUIRE(before.constFirst()->id == compass);
    REQUIRE(TimelineEdits::shift(document, compass, 1, 0.0, look, &message));
    CHECK(document.timeline().eventsInOrder().at(1)->id == compass);

    // To another thread
    REQUIRE(TimelineEdits::moveToLane(document, compass, open.journey, open.well,
                                      QStringLiteral("Studnia"), &message));
    CHECK(open.event(compass).thread == open.well);
    CHECK(document.selectedLane() == open.well);

    // A character an event already has is not added twice
    TimelineEvent both = open.event(compass);
    both.characters = {QStringLiteral("Amara"), QStringLiteral("Idris")};
    CHECK_FALSE(TimelineEdits::setLane(both, TimelineView::Lanes::Characters,
                                       QStringLiteral("Amara"), QStringLiteral("Idris")));
    REQUIRE(TimelineEdits::setLane(both, TimelineView::Lanes::Characters,
                                   QStringLiteral("Amara"), QStringLiteral("Selim")));
    CHECK(both.characters == QStringList{QStringLiteral("Idris"), QStringLiteral("Selim")});

    // Deleting selects the event after it
    REQUIRE(TimelineEdits::remove(document, compass, &message));
    CHECK(document.timeline().event(compass) == nullptr);
    CHECK_FALSE(document.selected().isEmpty());
}

TEST_CASE("Timeline axis: the keys add, rename, move and delete events",
          "[timeline][axis][gui]") {
    OpenTimeline open;
    TimelineDocument& document = open.document;
    const QString compass = open.add(QStringLiteral("Kompas"), open.journey, APRIL_14 + 2);
    TimelineAxisView axis(document);
    axis.resize(1000, 500);
    axis.show();
    QApplication::processEvents();

    // The view keeps its keys from the shortcuts of the window
    CHECK(keepsFromShortcuts(&axis, Qt::Key_Z, Qt::ControlModifier));
    CHECK(keepsFromShortcuts(&axis, Qt::Key_Delete, Qt::NoModifier));

    QString told;
    QObject::connect(&axis, &TimelineAxisView::told,
                     [&told](const QString& message, bool /*undoable*/) { told = message; });
    document.select(compass, open.journey);

    // Insert: a new event the day after, its title typed in place
    press(&axis, Qt::Key_Insert);
    REQUIRE(axis.isEditing());
    const QString added = document.selected();
    REQUIRE(added != compass);
    auto* editor = axis.viewport()->findChild<QLineEdit*>();
    REQUIRE(editor != nullptr);
    editor->setText(QStringLiteral("Burza piaskowa"));
    press(editor, Qt::Key_Return);
    CHECK_FALSE(axis.isEditing());
    CHECK(open.event(added).title == QStringLiteral("Burza piaskowa"));
    CHECK(open.event(added).time->first == APRIL_14 + 3);

    // A new event left without a title goes away
    press(&axis, Qt::Key_Insert);
    REQUIRE(axis.isEditing());
    const QString empty = document.selected();
    press(axis.viewport()->findChild<QLineEdit*>(), Qt::Key_Escape);
    CHECK(document.timeline().event(empty) == nullptr);

    // Ctrl+Right: a day later; Ctrl+Z undoes it
    document.select(compass, open.journey);
    press(&axis, Qt::Key_Right, Qt::ControlModifier);
    CHECK(open.event(compass).time->first == APRIL_14 + 3);
    CHECK(told.contains(QStringLiteral("Kompas")));
    press(&axis, Qt::Key_Z, Qt::ControlModifier);
    CHECK(open.event(compass).time->first == APRIL_14 + 2);

    // Ctrl+Down: to the next thread
    press(&axis, Qt::Key_Down, Qt::ControlModifier);
    CHECK(open.event(compass).thread == open.well);

    // Right: the next event in time
    document.select(compass, open.well);
    press(&axis, Qt::Key_Right);
    CHECK(document.selected() == added);

    // Typing renames the event
    press(&axis, Qt::Key_K, Qt::NoModifier, QStringLiteral("K"));
    REQUIRE(axis.isEditing());
    CHECK(axis.viewport()->findChild<QLineEdit*>()->text() == QStringLiteral("K"));
    press(axis.viewport()->findChild<QLineEdit*>(), Qt::Key_Escape);
    CHECK(open.event(added).title == QStringLiteral("Burza piaskowa"));

    // Delete, and Undo brings it back
    press(&axis, Qt::Key_Delete);
    CHECK(document.timeline().event(added) == nullptr);
    press(&axis, Qt::Key_Z, Qt::ControlModifier);
    CHECK(document.timeline().event(added) != nullptr);

    // Zoom: Ctrl+minus, Ctrl+0
    const int percent = axis.zoomPercent();
    press(&axis, Qt::Key_Minus, Qt::ControlModifier);
    CHECK(axis.zoomPercent() < percent);
    press(&axis, Qt::Key_0, Qt::ControlModifier);
    CHECK(axis.zoomPercent() == 100);
}

TEST_CASE("Timeline list: a time it does not read stays as typed, the event waits in the tray",
          "[timeline][list][gui]") {
    OpenTimeline open;
    TimelineDocument& document = open.document;
    const QString compass = open.add(QStringLiteral("Kompas"), open.journey, APRIL_14 + 2);
    TimelineIndex index;
    index.characters.append(
        TimelineIndex::Entry{QStringLiteral("card-amara"), QStringLiteral("Amara"), QColor()});
    TimelineList list(document);
    list.setIndex(&index);

    list.applyEdit(compass, TimelineList::When, QStringLiteral("16 kwietnia 1931, wieczorem"));
    REQUIRE(open.event(compass).time.has_value());
    CHECK(open.event(compass).time->first == APRIL_14 + 2);
    CHECK(open.event(compass).time->part == StoryTime::Part::Evening);

    QString told;
    QObject::connect(&list, &TimelineList::told,
                     [&told](const QString& message, bool /*undoable*/) { told = message; });
    list.applyEdit(compass, TimelineList::When, QStringLiteral("rok później"));
    CHECK_FALSE(open.event(compass).time.has_value());
    CHECK(open.event(compass).timeText == QStringLiteral("rok później"));
    CHECK(told.contains(QStringLiteral("rok później")));
    document.undo();
    CHECK(open.event(compass).time.has_value());

    // The characters: a card by its name, else the name
    list.applyEdit(compass, TimelineList::Characters, QStringLiteral("amara, Ktoś"));
    CHECK(open.event(compass).characters ==
          QStringList{QStringLiteral("card-amara"), QStringLiteral("Ktoś")});
    CHECK(list.editText(compass, TimelineList::Characters) == QStringLiteral("Amara, Ktoś"));

    // Ctrl+Z in the list
    list.applyEdit(compass, TimelineList::Title, QStringLiteral("Kompas przy studni"));
    CHECK(open.event(compass).title == QStringLiteral("Kompas przy studni"));
    press(&list, Qt::Key_Z, Qt::ControlModifier);
    CHECK(open.event(compass).title == QStringLiteral("Kompas"));
}

TEST_CASE("Timeline book view: a chapter that goes back in time shows a flashback",
          "[timeline][book][gui]") {
    OpenTimeline open;
    TimelineDocument& document = open.document;
    const QString start = open.add(QStringLiteral("Wyjazd z Kairu"), open.journey, APRIL_14);
    const QString oasis = open.add(QStringLiteral("Oaza"), open.journey, APRIL_14 + 30);
    const QString child =
        open.add(QStringLiteral("Dzieciństwo Selima"), open.well, APRIL_14 - 3650);
    open.add(QStringLiteral("Ktoś zasypuje źródło"), open.well, APRIL_14 + 10);
    const auto tell = [&document](const QString& id, const QString& chapter) {
        document.changeEvent(id, QStringLiteral("chapters"),
                             [&chapter](TimelineEvent& event) { event.chapters = {chapter}; });
    };
    tell(start, QStringLiteral("c1"));
    tell(oasis, QStringLiteral("c2"));
    tell(child, QStringLiteral("c3"));
    TimelineIndex index;
    index.chapters = {{QStringLiteral("c1"), QStringLiteral("Rozdział 1"), QColor()},
                      {QStringLiteral("c2"), QStringLiteral("Rozdział 2"), QColor()},
                      {QStringLiteral("c3"), QStringLiteral("Rozdział 3"), QColor()}};
    TimelineBookView view(document);
    view.resize(1000, 500);
    view.setIndex(&index);

    REQUIRE(view.marks().size() == 3);
    CHECK_FALSE(view.marks().at(0).flashback);
    CHECK_FALSE(view.marks().at(1).flashback);
    CHECK(view.marks().at(2).id == child);
    CHECK(view.marks().at(2).flashback);
    CHECK(view.marks().at(0).from < view.marks().at(1).from);

    // The event the book does not tell is a chip under the drawing
    REQUIRE(view.chips().size() == 1);
}

TEST_CASE("Timeline in Polish: the list, its dates and Settings > Timelines",
          "[timeline][list][gui]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    OpenTimeline open;
    open.add(QStringLiteral("Kompas"), open.journey, APRIL_14 + 2);
    TimelineList list(open.document);
    list.setLook(TimelineLook::current());
    REQUIRE(list.rowCount() == 1);
    CHECK(list.item(0, TimelineList::When)->text() == QStringLiteral("16 kwietnia 1931"));
    CHECK(list.horizontalHeaderItem(TimelineList::Title)->text() ==
          QStringLiteral("Wydarzenie"));
    CHECK(TimelineWords::trayName() == QStringLiteral("Bez miejsca na osi"));

    const TimelineSettingsPage page;
    const auto* scale = page.findChild<QComboBox*>(QStringLiteral("timelinesNewScale"));
    REQUIRE(scale != nullptr);
    CHECK(scale->itemText(0) == QStringLiteral("daty"));
}

TEST_CASE("Timeline module: the program finds its editor, its new files and its settings",
          "[timeline][module]") {
    // A picture of the tab in Polish, with KALAHARI_TIMELINE_SHOT_PL (see below)
    std::unique_ptr<Polish> polish;
    if (!qgetenv("KALAHARI_TIMELINE_SHOT_PL").isEmpty()) {
        polish = std::make_unique<Polish>();
    }
    kalahari::gui::registerAllIcons();  // the icons of the bar, in the picture
    registerTimelineModule();
    const std::shared_ptr<kalahari::core::IWindowEditorProvider> provider =
        kalahari::core::windowEditorProvider(QStringLiteral("timeline"));
    REQUIRE(provider != nullptr);
    CHECK(provider->fileSuffix() == QStringLiteral("ktl"));
    CHECK_FALSE(provider->settingsTitle().isEmpty());

    Timeline timeline;
    REQUIRE(timeline.fromJson(QJsonDocument::fromJson(provider->newFile()).object()));
    CHECK(timeline.threads().size() == 1);
    CHECK(timeline.events().isEmpty());
    CHECK(timeline.dayOne() > 0);
    CHECK(timeline.view() == TimelineLook::newView());

    const std::unique_ptr<QWidget> page(provider->createSettingsPage());
    CHECK(qobject_cast<kalahari::gui::SettingsPage*>(page.get()) != nullptr);

    // A timeline added to the Workshop gets its file from the module, and opens on its tab
    const QTemporaryDir dir;
    auto& book = kalahari::test::book();
    kalahari::core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(book.createProject(dir.path(), "Times", "Anna", "pl", true, "kalahari.novel"));
    const kalahari::core::KindRef kind = book.bookTypes().findKind(
        QStringLiteral("kalahari.base"), QStringLiteral("timeline"));
    REQUIRE(kind);
    CHECK(kalahari::core::ProjectManager::canAdd(kind));
    const QString id = book.addElement(kind, "Czas akcji", kalahari::core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const kalahari::core::ProjectElement* element = book.findElement(id);
    REQUIRE(element != nullptr);
    CHECK(element->file.endsWith(QStringLiteral(".ktl")));
    Timeline added;
    REQUIRE(added.load(book.filePathOf(*element)));

    const kalahari::core::WindowEditorContext context{book, kalahari::test::commands(), id, {},
                                                      {}};
    const std::unique_ptr<QWidget> tab(provider->createEditor(context, nullptr));
    auto* editor = qobject_cast<TimelineEditor*>(tab.get());
    REQUIRE(editor != nullptr);
    CHECK(editor->document().timeline().threads().size() == 1);
    CHECK(editor->axis() != nullptr);
    CHECK(editor->list() != nullptr);
    CHECK(editor->bookView() != nullptr);
    if (const QByteArray shot = qgetenv("KALAHARI_TIMELINE_SHOT"); !shot.isEmpty()) {
        // A picture of the tab, to look at it (not checked)

        TimelineDocument& document = editor->document();
        const QString journey = document.timeline().threads().constFirst().id;
        QString well;
        document.change(QStringLiteral("shot"), [&](Timeline& changed) {
            changed.setDayOne(APRIL_14);
            well = changed.addThread(QStringLiteral("Studnia"), QStringLiteral("2"));
            changed.addEvent(eventOf(QStringLiteral("Amara znajduje mapę"), journey, APRIL_14));
            changed.addEvent(eventOf(QStringLiteral("Wyjazd z Kairu"), journey, APRIL_14 + 2));
            changed.addEvent(eventOf(QStringLiteral("Burza piaskowa"), journey, APRIL_14 + 5));
            changed.addEvent(eventOf(QStringLiteral("Kompas przy studni"), well, APRIL_14 + 3));
            changed.addEvent(
                eventOf(QStringLiteral("Ktoś zasypuje źródło"), well, APRIL_14 + 300));
            changed.addEvent(eventOf(QStringLiteral("Selim rysuje mapę"), QString(), {}));
            return true;
        });
        editor->resize(1100, 600);
        editor->show();
        QApplication::processEvents();
        const QString path = QString::fromLocal8Bit(shot);
        editor->grab().save(path);
        // The other views, beside it: "-1.png", "-2.png"
        int view = 0;
        for (QToolButton* button : editor->findChildren<QToolButton*>()) {
            if (const QString text = button->text();
                text == QStringLiteral("List") || text == QStringLiteral("Lista") ||
                text == QStringLiteral("Time and the Book") ||
                text == QStringLiteral("Czas i książka")) {
                button->click();
                for (int pass = 0; pass < 3; ++pass) {
                    QApplication::processEvents();  // The bar wraps again after a few passes
                }
                editor->grab().save(QString(path).replace(QStringLiteral(".png"),
                                                          QStringLiteral("-%1.png").arg(++view)));
            }
        }
    }
    REQUIRE(book.closeProject(false));
    kalahari::core::ExtensionPointRegistry::getInstance().unregisterPlugin("kalahari.timelines");
    CHECK(kalahari::core::windowEditorProvider(QStringLiteral("timeline")) == nullptr);
}

TEST_CASE("Timeline editor: the commands of its bar add, move, delete and undo events",
          "[timeline][editor][gui]") {
    kalahari::gui::registerAllIcons();
    registerTimelineModule();
    const std::shared_ptr<kalahari::core::IWindowEditorProvider> provider =
        kalahari::core::windowEditorProvider(QStringLiteral("timeline"));
    REQUIRE(provider != nullptr);
    const QTemporaryDir dir;
    auto& book = kalahari::test::book();
    kalahari::core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(book.createProject(dir.path(), "Times", "Anna", "pl", true, "kalahari.novel"));
    const kalahari::core::KindRef kind = book.bookTypes().findKind(
        QStringLiteral("kalahari.base"), QStringLiteral("timeline"));
    REQUIRE(kind);
    const QString id = book.addElement(kind, "Czas akcji", kalahari::core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const kalahari::core::WindowEditorContext context{book, kalahari::test::commands(), id, {},
                                                      {}};
    std::unique_ptr<QWidget> tab(provider->createEditor(context, nullptr));
    auto* editor = qobject_cast<TimelineEditor*>(tab.get());
    REQUIRE(editor != nullptr);
    editor->resize(1100, 600);
    editor->show();
    QApplication::processEvents();
    TimelineDocument& document = editor->document();
    using Command = TimelineEditor::Command;
    const auto command = [editor](Command which) { return editor->command(which); };
    const auto button = [editor](const char* name) {
        return editor->findChild<QToolButton*>(QString::fromLatin1(name));
    };

    // Without events: the axis says how to add one; only New Event can be given
    REQUIRE(document.timeline().events().isEmpty());
    CHECK_FALSE(TimelineAxisView::emptyHint().isEmpty());
    CHECK(command(Command::New)->isEnabled());
    for (const Command which : {Command::Edit, Command::Chapter, Command::Earlier, Command::Later,
                                Command::Delete, Command::Undo, Command::Redo}) {
        CHECK_FALSE(command(which)->isEnabled());
    }
    // The buttons: the main ones with their names, all with their keys in the tooltip
    REQUIRE(button("timelineNewEvent") != nullptr);
    CHECK(button("timelineNewEvent")->toolButtonStyle() == Qt::ToolButtonTextBesideIcon);
    CHECK(button("timelineDeleteEvent")->toolButtonStyle() == Qt::ToolButtonTextBesideIcon);
    CHECK(button("timelineUndo")->toolButtonStyle() == Qt::ToolButtonIconOnly);
    CHECK(command(Command::New)->toolTip() ==
          kalahari::gui::CardBar::withKeys(command(Command::New)->text(),
                                           QKeyCombination(Qt::Key_Insert)));
    CHECK(command(Command::Delete)->toolTip().endsWith(QLatin1Char(')')));
    CHECK_FALSE(command(Command::New)->icon().isNull());
    CHECK(button("timelineEarlier")->isVisible());

    // New Event: on the axis, its title typed in place
    command(Command::New)->trigger();
    REQUIRE(editor->axis()->isEditing());
    auto* title = editor->axis()->viewport()->findChild<QLineEdit*>();
    REQUIRE(title != nullptr);
    title->setText(QStringLiteral("Burza piaskowa"));
    press(title, Qt::Key_Return);
    REQUIRE(document.timeline().events().size() == 1);
    const QString storm = document.selected();
    REQUIRE(document.timeline().event(storm) != nullptr);
    CHECK(document.timeline().event(storm)->title == QStringLiteral("Burza piaskowa"));

    // The event selected: the commands that change it can be given
    for (const Command which : {Command::Edit, Command::Delete, Command::Undo}) {
        CHECK(command(which)->isEnabled());
    }
    CHECK_FALSE(command(Command::Chapter)->isEnabled());  // in no chapter yet

    // Without a time it waits in the tray: no steps in time
    CHECK_FALSE(document.timeline().event(storm)->time.has_value());
    CHECK_FALSE(command(Command::Later)->isEnabled());

    // Later: a step later on the axis; Undo and Redo
    document.changeEvent(storm, QStringLiteral("time"),
                         [](TimelineEvent& event) { event.time = StoryTime::day(APRIL_14); });
    document.select(storm);
    REQUIRE(command(Command::Later)->isEnabled());
    command(Command::Later)->trigger();
    CHECK(document.timeline().event(storm)->time->first == APRIL_14 + 1);
    command(Command::Undo)->trigger();
    CHECK(document.timeline().event(storm)->time->first == APRIL_14);
    CHECK(command(Command::Redo)->isEnabled());
    command(Command::Redo)->trigger();
    CHECK(document.timeline().event(storm)->time->first == APRIL_14 + 1);
    CHECK(command(Command::LaneUp)->isEnabled());

    // Delete the Event, and Undo brings it back
    document.select(storm);
    command(Command::Delete)->trigger();
    CHECK(document.timeline().event(storm) == nullptr);
    command(Command::Undo)->trigger();
    CHECK(document.timeline().event(storm) != nullptr);

    // The list: no steps of the axis; New Event adds a row
    QToolButton* listButton = nullptr;
    QToolButton* bookButton = nullptr;
    for (QToolButton* each : editor->findChildren<QToolButton*>()) {
        if (each->text() == QStringLiteral("List")) {
            listButton = each;
        } else if (each->text() == QStringLiteral("Time and the Book")) {
            bookButton = each;
        }
    }
    REQUIRE(listButton != nullptr);
    REQUIRE(bookButton != nullptr);
    listButton->click();
    QApplication::processEvents();
    CHECK_FALSE(button("timelineEarlier")->isVisible());
    CHECK_FALSE(TimelineList::emptyHint().isEmpty());
    command(Command::New)->trigger();
    QApplication::processEvents();
    CHECK(document.timeline().events().size() == 2);
    command(Command::Undo)->trigger();
    QApplication::processEvents();
    CHECK(document.timeline().events().size() == 1);

    // The time and the book: a new event in the window "Event", gone when it is cancelled
    bookButton->click();
    QApplication::processEvents();
    bool asked = false;
    QTimer::singleShot(0, [&asked]() {
        if (auto* window = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            asked = true;
            window->reject();
        }
    });
    press(editor->bookView(), Qt::Key_Insert);
    CHECK(asked);
    CHECK(document.timeline().events().size() == 1);
    // Delete there deletes the event selected
    document.select(storm);
    press(editor->bookView(), Qt::Key_Delete);
    CHECK(document.timeline().event(storm) == nullptr);
    CHECK_FALSE(command(Command::Delete)->isEnabled());

    tab.reset();
    REQUIRE(book.closeProject(false));
    kalahari::core::ExtensionPointRegistry::getInstance().unregisterPlugin("kalahari.timelines");
}
