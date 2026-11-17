// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_clippings.cpp
/// @brief Clippings: Edit > Cut to Clippings takes the fragment to a new clipping of the
///        Workshop, which keeps where it comes from; Ctrl+Z and Ctrl+Y take the clipping with
///        the fragment; Insert > Clipping puts it in at the cursor (it stays marked as inserted,
///        or goes), and Restore to the Place It Came From puts it back where it was

#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/clippings_coordinator.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/clipping_dialogs.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/clipping_origin_box.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::Clippings;
using editor::BookEditor;
using editor::CursorPosition;
using gui::ClippingsCoordinator;
using test::OpenNovel;
using test::Window;

namespace {

/// "It |rained |all night.": the word cut in most of the tests, in the second paragraph
constexpr int RAINED_FIRST = 3;
constexpr int RAINED_LAST = 10;
/// The end of "Calm."
constexpr int CALM_END = 5;
/// How often withDialog() looks for the window of the program (ms)
constexpr int DIALOG_POLL_MS = 10;

/// A place in a text: character @p offset of paragraph @p paragraph
CursorPosition at(int paragraph, int offset) {
    return {.paragraph = paragraph, .offset = offset};
}

/// The word "rained" of stormKml()
editor::SelectionRange rained() {
    return {.start = at(1, RAINED_FIRST), .end = at(1, RAINED_LAST)};
}

/// A chapter of three paragraphs
QString stormKml() {
    return QStringLiteral(
        "<kml><p>The storm came.</p><p>It rained <b>all</b> night.</p><p>The end.</p></kml>");
}

/// stormKml() without "rained "
QString stormWithoutRainedKml() {
    return QStringLiteral(
        "<kml><p>The storm came.</p><p>It <b>all</b> night.</p><p>The end.</p></kml>");
}

/// Save a text of the book in its file
void save(OpenNovel& book, const QString& elementId, const QString& kml) {
    book.pm.setChapterContent(elementId, kml);
    REQUIRE(book.pm.saveChapterContent(elementId));
}

/// The text of a text of the book as its file on the disk has it
QString fileOf(const OpenNovel& book, const QString& elementId) {
    const core::ProjectElement* element = book.pm.findElement(elementId);
    REQUIRE(element != nullptr);
    QFile file(book.pm.filePathOf(*element));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll())
        .object()
        .value(QStringLiteral("content"))
        .toObject()
        .value(QStringLiteral("kml"))
        .toString();
}

/// @p kml as the editor writes it
QString asWritten(const QString& kml) {
    BookEditor editor;
    editor.fromKml(kml);
    return editor.toKml();
}

/// The editor of a text open in a window
BookEditor* openText(const Window& window, const QString& elementId) {
    for (int i = 0; i < window.tabs->count(); ++i) {
        if (window.tabs->widget(i)->property("elementId").toString() == elementId) {
            return window.editor(i)->getBookEditor();
        }
    }
    return nullptr;
}

/// Open a text of the book in its tab
BookEditor* open(Window& window, const QString& elementId) {
    const core::ProjectElement* element = test::book().findElement(elementId);
    REQUIRE(element != nullptr);
    window.navigator.onElementSelected(elementId, element->title);
    REQUIRE(openText(window, elementId) != nullptr);
    return openText(window, elementId);
}

/// Close the tab of a text of the book
void closeText(Window& window, OpenNovel& book, const QString& elementId) {
    const core::ProjectElement* element = book.pm.findElement(elementId);
    REQUIRE(element != nullptr);
    window.navigator.closeTabsOf(*element);
    QCoreApplication::processEvents();
    // The editor of the tab goes now, not later
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// The clippings of the book, in the order of its texts
QStringList clippingIds(const OpenNovel& book) {
    QStringList ids;
    for (const QString& elementId : book.pm.textElementIds()) {
        const core::ProjectElement* element = book.pm.findElement(elementId);
        if (element != nullptr && core::ProjectManager::isClipping(*element)) {
            ids << elementId;
        }
    }
    return ids;
}

/// The title of a text of the book
QString titleOf(const OpenNovel& book, const QString& elementId) {
    const core::ProjectElement* element = book.pm.findElement(elementId);
    REQUIRE(element != nullptr);
    return element->title;
}

/// What clipping @p clippingId of the book knows besides its text
core::ClippingData dataOf(const OpenNovel& book, const QString& clippingId) {
    const core::ProjectElement* element = book.pm.findElement(clippingId);
    REQUIRE(element != nullptr);
    return element->clipping;
}

/// The keys of Edit > Undo as the notices name them
QString undoKeys() {
    const gui::Command* command = test::commands().getCommand("edit.undo");
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QStringLiteral("Edit > Undo");
}

/// Choose @p value for setting @p key (core::Clippings)
void choose(const char* key, const char* value) {
    core::SettingsManager::getInstance().set<std::string>(key, value);
}

/// The choice of setting @p key (core::Clippings)
std::string chosen(const char* key) {
    return core::SettingsManager::getInstance().get<std::string>(key);
}

/// The settings of the clippings as they are by default
void useDefaultSettings() {
    choose(Clippings::CUT_SETTING, Clippings::CUT_AT_ONCE);
    choose(Clippings::NO_SELECTION_SETTING, Clippings::NO_SELECTION_PARAGRAPH);
    choose(Clippings::INSERT_SETTING, Clippings::INSERT_WINDOW);
    choose(Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_KEEP);
    core::SettingsManager::getInstance().set<bool>(Clippings::KEEP_WORDS_SETTING, false);
    core::SettingsManager::getInstance().set<bool>(Clippings::MARKS_SETTING, false);
}

/// Run @p command; the first window of the program it shows goes to @p answer, which closes it
/// (a window left open is closed with Esc)
/// @return Whether the command showed a window
bool withDialog(const std::function<void()>& command,
                const std::function<void(gui::dialogs::KalahariDialog&)>& answer) {
    bool answered = false;
    QTimer timer;
    timer.setInterval(DIALOG_POLL_MS);
    QObject::connect(&timer, &QTimer::timeout, [&answered, &answer]() {
        auto* dialog = qobject_cast<gui::dialogs::KalahariDialog*>(test::shownDialog());
        if (dialog == nullptr || answered) {
            return;
        }
        answered = true;
        answer(*dialog);
        if (dialog->isVisible()) {
            dialog->reject();
        }
    });
    timer.start();
    command();
    return answered;
}

/// Esc in a window
void reject(gui::dialogs::KalahariDialog& dialog) {
    dialog.reject();
}

/// The small window of Cut to Clippings: the title "  Rain  ", a note, and the next cuts at
/// once; @p offered gets the title the window offered
void answerCut(gui::dialogs::KalahariDialog& dialog, QString& offered) {
    auto* title = dialog.findChild<QLineEdit*>(QStringLiteral("clippingTitle"));
    auto* note = dialog.findChild<QPlainTextEdit*>(QStringLiteral("clippingNote"));
    auto* atOnce = dialog.findChild<QCheckBox*>(QStringLiteral("clippingCutAtOnce"));
    if (title == nullptr || note == nullptr || atOnce == nullptr) {
        return;
    }
    offered = title->text();
    title->setText(QStringLiteral("  Rain  "));
    note->setPlainText(QStringLiteral("Maybe in chapter 5."));
    atOnce->setChecked(true);
    dialog.acceptButton()->click();
}

/// What the window of Insert > Clipping showed before and after the search "WET"
struct WindowSearch {
    bool opened = false;  ///< The window opened
    QStringList shown;    ///< The clippings it showed first
    QStringList found;    ///< Those the search found
};

/// The window of Insert > Clipping: @p result gets the clippings it shows and those the search
/// "WET" finds (in the notes too), and Enter inserts the one chosen
void answerSearch(gui::dialogs::KalahariDialog& dialog, WindowSearch& result) {
    auto* insert = qobject_cast<gui::dialogs::InsertClippingDialog*>(&dialog);
    auto* search = dialog.findChild<QLineEdit*>(QStringLiteral("clippingSearch"));
    if (insert == nullptr || search == nullptr) {
        return;
    }
    result.shown = insert->shownIds();
    search->setText(QStringLiteral("WET"));
    result.found = insert->shownIds();
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(search, &enter);
}

/// The window of Insert > Clipping: Restore to the Place It Came From for the second clipping
void answerRestore(gui::dialogs::KalahariDialog& dialog) {
    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("clippingList"));
    auto* restore = dialog.findChild<QPushButton*>(QStringLiteral("clippingRestore"));
    if (list == nullptr || restore == nullptr) {
        return;
    }
    list->setCurrentRow(1);
    restore->click();
}

/// A key pressed in @p widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// The list of Insert > Clipping at the cursor, open in @p window
gui::dialogs::ClippingListPopup* popupOf(const Window& window) {
    return window.window.findChild<gui::dialogs::ClippingListPopup*>();
}

/// The search of the list at the cursor
QLineEdit* searchOf(const gui::dialogs::ClippingListPopup& popup) {
    auto* search = popup.findChild<QLineEdit*>(QStringLiteral("clippingSearch"));
    REQUIRE(search != nullptr);
    return search;
}

