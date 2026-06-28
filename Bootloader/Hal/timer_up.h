/**
 * @file Bootloader/Hal/timer_up.h
 * @brief timer_up module
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdbool.h>
#include "interrupt_up.h"

// The timer driver is the (now C++) Timer class (drivers/timer/stmp_timer.hpp);
// this header keeps C linkage only for the seams still reached by name:
// up_TimerSetup (called from the FreeRTOS port layer under External/) and
// portAckTimerIRQ (the up_isr ack dispatch, to be wired by the interrupt_up
// refactor). up_TimerTick is the FreeRTOS tick service.
#ifdef __cplusplus
extern "C" {
#endif

void portAckTimerIRQ(void);

bool up_TimerSetup(void);
void up_TimerTick(void);

#ifdef __cplusplus
}
#endif

#endif