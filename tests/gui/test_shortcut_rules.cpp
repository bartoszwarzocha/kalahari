/// @file test_shortcut_rules.cpp
/// @brief Which keys a command can have: the keys of the text, of the bars and of the system

#include <catch2/catch_test_macros.hpp>
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/editor/text_keys.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QShortcut>

#include <algorithm>
#include <string>

using namespace kalahari::gui;
using Result = KeyCheck::Result;

namespace {

QKeyCombination keys(Qt::KeyboardModifiers modifiers, Qt::Key key) {
    return QKeyCombination(modifiers, key);
}

Result resultOf(ShortcutPlatform platform, QKeyCombination combination,
                const std::string& commandId = "file.close") {
    return ShortcutRules(platform).check(combination, commandId).result;
}

const Qt::KeyboardModifiers NONE = Qt::NoModifier;
const Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
const Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
const Qt::KeyboardModifiers ALT = Qt::AltModifier;
const Qt::KeyboardModifiers META = Qt::MetaModifier;

}  // namespace

TEST_CASE("Shortcut rules: keys that type text are refused", "[gui][shortcuts][rules]") {
    for (const ShortcutPlatform platform : {ShortcutPlatform::Windows, ShortcutPlatform::Linux}) {
        CHECK(resultOf(platform, keys(NONE, Qt::Key_K)) == Result::Refused);
        CHECK(resultOf(platform, keys(SHIFT, Qt::Key_K)) == Result::Refused);
        CHECK(resultOf(platform, keys(NONE, Qt::Key_Space)) == Result::Refused);
        CHECK(resultOf(platform, keys(SHIFT, Qt::Key_5)) == Result::Refused);
        CHECK(resultOf(platform, keys(NONE, Qt::Key_Exclam)) == Result::Refused);
        CHECK(resultOf(platform, keys(CTRL, Qt::Key_K)) == Result::Allowed);
        CHECK(resultOf(platform, keys(ALT, Qt::Key_K)) == Result::Allowed);
        CHECK(resultOf(platform, keys(CTRL | SHIFT, Qt::Key_K)) == Result::Allowed);
        CHECK(resultOf(platform, keys(NONE, Qt::Key_F7)) == Result::Allowed);
        const KeyCheck typing = ShortcutRules(platform).check(keys(NONE, Qt::Key_K), "file.close");
        CHECK(typing.reason.contains(QStringLiteral("types text")));
    }

    // On macOS Option with a key types a character as well: Cmd or Control makes a shortcut
    const ShortcutPlatform mac = ShortcutPlatform::MacOS;
    CHECK(resultOf(mac, keys(NONE, Qt::Key_K)) == Result::Refused);
    CHECK(resultOf(mac, keys(ALT, Qt::Key_K)) == Result::Refused);
    CHECK(resultOf(mac, keys(ALT | SHIFT, Qt::Key_K)) == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_K)) == Result::Allowed);
    CHECK(resultOf(mac, keys(META, Qt::Key_G)) == Result::Allowed);
    CHECK(resultOf(mac, keys(CTRL | ALT, Qt::Key_K)) == Result::Allowed);
}

TEST_CASE("Shortcut rules: Ctrl+Alt is AltGr on Windows and the desktop's on Linux",
          "[gui][shortcuts][rules]") {
    const KeyCheck windows =
        ShortcutRules(ShortcutPlatform::Windows).check(keys(CTRL | ALT, Qt::Key_K), "file.close");
    CHECK(windows.result == Result::Refused);
    CHECK(windows.reason.contains(QStringLiteral("AltGr")));
    CHECK(resultOf(ShortcutPlatform::Windows, keys(CTRL | ALT | SHIFT, Qt::Key_F5)) ==
          Result::Refused);

    const KeyCheck linux =
        ShortcutRules(ShortcutPlatform::Linux).check(keys(CTRL | ALT, Qt::Key_T), "file.close");
    CHECK(linux.result == Result::Refused);
    CHECK(linux.reason.contains(QStringLiteral("Ctrl+Alt")));
}

