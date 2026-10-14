/// @file test_document_commands.cpp
/// @brief The commands that work on the document in front are greyed out without one

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/utils/document_commands.h"

#include <QAction>

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
    }
    hasBook = false;
    utils::updateOpenBookCommands(registry);
    CHECK_FALSE(registry.getAction(std::string("file.close"))->isEnabled());
    CHECK_FALSE(registry.getAction(std::string("file.export.archive"))->isEnabled());

    registry.clear();
}
