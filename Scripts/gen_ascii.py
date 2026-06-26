"""Generate ASCII bitmap glyphs from Unifont for ExistOS UI.

Strategy: render each printable ASCII glyph at Unifont's native 16 px, then
DOWNSCALE (not crop) into the target cell. A single shared vertical band —
the union of every glyph's ink, computed once — is cropped identically for
all glyphs, so every size keeps one common baseline. The band is then
resampled to (target_w, target_h) and thresholded to 1 bit.

Why not crop: Unifont is a 16 px design. Cropping a 16 px glyph into an 8 px
cell chops the tops off tall glyphs and, when the crop window is chosen per
glyph, scrambles the shared baseline. Downscaling keeps the whole glyph and a
single band keeps the baseline consistent.

Cell convention (matches draw_char_ascii / khicas kcasporing_gl.c): glyph N =
(ch - ' ') * target_h, one byte per row, MSB = leftmost pixel. Ink is packed
left-aligned into the first target_w columns; the remaining columns stay blank
so adjacent glyphs (advance 6/6/7/8 px for 5x8/6x12/7x14/8x16) do not collide.

For 8x16 the full 16 px render is emitted verbatim (target == native).

Run from the repository root::

    uv run -q --project Scripts python Scripts/gen_ascii.py --preview   # eyeball
    uv run -q --project Scripts python Scripts/gen_ascii.py --write     # write vgafont.c
    uv run -q --project Scripts python Scripts/gen_ascii.py             # print to stdout
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = ROOT / "fonts" / "Assets" / "unifont-17.0.04.otf"
VGAFONT_PATH = ROOT / "fonts" / "vgafont.c"

ASCII_FIRST = 0x20
ASCII_LAST = 0x7E

# (label, target_height, target_width) — N x M label means N px wide, M px tall.
SIZES: list[tuple[str, int, int]] = [
    ("5x8",  8,  5),
    ("6x12", 12, 6),
    ("7x14", 14, 7),
    ("8x16", 16, 8),
]

NATIVE = 16          # Unifont halfwidth cell is 8 wide x 16 tall
SRC_W = 8            # Unifont halfwidth ink lives in the left 8 columns
THRESHOLD = 160      # grayscale < THRESHOLD counts as ink (tuned for downscale)
RESAMPLE = Image.BOX  # area-average; best for downscaling pixel fonts

_RESAMPLE_BY_NAME = {
    "box": Image.BOX,
    "lanczos": Image.LANCZOS,
    "bilinear": Image.BILINEAR,
    "nearest": Image.NEAREST,
}

# Doxygen preamble preserved verbatim when rewriting fonts/vgafont.c.
VGAFONT_HEADER = """/**
 * @file fonts/vgafont.c
 * @brief vgafont module
 */
"""


def render_native(char: str) -> Image.Image:
    """Render one glyph at Unifont's native 16 px (white bg, black ink)."""
    font = ImageFont.truetype(str(FONT_PATH), NATIVE)
    img = Image.new("L", (NATIVE, NATIVE), 255)
    draw = ImageDraw.Draw(img)
    draw.text((0, -1), char, font=font, fill=0)
    return img


def ink_rows(img: Image.Image, threshold: int) -> tuple[int, int]:
    """Return (top, bottom) row of the first/last row containing any ink.

    Returns (NATIVE, -1) for a blank glyph (top > bottom).
    """
    pixels = img.load()
    top, bottom = NATIVE, -1
    for y in range(NATIVE):
        for x in range(NATIVE):
            if pixels[x, y] < threshold:
                if y < top:
                    top = y
                if y > bottom:
                    bottom = y
                break
    return top, bottom


def global_band(threshold: int) -> tuple[int, int]:
    """Union vertical ink band across all printable ASCII (shared baseline)."""
    top, bottom = NATIVE, -1
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        t, b = ink_rows(render_native(chr(code)), threshold)
        if b < t:  # blank glyph
            continue
        top = min(top, t)
        bottom = max(bottom, b)
    if bottom < top:  # everything blank (shouldn't happen)
        return 0, NATIVE - 1
    return top, bottom


