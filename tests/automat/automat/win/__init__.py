"""Windows layer of the test automaton.

Everything here works from outside the program, the way a person and a screen reader do:
real input (SendInput), UI Automation, system window events, screenshots and the
clipboard. Nothing is injected into the program under test.
"""

import ctypes
import sys

if sys.platform != "win32":  # pragma: no cover - imported only on Windows
    raise ImportError("automat.win works only on Windows")

_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ctypes.c_void_p(-4)


def make_dpi_aware() -> bool:
    """Use physical pixels everywhere, so UI Automation rectangles, the cursor and
    screenshots share one coordinate system. Call once, before any window work."""
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    try:
        user32.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
        user32.SetProcessDpiAwarenessContext.restype = ctypes.c_int
        if user32.SetProcessDpiAwarenessContext(_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2):
            return True
    except AttributeError:
        pass
    try:
        return bool(ctypes.WinDLL("shcore").SetProcessDpiAwareness(2) == 0)
    except (AttributeError, OSError):
        return bool(user32.SetProcessDPIAware())
