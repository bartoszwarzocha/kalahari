/// @file kalahari_style.cpp
/// @brief Implementation of KalahariStyle for dynamic icon sizing
///
/// OpenSpec #00026: KalahariStyle reads icon sizes from ArtProvider (central visual resource manager)
/// and forces style refresh when sizes change via resourcesChanged() signal.

#include "kalahari/gui/kalahari_style.h"
#include "kalahari/core/art_provider.h"
#include <QApplication>
#include <QWidget>

namespace kalahari {
namespace gui {

KalahariStyle::KalahariStyle()
    : QProxyStyle("Fusion")
{
    // Connect to ArtProvider::resourcesChanged() to force style refresh
    QObject::connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
                     this, &KalahariStyle::onResourcesChanged);
}

int KalahariStyle::pixelMetric(PixelMetric metric,
                                const QStyleOption* option,
                                const QWidget* widget) const {
    // Get icon sizes from ArtProvider (central source of truth)
    auto& artProvider = core::ArtProvider::getInstance();

    switch (metric) {
        case PM_SmallIconSize:
            // Used by QMenu for menu item icons
            return artProvider.getIconSize(core::IconContext::Menu);

        case PM_ToolBarIconSize:
            // Used by QToolBar
            return artProvider.getIconSize(core::IconContext::Toolbar);

        case PM_ListViewIconSize:
        case PM_IconViewIconSize:
            // Used by tree views and list views
            return artProvider.getIconSize(core::IconContext::TreeView);

        case PM_TabBarIconSize:
            // Used by tab bars
            return artProvider.getIconSize(core::IconContext::TabBar);

        case PM_ButtonIconSize:
            // Used by push buttons
            return artProvider.getIconSize(core::IconContext::Button);

        default:
            // Fall back to base Fusion style
            return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

QIcon KalahariStyle::standardIcon(StandardPixmap standardIcon,
                                   const QStyleOption* option,
                                   const QWidget* widget) const {
    auto& artProvider = core::ArtProvider::getInstance();

    // Override toolbar extension button icons to use theme-aware chevrons
    // These are the ">>" buttons shown when toolbar overflows
    switch (standardIcon) {
        case SP_ToolBarHorizontalExtensionButton:
            return artProvider.getIcon("common.chevronRight", core::IconContext::Toolbar);

        case SP_ToolBarVerticalExtensionButton:
            return artProvider.getIcon("navigation.down", core::IconContext::Toolbar);

        default:
            return QProxyStyle::standardIcon(standardIcon, option, widget);
    }
}

void KalahariStyle::onResourcesChanged() {
    // Force all widgets to re-query style metrics
    // This triggers repaint with new icon sizes
    if (qApp) {
        for (QWidget* widget : qApp->allWidgets()) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        }
    }
}

} // namespace gui
} // namespace kalahari
