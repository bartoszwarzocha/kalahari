/// @file kalahari_style.cpp
/// @brief Implementation of KalahariStyle for dynamic icon sizing
///
/// OpenSpec #00026: KalahariStyle reads icon sizes from ArtProvider (central visual resource manager)
/// and forces style refresh when sizes change via resourcesChanged() signal.

#include "kalahari/gui/kalahari_style.h"
#include "kalahari/core/art_provider.h"
#include <algorithm>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
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

void KalahariStyle::drawPrimitive(PrimitiveElement element,
                                  const QStyleOption* option,
                                  QPainter* painter,
                                  const QWidget* widget) const {
    if (element == PE_PanelTipLabel && option != nullptr) {
        // Tooltip: theme background with a thin frame (the palette's mid color)
        painter->save();
        painter->fillRect(option->rect, option->palette.color(QPalette::ToolTipBase));
        painter->setPen(option->palette.color(QPalette::Mid));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
        painter->restore();
        return;
    }
    if (element != PE_IndicatorCheckBox || option == nullptr) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }

    const QPalette::ColorGroup group =
        (option->state & State_Enabled) ? QPalette::Active : QPalette::Disabled;
    const QPalette& palette = option->palette;
    const bool checked = (option->state & State_On) != 0;
    const bool partial = (option->state & State_NoChange) != 0;
    const bool hovered = (option->state & State_MouseOver) && (option->state & State_Enabled);

    const QRectF box = QRectF(option->rect).adjusted(1.5, 1.5, -1.5, -1.5);
    const QColor highlight = palette.color(group, QPalette::Highlight);
    QColor frame = palette.color(group, QPalette::Text);
    frame.setAlphaF(0.55F);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (checked || partial) {
        painter->setPen(QPen(highlight, 1.0));
        painter->setBrush(highlight);
    } else {
        painter->setPen(QPen(hovered ? highlight : frame, 1.0));
        painter->setBrush(palette.color(group, QPalette::Base));
    }
    painter->drawRoundedRect(box, 2.0, 2.0);

    const QColor markColor = palette.color(group, QPalette::HighlightedText);
    const qreal penWidth = std::max<qreal>(1.5, box.width() / 7.0);
    painter->setPen(QPen(markColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    if (checked) {
        QPainterPath mark;
        mark.moveTo(box.left() + box.width() * 0.22, box.top() + box.height() * 0.52);
        mark.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.72);
        mark.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.30);
        painter->drawPath(mark);
    } else if (partial) {
        const qreal y = box.center().y();
        painter->drawLine(QPointF(box.left() + box.width() * 0.25, y),
                          QPointF(box.right() - box.width() * 0.25, y));
    }

    painter->restore();
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
