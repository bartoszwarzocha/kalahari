// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_footnotes_panel.cpp
/// @brief The Footnotes panel: its cards, kinds, search, scope, keys and menu, and what it
///        lists and does in the texts of a book

#include <catch2/catch_test_macros.hpp>

#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/footnotes_panel_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/footnotes_panel.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QStandardItemModel>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace kalahari;
using editor::NoteKind;
using gui::FootnoteEntry;
using gui::FootnoteKinds;
using gui::FootnoteScope;
using gui::FootnotesPanel;
using test::OpenNovel;
using test::Window;

namespace {

/// A note of a text, as the panel is given it
FootnoteEntry entryOf(const QString& elementId, const QString& title, int order,
                      const QString& id, NoteKind kind, const QString& number,
                      const QString& text, const QString& before = QString()) {
    FootnoteEntry entry;
    entry.elementId = elementId;
    entry.textTitle = title;
    entry.textOrder = order;
    entry.id = id;
    entry.kind = kind;
    entry.number = number;
    entry.text = text;
    entry.before = before;
    return entry;
}

/// Two footnotes and an endnote of one text, a footnote of the next one
std::vector<FootnoteEntry> someNotes() {
    return {entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0, QStringLiteral("s1"),
                    NoteKind::Footnote, QStringLiteral("1"), QStringLiteral("A name of the guide"),
                    QStringLiteral("called Hilary")),
            entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0, QStringLiteral("s2"),
                    NoteKind::Footnote, QStringLiteral("2"), QStringLiteral("Nine camels")),
            entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0, QStringLiteral("se"),
                    NoteKind::Endnote, QStringLiteral("i"), QStringLiteral("And a donkey")),
            entryOf(QStringLiteral("calm"), QStringLiteral("The Calm"), 1, QStringLiteral("c1"),
                    NoteKind::Footnote, QStringLiteral("3"), QStringLiteral("Still water"))};
}

/// The ids of the notes the panel shows, parted by " "
std::string shownIds(const FootnotesPanel& panel) {
    QStringList ids;
    for (const FootnoteEntry& entry : panel.shownEntries()) {
        ids << entry.id;
    }
    return ids.join(QLatin1Char(' ')).toStdString();
}

/// The notes a signal of the panel gave, in their order
struct Caught {
    std::vector<FootnoteEntry> entries;

    [[nodiscard]] int count() const { return static_cast<int>(entries.size()); }
    [[nodiscard]] const FootnoteEntry& last() const { return entries.back(); }
};

/// Catch the notes a signal of the panel gives
template <typename Signal>
void catchIn(FootnotesPanel& panel, Signal signal, Caught& caught) {
    QObject::connect(&panel, signal,
                     [&caught](const FootnoteEntry& entry) { caught.entries.push_back(entry); });
}

/// A key pressed and released in a widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// The texts of a menu's items, separators as "|"
QStringList textsOf(const QMenu& menu) {
    QStringList texts;
    for (const QAction* action : menu.actions()) {
        texts << (action->isSeparator() ? QStringLiteral("|") : action->text());
    }
    return texts;
}

/// The texts of a book with footnotes and endnotes, saved
struct NotedBook {
    OpenNovel book{QStringLiteral("Notes")};
    QString storm;
    QString calm;
    QString after;

    NotedBook() {
        storm = book.chapterId;
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        after = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                   QStringLiteral("The Sun"), core::BookPlace::Main);
        REQUIRE_FALSE(calm.isEmpty());
        REQUIRE_FALSE(after.isEmpty());
        save(storm, QStringLiteral("<kml><p>Rain<footnote id=\"s1\">Wet</footnote> fell"
                                   "<footnote id=\"s2\">Hard</footnote>.</p>"
                                   "<p>Wind<endnote id=\"se\">Storm end</endnote>.</p></kml>"));
        save(calm, QStringLiteral("<kml><p>Calm<footnote id=\"c1\">Still</footnote> came"
                                  "<endnote id=\"ce\">Calm end</endnote>.</p></kml>"));
        save(after, QStringLiteral("<kml><p>Sun<footnote id=\"a1\">Hot</footnote>.</p></kml>"));
    }

    void save(const QString& id, const QString& kml) {
        book.pm.setChapterContent(id, kml);
        REQUIRE(book.pm.saveChapterContent(id));
    }
};

