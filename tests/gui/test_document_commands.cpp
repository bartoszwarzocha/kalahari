/// @file test_document_commands.cpp
/// @brief The commands that work on the document in front are greyed out without one, those of
/// the open book without a book, Save All without a document; a greyed out command says why,
/// also after its keys

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/utils/document_commands.h"

#include "../editor/editor_test_utils.h"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QStatusBar>

#include <algorithm>
#include <string>
#include <vector>

using namespace kalahari::gui;

namespace {

/// The callbacks of the program's commands, each doing nothing: the commands run, so only
/// their enabled state greys them out
CommandCallbacks runningCallbacks() {
    const auto none = []() {};
    CommandCallbacks callbacks;
    for (std::function<void()>* callback :
         {&callbacks.onSaveDocument, &callbacks.onUndo, &callbacks.onFind,
          &callbacks.onFindNext, &callbacks.onFindPrevious, &callbacks.onFindReplace,
          &callbacks.onFormatBold, &callbacks.onFormatItalic, &callbacks.onFormatUnderline,
          &callbacks.onFormatStrikethrough, &callbacks.onAlignLeft, &callbacks.onAlignCenter,
          &callbacks.onAlignRight, &callbacks.onAlignJustify, &callbacks.onViewModeContinuous,
          &callbacks.onViewModePage, &callbacks.onDarkPaperToggle, &callbacks.onZoomIn,
          &callbacks.onZoomOut, &callbacks.onZoomReset, &callbacks.onZoomPageWidth,
          &callbacks.onZoomWholePage}) {
        *callback = none;
    }
    return callbacks;
}

bool isDocumentCommand(const std::string& id) {
    const std::vector<std::string>& ids = utils::documentCommands();
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

/// The keys of the window need an active window; the offscreen platform has one without a
/// window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

/// Keys pressed and released in a widget, as the keyboard does it: the window's shortcuts first
void press(QWidget* widget, const QKeySequence& keys) {
    const QKeyCombination combination = keys[0];
    QKeyEvent pressed(QEvent::KeyPress, combination.key(), combination.keyboardModifiers());
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, combination.key(), combination.keyboardModifiers());
    QApplication::sendEvent(widget, &released);
}

}  // namespace

TEST_CASE("Document commands: greyed out without a document in front, with their keys",
          "[gui][commands]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    registerAllCommands(runningCallbacks());

    // Every command of the list is one of the program's, and runs
    for (const std::string& id : utils::documentCommands()) {
        INFO(id);
        const Command* command = registry.getCommand(id);
        REQUIRE(command != nullptr);
        CHECK(command->canExecute());
        REQUIRE(registry.getAction(id) != nullptr);
    }
    // Find, the formats, the alignments, the view mode and the zoom, each with its keys
    for (const char* id : {"edit.find", "edit.findNext", "edit.findPrevious", "edit.findReplace",
                           "format.bold", "format.italic", "format.underline",
                           "format.strikethrough", "format.alignLeft", "format.alignCenter",
                           "format.alignRight", "format.justify", "view.mode.continuous",
                           "view.mode.page", "view.zoomIn", "view.zoomOut", "view.resetZoom",
                           "view.zoomPageWidth", "view.zoomWholePage"}) {
        INFO(id);
        CHECK(isDocumentCommand(id));
    }
    // Not the commands that work without a document, or on the element with the cursor
    for (const char* id : {"file.save", "edit.undo", "view.darkPaper"}) {
        INFO(id);
        CHECK_FALSE(isDocumentCommand(id));
    }

    // The Dashboard in front: greyed out (a disabled action takes no keys either)
    bool hasDocument = false;
    utils::bindToDocument(registry, [&hasDocument]() { return hasDocument; });
    for (const std::string& id : utils::documentCommands()) {
        INFO(id);
        CHECK_FALSE(registry.getAction(id)->isEnabled());
    }
    CHECK(registry.getAction(std::string("file.save"))->isEnabled());
    CHECK(registry.getAction(std::string("view.darkPaper"))->isEnabled());

    // A chapter comes to the front: the states shown again
    hasDocument = true;
    utils::updateDocumentCommands(registry);
    for (const std::string& id : utils::documentCommands()) {
        INFO(id);
        CHECK(registry.getAction(id)->isEnabled());
    }

    // And back to the Dashboard
    hasDocument = false;
    utils::updateDocumentCommands(registry);
    CHECK_FALSE(registry.getAction(std::string("edit.find"))->isEnabled());
    CHECK_FALSE(registry.getAction(std::string("format.bold"))->isEnabled());

    registry.clear();
}

