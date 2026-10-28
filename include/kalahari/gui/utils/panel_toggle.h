/// @file panel_toggle.h
/// @brief Show/hide toggles of the dock panels (View > Panels and the View toolbar)

#ifndef KALAHARI_GUI_UTILS_PANEL_TOGGLE_H
#define KALAHARI_GUI_UTILS_PANEL_TOGGLE_H

class QAction;
class QDockWidget;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief True while a panel is open
///
/// A panel is open until it is closed, also while its tab lies under another panel's tab
/// (QDockWidget::visibilityChanged() reports it as not visible then) and before the main
/// window is shown.
bool isPanelOpen(const QDockWidget* dock);

/// @brief True while a panel can be seen: open, and floating or with its tab on top of its
/// group
bool isPanelOnTop(const QDockWidget* dock);

/// @brief The click of a panel toggle: a closed panel opens with its tab on top, an open one
/// under another tab comes on top, and a panel on top closes
void togglePanel(QDockWidget* dock);

/// @brief Keep a panel toggle checked while its panel is open
///
/// Makes the action checkable and updates it whenever the panel is shown or hidden, or
/// its tab is raised or covered.
void followPanel(QAction* action, QDockWidget* dock);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_PANEL_TOGGLE_H
