/// @file shortcut_rules.cpp
/// @brief Implementation of ShortcutRules

#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/editor/text_keys.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QStringList>

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

using editor::TextKey;
using editor::TextKeyAction;

constexpr Qt::KeyboardModifiers NONE = Qt::NoModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;  // Cmd on macOS
constexpr Qt::KeyboardModifiers ALT = Qt::AltModifier;       // Option on macOS
constexpr Qt::KeyboardModifiers META = Qt::MetaModifier;     // Windows, Super; Control on macOS

/// The key without its modifiers, as a number: a key press can have a value Qt::Key does not
/// name, so it is not cast to one
int keyCodeOf(QKeyCombination keys) {
    return keys.toCombined() & ~int(Qt::KeyboardModifierMask);
}

/// The same key with other modifiers
QKeyCombination withModifiers(QKeyCombination keys, Qt::KeyboardModifiers modifiers) {
    return QKeyCombination::fromCombined(keyCodeOf(keys) | static_cast<int>(modifiers.toInt()));
}

/// The keypad and the group switch make no difference to a shortcut
QKeyCombination withoutKeypad(QKeyCombination keys) {
    return withModifiers(keys, keys.keyboardModifiers() &
                                   ~(Qt::KeypadModifier | Qt::GroupSwitchModifier));
}

QKeyCombination keyWith(Qt::KeyboardModifiers modifiers, Qt::Key key) {
    return QKeyCombination(modifiers, key);
}

/// Enter of the main keyboard and of the keypad
QList<QKeyCombination> enterKeys(Qt::KeyboardModifiers modifiers) {
    return {keyWith(modifiers, Qt::Key_Return), keyWith(modifiers, Qt::Key_Enter)};
}

/// A key that is a character (a letter, a digit, a mark, the space)
bool isCharacterKey(int key) {
    return key > 0 && key < Qt::Key_Escape && QChar::isPrint(static_cast<char32_t>(key));
}

}  // namespace

// ============================================================================
// FixedKeys
// ============================================================================

bool FixedKeys::covers(QKeyCombination pressed) const {
    const QKeyCombination pressedKeys = withoutKeypad(pressed);
    if (anyKeyWith != Qt::NoModifier &&
        (pressedKeys.keyboardModifiers() & anyKeyWith) == anyKeyWith) {
        return true;
    }
    for (const QKeyCombination fixed : keys) {
        if (withoutKeypad(fixed) == pressedKeys) {
            return true;
        }
    }
    return false;
}

QString FixedKeys::keysText() const {
    if (!summary.isEmpty()) {
        return summary;
    }
    QStringList texts;
    for (const QKeyCombination fixed : keys) {
        // Enter of the keypad does what the main one does: the list names it once
        if (fixed.key() == Qt::Key_Enter &&
            keys.contains(QKeyCombination(fixed.keyboardModifiers(), Qt::Key_Return))) {
            continue;
        }
        const QString text = ShortcutRules::keysText(fixed);
        if (!texts.contains(text)) {
            texts.append(text);
        }
    }
    if (!anyKeyText.isEmpty()) {
        texts.append(anyKeyText);
    }
    return texts.join(QStringLiteral(", "));
}

QString FixedKeys::reasonFor(QKeyCombination pressed) const {
    const QKeyCombination pressedKeys = withoutKeypad(pressed);
    for (qsizetype index = 0; index < keys.size() && index < keyReasons.size(); ++index) {
        if (withoutKeypad(keys[index]) == pressedKeys) {
            return keyReasons[index];
        }
    }
    return reason.contains(QLatin1String("%1")) ? reason.arg(ShortcutRules::keysText(pressedKeys))
                                                : reason;
}

// ============================================================================
// ShortcutRules
// ============================================================================

ShortcutRules::ShortcutRules(ShortcutPlatform platform)
    : m_platform(platform)
{
}

