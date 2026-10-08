/// @file test_annotations_panel.cpp
/// @brief The Annotations panel and the commands of the annotations

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/gui/panels/annotation_entry.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimeZone>
#include <QVBoxLayout>

#include <memory>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;
using editor::AnnotationKind;

namespace {

/// An entry of an annotation in a chapter
AnnotationEntry entryOf(const QString& id, AnnotationKind kind, const QString& text,
                        int chapterOrder = 0, int textOrder = 0) {
    AnnotationEntry entry;
    entry.annotation.id = id;
    entry.annotation.kind = kind;
    entry.annotation.text = text;
    entry.elementId = QStringLiteral("ch-%1").arg(chapterOrder);
    entry.chapterTitle = QStringLiteral("Chapter %1").arg(chapterOrder);
    entry.chapterOrder = chapterOrder;
    entry.textOrder = textOrder;
    return entry;
}

/// The ids of entries, in their order
QString idsOf(const std::vector<AnnotationEntry>& entries) {
    QStringList ids;
    for (const AnnotationEntry& entry : entries) {
        ids << entry.annotation.id;
    }
    return ids.join(QLatin1Char(' '));
}

/// An <annotation> element of a chapter's KML
QString record(const QString& id, const QString& kind, const QString& text) {
    return QStringLiteral("<annotation id=\"%1\" kind=\"%2\">%3</annotation>").arg(id, kind, text);
}

/// A chapter's KML with these annotations and paragraphs
QString kmlWith(const QString& annotations, const QStringList& paragraphs) {
    QString kml = QStringLiteral("<kml><annotations>") + annotations + QStringLiteral("</annotations>");
    for (const QString& paragraph : paragraphs) {
        kml += QStringLiteral("<p>") + paragraph + QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

/// A key press sent to a widget
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

/// A document outside the book in a tab, the Annotations panel and their coordinator
struct Desk {
    QTabWidget tabs;
    EditorPanel* document;
    AnnotationsPanel panel;
    std::unique_ptr<AnnotationsCoordinator> coordinator;

    explicit Desk(const QString& kml)
        : document(new EditorPanel()) {
        document->setContent(kml);
        tabs.addTab(document, QStringLiteral("Notes"));
        panel.show();  // the panel lists the annotations while it can be seen
        coordinator =
            std::make_unique<AnnotationsCoordinator>(&panel, nullptr, &tabs, nullptr, nullptr);
    }

    editor::BookEditor& editor() { return *document->getBookEditor(); }

    /// The annotation with this id
    editor::Annotation annotation(const QString& id) {
        for (const editor::AnnotationPlace& place : editor().annotations()) {
            if (place.annotation.id == id) {
                return place.annotation;
            }
        }
        return {};
    }

    /// The listed entry of the annotation with this id
    AnnotationEntry entry(const QString& id) {
        for (const AnnotationEntry& listed : panel.entries()) {
            if (listed.annotation.id == id) {
                return listed;
            }
        }
        return {};
    }
};

}  // namespace

// =============================================================================
// Filters and orders
// =============================================================================

TEST_CASE("Annotations panel: an annotation passes the filters", "[gui][annotations]") {
    const QDateTime now(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
    AnnotationEntry todo = entryOf(QStringLiteral("t"), AnnotationKind::Todo,
                                   QStringLiteral("Check the dates"));
    todo.annotation.created = now.addDays(-3);
    AnnotationFilter filter;

    SECTION("by state: the open ones at first") {
        CHECK(matches(todo, filter, now));
        todo.annotation.done = true;
        CHECK_FALSE(matches(todo, filter, now));
        filter.state = AnnotationStateFilter::Done;
        CHECK(matches(todo, filter, now));
        filter.state = AnnotationStateFilter::All;
        CHECK(matches(todo, filter, now));
    }

    SECTION("by kind; a kind not shown is still counted") {
        filter.todos = false;
        CHECK_FALSE(matches(todo, filter, now));
        CHECK(matchesExceptKind(todo, filter, now));
    }

    SECTION("by text, in any case") {
        filter.text = QStringLiteral("  the DATES ");
        CHECK(matches(todo, filter, now));
        filter.text = QStringLiteral("names");
        CHECK_FALSE(matches(todo, filter, now));
    }

    SECTION("by date") {
        filter.date = AnnotationDateFilter::Today;
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Last7Days;
        CHECK(matches(todo, filter, now));
        todo.annotation.created = now.addDays(-8);
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Last30Days;
        CHECK(matches(todo, filter, now));

        // Made at an unknown time: only for any date
        todo.annotation.created = QDateTime();
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Any;
        CHECK(matches(todo, filter, now));
    }
}

TEST_CASE("Annotations panel: the orders of the annotations", "[gui][annotations]") {
    const QDateTime now(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
    std::vector<AnnotationEntry> entries = {
        entryOf(QStringLiteral("b2"), AnnotationKind::Note, QString(), 1, 1),
        entryOf(QStringLiteral("a1"), AnnotationKind::Comment, QString(), 0, 0),
        entryOf(QStringLiteral("b1"), AnnotationKind::Todo, QString(), 1, 0),
        entryOf(QStringLiteral("a2"), AnnotationKind::Note, QString(), 0, 1)};
    entries[0].annotation.created = now.addDays(-1);  // b2
    entries[1].annotation.created = now.addDays(-2);  // a1
    entries[3].annotation.created = now;              // a2; b1 made at an unknown time

    SECTION("as in the book: chapter by chapter, in the order of the text") {
        sortEntries(entries, AnnotationSort::TextOrder);
        CHECK(idsOf(entries) == QStringLiteral("a1 a2 b1 b2"));
    }

    SECTION("the newest first, those made at an unknown time last") {
        sortEntries(entries, AnnotationSort::Newest);
        CHECK(idsOf(entries) == QStringLiteral("a2 b2 a1 b1"));
    }
}

// =============================================================================
// The panel
// =============================================================================

TEST_CASE("Annotations panel: cards of the annotations that pass the filters",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    AnnotationEntry comment =
        entryOf(QStringLiteral("c"), AnnotationKind::Comment, QStringLiteral("Too long"), 0, 0);
    AnnotationEntry done =
        entryOf(QStringLiteral("t"), AnnotationKind::Todo, QStringLiteral("Check"), 0, 1);
    done.annotation.done = true;
    const AnnotationEntry note =
        entryOf(QStringLiteral("n"), AnnotationKind::Note, QStringLiteral("Idea"), 0, 2);
    panel.setEntries({comment, done, note});

    // The open ones at first
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c n"));
    CHECK(panel.card(done.key()) == nullptr);

    AnnotationFilter filter = panel.filter();
    filter.state = AnnotationStateFilter::All;
    panel.setFilter(filter);
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c t n"));

    // A card stays for its annotation listed again, and shows it as it is now
    AnnotationCard* card = panel.card(comment.key());
    REQUIRE(card != nullptr);
    comment.annotation.text = QStringLiteral("Shorter");
    panel.setEntries({comment, done, note});
    CHECK(panel.card(comment.key()) == card);
    CHECK(card->text() == QStringLiteral("Shorter"));

    // Gone from the list, gone from the panel
    panel.setEntries({comment});
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c"));
    CHECK(panel.card(note.key()) == nullptr);
}

TEST_CASE("Annotations panel: editing an annotation the filters hide shows it",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    AnnotationEntry done =
        entryOf(QStringLiteral("t"), AnnotationKind::Todo, QStringLiteral("Check"), 0, 0);
    done.annotation.done = true;
    panel.setEntries({done});
    AnnotationFilter filter = panel.filter();
    filter.text = QStringLiteral("names");
    filter.todos = false;
    panel.setFilter(filter);
    REQUIRE(panel.shownEntries().empty());

    panel.editAnnotation(done.key());
    CHECK(panel.filter().text.isEmpty());
    CHECK(panel.filter().todos);
    CHECK(panel.filter().state == AnnotationStateFilter::All);
    CHECK(panel.card(done.key()) != nullptr);
    CHECK(panel.selectedKey() == done.key());
}

TEST_CASE("Annotations panel: keys go through the cards and delete the selected one",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    std::vector<AnnotationEntry> entries = {
        entryOf(QStringLiteral("a"), AnnotationKind::Comment, QStringLiteral("One"), 0, 0),
        entryOf(QStringLiteral("b"), AnnotationKind::Todo, QStringLiteral("Two"), 0, 1),
        entryOf(QStringLiteral("c"), AnnotationKind::Note, QStringLiteral("Three"), 0, 2)};
    panel.setEntries(entries);

    QStringList activated;
    QObject::connect(&panel, &AnnotationsPanel::annotationActivated,
                     [&activated](const AnnotationEntry& entry) { activated << entry.annotation.id; });
    // As the coordinator does: the annotation goes, and the panel lists the others
    QObject::connect(&panel, &AnnotationsPanel::deleteRequested,
                     [&panel, &entries](const AnnotationEntry& entry) {
                         std::erase_if(entries, [&entry](const AnnotationEntry& e) {
                             return e.key() == entry.key();
                         });
                         panel.setEntries(entries);
                     });

    press(&panel, Qt::Key_Down);
    press(&panel, Qt::Key_Down);
    press(&panel, Qt::Key_Up);
    CHECK(activated.join(QLatin1Char(' ')) == QStringLiteral("a b a"));
    CHECK(panel.selectedKey() == entries[0].key());

    // The card after the deleted one is selected
    press(&panel, Qt::Key_Delete);
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("b c"));
    CHECK(panel.selectedKey() == entries[0].key());

    // ... or the one before the last
    press(&panel, Qt::Key_Down);
    press(&panel, Qt::Key_Delete);
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("b"));
    CHECK(panel.selectedKey() == entries[0].key());
}

TEST_CASE("Annotations panel: a card is edited on purpose, not when the focus passes by",
          "[gui][annotations]") {
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    // The panel, then a field, then a card: in the focus chain the card comes right after
    // the field, as the cards listed after an editor was opened come after the editor
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* panel = new AnnotationsPanel(&window);
    auto* field = new QLineEdit(&window);
    layout->addWidget(field);
    layout->addWidget(panel);
    const AnnotationEntry entry =
        entryOf(QStringLiteral("a"), AnnotationKind::Comment, QStringLiteral("One"));
    panel->setEntries({entry});
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    field->setFocus();
    REQUIRE(field->hasFocus());
    int started = 0;
    QObject::connect(panel, &AnnotationsPanel::editingStarted,
                     [&started](const AnnotationEntry&) { ++started; });

    // As when another tab comes to the front: the focus goes on from the hidden field
    field->hide();
    CHECK(started == 0);
    CHECK_FALSE(panel->card(entry.key())->isEditing());

    // Asked for, the editing starts
    panel->editAnnotation(entry.key());
    CHECK(started == 1);
    CHECK(panel->card(entry.key())->isEditing());
}

// =============================================================================
// The commands and the panel with a document
// =============================================================================

TEST_CASE("Annotations: a new annotation gets its text in its card, in the step that added it",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two three")}));
    desk.editor().setSelection({{0, 4}, {0, 7}});

    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Todo));
    REQUIRE(desk.editor().annotations().size() == 1);
    const editor::AnnotationPlace place = desk.editor().annotations().front();
    CHECK(place.start == 4);
    CHECK(place.end == 7);
    CHECK(place.annotation.kind == AnnotationKind::Todo);
    CHECK(place.annotation.author == AnnotationsCoordinator::author());
    CHECK(desk.coordinator->isEditing());