def pack_bytes(img: Image.Image, width: int, threshold: int) -> list[int]:
    """One byte per row, MSB-left, ink = pixel < threshold."""
    pixels = img.load()
    out: list[int] = []
    for y in range(img.height):
        b = 0
        for x in range(min(width, img.width)):
            if pixels[x, y] < threshold:
                b |= 0x80 >> x
        out.append(b)
    return out


def render_glyph(char: str, target_h: int, target_w: int,
                 band: tuple[int, int], threshold: int,
                 resample: int) -> list[int]:
    """Downscale one glyph into a (target_w x target_h) 1-bit cell.

    8x16 (target_h == NATIVE) is emitted verbatim. Smaller sizes crop the
    shared band and resample both axes, preserving a common baseline.
    """
    native = render_native(char)

    if target_h == NATIVE:
        return pack_bytes(native, target_w, threshold)

    g_top, g_bottom = band
    box = native.crop((0, g_top, SRC_W, g_bottom + 1))
    scaled = box.resize((target_w, target_h), resample)
    return pack_bytes(scaled, target_w, threshold)


def build_table(label: str, target_h: int, target_w: int,
                band: tuple[int, int], threshold: int,
                resample: int) -> dict[int, list[int]]:
    return {
        code: render_glyph(chr(code), target_h, target_w, band, threshold, resample)
        for code in range(ASCII_FIRST, ASCII_LAST + 1)
    }


def format_c_array(label: str, glyphs: dict[int, list[int]]) -> str:
    lines = [f"const unsigned char VGA_Ascii_{label}[] = {{"]
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        tag = chr(code) if 0x21 <= code <= 0x7E else f"0x{code:02X}"
        comment = f"  // {code:3d} '{tag}'"
        hex_parts = ",".join(f"0x{b:02X}" for b in glyphs[code])
        lines.append(f"    {hex_parts},{comment}")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def format_preview(label: str, target_h: int, target_w: int,
                   glyphs: dict[int, list[int]]) -> str:
    """ASCII-art dump of every glyph for terminal eyeballing."""
    lines = [f"==== {label}  ({target_w}x{target_h}) ===="]
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        tag = chr(code) if 0x21 <= code <= 0x7E else " "
        lines.append(f"'{tag}' (0x{code:02X}):")
        for b in glyphs[code]:
            row = "".join("#" if (b << x) & 0x80 else "." for x in range(target_w))
            lines.append("    " + row)
        lines.append("")
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_mutually_exclusive_group()
    g.add_argument("--stdout", action="store_true",
                   help="print the four C arrays to stdout (default)")
    g.add_argument("--write", action="store_true",
                   help="rewrite fonts/vgafont.c in place")
    g.add_argument("--preview", action="store_true",
                   help="dump each glyph as ASCII art for eyeballing")
    p.add_argument("--threshold", type=int, default=THRESHOLD,
                   help=f"ink threshold 0-255 (default {THRESHOLD})")
    p.add_argument("--resample", choices=sorted(_RESAMPLE_BY_NAME), default="box",
                   help="downscale filter (default box)")
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if not FONT_PATH.exists():
        print(f"Error: font not found at {FONT_PATH}", file=sys.stderr)
        return 1

    threshold = args.threshold
    resample = _RESAMPLE_BY_NAME[args.resample]
    band = global_band(threshold)

    tables = {
        label: build_table(label, th, tw, band, threshold, resample)
        for label, th, tw in SIZES
    }

    if args.preview:
        for label, th, tw in SIZES:
            print(format_preview(label, th, tw, tables[label]))
        return 0

    arrays = "\n".join(format_c_array(label, tables[label]) for label, _, _ in SIZES)

    if args.write:
        VGAFONT_PATH.write_text(VGAFONT_HEADER + "\n" + arrays,
                                encoding="utf-8", newline="\n")
        print(f"Wrote {VGAFONT_PATH} (band={band}, threshold={threshold}, "
              f"resample={args.resample})", file=sys.stderr)
        return 0

    # default: --stdout
    print(arrays, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
