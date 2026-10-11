/// @file layout_utils.h
/// @brief Layout utility functions for Qt widgets
///
/// Provides common layout manipulation utilities that handle edge cases
/// correctly, such as recursive clearing of nested layouts.

#ifndef KALAHARI_GUI_UTILS_LAYOUT_UTILS_H
#define KALAHARI_GUI_UTILS_LAYOUT_UTILS_H

class QLayout;
class QSize;
class QWidget;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief Recursively clear a layout and delete all items
///
/// Properly handles:
/// - Widgets (deleted via deleteLater())
/// - Nested layouts (recursively cleared)
/// - Spacer items
///
/// This function is safer than the common pattern of just checking
/// item->widget(), which misses nested layouts and causes memory leaks.
///
/// @param layout The layout to clear. If nullptr, does nothing.
///
/// Example usage:
/// @code
/// #include "kalahari/gui/utils/layout_utils.h"
///
/// void MyWidget::refreshContent() {
///     kalahari::gui::utils::clearLayout(m_contentLayout);
///     // Now add new widgets to the cleared layout
///     m_contentLayout->addWidget(new QLabel(tr("New content")));
/// }
/// @endcode
void clearLayout(QLayout* layout);

/// @brief Give a window its preferred size, or less where the screen is smaller
///
/// A window larger than the screen (e.g. 1366x768 at 150%) would hide its buttons;
/// it keeps a margin for the title bar and the taskbar instead.
///
/// @param window The window to resize
/// @param preferred The size the window has on a screen big enough for it
void resizeWithinScreen(QWidget* window, const QSize& preferred);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_LAYOUT_UTILS_H
