/// @file kalahari_style.h
/// @brief Custom QProxyStyle for dynamic icon sizing
///
/// OpenSpec #00026: KalahariStyle reads icon sizes from ArtProvider and
/// forces style refresh when sizes change via resourcesChanged() signal.

#pragma once

#include <QProxyStyle>

namespace kalahari {
namespace gui {

/// @brief Custom style that reads icon sizes from ArtProvider
///
/// Overrides Qt pixel metrics (PM_SmallIconSize, PM_ToolBarIconSize, etc.)
/// to use sizes from ArtProvider. Automatically refreshes all widgets
/// when ArtProvider::resourcesChanged() is emitted.
class KalahariStyle : public QProxyStyle {
    Q_OBJECT

public:
    /// @brief Constructor - wraps Fusion style and connects to ArtProvider
    explicit KalahariStyle();

    /// @brief Override pixel metrics to return dynamic icon sizes from ArtProvider
    /// @param metric The pixel metric being queried
    /// @param option Style option (may be nullptr)
    /// @param widget Widget context (may be nullptr)
    /// @return Pixel value for the requested metric
    int pixelMetric(PixelMetric metric,
                    const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override;

private slots:
    /// @brief Slot called when ArtProvider resources change
    ///
    /// Forces all widgets to re-query style metrics by unpolish/polish cycle.
    /// This triggers repaint with new icon sizes for menus, buttons, etc.
    void onResourcesChanged();
};

} // namespace gui
} // namespace kalahari
