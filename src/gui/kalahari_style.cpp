/// @file kalahari_style.cpp
/// @brief Implementation of KalahariStyle for dynamic icon sizing

#include "kalahari/gui/kalahari_style.h"
#include "kalahari/core/icon_registry.h"

namespace kalahari {
namespace gui {

KalahariStyle::KalahariStyle()
    : QProxyStyle("Fusion") {
}

int KalahariStyle::pixelMetric(PixelMetric metric,
                                const QStyleOption* option,
                                const QWidget* widget) const {
    // Get icon sizes from IconRegistry (single source of truth)
    const auto& sizes = core::IconRegistry::getInstance().getSizes();

    switch (metric) {
        case PM_SmallIconSize:
            // Used by QMenu for menu item icons
            return sizes.menu;

        case PM_ToolBarIconSize:
            // Used by QToolBar (though we also set explicitly via setIconSize)
            return sizes.toolbar;

        case PM_ListViewIconSize:
        case PM_IconViewIconSize:
            // Used by tree views and list views
            return sizes.treeView;

        case PM_TabBarIconSize:
            // Used by tab bars
            return sizes.tabBar;

        case PM_ButtonIconSize:
            // Used by push buttons
            return sizes.button;

        default:
            // Fall back to base Fusion style
            return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

} // namespace gui
} // namespace kalahari
