"""Generate ASCII bitmap glyphs from unifont for ExistOS UI.

Outputs C arrays matching the VGA_Ascii_5x8 / VGA_Ascii_6x12 / VGA_Ascii_8x16
format expected by ``System/UI/Core/UICore.h``.

Run from the repository root::

    uv run --project Scripts python gen_ascii.py
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = Path(__file__).resolve().parent.parent / "fonts" / "Assets" / "unifont-17.0.04.otf"

# ASCII printable range: 0x20 (space) .. 0x7E (~)
ASCII_FIRST = 0x20
ASCII_LAST = 0x7E
NUM_CHARS = ASCII_LAST - ASCII_FIRST + 1  # 95

# Target sizes: (label, height, width)
SIZES: list[tuple[str, int, int]] = [
    ("5x8", 8, 8),
    ("6x12", 12, 8),
    ("7x14", 14, 7),
    ("8x16", 16, 8),
]

RENDER_HEIGHT = 48  # render large then scale down
MARGIN = 12
THRESHOLD_BBOX = 128
THRESHOLD_EXTRACT = 160


def find_bounding_box(img: Image.Image, threshold: int) -> tuple[int, int, int, int]:
    """Return (left, top, right, bottom) bounding box of dark pixels."""
    pixels = img.load()
    width, height = img.size
    left = width
    right = 0
    top = height
    bottom = 0
    for y in range(height):
        for x in range(width):
            if pixels[x, y] < threshold:
                if x < left:
                    left = x
                if x > right:
                    right = x
                if y < top:
                    top = y
                if y > bottom:
                    bottom = y
    return left, top, right, bottom


def extract_bytes(img: Image.Image, width: int, height: int, threshold: int) -> list[int]:
    """Convert a grayscale image into a list of bytes (bit7 = leftmost pixel)."""
    pixels = img.load()
    out: list[int] = []
    for y in range(height):
        byte_val = 0
        for x in range(width):
            if pixels[x, y] < threshold:
                byte_val |= 0x80 >> x
        out.append(byte_val)
    return out


def render_all_ascii(
    font: ImageFont.FreeTypeFont,
    target_h: int,
    target_w: int,
) -> dict[int, list[int]]:
    """Return {codepoint: [bytes]} for all ASCII printable chars."""
    result: dict[int, list[int]] = {}

    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        char = chr(code)
        big_w = RENDER_HEIGHT + MARGIN * 2
        big_h = RENDER_HEIGHT + MARGIN * 2
        big_img = Image.new("L", (big_w, big_h), 255)
        draw = ImageDraw.Draw(big_img)
        draw.text((MARGIN, 0), char, font=font, fill=0)

        left, top, right, bottom = find_bounding_box(big_img, THRESHOLD_BBOX)
        if left > right:
            # Empty glyph → emit blank
            result[code] = [0] * target_h
            continue

        gh = bottom - top + 1
        gw = right - left + 1
        cropped = big_img.crop((left, top, right + 1, bottom + 1))

        # Scale maintaining aspect ratio to fit in target_w x target_h
        scale = min(target_w / gw, target_h / gh)
        dst_w = max(1, int(gw * scale))
        dst_h = max(1, int(gh * scale))
        scaled = cropped.resize((dst_w, dst_h), Image.Resampling.LANCZOS)

        # Paste centered
        tar_img = Image.new("L", (target_w, target_h), 255)
        off_x = (target_w - dst_w) // 2
        off_y = (target_h - dst_h) // 2
        tar_img.paste(scaled, (off_x, off_y))

        result[code] = extract_bytes(tar_img, target_w, target_h, THRESHOLD_EXTRACT)

    return result


def format_c_array(
    label: str,
    glyphs: dict[int, list[int]],
    target_h: int,
) -> str:
    """Format the complete C array for one font size."""
    lines: list[str] = []
    lines.append(f"const unsigned char VGA_Ascii_{label}[] = {{")
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        ch = chr(code) if 0x21 <= code <= 0x7E else f"0x{code:02X}"
        comment = f"  // {code:3d} '{ch}'"
        hex_parts = ",".join(f"0x{b:02X}" for b in glyphs[code])
        lines.append(f"    {hex_parts},{comment}")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    font_path = FONT_PATH
    if not font_path.exists():
        print(f"Error: font not found at {font_path}", file=sys.stderr)
        return 1

    font = ImageFont.truetype(str(font_path), RENDER_HEIGHT)

    for label, target_h, target_w in SIZES:
        print(f"Rendering {label} ({target_w}x{target_h})...", file=sys.stderr)
        glyphs = render_all_ascii(font, target_h, target_w)
        c_array = format_c_array(label, glyphs, target_h)
        print(c_array)

    print(f"Done — paste arrays into fonts/vgafont.c", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