QString ShortcutRules::keysText(QKeyCombination keys) {
#if !defined(Q_OS_MACOS)
    // Qt writes the Windows key and the Super key as Meta: they go by their own names, as
    // the rows of the system call them
    if (keys.keyboardModifiers().testFlag(Qt::MetaModifier)) {
#if defined(Q_OS_WIN)
        const QString metaKey = tr("Win");
#else
        const QString metaKey = tr("Super");
#endif
        const QKeyCombination rest =
            withModifiers(keys, keys.keyboardModifiers() & ~Qt::MetaModifier);
        return metaKey + QLatin1Char('+') + QKeySequence(rest).toString(QKeySequence::NativeText);
    }
#endif
    return QKeySequence(keys).toString(QKeySequence::NativeText);
}

QKeyCombination ShortcutRules::keysOf(const QKeyEvent* event) {
    int key = event->key();
    Qt::KeyboardModifiers modifiers =
        event->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);

    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_Mode_switch:
    case Qt::Key_unknown:
    case 0:
        return QKeyCombination(Qt::Key_unknown);
    case Qt::Key_Backtab:
        key = Qt::Key_Tab;
        modifiers |= Qt::ShiftModifier;
        break;
    default:
        break;
    }

    // Shift+1 comes as "!": the shortcut is Ctrl+!, as Qt matches it, not Ctrl+Shift+!
    if (modifiers.testFlag(Qt::ShiftModifier) && isCharacterKey(key) && key != Qt::Key_Space &&
        !QChar::isLetterOrNumber(static_cast<char32_t>(key))) {
        modifiers &= ~Qt::ShiftModifier;
    }
    return QKeyCombination(modifiers, static_cast<Qt::Key>(key));
}

std::vector<FixedKeyGroup> ShortcutRules::fixedGroups() const {
    std::vector<FixedKeyGroup> groups = textGroups();
    groups.push_back(systemGroup());
    return groups;
}

