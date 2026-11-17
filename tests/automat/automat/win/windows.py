"""Top-level windows through plain Win32 calls, which never wait for the program: a busy
program still answers these (the system keeps the window list), unlike UI Automation."""

import ctypes
from ctypes import wintypes

from automat.win.events import window_class, window_title

user32 = ctypes.WinDLL("user32", use_last_error=True)

GA_ROOT = 2
WM_CLOSE = 0x0010

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wintypes.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsIconic.argtypes = [wintypes.HWND]
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.WindowFromPoint.argtypes = [wintypes.POINT]
user32.WindowFromPoint.restype = wintypes.HWND
user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
user32.GetAncestor.restype = wintypes.HWND
user32.GetForegroundWindow.restype = wintypes.HWND
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.SetForegroundWindow.argtypes = [wintypes.HWND]


def owner_pid(hwnd) -> int:
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    return pid.value


def rect(hwnd) -> tuple:
    r = wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(r)):
        return (0, 0, 0, 0)
    return (r.left, r.top, r.right, r.bottom)


def top_level(pid: int) -> list:
    """The visible top-level windows of the process, front to back:
    [{"hwnd", "title", "class", "rect"}]."""
    found = []

    def visit(hwnd, _):
        if owner_pid(hwnd) == pid and user32.IsWindowVisible(hwnd) and not user32.IsIconic(hwnd):
            left, top, right, bottom = rect(hwnd)
            if right > left and bottom > top:
                found.append({"hwnd": int(hwnd), "title": window_title(hwnd),
                              "class": window_class(hwnd), "rect": (left, top, right, bottom)})
        return True

    user32.EnumWindows(WNDENUMPROC(visit), 0)
    return found


def root_at(x: int, y: int) -> int:
    """The top-level window under the screen point."""
    hwnd = user32.WindowFromPoint(wintypes.POINT(int(x), int(y)))
    if not hwnd:
        return 0
    return int(user32.GetAncestor(hwnd, GA_ROOT) or 0)


def foreground() -> int:
    return int(user32.GetForegroundWindow() or 0)


def bring_forward(hwnd: int) -> bool:
    return bool(user32.SetForegroundWindow(hwnd))


def ask_to_close(hwnd: int) -> None:
    """What the close button of the title bar does."""
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)


__all__ = ["owner_pid", "rect", "top_level", "root_at", "foreground", "bring_forward",
           "ask_to_close", "window_title"]