TEST_CASE("Shortcut rules: the keys of the system", "[gui][shortcuts][rules]") {
    const ShortcutPlatform windows = ShortcutPlatform::Windows;
    CHECK(resultOf(windows, keys(ALT, Qt::Key_Tab)) == Result::Refused);
    CHECK(resultOf(windows, keys(ALT, Qt::Key_Space)) == Result::Refused);
    CHECK(resultOf(windows, keys(CTRL, Qt::Key_Escape)) == Result::Refused);
    CHECK(resultOf(windows, keys(CTRL | SHIFT, Qt::Key_Escape)) == Result::Refused);
    CHECK(resultOf(windows, keys(META, Qt::Key_E)) == Result::Refused);
    CHECK(resultOf(windows, keys(NONE, Qt::Key_Print)) == Result::Refused);
    CHECK(resultOf(windows, keys(NONE, Qt::Key_Tab)) == Result::Refused);
    CHECK(resultOf(windows, keys(SHIFT, Qt::Key_Tab)) == Result::Refused);
    // Alt+F4 closes the window: only Exit may have it
    CHECK(resultOf(windows, keys(ALT, Qt::Key_F4), "file.close") == Result::Refused);
    CHECK(resultOf(windows, keys(ALT, Qt::Key_F4), "file.exit") == Result::Allowed);

    const ShortcutPlatform linux = ShortcutPlatform::Linux;
    CHECK(resultOf(linux, keys(ALT, Qt::Key_Tab)) == Result::Refused);
    CHECK(resultOf(linux, keys(META, Qt::Key_E)) == Result::Refused);
    CHECK(resultOf(linux, keys(ALT, Qt::Key_F4), "file.close") == Result::Refused);
    CHECK(resultOf(linux, keys(ALT, Qt::Key_F4), "file.exit") == Result::Allowed);

    // macOS: Cmd is Ctrl, Control is Meta
    const ShortcutPlatform mac = ShortcutPlatform::MacOS;
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Q), "file.close") == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Q), "file.exit") == Result::Allowed);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Comma), "file.close") == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Comma), "edit.settings") == Result::Allowed);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_H)) == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Tab)) == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Space)) == Result::Refused);
    CHECK(resultOf(mac, keys(META, Qt::Key_Up)) == Result::Refused);
    CHECK(resultOf(mac, keys(META, Qt::Key_F2)) == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL | SHIFT, Qt::Key_4)) == Result::Refused);
    CHECK(resultOf(mac, keys(CTRL, Qt::Key_Dollar)) == Result::Refused);
    // Alt+F4 is no key of macOS
    CHECK(resultOf(mac, keys(ALT, Qt::Key_F4)) == Result::Allowed);
}

TEST_CASE("Shortcut rules: the fixed keys say where they work", "[gui][shortcuts][rules]") {
    const ShortcutRules windows(ShortcutPlatform::Windows);

    const KeyCheck word = windows.check(keys(CTRL, Qt::Key_Backspace), "file.close");
    CHECK(word.result == Result::Refused);
    CHECK(word.reason.contains(QStringLiteral("fixed key")));
    CHECK(word.reason.contains(
        QStringLiteral("In the text › Delete the word before / after the cursor")));

    // A key of several places names each of them
    const KeyCheck escape = windows.check(keys(NONE, Qt::Key_Escape), "file.close");
    CHECK(escape.result == Result::Refused);
    CHECK(escape.reason.contains(QStringLiteral("Find bar › Close the bar")));
    CHECK(escape.reason.contains(QStringLiteral("Annotation frame › Drop the text")));
    CHECK(escape.reason.contains(QStringLiteral("Distraction-Free › Leave it")));

    CHECK(windows.check(keys(ALT, Qt::Key_C), "file.close")
              .reason.contains(QStringLiteral("Find bar")));
    CHECK(windows.check(keys(CTRL, Qt::Key_Return), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(CTRL, Qt::Key_Enter), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(CTRL, Qt::Key_Tab), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(CTRL | SHIFT, Qt::Key_Tab), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(NONE, Qt::Key_Return), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(SHIFT, Qt::Key_F10), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(NONE, Qt::Key_Menu), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(CTRL | SHIFT, Qt::Key_Z), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(SHIFT, Qt::Key_Delete), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(CTRL | SHIFT, Qt::Key_Right), "file.close").result == Result::Refused);
    CHECK(windows.check(keys(NONE, Qt::Key_PageDown), "file.close").result == Result::Refused);
    // The keypad makes no difference
    CHECK(windows.check(QKeyCombination(CTRL | Qt::KeypadModifier, Qt::Key_Home), "file.close")
              .result == Result::Refused);

    // The keys of the Annotations panel work only in its list: a command may have them
    CHECK(windows.check(keys(NONE, Qt::Key_F2), "view.navigator").result == Result::Allowed);
    CHECK(windows.check(keys(NONE, Qt::Key_Delete), "file.close").result == Result::Refused);

    // macOS: Option+arrows move by words, and the options of the find bar are Option+Cmd
    const ShortcutRules mac(ShortcutPlatform::MacOS);
    CHECK(mac.check(keys(ALT, Qt::Key_Left), "file.close").result == Result::Refused);
    CHECK(mac.check(keys(CTRL, Qt::Key_Up), "file.close").result == Result::Refused);
    CHECK(mac.check(keys(META, Qt::Key_K), "file.close").result == Result::Refused);
    CHECK(mac.check(keys(CTRL | ALT, Qt::Key_C), "file.close").result == Result::Refused);
    CHECK(mac.check(keys(ALT, Qt::Key_Down), "file.close").result == Result::Refused);
    CHECK(mac.check(keys(CTRL | ALT, Qt::Key_Down), "edit.nextTodo").result == Result::Allowed);
}

