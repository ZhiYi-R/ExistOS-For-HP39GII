/**
 * @file Bootloader/drivers/stmp_uartdbg.hpp
 * @brief UART debug driver — pure-static @c Uart singleton + C write seam.
 *
 * Phase 3 of the HAL C++23 migration. The @c Uart pure-static singleton is the
 * debug UART driver: a no-op bring-up entry (@c init -- the controller is
 * configured by the boot ROM, so software brings up nothing) and the busy-poll
 * byte write. The peripheral is stateless, so there is nothing to @c friend.
 * Method bodies live out-of-line in stmp_uartdbg.cpp, so this header pulls no
 * reg_model.hpp and stays clear of the reg_types VERSION bitfield's collision
 * with SystemConfig.h's @c VERSION macro.
 *
 * @c uart_putc stays an @c extern @c "C" seam, declared below the class so the C
 * translation unit that includes this header (stub.c's newlib write stub)
 * resolves it unmangled; the class is guarded by @c __cplusplus, so that C unit
 * simply skips it. Its .cpp def is the in-class @c putc, defined @c inline so the
 * shim folds to the bare busy-poll + store bit-for-bit. @c init is an ordinary
 * out-of-line static method, called directly by @c boardInit (the uartInit thin
 * wrapper is gone).
 */
#pragma once

#ifdef __cplusplus

// Debug-UART pure-static singleton (method definitions in stmp_uartdbg.cpp).
class Uart {
public:
    // No-op bring-up entry: the debug UART is configured by the boot ROM, so
    // there is nothing to do here. Out-of-line static method, called directly by
    // boardInit.
    static void init();

    // Busy-poll byte write. Reached only through the kept extern "C" uart_putc
    // shim (same TU), so its .cpp def is `inline` and folds into that shim
    // bit-for-bit; the def stays in the .cpp because it touches reg_model (kept
    // out of this header to dodge the VERSION collision).
    static void putc(unsigned char c);

private:
    static bool busy();
};

extern "C" {
#endif

void uart_putc(unsigned char c);

#ifdef __cplusplus
}
#endif
