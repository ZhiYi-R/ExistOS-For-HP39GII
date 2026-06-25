"""Transcode a text file from UTF-8 to another byte encoding.

The repository keeps all source in UTF-8 (consistent editor / git / CI behaviour).
But the firmware's on-screen CJK renderer (`draw_char_GBK16`) decodes GB2312 and
`arm-none-eabi-gcc` has no iconv, so string literals reach the binary as raw source
bytes. This script produces a GBK-encoded copy of a UTF-8 header at build time, so
the checked-in source stays UTF-8 while the firmware gets the bytes it needs.

Encoding errors are fatal: a character missing from the target encoding aborts the
build rather than being silently dropped.

Run from the repository root::

    uv run --project Scripts python Scripts/transcode.py <input> <output> <encoding>
"""

from __future__ import annotations

import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        sys.stderr.write(
            "usage: transcode.py <input> <output> <encoding>\n"
        )
        return 2

    src = Path(argv[1])
    dst = Path(argv[2])
    encoding = argv[3]

    text = src.read_text(encoding="utf-8")
    try:
        data = text.encode(encoding, errors="strict")
    except UnicodeEncodeError as exc:
        bad = text[exc.start:exc.end]
        sys.stderr.write(
            f"transcode.py: {src}: cannot encode {bad!r} "
            f"(offset {exc.start}) to {encoding}: {exc.reason}\n"
        )
        return 1

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