TEST_CASE("Shortcut rules: keys some desktops take only bring a warning", "[gui][shortcuts][rules]") {
    const ShortcutRules linux(ShortcutPlatform::Linux);
    const KeyCheck desktops = linux.check(keys(CTRL, Qt::Key_F2), "file.close");
    CHECK(desktops.result == Result::Warning);
    CHECK(desktops.reason.contains(QStringLiteral("KDE and Xfce")));
    CHECK(linux.check(keys(ALT, Qt::Key_F2), "file.close").result == Result::Warning);
    CHECK(linux.check(keys(ALT, Qt::Key_F11), "file.close").result == Result::Allowed);
    CHECK(linux.check(keys(CTRL | SHIFT, Qt::Key_F2), "file.close").result == Result::Allowed);

    CHECK(resultOf(ShortcutPlatform::Windows, keys(CTRL, Qt::Key_F2)) == Result::Allowed);
    CHECK(resultOf(ShortcutPlatform::Windows, keys(ALT, Qt::Key_F2)) == Result::Allowed);

    CHECK(resultOf(ShortcutPlatform::MacOS, keys(NONE, Qt::Key_F11)) == Result::Warning);
    CHECK(resultOf(ShortcutPlatform::MacOS, keys(SHIFT, Qt::Key_F11)) == Result::Allowed);
}

TEST_CASE("Shortcut rules: every shortcut of the program is allowed", "[gui][shortcuts][rules]") {
    registerAllCommands(CommandCallbacks{});
    const ShortcutRules rules;
    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        if (command.shortcut.isEmpty()) {
            continue;
        }
        const QKeyCombination combination = command.shortcut.toQKeySequence()[0];
        const KeyCheck check = rules.check(combination, command.id);
        INFO(command.id << ": " << check.reason.toStdString());
        CHECK(check.result != Result::Refused);
    }
}

TEST_CASE("Shortcut rules: the list shows every key of the text", "[gui][shortcuts][rules]") {
    for (const ShortcutPlatform platform :
         {ShortcutPlatform::Windows, ShortcutPlatform::Linux, ShortcutPlatform::MacOS}) {
        const bool mac = platform == ShortcutPlatform::MacOS;
        const std::vector<FixedKeyGroup> groups = ShortcutRules(platform).fixedGroups();
        REQUIRE_FALSE(groups.empty());
        const FixedKeyGroup& text = groups.front();
        CHECK(text.title == QStringLiteral("In the text"));
        CHECK(groups.back().system);

        for (const kalahari::editor::TextKey& textKey : kalahari::editor::textKeys(mac)) {
            bool shown = false;
            for (const FixedKeys& row : text.rows) {
                shown = shown || row.covers(textKey.keys);
            }
            INFO(QKeySequence(textKey.keys).toString(QKeySequence::PortableText).toStdString()
                 << (mac ? " (macOS)" : ""));
            CHECK(shown);
        }
    }
}

TEST_CASE("Shortcut rules: the keys of the find bar are fixed", "[gui][shortcuts][rules]") {
    // The bar's shortcuts work while it is shown: a command with one of them would be
    // ambiguous there
    kalahari::editor::FindReplaceBar bar;
    const QList<QShortcut*> shortcuts = bar.findChildren<QShortcut*>();
    REQUIRE_FALSE(shortcuts.isEmpty());
    const ShortcutRules rules;
    for (const QShortcut* shortcut : shortcuts) {
        INFO(shortcut->key().toString(QKeySequence::PortableText).toStdString());
        CHECK(rules.check(shortcut->key()[0], "file.close").result == Result::Refused);
    }
}

TEST_CASE("Shortcut rules: a key press is written as the shortcuts are", "[gui][shortcuts][rules]") {
    const auto pressed = [](int key, Qt::KeyboardModifiers modifiers) {
        const QKeyEvent event(QEvent::KeyPress, key, modifiers);
        return ShortcutRules::keysOf(&event);
    };
    CHECK(pressed(Qt::Key_Backtab, SHIFT) == keys(SHIFT, Qt::Key_Tab));
    CHECK(pressed(Qt::Key_Backtab, CTRL | SHIFT) == keys(CTRL | SHIFT, Qt::Key_Tab));
    // Shift+1 is "!", which Qt matches without Shift
    CHECK(pressed(Qt::Key_Exclam, CTRL | SHIFT) == keys(CTRL, Qt::Key_Exclam));
    CHECK(pressed(Qt::Key_Plus, CTRL | SHIFT) == keys(CTRL, Qt::Key_Plus));
    CHECK(pressed(Qt::Key_A, CTRL | SHIFT) == keys(CTRL | SHIFT, Qt::Key_A));
    CHECK(pressed(Qt::Key_5, CTRL | SHIFT) == keys(CTRL | SHIFT, Qt::Key_5));
    CHECK(pressed(Qt::Key_Space, SHIFT) == keys(SHIFT, Qt::Key_Space));
    CHECK(pressed(Qt::Key_Plus, CTRL | Qt::KeypadModifier) == keys(CTRL, Qt::Key_Plus));
    CHECK(pressed(Qt::Key_Control, CTRL).key() == Qt::Key_unknown);
    CHECK(pressed(Qt::Key_Shift, SHIFT).key() == Qt::Key_unknown);
    CHECK(pressed(Qt::Key_Alt, ALT).key() == Qt::Key_unknown);
}

