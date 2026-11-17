"""System events of the program's windows, received outside the program's process.

SetWinEventHook with WINEVENT_OUTOFCONTEXT delivers the events to our own thread, so the
program is not touched. Each event is stamped with time.perf_counter() when it arrives:
the moment a window was shown or closed, measured from the input that caused it.
"""

import ctypes
import threading
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

EVENT_OBJECT_DESTROY = 0x8001
EVENT_OBJECT_SHOW = 0x8002
EVENT_OBJECT_HIDE = 0x8003
EVENT_OBJECT_NAMECHANGE = 0x800C
WINEVENT_OUTOFCONTEXT = 0x0000
OBJID_WINDOW = 0
CHILDID_SELF = 0
GA_ROOT = 2
WM_QUIT = 0x0012

WINEVENTPROC = ctypes.WINFUNCTYPE(None, wintypes.HANDLE, wintypes.DWORD, wintypes.HWND,
                                  wintypes.LONG, wintypes.LONG, wintypes.DWORD, wintypes.DWORD)

user32.SetWinEventHook.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.HMODULE, WINEVENTPROC,
                                   wintypes.DWORD, wintypes.DWORD, wintypes.DWORD]
user32.SetWinEventHook.restype = wintypes.HANDLE
user32.UnhookWinEvent.argtypes = [wintypes.HANDLE]
user32.GetMessageW.argtypes = [ctypes.POINTER(wintypes.MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT]
user32.GetMessageW.restype = wintypes.BOOL
user32.TranslateMessage.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.PostThreadMessageW.argtypes = [wintypes.DWORD, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
user32.GetAncestor.restype = wintypes.HWND
user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wintypes.HWND]
kernel32.GetCurrentThreadId.restype = wintypes.DWORD


def window_title(hwnd) -> str:
    length = user32.GetWindowTextLengthW(hwnd)
    buffer = ctypes.create_unicode_buffer(length + 2)
    user32.GetWindowTextW(hwnd, buffer, length + 2)
    return buffer.value


def window_class(hwnd) -> str:
    buffer = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buffer, 256)
    return buffer.value


class WindowEvent:
    __slots__ = ("time", "kind", "hwnd", "title", "class_name")

    def __init__(self, moment, kind, hwnd, title, class_name):
        self.time = moment
        self.kind = kind  # "shown", "hidden", "destroyed", "renamed"
        self.hwnd = hwnd
        self.title = title
        self.class_name = class_name

    def as_dict(self, origin: float = 0.0) -> dict:
        return {"t_ms": round((self.time - origin) * 1000, 1), "kind": self.kind,
                "hwnd": self.hwnd, "title": self.title, "class": self.class_name}


_KINDS = {EVENT_OBJECT_SHOW: "shown", EVENT_OBJECT_HIDE: "hidden",
          EVENT_OBJECT_DESTROY: "destroyed", EVENT_OBJECT_NAMECHANGE: "renamed"}


class WindowEvents:
    """Top-level windows of one process being shown, hidden, destroyed or renamed."""

    def __init__(self, pid: int):
        self.pid = pid
        self._events = []
        self._titles = {}  # hwnd -> last known title (a destroyed window has none any more)
        self._lock = threading.Lock()
        self._changed = threading.Condition(self._lock)
        self._thread = None
        self._thread_id = 0
        self._ready = threading.Event()
        self._proc = WINEVENTPROC(self._callback)
        self.error = None

    def start(self) -> "WindowEvents":
        self._thread = threading.Thread(target=self._run, name="window-events", daemon=True)
        self._thread.start()
        self._ready.wait(5)
        if self.error:
            raise RuntimeError(self.error)
        return self

    def stop(self) -> None:
        if self._thread and self._thread.is_alive():
            user32.PostThreadMessageW(self._thread_id, WM_QUIT, 0, 0)
            self._thread.join(2)

    def _run(self) -> None:
        self._thread_id = kernel32.GetCurrentThreadId()
        hooks = [user32.SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, None, self._proc,
                                        self.pid, 0, WINEVENT_OUTOFCONTEXT),
                 user32.SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, None,
                                        self._proc, self.pid, 0, WINEVENT_OUTOFCONTEXT)]
        if not all(hooks):
            self.error = f"SetWinEventHook failed (error {ctypes.get_last_error()})"
            self._ready.set()
            return
        self._ready.set()
        msg = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))
        for hook in hooks:
            user32.UnhookWinEvent(hook)

    def _callback(self, _hook, event, hwnd, id_object, id_child, _thread, _ms):
        moment = time.perf_counter()
        if id_object != OBJID_WINDOW or id_child != CHILDID_SELF or not hwnd:
            return
        if user32.GetAncestor(hwnd, GA_ROOT) != hwnd:
            return  # only top-level windows
        kind = _KINDS.get(event)
        if kind is None:
            return
        if kind == "destroyed":
            title, class_name = self._titles.get(hwnd, ("", ""))
        else:
            title, class_name = window_title(hwnd), window_class(hwnd)
        with self._changed:
            if kind != "destroyed":
                self._titles[hwnd] = (title, class_name)
            self._events.append(WindowEvent(moment, kind, hwnd, title, class_name))
            self._changed.notify_all()

    def events(self, since: float = 0.0) -> list:
        with self._lock:
            return [e for e in self._events if e.time >= since]

    def wait_for(self, predicate, since: float, timeout: float):
        """The first event after `since` for which predicate(event) is true, or None."""
        deadline = time.perf_counter() + timeout
        with self._changed:
            while True:
                for event in self._events:
                    if event.time >= since and predicate(event):
                        return event
                left = deadline - time.perf_counter()
                if left <= 0:
                    return None
                self._changed.wait(min(left, 0.05))

    def wait_shown(self, title: str, since: float, timeout: float):
        return self.wait_for(lambda e: e.kind == "shown" and e.title == title, since, timeout)

    def wait_closed(self, title: str, since: float, timeout: float):
        return self.wait_for(lambda e: e.kind in ("hidden", "destroyed") and e.title == title,
                             since, timeout)
