/// @file widget_keys.cpp
/// @brief Implementation of WidgetKeys

#include "kalahari/gui/widget_keys.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QTextEdit>

namespace kalahari {
namespace gui {

namespace {

constexpr Qt::KeyboardModifiers NONE = Qt::NoModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers ALT = Qt::AltModifier;

/// A widget with text and the keys: the line of a field, a text, or a label whose text can
/// be selected
struct TextField {
    enum class Kind { None, Line, Text, Label };
    Kind kind = Kind::None;
    QWidget* widget = nullptr;  ///< The QLineEdit, the QTextEdit or QPlainTextEdit, the QLabel
    bool editable = false;
    bool selectable = false;    ///< Its text can be selected and copied
};

TextField textFieldOf(QWidget* widget) {
    // A spin box and a combo box one types into give their keys to the line in them
    QLineEdit* line = qobject_cast<QLineEdit*>(widget);
    if (line == nullptr) {
        if (auto* spinBox = qobject_cast<QAbstractSpinBox*>(widget)) {
            line = spinBox->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
        } else if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
            line = comboBox->lineEdit();
        }
    }
    if (line != nullptr) {
        return {TextField::Kind::Line, line, !line->isReadOnly(), true};
    }

    TextField::Kind kind = TextField::Kind::Text;
    Qt::TextInteractionFlags flags;
    if (const auto* text = qobject_cast<QTextEdit*>(widget)) {
        flags = text->textInteractionFlags();
    } else if (const auto* plain = qobject_cast<QPlainTextEdit*>(widget)) {
        flags = plain->textInteractionFlags();
    } else if (const auto* label = qobject_cast<QLabel*>(widget)) {
        flags = label->textInteractionFlags();
        kind = TextField::Kind::Label;
    } else {
        return {};
    }
    const bool editable = flags.testFlag(Qt::TextEditable);
    const bool selectable =
        editable || (flags & (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)) != 0;
    if (kind == TextField::Kind::Label && !selectable) {
        return {};
    }
    return {kind, widget, editable, selectable};
}

/// A field that records shortcuts takes every key while it records
bool records(const QWidget* widget) {
    const auto* recorder = qobject_cast<const ShortcutRecorder*>(widget);
    return recorder != nullptr && recorder->isRecording();
}

/// The keys that copy and select all, which a field with text keeps for itself
const QList<QKeyCombination>& copyKeys() {
    static const QList<QKeyCombination> keys = {QKeyCombination(CTRL, Qt::Key_C),
                                                QKeyCombination(CTRL, Qt::Key_Insert),
                                                QKeyCombination(CTRL, Qt::Key_A)};
    return keys;
}

/// A copy of a key event, of another type or as a repeated key
QKeyEvent copyOf(const QKeyEvent* event, QEvent::Type type, bool autoRepeat) {
    return QKeyEvent(type, event->key(), event->modifiers(), event->nativeScanCode(),
                     event->nativeVirtualKey(), event->nativeModifiers(), event->text(),
                     autoRepeat, event->count(), event->device());
}

/// Whether the widget itself, without the filters of the program, keeps the key from the
/// window's shortcuts (asking it changes nothing)
bool widgetKeeps(QWidget* widget, const QKeyEvent* event) {
    QKeyEvent probe = copyOf(event, QEvent::ShortcutOverride, event->isAutoRepeat());
    probe.ignore();
    static_cast<QObject*>(widget)->event(&probe);  // the widget's own event(), public here
    return probe.isAccepted();
}

void undoOrRedo(QWidget* widget, bool redo) {
    if (auto* line = qobject_cast<QLineEdit*>(widget)) {
        redo ? line->redo() : line->undo();
    } else if (auto* text = qobject_cast<QTextEdit*>(widget)) {
        redo ? text->redo() : text->undo();
    } else if (auto* plain = qobject_cast<QPlainTextEdit*>(widget)) {
        redo ? plain->redo() : plain->undo();
    }
}

/// Whether a line completes the text in itself: there an arrow keeps what was completed
bool completesInline(const QLineEdit* line) {
    const QCompleter* completer = line->completer();
    return completer != nullptr && completer->completionMode() == QCompleter::InlineCompletion;
}

/// The context of Qt's words in the context menu of a field, or none for another menu: the
/// menu of a line belongs to the line, of a label to the label, and of a text to its
/// viewport (or to the text, when the program asks the text for the menu)
const char* menuContextOf(const QMenu* menu) {
    const QObject* owner = menu->parent();
    if (qobject_cast<const QLineEdit*>(owner) != nullptr) {
        return "QLineEdit";
    }
    if (qobject_cast<const QLabel*>(owner) != nullptr) {
        return "QWidgetTextControl";
    }
    if (owner != nullptr) {
        const auto* area = qobject_cast<const QAbstractScrollArea*>(owner->parent());
        if (area != nullptr && area->viewport() == owner) {
            owner = area;
        }
    }
    if (qobject_cast<const QTextEdit*>(owner) != nullptr ||
        qobject_cast<const QPlainTextEdit*>(owner) != nullptr) {
        return "QWidgetTextControl";
    }
    return nullptr;
}

}  // namespace

WidgetKeys::WidgetKeys(ShortcutPlatform platform, QObject* parent)
    : QObject(parent)
    , m_active(platform != ShortcutPlatform::MacOS)
    , m_linux(platform == ShortcutPlatform::Linux) {}

const QList<QKeyCombination>& WidgetKeys::linuxOnlyKeys() {
    static const QList<QKeyCombination> keys = {
        QKeyCombination(CTRL, Qt::Key_D),          QKeyCombination(CTRL, Qt::Key_E),
        QKeyCombination(CTRL, Qt::Key_K),          QKeyCombination(CTRL, Qt::Key_U),
        QKeyCombination(CTRL | SHIFT, Qt::Key_A),  QKeyCombination(CTRL | SHIFT, Qt::Key_Insert),
        QKeyCombination(NONE, Qt::Key_F14),        QKeyCombination(NONE, Qt::Key_F16),
        QKeyCombination(NONE, Qt::Key_F18),        QKeyCombination(NONE, Qt::Key_F20)};
    return keys;
}

const QList<QKeyCombination>& WidgetKeys::windowsUndoKeys() {
    static const QList<QKeyCombination> keys = {QKeyCombination(ALT, Qt::Key_Backspace)};
    return keys;
}

const QList<QKeyCombination>& WidgetKeys::windowsRedoKeys() {
    static const QList<QKeyCombination> keys = {QKeyCombination(CTRL, Qt::Key_Y),
                                                QKeyCombination(ALT | SHIFT, Qt::Key_Backspace)};
    return keys;
}

bool WidgetKeys::eventFilter(QObject* watched, QEvent* event) {
    if (!m_active || event == m_delivering || !watched->isWidgetType()) {
        return false;
    }
    auto* widget = static_cast<QWidget*>(watched);
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        return shortcutOverride(widget, static_cast<QKeyEvent*>(event));
    case QEvent::KeyPress:
        return keyPress(widget, static_cast<QKeyEvent*>(event));
    case QEvent::Polish:
    case QEvent::Show:
        // Polished before the menu takes its size; shown, in case it was polished earlier
        if (auto* menu = qobject_cast<QMenu*>(widget)) {
            showKeys(menu);
        }
        return false;
    default:
        return false;
    }
}