/// The list of choices @p name of a page of the Settings
QComboBox* choiceList(const QWidget& page, const char* name) {
    auto* found = page.findChild<QComboBox*>(QString::fromLatin1(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The value of the item chosen in list @p name of @p page
std::string shownIn(const QWidget& page, const char* name) {
    return choiceList(page, name)->currentData().toString().toStdString();
}

/// Choose the item of value @p value in list @p name of @p page
void pick(const QWidget& page, const char* name, const char* value) {
    QComboBox* list = choiceList(page, name);
    const int index = list->findData(QString::fromLatin1(value));
    REQUIRE(index >= 0);
    list->setCurrentIndex(index);
}

/// Cut to Clippings and Insert > Clipping, alone in @p commands
void addClippingCommands(gui::CommandRegistry& commands) {
    for (const char* commandId :
         {ClippingsCoordinator::CUT_COMMAND, ClippingsCoordinator::INSERT_COMMAND}) {
        gui::Command command;
        command.id = commandId;
        commands.registerCommand(command);
    }
}

/// A novel of two chapters, "The Storm" with stormKml() and "The Calm" after it
class ClippingBook {
public:
    ClippingBook()
        : m_calm(m_book.pm.addElement(m_book.pm.chapterKindFor(core::BookPlace::Main),
                                      QStringLiteral("The Calm"), core::BookPlace::Main)) {
        REQUIRE_FALSE(m_calm.isEmpty());
        save(m_book, m_book.chapterId, stormKml());
        save(m_book, m_calm, QStringLiteral("<kml><p>Calm.</p></kml>"));
    }

    /// The book
    [[nodiscard]] OpenNovel& book() { return m_book; }
    /// "The Storm"
    [[nodiscard]] const QString& storm() const { return m_book.chapterId; }
    /// "The Calm"
    [[nodiscard]] const QString& calm() const { return m_calm; }

private:
    OpenNovel m_book{QStringLiteral("Clippings")};
    QString m_calm;
};

/// The clippings over the tabs of a window, as the main window has them, with the settings of
/// the clippings as they are by default; the notices under the texts are kept with their buttons
class Clips {
public:
    explicit Clips(Window& window)
        : m_coordinator(
              test::book(), test::commands(), window.tabs, &window.navigator, {},
              ClippingsCoordinator::Feedback{
                  .showNotice =
                      [this](gui::EditorPanel* /*editor*/, const QString& message,
                             const QString& actionText, const QString& /*actionToolTip*/,
                             const std::function<void()>& action) {
                          m_notices.append(message + QLatin1Char('|') + actionText);
                          m_buttons.append(action);
                      },
                  .statusBar = &window.statusBar,
                  .window = &window.window}) {
        useDefaultSettings();
    }

    /// The coordinator
    [[nodiscard]] ClippingsCoordinator& coordinator() { return m_coordinator; }
    /// The notices shown, each with the text of its button after "|"
    [[nodiscard]] const QStringList& notices() const { return m_notices; }
    /// How many notices had a button
    [[nodiscard]] qsizetype buttons() const { return m_buttons.size(); }
    /// Press the button of the first notice
    void pressButton() const {
        REQUIRE_FALSE(m_buttons.isEmpty());
        m_buttons.first()();
    }
    /// Forget the notices shown so far
    void forgetNotices() {
        m_notices.clear();
        m_buttons.clear();
    }

private:
    QStringList m_notices;
    QList<std::function<void()>> m_buttons;
    ClippingsCoordinator m_coordinator;
};

/// Two clippings of "The Storm": "rained", with the note "Too wet.", then its last paragraph
struct TwoClippings {
    QString rained;
    QString last;
};

/// Cut the two clippings of TwoClippings from @p storm
TwoClippings cutTwo(Clips& clips, OpenNovel& book, BookEditor* storm) {
    storm->setSelection(rained());
    TwoClippings cut;
    cut.rained = clips.coordinator().cut(storm);
    REQUIRE_FALSE(cut.rained.isEmpty());
    REQUIRE(book.pm.setNotes(cut.rained, QStringLiteral("Too wet.")));
    storm->setCursorPosition(at(2, 4));
    cut.last = clips.coordinator().cut(storm);  // "end." goes with "The end."
    REQUIRE_FALSE(cut.last.isEmpty());
    return cut;
}

/// What the small window of Cut to Clippings offered, and what the cut gave
struct WindowCut {
    bool shown = false;  ///< The window opened
    QString offered;     ///< The title it offered
    QString clippingId;  ///< The clipping cut; empty: none
};

/// Cut to Clippings in @p text with the small window: answered as answerCut() does, or closed
/// with Esc (@p escape)
WindowCut cutInWindow(Clips& clips, BookEditor* text, bool escape) {
    WindowCut cut;
    cut.clippingId = QStringLiteral("none");
    const auto command = [&clips, &cut, text]() { cut.clippingId = clips.coordinator().cut(text); };
    if (escape) {
        cut.shown = withDialog(command, &reject);
    } else {
        cut.shown = withDialog(command, [&cut](gui::dialogs::KalahariDialog& dialog) {
            answerCut(dialog, cut.offered);
        });
    }
    return cut;
}

/// Insert > Clipping in @p text, the window answered as answerSearch() does
WindowSearch insertWithSearch(Clips& clips, BookEditor* text) {
    WindowSearch search;
    search.opened = withDialog([&clips, text]() { clips.coordinator().insert(text); },
                               [&search](gui::dialogs::KalahariDialog& dialog) {
                                   answerSearch(dialog, search);
                               });
    return search;
}

/// The text with the keys, as the test sets it
class Keys {
public:
    /// The text with the keys from now on; none: the keys are elsewhere
    void set(BookEditor* text) { m_text = text; }

    /// For the coordinator: the text as it is when asked
    [[nodiscard]] ClippingsCoordinator::TextWithKeys get() const {
        return [this]() { return m_text; };
    }

private:
    BookEditor* m_text = nullptr;
};

/// Whether @p place is in the Workshop
bool isInWorkshop(const std::optional<core::ElementPlace>& place) {
    return place.has_value() && place->place == core::BookPlace::Workshop;
}

}  // namespace

TEST_CASE("Cut to Clippings: the selection goes to a new clipping of the Workshop",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());

    text->setSelection(rained());
    CHECK(clips.coordinator().whyNotCut(text).isEmpty());
    CHECK(clips.coordinator().canCut(*text));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());

    // A clipping of the Workshop, among the materials, titled with its words
    const core::ProjectElement* clipping = book.pm.findElement(clippingId);
    REQUIRE(clipping != nullptr);
    CHECK(core::ProjectManager::isClipping(*clipping));
    CHECK(clipping->title == QStringLiteral("rained"));
    const std::optional<core::ElementPlace> place = book.pm.project()->placeOf(clippingId);
    CHECK(isInWorkshop(place));
    const core::KindRef kind = book.pm.kindOf(*clipping);
    REQUIRE(kind);
    CHECK(kind.kind->workshopGroup == core::WorkshopGroup::Resources);
    CHECK(fileOf(book, clippingId) == asWritten(QStringLiteral("<kml><p>rained </p></kml>")));

    // The text without it, saved
    CHECK(text->toKml() == asWritten(stormWithoutRainedKml()));
    CHECK(fileOf(book, clipped.storm()) == asWritten(stormWithoutRainedKml()));
    CHECK_FALSE(window.navigator.isChapterDirty(clipped.storm()));
    CHECK_FALSE(text->hasSelection());

    // Where it comes from: the text, its paragraph and the words around the place
    const core::ClippingOrigin origin = clipping->clipping.origin;
    CHECK(origin.element == clipped.storm());
    CHECK(origin.title == QStringLiteral("The Storm"));
    CHECK(origin.paragraph == 2);
    CHECK(origin.before == QStringLiteral("It "));
    CHECK(origin.after == QStringLiteral("all night."));
    CHECK(origin.cut.isValid());
    CHECK(clipping->clipping.inserted.isEmpty());

    // The notice says where it went, with Undo
    CHECK(clips.notices() ==
          QStringList{QStringLiteral("The fragment cut (1 word(s)) is in the clippings as "
                                     "\"rained\". %1 takes the cut back.|&Undo")
                          .arg(undoKeys())});

    // Undo of the notice: the fragment comes back, the clipping goes with its file
    const QString clippingFile = book.pm.filePathOf(*clipping);
    REQUIRE(QFile::exists(clippingFile));
    CHECK(clips.coordinator().canUndoLast());
    REQUIRE(clips.buttons() == 1);
    clips.pressButton();
    CHECK(book.pm.findElement(clippingId) == nullptr);
    CHECK_FALSE(QFile::exists(clippingFile));
    CHECK(text->toKml() == asWritten(stormKml()));
    CHECK(fileOf(book, clipped.storm()) == asWritten(stormKml()));
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The fragment is back in the text, and the clipping \"rained\" went "
                         "from the Workshop."));
    CHECK_FALSE(clips.coordinator().canUndoLast());
    CHECK_FALSE(clips.coordinator().undoLast());

    // Ctrl+Y cuts it again: the clipping as it was
    text->redo();
    const QStringList again = clippingIds(book);
    REQUIRE(again.size() == 1);
    CHECK(titleOf(book, again.first()) == QStringLiteral("rained"));
    CHECK(dataOf(book, again.first()).origin == origin);
    CHECK(fileOf(book, again.first()) == asWritten(QStringLiteral("<kml><p>rained </p></kml>")));
    CHECK(text->toKml() == asWritten(stormWithoutRainedKml()));
    CHECK(fileOf(book, clipped.storm()) == asWritten(stormWithoutRainedKml()));
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The fragment is in the clippings again as \"rained\"."));

    // Ctrl+Z takes it back once more
    text->undo();
    CHECK(clippingIds(book).isEmpty());
    CHECK(text->toKml() == asWritten(stormKml()));
    CHECK(fileOf(book, clipped.storm()) == asWritten(stormKml()));

    // A new edit: Ctrl+Y has nothing to cut again
    text->setCursorPosition(at(0, 0));
    text->insertText(QStringLiteral("Then "));
    text->redo();
    CHECK(clippingIds(book).isEmpty());
}

