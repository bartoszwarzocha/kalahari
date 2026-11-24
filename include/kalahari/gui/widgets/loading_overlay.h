// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file loading_overlay.h
/// @brief Over a widget whose work is under way (the card of a book being opened): the widget
///        darkened and a spinner turning on it

#ifndef KALAHARI_GUI_WIDGETS_LOADING_OVERLAY_H
#define KALAHARI_GUI_WIDGETS_LOADING_OVERLAY_H

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QEvent;
class QObject;
class QPaintEvent;

namespace kalahari::gui {

/// @brief The widget it covers darkened (the theme's shadow, see through) and a spinner
/// turning in its middle (the theme's bright text), until it is deleted
///
/// The work it waits for may keep the program busy, as the opening of a book does: then the
/// timer waits, but the events the program sends to its widgets meanwhile still turn the
/// spinner. A window that asks a question (a modal dialog) hides it until it is answered.
/// It takes no mouse clicks.
class LoadingOverlay : public QWidget {
public:
    /// The time between two positions of the spinner, in milliseconds
    static constexpr int FRAME_MS = 40;
    /// The angle the spinner turns between two positions, in degrees
    static constexpr int STEP_DEGREES = 30;

    /// @brief Cover @p covered, at once drawn on the screen
    explicit LoadingOverlay(QWidget* covered);
    ~LoadingOverlay() override;
    LoadingOverlay(const LoadingOverlay&) = delete;
    LoadingOverlay& operator=(const LoadingOverlay&) = delete;
    LoadingOverlay(LoadingOverlay&&) = delete;
    LoadingOverlay& operator=(LoadingOverlay&&) = delete;

    /// @brief The angle of the spinner now, in degrees (0-359)
    [[nodiscard]] int angle() const { return m_angle; }

protected:
    void paintEvent(QPaintEvent* event) override;
    /// @brief Follows the size of the widget it covers; turns while the program is busy;
    /// hides while a question waits for its answer
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The next position of the spinner, once FRAME_MS passed since the last one
    /// @return Whether it moved
    bool turn();

    QTimer m_timer;
    QElapsedTimer m_clock;     ///< Since the last position
    int m_angle = 0;
    int m_questions = 0;       ///< Modal windows shown now
    bool m_painting = false;   ///< Drawn now, from the events of the busy program
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_LOADING_OVERLAY_H
