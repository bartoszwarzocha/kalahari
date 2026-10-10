/// @file shortcut_recorder.cpp
/// @brief Implementation of ShortcutRecorder

#include "kalahari/gui/widgets/shortcut_recorder.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QFocusEvent>
#include <QKeyEvent>

namespace kalahari {
namespace gui {

ShortcutRecorder::ShortcutRecorder(QWidget* parent)
    : QLineEdit(parent)
{
}

void ShortcutRecorder::startRecording(const QString& prompt, bool keepRecording) {
    if (!m_recording) {
        m_wasReadOnly = isReadOnly();
        m_placeholder = placeholderText();
        m_text = text();
    }
    m_recording = true;
    m_keepRecording = keepRecording;
    m_recordedText.clear();
    // Typing does not change the field: the keys are recorded instead
    setReadOnly(true);
    clear();
    setPlaceholderText(prompt);
    setFocus(Qt::OtherFocusReason);
}

void ShortcutRecorder::stopRecording() {
    if (!m_recording) {
        return;
    }
    finish();
    setText(m_text);
}

void ShortcutRecorder::finish() {
    m_recording = false;
    setReadOnly(m_wasReadOnly);
    setPlaceholderText(m_placeholder);
}

bool ShortcutRecorder::event(QEvent* event) {
    if (m_recording) {
        switch (event->type()) {
        case QEvent::ShortcutOverride:
            // No shortcut, mnemonic or default button takes the key: it is recorded
            event->accept();
            return true;
        case QEvent::KeyPress:
            // Also Tab and Shift+Tab, which would move to the next field
            keyPressEvent(static_cast<QKeyEvent*>(event));
            return true;
        default:
            break;
        }
    }
    return QLineEdit::event(event);
}

void ShortcutRecorder::keyPressEvent(QKeyEvent* event) {
    if (!m_recording) {
        QLineEdit::keyPressEvent(event);
        return;
    }
    event->accept();

    const Qt::KeyboardModifiers modifiers =
        event->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    if (event->key() == Qt::Key_Escape && modifiers == Qt::NoModifier) {
        stopRecording();
        emit recordingStopped(true);
        return;
    }

    const QKeyCombination keys = ShortcutRules::keysOf(event);
    if (keys.key() == Qt::Key_unknown) {
        showModifiers(modifiers);  // a modifier alone: wait for the key
        return;
    }

    if (m_keepRecording) {
        m_recordedText = ShortcutRules::keysText(keys);
        setText(m_recordedText);
    } else {
        finish();
    }
    emit recorded(keys);
}

void ShortcutRecorder::keyReleaseEvent(QKeyEvent* event) {
    if (!m_recording) {
        QLineEdit::keyReleaseEvent(event);
        return;
    }
    event->accept();
    // The modifiers still held down: the systems differ in whether the released one is
    // still among the event's
    Qt::KeyboardModifiers held =
        event->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    switch (event->key()) {
    case Qt::Key_Shift:
        held &= ~Qt::ShiftModifier;
        break;
    case Qt::Key_Control:
        held &= ~Qt::ControlModifier;
        break;
    case Qt::Key_Alt:
        held &= ~Qt::AltModifier;
        break;
    case Qt::Key_Meta:
        held &= ~Qt::MetaModifier;
        break;
    default:
        break;
    }
    showModifiers(held);
}

void ShortcutRecorder::focusOutEvent(QFocusEvent* event) {
    // A key's own popup (the system's) is not leaving the field
    if (m_recording && event->reason() != Qt::PopupFocusReason) {
        finish();
        if (!m_keepRecording) {
            setText(m_text);
        }
        emit recordingStopped(false);
    }
    QLineEdit::focusOutEvent(event);
}

void ShortcutRecorder::showModifiers(Qt::KeyboardModifiers modifiers) {
    if (modifiers == Qt::NoModifier) {
        // The keys recorded last, or the prompt again
        setText(m_recordedText);
        return;
    }
    // The modifiers as the system writes them: "Ctrl+Shift+" or "⇧⌘", then the key to come
    const QString withKey = ShortcutRules::keysText(QKeyCombination(modifiers, Qt::Key_A));
    const QString key = ShortcutRules::keysText(QKeyCombination(Qt::Key_A));
    setText(withKey.chopped(key.size()) + QStringLiteral("…"));
}

} // namespace gui
} // namespace kalahari
