/**
 * @file Bootloader/drivers/input/stmp_gpio.hpp
 * @brief GPIO keyboard-matrix driver — pure-static @c Keyboard singleton (see stmp_gpio.cpp).
 *
 * Split out of the driver .cpp in Phase 3. The class is the typed entry point
 * called directly by its C++ consumers (start.cpp / board_up.cpp / keyboard_up.cpp).
 * The keyboard is polled (scanned from @c key_task), so it owns no ISR and there
 * is no @c extern @c "C" seam to preserve. @c Keys_t comes from keyboard_up.h,
 * which stays a plain cross-language enum shared by value.
 */
#pragma once

#include "keyboard_up.h"   // Keys_t

class Keyboard {
public:
    static void gpioInit();
    static void scan();
    static bool isKeyDown(Keys_t key);
    static Keys_t getChangedKey();

private:
    // key_matrix and key_matrix_last were two adjacent file-scope .bss arrays
    // that -Os addressed off one shared base; grouping them into a single
    // aggregate (zero-init -> .bss, no global ctor) keeps that shared-base
    // codegen, where two independent static-inline members would not.
    struct State {
        uint8_t key_matrix[5][11];
        uint8_t key_matrix_last[5][11];
    };
    static inline State st = {};
    static inline Keys_t ChangedKey = (Keys_t)255;

    // Single-call-site (loop-body) helpers that -Os folded into the scan path;
    // always_inline preserves that folding. Defined in stmp_gpio.cpp and used
    // only there, so they emit no out-of-line body.
    [[gnu::always_inline]] static void set_row_line(int row_line);
    [[gnu::always_inline]] static unsigned int read_col_line(int col_line);
};
