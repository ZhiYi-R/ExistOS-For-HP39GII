/**
 * @file System/Apps/user/khicas/khicas_cjk.cpp
 * @brief Thin wrapper over cjkfont::glyph for the KhiCAS porting layer.
 *
 * Keeps the C++20 consteval table header (cjk_font.h) out of the rendering TU
 * (kcasporing_gl.cpp) behind one narrow entry point. The generated data header
 * cjk_font_data.h is produced by the gen_cjk target (see System/CMakeLists.txt);
 * khicas_lib must depend on gen_cjk and carry the generated directory plus
 * System/UI/Core on its include path.
 */
#include "khicas_cjk.h"

#include "cjk_font.h"

const uint8_t *khicas_cjk_glyph(uint32_t cp) {
    return cjkfont::glyph(cp);
}