/// The notes and the panel over the tabs of a window, as the main window has them
struct Notes {
    gui::FootnoteCoordinator footnotes;
    FootnotesPanel panel{kalahari::test::commands()};
    gui::FootnotesPanelCoordinator coordinator;

    explicit Notes(Window& window)
        : footnotes(
              kalahari::test::book(), kalahari::test::commands(), window.tabs,
              [&window]() -> editor::BookEditor* {
                  if (window.tabs->count() == 0) {
                      return nullptr;
                  }
                  gui::EditorPanel* text = window.editor(window.tabs->currentIndex());
                  return text != nullptr ? text->getBookEditor() : nullptr;
              },
              [&window](const QString& id) { window.navigator.onElementSelected(id, QString()); },
              &window.window)
        , coordinator(kalahari::test::book(), footnotes, &panel, nullptr, window.tabs,
                      [&window](const QString& id) {
                          window.navigator.onElementSelected(id, QString());
                      }) {
        footnotes.connectCommands();
    }

    /// The ids of the notes listed, parted by " "
    [[nodiscard]] std::string ids() const {
        QStringList ids;
        for (const FootnoteEntry& entry : coordinator.entries()) {
            ids << entry.id;
        }
        return ids.join(QLatin1Char(' ')).toStdString();
    }

    /// A note listed, by its id
    [[nodiscard]] FootnoteEntry entry(const QString& id) const {
        for (const FootnoteEntry& each : coordinator.entries()) {
            if (each.id == id) {
                return each;
            }
        }
        FAIL("no note " << id.toStdString());
        return {};
    }
};

/// The editor of a text opened in the window
editor::BookEditor& open(Window& window, const QString& id) {
    window.navigator.onElementSelected(id, QString());
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(text != nullptr);
    return *text;
}

/// The kinds of the notes of an editor, "f" or "e", in text order
std::string kindsIn(const editor::BookEditor& text) {
    std::string kinds;
    for (const editor::NotePlace& place : editor::notesIn(*text.textDocument())) {
        kinds += place.note.kind == NoteKind::Endnote ? 'e' : 'f';
    }
    return kinds;
}

}  // namespace

TEST_CASE("Footnotes panel: the kinds with their counts and the search narrow the cards",
          "[gui][footnotes][panel]") {
    FootnotesPanel panel(kalahari::test::commands());
    panel.setDocumentState(true, true);
    panel.setEntries(someNotes());
    CHECK(shownIds(panel) == "s1 s2 se c1");
    CHECK(panel.kindButton(FootnoteKinds::All)->text() == QStringLiteral("All (4)"));
    CHECK(panel.kindButton(FootnoteKinds::Footnotes)->text() == QStringLiteral("Footnotes (3)"));
    CHECK(panel.kindButton(FootnoteKinds::Endnotes)->text() == QStringLiteral("Endnotes (1)"));
    CHECK(panel.emptyLabel()->isHidden());

    // The kinds
    panel.kindButton(FootnoteKinds::Endnotes)->click();
    CHECK(panel.kinds() == FootnoteKinds::Endnotes);
    CHECK(shownIds(panel) == "se");
    panel.setKinds(FootnoteKinds::Footnotes);
    CHECK(shownIds(panel) == "s1 s2 c1");
    panel.setKinds(FootnoteKinds::All);

    // The search, in the texts of the notes, case aside; the counts follow it
    panel.setSearchText(QStringLiteral("CAMEL"));
    CHECK(shownIds(panel) == "s2");
    CHECK(panel.kindButton(FootnoteKinds::All)->text() == QStringLiteral("All (1)"));
    CHECK(panel.kindButton(FootnoteKinds::Endnotes)->text() == QStringLiteral("Endnotes (0)"));
    panel.setSearchText(QStringLiteral("zebra"));
    CHECK(panel.shownEntries().empty());
    CHECK_FALSE(panel.emptyLabel()->isHidden());
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No note contains “zebra”."));
    panel.setSearchText(QString());
    panel.setKinds(FootnoteKinds::Endnotes);
    panel.setEntries({someNotes().at(0)});
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No notes of this kind."));
}

