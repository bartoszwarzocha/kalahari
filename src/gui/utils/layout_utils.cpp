/// @file layout_utils.cpp
/// @brief Implementation of layout utility functions

#include "kalahari/gui/utils/layout_utils.h"

#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

namespace kalahari {
namespace gui {
namespace utils {

void clearLayout(QLayout* layout) {
    if (!layout) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        // If item has a widget, schedule it for deletion
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        // If item has a nested layout, recursively clear it
        else if (QLayout* childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        // Delete the item itself (handles spacers too)
        delete item;
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
