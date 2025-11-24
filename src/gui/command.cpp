/// @file command.cpp
/// @brief Command Registry data structures implementation (Qt6)

#include "kalahari/gui/command.h"
#include "kalahari/core/icon_registry.h"
#include <QFile>
#include <QImageReader>
#include <QApplication>
#include <QPainter>
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

IconSet IconSet::fromStandardIcon(QStyle::StandardPixmap icon) {
    IconSet iconSet;
    QStyle* style = QApplication::style();

    if (!style) {
        return iconSet;  // Empty IconSet if no style
    }

    QIcon qIcon = style->standardIcon(icon);

    // Extract pixmaps in 3 sizes
    iconSet.icon16 = qIcon.pixmap(16, 16);
    iconSet.icon24 = qIcon.pixmap(24, 24);
    iconSet.icon32 = qIcon.pixmap(32, 32);

    return iconSet;
}

IconSet IconSet::createPlaceholder(const QString& letter, const QColor& color) {
    IconSet iconSet;

    // Helper lambda to create pixmap of given size
    auto createPixmap = [&](int size) {
        QPixmap pixmap(size, size);
        pixmap.fill(color);

        QPainter painter(&pixmap);
        painter.setPen(Qt::white);

        // Font configuration
        QFont font = painter.font();
        font.setPixelSize(static_cast<int>(size * 0.6));  // 60% of pixmap height
        font.setBold(true);
        painter.setFont(font);

        // Draw letter centered
        painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);

        return pixmap;
    };

    iconSet.icon16 = createPixmap(16);
    iconSet.icon24 = createPixmap(24);
    iconSet.icon32 = createPixmap(32);

    return iconSet;
}

IconSet IconSet::fromRegistry(const QString& actionId, const QString& theme) {
    IconSet iconSet;

    // Get IconRegistry instance
    auto& registry = kalahari::core::IconRegistry::getInstance();

    // Get icon sizes from registry
    auto sizes = registry.getSizes();

    // Load icons in 3 sizes with current theme colors
    QIcon icon16 = registry.getIcon(actionId, theme, sizes.menu);     // 16px (menu)
    QIcon icon24 = registry.getIcon(actionId, theme, sizes.toolbar);  // 24px (toolbar)
    QIcon icon32 = registry.getIcon(actionId, theme, sizes.dialog);   // 32px (large toolbar/dialog)

    // Convert QIcon → QPixmap for IconSet
    if (!icon16.isNull()) {
        iconSet.icon16 = icon16.pixmap(sizes.menu, sizes.menu);
    }
    if (!icon24.isNull()) {
        iconSet.icon24 = icon24.pixmap(sizes.toolbar, sizes.toolbar);
    }
    if (!icon32.isNull()) {
        iconSet.icon32 = icon32.pixmap(sizes.dialog, sizes.dialog);
    }

    return iconSet;
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

    // Extract first key combination (use toCombined() instead of deprecated operator int)
    int combined = seq[0].toCombined();

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
