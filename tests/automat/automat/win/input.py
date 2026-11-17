"""Real mouse and keyboard input through SendInput.

The cursor really moves and the keys really go through the system input queue, so the
program receives exactly what it would receive from a person. Every action returns the
moment (time.perf_counter, in seconds) when its last input event was handed to the
system: that is T0 of the step.
"""

import ctypes
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

INPUT_MOUSE = 0
INPUT_KEYBOARD = 1

MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_VIRTUALDESK = 0x4000
MOUSEEVENTF_ABSOLUTE = 0x8000

KEYEVENTF_EXTENDEDKEY = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004

SM_XVIRTUALSCREEN = 76
SM_YVIRTUALSCREEN = 77
SM_CXVIRTUALSCREEN = 78
SM_CYVIRTUALSCREEN = 79

ULONG_PTR = ctypes.c_size_t


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG), ("mouseData", wintypes.DWORD),
                ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ULONG_PTR)]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD), ("dwFlags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("dwExtraInfo", ULONG_PTR)]


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [("uMsg", wintypes.DWORD), ("wParamL", wintypes.WORD), ("wParamH", wintypes.WORD)]


class _INPUTUNION(ctypes.Union):
    _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT), ("hi", HARDWAREINPUT)]


class INPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", wintypes.DWORD), ("u", _INPUTUNION)]


user32.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
user32.SendInput.restype = wintypes.UINT
user32.GetSystemMetrics.argtypes = [ctypes.c_int]
user32.GetSystemMetrics.restype = ctypes.c_int
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.SetCursorPos.restype = wintypes.BOOL
user32.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
user32.GetCursorPos.restype = wintypes.BOOL
user32.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user32.MapVirtualKeyW.restype = wintypes.UINT


class InputError(RuntimeError):
    """The system did not take the input (for example UIPI blocked it, or the desktop is locked)."""


def _send(inputs: list) -> float:
    array = (INPUT * len(inputs))(*inputs)
    sent = user32.SendInput(len(inputs), array, ctypes.sizeof(INPUT))
    moment = time.perf_counter()
    if sent != len(inputs):
        raise InputError(f"SendInput took {sent} of {len(inputs)} events "
                         f"(error {ctypes.get_last_error()})")
    return moment


def cursor_position() -> tuple:
    point = wintypes.POINT()
    user32.GetCursorPos(ctypes.byref(point))
    return point.x, point.y


def _mouse(flags: int, dx: int = 0, dy: int = 0, data: int = 0) -> INPUT:
    event = INPUT(type=INPUT_MOUSE)
    event.mi = MOUSEINPUT(dx, dy, data & 0xFFFFFFFF, flags, 0, 0)
    return event


def move_to(x: int, y: int, steps: int = 6, duration: float = 0.06) -> None:
    """Move the cursor to the screen point (x, y) in physical pixels, in a few steps, like a
    hand, so the program sees the cursor enter and hover over the target."""
    left = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
    top = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
    width = max(2, user32.GetSystemMetrics(SM_CXVIRTUALSCREEN))
    height = max(2, user32.GetSystemMetrics(SM_CYVIRTUALSCREEN))
    start_x, start_y = cursor_position()
    steps = max(1, steps)
    for i in range(1, steps + 1):
        px = start_x + (x - start_x) * i / steps
        py = start_y + (y - start_y) * i / steps
        nx = round((px - left) * 65535 / (width - 1))
        ny = round((py - top) * 65535 / (height - 1))
        _send([_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, nx, ny)])
        if i < steps:
            time.sleep(duration / steps)
    if cursor_position() != (x, y):
        # Rounding of the normalised coordinates can miss by a pixel: put the cursor exactly.
        user32.SetCursorPos(int(x), int(y))


_BUTTONS = {
    "left": (MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP),
    "right": (MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP),
    "middle": (MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP),
}


def click(x: int, y: int, button: str = "left", double: bool = False, modifiers: tuple = ()) -> float:
    """Click at (x, y); returns the moment the (last) button was released."""
    down, up = _BUTTONS[button]
    move_to(x, y)
    time.sleep(0.03)
    held = [_vk_of(name) for name in modifiers]
    for vk in held:
        _send([_key(vk, False)])
    moment = 0.0
    for i in range(2 if double else 1):
        _send([_mouse(down)])
        time.sleep(0.02)
        moment = _send([_mouse(up)])
        if double and i == 0:
            time.sleep(0.06)
    for vk in reversed(held):
        _send([_key(vk, True)])
    return moment


