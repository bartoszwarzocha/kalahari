/// @file test_edit_coordinator.cpp
/// @brief Undo, Redo, Cut, Copy, Paste and Select All work on the element with the keys
///        (EditCoordinator)

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/panels/annotations_panel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <array>
#include <string>
#include <utility>

using namespace kalahari;
using namespace kalahari::gui;
using Edit = EditCoordinator::Edit;

namespace {

/// The commands, in the order of the Edit menu, with their names and ids
constexpr std::array<std::pair<Edit, const char*>, 6> NAMES = {{
    {Edit::Undo, "Undo"},
    {Edit::Redo, "Redo"},
    {Edit::Cut, "Cut"},
    {Edit::Copy, "Copy"},
    {Edit::Paste, "Paste"},
    {Edit::SelectAll, "SelectAll"},
}};

constexpr std::array<const char*, 6> IDS = {"edit.undo",  "edit.redo",  "edit.cut",
                                            "edit.copy",  "edit.paste", "edit.selectAll"};

/// The commands that can do something on an element, in the order of the Edit menu
std::string possibleOn(const EditCoordinator& coordinator, QWidget* element) {
    std::string possible;
    for (const auto& [edit, name] : NAMES) {
        if (coordinator.canDo(edit, element)) {
            possible += possible.empty() ? std::string(name) : std::string(" ") + name;
        }
    }
    return possible;
}

/// The commands of the window that are enabled, in the order of the Edit menu
std::string enabledCommands() {
    std::string enabled;
    for (std::size_t index = 0; index < IDS.size(); ++index) {
        const QAction* action =
            CommandRegistry::getInstance().getAction(QString::fromLatin1(IDS[index]));
        if (action != nullptr && action->isEnabled()) {
            const char* name = NAMES[index].second;
            enabled += enabled.empty() ? std::string(name) : std::string(" ") + name;
        }
    }
    return enabled;
}

std::string clipboardText() {
    return QGuiApplication::clipboard()->text().toStdString();
}

/// No book in front: the Annotations panel changes no text
editor::BookEditor* noBook() {
    return nullptr;
}

/// A key pressed and released in a widget, as the keyboard does it: the window's shortcuts
/// first
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// Whether a widget has the keys, after the events that give them
bool takesKeys(QWidget* widget) {
    widget->setFocus(Qt::OtherFocusReason);
    return test::waitUntil([widget] { return QApplication::focusWidget() == widget; });
}

/// The window's tests need an active window; the offscreen platform has one without a window
/// on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

void activate(QMainWindow& window) {
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
}

}  // namespace

// =============================================================================
// What the commands do on each element
// =============================================================================

TEST_CASE("Edit commands: a read-only field copies and selects all, and changes nothing",
          "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));

    SECTION("the Log: a text that only shows") {
        QTextEdit log;
        log.setPlainText(QStringLiteral("Log line"));
        log.setReadOnly(true);
        CHECK(possibleOn(coordinator, &log) == "SelectAll");

        CHECK(coordinator.execute(Edit::SelectAll, &log));
        CHECK(log.textCursor().selectedText() == QStringLiteral("Log line"));
        CHECK(possibleOn(coordinator, &log) == "Copy SelectAll");
        CHECK_FALSE(coordinator.execute(Edit::Paste, &log));
        CHECK_FALSE(coordinator.execute(Edit::Cut, &log));
        CHECK_FALSE(coordinator.execute(Edit::Undo, &log));
        CHECK(log.toPlainText() == QStringLiteral("Log line"));
        CHECK(clipboardText() == "Pasted");

        CHECK(coordinator.execute(Edit::Copy, &log));
        CHECK(clipboardText() == "Log line");
    }

    SECTION("a plain text that only shows") {
        QPlainTextEdit text;
        text.setPlainText(QStringLiteral("Plain"));
        text.setReadOnly(true);
        text.selectAll();
        CHECK(possibleOn(coordinator, &text) == "Copy SelectAll");
        CHECK_FALSE(coordinator.execute(Edit::Paste, &text));
        CHECK(text.toPlainText() == QStringLiteral("Plain"));
        CHECK(coordinator.execute(Edit::Copy, &text));
        CHECK(clipboardText() == "Plain");
    }

    SECTION("a field of one line that only shows") {
        QLineEdit line(QStringLiteral("Shown"));
        line.setReadOnly(true);
        CHECK(possibleOn(coordinator, &line) == "SelectAll");
        line.selectAll();
        CHECK(possibleOn(coordinator, &line) == "Copy SelectAll");
        CHECK_FALSE(coordinator.execute(Edit::Paste, &line));
        CHECK_FALSE(coordinator.execute(Edit::Cut, &line));
        CHECK(line.text() == QStringLiteral("Shown"));
        CHECK(coordinator.execute(Edit::Copy, &line));
        CHECK(clipboardText() == "Shown");
    }
}

