/// @file tab_focus.h
/// @brief Closing a tab gives the keys to the page in front

#ifndef KALAHARI_GUI_UTILS_TAB_FOCUS_H
#define KALAHARI_GUI_UTILS_TAB_FOCUS_H

class QTabWidget;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief Remove the tab at @p index from @p tabs, keeping the keyboard focus in the tabs
///
/// When the focus was on the page removed, Qt passes it on to the next field of the window,
/// such as the font box of a toolbar, and the keys typed next land there. Here the page now
/// in front gets it, as a switch of tabs gives it: its own field that had the focus last,
/// else its first field that takes the focus from Tab, else the page itself (the tabs
/// themselves when no page is left), as in Word and LibreOffice the document in front gets
/// the keys. A focus elsewhere, as in a panel while a tab behind is closed, stays there.
/// The page removed is not deleted.
/// @param tabs The tabs
/// @param index The index of the tab to remove; out of range does nothing
void removeTabKeepingFocus(QTabWidget& tabs, int index);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_TAB_FOCUS_H
