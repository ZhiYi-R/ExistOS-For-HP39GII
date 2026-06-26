/**
 * @file System/Apps/user/khicas/khicas_cjk.cpp
 * @brief extern "C" wrapper over cjkfont::glyph for the C porting layer.
 *
 * cjk_font.h is C++20 (consteval table); this single small TU bridges it to C.
 * The generated data header cjk_font_data.h is produced by the gen_cjk target
 * (see System/CMakeLists.txt); khicas_lib must depend on gen_cjk and carry the
 * generated directory plus System/UI/Core on its include path.
 */
#include "khicas_cjk.h"

#include "cjk_font.h"

extern "C" const uint8_t *khicas_cjk_glyph(uint32_t cp) {
    return cjkfont::glyph(cp);
}
