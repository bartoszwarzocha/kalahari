"""How long the program does not respond: a control message sent every 20 ms.

SendMessageTimeout with WM_NULL does not return until the window procedure has processed
the message (Microsoft Docs), so the time each call takes is the time the program's
window thread was busy. The longest call during a step is the step's longest "freeze".
"""

import ctypes
import threading
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

WM_NULL = 0x0000
SMTO_NORMAL = 0x0000

user32.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM,
                                       wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t)]
user32.SendMessageTimeoutW.restype = ctypes.c_ssize_t
user32.IsHungAppWindow.argtypes = [wintypes.HWND]
user32.IsHungAppWindow.restype = wintypes.BOOL
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL


class BusyProbe:
    """Sends WM_NULL to one window of the program every `interval` seconds. A busy stretch is
    seen to within about one interval (a probe of the GitHub machine: 0.8 s of work measured
    as 758–790 ms with 50 ms)."""

    def __init__(self, hwnd: int, interval: float = 0.02, timeout_ms: int = 60000):
        self.hwnd = hwnd
        self.interval = interval
        self.timeout_ms = timeout_ms
        self._samples = []  # (sent, answered or None when it timed out)
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None

    def start(self) -> "BusyProbe":
        self._thread = threading.Thread(target=self._run, name="busy-probe", daemon=True)
        self._thread.start()
        return self

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(self.timeout_ms / 1000 + 1)

    def _run(self) -> None:
        result = ctypes.c_size_t()
        while not self._stop.is_set() and user32.IsWindow(self.hwnd):
            sent = time.perf_counter()
            ok = user32.SendMessageTimeoutW(self.hwnd, WM_NULL, 0, 0, SMTO_NORMAL, self.timeout_ms,
                                            ctypes.byref(result))
            answered = time.perf_counter()
            with self._lock:
                self._samples.append((sent, answered if ok else None))
            self._stop.wait(max(0.0, self.interval - (answered - sent)))

    def longest_ms(self, start: float, end: float) -> float:
        """The longest time the program did not answer within [start, end], in ms. A call still
        unanswered counts until `end`."""
        longest = 0.0
        with self._lock:
            samples = list(self._samples)
        for sent, answered in samples:
            finish = answered if answered is not None else end
            if finish < start or sent > end:
                continue
            longest = max(longest, (min(finish, end) - max(sent, start)) * 1000)
        return round(longest, 1)

    def wait_responsive(self, since: float, timeout: float) -> bool:
        """Wait for an answer to a message sent after `since` (the program handled everything
        that came before)."""
        deadline = time.perf_counter() + timeout
        while time.perf_counter() < deadline:
            with self._lock:
                if any(s >= since and a is not None for s, a in self._samples):
                    return True
            time.sleep(0.01)
        return False


def is_hung(hwnd: int) -> bool:
    """The system's own view: the window has not taken messages for 5 seconds."""
    return bool(user32.IsHungAppWindow(hwnd))
