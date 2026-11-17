"""Screenshots and "ready on screen": when a region stops changing after an action.

Grabbing uses mss (GDI BitBlt on Windows). Each frame is stamped with time.perf_counter()
after the grab, so a moment is known to within one grab (a few ms for a dialog-sized
region) plus the screen's refresh interval.
"""

import time

from PIL import Image, ImageChops

try:
    from mss import MSS as _MSS
except ImportError:  # older mss
    from mss import mss as _MSS


class Screen:
    def __init__(self):
        self._sct = _MSS()

    def close(self) -> None:
        self._sct.close()

    def size(self) -> tuple:
        whole = self._sct.monitors[0]
        return whole["width"], whole["height"]

    def bounds(self) -> tuple:
        """The whole desktop (all screens) as (left, top, right, bottom)."""
        whole = self._sct.monitors[0]
        return (whole["left"], whole["top"], whole["left"] + whole["width"], whole["top"] + whole["height"])

    def grab(self, rect=None) -> Image.Image:
        """The screen, or the rectangle (left, top, right, bottom) of it, as an RGB image."""
        if rect is None:
            area = self._sct.monitors[0]
        else:
            left, top, right, bottom = rect
            area = {"left": int(left), "top": int(top),
                    "width": max(1, int(right - left)), "height": max(1, int(bottom - top))}
        shot = self._sct.grab(area)
        return Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")

    def watch(self, rect, start: float, settle_ms: float = 150, timeout: float = 10.0,
              ignore_caret: bool = True) -> dict:
        """Grab the region again and again from now on. Returns when it has not changed for
        settle_ms, or at the timeout:
          first_frame_ms - from `start` to the first frame: a change finished before it is not
                       seen, so "no change" means "ready within first_frame_ms"
          changed     - whether anything changed after `start`
          last_change_ms - from `start` to the frame that showed the last change
          settled     - whether it stopped changing before the timeout
          frames, frame_ms - number of frames and the mean time per frame
        A change narrower than 4 px (a blinking caret) is ignored when ignore_caret is set."""
        first_grab = time.perf_counter()
        previous = self.grab(rect)
        first_frame = time.perf_counter()
        frames = 1
        last_change = None
        deadline = start + timeout
        while True:
            image = self.grab(rect)
            moment = time.perf_counter()
            frames += 1
            if image.tobytes() != previous.tobytes():
                box = ImageChops.difference(image, previous).getbbox()
                if box and not (ignore_caret and box[2] - box[0] <= 3):
                    last_change = moment
                previous = image
            quiet_since = last_change if last_change is not None else start
            if (moment - quiet_since) * 1000 >= settle_ms:
                settled = True
                break
            if moment >= deadline:
                settled = False
                break
        return {
            "first_frame_ms": round((first_frame - start) * 1000, 1),
            "changed": last_change is not None,
            "last_change_ms": round((last_change - start) * 1000, 1) if last_change else 0.0,
            "settled": settled,
            "frames": frames,
            "frame_ms": round((time.perf_counter() - first_grab) * 1000 / frames, 1),
            "image": previous,
        }


def mean_color(image: Image.Image) -> tuple:
    small = image.convert("RGB").resize((1, 1), Image.Resampling.BOX)
    return small.getpixel((0, 0))


def luminance(color: tuple) -> float:
    """Relative luminance 0..1 (sRGB, WCAG formula)."""
    def channel(c):
        c = c / 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (channel(c) for c in color[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b
