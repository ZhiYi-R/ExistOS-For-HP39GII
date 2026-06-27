/**
 * @file Bootloader/Hal/clkctrl_up.h
 * @brief clkctrl_up module
 */

#ifndef __CLKCTRL_UP_H__
#define __CLKCTRL_UP_H__

#include <stdint.h>
#include <stdbool.h>

// clkctrl HAL is implemented in the (now C++) clock driver but called by name
// from board_up.c / start.c, so the interface keeps C linkage.
#ifdef __cplusplus
extern "C" {
#endif

void portCLKCtrlInit(void);


void CLKCtrlInit(void);

#ifdef __cplusplus
}
#endif

#endif