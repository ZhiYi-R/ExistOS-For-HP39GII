/**
 * @file Bootloader/Hal/timer_up.h
 * @brief timer_up module
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdbool.h>
#include "interrupt_up.h"

// The timer driver is the (now C++) Timer class (drivers/timer/stmp_timer.hpp).
// The only seam still reached by name is up_TimerSetup, called from the FreeRTOS
// port layer (External/.../port.c xPortStartScheduler) to start the tick timer;
// it keeps C linkage. The tick itself is acknowledged and serviced inline in the
// up_isr dispatcher (Hal/interrupt_up.cpp), so the former portAckTimerIRQ /
// up_TimerTick ack helpers are gone.
#ifdef __cplusplus
extern "C" {
#endif

bool up_TimerSetup(void);

#ifdef __cplusplus
}
#endif

#endif