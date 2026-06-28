/**
 * @file Bootloader/drivers/timer/stmp_timer.hpp
 * @brief Timer (TIMROT) driver — pure-static @c Timer singleton.
 *
 * Phase 3 of the HAL C++23 migration. The @c Timer pure-static singleton is the
 * timer-rotary (TIMROT) driver: bring-up plus per-timer IRQ-enable / count
 * control, with the reload-value state held as @c private @c static @c inline
 * members. Method bodies live out-of-line in stmp_timer.cpp, so this header pulls
 * no reg_model.hpp and stays clear of the reg_types VERSION bitfield's collision
 * with SystemConfig.h's @c VERSION macro.
 *
 * @c init / @c getTimer / @c setPeriod / @c enableIRQ / @c enable are ordinary
 * out-of-line static methods, called directly by @c up_TimerSetup (the FreeRTOS
 * tick-timer start in timer_up.cpp) -- the @c portTimer* forwarding shells are gone.
 *
 * The TIMER0 tick IRQ is acknowledged and serviced inline in the up_isr dispatcher
 * (Hal/interrupt_up.cpp), so this class carries no ackIRQ method: routing the hot
 * ISR path through one would cost an extra @c bl (not bit-identical) and would drag
 * reg_model.hpp into this header. The former portAckTimerIRQ / up_TimerTick ack
 * chain was therefore removed rather than wired.
 */
#pragma once

#include <stdint.h>

// Timer-rotary (TIMROT) pure-static singleton (method definitions in stmp_timer.cpp).
class Timer {
public:
    // Bring-up + per-timer control. Migrated onto Timer:: by up_TimerSetup, so
    // these are ordinary out-of-line static methods (the portTimer* shims that
    // used to wrap them are gone).
    static void init();
    static bool enableIRQ(int timer, bool enable);
    static bool enable(int timer, bool enable);
    static bool setPeriod(int timer, unsigned int us);
    static int  getTimer();

private:
    static inline uint32_t timer0ReloadVal = 0;
    static inline uint32_t timer1ReloadVal = 0;
};