TEST_CASE("Footnotes panel: what it says without notes", "[gui][footnotes][panel]") {
    FootnotesPanel panel(kalahari::test::commands());
    panel.setDocumentState(false, true);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No text is open."));
    panel.setBookScopeAvailable(true);
    panel.setDocumentState(false, true);
    CHECK(panel.emptyLabel()->text().startsWith(QStringLiteral("No text is open. Open a text, or "
                                                               "choose Book")));
    panel.setDocumentState(true, false);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("A plain text file keeps no notes."));
    panel.setDocumentState(true, true);
    CHECK(panel.emptyLabel()->text().startsWith(QStringLiteral("This text has no notes yet")));
    panel.setScope(FootnoteScope::Book);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("The book has no notes yet."));
}

TEST_CASE("Footnotes panel: the scope Book lists the notes under the titles of their texts",
          "[gui][footnotes][panel]") {
    FootnotesPanel panel(kalahari::test::commands());
    int scopes = 0;
    QObject::connect(&panel, &FootnotesPanel::scopeChanged, [&scopes]() { ++scopes; });
    panel.setEntries(someNotes());
    // Without a book only the text in front
    CHECK_FALSE(panel.scopeButton(FootnoteScope::Book)->isEnabled());
    panel.setScope(FootnoteScope::Book);
    CHECK(panel.scope() == FootnoteScope::Text);

    panel.setBookScopeAvailable(true);
    panel.scopeButton(FootnoteScope::Book)->click();
    CHECK(panel.scope() == FootnoteScope::Book);
    CHECK(scopes == 1);
    const auto* model = qobject_cast<const QStandardItemModel*>(panel.list()->model());
    REQUIRE(model != nullptr);
    REQUIRE(model->rowCount() == 6);
    CHECK(model->item(0)->text() == QStringLiteral("The Storm"));
    CHECK(model->item(0)->flags() == Qt::NoItemFlags);  // the arrows go over it
    CHECK(model->item(4)->text() == QStringLiteral("The Calm"));
    CHECK(shownIds(panel) == "s1 s2 se c1");
    // A card says what it is: its number, its text, the words before it
    CHECK(model->item(1)->text() ==
          QStringLiteral("Footnote 1: A name of the guide (called Hilary)"));
    CHECK(model->item(3)->text() == QStringLiteral("Endnote i: And a donkey"));

    // The book closed: back to the text in front
    panel.setBookScopeAvailable(false);
    CHECK(panel.scope() == FootnoteScope::Text);
    CHECK(model->rowCount() == 4);

    // The name of the kind of the text in front, cut when long
    panel.setTextName(QStringLiteral("Chapter"));
    CHECK(panel.scopeButton(FootnoteScope::Text)->text() == QStringLiteral("Chapter"));
    CHECK(panel.scopeButton(FootnoteScope::Text)->toolTip().isEmpty());
    panel.setTextName(QStringLiteral("A very long name of a kind of text"));
    CHECK(panel.scopeButton(FootnoteScope::Text)->text().endsWith(QChar(0x2026)));
    CHECK(panel.scopeButton(FootnoteScope::Text)->toolTip() ==
          QStringLiteral("A very long name of a kind of text"));
    panel.setTextName(QString());
    CHECK(panel.scopeButton(FootnoteScope::Text)->text() == QStringLiteral("Text"));
}

