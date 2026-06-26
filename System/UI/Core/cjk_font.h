/**
 * @file System/UI/Core/cjk_font.h
 * @brief Compile-time CJK glyph lookup: UTF-8 code point -> 16x16 bitmap.
 *
 * The generated cjk_font_data.h carries only the CJK glyphs the UI actually
 * uses (CJK_CODEPOINTS / CJK_GLYPHS, see Scripts/gen_cjk_font.py). This header
 * builds a fixed-capacity open-addressing hash table over those code points at
 * compile time (consteval), so lookup is O(1) with no runtime construction and
 * no allocation — the whole table lives in .rodata. This replaces the old
 * GB2312-indexed fonts_hzk blob and its build-time UTF-8->GBK transcode.
 */
#pragma once
#include <array>
#include <cstdint>

#include "cjk_font_data.h"

namespace cjkfont {

/// Smallest power of two >= n / 0.75 (keeps the load factor under 0.75).
consteval unsigned cap_for(unsigned n) {
    unsigned c = 1;
    while (c * 3u < n * 4u) {
        c <<= 1;
    }
    return c;
}

/// 32 - log2(cap); used to take the high bits of the multiplicative hash.
consteval unsigned shift_for(unsigned cap) {
    unsigned s = 32;
    while (cap > 1) {
        cap >>= 1;
        --s;
    }
    return s;
}

inline constexpr unsigned CAP = cap_for(CJK_GLYPH_COUNT);
inline constexpr unsigned HASH_SHIFT = shift_for(CAP);

/// Knuth multiplicative hash, high bits -> [0, CAP).
constexpr unsigned hash_cp(uint32_t cp) {
    return (uint32_t)(cp * 2654435761u) >> HASH_SHIFT;
}

struct Slot {
    uint32_t cp;   ///< stored code point (0 = empty)
    int16_t idx;   ///< index into CJK_GLYPHS, or -1 if empty
};

/// Build the open-addressing table from CJK_CODEPOINTS at compile time.
consteval std::array<Slot, CAP> build_table() {
    std::array<Slot, CAP> t{};
    for (unsigned i = 0; i < CAP; ++i) {
        t[i] = Slot{0, -1};
    }
    for (unsigned i = 0; i < CJK_GLYPH_COUNT; ++i) {
        uint32_t cp = CJK_CODEPOINTS[i];
        unsigned h = hash_cp(cp);
        while (t[h].idx != -1) {
            h = (h + 1) & (CAP - 1);
        }
        t[h] = Slot{cp, (int16_t)i};
    }
    return t;
}

inline constexpr std::array<Slot, CAP> TABLE = build_table();

/// Runtime lookup: code point -> glyph index, or -1 if not in the table.
constexpr int glyph_index(uint32_t cp) {
    unsigned h = hash_cp(cp);
    for (unsigned probes = 0; probes < CAP; ++probes) {
        const Slot &s = TABLE[h];
        if (s.idx == -1) {
            return -1;  // empty slot: cp absent (open addressing invariant)
        }
        if (s.cp == cp) {
            return s.idx;
        }
        h = (h + 1) & (CAP - 1);
    }
    return -1;
}

/// 32-byte 16x16 glyph for a code point, or nullptr if not in the table.
inline const uint8_t *glyph(uint32_t cp) {
    int i = glyph_index(cp);
    return i < 0 ? nullptr : CJK_GLYPHS[i];
}

/// Decode one UTF-8 scalar at p, advance p past it, return the code point.
/// Handles 1/2/3-byte forms; a truncated or invalid sequence advances one
/// byte and returns that byte (which won't match any CJK glyph). Never reads
/// past a NUL terminator: a continuation byte is checked before the next byte
/// is fetched.
inline uint32_t utf8_next(const char *&p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) {
        ++p;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)p[1];
        if ((c1 & 0xC0) == 0x80) {
            p += 2;
            return ((c & 0x1Fu) << 6) | (c1 & 0x3Fu);
        }
        ++p;
        return c;
    }
    if ((c & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)p[1];
        if ((c1 & 0xC0) == 0x80) {
            unsigned char c2 = (unsigned char)p[2];
            if ((c2 & 0xC0) == 0x80) {
                p += 3;
                return ((c & 0x0Fu) << 12) | ((c1 & 0x3Fu) << 6) | (c2 & 0x3Fu);
            }
        }
        ++p;
        return c;
    }
    ++p;  // 4-byte lead or invalid byte: skip, won't match a CJK glyph
    return c;
}

}  // namespace cjkfont