TEST_CASE("Shortcut rules: the Windows key and the Super key go by their names",
          "[gui][shortcuts][rules]") {
#if !defined(Q_OS_MACOS)
    // Qt writes them as Meta, which neither Windows nor the Linux desktops call them
#if defined(Q_OS_WIN)
    const QString metaKey = QStringLiteral("Win");
#else
    const QString metaKey = QStringLiteral("Super");
#endif
    CHECK(ShortcutRules::keysText(keys(META, Qt::Key_J)) == metaKey + QStringLiteral("+J"));
    CHECK(ShortcutRules::keysText(keys(META | CTRL | SHIFT, Qt::Key_F5)) ==
          metaKey + QStringLiteral("+") + ShortcutRules::keysText(keys(CTRL | SHIFT, Qt::Key_F5)));

    // The refusal names them as the row of the system does
    const KeyCheck check = ShortcutRules().check(keys(META, Qt::Key_J), "file.close");
    CHECK(check.result == Result::Refused);
    CHECK(check.reason.startsWith(metaKey + QStringLiteral("+J")));
#endif
    // Keys without them are written as Qt writes them
    CHECK(ShortcutRules::keysText(keys(CTRL | SHIFT, Qt::Key_F5)) ==
          QKeySequence(keys(CTRL | SHIFT, Qt::Key_F5)).toString(QKeySequence::NativeText));
}

TEST_CASE("Shortcut rules: rows of every key with some modifiers", "[gui][shortcuts][rules]") {
    FixedKeys altGr;
    altGr.anyKeyWith = CTRL | ALT;
    altGr.anyKeyText = QStringLiteral("Ctrl+Alt+…");
    CHECK(altGr.covers(keys(CTRL | ALT, Qt::Key_K)));
    CHECK(altGr.covers(keys(CTRL | ALT | SHIFT, Qt::Key_F5)));
    CHECK_FALSE(altGr.covers(keys(CTRL, Qt::Key_K)));
    CHECK(altGr.keysText() == QStringLiteral("Ctrl+Alt+…"));

    FixedKeys enter;
    enter.keys = {keys(NONE, Qt::Key_Return), keys(NONE, Qt::Key_Enter)};
    CHECK(enter.covers(QKeyCombination(Qt::KeypadModifier, Qt::Key_Enter)));
    CHECK_FALSE(enter.covers(keys(SHIFT, Qt::Key_Return)));
    // Enter of the keypad does what the main one does: the list names it once
    CHECK(enter.keysText() == ShortcutRules::keysText(keys(NONE, Qt::Key_Return)));
    enter.keys.append(keys(CTRL, Qt::Key_Enter));
    CHECK(enter.keysText() == ShortcutRules::keysText(keys(NONE, Qt::Key_Return)) +
                                  QStringLiteral(", ") +
                                  ShortcutRules::keysText(keys(CTRL, Qt::Key_Enter)));
}

TEST_CASE("Shortcut rules: the keys that select are written in words", "[gui][shortcuts][rules]") {
    for (const ShortcutPlatform platform :
         {ShortcutPlatform::Windows, ShortcutPlatform::Linux, ShortcutPlatform::MacOS}) {
        const bool mac = platform == ShortcutPlatform::MacOS;
        const FixedKeyGroup text = ShortcutRules(platform).fixedGroups().front();
        const auto selecting = std::find_if(text.rows.begin(), text.rows.end(),
                                            [](const FixedKeys& row) {
                                                return row.label == QStringLiteral("Selecting");
                                            });
        REQUIRE(selecting != text.rows.end());
        // A list of every key with Shift would be too long to read
        CHECK(selecting->keysText() == QStringLiteral("Shift with the keys that move the cursor"));
        for (const kalahari::editor::TextKey& textKey : kalahari::editor::textKeys(mac)) {
            if (textKey.extendSelection) {
                INFO(QKeySequence(textKey.keys).toString(QKeySequence::PortableText).toStdString());
                CHECK(selecting->covers(textKey.keys));
            }
        }
    }
}