TEST_CASE("Edit commands: a field one types into does them all", "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));

    SECTION("a field of one line") {
        QLineEdit line;
        CHECK(possibleOn(coordinator, &line) == "Paste");
        line.insert(QStringLiteral("abc"));
        line.selectAll();
        CHECK(possibleOn(coordinator, &line) == "Undo Cut Copy Paste SelectAll");

        CHECK(coordinator.execute(Edit::Cut, &line));
        CHECK(line.text().isEmpty());
        CHECK(clipboardText() == "abc");
        CHECK(coordinator.execute(Edit::Undo, &line));
        CHECK(line.text() == QStringLiteral("abc"));
        CHECK(possibleOn(coordinator, &line).find("Redo") != std::string::npos);
        CHECK(coordinator.execute(Edit::Redo, &line));
        CHECK(line.text().isEmpty());
        CHECK(coordinator.execute(Edit::Paste, &line));
        CHECK(line.text() == QStringLiteral("abc"));
    }

    SECTION("a text") {
        QTextEdit text;
        text.insertPlainText(QStringLiteral("abc"));
        text.selectAll();
        CHECK(possibleOn(coordinator, &text) == "Undo Cut Copy Paste SelectAll");
        CHECK(coordinator.execute(Edit::Cut, &text));
        CHECK(text.toPlainText().isEmpty());
        CHECK(coordinator.execute(Edit::Undo, &text));
        CHECK(text.toPlainText() == QStringLiteral("abc"));
        CHECK(coordinator.execute(Edit::Redo, &text));
        CHECK(text.toPlainText().isEmpty());
        CHECK(coordinator.execute(Edit::Paste, &text));
        CHECK(text.toPlainText() == QStringLiteral("abc"));
    }

    SECTION("a text one types into without its undo history") {
        QPlainTextEdit text;
        text.setUndoRedoEnabled(false);
        text.insertPlainText(QStringLiteral("abc"));
        CHECK(possibleOn(coordinator, &text) == "Paste SelectAll");
    }

    SECTION("a password copies nothing") {
        QLineEdit password(QStringLiteral("secret"));
        password.setEchoMode(QLineEdit::Password);
        password.selectAll();
        CHECK(possibleOn(coordinator, &password) == "Paste SelectAll");
        CHECK_FALSE(coordinator.execute(Edit::Copy, &password));
        CHECK(clipboardText() == "Pasted");
    }
}