TEST_CASE("Cut to Clippings: without a selection, the paragraph goes whole and comes back",
          "[gui][clippings]") {
    ClippingBook clipped;
    const OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());

    text->setCursorPosition(at(1, 4));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());
    CHECK(titleOf(book, clippingId) == QStringLiteral("It rained all night."));
    CHECK(text->toKml() ==
          asWritten(QStringLiteral("<kml><p>The storm came.</p><p>The end.</p></kml>")));
    CHECK(fileOf(book, clippingId) ==
          asWritten(QStringLiteral("<kml><p>It rained <b>all</b> night.</p><p></p></kml>")));
    const core::ClippingOrigin origin = dataOf(book, clippingId).origin;
    CHECK(origin.paragraph == 2);
    CHECK(origin.before.isEmpty());
    CHECK(origin.after == QStringLiteral("The end."));

    // Restore to the Place It Came From: the text is as it was, the clipping marked
    clips.forgetNotices();
    CHECK(clips.coordinator().restore(clippingId));
    CHECK(text->toKml() == asWritten(stormKml()));
    CHECK(fileOf(book, clipped.storm()) == asWritten(stormKml()));
    const QList<core::ClippingInsertion> inserted = dataOf(book, clippingId).inserted;
    REQUIRE(inserted.size() == 1);
    CHECK(inserted.first().element == clipped.storm());
    CHECK(inserted.first().at.isValid());
    CHECK(clips.notices().size() == 1);
}

TEST_CASE("Cut to Clippings: the last paragraph goes with the end of the one before it",
          "[gui][clippings]") {
    ClippingBook clipped;
    const OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());

    text->setCursorPosition(at(2, 2));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());
    CHECK(titleOf(book, clippingId) == QStringLiteral("The end."));
    CHECK(text->toKml() ==
          asWritten(QStringLiteral(
              "<kml><p>The storm came.</p><p>It rained <b>all</b> night.</p></kml>")));
    const core::ClippingOrigin origin = dataOf(book, clippingId).origin;
    CHECK(origin.paragraph == 2);
    CHECK(origin.before == QStringLiteral("It rained all night."));
    CHECK(origin.after.isEmpty());
    CHECK(clips.coordinator().restore(clippingId));
    CHECK(text->toKml() == asWritten(stormKml()));
}

TEST_CASE("Cut to Clippings: an empty paragraph has nothing to cut", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);

    save(book, clipped.storm(), QStringLiteral("<kml><p>One.</p><p> </p><p>Two.</p></kml>"));
    BookEditor* spaced = open(window, clipped.storm());
    spaced->setCursorPosition(at(1, 0));
    CHECK(clips.coordinator().cut(spaced).isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The paragraph with the cursor is empty, and nothing is selected"));
    CHECK(clippingIds(book).isEmpty());
    CHECK(clips.notices().isEmpty());
}

TEST_CASE("Cut to Clippings: the setting greys the command out without a selection",
          "[gui][clippings]") {
    ClippingBook clipped;
    const OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());

    choose(Clippings::NO_SELECTION_SETTING, Clippings::NO_SELECTION_NOTHING);
    text->setCursorPosition(at(1, 4));
    CHECK(clips.coordinator().whyNotCut(text) ==
          QStringLiteral("Select a fragment of the text first. Without a selection the "
                         "command cuts the paragraph with the cursor when the settings say "
                         "so."));
    CHECK(clips.coordinator().cut(text).isEmpty());
    CHECK(clippingIds(book).isEmpty());
    text->setSelection(rained());
    CHECK(clips.coordinator().whyNotCut(text).isEmpty());
    CHECK_FALSE(clips.coordinator().cut(text).isEmpty());
}

TEST_CASE("Cut to Clippings: the small window gives the title and a note first",
          "[gui][clippings]") {
    ClippingBook clipped;
    const OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    choose(Clippings::CUT_SETTING, Clippings::CUT_WITH_WINDOW);
    BookEditor* text = open(window, clipped.storm());

    text->setCursorPosition(at(1, 4));
    const WindowCut cut = cutInWindow(clips, text, false);
    CHECK(cut.shown);
    CHECK(cut.offered == QStringLiteral("It rained all night."));
    const QString clippingId = cut.clippingId;
    REQUIRE_FALSE(clippingId.isEmpty());
    CHECK(titleOf(book, clippingId) == QStringLiteral("Rain"));
    CHECK(book.pm.notes(clippingId) == QStringLiteral("Maybe in chapter 5."));
    CHECK(text->toKml() ==
          asWritten(QStringLiteral("<kml><p>The storm came.</p><p>The end.</p></kml>")));
    // The next cuts go without the window
    CHECK(chosen(Clippings::CUT_SETTING) == Clippings::CUT_AT_ONCE);

    // Undo and redo keep the title and the note
    text->undo();
    CHECK(clippingIds(book).isEmpty());
    text->redo();
    const QStringList again = clippingIds(book);
    REQUIRE(again.size() == 1);
    CHECK(titleOf(book, again.first()) == QStringLiteral("Rain"));
    CHECK(book.pm.notes(again.first()) == QStringLiteral("Maybe in chapter 5."));
}

TEST_CASE("Cut to Clippings: Esc in the small window, nothing is cut", "[gui][clippings]") {
    ClippingBook clipped;
    const OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    choose(Clippings::CUT_SETTING, Clippings::CUT_WITH_WINDOW);
    BookEditor* text = open(window, clipped.storm());

    text->setCursorPosition(at(1, 4));
    const WindowCut cut = cutInWindow(clips, text, true);
    CHECK(cut.shown);
    CHECK(cut.clippingId.isEmpty());
    CHECK(clippingIds(book).isEmpty());
    CHECK(text->toKml() == asWritten(stormKml()));
    CHECK_FALSE(text->hasSelection());
    CHECK(text->cursorPosition() == at(1, 4));
    CHECK(chosen(Clippings::CUT_SETTING) == Clippings::CUT_WITH_WINDOW);
}

TEST_CASE("Cut to Clippings: Ctrl+Z leaves a clipping changed since", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());

    SECTION("its title") {
        REQUIRE(book.pm.renameElement(clippingId, QStringLiteral("Wet")));
    }
    SECTION("its text, in its tab") {
        BookEditor* clippingText = open(window, clippingId);
        clippingText->setCursorPosition(at(0, 0));
        clippingText->insertText(QStringLiteral("hard "));
    }
    SECTION("its note") {
        REQUIRE(book.pm.setNotes(clippingId, QStringLiteral("Keep it.")));
    }

    // The fragment comes back, and the clipping stays
    text->undo();
    CHECK(text->toKml() == asWritten(stormKml()));
    REQUIRE(book.pm.findElement(clippingId) != nullptr);
    CHECK(window.statusBar.currentMessage().endsWith(
        QStringLiteral("changed since, so it stays in the Workshop.")));
    // Ctrl+Y cuts the fragment again, and the clipping is not doubled
    text->redo();
    CHECK(text->toKml() == asWritten(stormWithoutRainedKml()));
    CHECK(clippingIds(book) == QStringList{clippingId});
}

TEST_CASE("Cut to Clippings: Undo of the notice is gone once the text changes",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());
    REQUIRE(clips.coordinator().canUndoLast());

    text->setCursorPosition(at(0, 0));
    text->insertText(QStringLiteral("Then "));
    CHECK_FALSE(clips.coordinator().canUndoLast());
    CHECK_FALSE(clips.coordinator().undoLast());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone from the notice: the text changed "
                         "since."));
    CHECK(book.pm.findElement(clippingId) != nullptr);

    // Ctrl+Z in the text still goes back step by step: the edit, then the cut
    text->undo();
    CHECK(book.pm.findElement(clippingId) != nullptr);
    text->undo();
    CHECK(text->toKml() == asWritten(stormKml()));
    CHECK(clippingIds(book).isEmpty());
}

