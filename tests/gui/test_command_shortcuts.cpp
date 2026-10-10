/// @file test_command_shortcuts.cpp
/// @brief The keyboard shortcuts of the application's commands

#include <catch2/catch_test_macros.hpp>
#include "kalahari/editor/text_keys.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"

#include <QKeySequence>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
#ifdef Q_OS_MACOS
        // Option+Cmd: Option+Down is the text's
        {"edit.nextTodo", QStringLiteral("Ctrl+Alt+Down")},
        {"edit.previousTodo", QStringLiteral("Ctrl+Alt+Up")},
#else
        {"edit.nextTodo", QStringLiteral("Alt+Down")},
        {"edit.previousTodo", QStringLiteral("Alt+Up")},
#endif
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

TEST_CASE("Command shortcuts: no command takes a key of the text", "[gui][command][shortcuts]") {
    // Regression: on macOS Option+Down, Next To Do, is the text's move to the end of the
    // paragraph; the window's action would take it from the editor
    registerAllCommands(CommandCallbacks{});

    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        const QKeySequence keys = command.shortcut.toQKeySequence();
        if (keys.isEmpty()) {
            continue;
        }
        const QKeyCombination combination = keys[0];
        INFO(command.id << " has " << keys.toString(QKeySequence::PortableText).toStdString());
        CHECK(kalahari::editor::textKeyFor(combination.key(), combination.keyboardModifiers(),
                                           kalahari::editor::isMacOS())
                  .action == kalahari::editor::TextKeyAction::None);
    }
}

namespace {

/// @brief Whether @p c can be a part of a name or a number
bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// @brief C++ code without its comments and without what its strings and characters hold
///        (their quotes stay), line for line
std::string codeOf(const std::string& source) {
    enum class State { Code, LineComment, BlockComment, String, Character, RawString };
    State state = State::Code;
    std::string code;
    std::string rawStringEnd;  // )delimiter"
    std::size_t at = 0;
    while (at < source.size()) {
        const char c = source[at];
        const char next = at + 1 < source.size() ? source[at + 1] : '\0';
        if (c == '\n') {
            // The lines keep their numbers. A line comment ends with its line, unless the
            // line ends with a backslash.
            code += '\n';
            std::size_t last = at;
            while (last > 0 && source[last - 1] == '\r') {
                --last;
            }
            if (state == State::LineComment && (last == 0 || source[last - 1] != '\\')) {
                state = State::Code;
            }
            ++at;
            continue;
        }
        switch (state) {
        case State::Code:
            if (c == '/' && (next == '/' || next == '*')) {
                state = next == '/' ? State::LineComment : State::BlockComment;
                code += ' ';
                at += 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                // The word just before the quote: a prefix (u8, L, R...) or a number (1'000)
                std::size_t start = code.size();
                while (start > 0 && isWordChar(code[start - 1])) {
                    --start;
                }
                const std::string word = code.substr(start);
                code += c;
                ++at;
                if (c == '\'' && !word.empty() &&
                    std::isdigit(static_cast<unsigned char>(word.front())) != 0) {
                    continue;  // A digit separator
                }
                if (c == '"' && (word == "R" || word == "u8R" || word == "uR" || word == "UR" ||
                                 word == "LR")) {
                    const std::size_t open = source.find('(', at);
                    if (open == std::string::npos) {
                        return code;
                    }
                    rawStringEnd = ')' + source.substr(at, open - at) + '"';
                    state = State::RawString;
                    at = open + 1;
                    continue;
                }
                state = c == '"' ? State::String : State::Character;
                continue;
            }
            code += c;
            ++at;
            continue;
        case State::LineComment:
            ++at;
            continue;
        case State::BlockComment:
            if (c == '*' && next == '/') {
                state = State::Code;
                at += 2;
                continue;
            }
            ++at;
            continue;
        case State::String:
        case State::Character:
            if (c == '\\') {
                if (next == '\n') {
                    code += '\n';
                }
                at += 2;
                continue;
            }
            if (c == (state == State::String ? '"' : '\'')) {
                code += c;
                state = State::Code;
            }
            ++at;
            continue;
        case State::RawString:
            if (source.compare(at, rawStringEnd.size(), rawStringEnd) == 0) {
                code += '"';
                state = State::Code;
                at += rawStringEnd.size();
                continue;
            }
            ++at;
            continue;
        }
    }
    return code;
}

/// @brief The lines of @p source that take keys from the system: Qt's standard keys
///        (QKeySequence::StandardKey, QKeySequence::keyBindings()) or the undo actions of
///        QUndoStack and QUndoGroup, which have them
/// @return The number and the text of each line
std::vector<std::pair<int, std::string>> systemKeysIn(const std::string& source) {
    // What QKeySequence has besides its standard keys
    static const std::set<QString> notKeys = {
        QStringLiteral("SequenceFormat"), QStringLiteral("NativeText"),
        QStringLiteral("PortableText"),   QStringLiteral("SequenceMatch"),
        QStringLiteral("NoMatch"),        QStringLiteral("PartialMatch"),
        QStringLiteral("ExactMatch"),     QStringLiteral("fromString"),
        QStringLiteral("listFromString"), QStringLiteral("listToString"),
        QStringLiteral("mnemonic"),       QStringLiteral("toString"),
        QStringLiteral("count"),          QStringLiteral("isEmpty"),
        QStringLiteral("matches"),        QStringLiteral("swap"),
        QStringLiteral("isDetached"),     QStringLiteral("operator"),
    };
    static const QRegularExpression pattern(
        QStringLiteral(R"(\bQKeySequence\s*::\s*(\w+)|\b(createUndoAction|createRedoAction)\b)"));

    const QString code = QString::fromUtf8(codeOf(source));
    const QStringList lines = QString::fromUtf8(source).split(QLatin1Char('\n'));
    std::vector<std::pair<int, std::string>> found;
    QRegularExpressionMatchIterator matches = pattern.globalMatch(code);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        if (match.hasCaptured(1) && notKeys.count(match.captured(1)) == 1) {
            continue;
        }
        const auto line =
            static_cast<int>(QStringView(code).left(match.capturedEnd()).count(QLatin1Char('\n')));
        const QString text = line < lines.size() ? lines.at(line).trimmed() : QString();
        if (found.empty() || found.back().first != line + 1) {
            found.emplace_back(line + 1, text.toStdString());
        }
    }
    return found;
}

}  // namespace