TEST_CASE("Edit commands: the field of a spin box and of a combo box one types into",
          "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    QGuiApplication::clipboard()->setText(QStringLiteral("24"));

    SECTION("a spin box: the keys are in the box, the text is in its field") {
        QSpinBox spin;
        spin.setValue(12);
        CHECK(possibleOn(coordinator, &spin) == "Paste SelectAll");
        CHECK(coordinator.execute(Edit::SelectAll, &spin));
        CHECK(possibleOn(coordinator, &spin) == "Cut Copy Paste SelectAll");
        CHECK(coordinator.execute(Edit::Paste, &spin));
        CHECK(spin.text() == QStringLiteral("24"));
    }

    SECTION("a combo box one types into") {
        QComboBox combo;
        combo.setEditable(true);
        combo.setEditText(QStringLiteral("Serif"));
        CHECK(possibleOn(coordinator, &combo) == "Paste SelectAll");
        CHECK(coordinator.execute(Edit::SelectAll, &combo));
        CHECK(coordinator.execute(Edit::Copy, &combo));
        CHECK(clipboardText() == "Serif");
    }

    SECTION("a combo box one only chooses in") {
        QComboBox combo;
        combo.addItem(QStringLiteral("Serif"));
        CHECK(possibleOn(coordinator, &combo).empty());
    }
}

TEST_CASE("Edit commands: the book's text", "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    editor::BookEditor book;
    test::resizeWidget(book, QSize(600, 400));
    book.fromKml(test::kmlOf({QStringLiteral("One two")}));
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));
    CHECK(possibleOn(coordinator, &book) == "Paste SelectAll");

    book.setSelection({{0, 0}, {0, 3}});
    CHECK(possibleOn(coordinator, &book) == "Cut Copy Paste SelectAll");
    CHECK(coordinator.execute(Edit::Cut, &book));
    CHECK(book.plainText() == QStringLiteral(" two"));
    CHECK(clipboardText() == "One");
    CHECK(possibleOn(coordinator, &book) == "Undo Paste SelectAll");

    CHECK(coordinator.execute(Edit::Undo, &book));
    CHECK(book.plainText() == QStringLiteral("One two"));
    CHECK(coordinator.execute(Edit::Redo, &book));
    CHECK(book.plainText() == QStringLiteral(" two"));
    CHECK(coordinator.execute(Edit::Paste, &book));
    CHECK(book.plainText() == QStringLiteral("One two"));

    CHECK(coordinator.execute(Edit::SelectAll, &book));
    CHECK(coordinator.execute(Edit::Copy, &book));
    CHECK(clipboardText() == "One two");
}

TEST_CASE("Edit commands: a list copies its current item and selects all where it can",
          "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));
    QTreeWidget list;
    list.addTopLevelItem(new QTreeWidgetItem(QStringList{QStringLiteral("Chapter 1")}));
    list.addTopLevelItem(new QTreeWidgetItem(QStringList{QStringLiteral("Chapter 2")}));

    // One item at a time: it copies the current one, pastes and undoes nothing
    CHECK(possibleOn(coordinator, &list).empty());
    list.setCurrentItem(list.topLevelItem(1));
    CHECK(possibleOn(coordinator, &list) == "Copy");
    CHECK(coordinator.execute(Edit::Copy, &list));
    CHECK(clipboardText() == "Chapter 2");
    CHECK_FALSE(coordinator.execute(Edit::Paste, &list));

    // Many: it selects them all
    list.setSelectionMode(QAbstractItemView::ExtendedSelection);
    CHECK(possibleOn(coordinator, &list) == "Copy SelectAll");
    CHECK(coordinator.execute(Edit::SelectAll, &list));
    CHECK(list.selectedItems().size() == 2);
}

