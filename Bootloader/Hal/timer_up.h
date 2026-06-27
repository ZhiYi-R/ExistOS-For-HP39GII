/**
 * @file Bootloader/Hal/timer_up.h
 * @brief timer_up module
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdbool.h>
#include "interrupt_up.h"

// Timer HAL is implemented in the (now C++) timer driver but called by name from
// board_up.c and the C ISR dispatch, so the interface keeps C linkage.
#ifdef __cplusplus
extern "C" {
#endif

int portGetTimerNum(void);
bool portSetTimerPeriod(int timer, unsigned int us);
bool portEnableTimerIRQ(int timer, bool enable);
bool portEnableTimer(int timer, bool enable);
void portTimerInit(void);

void portAckTimerIRQ(void);
int portGetTimer(void);

bool up_TimerSetup(void);
void up_TimerIRQ(IRQNumber IRQNum);
void up_TimerTick(void);

#ifdef __cplusplus
}
#endif

#endif