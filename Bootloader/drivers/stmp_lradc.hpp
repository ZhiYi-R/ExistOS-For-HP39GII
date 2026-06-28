/**
 * @file Bootloader/drivers/stmp_lradc.hpp
 * @brief LRADC driver — pure-static @c Lradc singleton (see stmp_lradc.cpp).
 *
 * Split out of board_up.h in Phase 3. The class is the typed entry point called
 * directly by its C++ consumers (start.cpp / stmp_board.cpp). The LRADC IRQ
 * @c port_LRADC_IRQ stays a free @c extern @c "C" function (dispatched by name
 * from the C interrupt unit) and is not a class member, so it is not declared here.
 */
#pragma once

#include <cstdint>

class Lradc {
public:
    static void init();
    static void enable(bool enable, uint32_t ch);
    static uint32_t convCh(uint32_t ch, uint32_t samples);
};