TEST_CASE("Edit commands: in the find bar and in the Annotations panel, Undo and Redo work on "
          "the text",
          "[gui][edit]") {
    editor::BookEditor book;
    test::resizeWidget(book, QSize(600, 400));
    book.fromKml(test::kmlOf({QStringLiteral("One two")}));
    book.setCursorPosition({0, 7});
    book.insertText(QStringLiteral(" three"));
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));

    SECTION("the find bar's buttons: the book it searches") {
        EditCoordinator coordinator(noBook);
        book.showFindReplace();
        auto* bar = book.findChild<editor::FindReplaceBar*>();
        REQUIRE(bar != nullptr);
        auto* button = bar->findChild<QPushButton*>();
        REQUIRE(button != nullptr);
        CHECK(possibleOn(coordinator, button) == "Undo");
        CHECK(coordinator.execute(Edit::Undo, button));
        CHECK(book.plainText() == QStringLiteral("One two"));
        CHECK(possibleOn(coordinator, button) == "Redo");
        CHECK_FALSE(coordinator.execute(Edit::Paste, button));
        CHECK(book.plainText() == QStringLiteral("One two"));

        // Its fields are fields
        auto* field = bar->findChild<QLineEdit*>();
        REQUIRE(field != nullptr);
        CHECK(possibleOn(coordinator, field).find("Redo") == std::string::npos);
    }

    SECTION("the Annotations panel outside its fields: the book in front") {
        AnnotationsPanel panel;
        auto* area = panel.findChild<QScrollArea*>();
        REQUIRE(area != nullptr);
        QWidget* list = area->widget();
        REQUIRE(list != nullptr);

        EditCoordinator withBook([&book]() -> editor::BookEditor* { return &book; });
        CHECK(possibleOn(withBook, list) == "Undo");
        CHECK(withBook.execute(Edit::Undo, list));
        CHECK(book.plainText() == QStringLiteral("One two"));
        CHECK_FALSE(withBook.execute(Edit::Paste, list));

        // No book in front (the Dashboard): nothing
        EditCoordinator withoutBook(noBook);
        CHECK(possibleOn(withoutBook, list).empty());
    }
}

TEST_CASE("Edit commands: elsewhere they do nothing", "[gui][edit]") {
    EditCoordinator coordinator(noBook);
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));
    QPushButton button(QStringLiteral("Open"));
    CHECK(possibleOn(coordinator, &button).empty());
    CHECK_FALSE(coordinator.execute(Edit::Paste, &button));
    CHECK(possibleOn(coordinator, nullptr).empty());
}

// =============================================================================
// The window: the element with the keys, the commands' keys and states
// =============================================================================

TEST_CASE("Edit commands: the menu bar and the window's toolbars give them to the element that "
          "had the keys",
          "[gui][edit]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    QMainWindow window;
    auto* central = new QWidget();
    auto* layout = new QVBoxLayout(central);
    auto* field = new QLineEdit();
    auto* button = new QPushButton(QStringLiteral("Open"));
    auto* panelBar = new QToolBar();  // a panel's own toolbar
    panelBar->addAction(QStringLiteral("Clear"));
    layout->addWidget(field);
    layout->addWidget(button);
    layout->addWidget(panelBar);
    window.setCentralWidget(central);
    window.menuBar()->addMenu(QStringLiteral("Edit"));
    QToolBar* windowBar = window.addToolBar(QStringLiteral("Edit"));
    QAction* paste = windowBar->addAction(QStringLiteral("Paste"));
    auto* toolButton = qobject_cast<QToolButton*>(windowBar->widgetForAction(paste));
    auto* panelButton =
        qobject_cast<QToolButton*>(panelBar->widgetForAction(panelBar->actions().first()));
    REQUIRE(toolButton != nullptr);
    REQUIRE(panelButton != nullptr);
    EditCoordinator coordinator(noBook);
    coordinator.connectCommands();
    activate(window);

    REQUIRE(takesKeys(field));
    CHECK(coordinator.focusedElement() == field);
    REQUIRE(takesKeys(window.menuBar()));  // Alt, F10
    CHECK(coordinator.focusedElement() == field);
    REQUIRE(takesKeys(toolButton));
    CHECK(coordinator.focusedElement() == field);

    // A panel's toolbar has its own commands; a button is not a field
    REQUIRE(takesKeys(panelButton));
    CHECK(coordinator.focusedElement() == panelButton);
    REQUIRE(takesKeys(button));
    CHECK(coordinator.focusedElement() == button);
    REQUIRE(takesKeys(window.menuBar()));
    CHECK(coordinator.focusedElement() == button);

    // An element that goes is forgotten
    REQUIRE(takesKeys(field));
    REQUIRE(takesKeys(window.menuBar()));
    delete field;
    CHECK(coordinator.focusedElement() == nullptr);
}