    // Listed and selected in the panel
    const AnnotationEntry listed = desk.entry(place.annotation.id);
    REQUIRE(desk.panel.card(listed.key()) != nullptr);
    CHECK(desk.panel.selectedKey() == listed.key());

    // Typed in the card, a while apart
    emit desk.panel.textEdited(listed, QStringLiteral("Ch"));
    test::runEventLoop(400);
    CHECK(desk.annotation(listed.annotation.id).text == QStringLiteral("Ch"));
    emit desk.panel.textEdited(listed, QStringLiteral("Check it"));
    emit desk.panel.editingFinished(listed);
    CHECK_FALSE(desk.coordinator->isEditing());
    CHECK(desk.annotation(listed.annotation.id).text == QStringLiteral("Check it"));

    // One undo step takes it away with its text, and brings it back
    desk.editor().undo();
    CHECK(desk.editor().annotations().empty());
    desk.editor().redo();
    CHECK(desk.annotation(listed.annotation.id).text == QStringLiteral("Check it"));
}

TEST_CASE("Annotations: a new annotation left without text goes away without a trace",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two three")}));
    desk.editor().setCursorPosition({0, 3});
    desk.editor().insertText(QStringLiteral(","));
    REQUIRE(desk.editor().canUndo());

    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Comment));
    const AnnotationEntry listed = desk.panel.entries().front();
    emit desk.panel.textEdited(listed, QStringLiteral("  "));
    emit desk.panel.editingFinished(listed);

    CHECK(desk.editor().annotations().empty());
    CHECK_FALSE(desk.editor().canRedo());

    // The typing before it is the step to undo
    desk.editor().undo();
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two three"));
}

