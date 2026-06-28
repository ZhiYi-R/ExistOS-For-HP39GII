/**
 * @file Bootloader/drivers/stmp_power.hpp
 * @brief Power / charger driver — pure-static @c Power singleton (see stmp_power.cpp).
 *
 * Split out of board_up.h in Phase 3. The class is the typed entry point called
 * directly by its C++ consumers (start.cpp / llapi.cpp / stmp_board.cpp). The
 * power IRQ @c portPowerIRQ stays a free @c extern @c "C" function (dispatched by
 * name from the C interrupt unit) and @c g_chargeEnable stays a global data seam;
 * neither is a class member, so neither is declared here.
 */
#pragma once

class Power {
public:
    static void init();
    static void chargeEnable(bool enable);
    static void powerOff();
};
