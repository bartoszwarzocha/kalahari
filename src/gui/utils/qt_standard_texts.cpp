/// @file qt_standard_texts.cpp
/// @brief Qt's own texts that Kalahari translates
///
/// Qt translates its standard buttons, the menus of text fields, number fields and
/// scroll bars, its panel and tab buttons and its own file window through
/// QCoreApplication::translate() with its own contexts. Kalahari ships no
/// qtbase translations, so these entries put the texts into kalahari_*.ts
/// (lupdate reads the QT_TRANSLATE_NOOP markers; the array is never used).
/// The names of keys (context QShortcut) are not here.

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

    // Command line help (QCommandLineParser::helpText)
    QT_TRANSLATE_NOOP("QCommandLineParser", "Usage: %1"),
    QT_TRANSLATE_NOOP("QCommandLineParser", "[options]"),
    QT_TRANSLATE_NOOP("QCommandLineParser", "Options:"),
    QT_TRANSLATE_NOOP("QCommandLineParser", "Unknown option '%1'."),
    QT_TRANSLATE_NOOP("QCommandLineParser", "Unknown options: %1."),
    QT_TRANSLATE_NOOP("QCommandLineParser", "Missing value after '%1'."),
    QT_TRANSLATE_NOOP("QCommandLineParser", "Unexpected value after '%1'."),

    // Context menu of text fields (QLineEdit)
    QT_TRANSLATE_NOOP("QLineEdit", "&Undo"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Redo"),
    QT_TRANSLATE_NOOP("QLineEdit", "Cu&t"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Copy"),
    QT_TRANSLATE_NOOP("QLineEdit", "&Paste"),
    QT_TRANSLATE_NOOP("QLineEdit", "Delete"),
    QT_TRANSLATE_NOOP("QLineEdit", "Select All"),

    // Context menu of multi-line text fields (QTextEdit, QPlainTextEdit)
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Undo"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Redo"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Cu&t"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Copy"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Copy &Link Location"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "&Paste"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Delete"),
    QT_TRANSLATE_NOOP("QWidgetTextControl", "Select All"),

    // Context menu of number fields (QSpinBox, QDoubleSpinBox); the rest comes from QLineEdit
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Select All"),
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Step up"),
    QT_TRANSLATE_NOOP("QAbstractSpinBox", "Step &down"),

    // Context menu of scroll bars
    QT_TRANSLATE_NOOP("QScrollBar", "Scroll here"),
    QT_TRANSLATE_NOOP("QScrollBar", "Left edge"),
    QT_TRANSLATE_NOOP("QScrollBar", "Top"),
    QT_TRANSLATE_NOOP("QScrollBar", "Right edge"),
    QT_TRANSLATE_NOOP("QScrollBar", "Bottom"),
    QT_TRANSLATE_NOOP("QScrollBar", "Page left"),
    QT_TRANSLATE_NOOP("QScrollBar", "Page up"),
    QT_TRANSLATE_NOOP("QScrollBar", "Page right"),
    QT_TRANSLATE_NOOP("QScrollBar", "Page down"),
    QT_TRANSLATE_NOOP("QScrollBar", "Scroll left"),
    QT_TRANSLATE_NOOP("QScrollBar", "Scroll up"),
    QT_TRANSLATE_NOOP("QScrollBar", "Scroll right"),
    QT_TRANSLATE_NOOP("QScrollBar", "Scroll down"),

    // Tab bars: the scroll buttons and the close button of a tab
    QT_TRANSLATE_NOOP("QTabBar", "Scroll Left"),
    QT_TRANSLATE_NOOP("QTabBar", "Scroll Right"),
    QT_TRANSLATE_NOOP("CloseButton", "Close Tab"),

    // Panels (QDockWidget): the buttons of the title bar
    QT_TRANSLATE_NOOP("QDockWidget", "Close"),
    QT_TRANSLATE_NOOP("QDockWidget", "Float"),
    QT_TRANSLATE_NOOP("QDockWidget", "Undocks and re-attaches the dock widget"),

    // Drop-down lists, for screen readers
    QT_TRANSLATE_NOOP("QComboBox", "Open the combo box selection popup"),

    // Actions named to screen readers
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Press"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Triggers the action"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Increase"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Increase the value"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Decrease"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Decrease the value"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Shows the menu"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "SetFocus"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Sets the focus"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Toggle"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Toggles the state"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scroll Left"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scrolls to the left"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scroll Right"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scrolls to the right"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scrolls up"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Scrolls down"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Goes back a page"),
    QT_TRANSLATE_NOOP("QAccessibleActionInterface", "Goes to the next page"),

    // Qt's own file window, used where the system has none (some Linux desktops)
    QT_TRANSLATE_NOOP("QFileDialog", "Look in:"),
    QT_TRANSLATE_NOOP("QFileDialog", "Back"),
    QT_TRANSLATE_NOOP("QFileDialog", "Go back"),
    QT_TRANSLATE_NOOP("QFileDialog", "Forward"),
    QT_TRANSLATE_NOOP("QFileDialog", "Go forward"),
    QT_TRANSLATE_NOOP("QFileDialog", "Parent Directory"),
    QT_TRANSLATE_NOOP("QFileDialog", "Go to the parent directory"),
    QT_TRANSLATE_NOOP("QFileDialog", "Create New Folder"),
    QT_TRANSLATE_NOOP("QFileDialog", "Create a New Folder"),
    QT_TRANSLATE_NOOP("QFileDialog", "List View"),
    QT_TRANSLATE_NOOP("QFileDialog", "Change to list view mode"),
    QT_TRANSLATE_NOOP("QFileDialog", "Detail View"),
    QT_TRANSLATE_NOOP("QFileDialog", "Change to detail view mode"),
    QT_TRANSLATE_NOOP("QFileDialog", "Sidebar"),
    QT_TRANSLATE_NOOP("QFileDialog", "List of places and bookmarks"),
    QT_TRANSLATE_NOOP("QFileDialog", "Recent Places"),
    QT_TRANSLATE_NOOP("QFileDialog", "Files"),
    QT_TRANSLATE_NOOP("QFileDialog", "File &name:"),
    QT_TRANSLATE_NOOP("QFileDialog", "Directory:"),
    QT_TRANSLATE_NOOP("QFileDialog", "Files of type:"),
    QT_TRANSLATE_NOOP("QFileDialog", "All Files (*)"),
    QT_TRANSLATE_NOOP("QFileDialog", "All files (*)"),
    QT_TRANSLATE_NOOP("QFileDialog", "Directories"),
    QT_TRANSLATE_NOOP("QFileDialog", "Open"),
    QT_TRANSLATE_NOOP("QFileDialog", "&Open"),
    QT_TRANSLATE_NOOP("QFileDialog", "&Save"),
    QT_TRANSLATE_NOOP("QFileDialog", "Save As"),
    QT_TRANSLATE_NOOP("QFileDialog", "&Choose"),
    QT_TRANSLATE_NOOP("QFileDialog", "Find Directory"),
    QT_TRANSLATE_NOOP("QFileDialog", "&Rename"),
    QT_TRANSLATE_NOOP("QFileDialog", "&Delete"),
    QT_TRANSLATE_NOOP("QFileDialog", "Delete"),
    QT_TRANSLATE_NOOP("QFileDialog", "Remove"),
    QT_TRANSLATE_NOOP("QFileDialog", "Show &hidden files"),
    QT_TRANSLATE_NOOP("QFileDialog", "&New Folder"),
    QT_TRANSLATE_NOOP("QFileDialog", "New Folder"),
    QT_TRANSLATE_NOOP("QFileDialog", "Show "),
    QT_TRANSLATE_NOOP("QFileDialog", "File"),
    QT_TRANSLATE_NOOP("QFileDialog", "Folder"),
    QT_TRANSLATE_NOOP("QFileDialog", "Alias"),
    QT_TRANSLATE_NOOP("QFileDialog", "Shortcut"),
    QT_TRANSLATE_NOOP("QFileDialog", "Drive"),
    QT_TRANSLATE_NOOP("QFileDialog", "Unknown"),
    QT_TRANSLATE_NOOP("QFileDialog", "%1 already exists.\nDo you want to replace it?"),
    QT_TRANSLATE_NOOP("QFileDialog", "%1\nFile not found.\nPlease verify the correct file name was given."),
    QT_TRANSLATE_NOOP("QFileDialog", "%1\nDirectory not found.\nPlease verify the correct directory name was given."),
    QT_TRANSLATE_NOOP("QFileDialog", "Are you sure you want to delete '%1'?"),
    QT_TRANSLATE_NOOP("QFileDialog", "'%1' is write protected.\nDo you want to delete it anyway?"),
    QT_TRANSLATE_NOOP("QFileDialog", "Could not delete directory."),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Name"),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Size"),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Type"),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Kind"),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Date Modified"),
    QT_TRANSLATE_NOOP("QFileSystemModel", "Computer"),
};

}  // namespace