TEST_CASE("Insert > Clipping: in at the cursor, marked as inserted or taken from the Workshop",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* storm = open(window, clipped.storm());
    storm->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(clippingId.isEmpty());
    BookEditor* calm = open(window, clipped.calm());
    const QString calmWith = asWritten(QStringLiteral("<kml><p>Calm.rained </p></kml>"));
    const QString calmWithout = asWritten(QStringLiteral("<kml><p>Calm.</p></kml>"));
    calm->setCursorPosition(at(0, CALM_END));
    clips.forgetNotices();

    SECTION("it stays, marked as inserted; Ctrl+Z takes the mark back") {
        CHECK(clips.coordinator().whyNotInsert(calm).isEmpty());
        CHECK(clips.coordinator().insertAt(calm, clippingId));
        CHECK(calm->toKml() == calmWith);
        CHECK(fileOf(book, clipped.calm()) == calmWith);
        const QList<core::ClippingInsertion> inserted = dataOf(book, clippingId).inserted;
        REQUIRE(inserted.size() == 1);
        CHECK(inserted.first().element == clipped.calm());
        CHECK(clips.notices() ==
              QStringList{QStringLiteral("Clipping \"rained\" inserted (1 word(s)). It stays in "
                                         "the Workshop, marked as inserted. %1 takes the "
                                         "insertion back.|&Undo")
                              .arg(undoKeys())});
        // Not among those not inserted yet
        CHECK(clips.coordinator().clippings(false).isEmpty());
        REQUIRE(clips.coordinator().clippings(true).size() == 1);
        CHECK(clips.coordinator().clippings(true).first().inserted);

        clips.pressButton();
        CHECK(calm->toKml() == calmWithout);
        CHECK(fileOf(book, clipped.calm()) == calmWithout);
        CHECK(dataOf(book, clippingId).inserted.isEmpty());
        calm->redo();
        CHECK(calm->toKml() == calmWith);
        CHECK(dataOf(book, clippingId).inserted.size() == 1);
    }

    SECTION("it goes from the Workshop; Ctrl+Z brings it back") {
        choose(Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_REMOVE);
        CHECK(clips.coordinator().insertAt(calm, clippingId));
        CHECK(calm->toKml() == calmWith);
        CHECK(book.pm.findElement(clippingId) == nullptr);
        CHECK(clippingIds(book).isEmpty());
        CHECK(clips.notices() ==
              QStringList{QStringLiteral("Clipping \"rained\" inserted (1 word(s)) and taken "
                                         "from the Workshop. %1 takes both back.|&Undo")
                              .arg(undoKeys())});

        calm->undo();
        CHECK(calm->toKml() == calmWithout);
        const QStringList back = clippingIds(book);
        REQUIRE(back.size() == 1);
        CHECK(titleOf(book, back.first()) == QStringLiteral("rained"));
        CHECK(dataOf(book, back.first()).origin.element == clipped.storm());
        CHECK(dataOf(book, back.first()).inserted.isEmpty());
        CHECK(fileOf(book, back.first()) == asWritten(QStringLiteral("<kml><p>rained </p></kml>")));
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("The clipping \"rained\" is in the Workshop again."));

        calm->redo();
        CHECK(calm->toKml() == calmWith);
        CHECK(clippingIds(book).isEmpty());
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("The clipping \"rained\" is in the text again and went from the "
                             "Workshop."));

        // The cut in the other text still goes back with Ctrl+Z, without a clipping to take
        storm->undo();
        CHECK(storm->toKml() == asWritten(stormKml()));
        CHECK(clippingIds(book).isEmpty());
    }

    SECTION("a clipping cannot go into itself") {
        BookEditor* itself = open(window, clippingId);
        itself->setCursorPosition(at(0, 0));
        CHECK_FALSE(clips.coordinator().insertAt(itself, clippingId));
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("A clipping cannot be inserted into itself"));
        // The only clipping: nothing else to insert there
        CHECK(clips.coordinator().whyNotInsert(itself) ==
              QStringLiteral("The book has no clippings yet (Edit > Cut to Clippings)"));
    }
}

TEST_CASE("Insert > Clipping: Restore to the Place It Came From", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* storm = open(window, clipped.storm());
    storm->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(clippingId.isEmpty());
    open(window, clipped.calm());  // another text in front
    clips.forgetNotices();

    SECTION("the place as it was: the text in front, the clipping in its place") {
        // A paragraph before it does not matter
        storm->setCursorPosition(at(0, 0));
        storm->insertText(QStringLiteral("Night.\n"));
        CHECK(clips.coordinator().restore(clippingId));
        CHECK(window.tabs->currentWidget()->property("elementId").toString() == clipped.storm());
        CHECK(storm->toKml() == asWritten(QStringLiteral(
                                    "<kml><p>Night.</p><p>The storm came.</p><p>It rained "
                                    "<b>all</b> night.</p><p>The end.</p></kml>")));
        CHECK(dataOf(book, clippingId).inserted.size() == 1);
    }

    SECTION("the words around it changed: the cursor near it, and Insert Here") {
        save(book, clipped.storm(),
             QStringLiteral("<kml><p>The storm came.</p><p>It snowed.</p><p>The end.</p></kml>"));
        closeText(window, book, clipped.storm());
        CHECK_FALSE(clips.coordinator().restore(clippingId));
        const BookEditor* reopened = openText(window, clipped.storm());
        REQUIRE(reopened != nullptr);
        // After the words before it, which are still there
        CHECK(reopened->cursorPosition() == at(1, RAINED_FIRST));
        CHECK(clips.notices() ==
              QStringList{QStringLiteral("The exact place is no longer in the text. The cursor is "
                                         "in the paragraph the fragment came from.|&Insert Here")});
        CHECK(dataOf(book, clippingId).inserted.isEmpty());
        REQUIRE(clips.buttons() == 1);
        clips.pressButton();
        CHECK(reopened->toKml() == asWritten(QStringLiteral(
                                       "<kml><p>The storm came.</p><p>It rained snowed.</p>"
                                       "<p>The end.</p></kml>")));
        CHECK(dataOf(book, clippingId).inserted.size() == 1);
    }

    SECTION("the text it came from is gone") {
        const std::optional<core::ProjectElement> removed = book.pm.removeElement(clipped.storm());
        REQUIRE(removed.has_value());
        window.navigator.closeTabsOf(removed.value_or(core::ProjectElement()));
        QCoreApplication::processEvents();
        CHECK_FALSE(clips.coordinator().restore(clippingId));
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("The text this clipping was cut from is no longer in the book"));
        const QList<gui::ClippingEntry> entries = clips.coordinator().clippings(true);
        REQUIRE(entries.size() == 1);
        CHECK(entries.first().originGone);
        CHECK(entries.first().from == QStringLiteral("The Storm (deleted)"));
    }

    SECTION("a clipping added by hand has no place") {
        const QString byHand = book.pm.addClipping(QStringLiteral("Loose"),
                                                   QStringLiteral("<kml><p>Loose.</p></kml>"),
                                                   core::ClippingData());
        REQUIRE_FALSE(byHand.isEmpty());
        CHECK_FALSE(clips.coordinator().showOrigin(byHand));
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("This clipping was added by hand: it has no place it came from"));
    }
}

TEST_CASE("Insert > Clipping: the entries, the last cut first", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    const TwoClippings cut = cutTwo(clips, book, open(window, clipped.storm()));

    // With their words, where they come from and the notes
    const QList<gui::ClippingEntry> entries = clips.coordinator().clippings(true);
    REQUIRE(entries.size() == 2);
    CHECK(entries.at(0).id == cut.last);
    CHECK(entries.at(1).id == cut.rained);
    CHECK(entries.at(1).title == QStringLiteral("rained"));
    CHECK(entries.at(1).from == QStringLiteral("The Storm"));
    CHECK(entries.at(1).fromId == clipped.storm());
    CHECK(entries.at(1).words == 1);
    CHECK(entries.at(1).note == QStringLiteral("Too wet."));
    CHECK_FALSE(entries.at(1).inserted);
}

TEST_CASE("Insert > Clipping: the window, the search, then Insert", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    const TwoClippings cut = cutTwo(clips, book, open(window, clipped.storm()));
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(0, CALM_END));

    const WindowSearch search = insertWithSearch(clips, calm);
    CHECK(search.opened);
    CHECK(search.shown == QStringList{cut.last, cut.rained});
    // The note is searched too
    CHECK(search.found == QStringList{cut.rained});
    CHECK(calm->toKml() == asWritten(QStringLiteral("<kml><p>Calm.rained </p></kml>")));
}

TEST_CASE("Insert > Clipping: the window, Restore to the Place It Came From",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* storm = open(window, clipped.storm());
    cutTwo(clips, book, storm);
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(0, CALM_END));

    const bool opened =
        withDialog([&clips, calm]() { clips.coordinator().insert(calm); }, &answerRestore);
    CHECK(opened);
    CHECK(storm->toKml() ==
          asWritten(QStringLiteral("<kml><p>The storm came.</p><p>It rained <b>all</b> "
                                   "night.</p></kml>")));
    CHECK(calm->toKml() == asWritten(QStringLiteral("<kml><p>Calm.</p></kml>")));
}