TEST_CASE("Annotations: the text of an annotation changes in a step of its own",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("Old")),
                      {QStringLiteral("<anchor ref=\"c1\">One</anchor> two")}));
    REQUIRE(desk.panel.entries().size() == 1);
    const AnnotationEntry listed = desk.entry(QStringLiteral("c1"));

    // The writer typed in the text first
    desk.editor().setCursorPosition({0, 7});
    desk.editor().insertText(QStringLiteral(" three"));

    emit desk.panel.editingStarted(listed);
    emit desk.panel.textEdited(listed, QStringLiteral("New"));
    test::runEventLoop(400);
    emit desk.panel.textEdited(listed, QStringLiteral("Newer"));
    emit desk.panel.editingFinished(listed);
    CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Newer"));

    desk.editor().undo();
    CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Old"));
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two three"));
    desk.editor().undo();
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two"));
}

TEST_CASE("Annotations: done and deleted in the panel, both undone",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("Fix")) +
                          record(QStringLiteral("n1"), QStringLiteral("note"), QStringLiteral("Idea")),
                      {QStringLiteral("<anchor ref=\"t1\">One</anchor> two<anchor ref=\"n1\"/>")}));
    REQUIRE(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1 n1"));

    SECTION("a TODO done leaves the open ones") {
        const AnnotationEntry todo = desk.entry(QStringLiteral("t1"));
        emit desk.panel.doneToggled(todo, true);
        CHECK(desk.annotation(QStringLiteral("t1")).done);
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("n1"));

        desk.editor().undo();
        CHECK_FALSE(desk.annotation(QStringLiteral("t1")).done);
        desk.coordinator->refresh();
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1 n1"));
    }

    SECTION("a deleted note leaves the text") {
        const AnnotationEntry note = desk.entry(QStringLiteral("n1"));
        emit desk.panel.deleteRequested(note);
        CHECK(desk.annotation(QStringLiteral("n1")).id.isEmpty());
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1"));

        desk.editor().undo();
        CHECK(desk.annotation(QStringLiteral("n1")).text == QStringLiteral("Idea"));
    }
}