bool WidgetKeys::shortcutOverride(QWidget* widget, QKeyEvent* event) {
    if (records(widget)) {
        return false;
    }
    const TextField field = textFieldOf(widget);
    if (field.kind == TextField::Kind::None) {
        return false;
    }
    const QKeyCombination keys = ShortcutRules::keysOf(event);

    if (m_linux) {
        // Qt's line takes these on Linux (Ctrl+E: the end of the line, Ctrl+U: deleting it);
        // on Windows they are the window's (Ctrl+E centers the paragraph, Ctrl+U underlines).
        // A text does not take Ctrl+E or Ctrl+U, and the frame of an annotation keeps every
        // key itself, so the texts are left as they are
        if (field.kind == TextField::Kind::Line && linuxOnlyKeys().contains(keys) &&
            widgetKeeps(widget, event)) {
            event->ignore();
            return true;
        }

        // Undo and redo with the keys of Windows: the field's when it can be edited
        if (windowsUndoKeys().contains(keys) || windowsRedoKeys().contains(keys)) {
            if (field.kind != TextField::Kind::Label && field.editable) {
                event->accept();
                return true;
            }
            return false;
        }
    }

    // Copying and selecting all are the field's, also in a read-only one: Qt leaves them to
    // the window there, and Ctrl+C in the log copied the selection of the book's text
    if (field.selectable && copyKeys().contains(keys)) {
        event->accept();
        return true;
    }
    return false;
}

bool WidgetKeys::keyPress(QWidget* widget, QKeyEvent* event) {
    // On Windows Qt gives the widgets these keys itself
    if (!m_linux || records(widget)) {
        return false;
    }
    const QKeyCombination keys = ShortcutRules::keysOf(event);
    if (keys == QKeyCombination(SHIFT, Qt::Key_F10)) {
        return openContextMenu(widget, event);
    }

    const TextField field = textFieldOf(widget);
    const bool hasTextKeys = field.kind != TextField::Kind::None ||
                             qobject_cast<QAbstractItemView*>(widget) != nullptr;
    if (hasTextKeys && linuxOnlyKeys().contains(keys)) {
        event->ignore();  // as on Windows: nothing here, on to the window
        return true;
    }

    if (field.kind == TextField::Kind::Line || field.kind == TextField::Kind::Text) {
        const bool undo = windowsUndoKeys().contains(keys);
        if (undo || windowsRedoKeys().contains(keys)) {
            if (field.editable) {
                undoOrRedo(field.widget, !undo);
                event->accept();
            } else {
                // As on Windows: a read-only line takes the key and does nothing, a read-only
                // text passes it on
                event->setAccepted(field.kind == TextField::Kind::Line);
            }
            return true;
        }
    }

    // An arrow over a selection moves the cursor from where it is, not from the edge of the
    // selection
    if (field.kind == TextField::Kind::Line &&
        (keys == QKeyCombination(NONE, Qt::Key_Left) ||
         keys == QKeyCombination(NONE, Qt::Key_Right))) {
        auto* line = static_cast<QLineEdit*>(field.widget);
        if (line->hasSelectedText() && !completesInline(line)) {
            line->deselect();
        }
        return false;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        return passEnterOn(widget, event);
    }
    return false;
}

