"""The system clipboard: plain text and the "HTML Format" that programs put there on copy."""

import ctypes
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

CF_UNICODETEXT = 13
GMEM_MOVEABLE = 0x0002

user32.OpenClipboard.argtypes = [wintypes.HWND]
user32.OpenClipboard.restype = wintypes.BOOL
user32.CloseClipboard.restype = wintypes.BOOL
user32.EmptyClipboard.restype = wintypes.BOOL
user32.GetClipboardData.argtypes = [wintypes.UINT]
user32.GetClipboardData.restype = wintypes.HANDLE
user32.SetClipboardData.argtypes = [wintypes.UINT, wintypes.HANDLE]
user32.SetClipboardData.restype = wintypes.HANDLE
user32.RegisterClipboardFormatW.argtypes = [wintypes.LPCWSTR]
user32.RegisterClipboardFormatW.restype = wintypes.UINT
user32.IsClipboardFormatAvailable.argtypes = [wintypes.UINT]
user32.IsClipboardFormatAvailable.restype = wintypes.BOOL
user32.GetClipboardSequenceNumber.restype = wintypes.DWORD
kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalLock.restype = wintypes.LPVOID
kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalSize.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalSize.restype = ctypes.c_size_t
kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = wintypes.HGLOBAL

CF_HTML = user32.RegisterClipboardFormatW("HTML Format")


class _Open:
    def __enter__(self):
        for _ in range(50):  # another program may hold the clipboard for a moment
            if user32.OpenClipboard(None):
                return self
            time.sleep(0.02)
        raise OSError("the clipboard is held by another program")

    def __exit__(self, *_):
        user32.CloseClipboard()


def _read(fmt: int):
    if not user32.IsClipboardFormatAvailable(fmt):
        return None
    with _Open():
        handle = user32.GetClipboardData(fmt)
        if not handle:
            return None
        pointer = kernel32.GlobalLock(handle)
        try:
            size = kernel32.GlobalSize(handle)
            return ctypes.string_at(pointer, size)
        finally:
            kernel32.GlobalUnlock(handle)


def sequence() -> int:
    """Changes every time anything is put on the clipboard."""
    return user32.GetClipboardSequenceNumber()


def text():
    data = _read(CF_UNICODETEXT)
    if data is None:
        return None
    return data.decode("utf-16-le", errors="replace").split("\x00", 1)[0]


def html():
    """The HTML fragment from "HTML Format" (between StartFragment and EndFragment), or None."""
    data = _read(CF_HTML)
    if data is None:
        return None
    raw = data.split(b"\x00", 1)[0]
    header = {}
    for line in raw.split(b"\r\n")[:8]:
        if b":" in line:
            key, _, value = line.partition(b":")
            header[key.decode("ascii", "ignore")] = value.decode("ascii", "ignore")
    try:
        start, end = int(header["StartFragment"]), int(header["EndFragment"])
        return raw[start:end].decode("utf-8", errors="replace")
    except (KeyError, ValueError):
        return raw.decode("utf-8", errors="replace")


def set_text(value: str) -> None:
    data = (value + "\x00").encode("utf-16-le")
    handle = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
    pointer = kernel32.GlobalLock(handle)
    ctypes.memmove(pointer, data, len(data))
    kernel32.GlobalUnlock(handle)
    with _Open():
        user32.EmptyClipboard()
        user32.SetClipboardData(CF_UNICODETEXT, handle)


def clear() -> None:
    with _Open():
        user32.EmptyClipboard()