TEST_CASE("Footnotes panel: the keys of the list and of the search", "[gui][footnotes][panel]") {
    FootnotesPanel panel(kalahari::test::commands());
    panel.setEntries(someNotes());
    Caught activated;
    Caught edits;
    Caught deletes;
    catchIn(panel, &FootnotesPanel::entryActivated, activated);
    catchIn(panel, &FootnotesPanel::editRequested, edits);
    catchIn(panel, &FootnotesPanel::deleteRequested, deletes);
    int back = 0;
    QObject::connect(&panel, &FootnotesPanel::editorFocusRequested, [&back]() { ++back; });

    // Down in the search: the first card
    press(panel.searchEdit(), Qt::Key_Down);
    CHECK(panel.selectedKey() == QStringLiteral("storm/s1"));
    // The arrows select a note: the text goes to its number
    const int before = activated.count();
    panel.list()->setCurrentIndex(panel.list()->model()->index(1, 0));
    CHECK(activated.count() == before + 1);
    CHECK(activated.last().id == QStringLiteral("s2"));
    // Selected by the program: no going there
    panel.selectEntry(QStringLiteral("calm/c1"));
    CHECK(activated.count() == before + 1);
    CHECK(panel.selectedKey() == QStringLiteral("calm/c1"));

    // Enter and F2 edit it, Delete deletes it
    press(panel.list(), Qt::Key_Return);
    press(panel.list(), Qt::Key_F2);
    REQUIRE(edits.count() == 2);
    CHECK(edits.entries.at(1).id == QStringLiteral("c1"));
    press(panel.list(), Qt::Key_Delete);
    REQUIRE(deletes.count() == 1);
    CHECK(deletes.entries.at(0).key() == QStringLiteral("calm/c1"));
    // A book open read-only: not deleted
    panel.setReadOnly(true);
    press(panel.list(), Qt::Key_Delete);
    CHECK(deletes.count() == 1);
    panel.setReadOnly(false);

    // Esc: back to the text, from the list and from the search
    press(panel.list(), Qt::Key_Escape);
    press(panel.searchEdit(), Qt::Key_Escape);
    CHECK(back == 2);

    // The selected note stays selected when the list is made again
    const std::vector<FootnoteEntry> all = someNotes();
    panel.setEntries(std::vector<FootnoteEntry>(all.begin() + 1, all.end()));
    CHECK(panel.selectedKey() == QStringLiteral("calm/c1"));
    CHECK(activated.count() == before + 1);
}

TEST_CASE("Footnotes panel: the menu of a note", "[gui][footnotes][panel]") {
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    FootnotesPanel panel(kalahari::test::commands());
    panel.setEntries(someNotes());
    Caught edits;
    Caught converts;
    Caught deletes;
    catchIn(panel, &FootnotesPanel::editRequested, edits);
    catchIn(panel, &FootnotesPanel::convertRequested, converts);
    catchIn(panel, &FootnotesPanel::deleteRequested, deletes);
    CHECK(panel.createMenu(QStringLiteral("none/x")) == nullptr);

    const std::unique_ptr<QMenu> footnote(panel.createMenu(QStringLiteral("storm/s1")));
    REQUIRE(footnote != nullptr);
    CHECK(textsOf(*footnote) ==
          QStringList({QStringLiteral("&Edit Footnote"), QStringLiteral("Con&vert to Endnote"),
                       QStringLiteral("&Copy Text"), QStringLiteral("&Delete Footnote"),
                       QStringLiteral("|"), QStringLiteral("Foo&tnote Layout...")}));
    const QList<QAction*> items = footnote->actions();
    items.at(0)->trigger();
    items.at(1)->trigger();
    items.at(2)->trigger();
    items.at(3)->trigger();
    CHECK(edits.count() == 1);
    CHECK(converts.count() == 1);
    CHECK(deletes.count() == 1);
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("A name of the guide"));

    const std::unique_ptr<QMenu> endnote(panel.createMenu(QStringLiteral("storm/se")));
    REQUIRE(endnote != nullptr);
    CHECK(endnote->actions().at(1)->text() == QStringLiteral("Con&vert to Footnote"));
    CHECK(endnote->actions().at(3)->text() == QStringLiteral("&Delete Endnote"));

    // Read-only: only edited (shown) and copied
    panel.setReadOnly(true);
    const std::unique_ptr<QMenu> readOnly(panel.createMenu(QStringLiteral("storm/s1")));
    REQUIRE(readOnly != nullptr);
    CHECK(readOnly->actions().at(0)->isEnabled());
    CHECK_FALSE(readOnly->actions().at(1)->isEnabled());
    CHECK(readOnly->actions().at(2)->isEnabled());
    CHECK_FALSE(readOnly->actions().at(3)->isEnabled());
    kalahari::test::commands().clear();
}

