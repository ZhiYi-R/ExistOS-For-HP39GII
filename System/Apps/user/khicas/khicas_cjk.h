/**
 * @file System/Apps/user/khicas/khicas_cjk.h
 * @brief Narrow entry point to the system's compile-time CJK glyph table.
 *
 * The glyph table lives in the C++20 header System/UI/Core/cjk_font.h (a consteval
 * hash table over the code points scanned by Scripts/gen_cjk_font.py). This wrapper
 * lets the KhiCAS porting layer (kcasporing_gl.cpp) look up a Chinese glyph without
 * pulling that heavy consteval header into the rendering translation unit.
 */
#ifndef KHICAS_CJK_H
#define KHICAS_CJK_H

#include <cstdint>

/**
 * Look up the 16x16 glyph for a Unicode code point.
 *
 * @param cp Unicode scalar value (intended for CJK ideographs, 0x4E00..0x9FFF).
 * @return Pointer to a 32-byte bitmap (16 rows x 2 bytes, MSB-left), or NULL if
 *         the code point is not present in the compiled glyph table.
 */
const uint8_t *khicas_cjk_glyph(uint32_t cp);

#endif /* KHICAS_CJK_H */
