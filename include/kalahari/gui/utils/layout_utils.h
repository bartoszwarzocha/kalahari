/// @file layout_utils.h
/// @brief Layout utility functions for Qt widgets
///
/// Provides common layout manipulation utilities that handle edge cases
/// correctly, such as recursive clearing of nested layouts.

#ifndef KALAHARI_GUI_UTILS_LAYOUT_UTILS_H
#define KALAHARI_GUI_UTILS_LAYOUT_UTILS_H

class QLayout;

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

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_LAYOUT_UTILS_H
