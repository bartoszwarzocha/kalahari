/// @file busy_indicator.cpp
/// @brief Implementation of BusyIndicator widget

#include "kalahari/gui/busy_indicator.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/logger.h"

#include <QPainter>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QResizeEvent>
#include <future>
#include <chrono>
#include <cmath>

namespace kalahari {
namespace gui {

// Static member initialization
BusyIndicator* BusyIndicator::s_activeIndicator = nullptr;

// ============================================================================
// Constructor / Destructor
// ============================================================================

BusyIndicator::BusyIndicator(QWidget* parent)
    : QWidget(parent)
    , m_active(false)
    , m_spinnerSize(48)
    , m_angle(0)
    , m_pulsePhase(0)
    , m_animationTimer(new QTimer(this))
    , m_messageLabel(nullptr)
{
    // Get primary color from theme
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    m_color = theme.colors.primary;

    // Make widget cover entire parent
    if (parent) {
        setGeometry(parent->rect());
    }

    // Semi-transparent overlay
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);

    // Create message label
    m_messageLabel = new QLabel(this);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    // Use brightText from theme for text on dark overlay background
    m_messageLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 14px; font-weight: bold; "
        "background: transparent; }").arg(theme.palette.brightText.name()));
    m_messageLabel->hide();

    // Setup animation timer (60 FPS)
    connect(m_animationTimer, &QTimer::timeout, this, &BusyIndicator::onAnimationTick);

    // Initially hidden
    QWidget::hide();

    core::Logger::getInstance().debug("BusyIndicator: Created");
}

BusyIndicator::~BusyIndicator() {
    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void BusyIndicator::show(const QString& message) {
    m_active = true;
    m_message = message;
    m_angle = 0;
    m_pulsePhase = 0;

    // Set as active indicator for tick()
    s_activeIndicator = this;

    // Update geometry to match parent
    updateGeometry();

    // Position message label below spinner
    if (!message.isEmpty()) {
        m_messageLabel->setText(message);
        m_messageLabel->adjustSize();
        int labelX = (width() - m_messageLabel->width()) / 2;
        int labelY = height() / 2 + m_spinnerSize / 2 + 20;
        m_messageLabel->move(labelX, labelY);
        m_messageLabel->show();
    } else {
        m_messageLabel->hide();
    }

    // Start animation timer (but tick() is primary animation driver now)
    m_animationTimer->start(16);  // ~60 FPS

    // Show and raise to top
    QWidget::show();
    raise();

    // Initial paint
    repaint();
    QApplication::processEvents();

    core::Logger::getInstance().debug("BusyIndicator: Shown with message '{}'",
                                       message.toStdString());
}

void BusyIndicator::hide() {
    m_active = false;
    s_activeIndicator = nullptr;  // Clear active indicator
    m_animationTimer->stop();
    m_messageLabel->hide();
    QWidget::hide();

    core::Logger::getInstance().debug("BusyIndicator: Hidden");
}

void BusyIndicator::tick() {
    if (!s_activeIndicator) {
        return;  // No active indicator, safe no-op
    }

    // Advance animation by several frames for visible progress
    for (int i = 0; i < 5; ++i) {
        s_activeIndicator->onAnimationTick();
        s_activeIndicator->repaint();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
    }
}

void BusyIndicator::setColor(const QColor& color) {
    m_color = color;
    if (m_active) {
        update();
    }
}

void BusyIndicator::setSpinnerSize(int size) {
    m_spinnerSize = qBound(24, size, 128);
    if (m_active) {
        update();
    }
}

bool BusyIndicator::run(QWidget* parent, const QString& message,
                        const std::function<void()>& operation) {
    if (!parent || !operation) {
        return false;
    }

    BusyIndicator indicator(parent);
    indicator.show(message);

    // Execute operation - use tick() inside operation for animation
    // Example: step1(); BusyIndicator::tick(); step2(); BusyIndicator::tick();
    operation();

    // Process any pending events from operation
    QApplication::processEvents();

    indicator.hide();
    return true;
}

// ============================================================================
// Protected Methods
// ============================================================================

void BusyIndicator::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw semi-transparent overlay
    painter.fillRect(rect(), QColor(0, 0, 0, 128));

    // Calculate center position
    int centerX = width() / 2;
    int centerY = height() / 2;

    // Draw 3 pulsating dots with phase offset
    const int dotRadius = 10;
    const int dotSpacing = 30;
    const int dotCount = 3;

    painter.setPen(Qt::NoPen);

    for (int i = 0; i < dotCount; ++i) {
        // Each dot has 120-degree phase offset
        int phase = (m_pulsePhase + i * 120) % 360;

        // Calculate opacity using sine wave (0.3 to 1.0)
        double radians = phase * 3.14159265 / 180.0;
        double opacity = 0.3 + 0.7 * (0.5 + 0.5 * std::sin(radians));

        // Calculate scale using sine wave (0.6 to 1.0)
        double scale = 0.6 + 0.4 * (0.5 + 0.5 * std::sin(radians));

        // Set color with opacity
        QColor dotColor = m_color;
        dotColor.setAlphaF(opacity);
        painter.setBrush(dotColor);

        // Calculate dot position
        int dotX = centerX + (i - 1) * dotSpacing;
        int dotY = centerY;
        int scaledRadius = static_cast<int>(dotRadius * scale);

        // Draw dot
        painter.drawEllipse(QPoint(dotX, dotY), scaledRadius, scaledRadius);
    }
}

void BusyIndicator::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateGeometry();
}

// ============================================================================
// Private Slots
// ============================================================================

void BusyIndicator::onAnimationTick() {
    // Rotate 6 degrees per tick (60 FPS = 360 degrees per second)
    m_angle = (m_angle + 6) % 360;

    // Pulse opacity (0.3 to 1.0)
    m_pulsePhase = (m_pulsePhase + 4) % 360;

    update();
}

// ============================================================================
// Private Methods
// ============================================================================

void BusyIndicator::updateGeometry() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());

        // Reposition message label
        if (m_messageLabel && m_messageLabel->isVisible()) {
            int labelX = (width() - m_messageLabel->width()) / 2;
            int labelY = height() / 2 + m_spinnerSize / 2 + 20;
            m_messageLabel->move(labelX, labelY);
        }
    }
}

} // namespace gui
} // namespace kalahari
