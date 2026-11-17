"""Screenshots and "ready on screen": when a region stops changing after an action.

Grabbing uses mss (GDI BitBlt on Windows). Each frame is stamped with time.perf_counter()
after the grab, so a moment is known to within one grab (a few ms for a dialog-sized
region) plus the screen's refresh interval.
"""

import time

from PIL import Image, ImageChops

from automat import stopping

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
              ignore_caret: bool = True, before=None) -> dict:
        """Grab the region again and again from now on. Returns when it has not changed for
        settle_ms, or at the timeout:
          first_frame_ms - from `start` to the first frame: a change finished before it is not
                       seen, so "no change" means "ready within first_frame_ms"
          changed     - whether anything changed after `start`
          last_change_ms - from `start` to the frame that showed the last change
          settled     - whether it stopped changing before the timeout
          frames, frame_ms - number of frames and the mean time per frame
        A change narrower than 4 px (a blinking caret) is ignored when ignore_caret is set.
        `before` is the region grabbed before the action: a change finished before the first
        frame is then seen too (at first_frame_ms, the latest moment it can have been)."""
        first_grab = time.perf_counter()
        previous = self.grab(rect)
        first_frame = time.perf_counter()
        frames = 1
        last_change = None
        if before is not None and differs(before, previous, ignore_caret):
            last_change = first_frame
        deadline = start + timeout
        while True:
            stopping.check()
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


def differs(old: Image.Image, new: Image.Image, ignore_caret: bool = True) -> bool:
    """Whether two grabs of a region differ (another size counts; a change narrower than 4 px,
    a blinking caret, does not when ignore_caret is set)."""
    if old.size != new.size:
        return True
    if old.tobytes() == new.tobytes():
        return False
    box = ImageChops.difference(old.convert("RGB"), new.convert("RGB")).getbbox()
    return bool(box) and not (ignore_caret and box[2] - box[0] <= 3)


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


def compare(pattern: Image.Image, image: Image.Image, tolerance: int = 24, masks=(),
            allowed: float = 0.002) -> dict:
    """An area against its approved pattern. A pixel differs when one of its channels differs
    by more than `tolerance` (0-255); the area matches when at most `allowed` of its pixels
    (0.2%: antialiasing, a caret) differ. `masks` are rectangles [x, y, width, height] in the
    area, left out of the comparison (a clock, a blinking caret).
      same   - whether the area matches
      share  - the share of differing pixels (0..1), None for another size
      diff   - a picture of the differences (red on the pattern, faded), None when the same"""
    if pattern.size != image.size:
        return {"same": False, "share": None, "diff": None,
                "detail": f"inny rozmiar: wzorzec {pattern.size[0]}×{pattern.size[1]}, "
                          f"ekran {image.size[0]}×{image.size[1]}"}
    first, second = pattern.convert("RGB"), image.convert("RGB")
    difference = ImageChops.difference(first, second)
    channels = difference.split()
    over = [c.point(lambda v: 255 if v > tolerance else 0) for c in channels]
    mask = ImageChops.lighter(ImageChops.lighter(over[0], over[1]), over[2])
    for x, y, width, height in masks or ():
        mask.paste(0, (int(x), int(y), int(x + width), int(y + height)))
    differing = mask.histogram()[255]
    total = max(1, first.size[0] * first.size[1])
    share = differing / total
    same = share <= allowed
    diff = None
    if not same:
        faded = Image.blend(first, Image.new("RGB", first.size, (255, 255, 255)), 0.6)
        diff = Image.composite(Image.new("RGB", first.size, (220, 0, 0)), faded, mask)
    return {"same": same, "share": share, "diff": diff,
            "detail": f"różni się {_percent(share)} pikseli (dopuszczalne {_percent(allowed)})"}


def _percent(share: float) -> str:
    return f"{share * 100:.2f}".rstrip("0").rstrip(".").replace(".", ",") + "%"
