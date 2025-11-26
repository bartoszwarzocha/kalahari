/// @file kalahari_style.h
/// @brief Custom QProxyStyle for dynamic icon sizing in menus
/// @details Overrides PM_SmallIconSize and PM_ToolBarIconSize to use IconRegistry settings

#pragma once

#include <QProxyStyle>

namespace kalahari {
namespace gui {

/// Custom style that reads icon sizes from IconRegistry
/// This allows menu icons to respect user-configured sizes
class KalahariStyle : public QProxyStyle {
    Q_OBJECT

public:
    /// Constructor - wraps Fusion style
    explicit KalahariStyle();

    /// Override pixel metrics to return dynamic icon sizes
    /// @param metric The pixel metric being queried
    /// @param option Style option (may be nullptr)
    /// @param widget Widget context (may be nullptr)
    /// @return Pixel value for the requested metric
    int pixelMetric(PixelMetric metric,
                    const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override;
};

} // namespace gui
} // namespace kalahari