TEST_CASE("Command shortcuts: the check of the system's keys finds them in the code only",
          "[gui][command][shortcuts]") {
    CHECK(systemKeysIn("auto* undo = stack->createUndoAction(this);").size() == 1);
    CHECK(systemKeysIn("auto* redo = group.createRedoAction(this, tr(\"Redo\"));").size() == 1);
    CHECK(systemKeysIn("if (event->matches(QKeySequence::Undo)) {").size() == 1);
    CHECK(systemKeysIn("action->setShortcuts(QKeySequence :: Copy);").size() == 1);
    CHECK(systemKeysIn("const auto keys = QKeySequence::keyBindings(key);").size() == 1);
    CHECK(systemKeysIn("void press(QKeySequence::StandardKey key);").size() == 1);
    CHECK(systemKeysIn("int many = 1'000; auto keys = QKeySequence::Paste;").size() == 1);
    CHECK(systemKeysIn("const char quote = '\"'; auto keys = QKeySequence::Cut;").size() == 1);
    CHECK(systemKeysIn("keys.toString(QKeySequence::NativeText);").empty());
    CHECK(systemKeysIn("QKeySequence::fromString(text, QKeySequence::PortableText);").empty());
    CHECK(systemKeysIn("// QKeySequence::Copy differs\n/* createUndoAction */").empty());
    CHECK(systemKeysIn("auto text = \"QKeySequence::Copy \\\" createRedoAction\";").empty());
    CHECK(systemKeysIn("auto svg = R\"x(QKeySequence::Copy)\" )x\";").empty());

    // The line it names is the line of the key
    const auto found = systemKeysIn("int a;\n/* a\nb */ auto keys =\n    QKeySequence::Find;");
    REQUIRE(found.size() == 1);
    CHECK(found.front().first == 4);
    CHECK(found.front().second == "QKeySequence::Find;");
}

TEST_CASE("Command shortcuts: no key is the system's own", "[gui][command][shortcuts]") {
    // Qt's standard keys differ between the systems and the Linux desktops (Redo is Ctrl+Y
    // on Windows and Ctrl+Shift+Z on Linux; Close is Ctrl+F4 on Windows and Ctrl+W on
    // Linux). Every key of the program is written out, the same on every system; on macOS
    // they differ only where the documentation says.
    const std::filesystem::path sources(KALAHARI_SOURCE_DIR);
    int files = 0;
    std::ostringstream found;
    for (const char* folder : {"src", "include"}) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sources / folder)) {
            const std::string extension = entry.path().extension().string();
            if (!entry.is_regular_file() || (extension != ".cpp" && extension != ".h")) {
                continue;
            }
            std::ifstream file(entry.path(), std::ios::binary);
            const std::string source{std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>()};
            ++files;
            for (const auto& [line, text] : systemKeysIn(source)) {
                found << '\n'
                      << entry.path().lexically_relative(sources).generic_string() << ':' << line
                      << ": " << text;
            }
        }
    }
    CHECK(files > 200);
    const std::string list = found.str();
    INFO("Keys of the system:" << list);
    CHECK(list.empty());
}