def drag(x1: int, y1: int, x2: int, y2: int) -> float:
    """Press the left button at (x1, y1), move to (x2, y2) and release there."""
    move_to(x1, y1)
    time.sleep(0.03)
    _send([_mouse(MOUSEEVENTF_LEFTDOWN)])
    time.sleep(0.08)
    move_to(x2, y2, steps=12, duration=0.25)
    time.sleep(0.08)
    return _send([_mouse(MOUSEEVENTF_LEFTUP)])


def wheel(x: int, y: int, notches: int) -> float:
    """Turn the wheel over (x, y); positive notches scroll up."""
    move_to(x, y)
    return _send([_mouse(MOUSEEVENTF_WHEEL, data=120 * notches)])


# --- Keyboard --------------------------------------------------------------------------

VK = {
    "backspace": 0x08, "tab": 0x09, "enter": 0x0D, "return": 0x0D, "shift": 0x10, "ctrl": 0x11,
    "control": 0x11, "alt": 0x12, "pause": 0x13, "capslock": 0x14, "esc": 0x1B, "escape": 0x1B,
    "space": 0x20, "pageup": 0x21, "pagedown": 0x22, "end": 0x23, "home": 0x24, "left": 0x25,
    "up": 0x26, "right": 0x27, "down": 0x28, "printscreen": 0x2C, "insert": 0x2D, "delete": 0x2E,
    "del": 0x2E, "win": 0x5B, "apps": 0x5D, "menu": 0x5D, "numlock": 0x90,
    "plus": 0xBB, "comma": 0xBC, "minus": 0xBD, "period": 0xBE,
}
for _n in range(1, 25):
    VK[f"f{_n}"] = 0x6F + _n

# Keys that need the extended-key flag (their scan codes start with E0).
_EXTENDED = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2C, 0x2D, 0x2E, 0x5B, 0x5D, 0x90}


def _vk_of(name: str) -> int:
    key = name.strip().lower()
    if key in VK:
        return VK[key]
    if len(key) == 1 and ("a" <= key <= "z" or "0" <= key <= "9"):
        return ord(key.upper())
    raise ValueError(f"unknown key: {name!r}")


def _key(vk: int, up: bool) -> INPUT:
    flags = (KEYEVENTF_KEYUP if up else 0) | (KEYEVENTF_EXTENDEDKEY if vk in _EXTENDED else 0)
    event = INPUT(type=INPUT_KEYBOARD)
    event.ki = KEYBDINPUT(vk, user32.MapVirtualKeyW(vk, 0), flags, 0, 0)
    return event


def parse_chord(chord: str) -> list:
    """'Ctrl+Shift+I' -> [VK_CONTROL, VK_SHIFT, 'I']; a lone '+' is the plus key."""
    if chord.strip() == "+":
        return [VK["plus"]]
    parts = [p for p in chord.split("+")]
    keys = []
    for i, part in enumerate(parts):
        if part == "" and i == len(parts) - 1:
            keys.append(VK["plus"])  # 'Ctrl++'
        elif part != "":
            keys.append(_vk_of(part))
    return keys


def press(chord: str) -> float:
    """Press a key chord such as 'Esc', 'Ctrl+Shift+I' or 'Alt+F4'; returns the moment the
    main key was released. Several chords can be given separated by spaces: 'Alt+W Down'."""
    moment = 0.0
    for single in chord.split():
        keys = parse_chord(single)
        for vk in keys:
            _send([_key(vk, False)])
            time.sleep(0.015)
        for vk in reversed(keys):
            stamp = _send([_key(vk, True)])
            if vk == keys[-1]:
                moment = stamp
            time.sleep(0.01)
        time.sleep(0.03)
    return moment


def _unicode(unit: int, up: bool) -> INPUT:
    event = INPUT(type=INPUT_KEYBOARD)
    event.ki = KEYBDINPUT(0, unit, KEYEVENTF_UNICODE | (KEYEVENTF_KEYUP if up else 0), 0, 0)
    return event


def type_text(text: str, interval: float = 0.02) -> float:
    """Type text character by character, any language (VK_PACKET); a new line is Enter and a
    tab is Tab. Returns the moment the last character was released."""
    moment = 0.0
    for char in text:
        if char == "\n":
            moment = press("Enter")
        elif char == "\t":
            moment = press("Tab")
        else:
            data = char.encode("utf-16-le")
            units = [int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data), 2)]
            _send([_unicode(u, False) for u in units])
            moment = _send([_unicode(u, True) for u in units])
        time.sleep(interval)
    return moment
