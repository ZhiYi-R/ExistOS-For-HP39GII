"""Generate HZK-format 16×16 Chinese bitmap fonts from unifont.

Reads all GB2312-80 characters from the standard grid (rows 0xA1-0xF7,
columns 0xA1-0xFE) and renders each from unifont at 16 px into the
HZK binary format used by ExistOS.

Outputs:
  fonts/fonts_hzk16s      — raw binary (261,696 bytes)
  fonts/fonts_hzk16s.ld   — linker-script BYTE() form for the same data
  fonts/fonts_hzk16h      — (identical copy, Hei variant placeholder)
  fonts/fonts_hzk16h.ld   — (identical copy)

Run from the repository root::

    uv run --project Scripts python gen_hzk.py
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = Path(__file__).resolve().parent.parent / "fonts" / "Assets" / "unifont-17.0.04.otf"
FONTS_DIR = Path(__file__).resolve().parent.parent / "fonts"

HZK_ROWS = 87   # 0xA1 .. 0xF7
HZK_COLS = 94   # 0xA1 .. 0xFE
CHAR_SIZE = 32  # bytes per character (16 rows × 2 bytes)
FILE_SIZE = HZK_ROWS * HZK_COLS * CHAR_SIZE  # 261,696

THRESHOLD = 160


def extract_glyph_bytes(img: Image.Image) -> list[int]:
    """Extract 32 bytes from a 16×16 grayscale image (MSB-left, row-major)."""
    pixels = img.load()
    out: list[int] = []
    for y in range(16):
        left_byte = 0
        right_byte = 0
        for x in range(8):
            if pixels[x, y] < THRESHOLD:
                left_byte |= 0x80 >> x
        for x in range(8, 16):
            if pixels[x, y] < THRESHOLD:
                right_byte |= 0x80 >> (x - 8)
        out.append(left_byte)
        out.append(right_byte)
    return out


def render_gb2312_char(font: ImageFont.FreeTypeFont, row: int, col: int) -> list[int]:
    """Render one GB2312 character and return its 32-byte HZK entry.

    Returns 32 zero bytes if the character is not valid GB2312.
    """
    gb_bytes = bytes([0xA1 + row, 0xA1 + col])
    try:
        unicode_char = gb_bytes.decode("gb2312")
    except ValueError:
        return [0] * CHAR_SIZE

    # Render at 16 px onto a 16×16 canvas
    img = Image.new("L", (16, 16), 255)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), unicode_char, font=font, fill=0)

    return extract_glyph_bytes(img)


def binary_to_ld(bin_data: bytes) -> str:
    """Convert raw binary data to the BYTE(0x..) linker-script format."""
    lines: list[str] = []
    for b in bin_data:
        lines.append(f"BYTE(0x{b:02X})")
    return "\n".join(lines)


def main() -> int:
    font_path = FONT_PATH
    if not font_path.exists():
        print(f"Error: font not found at {font_path}", file=sys.stderr)
        return 1

    print(f"Loading unifont from {font_path} ...", file=sys.stderr)
    font = ImageFont.truetype(str(font_path), 16)

    binary = bytearray()
    total = HZK_ROWS * HZK_COLS
    rendered = 0
    blank = 0

    for row in range(HZK_ROWS):
        gb_row = 0xA1 + row
        for col in range(HZK_COLS):
            gb_col = 0xA1 + col
            glyph = render_gb2312_char(font, row, col)
            binary.extend(glyph)
            if any(glyph):
                rendered += 1
            else:
                blank += 1

    assert len(binary) == FILE_SIZE, f"Expected {FILE_SIZE} bytes, got {len(binary)}"

    # Write binary file
    bin_path_s = FONTS_DIR / "fonts_hzk16s"
    bin_path_h = FONTS_DIR / "fonts_hzk16h"
    bin_path_s.write_bytes(binary)
    bin_path_h.write_bytes(binary)
    print(f"Wrote {bin_path_s} ({len(binary)} bytes, {rendered} rendered + {blank} blank)", file=sys.stderr)
    print(f"Wrote {bin_path_h} ({len(binary)} bytes, {rendered} rendered + {blank} blank)", file=sys.stderr)

    # Write .ld files (BYTE(...) per byte, one per line)
    ld_text = binary_to_ld(binary)
    ld_path_s = FONTS_DIR / "fonts_hzk16s.ld"
    ld_path_h = FONTS_DIR / "fonts_hzk16h.ld"
    ld_path_s.write_text(ld_text)
    ld_path_h.write_text(ld_text)
    print(f"Wrote {ld_path_s} ({len(ld_text.splitlines())} lines)", file=sys.stderr)
    print(f"Wrote {ld_path_h} ({len(ld_text.splitlines())} lines)", file=sys.stderr)

    print(f"Done!", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
