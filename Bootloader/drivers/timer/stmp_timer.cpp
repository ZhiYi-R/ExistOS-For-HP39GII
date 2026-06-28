/**
 * @file Bootloader/drivers/timer/stmp_timer.cpp
 * @brief Timer driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the timer driver becomes the @c Timer
 * pure-static singleton. Its reload-value state moves into the class as
 * @c private @c static @c inline members (file-local before, no cross-TU use).
 *
 * Migrated to the typed register model. TIMROT_ROTCTRL / TIMROT_TIMCTRLn carry
 * atomic SET/CLR aliases, so BF_SET/CLR and BF_CS1n become reg::*::set/clr.
 * TIMROT_TIMCOUNTn has no atomic alias (its legacy SET/CLR are software RMW on
 * .U), so its field writes are bitfield stores / read-modify-write -- identical
 * to the BW_/BF_CS1n expansions they replace.
 *
 * The legacy @c portTimer* entries survive as thin @c extern @c "C" forwarding
 * shims (timer_up.h declares the interface @c extern @c "C"; they are reached by
 * name from board_up.c, the C ISR dispatch and the timer_up HAL wrapper). Each
 * entry has a single caller (its shim), so always_inline folds the body back into
 * the named entry bit-for-bit. Caller migration onto @c Timer:: is deferred to
 * the layer-merge phase.
 */



#include "hw_irq.h"
#include "timer_up.h"
#include <stdint.h>
#include "interrupt_up.h"

#include "reg_model.hpp"
#include "reg_values.hpp"

#include "debug.h"

class Timer {
public:
    // Each entry has a single caller (its extern "C" shim); always_inline folds
    // the body straight into the named entry so it is bit-for-bit the pre-class
    // function. Phase 3 migrates callers onto Timer:: directly.
    [[gnu::always_inline]] static void init();
    [[gnu::always_inline]] static void ackIRQ();
    [[gnu::always_inline]] static bool enableIRQ(int timer, bool enable);
    [[gnu::always_inline]] static bool enable(int timer, bool enable);
    [[gnu::always_inline]] static bool setPeriod(int timer, unsigned int us);
    [[gnu::always_inline]] static int getNum();
    [[gnu::always_inline]] static int getTimer();

private:
    static inline uint32_t timer0ReloadVal = 0;
    static inline uint32_t timer1ReloadVal = 0;
};

inline void Timer::init()
{

    reg::TIMROT_ROTCTRL::clr(reg::TIMROT_ROTCTRL_::SFTRST::mask);
    reg::TIMROT_ROTCTRL::clr(reg::TIMROT_ROTCTRL_::CLKGATE::mask);

    reg::TIMROT_ROTCTRL::set(reg::TIMROT_ROTCTRL_::SFTRST::mask);
    while(reg::TIMROT_ROTCTRL::B().CLKGATE == 0)
        ;

    reg::TIMROT_ROTCTRL::clr(reg::TIMROT_ROTCTRL_::SFTRST::mask);
    reg::TIMROT_ROTCTRL::clr(reg::TIMROT_ROTCTRL_::CLKGATE::mask);


    reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::SELECT::mask);
    reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::SELECT::val(reg::TIMROT_TIMCTRLn_sym::SELECT__32KHZ_XTAL));
    reg::TIMROT_TIMCTRLn::clr(1, reg::TIMROT_TIMCTRLn_::SELECT::mask);
    reg::TIMROT_TIMCTRLn::set(1, reg::TIMROT_TIMCTRLn_::SELECT::val(reg::TIMROT_TIMCTRLn_sym::SELECT__32KHZ_XTAL));


}



inline void Timer::ackIRQ()
{
    reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::IRQ::mask);
    up_TimerTick();
}

inline bool Timer::enableIRQ(int timer, bool enable)
{

        reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::mask);
        reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::val(enable));
        portEnableIRQ(HW_IRQ_TIMER0, (unsigned int)enable);
        return true;
}

inline bool Timer::enable(int timer, bool enable)
{

        if(enable){
            reg::TIMROT_TIMCOUNTn::B(0).FIXED_COUNT = timer0ReloadVal;
        }else{
            reg::TIMROT_TIMCOUNTn::wr(0, reg::TIMROT_TIMCOUNTn::rd(0) & ~reg::TIMROT_TIMCOUNTn_::FIXED_COUNT::mask);
            reg::TIMROT_TIMCOUNTn::wr(0, reg::TIMROT_TIMCOUNTn::rd(0) | reg::TIMROT_TIMCOUNTn_::FIXED_COUNT::val(0));
        }
        reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::RELOAD::mask);
        reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::RELOAD::val(enable));
        reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::UPDATE::mask);
        reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::UPDATE::val(enable));
        return true;
}

inline bool Timer::setPeriod(int timer, unsigned int us)
{


        timer0ReloadVal = us/32;
        return true;


}

inline int Timer::getNum(){
    return 1;
}


inline int Timer::getTimer(){
    return HW_IRQ_TIMER0;
}

// ---------------------------------------------------------------------------
// extern "C" seams (timer_up.h declares the interface extern "C").
// Caller migration onto Timer:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" void portTimerInit(void)                       { Timer::init(); }
extern "C" void portAckTimerIRQ(void)                     { Timer::ackIRQ(); }
extern "C" bool portEnableTimerIRQ(int timer, bool enable){ return Timer::enableIRQ(timer, enable); }
extern "C" bool portEnableTimer(int timer, bool enable)   { return Timer::enable(timer, enable); }
extern "C" bool portSetTimerPeriod(int timer, unsigned int us) { return Timer::setPeriod(timer, us); }
extern "C" int portGetTimerNum(void)                      { return Timer::getNum(); }
extern "C" int portGetTimer(void)                         { return Timer::getTimer(); }
