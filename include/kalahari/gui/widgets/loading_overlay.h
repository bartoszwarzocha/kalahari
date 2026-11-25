// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

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
/// The spinner turns evenly, one turn in TURN_MS, its angle taken from the time, about 60
/// pictures a second. The work it waits for may keep the program busy, as the opening of a
/// book does: then the timer waits, but the beats of the work (core::BusyPulse) and the events
/// the program sends to its widgets meanwhile draw the next picture once FRAME_MS passed. A
/// window that asks a question (a modal dialog) hides it until it is answered. It takes no
/// mouse clicks.
class LoadingOverlay : public QWidget {
public:
    /// The time between two pictures of the spinner, in milliseconds (about 60 a second)
    static constexpr int FRAME_MS = 16;
    /// The time of one turn of the spinner, in milliseconds
    static constexpr int TURN_MS = 1000;

    /// @brief Cover @p covered, at once drawn on the screen
    explicit LoadingOverlay(QWidget* covered);
    ~LoadingOverlay() override;
    LoadingOverlay(const LoadingOverlay&) = delete;
    LoadingOverlay& operator=(const LoadingOverlay&) = delete;
    LoadingOverlay(LoadingOverlay&&) = delete;
    LoadingOverlay& operator=(LoadingOverlay&&) = delete;

    /// @brief The angle of the spinner now, in degrees (0-359): it follows the time
    [[nodiscard]] int angle() const;

    /// @brief The pictures of the spinner drawn so far
    [[nodiscard]] int frames() const { return m_frames; }

protected:
    void paintEvent(QPaintEvent* event) override;
    /// @brief Follows the size of the widget it covers; turns while the program is busy;
    /// hides while a question waits for its answer
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Draw the next picture at once, once FRAME_MS passed since the last one: the
    ///        program is busy, its timers wait
    void drawWhenDue();

    /// @brief A beat of the busy work (core::BusyPulse): the overlays draw their next picture
    static void beat();

    QTimer m_timer;
    QElapsedTimer m_clock;     ///< Since it covers the widget: the angle of the spinner
    QElapsedTimer m_frame;     ///< Since the last picture
    int m_frames = 0;
    int m_questions = 0;       ///< Modal windows shown now
    bool m_painting = false;   ///< Drawn now, from the busy program
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_LOADING_OVERLAY_H
