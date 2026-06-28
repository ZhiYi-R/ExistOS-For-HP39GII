/**
 * @file Bootloader/drivers/timer/stmp_timer.cpp
 * @brief Timer driver
 *
 * Migrated to the typed register model. TIMROT_ROTCTRL / TIMROT_TIMCTRLn carry
 * atomic SET/CLR aliases, so BF_SET/CLR and BF_CS1n become reg::*::set/clr.
 * TIMROT_TIMCOUNTn has no atomic alias (its legacy SET/CLR are software RMW on
 * .U), so its field writes are bitfield stores / read-modify-write -- identical
 * to the BW_/BF_CS1n expansions they replace.
 */



#include "hw_irq.h"
#include "timer_up.h"
#include <stdint.h>
#include "interrupt_up.h"

#include "reg_model.hpp"
#include "reg_values.hpp"

#include "debug.h"

static uint32_t timer0ReloadVal;
static uint32_t timer1ReloadVal;

void portTimerInit(void)
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



void portAckTimerIRQ(void)
{
    reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::IRQ::mask);
    up_TimerTick();
}

bool portEnableTimerIRQ(int timer, bool enable)
{

        reg::TIMROT_TIMCTRLn::clr(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::mask);
        reg::TIMROT_TIMCTRLn::set(0, reg::TIMROT_TIMCTRLn_::IRQ_EN::val(enable));
        portEnableIRQ(HW_IRQ_TIMER0, (unsigned int)enable);
        return true;
}

bool portEnableTimer(int timer, bool enable)
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

bool portSetTimerPeriod(int timer, unsigned int us)
{


        timer0ReloadVal = us/32;
        return true;
        

}

int portGetTimerNum(void){
    return 1;
}


int portGetTimer(void){
    return HW_IRQ_TIMER0;
}