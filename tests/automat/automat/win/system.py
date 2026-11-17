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


class LASTINPUTINFO(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.UINT), ("dwTime", wintypes.DWORD)]


user32.GetLastInputInfo.argtypes = [ctypes.POINTER(LASTINPUTINFO)]
kernel32.GetTickCount.restype = wintypes.DWORD


def idle_ms():
    """How long nobody has touched the mouse or the keyboard, in ms: the timer that the
    lock and the screen saver count. None when Windows does not tell."""
    info = LASTINPUTINFO(cbSize=ctypes.sizeof(LASTINPUTINFO))
    if not user32.GetLastInputInfo(ctypes.byref(info)):
        return None
    return (kernel32.GetTickCount() - info.dwTime) & 0xFFFFFFFF


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
        "idle_ms": idle_ms(),
    }


# --- Processes ---------------------------------------------------------------------------

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
psapi = ctypes.WinDLL("psapi", use_last_error=True)
psapi.EnumProcesses.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                                ctypes.POINTER(wintypes.DWORD)]
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]


def process_image(pid: int):
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return None
    try:
        size = wintypes.DWORD(1024)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            return None
        return buffer.value
    finally:
        kernel32.CloseHandle(handle)


def processes_of(image) -> list:
    """The ids of the running processes started from the program file `image`."""
    wanted = os.path.normcase(os.path.abspath(str(image)))
    count = 4096
    while True:
        array = (wintypes.DWORD * count)()
        needed = wintypes.DWORD()
        psapi.EnumProcesses(array, ctypes.sizeof(array), ctypes.byref(needed))
        if needed.value < ctypes.sizeof(array):
            break
        count *= 2
    found = []
    for pid in array[: needed.value // ctypes.sizeof(wintypes.DWORD)]:
        if pid and pid != os.getpid():
            path = process_image(pid)
            if path and os.path.normcase(os.path.abspath(path)) == wanted:
                found.append(pid)
    return found


# --- Known folders (the ones Qt's QStandardPaths uses on Windows) --------------------------

class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD), ("Data2", wintypes.WORD), ("Data3", wintypes.WORD),
                ("Data4", ctypes.c_ubyte * 8)]

    def __init__(self, text: str):
        super().__init__()
        import uuid  # noqa: PLC0415
        value = uuid.UUID(text)
        self.Data1, self.Data2, self.Data3 = value.fields[0], value.fields[1], value.fields[2]
        for i, byte in enumerate(value.bytes[8:]):
            self.Data4[i] = byte


FOLDERS = {
    "documents": "{FDD39AD0-238F-46AF-ADB4-6C85480369C7}",
    "roaming_app_data": "{3EB685DB-65F9-4CF6-A03A-E3EF65729F3D}",
    "local_app_data": "{F1B32785-6FBA-4FCF-9D55-7B8E7F157091}",
}
shell32 = ctypes.WinDLL("shell32", use_last_error=True)
ole32 = ctypes.WinDLL("ole32")
shell32.SHGetKnownFolderPath.argtypes = [ctypes.POINTER(GUID), wintypes.DWORD, wintypes.HANDLE,
                                         ctypes.POINTER(ctypes.c_wchar_p)]
ole32.CoTaskMemFree.argtypes = [ctypes.c_void_p]


def known_folder(name: str):
    path = ctypes.c_wchar_p()
    guid = GUID(FOLDERS[name])
    if shell32.SHGetKnownFolderPath(ctypes.byref(guid), 0, None, ctypes.byref(path)) != 0:
        return None
    try:
        return path.value
    finally:
        ole32.CoTaskMemFree(path)
