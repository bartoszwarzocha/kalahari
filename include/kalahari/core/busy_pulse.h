// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file busy_pulse.h
/// @brief The beats of long work on the program's own thread (a book being opened), for what
///        waits for it to draw itself meanwhile

#pragma once

namespace kalahari::core {

/// @brief Long work that keeps the program busy (the opening of a book: its files, its texts)
/// beats between its steps; the listener, a spinner that waits for the work
/// (gui::LoadingOverlay), may draw itself then, as the program has no time for its timers.
///
/// A beat costs nothing while nobody listens. The listener is told on the thread that beats:
/// it does nothing on a thread other than its own.
class BusyPulse {
public:
    /// What is told of each beat
    using Listener = void (*)();

    /// @brief A step of the work is done: the listener, if any, is told
    static void beat();

    /// @brief Who is told of the beats; nullptr: nobody
    static void setListener(Listener listener);
};

}  // namespace kalahari::core
