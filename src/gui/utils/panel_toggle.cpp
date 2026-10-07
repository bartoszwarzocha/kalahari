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

void togglePanel(QDockWidget* dock) {
    if (dock == nullptr) {
        return;
    }
    if (isPanelOpen(dock)) {
        dock->hide();
        return;
    }
    dock->show();
    // A panel shown again in a tab group would lie under the current tab
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