TEST_CASE("Insert > Clipping: Esc in the window, nothing happens", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    cutTwo(clips, book, open(window, clipped.storm()));
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(0, CALM_END));

    const bool opened =
        withDialog([&clips, calm]() { clips.coordinator().insert(calm); }, &reject);
    CHECK(opened);
    CHECK(calm->toKml() == asWritten(QStringLiteral("<kml><p>Calm.</p></kml>")));
}

TEST_CASE("Insert > Clipping: the list at the cursor, those not inserted first, Enter inserts",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    const TwoClippings cut = cutTwo(clips, book, open(window, clipped.storm()));
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(0, CALM_END));
    choose(Clippings::INSERT_SETTING, Clippings::INSERT_LIST);

    REQUIRE(clips.coordinator().insertAt(calm, cut.last));
    calm->setCursorPosition(at(0, 0));
    clips.coordinator().insert(calm);
    const gui::dialogs::ClippingListPopup* popup = popupOf(window);
    REQUIRE(popup != nullptr);
    CHECK(popup->isVisible());
    CHECK(popup->shownIds() == QStringList{cut.rained, cut.last});
    QLineEdit* search = searchOf(*popup);
    press(search, Qt::Key_Down);
    CHECK(popup->chosenId() == cut.last);
    press(search, Qt::Key_Up);
    CHECK(popup->chosenId() == cut.rained);
    press(search, Qt::Key_Return);
    CHECK(calm->textDocument()->firstBlock().text().startsWith(QStringLiteral("rained Calm.")));
    QCoreApplication::processEvents();
    CHECK(popupOf(window) == nullptr);
}

TEST_CASE("Insert > Clipping: the list at the cursor, Esc closes it, Alt+P restores",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* storm = open(window, clipped.storm());
    const TwoClippings cut = cutTwo(clips, book, storm);
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(0, CALM_END));
    choose(Clippings::INSERT_SETTING, Clippings::INSERT_LIST);

    clips.coordinator().insert(calm);
    REQUIRE(popupOf(window) != nullptr);
    press(searchOf(*popupOf(window)), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(popupOf(window) == nullptr);
    CHECK(calm->toKml() == asWritten(QStringLiteral("<kml><p>Calm.</p></kml>")));

    clips.coordinator().insert(calm);
    gui::dialogs::ClippingListPopup* popup = popupOf(window);
    REQUIRE(popup != nullptr);
    popup->setSearchText(QStringLiteral("rained"));
    CHECK(popup->shownIds() == QStringList{cut.rained});
    press(searchOf(*popup), Qt::Key_P, Qt::AltModifier);
    CHECK(storm->toKml() == asWritten(QStringLiteral(
                                "<kml><p>The storm came.</p><p>It rained <b>all</b> night."
                                "</p></kml>")));
}

TEST_CASE("Clippings: what cannot be done says why", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setCursorPosition(at(1, 4));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());

    // Without a text
    CHECK(clips.coordinator().cutAs(nullptr, QString(), QString()).isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The cursor is not in a text of the book or in a working note"));
    window.statusBar.clearMessage();
    clips.coordinator().insert(nullptr);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The cursor is not in a text of the book or in a working note"));
    CHECK_FALSE(clips.coordinator().insertAt(nullptr, clippingId));

    // A clipping no longer in the Workshop
    CHECK_FALSE(clips.coordinator().insertAt(text, QStringLiteral("gone")));
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The clipping is no longer in the Workshop"));
    window.statusBar.clearMessage();
    CHECK_FALSE(clips.coordinator().restore(QStringLiteral("gone")));
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The clipping is no longer in the Workshop"));

    // Nothing to cut in the small window: an empty paragraph
    save(book, clipped.calm(), QStringLiteral("<kml><p>Calm.</p><p> </p></kml>"));
    BookEditor* calm = open(window, clipped.calm());
    calm->setCursorPosition(at(1, 0));
    choose(Clippings::CUT_SETTING, Clippings::CUT_WITH_WINDOW);
    CHECK(clips.coordinator().cut(calm).isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The paragraph with the cursor is empty, and nothing is selected"));
    CHECK(clippingIds(book) == QStringList{clippingId});
}

TEST_CASE("Insert > Clipping: Open shows the clipping in its tab", "[gui][clippings]") {
    const ClippingBook clipped;
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setCursorPosition(at(1, 4));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());

    clips.coordinator().act(text, gui::ClippingChoice::Open, clippingId);
    BookEditor* clipping = openText(window, clippingId);
    REQUIRE(clipping != nullptr);
    // Its text is that of the tab, also before it is saved
    clipping->setCursorPosition(at(0, 0));
    clipping->insertText(QStringLiteral("Heavy "));
    CHECK(clips.coordinator().kmlOf(clippingId) == clipping->toKml());
    CHECK(clips.coordinator().kmlOf(clippingId).contains(QStringLiteral("Heavy ")));
}

TEST_CASE("Cut to Clippings: a text closed takes its Undo with it", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setCursorPosition(at(1, 4));
    REQUIRE_FALSE(clips.coordinator().cut(text).isEmpty());
    REQUIRE(clips.coordinator().canUndoLast());

    closeText(window, book, clipped.storm());
    CHECK_FALSE(clips.coordinator().canUndoLast());
    CHECK_FALSE(clips.coordinator().undoLast());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone from the notice: the text changed "
                         "since."));
    // The text opened again cuts and takes back as before
    BookEditor* again = open(window, clipped.storm());
    again->setCursorPosition(at(0, 0));
    const QString clippingId = clips.coordinator().cut(again);
    REQUIRE_FALSE(clippingId.isEmpty());
    again->undo();
    CHECK(book.pm.findElement(clippingId) == nullptr);
}

TEST_CASE("Clippings: without a book, the reason", "[gui][clippings]") {
    REQUIRE_FALSE(test::book().isProjectOpen());
    const ClippingsCoordinator coordinator(test::book(), test::commands(), nullptr, nullptr, {},
                                           {});
    CHECK(coordinator.whyNotCut(nullptr) == QStringLiteral("No book is open"));
    CHECK(coordinator.whyNotInsert(nullptr) == QStringLiteral("No book is open"));
    CHECK(coordinator.clippings(true).isEmpty());
}

TEST_CASE("Clippings: without a text of the book, the reason", "[gui][clippings]") {
    const ClippingBook clipped;
    Window window;
    Clips clips(window);
    CHECK(clips.coordinator().whyNotCut(nullptr).toStdString() ==
          "The cursor is not in a text of the book or in a working note");
    BookEditor outside;
    outside.fromKml(stormKml());
    CHECK(clips.coordinator().whyNotCut(&outside).toStdString() ==
          "The cursor is not in a text of the book or in a working note");
}

TEST_CASE("Clippings: a text that cannot change, the reason", "[gui][clippings]") {
    const ClippingBook clipped;
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setCursorPosition(at(1, 0));

    text->setReadOnly(true);
    CHECK(clips.coordinator().whyNotCut(text) == gui::readOnlyReason());
    CHECK(clips.coordinator().whyNotInsert(text) == gui::readOnlyReason());
    text->setReadOnly(false);
    CHECK(clips.coordinator().whyNotCut(text).isEmpty());
}

TEST_CASE("Clippings: a working note of the Workshop can be cut from", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);

    const core::KindRef note = book.pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                            QStringLiteral("work_note"));
    REQUIRE(note);
    const QString ideas =
        book.pm.addElement(note, QStringLiteral("Ideas"), core::BookPlace::Workshop);
    REQUIRE_FALSE(ideas.isEmpty());
    save(book, ideas, QStringLiteral("<kml><p>One.</p><p>Two.</p></kml>"));
    BookEditor* workshop = open(window, ideas);
    workshop->setCursorPosition(at(1, 0));
    CHECK(clips.coordinator().whyNotCut(workshop).isEmpty());
    const QString clippingId = clips.coordinator().cut(workshop);
    REQUIRE_FALSE(clippingId.isEmpty());
    CHECK(dataOf(book, clippingId).origin.element == ideas);
}

TEST_CASE("Clippings: a clipping is not cut from again; nothing to insert until a cut",
          "[gui][clippings]") {
    const ClippingBook clipped;
    Window window;
    Clips clips(window);
    BookEditor* text = open(window, clipped.storm());
    text->setCursorPosition(at(1, 0));

    CHECK(clips.coordinator().whyNotInsert(text) ==
          QStringLiteral("The book has no clippings yet (Edit > Cut to Clippings)"));
    const QString clippingId = clips.coordinator().cut(text);
    REQUIRE_FALSE(clippingId.isEmpty());
    CHECK(clips.coordinator().whyNotInsert(text).isEmpty());
    BookEditor* clipping = open(window, clippingId);
    clipping->setCursorPosition(at(0, 1));
    CHECK(clips.coordinator().whyNotCut(clipping).toStdString() ==
          "This text is a clipping already");
}

