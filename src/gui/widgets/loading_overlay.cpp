// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file loading_overlay.cpp
/// @brief Over a widget whose work is under way: the widget darkened and a spinner turning

#include "kalahari/gui/widgets/loading_overlay.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// How much of the theme's shadow darkens the widget
constexpr double DIM_SHARE = 0.45;
/// The spinner at most this big, and at most this share of the lower side of the widget
constexpr int SPINNER_SIZE = 40;
constexpr double SPINNER_SHARE = 0.6;
/// The line of the spinner, a share of its size
constexpr double LINE_SHARE = 0.12;
/// The arc of the spinner, in degrees, and a full turn
constexpr int ARC_DEGREES = 270;
constexpr int FULL_TURN = 360;
/// QPainter measures the angles in sixteenths of a degree
constexpr int SIXTEENTHS = 16;

/// The events the program sends to its widgets at once, also while it is busy (the widgets
/// of a book being made): each may turn the spinner. Not the painting, nor the timers and
/// the input, which wait for the program.
bool sentWhileBusy(QEvent::Type type) {
    switch (type) {
    case QEvent::ChildAdded:
    case QEvent::ChildPolished:
    case QEvent::ChildRemoved:
    case QEvent::Polish:
    case QEvent::Create:
    case QEvent::DynamicPropertyChange:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ParentChange:
        return true;
    default:
        return false;
    }
}

/// Whether @p watched is a window that asks a question: a modal dialog
bool isQuestion(const QObject* watched) {
    const auto* widget = qobject_cast<const QWidget*>(watched);
    return widget != nullptr && widget->isWindow() && widget->isModal();
}

}  // namespace

LoadingOverlay::LoadingOverlay(QWidget* covered) : QWidget(covered) {
    setObjectName(QStringLiteral("loadingOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(covered->rect());
    covered->installEventFilter(this);
    qApp->installEventFilter(this);
    m_timer.setInterval(FRAME_MS);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (turn()) {
            update();
        }
    });
    m_clock.start();
    m_timer.start();
    raise();
    show();
    // On the screen before the work keeps the program busy
    repaint();
}

LoadingOverlay::~LoadingOverlay() {
    qApp->removeEventFilter(this);
}

bool LoadingOverlay::turn() {
    if (m_clock.elapsed() < FRAME_MS) {
        return false;
    }
    m_clock.restart();
    m_angle = (m_angle + STEP_DEGREES) % FULL_TURN;
    return true;
}

bool LoadingOverlay::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type type = event->type();
    if (watched == parentWidget() && type == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        return false;
    }
    // A question hides it until it is answered: nothing loads meanwhile
    const bool shownOrHidden = type == QEvent::Show || type == QEvent::Hide;
    if (shownOrHidden && isQuestion(watched) && watched != window()) {
        m_questions = std::max(0, m_questions + (type == QEvent::Show ? 1 : -1));
        setVisible(m_questions == 0);
        return false;
    }
    // The program is busy: the timer waits, the events it sends turn the spinner
    if (!m_painting && isVisible() && sentWhileBusy(type) && turn()) {
        m_painting = true;
        repaint();
        m_painting = false;
    }
    return false;
}

void LoadingOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // The widget darkened by the theme's shadow
    QColor shadow = palette().color(QPalette::Shadow);
    shadow.setAlphaF(static_cast<float>(DIM_SHARE));
    painter.fillRect(rect(), shadow);

    // The spinner in the middle, in the theme's bright text
    const int size = std::min(SPINNER_SIZE, static_cast<int>(std::min(width(), height()) *
                                                             SPINNER_SHARE));
    if (size <= 0) {
        return;
    }
    const double line = std::max(1.0, size * LINE_SHARE);
    QPen pen(palette().color(QPalette::BrightText));
    pen.setWidthF(line);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    QRectF circle(0, 0, size - line, size - line);
    circle.moveCenter(QRectF(rect()).center());
    painter.drawArc(circle, -m_angle * SIXTEENTHS, ARC_DEGREES * SIXTEENTHS);
}

}  // namespace kalahari::gui