std::vector<FixedKeyGroup> ShortcutRules::textGroups() const {
    const bool mac = m_platform == ShortcutPlatform::MacOS;
    using A = TextKeyAction;
    std::vector<FixedKeyGroup> groups;

    // The text: the table of the editor, row by row; each key says what it does
    FixedKeyGroup text;
    text.title = tr("Fixed: in the text");
    const auto keysFor = [mac](const QString& label, std::initializer_list<A> actions) {
        FixedKeys fixed{label, {}, NONE, {}, {}, {}};
        for (const TextKey& textKey : editor::textKeys(mac)) {
            if (!textKey.extendSelection &&
                std::find(actions.begin(), actions.end(), textKey.action) != actions.end()) {
                fixed.keys.append(textKey.keys);
                fixed.keyReasons.append(textKeyReason(textKey.keys, textKey.action, false));
            }
        }
        return fixed;
    };
    const auto row = [&text](FixedKeys fixed) {
        if (!fixed.keys.isEmpty()) {
            text.rows.push_back(std::move(fixed));
        }
    };
    row(keysFor(tr("Character left / right"), {A::CharacterLeft, A::CharacterRight}));
    row(keysFor(tr("Word left / right"),
                {A::WordLeft, A::WordRight, A::WordStartBackward, A::WordEndForward}));
    row(keysFor(tr("Line up / down"), {A::LineUp, A::LineDown}));
    if (mac) {
        row(keysFor(tr("Start / end of the paragraph"),
                    {A::ParagraphUp, A::ParagraphEndForward, A::ParagraphStart, A::ParagraphEnd}));
    } else {
        row(keysFor(tr("Start of the paragraph / next paragraph"),
                    {A::ParagraphUp, A::ParagraphDown}));
    }
    row(keysFor(tr("Start / end of the line"), {A::LineStart, A::LineEnd}));
    row(keysFor(tr("Start / end of the text"), {A::DocumentStart, A::DocumentEnd}));
    row(keysFor(tr("Screen up / down"), {A::PageUp, A::PageDown}));
    row(keysFor(tr("Scrolling without the cursor"),
                {A::ScrollToStart, A::ScrollToEnd, A::ScrollPageUp, A::ScrollPageDown,
                 A::CenterCursor}));

    // Shift with every key above: two of them stand for all, as all would be too many to read
    FixedKeys selecting{tr("Selecting: Shift with the keys above"), {}, NONE, {}, {}, {}};
    for (const TextKey& textKey : editor::textKeys(mac)) {
        if (textKey.extendSelection) {
            selecting.keys.append(textKey.keys);
            selecting.keyReasons.append(textKeyReason(textKey.keys, textKey.action, true));
        }
    }
    const QKeyCombination byWord = keyWith((mac ? ALT : CTRL) | SHIFT, Qt::Key_Right);
    selecting.summary = keysText(keyWith(SHIFT, Qt::Key_Left)) + QStringLiteral(", ") +
                        keysText(byWord);
    row(std::move(selecting));

    row(keysFor(tr("Delete the character before / after the cursor"),
                {A::DeleteBackward, A::DeleteForward}));
    row(keysFor(tr("Delete the word before / after the cursor"),
                {A::DeleteWordBackward, A::DeleteWordForward, A::DeleteToWordStart,
                 A::DeleteToWordEnd}));
    row(keysFor(tr("Delete to the start of the line"), {A::DeleteToLineStart}));
    row(keysFor(tr("Delete the diacritic"), {A::DeleteDiacritic}));
    row(keysFor(tr("Cut to the end of the paragraph / paste what was cut"),
                {A::KillToParagraphEnd, A::Yank}));
    row(keysFor(tr("Swap the characters around the cursor"), {A::Transpose}));
    row(keysFor(tr("New paragraph"), {A::NewParagraph}));
    row(keysFor(tr("New paragraph after the cursor"), {A::OpenLine}));
    row(keysFor(tr("Undo (second shortcut)"), {A::Undo}));
    row(keysFor(tr("Redo (second shortcut)"), {A::Redo}));
    row(keysFor(tr("Cut, copy, paste (second shortcuts)"), {A::Cut, A::Copy, A::Paste}));
    if (!mac) {
        // The Menu key and Shift+F10 open it in every widget: the Menu key through Qt
        // (QContextMenuEvent), Shift+F10 through Windows and on Linux through WidgetKeys
        FixedKeys menu = keysFor(tr("Context menu"), {A::ContextMenu});
        const QKeyCombination menuKey = keyWith(NONE, Qt::Key_Menu);
        menu.keys.prepend(menuKey);
        menu.keyReasons.prepend(textKeyReason(menuKey, A::ContextMenu, false));
        row(std::move(menu));
    }
    groups.push_back(std::move(text));

    // The tabs of the books' texts: QTabWidget switches them with Ctrl+Tab (on macOS that
    // is Cmd+Tab, which the system takes)
    if (!mac) {
        FixedKeyGroup tabs;
        tabs.title = tr("Fixed: tabs");
        const QKeyCombination nextTab = keyWith(CTRL, Qt::Key_Tab);
        const QKeyCombination previousTab = keyWith(CTRL | SHIFT, Qt::Key_Tab);
        FixedKeys switching{tr("Next / previous tab"), {nextTab, previousTab}, NONE, {}, {}, {}};
        switching.keyReasons = {tr("%1 goes to the next tab.").arg(keysText(nextTab)),
                                tr("%1 goes back to the previous tab.").arg(keysText(previousTab))};
        tabs.rows.push_back(std::move(switching));
        groups.push_back(std::move(tabs));
    }

    // Esc closes what is open, wherever it is
    const QString escapeReason =
        tr("%1 closes the windows, the find bar and the annotation frame.");

    // The find bar (FindReplaceBar): on macOS Option with a letter types a character, so the
    // options there are Option+Cmd with the letter
    const Qt::KeyboardModifiers toggle = mac ? (CTRL | ALT) : ALT;
    FixedKeyGroup find;
    find.title = tr("Fixed: the find bar");
    FixedKeys matches{tr("Next / previous match"), {}, NONE, {}, {}, {}};
    for (const QKeyCombination enter : enterKeys(NONE)) {
        matches.keys.append(enter);
        matches.keyReasons.append(
            tr("In the find bar %1 goes to the next match.").arg(keysText(enter)));
    }
    for (const QKeyCombination enter : enterKeys(SHIFT)) {
        matches.keys.append(enter);
        matches.keyReasons.append(
            tr("In the find bar %1 goes to the previous match.").arg(keysText(enter)));
    }
    find.rows.push_back(std::move(matches));
    find.rows.push_back({tr("Replace (in the replace field)"), enterKeys(NONE), NONE, {},
                         tr("In the replace field %1 replaces the match."), {}});
    const QString turnsOn = tr("In the find bar %1 turns on “%2”.");
    const std::pair<QString, Qt::Key> options[] = {{tr("Match case"), Qt::Key_C},
                                                   {tr("Whole words"), Qt::Key_W},
                                                   {tr("Regular expression"), Qt::Key_R}};
    for (const auto& [label, key] : options) {
        const QKeyCombination optionKeys = keyWith(toggle, key);
        find.rows.push_back(
            {label, {optionKeys}, NONE, {}, turnsOn.arg(keysText(optionKeys), label), {}});
    }
    find.rows.push_back(
        {tr("Close the bar"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, escapeReason, {}});
    groups.push_back(std::move(find));

    // The frame an annotation is written in (AnnotationFrame)
    FixedKeyGroup frame;
    frame.title = tr("Fixed: the annotation frame");
    frame.rows.push_back({tr("Keep the text"), enterKeys(CTRL), NONE, {},
                          tr("In the annotation frame %1 keeps the text."), {}});
    frame.rows.push_back(
        {tr("Discard"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, escapeReason, {}});
    groups.push_back(std::move(frame));

    // The list of the Annotations panel takes its keys only while it has the keys
    FixedKeyGroup annotations;
    annotations.title = tr("Fixed: the Annotations panel");
    annotations.local = true;
    annotations.rows.push_back({tr("Choose an annotation"),
                                {keyWith(NONE, Qt::Key_Up), keyWith(NONE, Qt::Key_Down),
                                 keyWith(NONE, Qt::Key_Home), keyWith(NONE, Qt::Key_End)},
                                NONE, {}, {}, {}});
    QList<QKeyCombination> editKeys = enterKeys(NONE);
    editKeys.append(keyWith(NONE, Qt::Key_F2));
    annotations.rows.push_back({tr("Edit"), editKeys, NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Done / resolved"), {keyWith(NONE, Qt::Key_Space)}, NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Delete the annotation"), {keyWith(NONE, Qt::Key_Delete)}, NONE, {}, {}, {}});
    annotations.rows.push_back({tr("The annotation's menu"),
                                {keyWith(NONE, Qt::Key_Menu), keyWith(SHIFT, Qt::Key_F10)},
                                NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Back to the text"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, {}, {}});
    groups.push_back(std::move(annotations));

    FixedKeyGroup distractionFree;
    distractionFree.title = tr("Fixed: Distraction-Free");
    distractionFree.rows.push_back(
        {tr("Exit"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, escapeReason, {}});
    groups.push_back(std::move(distractionFree));

    return groups;
}

FixedKeyGroup ShortcutRules::systemGroup() const {
    FixedKeyGroup system;
    system.system = true;
    const QString switchReason =
        tr("%1 switches the windows of the system: it does not reach the program.");

    // Tab and Shift+Tab work as in every program: fixed keys rather than the system's
    const QKeyCombination nextField = keyWith(NONE, Qt::Key_Tab);
    const QKeyCombination previousField = keyWith(SHIFT, Qt::Key_Tab);
    FixedKeys fields{tr("Field by field"), {nextField, previousField}, NONE, {}, {}, {}};
    fields.keyReasons = {
        fixedKeyReason(tr("%1 goes to the next field.").arg(keysText(nextField))),
        fixedKeyReason(tr("%1 goes back to the previous field.").arg(keysText(previousField)))};
    fields.brief = tr("a fixed key");

    switch (m_platform) {
    case ShortcutPlatform::Windows: {
        system.title = tr("Fixed: the Windows system");
        system.rows.push_back(
            {tr("Close the window (also the Exit command)"), {keyWith(ALT, Qt::Key_F4)}, NONE, {},
             tr("%1 closes the window in Windows, so only the Exit command can have it."),
             "file.exit"});
        system.rows.push_back(
            {tr("Switch windows"),
             {keyWith(ALT, Qt::Key_Tab), keyWith(ALT | SHIFT, Qt::Key_Tab),
              keyWith(ALT, Qt::Key_Escape), keyWith(ALT | SHIFT, Qt::Key_Escape)},
             NONE, {}, switchReason, {}});
        system.rows.push_back({tr("Window menu"), {keyWith(ALT, Qt::Key_Space)}, NONE, {},
                               tr("%1 opens the window menu of the system."), {}});
        // The Windows key alone opens it too: the list names it, a shortcut cannot be it
        FixedKeys start{tr("Start menu"), {keyWith(CTRL, Qt::Key_Escape)}, NONE, {},
                        tr("%1 opens the Start menu."), {}};
        start.summary = tr("Win") + QStringLiteral(", ") + keysText(start.keys.first());
        system.rows.push_back(std::move(start));
        system.rows.push_back({tr("Task Manager"), {keyWith(CTRL | SHIFT, Qt::Key_Escape)}, NONE,
                               {}, tr("%1 opens the Task Manager."), {}});
        FixedKeys windowsKey{tr("Windows key"), {}, META, tr("Win+…"),
                             tr("%1: the Windows key belongs to the system: the program will "
                                "not get it."),
                             {}};
        windowsKey.brief = tr("the Windows key");
        system.rows.push_back(std::move(windowsKey));
        FixedKeys altGr{tr("Polish letters (AltGr)"), {}, CTRL | ALT, tr("Ctrl+Alt+…"),
                        tr("In Windows Ctrl+Alt is the AltGr key, with which ą, ć, ę… are typed. "
                           "A shortcut with Ctrl+Alt would type a letter or not work. Choose a "
                           "shortcut without Ctrl+Alt."),
                        {}};
        altGr.brief = tr("Ctrl+Alt is AltGr");
        system.rows.push_back(std::move(altGr));
        system.rows.push_back({tr("Screenshot"),
                               {keyWith(NONE, Qt::Key_Print), keyWith(ALT, Qt::Key_Print)}, NONE,
                               {}, tr("%1 takes a screenshot in the system."), {}});
        system.rows.push_back(std::move(fields));
        break;
    }
    case ShortcutPlatform::Linux: {
        system.title = tr("Fixed: the Linux desktop");
        system.rows.push_back(
            {tr("Close the window (also the Exit command)"), {keyWith(ALT, Qt::Key_F4)}, NONE, {},
             tr("%1 closes the window on the Linux desktops, so only the Exit command can have "
                "it."),
             "file.exit"});
        system.rows.push_back(
            {tr("Switch windows"), {keyWith(ALT, Qt::Key_Tab), keyWith(ALT | SHIFT, Qt::Key_Tab)},
             NONE, {}, switchReason, {}});
        system.rows.push_back({tr("Window menu"), {keyWith(ALT, Qt::Key_Space)}, NONE, {},
                               tr("%1 opens the window menu of the desktop."), {}});
        FixedKeys superKey{tr("Super key"), {}, META, tr("Super+…"),
                           tr("%1: the Super key belongs to the desktop: the program will not "
                              "get it."),
                           {}};
        superKey.brief = tr("the Super key");
        system.rows.push_back(std::move(superKey));
        FixedKeys desktopKeys{tr("Desktop keys (Ctrl+Alt)"), {}, CTRL | ALT, tr("Ctrl+Alt+…"),
                              tr("The Linux desktops take keys with Ctrl+Alt for themselves (the "
                                 "terminal, the workspaces, the consoles), and in Windows "
                                 "Ctrl+Alt is the AltGr key, with which ą, ć, ę… are typed. "
                                 "Choose a shortcut without Ctrl+Alt."),
                              {}};
        desktopKeys.brief = tr("Ctrl+Alt belongs to the desktop");
        system.rows.push_back(std::move(desktopKeys));
        system.rows.push_back({tr("Screenshot"),
                               {keyWith(NONE, Qt::Key_Print), keyWith(ALT, Qt::Key_Print)}, NONE,
                               {}, tr("%1 takes a screenshot on the desktop."), {}});
        system.rows.push_back(std::move(fields));
        break;
    }
    case ShortcutPlatform::MacOS:
        system.title = tr("Fixed: the macOS system");
        system.rows = {
            {tr("Quit (also the Exit command)"), {keyWith(CTRL, Qt::Key_Q)}, NONE, {},
             tr("%1 quits programs in macOS, so only the Exit command can have it."), "file.exit"},
            {tr("Switch programs"),
             {keyWith(CTRL, Qt::Key_Tab), keyWith(CTRL | SHIFT, Qt::Key_Tab)},
             NONE,
             {},
             tr("%1 switches the programs of the system: it does not reach the program."),
             {}},
            {tr("Hide Kalahari / the other programs"),
             {keyWith(CTRL, Qt::Key_H), keyWith(CTRL | ALT, Qt::Key_H)}, NONE, {},
             tr("%1 hides programs in macOS."), {}},
            {tr("Force Quit"), {keyWith(CTRL | ALT, Qt::Key_Escape)}, NONE, {},
             tr("%1 opens Force Quit Applications."), {}},
            {tr("Spotlight and input sources"),
             {keyWith(CTRL, Qt::Key_Space), keyWith(CTRL | ALT, Qt::Key_Space),
              keyWith(META, Qt::Key_Space), keyWith(META | ALT, Qt::Key_Space)},
             NONE, {}, tr("%1 opens Spotlight or switches the input source."), {}},
            {tr("Mission Control and Spaces"),
             {keyWith(META, Qt::Key_Up), keyWith(META, Qt::Key_Down), keyWith(META, Qt::Key_Left),
              keyWith(META, Qt::Key_Right)},
             NONE, {}, tr("%1 belongs to Mission Control: it does not reach the program."), {}},
            {tr("Keyboard navigation"),
             {keyWith(META, Qt::Key_F1), keyWith(META, Qt::Key_F2), keyWith(META, Qt::Key_F3),
              keyWith(META, Qt::Key_F4), keyWith(META, Qt::Key_F5), keyWith(META, Qt::Key_F6),
              keyWith(META, Qt::Key_F7), keyWith(META, Qt::Key_F8)},
             NONE,
             {},
             tr("%1 moves the keys to the menu bar, the Dock or the windows of macOS."),
             {}},
            // Cmd+Shift+3 comes as Cmd+# (keysOf())
            {tr("Screenshots"),
             {keyWith(CTRL | SHIFT, Qt::Key_3), keyWith(CTRL | SHIFT, Qt::Key_4),
              keyWith(CTRL | SHIFT, Qt::Key_5), keyWith(CTRL, Qt::Key_NumberSign),
              keyWith(CTRL, Qt::Key_Dollar), keyWith(CTRL, Qt::Key_Percent)},
             NONE, {}, tr("%1 takes a screenshot in macOS."), {}},
            {tr("Settings"), {keyWith(CTRL, Qt::Key_Comma)}, NONE, {},
             tr("%1 opens the settings of programs in macOS, so only the Settings command can "
                "have it."),
             "edit.settings"},
        };
        system.rows.push_back(std::move(fields));
        break;
    }
    return system;
}

QString ShortcutRules::textKeyReason(QKeyCombination keys, TextKeyAction action, bool selecting) {
    QString does;
    switch (action) {
    case TextKeyAction::None:
        break;
    case TextKeyAction::CharacterLeft:
    case TextKeyAction::CharacterRight:
        does = selecting ? tr("extends the selection by a character")
                         : tr("moves the cursor by a character");
        break;
    case TextKeyAction::WordLeft:
    case TextKeyAction::WordRight:
        does = selecting ? tr("extends the selection by a word") : tr("moves the cursor by a word");
        break;
    case TextKeyAction::WordStartBackward:
        does = selecting ? tr("extends the selection to the start of the word")
                         : tr("moves the cursor to the start of the word");
        break;
    case TextKeyAction::WordEndForward:
        does = selecting ? tr("extends the selection to the end of the word")
                         : tr("moves the cursor to the end of the word");
        break;
    case TextKeyAction::LineUp:
    case TextKeyAction::LineDown:
        does = selecting ? tr("extends the selection by a line") : tr("moves the cursor by a line");
        break;
    case TextKeyAction::LineStart:
        does = selecting ? tr("extends the selection to the start of the line")
                         : tr("moves the cursor to the start of the line");
        break;
    case TextKeyAction::LineEnd:
        does = selecting ? tr("extends the selection to the end of the line")
                         : tr("moves the cursor to the end of the line");
        break;
    case TextKeyAction::ParagraphStart:
    case TextKeyAction::ParagraphUp:
        does = selecting ? tr("extends the selection to the start of the paragraph")
                         : tr("moves the cursor to the start of the paragraph");
        break;
    case TextKeyAction::ParagraphDown:
        does = selecting ? tr("extends the selection to the next paragraph")
                         : tr("moves the cursor to the next paragraph");
        break;
    case TextKeyAction::ParagraphEnd:
    case TextKeyAction::ParagraphEndForward:
        does = selecting ? tr("extends the selection to the end of the paragraph")
                         : tr("moves the cursor to the end of the paragraph");
        break;
    case TextKeyAction::DocumentStart:
        does = selecting ? tr("extends the selection to the start of the text")
                         : tr("moves the cursor to the start of the text");
        break;
    case TextKeyAction::DocumentEnd:
        does = selecting ? tr("extends the selection to the end of the text")
                         : tr("moves the cursor to the end of the text");
        break;
    case TextKeyAction::PageUp:
    case TextKeyAction::PageDown:
        does = selecting ? tr("extends the selection by a screen")
                         : tr("scrolls the text by a screen");
        break;
    case TextKeyAction::ScrollPageUp:
    case TextKeyAction::ScrollPageDown:
        does = tr("scrolls the text by a screen without moving the cursor");
        break;
    case TextKeyAction::ScrollToStart:
        does = tr("scrolls the text to the start without moving the cursor");
        break;
    case TextKeyAction::ScrollToEnd:
        does = tr("scrolls the text to the end without moving the cursor");
        break;
    case TextKeyAction::CenterCursor:
        does = tr("scrolls the text so that the cursor is in the middle");
        break;
    case TextKeyAction::DeleteBackward:
        does = tr("deletes the character before the cursor");
        break;
    case TextKeyAction::DeleteForward:
        does = tr("deletes the character after the cursor");
        break;
    case TextKeyAction::DeleteDiacritic:
        does = tr("deletes the diacritic of the letter before the cursor");
        break;
    case TextKeyAction::DeleteWordBackward:
        does = tr("deletes the word before the cursor");
        break;
    case TextKeyAction::DeleteWordForward:
        does = tr("deletes the word after the cursor");
        break;
    case TextKeyAction::DeleteToWordStart:
        does = tr("deletes the text to the start of the word");
        break;
    case TextKeyAction::DeleteToWordEnd:
        does = tr("deletes the text to the end of the word");
        break;
    case TextKeyAction::DeleteToLineStart:
        does = tr("deletes the text to the start of the line");
        break;
    case TextKeyAction::KillToParagraphEnd:
        does = tr("cuts the text to the end of the paragraph");
        break;
    case TextKeyAction::NewParagraph:
        does = tr("starts a new paragraph");
        break;
    case TextKeyAction::OpenLine:
        does = tr("starts a new paragraph after the cursor");
        break;
    case TextKeyAction::Transpose:
        does = tr("swaps the characters around the cursor");
        break;
    case TextKeyAction::Yank:
        does = tr("pastes the text cut last");
        break;
    case TextKeyAction::Undo:
        does = tr("undoes (the second shortcut of Undo)");
        break;
    case TextKeyAction::Redo:
        does = tr("redoes (the second shortcut of Redo)");
        break;
    case TextKeyAction::Cut:
        does = tr("cuts (the second shortcut of Cut)");
        break;
    case TextKeyAction::Copy:
        does = tr("copies (the second shortcut of Copy)");
        break;
    case TextKeyAction::Paste:
        does = tr("pastes (the second shortcut of Paste)");
        break;
    case TextKeyAction::ContextMenu:
        does = tr("opens the context menu");
        break;
    }
    return tr("In the text %1 %2.").arg(keysText(keys), does);
}

QString ShortcutRules::fixedKeyReason(const QString& does) {
    return does + QLatin1Char(' ') + tr("It is a fixed key: choose another shortcut.");
}

QString ShortcutRules::typingReason(QKeyCombination keys) const {
    if (!isCharacterKey(keyCodeOf(keys))) {
        return {};
    }
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    if (m_platform == ShortcutPlatform::MacOS) {
        // Option (Alt) with a key types a character too
        if (modifiers & (CTRL | META)) {
            return {};
        }
        return tr("%1 types text. Add ⌘ or ⌃ to it, or choose one of the keys F1–F12.")
            .arg(keysText(keys));
    }
    if (modifiers & (CTRL | ALT | META)) {
        return {};
    }
    return tr("%1 types text. Add Ctrl or Alt to it, or choose one of the keys F1–F12.")
        .arg(keysText(keys));
}

QString ShortcutRules::desktopWarning(QKeyCombination keys) const {
    const int key = keyCodeOf(keys);
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    const bool functionKey = key >= Qt::Key_F1 && key <= Qt::Key_F12;
    if (m_platform == ShortcutPlatform::Linux && functionKey) {
        // KDE switches its desktops with Ctrl+F1–F4, Xfce with Ctrl+F1–F12
        if (modifiers == CTRL && key <= Qt::Key_F4) {
            return tr("In KDE and Xfce %1 switches the desktops, so there it may not reach the "
                      "program.")
                .arg(keysText(keys));
        }
        if (modifiers == CTRL) {
            return tr("In Xfce %1 switches the desktops, so there it may not reach the program.")
                .arg(keysText(keys));
        }
        // Alt+F4 closes the window: the rows of the system say who may have it
        if (modifiers == ALT && key <= Qt::Key_F10 && key != Qt::Key_F4) {
            return tr("The Linux desktops use %1 for their menus and windows, so there it may not "
                      "reach the program.")
                .arg(keysText(keys));
        }
    }
    if (m_platform == ShortcutPlatform::MacOS && key == Qt::Key_F11 && modifiers == NONE) {
        return tr("%1 shows the desktop in macOS, so it may not reach the program.")
            .arg(keysText(keys));
    }
    return {};
}

KeyCheck ShortcutRules::check(QKeyCombination pressed, const std::string& commandId) const {
    const QKeyCombination keys = withoutKeypad(pressed);

    if (const QString typing = typingReason(keys); !typing.isEmpty()) {
        return {KeyCheck::Result::Refused, typing, tr("types text")};
    }

    // What the keys do first: in the text, in the bars and frames, then in the system
    for (const FixedKeyGroup& group : fixedGroups()) {
        if (group.local) {
            continue;  // they work only in their list: a command may have them too
        }
        for (const FixedKeys& row : group.rows) {
            if (!row.covers(keys) || (!row.allowedFor.empty() && row.allowedFor == commandId)) {
                continue;
            }
            if (group.system) {
                return {KeyCheck::Result::Refused, row.reasonFor(keys),
                        row.brief.isEmpty() ? tr("a key of the system") : row.brief};
            }
            return {KeyCheck::Result::Refused, fixedKeyReason(row.reasonFor(keys)),
                    tr("a fixed key")};
        }
    }

    if (const QString warning = desktopWarning(keys); !warning.isEmpty()) {
        return {KeyCheck::Result::Warning, warning};
    }
    return {};
}

} // namespace gui
} // namespace kalahari
