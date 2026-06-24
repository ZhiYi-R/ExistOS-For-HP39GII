"""Generate ASCII bitmap glyphs from unifont for ExistOS UI.

Strategy: render at Unifont's native 16 px, detect content bounding box,
and either centre-crop or bottom-align depending on whether the content
fits the target height.

- If content_height ≤ target_height → centre-crop (even padding).
- If content_height > target_height → bottom-align (preserve descenders).

For the 8x16 size the full 16 px render is used verbatim.

Run from the repository root::

    uv run --project Scripts python Scripts/gen_ascii.py
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = Path(__file__).resolve().parent.parent / "fonts" / "Assets" / "unifont-17.0.04.otf"

ASCII_FIRST = 0x20
ASCII_LAST = 0x7E

# (label, target_height, target_width (in bits))
SIZES: list[tuple[str, int, int]] = [
    ("5x8",  8,  8),
    ("6x12", 12, 8),
    ("7x14", 14, 7),
    ("8x16", 16, 8),
]

NATIVE = 16
THRESHOLD = 128


def find_content_bbox(img: Image.Image) -> tuple[int, int]:
    """Return (top, bottom) row of the first/last dark pixel."""
    pixels = img.load()
    top, bottom = NATIVE, 0
    for y in range(NATIVE):
        for x in range(NATIVE):
            if pixels[x, y] < THRESHOLD:
                if y < top:
                    top = y
                if y > bottom:
                    bottom = y
                    break  # once we find bottom for this row, move on
    return top, bottom


def extract_bytes(img: Image.Image, width: int) -> list[int]:
    """Extract one byte per row (MSB-left)."""
    pixels = img.load()
    out: list[int] = []
    for y in range(img.height):
        b = 0
        for x in range(min(width, img.width)):
            if pixels[x, y] < THRESHOLD:
                b |= 0x80 >> x
        out.append(b)
    return out


def render_char(char: str, target_h: int, target_w: int) -> list[int]:
    """Render one char with smart cropping.

    1. Native 16 px render.
    2. Find content bounding box (top, bottom rows).
    3. If content_height ≤ target_h: centre-crop.
    4. If content_height > target_h: bottom-align (keep descender).
    """
    font = ImageFont.truetype(str(FONT_PATH), NATIVE)
    native = Image.new("L", (NATIVE, NATIVE), 255)
    draw = ImageDraw.Draw(native)
    draw.text((0, -1), char, font=font, fill=0)

    # 8x16 — verbatim full render
    if target_h == NATIVE:
        return extract_bytes(native, target_w)

    top, bottom = find_content_bbox(native)
    if top > bottom:  # blank glyph
        return [0] * target_h

    content_h = bottom - top + 1

    if content_h <= target_h:
        # Centre-crop: evenly pad above and below content
        pad = target_h - content_h
        crop_top = max(0, top - pad // 2)
        # If that would go out of bounds, shift down
        if crop_top + target_h > NATIVE:
            crop_top = NATIVE - target_h
        crop = native.crop((0, crop_top, target_w, crop_top + target_h))
        return extract_bytes(crop, target_w)
    else:
        # Bottom-align: keep the bottommost target_h rows (preserve descender)
        crop_top = NATIVE - target_h
        crop = native.crop((0, crop_top, target_w, NATIVE))
        return extract_bytes(crop, target_w)


def format_c_array(label: str, glyphs: dict[int, list[int]],
                   target_h: int) -> str:
    lines: list[str] = []
    lines.append(f"const unsigned char VGA_Ascii_{label}[] = {{")
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        tag = chr(code) if 0x21 <= code <= 0x7E else f"0x{code:02X}"
        comment = f"  // {code:3d} '{tag}'"
        hex_parts = ",".join(f"0x{b:02X}" for b in glyphs[code])
        lines.append(f"    {hex_parts},{comment}")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    if not FONT_PATH.exists():
        print(f"Error: font not found at {FONT_PATH}", file=sys.stderr)
        return 1

    for label, target_h, target_w in SIZES:
        glyphs: dict[int, list[int]] = {}
        for code in range(ASCII_FIRST, ASCII_LAST + 1):
            glyphs[code] = render_char(chr(code), target_h, target_w)
        print(format_c_array(label, glyphs, target_h))

    return 0


if __name__ == "__main__":
    sys.exit(main())
