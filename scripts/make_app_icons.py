#!/usr/bin/env python3
"""
make_app_icons.py - The icons of the program, made from its logo.

The logo (resources/images/app_logo.png) is cut to a square around the drawing and scaled
to the sizes each system asks for:

    resources/images/app_icon/kalahari_<N>.png   the window icon of the program and the
                                                 icon of the menus of Linux (16 to 512 px)
    packaging/icons/kalahari.ico                 Windows: the program file, its shortcuts
                                                 and its installer
    packaging/icons/kalahari.icns                macOS: Kalahari.app

The files are kept in the repository; run the script again after the logo changes.

Usage:
    python scripts/make_app_icons.py

Needs Pillow (pip install pillow).
"""

import sys
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
LOGO = REPO_ROOT / "resources" / "images" / "app_logo.png"
PNG_DIR = REPO_ROOT / "resources" / "images" / "app_icon"
ICONS_DIR = REPO_ROOT / "packaging" / "icons"

# The sizes of the window and menu icons (the hicolor theme of Linux has the same)
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256, 512]
# The sizes Windows takes from an .ico (Explorer, the taskbar, high DPI)
ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
# The margin around the drawing, as a part of the side of the square
MARGIN = 0.02


def square_logo(logo: Image.Image) -> Image.Image:
    """The drawing of the logo in a transparent square, centred, with a thin margin."""
    box = logo.getchannel("A").getbbox()
    if box is None:
        raise SystemExit(f"{LOGO} has no visible pixels")
    drawing = logo.crop(box)
    side = round(max(drawing.size) * (1 + 2 * MARGIN))
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(drawing, ((side - drawing.width) // 2, (side - drawing.height) // 2))
    return square


def scaled(square: Image.Image, size: int) -> Image.Image:
    return square.resize((size, size), Image.Resampling.LANCZOS)


def main() -> int:
    logo = Image.open(LOGO).convert("RGBA")
    square = square_logo(logo)

    PNG_DIR.mkdir(parents=True, exist_ok=True)
    for size in PNG_SIZES:
        scaled(square, size).save(PNG_DIR / f"kalahari_{size}.png", optimize=True)

    ICONS_DIR.mkdir(parents=True, exist_ok=True)
    # Each size of the .ico scaled from the square itself, not from the next larger size
    largest = scaled(square, max(ICO_SIZES))
    largest.save(ICONS_DIR / "kalahari.ico", sizes=[(s, s) for s in ICO_SIZES],
                 append_images=[scaled(square, s) for s in ICO_SIZES[:-1]])
    # The .icns takes its sizes (16 to 512, also at 2x) from the largest image given
    scaled(square, 1024).save(ICONS_DIR / "kalahari.icns")

    print(f"Made {len(PNG_SIZES)} PNG files in {PNG_DIR.relative_to(REPO_ROOT)}, "
          f"kalahari.ico and kalahari.icns in {ICONS_DIR.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