TEST_CASE("Clippings: the commands and their settings", "[gui][clippings]") {
    const ClippingBook clipped;
    Window window;
    BookEditor* text = open(window, clipped.storm());

    // The defaults: at once, the paragraph, the window, kept as inserted, the words lowered
    core::SettingsManager::getInstance().resetToDefaults();
    CHECK(chosen(Clippings::CUT_SETTING) == Clippings::CUT_AT_ONCE);
    CHECK(chosen(Clippings::NO_SELECTION_SETTING) == Clippings::NO_SELECTION_PARAGRAPH);
    CHECK(chosen(Clippings::INSERT_SETTING) == Clippings::INSERT_WINDOW);
    CHECK(chosen(Clippings::AFTER_INSERT_SETTING) == Clippings::AFTER_INSERT_KEEP);
    CHECK_FALSE(core::SettingsManager::getInstance().get<bool>(Clippings::KEEP_WORDS_SETTING));

    // The commands of a registry of their own run on the text with the keys
    gui::CommandRegistry commands;
    addClippingCommands(commands);
    gui::Command undo;
    undo.id = "edit.undo";
    undo.shortcut = gui::KeyboardShortcut(Qt::Key_Z, Qt::ControlModifier);
    commands.registerCommand(undo);
    Keys keys;
    ClippingsCoordinator coordinator(
        test::book(), commands, window.tabs, &window.navigator, keys.get(),
        ClippingsCoordinator::Feedback{
            .showNotice = {}, .statusBar = &window.statusBar, .window = &window.window});
    coordinator.connectCommands();
    keys.set(text);
    text->setCursorPosition(at(1, 4));
    gui::Command* cut = commands.getCommand(ClippingsCoordinator::CUT_COMMAND);
    REQUIRE(cut != nullptr);
    REQUIRE(cut->disabledReason);
    CHECK(cut->disabledReason().isEmpty());
    CHECK(coordinator.canCut(*text));
    CHECK_FALSE(coordinator.canInsert(*text));
    cut->execute();
    CHECK(text->toKml() ==
          asWritten(QStringLiteral("<kml><p>The storm came.</p><p>The end.</p></kml>")));
    // Without the notices under the texts the status bar says it, with the keys of Undo
    CHECK(window.statusBar.currentMessage().endsWith(
        QStringLiteral("%1 takes the cut back.")
            .arg(QKeySequence(Qt::CTRL | Qt::Key_Z).toString(QKeySequence::NativeText))));
    CHECK(coordinator.canInsert(*text));
    const gui::Command* insert = commands.getCommand(ClippingsCoordinator::INSERT_COMMAND);
    REQUIRE(insert != nullptr);
    REQUIRE(insert->disabledReason);
    CHECK(insert->disabledReason().isEmpty());
    keys.set(nullptr);
    CHECK(cut->disabledReason() ==
          QStringLiteral("The cursor is not in a text of the book or in a working note"));
}

