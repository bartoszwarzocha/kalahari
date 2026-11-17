"""Facts about the session the automaton runs in: screen, desktop, session, clock."""

import ctypes
import getpass
import os
import platform
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

DESKTOP_READOBJECTS = 0x0001
UOI_NAME = 2

user32.OpenInputDesktop.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
user32.OpenInputDesktop.restype = wintypes.HANDLE
user32.CloseDesktop.argtypes = [wintypes.HANDLE]
user32.GetUserObjectInformationW.argtypes = [wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID,
                                             wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
user32.GetForegroundWindow.restype = wintypes.HWND
user32.GetDpiForSystem.restype = wintypes.UINT
kernel32.ProcessIdToSessionId.argtypes = [wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
kernel32.WTSGetActiveConsoleSessionId.restype = wintypes.DWORD
kernel32.QueryPerformanceFrequency.argtypes = [ctypes.POINTER(ctypes.c_longlong)]
kernel32.QueryPerformanceCounter.argtypes = [ctypes.POINTER(ctypes.c_longlong)]

_freq = ctypes.c_longlong()
kernel32.QueryPerformanceFrequency(ctypes.byref(_freq))


def qpc() -> float:
    """QueryPerformanceCounter in seconds: one clock for every process on the machine."""
    now = ctypes.c_longlong()
    kernel32.QueryPerformanceCounter(ctypes.byref(now))
    return now.value / _freq.value


def input_desktop():
    """The name of the desktop that receives input ("Default" when someone can work on it),
    or None when it cannot be opened (locked screen, no interactive session)."""
    handle = user32.OpenInputDesktop(0, False, DESKTOP_READOBJECTS)
    if not handle:
        return None
    try:
        buffer = ctypes.create_unicode_buffer(256)
        needed = wintypes.DWORD()
        if not user32.GetUserObjectInformationW(handle, UOI_NAME, buffer, ctypes.sizeof(buffer),
                                                ctypes.byref(needed)):
            return None
        return buffer.value
    finally:
        user32.CloseDesktop(handle)


def session_info() -> dict:
    session = wintypes.DWORD()
    kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(session))
    from automat.win.events import window_title
    foreground = user32.GetForegroundWindow()
    return {
        "windows": platform.platform(),
        "python": platform.python_version(),
        "user": getpass.getuser(),
        "session": session.value,
        "console_session": kernel32.WTSGetActiveConsoleSessionId(),
        "input_desktop": input_desktop(),
        "screen": [user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)],
        "virtual_screen": [user32.GetSystemMetrics(78), user32.GetSystemMetrics(79)],
        "dpi": user32.GetDpiForSystem(),
        "foreground": window_title(foreground) if foreground else None,
    }