TEST_CASE("Annotations: the next and previous TODO, with their cards selected",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("A")) +
                          record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("B")) +
                          record(QStringLiteral("t2"), QStringLiteral("todo"), QStringLiteral("C")),
                      {QStringLiteral("One <anchor ref=\"t1\">two</anchor> three<anchor ref=\"c1\"/> "
                                      "<anchor ref=\"t2\">four</anchor>")}));
    desk.editor().setCursorPosition({0, 0});

    REQUIRE(desk.coordinator->goToNextTodo());
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t1")).key());
    REQUIRE(desk.coordinator->goToNextTodo());  // past the comment
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t2")).key());
    // Its fragment selected ("four")
    CHECK(desk.editor().selection().normalized().start == editor::CursorPosition{0, 14});
    CHECK(desk.editor().selection().normalized().end == editor::CursorPosition{0, 18});
    CHECK_FALSE(desk.coordinator->goToNextTodo());
    REQUIRE(desk.coordinator->goToPreviousTodo());
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t1")).key());
}

TEST_CASE("Annotations: the tab of a chapter behind comes to the front for its annotation",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two")}));
    auto* chapter = new EditorPanel();
    chapter->setContent(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("Fix")),
                                {QStringLiteral("Three <anchor ref=\"t1\">four</anchor>")}));
    chapter->setProperty("elementId", QStringLiteral("ch-2"));
    desk.tabs.addTab(chapter, QStringLiteral("Chapter Two"));
    REQUIRE(desk.tabs.currentIndex() == 0);
    editor::BookEditor& chapterEditor = *chapter->getBookEditor();

    // Its entry as the list of the whole book has it
    AnnotationEntry todo;
    todo.annotation = chapterEditor.annotations().front().annotation;
    todo.elementId = QStringLiteral("ch-2");

    SECTION("gone to: its fragment is selected in front") {
        emit desk.panel.annotationActivated(todo);
        CHECK(desk.tabs.currentWidget() == chapter);
        CHECK(chapterEditor.selection().normalized().start == editor::CursorPosition{0, 6});
        CHECK(chapterEditor.selection().normalized().end == editor::CursorPosition{0, 10});
    }

    SECTION("changed: in front, where Ctrl+Z undoes the change") {
        emit desk.panel.doneToggled(todo, true);
        CHECK(desk.tabs.currentWidget() == chapter);
        CHECK(chapterEditor.annotations().front().annotation.done);
        CHECK(chapterEditor.canUndo());
    }
}

