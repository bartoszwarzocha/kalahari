/// @file caps_lock.h
/// @brief Caps Lock turned off by the program, where the system lets it: after autocorrect
///        changed a word typed with it on by mistake, as Word does

#pragma once

#include <functional>

namespace kalahari {
namespace gui {

/// @brief Turn Caps Lock off
///
/// Windows: the key pressed for the writer, when it is on. Linux with X11: its lock taken
/// off. Wayland and macOS do not let a program change it (on macOS Shift gives small
/// letters with it on, so autocorrect has no word to change there).
/// @return Whether Caps Lock is off now, as far as the system tells; false where the program
///         cannot change it, and the caller tells the writer
bool turnCapsLockOff();

/// @brief A Caps Lock in place of the system's, for tests: the key of the system stays as
///        it is; an empty function gives the system's key back
/// @param turnOff Does turnCapsLockOff()
void setCapsLockForTests(std::function<bool()> turnOff);

} // namespace gui
} // namespace kalahari
