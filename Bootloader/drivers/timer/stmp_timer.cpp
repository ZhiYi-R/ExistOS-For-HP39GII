/**
 * @file Bootloader/drivers/timer/stmp_timer.cpp
 * @brief Timer (TIMROT) driver — pure-static @c Timer singleton (definitions).
 *
 * The @c Timer class is declared in stmp_timer.hpp; this file carries the method
 * definitions, the kept @c extern @c "C" portAckTimerIRQ seam, and the
 * reload-value state.
 *
 * Uses the typed register model. TIMROT_ROTCTRL / TIMROT_TIMCTRLn carry atomic
 * SET/CLR aliases, so BF_SET/CLR and BF_CS1n become reg::*::set/clr.
 * TIMROT_TIMCOUNTn has no atomic alias (its legacy SET/CLR are software RMW on
 * .U), so its field writes are bitfield stores / read-modify-write -- identical
 * to the BW_/BF_CS1n expansions they replace.
 *
 * @c init / @c getTimer / @c setPeriod / @c enableIRQ / @c enable are ordinary
 * out-of-line static methods, called directly by @c up_TimerSetup; their old
 * portTimer* forwarding shims (and the dead portGetTimerNum / Timer::getNum) are
 * gone. @c ackIRQ stays @c inline and folds into the kept @c portAckTimerIRQ
 * shim, whose only (commented-out) caller lives in the up_isr dispatcher -- that
 * ack wiring is the interrupt_up service-layer refactor's call.
 */



#include "stmp_timer.hpp"

#include "hw_irq.h"
#include "timer_up.h"
#include <stdint.h>
#include "interrupt_up.h"

#include "reg_model.hpp"
#include "reg_values.hpp"

#include "debug.h"

void Timer::init()
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

bool Timer::enableIRQ(int timer, bool enable)
{

        reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::mask);
        reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::val(enable));
        portEnableIRQ(HW_IRQ_TIMER0, (unsigned int)enable);
        return true;
}

bool Timer::enable(int timer, bool enable)
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

bool Timer::setPeriod(int timer, unsigned int us)
{


        timer0ReloadVal = us/32;
        return true;


}

int Timer::getTimer(){
    return HW_IRQ_TIMER0;
}

// ---------------------------------------------------------------------------
// extern "C" ack seam (declared in timer_up.h). Its only caller is the
// commented-out timer case in the up_isr dispatcher; kept so the interrupt_up
// service-layer refactor can re-point that dispatch here. The inline ackIRQ
// above folds into it, so the emitted clear-IRQ + tick is the pre-class path.
// ---------------------------------------------------------------------------
extern "C" void portAckTimerIRQ(void) { Timer::ackIRQ(); }