TEST_CASE("Footnotes panel: the cards follow the settings and the theme",
          "[gui][footnotes][panel][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const int savedLines = settings.get<int>(FootnotesPanel::LINES_SETTING, 2);
    FootnotesPanel panel(kalahari::test::commands());
    panel.resize(300, 400);
    std::vector<FootnoteEntry> notes = someNotes();
    notes[0].text = QStringLiteral("A long text of a note that goes on and on, line after line, "
                                   "and on again, more than one line of the narrow panel can "
                                   "show, and more than two, and still more words.");
    panel.setEntries(notes);
    panel.show();
    QApplication::processEvents();

    settings.set<int>(FootnotesPanel::LINES_SETTING, 1);
    QApplication::processEvents();
    const int oneLine = panel.list()->sizeHintForRow(0);
    settings.set<int>(FootnotesPanel::LINES_SETTING, 4);
    QApplication::processEvents();
    const int fourLines = panel.list()->sizeHintForRow(0);
    CHECK(fourLines > oneLine);
    // A card of a short text is not as high
    CHECK(panel.list()->sizeHintForRow(1) < fourLines);

    // The colors of the kinds come from the theme
    const auto& themes = core::ThemeManager::getInstance();
    CHECK(panel.kindColor(NoteKind::Footnote) ==
          themes.editorColor(FootnotesPanel::FOOTNOTE_COLOR_KEY, QColor()));
    CHECK(panel.kindColor(NoteKind::Endnote) ==
          themes.editorColor(FootnotesPanel::ENDNOTE_COLOR_KEY, QColor()));
    CHECK(panel.kindColor(NoteKind::Footnote) != panel.kindColor(NoteKind::Endnote));

    settings.set<int>(FootnotesPanel::LINES_SETTING, savedLines);
}

TEST_CASE("Footnotes panel: the notes of the text in front and of the whole book",
          "[gui][footnotes][panel][book]") {
    NotedBook noted;
    Window window;
    Notes notes(window);
    CHECK(notes.coordinator.entries().empty());

    // The text in front, as its editor numbers its notes, with the words before them
    editor::BookEditor& storm = open(window, noted.storm);
    notes.coordinator.refresh();
    CHECK(notes.ids() == "s1 s2 se");
    const FootnoteEntry wet = notes.entry(QStringLiteral("s2"));
    CHECK(wet.number == storm.noteNumber(QStringLiteral("s2")));
    CHECK(wet.text == QStringLiteral("Hard"));
    CHECK(wet.before == QStringLiteral("Rain fell"));
    CHECK(wet.textTitle == QStringLiteral("The Storm"));
    CHECK(notes.panel.shownEntries().size() == 3);
    CHECK(notes.panel.scopeButton(FootnoteScope::Book)->isEnabled());

    // The book: every text in reading order, the closed ones as saved
    notes.panel.setScope(FootnoteScope::Book);
    notes.coordinator.refresh();
    CHECK(notes.ids() == "s1 s2 se c1 ce a1");
    const FootnoteEntry still = notes.entry(QStringLiteral("c1"));
    const FootnoteEntry calmEnd = notes.entry(QStringLiteral("ce"));
    const FootnoteEntry hot = notes.entry(QStringLiteral("a1"));
    CHECK(still.textTitle == QStringLiteral("The Calm"));
    CHECK(still.before == QStringLiteral("Calm"));
    CHECK(still.textOrder < hot.textOrder);

    // A closed text numbered as its editor numbers it once open
    editor::BookEditor& calm = open(window, noted.calm);
    CHECK(still.number == calm.noteNumber(QStringLiteral("c1")));
    CHECK(calmEnd.number == calm.noteNumber(QStringLiteral("ce")));
    editor::BookEditor& after = open(window, noted.after);
    CHECK(hot.number == after.noteNumber(QStringLiteral("a1")));

    // A text changed: the list follows it
    window.tabs->setCurrentWidget(window.editor(0));
    REQUIRE(storm.removeNote(QStringLiteral("s1")));
    notes.coordinator.refresh();
    CHECK(notes.ids() == "s2 se c1 ce a1");
}

