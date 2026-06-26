/**
 * @file System/Apps/user/khicas/khicas_cjk.h
 * @brief C-visible bridge to the system's compile-time CJK glyph table.
 *
 * The KhiCAS text path (kcasporing_gl.c) is plain C, but the glyph table lives
 * in the C++20 header System/UI/Core/cjk_font.h (a consteval hash table over the
 * code points scanned by Scripts/gen_cjk_font.py). This wrapper exposes a single
 * C entry point so the porting layer can look up a Chinese glyph without pulling
 * the C++ header into a C translation unit.
 */
#ifndef KHICAS_CJK_H
#define KHICAS_CJK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Look up the 16x16 glyph for a Unicode code point.
 *
 * @param cp Unicode scalar value (intended for CJK ideographs, 0x4E00..0x9FFF).
 * @return Pointer to a 32-byte bitmap (16 rows x 2 bytes, MSB-left), or NULL if
 *         the code point is not present in the compiled glyph table.
 */
const uint8_t *khicas_cjk_glyph(uint32_t cp);

#ifdef __cplusplus
}
#endif

#endif /* KHICAS_CJK_H */
