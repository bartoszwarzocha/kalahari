/// @file shortcut_rules.cpp
/// @brief Implementation of ShortcutRules

#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/editor/text_keys.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QStringList>

#include <initializer_list>

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

/// The keypad and the group switch make no difference to a shortcut
QKeyCombination withoutKeypad(QKeyCombination keys) {
    return QKeyCombination(
        keys.keyboardModifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier), keys.key());
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

/// The keys of the text table that do one of the actions
QList<QKeyCombination> textKeysOf(bool macOS, std::initializer_list<TextKeyAction> actions,
                                  bool selecting = false) {
    QList<QKeyCombination> keys;
    for (const TextKey& textKey : editor::textKeys(macOS)) {
        if (textKey.extendSelection != selecting) {
            continue;
        }
        for (const TextKeyAction action : actions) {
            if (textKey.action == action) {
                keys.append(textKey.keys);
                break;
            }
        }
    }
    return keys;
}

/// The keys of the text table that select
QList<QKeyCombination> selectingKeys(bool macOS) {
    QList<QKeyCombination> keys;
    for (const TextKey& textKey : editor::textKeys(macOS)) {
        if (textKey.extendSelection) {
            keys.append(textKey.keys);
        }
    }
    return keys;
}

}  // namespace

// ============================================================================
// FixedKeys
// ============================================================================