bool WidgetKeys::openContextMenu(QWidget* widget, QKeyEvent* event) {
    // The key first, as on Windows: a widget that takes Shift+F10 (the editor, the list of
    // the annotations) opens its menu itself
    QKeyEvent key = copyOf(event, event->type(), event->isAutoRepeat());
    bool gone = false;  // the widget may go while it handles the key
    const QMetaObject::Connection watching =
        connect(widget, &QObject::destroyed, this, [&gone] { gone = true; });
    deliverAgain(widget, &key);
    disconnect(watching);

    // Then the context menu of the widget with the keys, at its cursor, as Windows sends it
    if (!key.isAccepted() && !gone && widget->isEnabled()) {
        const QPoint pos = widget->inputMethodQuery(Qt::ImCursorRectangle).toRect().center();
        QContextMenuEvent menuEvent(QContextMenuEvent::Keyboard, pos, widget->mapToGlobal(pos),
                                    event->modifiers());
        QCoreApplication::sendEvent(widget, &menuEvent);
    }
    event->accept();
    return true;
}

bool WidgetKeys::passEnterOn(QWidget* widget, QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return false;
    }

    // On GNOME and the desktops like it Enter opens the list of a combo box one cannot type
    // into; on Windows it goes on to the window and its default button
    if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
        if (comboBox->isEditable()) {
            return false;
        }
        event->ignore();
        return true;
    }

    // There Enter also presses the button with the keys; on Windows only the default button
    // (and a button that becomes it with the keys) takes Enter
    bool pressed = false;
    if (const auto* pushButton = qobject_cast<QPushButton*>(widget)) {
        pressed = !pushButton->autoDefault() && !pushButton->isDefault();
    } else if (qobject_cast<QAbstractButton*>(widget) != nullptr) {
        pressed = true;
    } else if (const auto* groupBox = qobject_cast<QGroupBox*>(widget)) {
        pressed = groupBox->isCheckable();
    }
    if (!pressed) {
        return false;
    }

    // Qt presses a button only with the first press of a key: Enter as a repeated key goes
    // through the button's filters (the frame of an annotation saves with it) and on to the
    // window, as on Windows
    QKeyEvent repeated = copyOf(event, event->type(), true);
    deliverAgain(widget, &repeated);
    event->accept();  // the copy went as far as the key goes
    return true;
}

void WidgetKeys::deliverAgain(QWidget* widget, QKeyEvent* event) {
    const QScopedValueRollback<const QEvent*> delivering(m_delivering, event);
    QCoreApplication::sendEvent(widget, event);
}

void WidgetKeys::showKeys(QMenu* menu) {
    if (QCoreApplication::testAttribute(Qt::AA_DontShowShortcutsInContextMenus)) {
        return;
    }

    // Qt's fields name the commands of their menus in Qt's words and show the keys only of
    // those no command of the window has (and on Linux Ctrl+Shift+Z for Redo)
    const char* context = menuContextOf(menu);
    if (context == nullptr) {
        return;
    }

    struct Item {
        const char* text;
        QKeyCombination keys;
    };
    static const Item items[] = {
        {"&Undo", QKeyCombination(CTRL, Qt::Key_Z)}, {"&Redo", QKeyCombination(CTRL, Qt::Key_Y)},
        {"Cu&t", QKeyCombination(CTRL, Qt::Key_X)},  {"&Copy", QKeyCombination(CTRL, Qt::Key_C)},
        {"&Paste", QKeyCombination(CTRL, Qt::Key_V)},
        {"Select All", QKeyCombination(CTRL, Qt::Key_A)}};
    for (QAction* action : menu->actions()) {
        const QString label = action->text().section(QLatin1Char('\t'), 0, 0);
        for (const Item& item : items) {
            if (label == QCoreApplication::translate(context, item.text)) {
                action->setText(label + QLatin1Char('\t') + ShortcutRules::keysText(item.keys));
                break;
            }
        }
    }
}

void installWidgetKeys(QApplication& app) {
    // macOS keeps the keys of its own fields (see Settings > Keyboard Shortcuts)
    if (currentShortcutPlatform() == ShortcutPlatform::MacOS) {
        return;
    }
    app.installEventFilter(new WidgetKeys(currentShortcutPlatform(), &app));
}

} // namespace gui
} // namespace kalahari