TEST_CASE("Annotations: the commands work on the document in front", "[gui][annotations]") {
    registerAllCommands(CommandCallbacks{});
    Desk desk(test::kmlOf({QStringLiteral("One two")}));
    desk.coordinator->connectCommands();
    auto& registry = CommandRegistry::getInstance();
    QAction* addNote = registry.getAction(QStringLiteral("insert.note"));
    QAction* nextTodo = registry.getAction(QStringLiteral("edit.nextTodo"));
    REQUIRE(addNote != nullptr);
    REQUIRE(nextTodo != nullptr);
    CHECK(addNote->isEnabled());
    CHECK(nextTodo->isEnabled());

    addNote->trigger();
    REQUIRE(desk.editor().annotations().size() == 1);
    CHECK(desk.editor().annotations().front().annotation.kind == AnnotationKind::Note);

    // Not while writing without distraction: the panel is hidden then
    desk.coordinator->setAddingAvailable(false);
    CHECK_FALSE(addNote->isEnabled());
    CHECK(nextTodo->isEnabled());
    desk.coordinator->setAddingAvailable(true);

    // Nothing to work on without a document in front
    desk.tabs.addTab(new QWidget(), QStringLiteral("Dashboard"));
    desk.tabs.setCurrentIndex(1);
    CHECK_FALSE(addNote->isEnabled());
    CHECK_FALSE(nextTodo->isEnabled());
}

TEST_CASE("Annotations: who the new annotations are by", "[gui][annotations]") {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("annotations.author", "  Anna Nowak ");
    CHECK(AnnotationsCoordinator::author() == QStringLiteral("Anna Nowak"));

    // Without the setting and without a book: the computer's user
    settings.set<std::string>("annotations.author", "");
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty()) {
        user = qEnvironmentVariable("USER");
    }
    CHECK(AnnotationsCoordinator::author() == user.trimmed());
}