TEST_CASE("Footnotes panel: going to a note, editing, converting and deleting it",
          "[gui][footnotes][panel][book]") {
    NotedBook noted;
    Window window;
    Notes notes(window);
    std::vector<std::string> notices;
    std::function<void()> undo;
    notes.coordinator.setNoticeShower(
        [&notices, &undo](gui::EditorPanel*, const QString& message, const QString& action,
                          const QString&, std::function<void()> callback) {
            notices.push_back((message + QLatin1Char('|') + action).toStdString());
            undo = std::move(callback);
        });
    editor::BookEditor& storm = open(window, noted.storm);
    notes.panel.setScope(FootnoteScope::Book);
    notes.coordinator.refresh();
    const int tabs = window.tabs->count();

    // To its number in the text in front; the notes of a closed text do not open it
    storm.setCursorPosition(editor::CursorPosition{.paragraph = 1, .offset = 0});
    CHECK(notes.coordinator.goToNote(notes.entry(QStringLiteral("s2"))));
    CHECK(storm.cursorPosition().paragraph == 0);
    CHECK_FALSE(notes.coordinator.goToNote(notes.entry(QStringLiteral("c1"))));
    CHECK(window.tabs->count() == tabs);

    // A footnote made an endnote, one undo step
    CHECK(kindsIn(storm) == "ffe");
    REQUIRE(notes.coordinator.convertNote(notes.entry(QStringLiteral("s1"))));
    CHECK(kindsIn(storm) == "efe");
    CHECK(notes.entry(QStringLiteral("s1")).kind == NoteKind::Endnote);
    CHECK(notes.panel.selectedKey() == noted.storm + QStringLiteral("/s1"));
    storm.undo();
    CHECK(kindsIn(storm) == "ffe");

    // Deleted: the next card selected, the bar under the text says so, Undo brings it back
    notes.coordinator.refresh();
    const QString title = editor::BookEditor::noteTitle(NoteKind::Footnote,
                                                        storm.noteNumber(QStringLiteral("s1")));
    REQUIRE(notes.coordinator.deleteNote(notes.entry(QStringLiteral("s1"))));
    CHECK(kindsIn(storm) == "fe");
    REQUIRE(notices.size() == 1);
    CHECK(notices.at(0) == (QStringLiteral("Deleted: ") + title + QStringLiteral(".|&Undo"))
                               .toStdString());
    CHECK(notes.panel.selectedKey() == noted.storm + QStringLiteral("/s2"));
    REQUIRE(undo);
    undo();
    CHECK(kindsIn(storm) == "ffe");
    CHECK(notes.panel.selectedKey() == noted.storm + QStringLiteral("/s1"));
    // Once more: nothing, the deletion is no longer the last step
    undo();
    CHECK(kindsIn(storm) == "ffe");

    // A note of a closed text: its text opens, in front, and changes there
    REQUIRE(notes.coordinator.convertNote(notes.entry(QStringLiteral("a1"))));
    CHECK(window.tabs->count() == tabs + 1);
    auto* front = qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget());
    REQUIRE(front != nullptr);
    CHECK(front->property("elementId").toString() == noted.after);
    CHECK(kindsIn(*front->getBookEditor()) == "e");

    // Edit opens the frame of the note in its text
    CHECK(notes.coordinator.editNote(notes.entry(QStringLiteral("c1"))));
    front = qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget());
    REQUIRE(front != nullptr);
    CHECK(front->property("elementId").toString() == noted.calm);
}
