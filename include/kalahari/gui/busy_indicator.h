/// @file busy_indicator.h
/// @brief Reusable busy indicator (spinner overlay) widget
///
/// BusyIndicator provides a modal overlay with animated spinner
/// for operations taking 1-5 seconds. Uses theme primary color.
///
/// Usage:
/// @code
/// // Simple usage with lambda
/// BusyIndicator::run(this, "Applying theme...", [&]() {
///     // Long operation here
///     saveSettings();
/// });
///
/// // Manual control
/// BusyIndicator indicator(this);
/// indicator.show("Loading...");
/// doWork();
/// indicator.hide();
/// @endcode

#pragma once

#include <QWidget>
#include <QTimer>
#include <functional>

class QLabel;

namespace kalahari {
namespace gui {

/// @brief Modal overlay with animated spinner
///
/// Features:
/// - Animated spinning arc using theme primary color
/// - Optional status message
/// - Blocks parent widget interaction
/// - Auto-centers on parent
/// - Smooth fade in/out (optional)
class BusyIndicator : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget to overlay
    explicit BusyIndicator(QWidget* parent = nullptr);

    /// @brief Destructor
    ~BusyIndicator() override;

    /// @brief Show indicator with optional message
    /// @param message Status message (empty = no message)
    void show(const QString& message = QString());

    /// @brief Hide indicator
    void hide();

    /// @brief Check if indicator is currently visible
    bool isActive() const { return m_active; }

    /// @brief Set spinner color (default: theme primary)
    /// @param color Spinner color
    void setColor(const QColor& color);

    /// @brief Set spinner size
    /// @param size Diameter in pixels (default: 48)
    void setSpinnerSize(int size);

    /// @brief Static helper - run operation with busy indicator
    /// @param parent Parent widget
    /// @param message Status message
    /// @param operation Operation to execute
    /// @return true if operation completed
    static bool run(QWidget* parent, const QString& message,
                    const std::function<void()>& operation);

    /// @brief Animate current indicator (call from within operation)
    ///
    /// Call this between long-running steps to keep animation alive.
    /// Safe to call even if no indicator is active.
    ///
    /// @code
    /// BusyIndicator::run(this, "Working...", []() {
    ///     step1();
    ///     BusyIndicator::tick();  // animate
    ///     step2();
    ///     BusyIndicator::tick();  // animate
    ///     step3();
    /// });
    /// @endcode
    ///
    /// @note Future: Will be extended to setProgress(float) for progress bar
    static void tick();

protected:
    /// @brief Paint event - draws overlay and spinner
    void paintEvent(QPaintEvent* event) override;

    /// @brief Resize to match parent
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /// @brief Animation tick
    void onAnimationTick();

private:
    /// @brief Update geometry to match parent
    void updateGeometry();

    bool m_active;              ///< Is indicator visible
    QString m_message;          ///< Status message
    QColor m_color;             ///< Spinner color
    int m_spinnerSize;          ///< Spinner diameter
    int m_angle;                ///< Current rotation angle
    int m_pulsePhase;           ///< Pulse animation phase (0-359)
    QTimer* m_animationTimer;   ///< Animation timer
    QLabel* m_messageLabel;     ///< Message label (optional)

    /// @brief Currently active indicator (for static tick() method)
    static BusyIndicator* s_activeIndicator;
};

} // namespace gui
} // namespace kalahari
