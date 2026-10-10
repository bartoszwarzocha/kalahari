/// @file qt_standard_texts.cpp
/// @brief Qt's own texts that Kalahari translates
///
/// Qt translates its standard buttons, text field menus and the names of the keys in
/// shortcuts through QCoreApplication::translate() with its own contexts. Kalahari
/// ships no qtbase translations, so these entries put the texts into kalahari_*.ts
/// (lupdate reads the QT_TRANSLATE_NOOP markers; the array is never used).

#include <QtGlobal>

namespace {

[[maybe_unused]] const char* const QT_STANDARD_TEXTS[] = {
    // Standard buttons (QDialogButtonBox, QMessageBox)
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Save"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Save All"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Open"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "&Yes"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Yes to &All"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "&No"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "N&o to All"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Abort"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Retry"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Ignore"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Close"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Discard"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Help"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Apply"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Reset"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Restore Defaults"),

    // Context menu of text fields (QLineEdit)
    QT_TRANSLATE_NOOP("QLineEdit", "&Undo"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Redo"),
    QT_TRANSLATE_NOOP("QLineEdit", "Cu&t"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Copy"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Paste"),
    QT_TRANSLATE_NOOP("QLineEdit", "Delete"),
    QT_TRANSLATE_NOOP("QLineEdit", "Select All"),

    // Context menu of number fields (QSpinBox, QDoubleSpinBox), with the items above
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Select All"),
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Step up"),
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "Step &down"),

    // Context menu of texts and of labels whose text can be selected (QTextEdit,
    // QPlainTextEdit, QLabel)
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Undo"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Redo"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Cu&t"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Copy"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Copy &Link Location"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Paste"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Delete"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Select All"),

    // Keys as the menus and Settings > Keyboard Shortcuts write them (QKeySequence::NativeText)
    QT_TRANSLATE_NOOP("QShortcut", "Ctrl"),
    QT_TRANSLATE_NOOP("QShortcut", "Shift"),
    QT_TRANSLATE_NOOP("QShortcut", "Alt"),
    QT_TRANSLATE_NOOP("QShortcut", "Meta"),
    QT_TRANSLATE_NOOP("QShortcut", "F%1"),
    QT_TRANSLATE_NOOP("QShortcut", "Esc"),
    QT_TRANSLATE_NOOP("QShortcut", "Tab"),
    QT_TRANSLATE_NOOP("QShortcut", "Backspace"),
    QT_TRANSLATE_NOOP("QShortcut", "Return"),
    QT_TRANSLATE_NOOP("QShortcut", "Enter"),
    QT_TRANSLATE_NOOP("QShortcut", "Space"),
    QT_TRANSLATE_NOOP("QShortcut", "Ins"),
    QT_TRANSLATE_NOOP("QShortcut", "Del"),
    QT_TRANSLATE_NOOP("QShortcut", "Home"),
    QT_TRANSLATE_NOOP("QShortcut", "End"),
    QT_TRANSLATE_NOOP("QShortcut", "PgUp"),
    QT_TRANSLATE_NOOP("QShortcut", "PgDown"),
    QT_TRANSLATE_NOOP("QShortcut", "Left"),
    QT_TRANSLATE_NOOP("QShortcut", "Up"),
    QT_TRANSLATE_NOOP("QShortcut", "Right"),
    QT_TRANSLATE_NOOP("QShortcut", "Down"),
    QT_TRANSLATE_NOOP("QShortcut", "Menu"),
    QT_TRANSLATE_NOOP("QShortcut", "Print"),
    QT_TRANSLATE_NOOP("QShortcut", "Pause"),
};

}  // namespace
