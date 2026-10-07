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
        // If item has a widget, hide it and schedule it for deletion. Hidden at once:
        // inside a modal dialog the deletion waits until the dialog closes, and a
        // visible widget taken out of its layout stays drawn in the top-left corner
        if (QWidget* widget = item->widget()) {
            widget->hide();
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
