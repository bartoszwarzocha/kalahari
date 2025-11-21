/// @file command.cpp
/// @brief Command Registry data structures implementation (Qt6)

#include "kalahari/gui/command.h"
#include <QFile>
#include <QImageReader>
#include <cctype>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// IconSet Implementation
// ============================================================================

IconSet::IconSet(const QString& path) {
    // Load image from file
    QImageReader reader(path);
    QImage image = reader.read();

    if (image.isNull()) {
        return; // Failed to load, all pixmaps remain null
    }

    // Create 3 scaled versions
    // Note: Qt::SmoothTransformation handles aspect ratio and quality
    icon16 = QPixmap::fromImage(image.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon24 = QPixmap::fromImage(image.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon32 = QPixmap::fromImage(image.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QIcon IconSet::toQIcon() const {
    QIcon icon;

    if (!icon16.isNull()) {
        icon.addPixmap(icon16);
    }
    if (!icon24.isNull()) {
        icon.addPixmap(icon24);
    }
    if (!icon32.isNull()) {
        icon.addPixmap(icon32);
    }

    return icon;
}

// ============================================================================
// KeyboardShortcut Implementation
// ============================================================================

QString KeyboardShortcut::toString() const {
    if (isEmpty()) {
        return QString();
    }

    QString result;

    // Add modifiers
    if (modifiers & Qt::ControlModifier) {
        result += "Ctrl+";
    }
    if (modifiers & Qt::AltModifier) {
        result += "Alt+";
    }
    if (modifiers & Qt::ShiftModifier) {
        result += "Shift+";
    }
    if (modifiers & Qt::MetaModifier) {
        result += "Meta+";
    }

    // Add key
    // Special keys (F1-F12, Qt::Key_* constants)
    if (keyCode >= Qt::Key_F1 && keyCode <= Qt::Key_F12) {
        result += QString("F%1").arg(keyCode - Qt::Key_F1 + 1);
    }
    else if (keyCode == Qt::Key_Return || keyCode == Qt::Key_Enter) {
        result += "Enter";
    }
    else if (keyCode == Qt::Key_Escape) {
        result += "Esc";
    }
    else if (keyCode == Qt::Key_Tab) {
        result += "Tab";
    }
    else if (keyCode == Qt::Key_Backspace) {
        result += "Backspace";
    }
    else if (keyCode == Qt::Key_Delete) {
        result += "Delete";
    }
    else if (keyCode == Qt::Key_Insert) {
        result += "Insert";
    }
    else if (keyCode == Qt::Key_Home) {
        result += "Home";
    }
    else if (keyCode == Qt::Key_End) {
        result += "End";
    }
    else if (keyCode == Qt::Key_PageUp) {
        result += "PageUp";
    }
    else if (keyCode == Qt::Key_PageDown) {
        result += "PageDown";
    }
    else if (keyCode == Qt::Key_Up) {
        result += "Up";
    }
    else if (keyCode == Qt::Key_Down) {
        result += "Down";
    }
    else if (keyCode == Qt::Key_Left) {
        result += "Left";
    }
    else if (keyCode == Qt::Key_Right) {
        result += "Right";
    }
    else if (keyCode >= 32 && keyCode < 127) {
        // Printable ASCII (convert to uppercase for display)
        result += QString(QChar(std::toupper(keyCode)));
    }
    else {
        // Unknown key code - show as number
        result += QString("Key%1").arg(keyCode);
    }

    return result;
}

QKeySequence KeyboardShortcut::toQKeySequence() const {
    if (isEmpty()) {
        return QKeySequence();
    }

    // Combine key code with modifiers
    int combined = keyCode | modifiers;
    return QKeySequence(combined);
}

KeyboardShortcut KeyboardShortcut::fromQKeySequence(const QKeySequence& seq) {
    if (seq.isEmpty()) {
        return KeyboardShortcut();
    }

    // Extract first key combination
    int combined = seq[0];

    KeyboardShortcut result;
    result.keyCode = combined & ~Qt::KeyboardModifierMask;
    result.modifiers = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);

    return result;
}

KeyboardShortcut KeyboardShortcut::fromString(const QString& str) {
    if (str.isEmpty()) {
        return KeyboardShortcut();
    }

    // Use QKeySequence parser (handles platform differences automatically)
    QKeySequence seq = QKeySequence::fromString(str, QKeySequence::PortableText);

    if (seq.isEmpty()) {
        // Fallback to manual parsing
        return fromQKeySequence(QKeySequence(str));
    }

    return fromQKeySequence(seq);
}

// ============================================================================
// Command Implementation
// ============================================================================

QAction* Command::toQAction(QObject* parent) const {
    QAction* action = new QAction(QString::fromStdString(label), parent);

    // Set tooltip
    if (!tooltip.empty()) {
        action->setToolTip(QString::fromStdString(tooltip));
        action->setStatusTip(QString::fromStdString(tooltip));
    }

    // Set icon
    if (!icons.isEmpty()) {
        action->setIcon(icons.toQIcon());
    }

    // Set keyboard shortcut
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut.toQKeySequence());
    }

    // Set enabled state
    if (isEnabled) {
        action->setEnabled(isEnabled());
    }

    // Set checkable state
    if (isChecked) {
        action->setCheckable(true);
        action->setChecked(isChecked());
    }

    // Note: execute callback NOT connected here
    // Connect externally: QObject::connect(action, &QAction::triggered, ...)

    return action;
}

} // namespace gui
} // namespace kalahari
