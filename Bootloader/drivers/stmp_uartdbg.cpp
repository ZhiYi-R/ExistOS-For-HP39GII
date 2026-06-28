/**
 * @file Bootloader/drivers/stmp_uartdbg.cpp
 * @brief UART debug driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the debug UART becomes the @c Uart
 * pure-static singleton. The peripheral is stateless, so the class is just the
 * typed home for the busy-poll and the byte write; there is nothing to @c friend.
 *
 * @c uart_putc is the hard seam: the newlib write stub in stub.c (which stays C)
 * calls it by name, so it keeps C linkage. @c uartdbg_putc survives as a
 * forwarding shim for the legacy stmp_uartdbg.h declaration (no live caller, so
 * --gc-sections drops it). Both are defined in-class/inline so the emitted poll
 * + store is bit-for-bit identical to the pre-class path.
 */

#include "reg_model.hpp"

#include "uart_up.h"

#include "stmp_uartdbg.h"


class Uart {
public:
    static void putc(unsigned char c)
    {
        while (busy());
        reg::UARTDBGDR::B().DATA = c;
    }

private:
    static bool busy()
    {
        return reg::UARTDBGFR::B().BUSY;
    }
};

// ---------------------------------------------------------------------------
// Seams. uart_putc keeps C linkage (uart_up.h; called from the C newlib stub).
// uartdbg_putc forwards for its legacy header declaration. Caller migration onto
// Uart:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" void uart_putc(unsigned char c)
{
    Uart::putc(c);
}

void uartdbg_putc(unsigned char c)
{
    Uart::putc(c);
}
