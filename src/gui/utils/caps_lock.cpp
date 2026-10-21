/// @file caps_lock.cpp
/// @brief Caps Lock turned off by the program, where the system lets it

#include "kalahari/gui/utils/caps_lock.h"

#include <QGuiApplication>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX  // std::min and std::max, not the macros of windows.h
#endif
#include <windows.h>
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QLibrary>
#include <QtGui/qguiapplication_platform.h>
#endif

#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// @brief The Caps Lock of the tests (setCapsLockForTests())
std::function<bool()>& capsLockForTests() {
    static std::function<bool()> turnOff;
    return turnOff;
}

#ifdef Q_OS_WIN

/// @brief The key pressed and released for the writer, when its light is on
bool turnOffOnWindows() {
    if ((GetKeyState(VK_CAPITAL) & 0x0001) == 0) {
        return true;  // off already
    }
    INPUT keys[2] = {};
    keys[0].type = INPUT_KEYBOARD;
    keys[0].ki.wVk = VK_CAPITAL;
    keys[1].type = INPUT_KEYBOARD;
    keys[1].ki.wVk = VK_CAPITAL;
    keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, keys, sizeof(INPUT)) == 2;
}

#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

/// @brief The lock of the key taken off, on X11 (the X library is loaded when needed: the
///        program does not need it to start, as under Wayland)
bool turnOffOnX11() {
#if QT_CONFIG(xcb)
    auto* x11 = qGuiApp != nullptr
                    ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>()
                    : nullptr;
    if (x11 == nullptr || x11->display() == nullptr) {
        return false;  // Wayland, or no window system
    }
    using LockModifiers = int (*)(Display*, unsigned int, unsigned int, unsigned int);
    using Flush = int (*)(Display*);
    QLibrary xlib(QStringLiteral("X11"), 6);
    const auto lockModifiers = reinterpret_cast<LockModifiers>(xlib.resolve("XkbLockModifiers"));
    const auto flush = reinterpret_cast<Flush>(xlib.resolve("XFlush"));
    if (lockModifiers == nullptr || flush == nullptr) {
        return false;
    }
    constexpr unsigned int CORE_KEYBOARD = 0x0100;  // XkbUseCoreKbd
    constexpr unsigned int CAPS_LOCK = 1U << 1;     // LockMask
    if (lockModifiers(x11->display(), CORE_KEYBOARD, CAPS_LOCK, 0) == 0) {
        return false;
    }
    flush(x11->display());
    return true;
#else
    return false;
#endif
}

#endif

}  // namespace

bool turnCapsLockOff() {
    if (const std::function<bool()>& turnOff = capsLockForTests()) {
        return turnOff();
    }
#ifdef Q_OS_WIN
    return turnOffOnWindows();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return turnOffOnX11();
#else
    return false;
#endif
}

void setCapsLockForTests(std::function<bool()> turnOff) {
    capsLockForTests() = std::move(turnOff);
}

} // namespace gui
} // namespace kalahari
