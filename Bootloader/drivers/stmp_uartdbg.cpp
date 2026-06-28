/**
 * @file Bootloader/drivers/stmp_uartdbg.cpp
 * @brief UART debug driver — pure-static @c Uart singleton (definitions).
 *
 * The @c Uart class is declared in stmp_uartdbg.hpp; this file carries the
 * method definitions and the @c extern @c "C" uart_putc seam. The peripheral is
 * stateless, so there is nothing to @c friend.
 *
 * @c init() is a no-op out-of-line static method (the debug UART is configured by
 * the boot ROM), called directly by @c boardInit (the uartInit thin wrapper is
 * gone). @c putc / @c busy are defined @c inline so the kept @c uart_putc shim
 * (reached by name from stub.c's newlib write stub) folds to the bare busy-poll +
 * store bit-for-bit.
 */

#include "stmp_uartdbg.hpp"

#include "reg_model.hpp"


inline bool Uart::busy()
{
    return reg::UARTDBGFR::B().BUSY;
}

inline void Uart::putc(unsigned char c)
{
    while (busy());
    reg::UARTDBGDR::B().DATA = c;
}

void Uart::init()
{
}

// ---------------------------------------------------------------------------
// extern "C" write seam (declared in stmp_uartdbg.hpp; reached by name from the
// C newlib write stub in stub.c). The inline putc above folds into it so the
// emitted busy-poll + store is the pre-class path.
// ---------------------------------------------------------------------------
extern "C" void uart_putc(unsigned char c)
{
    Uart::putc(c);
}
