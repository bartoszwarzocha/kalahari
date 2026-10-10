/// @file test_command_shortcuts.cpp
/// @brief The keyboard shortcuts of the application's commands

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"

#include <QKeySequence>
#include <QString>
#include <map>
#include <string>

using namespace kalahari::gui;

TEST_CASE("Command shortcuts: no two commands share a shortcut", "[gui][command][shortcuts]") {
    // Regression: F3 was both Find Next and the Properties panel toggle. Find Next had no
    // callback, so its action was disabled; once it got one, F3 was an ambiguous shortcut
    // that triggered neither command.
    registerAllCommands(CommandCallbacks{});

    std::map<std::string, std::string> owners;
    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        const QKeySequence keys = command.shortcut.toQKeySequence();
        if (keys.isEmpty()) {
            continue;
        }
        const std::string text = keys.toString(QKeySequence::PortableText).toStdString();
        const auto [owner, added] = owners.emplace(text, command.id);
        INFO(text << " belongs to " << owner->second << " and " << command.id);
        CHECK(added);
    }
    CHECK(owners.size() > 20);
}

TEST_CASE("Command shortcuts: one set on every system, as the documentation lists them",
          "[gui][command][shortcuts]") {
    // Regression: Close Book took Qt's standard Close keys, Ctrl+F4 on Windows but Ctrl+W on
    // Linux, and Find & Replace had Ctrl+H, Ctrl+R (Align Right's) or none. Every shortcut
    // is written out; a command that gets or changes one is added here, and to the list of
    // shortcuts in the documentation. On macOS Qt shows Ctrl as Cmd and Meta as Control.
    const std::map<std::string, QString> documented = {
        {"file.new", QStringLiteral("Ctrl+N")},
        {"file.new.project", QStringLiteral("Ctrl+Shift+N")},
        {"file.open", QStringLiteral("Ctrl+O")},
        {"file.open.file", QStringLiteral("Ctrl+Shift+O")},
        {"file.close", QStringLiteral("Ctrl+F4")},
        {"file.save", QStringLiteral("Ctrl+S")},
        {"file.saveAs", QStringLiteral("Ctrl+Shift+S")},
#ifdef Q_OS_MACOS
        {"file.exit", QStringLiteral("Ctrl+Q")},  // Cmd+Q
#else
        {"file.exit", QStringLiteral("Alt+F4")},
#endif
        {"edit.undo", QStringLiteral("Ctrl+Z")},
        {"edit.redo", QStringLiteral("Ctrl+Y")},
        {"edit.cut", QStringLiteral("Ctrl+X")},
        {"edit.copy", QStringLiteral("Ctrl+C")},
        {"edit.paste", QStringLiteral("Ctrl+V")},
        {"edit.selectAll", QStringLiteral("Ctrl+A")},
        {"edit.find", QStringLiteral("Ctrl+F")},
        {"edit.findNext", QStringLiteral("F3")},
        {"edit.findPrevious", QStringLiteral("Shift+F3")},
#ifdef Q_OS_MACOS
        {"edit.findReplace", QStringLiteral("Ctrl+Alt+F")},  // Option+Cmd+F: Cmd+H hides
#else
        {"edit.findReplace", QStringLiteral("Ctrl+H")},
#endif
        {"edit.nextTodo", QStringLiteral("Alt+Down")},
        {"edit.previousTodo", QStringLiteral("Alt+Up")},
        {"insert.annotation", QStringLiteral("Ctrl+Shift+M")},
        {"format.bold", QStringLiteral("Ctrl+B")},
        {"format.italic", QStringLiteral("Ctrl+I")},
        {"format.underline", QStringLiteral("Ctrl+U")},
        {"format.alignLeft", QStringLiteral("Ctrl+L")},
        {"format.alignCenter", QStringLiteral("Ctrl+E")},
        {"format.alignRight", QStringLiteral("Ctrl+R")},
        {"format.justify", QStringLiteral("Ctrl+J")},
        {"view.navigator", QStringLiteral("F2")},
        {"view.log", QStringLiteral("F4")},
        {"view.search", QStringLiteral("F5")},
        {"view.assistant", QStringLiteral("F6")},
        {"view.properties", QStringLiteral("F8")},
        {"view.annotations", QStringLiteral("F9")},
        {"view.mode.continuous", QStringLiteral("Ctrl+1")},
        {"view.mode.page", QStringLiteral("Ctrl+2")},
        {"view.typewriter", QStringLiteral("Ctrl+3")},
        {"view.focus", QStringLiteral("Ctrl+4")},
        {"view.mode.distraction-free", QStringLiteral("Shift+F11")},
        {"view.zoomIn", QStringLiteral("Ctrl++")},
        {"view.zoomOut", QStringLiteral("Ctrl+-")},
        {"view.resetZoom", QStringLiteral("Ctrl+0")},
#ifdef Q_OS_MACOS
        {"view.fullScreen", QStringLiteral("Ctrl+Meta+F")},  // Control+Cmd+F: F11 shows the desktop
#else
        {"view.fullScreen", QStringLiteral("F11")},
#endif
        {"help.manual", QStringLiteral("F1")},
    };

    registerAllCommands(CommandCallbacks{});

    std::map<std::string, QKeySequence> actual;
    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        if (!command.shortcut.isEmpty()) {
            actual.emplace(command.id, command.shortcut.toQKeySequence());
        }
    }

    for (const auto& [id, keys] : documented) {
        INFO(id << " should have " << keys.toStdString());
        const auto found = actual.find(id);
        CHECK(found != actual.end());
        if (found != actual.end()) {
            CHECK(found->second == QKeySequence::fromString(keys, QKeySequence::PortableText));
        }
    }
    for (const auto& [id, keys] : actual) {
        INFO(id << " has " << keys.toString(QKeySequence::PortableText).toStdString()
                << ", which the list above does not have");
        CHECK(documented.count(id) == 1);
    }
}
