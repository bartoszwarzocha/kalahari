/// @file qt_standard_texts.cpp
/// @brief Qt's own texts that Kalahari translates
///
/// Qt translates its standard buttons and text field menus through
/// QCoreApplication::translate() with its own contexts. Kalahari ships no
/// qtbase translations, so these entries put the texts into kalahari_*.ts
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
};

}  // namespace