TEST_CASE("Edit commands: the commands ask no coordinator that is gone", "[gui][edit]") {
    // The registry outlives the window and its coordinator
    registerAllCommands(CommandCallbacks{});
    {
        EditCoordinator coordinator(noBook);
        coordinator.connectCommands();
    }
    for (const char* id : IDS) {
        INFO(id);
        const Command* command = CommandRegistry::getInstance().getCommand(id);
        REQUIRE(command != nullptr);
        CHECK(command->isEnabled == nullptr);
    }
}

TEST_CASE("Edit commands: the keys work on the element with the cursor", "[gui][edit]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    QMainWindow window;
    auto* book = new editor::BookEditor();
    window.setCentralWidget(book);
    book->fromKml(test::kmlOf({QStringLiteral("One two")}));
    auto* log = new QTextEdit();  // as the Log shows its lines
    log->setPlainText(QStringLiteral("Log line"));
    log->setReadOnly(true);
    auto* dock = new QDockWidget(QStringLiteral("Log"), &window);
    dock->setWidget(log);
    window.addDockWidget(Qt::BottomDockWidgetArea, dock);

    EditCoordinator coordinator(noBook);
    CommandCallbacks callbacks;
    callbacks.onUndo = [&coordinator] { coordinator.execute(Edit::Undo); };
    callbacks.onRedo = [&coordinator] { coordinator.execute(Edit::Redo); };
    callbacks.onCut = [&coordinator] { coordinator.execute(Edit::Cut); };
    callbacks.onCopy = [&coordinator] { coordinator.execute(Edit::Copy); };
    callbacks.onPaste = [&coordinator] { coordinator.execute(Edit::Paste); };
    callbacks.onSelectAll = [&coordinator] { coordinator.execute(Edit::SelectAll); };
    registerAllCommands(callbacks);
    for (const char* id : IDS) {
        window.addAction(CommandRegistry::getInstance().getAction(QString::fromLatin1(id)));
    }
    coordinator.connectCommands();
    QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));
    activate(window);

    // The Log: Paste, Cut and Undo do nothing, with their keys too; Copy copies
    REQUIRE(takesKeys(log));
    CHECK(enabledCommands() == "SelectAll");
    press(log, Qt::Key_V, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("One two"));
    CHECK(log->toPlainText() == QStringLiteral("Log line"));
    press(log, Qt::Key_A, Qt::ControlModifier);
    CHECK(log->textCursor().selectedText() == QStringLiteral("Log line"));
    CHECK(enabledCommands() == "Copy SelectAll");
    press(log, Qt::Key_X, Qt::ControlModifier);
    CHECK(log->toPlainText() == QStringLiteral("Log line"));
    CHECK(clipboardText() == "Pasted");
    press(log, Qt::Key_C, Qt::ControlModifier);
    CHECK(clipboardText() == "Log line");

    // The book: they work on its text
    REQUIRE(takesKeys(book));
    book->setCursorPosition({0, 7});
    CHECK(enabledCommands() == "Paste SelectAll");
    press(book, Qt::Key_V, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("One twoLog line"));
    CHECK(enabledCommands() == "Undo Paste SelectAll");

    // Undo in the Log does not undo in the book
    REQUIRE(takesKeys(log));
    CHECK(enabledCommands() == "Copy SelectAll");
    press(log, Qt::Key_Z, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("One twoLog line"));
    REQUIRE(takesKeys(book));
    press(book, Qt::Key_Z, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("One two"));

    // An empty clipboard: nothing to paste
    QGuiApplication::clipboard()->clear();
    CHECK(enabledCommands() == "Redo SelectAll");
}