TEST_CASE("Settings > Editor > Clippings: the choices, written only when changed",
          "[gui][settings][clippings]") {
    gui::CommandRegistry commands;
    gui::Command cut;
    cut.id = ClippingsCoordinator::CUT_COMMAND;
    cut.shortcut = gui::KeyboardShortcut(Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier);
    commands.registerCommand(cut);
    gui::EditorClippingsPage page(commands);
    page.load();
    CHECK_FALSE(page.isChanged());

    // What they are, with the keys of the commands that have them
    auto* intro = page.findChild<QLabel*>(QStringLiteral("clippingsIntro"));
    REQUIRE(intro != nullptr);
    // The keys as the system names them (on macOS with its symbols)
    const auto keysText = [](const gui::KeyboardShortcut& keys) {
        return keys.toQKeySequence().toString(QKeySequence::NativeText);
    };
    CHECK(intro->text().contains(
        QStringLiteral("(Edit > Cut to Clippings, %1)").arg(keysText(cut.shortcut))));
    CHECK(intro->text().contains(QStringLiteral("(Insert > Clipping from the Workshop)")));

    // The defaults
    CHECK(shownIn(page, "clippingsCut") == Clippings::CUT_AT_ONCE);
    CHECK(shownIn(page, "clippingsNoSelection") == Clippings::NO_SELECTION_PARAGRAPH);
    CHECK(shownIn(page, "clippingsInsert") == Clippings::INSERT_WINDOW);
    CHECK(shownIn(page, "clippingsAfterInsert") == Clippings::AFTER_INSERT_KEEP);

    // The alternatives
    pick(page, "clippingsCut", Clippings::CUT_WITH_WINDOW);
    pick(page, "clippingsNoSelection", Clippings::NO_SELECTION_NOTHING);
    pick(page, "clippingsInsert", Clippings::INSERT_LIST);
    pick(page, "clippingsAfterInsert", Clippings::AFTER_INSERT_REMOVE);
    CHECK(page.isChanged());
    std::vector<std::string> keys = page.apply();
    std::ranges::sort(keys);
    CHECK(keys == std::vector<std::string>{Clippings::AFTER_INSERT_SETTING, Clippings::CUT_SETTING,
                                           Clippings::INSERT_SETTING,
                                           Clippings::NO_SELECTION_SETTING});
    CHECK(chosen(Clippings::CUT_SETTING) == Clippings::CUT_WITH_WINDOW);
    CHECK(chosen(Clippings::NO_SELECTION_SETTING) == Clippings::NO_SELECTION_NOTHING);
    CHECK(chosen(Clippings::INSERT_SETTING) == Clippings::INSERT_LIST);
    CHECK(chosen(Clippings::AFTER_INSERT_SETTING) == Clippings::AFTER_INSERT_REMOVE);

    // The keys changed: the text follows
    const gui::KeyboardShortcut changed(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    commands.getCommand(ClippingsCoordinator::CUT_COMMAND)->shortcut = changed;
    emit commands.shortcutsChanged();
    CHECK(intro->text().contains(
        QStringLiteral("(Edit > Cut to Clippings, %1)").arg(keysText(changed))));

    // A value the page does not know shows the first choice
    choose(Clippings::CUT_SETTING, "unknown");
    gui::EditorClippingsPage other(commands);
    other.load();
    CHECK(shownIn(other, "clippingsCut") == Clippings::CUT_AT_ONCE);
    choose(Clippings::CUT_SETTING, Clippings::CUT_AT_ONCE);

    // The scissors beside the paragraphs: off by default, written when turned on
    auto* marks = page.findChild<QCheckBox*>(QStringLiteral("clippingsMarks"));
    REQUIRE(marks != nullptr);
    CHECK(marks->text() == QStringLiteral("&Draw scissors where a fragment was cut"));
    page.load();
    CHECK_FALSE(marks->isChecked());
    marks->setChecked(true);
    CHECK(page.apply() == std::vector<std::string>{Clippings::MARKS_SETTING});
    CHECK(core::SettingsManager::getInstance().get<bool>(Clippings::MARKS_SETTING, false));
    useDefaultSettings();

    // The Settings dialog opens at the page (the gears of the windows)
    gui::SettingsDialog dialog(test::book(), commands, nullptr);
    CHECK(dialog.openPage(QString::fromLatin1(ClippingsCoordinator::SETTINGS_PAGE)));
}

namespace {

/// The list of the clippings (MaterialList)
QString clippingsList() {
    return QStringLiteral("kalahari.base:") + QString::fromLatin1(Clippings::KIND);
}

/// The ids of @p columns
QStringList idsOf(const QList<gui::MaterialColumn>& columns) {
    QStringList ids;
    for (const gui::MaterialColumn& column : columns) {
        ids << column.id;
    }
    return ids;
}

/// The action of @p menu with text @p text; null: none
QAction* actionOf(const QMenu& menu, const QString& text) {
    const QList<QAction*> actions = menu.actions();
    const auto found = std::ranges::find_if(
        actions, [&text](const QAction* action) { return action->text() == text; });
    return found == actions.end() ? nullptr : *found;
}

/// The program's opener of the Settings dialog for the time of a test: the pages it opens
class Opener {
public:
    Opener() {
        gui::dialogs::KalahariDialog::setSettingsOpener(
            [this](QWidget* /*over*/, const QString& page) { m_pages << page; });
    }
    ~Opener() { gui::dialogs::KalahariDialog::setSettingsOpener(nullptr); }
    Opener(const Opener&) = delete;
    Opener& operator=(const Opener&) = delete;
    Opener(Opener&&) = delete;
    Opener& operator=(Opener&&) = delete;

    /// The pages opened
    [[nodiscard]] const QStringList& pages() const { return m_pages; }

private:
    QStringList m_pages;
};

/// The tabs and the panels of a window with the lists of the materials, connected to the
/// clippings as the main window connects them
struct ClippingLists {
    Window window;
    Clips clips{window};
    gui::MaterialListCoordinator lists{test::book(), window.tabs, &window.navigator,
                                       &window.statusBar, [](bool /*keys*/) {}};

    ClippingLists() { clips.coordinator().connectPanels(window.properties, &lists); }
};

}  // namespace

TEST_CASE("Clippings list: the columns, the rows and the order", "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    BookEditor* storm = open(window, clipped.storm());
    storm->setSelection(rained());
    const QString clippingId =
        clips.coordinator().cutAs(storm, QString(), QStringLiteral("Too  wet."));
    REQUIRE_FALSE(clippingId.isEmpty());
    const QString list = clippingsList();
    using gui::MaterialList;

    // Its columns, the time of the cut first, the last cut on top
    CHECK(MaterialList::isClippings(list));
    CHECK_FALSE(MaterialList::isClippings(QString()));
    const QList<gui::MaterialColumn> columns = MaterialList::columnsOf(book.pm, list);
    CHECK(idsOf(columns) == QStringList{QStringLiteral("title"), QStringLiteral("from"),
                                        QStringLiteral("cut"), QStringLiteral("words"),
                                        QStringLiteral("note"), QStringLiteral("inserted"),
                                        QStringLiteral("modified"), QStringLiteral("links")});
    CHECK(MaterialList::shownColumns(book.pm, list) ==
          QStringList{QStringLiteral("title"), QStringLiteral("from"), QStringLiteral("cut"),
                      QStringLiteral("words"), QStringLiteral("note"),
                      QStringLiteral("inserted")});
    CHECK(MaterialList::firstSortOf(list) == QStringLiteral("cut"));
    CHECK(MaterialList::firstSortOrderOf(list) == Qt::DescendingOrder);
    CHECK(MaterialList::firstSortOrderOf(QString()) == Qt::AscendingOrder);
    // Inserted is shown at first only where a clipping inserted stays in the Workshop
    choose(Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_REMOVE);
    CHECK_FALSE(MaterialList::shownColumns(book.pm, list).contains(QStringLiteral("inserted")));
    useDefaultSettings();

    // Its row: where it comes from, when it was cut, its note
    QList<gui::MaterialRow> rows = MaterialList::rowsOf(book.pm, list);
    REQUIRE(rows.size() == 1);
    const gui::MaterialRow& row = rows.first();
    CHECK(row.clipping);
    CHECK(row.originLive);
    CHECK(row.texts.value(QStringLiteral("from")) == QStringLiteral("The Storm"));
    CHECK_FALSE(row.texts.value(QStringLiteral("cut")).isEmpty());
    CHECK(row.order.value(QStringLiteral("cut")).typeId() == QMetaType::QDateTime);
    CHECK(row.texts.value(QStringLiteral("note")) == QStringLiteral("Too wet."));
    CHECK_FALSE(row.texts.contains(QStringLiteral("inserted")));
    CHECK(row.search.contains(QStringLiteral("the storm")));

    // Inserted: when
    REQUIRE(clips.coordinator().insertAt(open(window, clipped.calm()), clippingId));
    rows = MaterialList::rowsOf(book.pm, list);
    REQUIRE(rows.size() == 1);
    CHECK_FALSE(rows.first().texts.value(QStringLiteral("inserted")).isEmpty());
    CHECK(rows.first().order.value(QStringLiteral("inserted")).typeId() == QMetaType::QDateTime);

    // Another material is not a clipping
    const QString byHand = book.pm.addClipping(
        QStringLiteral("Loose"), QStringLiteral("<kml><p>Loose.</p></kml>"), core::ClippingData());
    rows = MaterialList::rowsOf(book.pm, list);
    REQUIRE(rows.size() == 2);
    for (const gui::MaterialRow& each : rows) {
        CHECK(each.clipping);
        CHECK(each.originLive == (each.id == clippingId));
    }
    for (const gui::MaterialRow& note :
         MaterialList::rowsOf(book.pm, QStringLiteral("kalahari.base:work_note"))) {
        CHECK_FALSE(note.clipping);
    }
    CHECK_FALSE(byHand.isEmpty());
}

TEST_CASE("Clippings: what the lists and the Properties say of a clipping", "[gui][clippings]") {
    using gui::ClippingTexts;
    core::ClippingOrigin origin;
    CHECK(ClippingTexts::placeText(origin).isEmpty());
    origin.element = QStringLiteral("gone");
    origin.title = QStringLiteral("Prologue");
    CHECK(ClippingTexts::placeText(origin).isEmpty());  // no paragraph
    origin.paragraph = 3;
    CHECK(ClippingTexts::placeText(origin) == QStringLiteral("the start of paragraph 3"));
    origin.before = QStringLiteral("one two  three four five ");
    CHECK(ClippingTexts::placeText(origin) ==
          QStringLiteral("paragraph 3, after \"…two three four five\""));
    CHECK(ClippingTexts::lastWords(QStringLiteral(" a b "), 4) == QStringLiteral("a b"));

    core::ClippingData data;
    CHECK(ClippingTexts::insertedText(data).isEmpty());
    data.inserted.append({.element = QStringLiteral("x"),
                          .at = QDateTime::currentDateTimeUtc(),
                          .extra = {}});
    CHECK_FALSE(ClippingTexts::insertedText(data).isEmpty());

    // Where it comes from, and what Restore to Its Place does
    CHECK(ClippingTexts::restoreToolTip({}) ==
          QStringLiteral("This clipping was added by hand: it has no place it came from"));
    CHECK(ClippingTexts::restoreToolTip({.title = QStringLiteral("A (deleted)"), .live = false}) ==
          QStringLiteral("The text it was cut from is no longer in the book"));
    CHECK(ClippingTexts::restoreToolTip({.title = QStringLiteral("A"), .live = true}) ==
          QStringLiteral("Insert it where it was cut from, in \"A\""));
    const OpenNovel book(QStringLiteral("Texts"));
    data.origin = origin;
    const gui::ClippingFrom gone = ClippingTexts::fromOf(book.pm, data);
    CHECK(gone.title == QStringLiteral("Prologue (deleted)"));
    CHECK_FALSE(gone.live);
    data.origin.element = book.chapterId;
    const gui::ClippingFrom live = ClippingTexts::fromOf(book.pm, data);
    CHECK(live.title == QStringLiteral("The Storm"));
    CHECK(live.live);
    CHECK(ClippingTexts::fromOf(book.pm, core::ClippingData()).title.isEmpty());
}

TEST_CASE("Clippings list: Restore to Its Place, Go to Its Place and the gear",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    ClippingLists window;
    Clips& clips = window.clips;
    BookEditor* storm = open(window.window, clipped.storm());
    storm->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(clippingId.isEmpty());
    open(window.window, clipped.calm());  // another text in front

    gui::MaterialListPanel* panel = window.lists.openList(clippingsList(), true);
    REQUIRE(panel != nullptr);
    REQUIRE(panel->selectMaterial(clippingId));
    QToolButton* restore = panel->restoreButton();
    CHECK_FALSE(restore->isHidden());
    CHECK(restore->text() == QStringLiteral("Restore to Its Place"));
    CHECK(restore->isEnabled());
    CHECK(restore->toolTip() ==
          QStringLiteral("Insert it where it was cut from, in \"The Storm\""));
    CHECK(panel->settingsButton()->toolTip() == QStringLiteral("Clipping Settings"));

    // The menu of its row
    {
        QMenu menu;
        panel->fillMenu(menu);
        QAction* go = actionOf(menu, QStringLiteral("&Go to Its Place"));
        REQUIRE(go != nullptr);
        REQUIRE(actionOf(menu, QStringLiteral("Restore to Its &Place")) != nullptr);
        CHECK(go->isEnabled());
        go->trigger();
    }
    CHECK(window.window.tabs->currentWidget()->property("elementId").toString() ==
          clipped.storm());
    CHECK(storm->cursorPosition() == at(1, RAINED_FIRST));

    // Restore to Its Place: the clipping is in its place again
    open(window.window, clipped.calm());
    REQUIRE(panel->selectMaterial(clippingId));
    restore->click();
    CHECK(storm->toKml() == asWritten(stormKml()));
    CHECK(dataOf(book, clippingId).inserted.size() == 1);

    // The gear: Settings > Editor > Clippings; without the program's opener, the settings of
    // the materials
    int materialSettings = 0;
    QObject::connect(&window.lists, &gui::MaterialListCoordinator::settingsRequested,
                     [&materialSettings]() { ++materialSettings; });
    {
        const Opener opener;
        panel->settingsButton()->click();
        CHECK(opener.pages() ==
              QStringList{QString::fromLatin1(gui::SettingsDialog::CLIPPINGS_PAGE)});
        CHECK(materialSettings == 0);
    }
    panel->settingsButton()->click();
    CHECK(materialSettings == 1);

    // A clipping added by hand has no place to go to
    const QString byHand = book.pm.addClipping(
        QStringLiteral("Loose"), QStringLiteral("<kml><p>Loose.</p></kml>"), core::ClippingData());
    REQUIRE_FALSE(byHand.isEmpty());
    panel->refresh();
    REQUIRE(panel->selectMaterial(byHand));
    CHECK_FALSE(restore->isEnabled());
    CHECK(restore->toolTip() ==
          QStringLiteral("This clipping was added by hand: it has no place it came from"));
    {
        QMenu menu;
        panel->fillMenu(menu);
        const QAction* go = actionOf(menu, QStringLiteral("&Go to Its Place"));
        REQUIRE(go != nullptr);
        CHECK_FALSE(go->isEnabled());
    }

    // Another list has neither the button nor the commands
    panel->setList(QStringLiteral("kalahari.base:work_note"));
    CHECK(restore->isHidden());
    CHECK(panel->settingsButton()->toolTip() == QStringLiteral("Material Settings"));
    {
        const Opener opener;
        panel->settingsButton()->click();
        CHECK(opener.pages().isEmpty());
        CHECK(materialSettings == 2);
    }
}

TEST_CASE("Properties: the origin of a clipping, Go to Its Place and Restore to Its Place",
          "[gui][clippings]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    ClippingLists window;
    Clips& clips = window.clips;
    BookEditor* storm = open(window.window, clipped.storm());
    storm->setSelection(rained());
    const QString clippingId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(clippingId.isEmpty());
    open(window.window, clipped.calm());

    gui::PropertiesPanel* properties = window.window.properties;
    properties->showChapterProperties(clippingId);
    const gui::ClippingOriginBox* origin = properties->clippingOrigin();
    REQUIRE(origin != nullptr);
    CHECK_FALSE(origin->isHidden());
    CHECK(origin->clippingId() == clippingId);
    CHECK(origin->title() == QStringLiteral("Origin"));
    CHECK(origin->fromLabel()->text() == QStringLiteral("<a href=\"origin\">The Storm</a>"));
    CHECK(origin->placeLabel()->text() == QStringLiteral("paragraph 2, after \"It\""));
    CHECK_FALSE(origin->cutLabel()->text().isEmpty());
    CHECK(origin->insertedLabel()->text() == QStringLiteral("no"));
    CHECK_FALSE(origin->insertedLabel()->isHidden());
    CHECK(origin->goButton()->isEnabled());
    CHECK(origin->goButton()->toolTip() ==
          QStringLiteral("Open \"The Storm\" at the place it was cut from"));
    CHECK(origin->restoreButton()->isEnabled());

    // The link and Go to Its Place: the text at the place
    emit origin->fromLabel()->linkActivated(QStringLiteral("origin"));
    CHECK(window.window.tabs->currentWidget()->property("elementId").toString() ==
          clipped.storm());
    open(window.window, clipped.calm());
    origin->goButton()->click();
    CHECK(window.window.tabs->currentWidget()->property("elementId").toString() ==
          clipped.storm());
    CHECK(storm->cursorPosition() == at(1, RAINED_FIRST));

    // Restore to Its Place, then it says when it was inserted
    origin->restoreButton()->click();
    CHECK(storm->toKml() == asWritten(stormKml()));
    properties->showChapterProperties(clippingId);
    CHECK(origin->insertedLabel()->text() != QStringLiteral("no"));

    // With a clipping inserted that goes, Inserted is shown only once it was inserted
    choose(Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_REMOVE);
    const QString byHand = book.pm.addClipping(
        QStringLiteral("Loose"), QStringLiteral("<kml><p>Loose.</p></kml>"), core::ClippingData());
    REQUIRE_FALSE(byHand.isEmpty());
    properties->showChapterProperties(byHand);
    CHECK(origin->fromLabel()->text() == QStringLiteral("added by hand"));
    CHECK(origin->placeLabel()->isHidden());
    CHECK(origin->cutLabel()->isHidden());
    CHECK(origin->insertedLabel()->isHidden());
    CHECK_FALSE(origin->goButton()->isEnabled());
    CHECK_FALSE(origin->restoreButton()->isEnabled());
    useDefaultSettings();

    // The text it came from deleted: its title then
    const std::optional<core::ProjectElement> removed = book.pm.removeElement(clipped.storm());
    REQUIRE(removed.has_value());
    window.window.navigator.closeTabsOf(removed.value_or(core::ProjectElement()));
    QCoreApplication::processEvents();
    properties->showChapterProperties(clippingId);
    CHECK(origin->fromLabel()->text() == QStringLiteral("The Storm (deleted)"));
    CHECK_FALSE(origin->goButton()->isEnabled());
    CHECK(origin->goButton()->toolTip() ==
          QStringLiteral("The text it was cut from is no longer in the book"));

    // Another element has no origin
    properties->showChapterProperties(clipped.calm());
    CHECK(origin->isHidden());
    CHECK(origin->clippingId().isEmpty());
}

namespace {

/// The scissors beside the paragraphs of @p text
QList<editor::ParagraphMark> scissorsOf(const BookEditor& text) {
    QList<editor::ParagraphMark> scissors;
    for (const editor::ParagraphMark& mark : text.paragraphMarks()) {
        if (mark.kind == editor::ParagraphMarkKind::Clippings) {
            scissors << mark;
        }
    }
    return scissors;
}

/// The events waiting now, and those they post, done
void settle() {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::processEvents();
    }
}

}  // namespace

