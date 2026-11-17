"""Text read from the screen, for what the program draws without a description for screen
readers (a chart, a PDF page): the OCR built into Windows (Windows.Media.Ocr), through the
winrt packages. Without them, or without an OCR language on the computer, read() says why.
"""

import asyncio

try:  # optional: the winrt-* packages of requirements.txt
    from winrt.windows.globalization import Language
    from winrt.windows.graphics.imaging import BitmapPixelFormat, SoftwareBitmap
    from winrt.windows.media.ocr import OcrEngine
    from winrt.windows.security.cryptography import CryptographicBuffer
    AVAILABLE = True
except ImportError:  # pragma: no cover - depends on the computer
    AVAILABLE = False


def _engine():
    for tag in ("pl", "pl-PL"):
        try:
            language = Language(tag)
            if OcrEngine.is_language_supported(language):
                return OcrEngine.try_create_from_language(language)
        except Exception:  # noqa: BLE001 - try the next one
            continue
    return OcrEngine.try_create_from_user_profile_languages()


async def _recognize(image):
    rgba = image.convert("RGBA")
    red, green, blue, alpha = rgba.split()
    from PIL import Image  # noqa: PLC0415
    bgra = Image.merge("RGBA", (blue, green, red, alpha)).tobytes()
    try:
        buffer = CryptographicBuffer.create_from_byte_array(bgra)
    except TypeError:  # older pywinrt: a list of numbers
        buffer = CryptographicBuffer.create_from_byte_array(list(bgra))
    bitmap = SoftwareBitmap.create_copy_from_buffer(buffer, BitmapPixelFormat.BGRA8, rgba.width, rgba.height)
    engine = _engine()
    if engine is None:
        return None, "na tym komputerze nie ma języka do rozpoznawania tekstu (OCR)"
    result = await engine.recognize_async(bitmap)
    return " ".join(line.text for line in result.lines), None


def read(image):
    """(text, None) or (None, why not). Small areas are enlarged: OCR wants letters at least
    about 20 px high."""
    if not AVAILABLE:
        return None, "nie ma pakietów winrt do rozpoznawania tekstu (OCR); pip install -r requirements.txt"
    if image.height < 60:
        scale = max(2, round(60 / max(1, image.height)))
        image = image.resize((image.width * scale, image.height * scale))
    try:
        return asyncio.run(_recognize(image))
    except Exception as error:  # noqa: BLE001 - OCR is a help, its failure a detail
        return None, f"rozpoznawanie tekstu nie powiodło się: {error}"