bool FixedKeys::covers(QKeyCombination pressed) const {
    const QKeyCombination keys = withoutKeypad(pressed);
    if (anyKeyWith != Qt::NoModifier && (keys.keyboardModifiers() & anyKeyWith) == anyKeyWith) {
        return true;
    }
    for (const QKeyCombination fixed : this->keys) {
        if (withoutKeypad(fixed) == keys) {
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
        const QKeyCombination rest(keys.keyboardModifiers() & ~Qt::MetaModifier, keys.key());
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

    // The text: the table of the editor, row by row
    FixedKeyGroup text;
    text.title = tr("In the text");
    const auto row = [&text](const QString& label, const QList<QKeyCombination>& keys) {
        if (!keys.isEmpty()) {
            text.rows.push_back({label, keys, NONE, {}, {}, {}});
        }
    };
    row(tr("Character left / right"), textKeysOf(mac, {A::CharacterLeft, A::CharacterRight}));
    row(tr("Word left / right"), textKeysOf(mac, {A::WordLeft, A::WordRight, A::WordStartBackward,
                                                  A::WordEndForward}));
    row(tr("Line up / down"), textKeysOf(mac, {A::LineUp, A::LineDown}));
    if (mac) {
        row(tr("Start / end of the paragraph"),
            textKeysOf(mac, {A::ParagraphUp, A::ParagraphEndForward, A::ParagraphStart,
                             A::ParagraphEnd}));
    } else {
        row(tr("Start of the paragraph / next paragraph"),
            textKeysOf(mac, {A::ParagraphUp, A::ParagraphDown}));
    }
    row(tr("Start / end of the line"), textKeysOf(mac, {A::LineStart, A::LineEnd}));
    row(tr("Start / end of the text"), textKeysOf(mac, {A::DocumentStart, A::DocumentEnd}));
    row(tr("Screen up / down"), textKeysOf(mac, {A::PageUp, A::PageDown}));
    row(tr("Scrolling without the cursor"),
        textKeysOf(mac, {A::ScrollToStart, A::ScrollToEnd, A::ScrollPageUp, A::ScrollPageDown,
                         A::CenterCursor}));
    // Shift with every key above: in words, as a list they would be too many to read
    text.rows.push_back({tr("Selecting"), selectingKeys(mac), NONE, {}, {}, {},
                         tr("Shift with the keys that move the cursor")});
    row(tr("Delete the character before / after the cursor"),
        textKeysOf(mac, {A::DeleteBackward, A::DeleteForward}));
    row(tr("Delete the word before / after the cursor"),
        textKeysOf(mac, {A::DeleteWordBackward, A::DeleteWordForward, A::DeleteToWordStart,
                         A::DeleteToWordEnd}));
    row(tr("Delete to the start of the line"), textKeysOf(mac, {A::DeleteToLineStart}));
    row(tr("Delete the diacritic"), textKeysOf(mac, {A::DeleteDiacritic}));
    row(tr("Cut to the end of the paragraph / paste what was cut"),
        textKeysOf(mac, {A::KillToParagraphEnd, A::Yank}));
    row(tr("Swap the characters around the cursor"), textKeysOf(mac, {A::Transpose}));
    row(tr("New paragraph"), textKeysOf(mac, {A::NewParagraph}));
    row(tr("New paragraph after the cursor"), textKeysOf(mac, {A::OpenLine}));
    row(tr("Undo (second shortcut)"), textKeysOf(mac, {A::Undo}));
    row(tr("Redo (second shortcut)"), textKeysOf(mac, {A::Redo}));
    row(tr("Cut, copy, paste (second shortcuts)"), textKeysOf(mac, {A::Cut, A::Copy, A::Paste}));
    if (!mac) {
        // The Menu key opens it in every widget (QContextMenuEvent), Shift+F10 in the editor
        QList<QKeyCombination> menuKeys = {keyWith(NONE, Qt::Key_Menu)};
        menuKeys.append(textKeysOf(mac, {A::ContextMenu}));
        row(tr("Context menu"), menuKeys);
    }
    groups.push_back(std::move(text));

    // The tabs of the books' texts: QTabWidget switches them with Ctrl+Tab (on macOS that
    // is Cmd+Tab, which the system takes)
    if (!mac) {
        FixedKeyGroup tabs;
        tabs.title = tr("Tabs");
        tabs.rows.push_back({tr("Next / previous tab"),
                             {keyWith(CTRL, Qt::Key_Tab), keyWith(CTRL | SHIFT, Qt::Key_Tab)},
                             NONE, {}, {}, {}});
        groups.push_back(std::move(tabs));
    }

    // The find bar (FindReplaceBar): on macOS Option with a letter types a character, so the
    // options there are Option+Cmd with the letter
    const Qt::KeyboardModifiers toggle = mac ? (CTRL | ALT) : ALT;
    FixedKeyGroup find;
    find.title = tr("Find bar");
    QList<QKeyCombination> nextKeys = enterKeys(NONE);
    nextKeys.append(enterKeys(SHIFT));
    find.rows.push_back({tr("Next / previous match"), nextKeys, NONE, {}, {}, {}});
    find.rows.push_back({tr("Replace (in the replace field)"), enterKeys(NONE), NONE, {}, {}, {}});
    find.rows.push_back({tr("Match case"), {keyWith(toggle, Qt::Key_C)}, NONE, {}, {}, {}});
    find.rows.push_back({tr("Whole words"), {keyWith(toggle, Qt::Key_W)}, NONE, {}, {}, {}});
    find.rows.push_back({tr("Regular expression"), {keyWith(toggle, Qt::Key_R)}, NONE, {}, {}, {}});
    find.rows.push_back({tr("Close the bar"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, {}, {}});
    groups.push_back(std::move(find));

    // The frame an annotation is written in (AnnotationFrame)
    FixedKeyGroup frame;
    frame.title = tr("Annotation frame");
    frame.rows.push_back({tr("Keep the text"), enterKeys(CTRL), NONE, {}, {}, {}});
    frame.rows.push_back({tr("Drop the text"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, {}, {}});
    groups.push_back(std::move(frame));

    // The list of the Annotations panel takes its keys only while it has the keys
    FixedKeyGroup annotations;
    annotations.title = tr("Annotations panel");
    annotations.local = true;
    annotations.rows.push_back({tr("Choose an annotation"),
                                {keyWith(NONE, Qt::Key_Up), keyWith(NONE, Qt::Key_Down),
                                 keyWith(NONE, Qt::Key_Home), keyWith(NONE, Qt::Key_End)},
                                NONE, {}, {}, {}});
    QList<QKeyCombination> editKeys = enterKeys(NONE);
    editKeys.append(keyWith(NONE, Qt::Key_F2));
    annotations.rows.push_back({tr("Edit"), editKeys, NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Done or resolved"), {keyWith(NONE, Qt::Key_Space)}, NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Delete the annotation"), {keyWith(NONE, Qt::Key_Delete)}, NONE, {}, {}, {}});
    annotations.rows.push_back({tr("The annotation's menu"),
                                {keyWith(NONE, Qt::Key_Menu), keyWith(SHIFT, Qt::Key_F10)},
                                NONE, {}, {}, {}});
    annotations.rows.push_back(
        {tr("Back to the text"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, {}, {}});
    groups.push_back(std::move(annotations));

    FixedKeyGroup distractionFree;
    distractionFree.title = tr("Distraction-Free");
    distractionFree.rows.push_back(
        {tr("Leave it"), {keyWith(NONE, Qt::Key_Escape)}, NONE, {}, {}, {}});
    groups.push_back(std::move(distractionFree));

    return groups;
}

FixedKeyGroup ShortcutRules::systemGroup() const {
    FixedKeyGroup system;
    system.system = true;
    const QList<QKeyCombination> fieldKeys = {keyWith(NONE, Qt::Key_Tab),
                                              keyWith(SHIFT, Qt::Key_Tab)};
    const QString fieldReason = tr("%1 moves between the fields of the windows.");
    const QString switchReason =
        tr("%1 switches the windows of the system: it does not reach the program.");

    switch (m_platform) {
    case ShortcutPlatform::Windows:
        system.title = tr("Windows");
        system.rows = {
            {tr("Close the window (also the Exit command)"), {keyWith(ALT, Qt::Key_F4)}, NONE, {},
             tr("Alt+F4 closes the window in Windows, so only the Exit command can have it."),
             "file.exit"},
            {tr("Switch windows"),
             {keyWith(ALT, Qt::Key_Tab), keyWith(ALT | SHIFT, Qt::Key_Tab),
              keyWith(ALT, Qt::Key_Escape), keyWith(ALT | SHIFT, Qt::Key_Escape)},
             NONE, {}, switchReason, {}},
            {tr("Window menu"), {keyWith(ALT, Qt::Key_Space)}, NONE, {},
             tr("%1 opens the window menu of the system."), {}},
            {tr("Start menu"), {keyWith(CTRL, Qt::Key_Escape)}, NONE, {},
             tr("%1 opens the Start menu."), {}},
            {tr("Task Manager"), {keyWith(CTRL | SHIFT, Qt::Key_Escape)}, NONE, {},
             tr("%1 opens the Task Manager."), {}},
            {tr("Windows key"), {}, META, tr("Win+…"),
             tr("%1: the Windows key belongs to the system, the program does not get it."), {}},
            {tr("Polish letters (AltGr)"), {}, CTRL | ALT, tr("Ctrl+Alt+…"),
             tr("In Windows Ctrl+Alt is AltGr, with which ą, ć, ę… are typed: a shortcut with "
                "Ctrl+Alt would type a letter or not work. Choose a shortcut without Ctrl+Alt."),
             {}},
            {tr("Screenshot"), {keyWith(NONE, Qt::Key_Print), keyWith(ALT, Qt::Key_Print)},
             NONE, {}, tr("%1 takes a screenshot in the system."), {}},
            {tr("Field by field"), fieldKeys, NONE, {}, fieldReason, {}},
        };
        break;
    case ShortcutPlatform::Linux:
        system.title = tr("Linux desktop");
        system.rows = {
            {tr("Close the window (also the Exit command)"), {keyWith(ALT, Qt::Key_F4)}, NONE, {},
             tr("Alt+F4 closes the window on the Linux desktops, so only the Exit command can "
                "have it."),
             "file.exit"},
            {tr("Switch windows"), {keyWith(ALT, Qt::Key_Tab), keyWith(ALT | SHIFT, Qt::Key_Tab)},
             NONE, {}, switchReason, {}},
            {tr("Window menu"), {keyWith(ALT, Qt::Key_Space)}, NONE, {},
             tr("%1 opens the window menu of the desktop."), {}},
            {tr("Super key"), {}, META, tr("Super+…"),
             tr("%1: the Super key belongs to the desktop, the program does not get it."), {}},
            {tr("Desktop keys (Ctrl+Alt)"), {}, CTRL | ALT, tr("Ctrl+Alt+…"),
             tr("The Linux desktops take keys with Ctrl+Alt for themselves (the terminal, the "
                "workspaces, the consoles), and in Windows Ctrl+Alt is AltGr, with which ą, ć, "
                "ę… are typed. Choose a shortcut without Ctrl+Alt."),
             {}},
            {tr("Screenshot"), {keyWith(NONE, Qt::Key_Print), keyWith(ALT, Qt::Key_Print)},
             NONE, {}, tr("%1 takes a screenshot on the desktop."), {}},
            {tr("Field by field"), fieldKeys, NONE, {}, fieldReason, {}},
        };
        break;
    case ShortcutPlatform::MacOS:
        system.title = tr("macOS");
        system.rows = {
            {tr("Quit (also the Exit command)"), {keyWith(CTRL, Qt::Key_Q)}, NONE, {},
             tr("%1 quits programs in macOS, so only the Exit command can have it."), "file.exit"},
            {tr("Switch programs"),
             {keyWith(CTRL, Qt::Key_Tab), keyWith(CTRL | SHIFT, Qt::Key_Tab)},
             NONE,
             {},
             tr("%1 switches the programs of the system: it does not reach Kalahari."),
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
            {tr("Field by field"), fieldKeys, NONE, {}, fieldReason, {}},
        };
        break;
    }
    return system;
}

QString ShortcutRules::typingReason(QKeyCombination keys) const {
    if (!isCharacterKey(keys.key())) {
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
    const int key = keys.key();
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    const bool functionKey = key >= Qt::Key_F1 && key <= Qt::Key_F12;
    if (m_platform == ShortcutPlatform::Linux && functionKey) {
        if (modifiers == CTRL) {
            return tr("In KDE and Xfce %1 switches the desktops, so there it may not reach the "
                      "program.")
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
    const QString text = keysText(keys);

    if (const QString typing = typingReason(keys); !typing.isEmpty()) {
        return {KeyCheck::Result::Refused, typing};
    }

    // Where the keys work: the first thing they do in each place
    QStringList places;
    for (const FixedKeyGroup& group : fixedGroups()) {
        if (group.local) {
            continue;
        }
        for (const FixedKeys& row : group.rows) {
            if (!row.covers(keys) || (!row.allowedFor.empty() && row.allowedFor == commandId)) {
                continue;
            }
            if (group.system) {
                const QString reason = row.reason.contains(QLatin1String("%1"))
                    ? row.reason.arg(text)
                    : row.reason;
                return {KeyCheck::Result::Refused, reason};
            }
            // As the list of the shortcuts shows it: the place, then what the keys do there
            places.append(QStringLiteral("%1 › %2").arg(group.title, row.label));
            break;
        }
    }
    if (!places.isEmpty()) {
        return {KeyCheck::Result::Refused,
                tr("%1 is a fixed key (%2). Choose another shortcut.")
                    .arg(text, places.join(QStringLiteral("; ")))};
    }

    if (const QString warning = desktopWarning(keys); !warning.isEmpty()) {
        return {KeyCheck::Result::Warning, warning};
    }
    return {};
}

} // namespace gui
} // namespace kalahari
