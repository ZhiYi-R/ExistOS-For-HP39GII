/**
 * @file Bootloader/drivers/stmp_uartdbg.cpp
 * @brief UART debug driver
 *
 * Migrated to the typed register model: BF_RD/BF_WR become reg::UARTDBG*::B()
 * field access. UARTDBGDR has no atomic SET alias, so the DATA write is a
 * bitfield store (RMW on the byte) -- identical to the old BW_ helper.
 */

#include "reg_model.hpp"

#include "uart_up.h"


unsigned int is_uartdbg_busy(void){
	return reg::UARTDBGFR::B().BUSY;
}

void uartdbg_putc(unsigned char c)
{
	while(is_uartdbg_busy());
	reg::UARTDBGDR::B().DATA = c;
}



void uart_putc(unsigned char c)
{
	uartdbg_putc(c);
}
