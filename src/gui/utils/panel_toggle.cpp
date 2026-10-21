/// @file panel_toggle.cpp
/// @brief Implementation of the dock panel toggles

#include "kalahari/gui/utils/panel_toggle.h"

#include <QAction>
#include <QDockWidget>

namespace kalahari {
namespace gui {
namespace utils {

bool isPanelOpen(const QDockWidget* dock) {
    return dock != nullptr && !dock->isHidden();
}

bool isPanelOnTop(const QDockWidget* dock) {
    if (!isPanelOpen(dock) || !dock->isVisible()) {
        return false;
    }
    // A floating panel is a window of its own; a docked one under another tab of its group
    // has nothing of it to show
    return dock->isFloating() || !dock->visibleRegion().isEmpty();
}

void togglePanel(QDockWidget* dock) {
    if (dock == nullptr) {
        return;
    }
    if (isPanelOnTop(dock)) {
        dock->hide();
        return;
    }
    // A closed panel shown again, or an open one under another tab, comes on top of its group
    dock->show();
    dock->raise();
}

void followPanel(QAction* action, QDockWidget* dock) {
    if (action == nullptr || dock == nullptr) {
        return;
    }
    action->setCheckable(true);
    action->setChecked(isPanelOpen(dock));
    // visibilityChanged also comes when the panel's tab is raised or covered, so the
    // panel itself is asked whether it is open
    QObject::connect(dock, &QDockWidget::visibilityChanged, action, [action, dock]() {
        action->setChecked(isPanelOpen(dock));
    });
}

} // namespace utils
} // namespace gui
} // namespace kalahari
