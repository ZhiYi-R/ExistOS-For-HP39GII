"""Generate a compact CJK 16x16 glyph table from the strings actually used.

The on-screen Chinese renderer no longer indexes a full 261,696-byte GB2312
font blob (fonts_hzk). Instead it carries only the glyphs that the UI actually
references. This script scans the UTF-8 language source(s) for CJK code points,
rasterizes each from GNU Unifont at 16 px, and emits a C++ header of constexpr
arrays that the firmware compiles into .rodata. A compile-time hash table in
System/UI/Core/cjk_font.h maps code point -> table index for O(1) lookup.

Glyph format matches the retired HZK blob exactly (16 rows x 2 bytes, MSB-left,
threshold 160, rendered from the same Unifont), so on-screen glyphs are
byte-identical to before — this change is purely about encoding robustness and
flash size, not appearance.

Run from the repository root::

    uv run -q --project Scripts python Scripts/gen_cjk_font.py --preview
    uv run -q --project Scripts python Scripts/gen_cjk_font.py --write fonts/cjk_font_data.h
    uv run -q --project Scripts python Scripts/gen_cjk_font.py            # print to stdout
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = ROOT / "fonts" / "Assets" / "unifont-17.0.04.otf"
DEFAULT_SOURCE = ROOT / "System" / "UI" / "Lang" / "UI_Chinese.h"

# CJK Unified Ideographs (basic block). The UI uses only Han characters plus
# ASCII punctuation; ASCII is rendered from vgafont, so only this range matters.
CJK_FIRST = 0x4E00
CJK_LAST = 0x9FFF

NATIVE = 16          # Unifont halfwidth/CJK cell rendered at 16 px
CHAR_SIZE = 32       # bytes per glyph: 16 rows x 2 bytes
THRESHOLD = 160      # grayscale < THRESHOLD counts as ink (matches the old hzk)

DATA_HEADER = """/**
 * @file cjk_font_data.h
 * @brief Generated CJK 16x16 glyph table (code-point indexed). DO NOT EDIT.
 *
 * Produced by Scripts/gen_cjk_font.py by scanning the UTF-8 UI language source
 * for CJK code points and rasterizing each from GNU Unifont at 16 px. Each glyph
 * is 32 bytes (16 rows x 2 bytes, MSB-left) — byte-identical to the retired
 * fonts_hzk blob. System/UI/Core/cjk_font.h builds a compile-time hash table
 * (code point -> index) over CJK_CODEPOINTS for O(1) runtime lookup.
 */
#pragma once
#include <cstdint>
"""


def collect_codepoints(sources: list[Path]) -> list[int]:
    """Distinct CJK code points across all source files, sorted ascending."""
    found: set[int] = set()
    for src in sources:
        text = src.read_text(encoding="utf-8")
        for ch in text:
            cp = ord(ch)
            if CJK_FIRST <= cp <= CJK_LAST:
                found.add(cp)
    return sorted(found)


def extract_glyph_bytes(img: Image.Image) -> list[int]:
    """Extract 32 bytes from a 16x16 grayscale image (MSB-left, row-major)."""
    pixels = img.load()
    out: list[int] = []
    for y in range(NATIVE):
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


def render_glyph(font: ImageFont.FreeTypeFont, cp: int) -> list[int]:
    """Rasterize one code point into its 32-byte glyph (matches the old hzk)."""
    img = Image.new("L", (NATIVE, NATIVE), 255)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), chr(cp), font=font, fill=0)
    return extract_glyph_bytes(img)


def format_data_header(codepoints: list[int],
                       glyphs: dict[int, list[int]]) -> str:
    n = len(codepoints)
    lines = [DATA_HEADER, ""]
    lines.append(f"inline constexpr unsigned CJK_GLYPH_COUNT = {n};")
    lines.append("")
    lines.append("inline constexpr uint32_t CJK_CODEPOINTS[CJK_GLYPH_COUNT] = {")
    for i in range(0, n, 8):
        chunk = codepoints[i:i + 8]
        lines.append("    " + "".join(f"0x{cp:04X}," for cp in chunk))
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr uint8_t CJK_GLYPHS[CJK_GLYPH_COUNT][32] = {")
    for cp in codepoints:
        hex_parts = ",".join(f"0x{b:02X}" for b in glyphs[cp])
        lines.append(f"    {{{hex_parts}}},  // U+{cp:04X} '{chr(cp)}'")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def format_preview(codepoints: list[int], glyphs: dict[int, list[int]]) -> str:
    """ASCII-art dump of every glyph for terminal eyeballing."""
    lines = [f"==== {len(codepoints)} CJK glyphs (16x16) ===="]
    for cp in codepoints:
        lines.append(f"U+{cp:04X} '{chr(cp)}':")
        data = glyphs[cp]
        for r in range(NATIVE):
            row_bits = (data[r * 2] << 8) | data[r * 2 + 1]
            row = "".join("#" if (row_bits << x) & 0x8000 else "." for x in range(NATIVE))
            lines.append("    " + row)
        lines.append("")
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--source", type=Path, action="append", default=None,
                   help=f"language source to scan (default {DEFAULT_SOURCE})")
    g = p.add_mutually_exclusive_group()
    g.add_argument("--stdout", action="store_true",
                   help="print the generated header to stdout (default)")
    g.add_argument("--write", type=Path, metavar="PATH",
                   help="write the generated header to PATH")
    g.add_argument("--preview", action="store_true",
                   help="dump each glyph as ASCII art for eyeballing")
    p.add_argument("--threshold", type=int, default=THRESHOLD,
                   help=f"ink threshold 0-255 (default {THRESHOLD})")
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    global THRESHOLD
    THRESHOLD = args.threshold

    if not FONT_PATH.exists():
        print(f"Error: font not found at {FONT_PATH}", file=sys.stderr)
        return 1
    sources = args.source if args.source else [DEFAULT_SOURCE]
    for src in sources:
        if not src.exists():
            print(f"Error: source not found at {src}", file=sys.stderr)
            return 1

    codepoints = collect_codepoints(sources)
    if not codepoints:
        print("Error: no CJK code points found in sources", file=sys.stderr)
        return 1

    font = ImageFont.truetype(str(FONT_PATH), NATIVE)
    glyphs = {cp: render_glyph(font, cp) for cp in codepoints}

    if args.preview:
        print(format_preview(codepoints, glyphs))
        return 0

    header = format_data_header(codepoints, glyphs)

    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(header, encoding="utf-8", newline="\n")
        print(f"Wrote {args.write} ({len(codepoints)} glyphs, "
              f"threshold={THRESHOLD})", file=sys.stderr)
        return 0

    # default: --stdout
    print(header, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
