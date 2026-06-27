/**
 * @file Bootloader/Hal/uart_up.h
 * @brief uart_up module
 */

#ifndef __UART_UP_H__
#define __UART_UP_H__





// uart_putc is implemented in the (now C++) UART driver but called by name from
// the newlib write stub in stub.c, which stays C; keep C linkage.
#ifdef __cplusplus
extern "C" {
#endif

void uart_putc(unsigned char c);
void uartInit(void);

#ifdef __cplusplus
}
#endif

#endif