TEST_CASE("Clippings: the scissors beside the paragraph a clipping was cut from, when the "
          "settings say so",
          "[gui][clippings][marks]") {
    ClippingBook clipped;
    OpenNovel& book = clipped.book();
    Window window;
    Clips clips(window);
    QStringList shown;
    gui::ParagraphMarksCoordinator marks(test::book(), window.tabs,
                                         [&shown](const QString& id) { shown << id; });
    clips.coordinator().connectMarks(marks);
    BookEditor* storm = open(window, clipped.storm());
    storm->setSelection(rained());
    const QString rainedId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(rainedId.isEmpty());
    settle();

    // Off by default: none
    CHECK(scissorsOf(*storm).isEmpty());
    CHECK(gui::ParagraphMarksCoordinator::clippingMarksOf(book.pm, clipped.storm(), *storm)
              .isEmpty());

    // Turned on: beside the paragraph it was cut from, with its words and when it was cut
    core::SettingsManager::getInstance().set<bool>(Clippings::MARKS_SETTING, true);
    settle();
    QList<editor::ParagraphMark> scissors = scissorsOf(*storm);
    REQUIRE(scissors.size() == 1);
    CHECK(scissors.first().paragraph == 1);
    CHECK(scissors.first().ids == QStringList{rainedId});
    REQUIRE(scissors.first().titles.size() == 1);
    const QString title = scissors.first().titles.first();
    CAPTURE(title.toStdString());
    CHECK(title.startsWith(QStringLiteral("\"rained\" (1 word")));
    CHECK(title.contains(QStringLiteral(", today, ")));
    // Another text, and a text of a file of its own, have none
    const BookEditor* calm = open(window, clipped.calm());
    settle();
    CHECK(scissorsOf(*calm).isEmpty());
    CHECK(gui::ParagraphMarksCoordinator::clippingMarksOf(book.pm, QString(), *storm).isEmpty());

    // A second cut from the paragraph: one mark naming both, the newest first; a cut from
    // another paragraph has its own; a clipping added by hand has none, one without the time
    // of its cut says its words alone
    storm = open(window, clipped.storm());
    storm->setSelection({.start = at(1, 0), .end = at(1, 2)});  // "It"
    const QString itId = clips.coordinator().cut(storm);
    REQUIRE_FALSE(itId.isEmpty());
    storm->setSelection({.start = at(0, 4), .end = at(0, 9)});
    const QString stormId = clips.coordinator().cut(storm);  // "storm"
    REQUIRE_FALSE(stormId.isEmpty());
    REQUIRE_FALSE(book.pm.addClipping(QStringLiteral("Loose"),
                                      QStringLiteral("<kml><p>Loose.</p></kml>"),
                                      core::ClippingData())
                      .isEmpty());
    core::ClippingData untimed;
    untimed.origin = {.element = clipped.storm(),
                      .title = QStringLiteral("The Storm"),
                      .paragraph = 1,
                      .before = {},
                      .after = {},
                      .cut = {},
                      .extra = {}};
    const QString untimedId = book.pm.addClipping(
        QStringLiteral("Untimed"), QStringLiteral("<kml><p>Thunder.</p></kml>"), untimed);
    REQUIRE_FALSE(untimedId.isEmpty());
    marks.refresh();  // added to the book, not shown in the Navigator yet
    scissors = scissorsOf(*storm);
    REQUIRE(scissors.size() == 2);
    CHECK(scissors.at(0).paragraph == 0);
    CHECK(scissors.at(0).ids == (QStringList{stormId, untimedId}));
    REQUIRE(scissors.at(0).titles.size() == 2);
    CHECK(scissors.at(0).titles.at(1).startsWith(QStringLiteral("\"Untimed\" (1 word")));
    CHECK_FALSE(scissors.at(0).titles.at(1).contains(QStringLiteral("today")));
    CHECK(scissors.at(1).paragraph == 1);
    CHECK(scissors.at(1).ids == (QStringList{itId, rainedId}));

    // Show Clipping shows it in the Properties panel; Restore Here puts it back, and its
    // scissors go: it is inserted
    emit storm->paragraphMarkClicked(rainedId);
    CHECK(shown == QStringList{rainedId});
    storm->undo();  // "storm" back
    storm->undo();  // "It" back
    settle();
    emit storm->clippingRestoreRequested(rainedId);
    settle();
    CHECK(storm->toKml() == asWritten(stormKml()));
    for (const editor::ParagraphMark& mark : scissorsOf(*storm)) {
        CHECK_FALSE(mark.ids.contains(rainedId));
    }

    // Turned off: none
    core::SettingsManager::getInstance().set<bool>(Clippings::MARKS_SETTING, false);
    settle();
    CHECK(scissorsOf(*storm).isEmpty());
    useDefaultSettings();
}
