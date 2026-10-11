/// @file tab_focus.h
/// @brief The keys go to the page in front of the tabs, also when a tab closes

#ifndef KALAHARI_GUI_UTILS_TAB_FOCUS_H
#define KALAHARI_GUI_UTILS_TAB_FOCUS_H

class QTabWidget;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief Give the keyboard focus to the page in front of @p tabs, as a switch of tabs gives it
///
/// Its own field that had the focus last, else its first field that takes the focus from Tab,
/// else the page itself; the tabs themselves when they have no page. In a window behind another
/// one of the program, the field gets the focus when its window comes back.
/// @param tabs The tabs
void focusPageInFront(QTabWidget& tabs);

/// @brief Remove the tab at @p index from @p tabs, keeping the keyboard focus in the tabs
///
/// When the focus was on the page removed, Qt passes it on to the next field of the window,
/// such as the font box of a toolbar, and the keys typed next land there. Here the page now
/// in front gets it, as focusPageInFront() gives it, as in Word and LibreOffice the document
/// in front gets the keys. So it does when the tab removed was the one in front, wherever the
/// focus was, as in the Navigator: the document coming to the front gets the keys. A focus
/// elsewhere while a tab behind is closed stays there.
/// The same holds while a window of the program is in front, such as the question about
/// unsaved changes: the page in front gets the keys when the window comes back.
/// The page removed is not deleted.
/// @param tabs The tabs
/// @param index The index of the tab to remove; out of range does nothing
void removeTabKeepingFocus(QTabWidget& tabs, int index);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_TAB_FOCUS_H
