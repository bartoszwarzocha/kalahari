/// @file kalahari_style.cpp
/// @brief Implementation of KalahariStyle for dynamic icon sizing, grey menu items under the
/// keys, tooltips and check boxes
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

namespace {

/// How much of the highlight color a grey menu item under the keys gets, over the menu
constexpr float GREY_ITEM_HIGHLIGHT = 0.3F;

/// @brief @p color mixed into @p base by @p amount (0: @p base, 1: @p color)
QColor mixed(const QColor& base, const QColor& color, float amount)
{
    const auto mix = [amount](float from, float to) { return from + (to - from) * amount; };
    return QColor::fromRgbF(mix(base.redF(), color.redF()), mix(base.greenF(), color.greenF()),
                            mix(base.blueF(), color.blueF()));
}

} // namespace

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

int KalahariStyle::styleHint(StyleHint hint,
                             const QStyleOption* option,
                             const QWidget* widget,
                             QStyleHintReturn* returnData) const {
    // The mouse and the arrow keys rest also on grey menu items, as on Windows, so the status
    // bar says that such a command is in preparation (Fusion skips them)
    if (hint == SH_Menu_AllowActiveAndDisabled) {
        return 1;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
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

void KalahariStyle::drawControl(ControlElement element,
                                const QStyleOption* option,
                                QPainter* painter,
                                const QWidget* widget) const {
    const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
    const bool greyUnderKeys = element == CE_MenuItem && item != nullptr &&
                               item->menuItemType != QStyleOptionMenuItem::Separator &&
                               item->state.testFlag(State_Selected) &&
                               !item->state.testFlag(State_Enabled);
    if (greyUnderKeys) {
        // Under the highlight the item as Fusion draws it, with its grey text: Fusion skips
        // only the highlight of a grey item. The menu's color is Fusion's (PE_PanelMenu).
        const QColor highlight = item->palette.color(QPalette::Active, QPalette::Highlight);
        const QColor menu = item->palette.color(QPalette::Active, QPalette::Base).lighter(108);
        painter->save();
        painter->fillRect(item->rect, mixed(menu, highlight, GREY_ITEM_HIGHLIGHT));
        painter->setPen(highlight);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(item->rect).adjusted(0.5, 0.5, -0.5, -0.5));
        painter->restore();
    }
    QProxyStyle::drawControl(element, option, painter, widget);
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
