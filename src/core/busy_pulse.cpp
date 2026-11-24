// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file busy_pulse.cpp
/// @brief The beats of long work on the program's own thread, for what waits for it

#include "kalahari/core/busy_pulse.h"

#include <atomic>

namespace kalahari::core {

namespace {

/// Who is told of the beats, from any thread
std::atomic<BusyPulse::Listener>& listenerOfBeats() {
    static std::atomic<BusyPulse::Listener> listener{nullptr};
    return listener;
}

}  // namespace

void BusyPulse::beat() {
    if (const Listener listener = listenerOfBeats().load(std::memory_order_acquire)) {
        listener();
    }
}

void BusyPulse::setListener(Listener listener) {
    listenerOfBeats().store(listener, std::memory_order_release);
}

}  // namespace kalahari::core