TEST_CASE("Book commands: Close Book and the export of the archive greyed out without a book",
          "[gui][commands]") {
    // Round 3 report 19.2: without a book they could only say that one must be opened first
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    CommandCallbacks callbacks = runningCallbacks();
    const auto none = []() {};
    for (std::function<void()>* callback :
         {&callbacks.onNewDocument, &callbacks.onOpenDocument, &callbacks.onNewProject,
          &callbacks.onImportArchive, &callbacks.onCloseDocument, &callbacks.onExportArchive}) {
        *callback = none;
    }
    registerAllCommands(callbacks);
    CHECK(utils::openBookCommands() ==
          std::vector<std::string>{"file.close", "file.export.archive"});

    bool hasBook = false;
    utils::bindToOpenBook(registry, [&hasBook]() { return hasBook; });
    for (const std::string& id : utils::openBookCommands()) {
        INFO(id);
        REQUIRE(registry.getCommand(id)->canExecute());
        CHECK_FALSE(registry.getAction(id)->isEnabled());
        // Its tooltip and the status bar say why (KalahariStyle lets the mouse and the arrow
        // keys rest on grey menu items)
        CHECK(registry.getAction(id)->toolTip() == QStringLiteral("No book is open"));
        CHECK(registry.getAction(id)->statusTip() == QStringLiteral("No book is open"));
    }
    // The commands that work without a book
    for (const char* id : {"file.new", "file.open", "file.new.project", "file.import.archive"}) {
        INFO(id);
        CHECK(registry.getAction(std::string(id))->isEnabled());
    }

    // A book opens, and closes again
    hasBook = true;
    utils::updateOpenBookCommands(registry);
    for (const std::string& id : utils::openBookCommands()) {
        INFO(id);
        CHECK(registry.getAction(id)->isEnabled());
        // Their usual tooltips again, and nothing in the status bar
        CHECK(registry.getAction(id)->toolTip() != QStringLiteral("No book is open"));
        CHECK(registry.getAction(id)->statusTip().isEmpty());
    }
    hasBook = false;
    utils::updateOpenBookCommands(registry);
    CHECK_FALSE(registry.getAction(std::string("file.close"))->isEnabled());
    CHECK_FALSE(registry.getAction(std::string("file.export.archive"))->isEnabled());

    registry.clear();
}

TEST_CASE("Book commands: the keys of a greyed out command say why in the status bar",
          "[gui][commands]") {
    // A greyed out command takes no keys: Ctrl+F4 without a book did nothing, and once it said
    // that a document was closed
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    CommandCallbacks callbacks = runningCallbacks();
    int closed = 0;
    callbacks.onCloseDocument = [&closed]() { ++closed; };
    registerAllCommands(callbacks);
    bool hasBook = false;
    utils::bindToOpenBook(registry, [&hasBook]() { return hasBook; });

    // The window takes the keys of Close Book, as from its menu bar
    QMainWindow window;
    QStatusBar statusBar;  // not the window's own, as in test_dock_layout.cpp (QTBUG-137524)
    auto* text = new QLineEdit(&window);
    window.setCentralWidget(text);
    QAction* close = registry.getAction(std::string("file.close"));
    REQUIRE(close != nullptr);
    REQUIRE(close->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_F4));
    window.addAction(close);
    utils::explainGreyedKeys(registry, &window, &statusBar);
    window.show();
    window.activateWindow();
    REQUIRE(kalahari::test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    text->setFocus(Qt::OtherFocusReason);

    // Without a book: the status bar says why, and the command does not run
    press(text, QKeySequence(Qt::CTRL | Qt::Key_F4));
    CHECK(statusBar.currentMessage() == QStringLiteral("No book is open"));
    CHECK(closed == 0);

    // With a book: the command takes its keys again
    statusBar.clearMessage();
    hasBook = true;
    utils::updateOpenBookCommands(registry);
    press(text, QKeySequence(Qt::CTRL | Qt::Key_F4));
    CHECK(closed == 1);
    CHECK(statusBar.currentMessage().isEmpty());

    // Other keys given to the command say it as well, and the old ones nothing
    hasBook = false;
    utils::updateOpenBookCommands(registry);
    close->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_K));
    press(text, QKeySequence(Qt::CTRL | Qt::Key_F4));
    CHECK(statusBar.currentMessage().isEmpty());
    press(text, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_K));
    CHECK(statusBar.currentMessage() == QStringLiteral("No book is open"));
    CHECK(closed == 1);

    registry.clear();
}

TEST_CASE("Save commands: a key given to Save All says why it is greyed out",
          "[gui][commands]") {
    // Save All has no key of its own: one the writer gives it says why, as the keys of Save do
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    CommandCallbacks callbacks = runningCallbacks();
    int saved = 0;
    callbacks.onSaveAll = [&saved]() { ++saved; };
    registerAllCommands(callbacks);
    QString whyNot = QStringLiteral("No document to save");
    const auto none = []() { return QString(); };
    utils::bindSaveCommands(registry, none, none, [&whyNot]() { return whyNot; });

    QMainWindow window;
    QStatusBar statusBar;  // not the window's own, as in test_dock_layout.cpp (QTBUG-137524)
    auto* text = new QLineEdit(&window);
    window.setCentralWidget(text);
    QAction* saveAll = registry.getAction(std::string("file.saveAll"));
    REQUIRE(saveAll != nullptr);
    CHECK(saveAll->shortcut().isEmpty());
    CHECK_FALSE(saveAll->isEnabled());
    CHECK(saveAll->statusTip() == whyNot);
    window.addAction(saveAll);
    utils::explainGreyedKeys(registry, &window, &statusBar);
    const QKeySequence keys(Qt::CTRL | Qt::ALT | Qt::Key_L);
    saveAll->setShortcut(keys);
    window.show();
    window.activateWindow();
    REQUIRE(kalahari::test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    text->setFocus(Qt::OtherFocusReason);

    // Nothing to save: the status bar says why, and the command does not run
    press(text, keys);
    CHECK(statusBar.currentMessage() == QStringLiteral("No document to save"));
    CHECK(saved == 0);

    // A document to save: the command takes its key again
    statusBar.clearMessage();
    whyNot.clear();
    utils::updateSaveCommands(registry);
    CHECK(saveAll->isEnabled());
    press(text, keys);
    CHECK(saved == 1);
    CHECK(statusBar.currentMessage().isEmpty());

    registry.clear();
}
