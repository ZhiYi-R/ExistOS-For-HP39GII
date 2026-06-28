/**
 * @file Bootloader/drivers/stmp_lradc.cpp
 * @brief LRADC driver
 *
 * Migrated to the typed register model. LRADC_CTRL1 and the multi-instance
 * LRADC_CHn carry MMIO atomic SET/CLR aliases -> reg::*::set/clr (and the
 * channel-indexed set(n,..)/clr(n,..)). BW_LRADC_CTRL0_SCHEDULE is a plain
 * bitfield write, so BF_WR(LRADC_CTRL0,SCHEDULE,v) maps to reg::*::B().SCHEDULE.
 */


#include "stdint.h"
#include "stdbool.h"

#include "hw_irq.h"
#include "reg_model.hpp"
#include "reg_values.hpp"
#include "debug.h"

#include "interrupt_up.h"

#include "board_up.h"



void portLRADC_init()
{

    reg::LRADC_CONVERSION::B().AUTOMATIC = 1;

    INFO("LRADC_STATUS:%08x\n", reg::LRADC_STATUS::rd());


    reg::LRADC_CTRL4::B().LRADC5SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL15; //VDD5V

    reg::LRADC_CTRL2::B().TEMPSENSE_PWD = 0;

    reg::LRADC_CTRL4::B().LRADC4SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL9;
    reg::LRADC_CTRL4::B().LRADC3SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL8;
    reg::LRADC_CTRL4::B().LRADC2SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL14;

}

void portLRADCEnable(bool enable ,uint32_t ch)
{
    INFO("Enable LRADC:%u,%d\n",ch, enable);

    portEnableIRQ(HW_IRQ_LRADC_CH0 + ch, (unsigned int)enable);

    if(enable)
    {
        reg::LRADC_CTRL1::set( ((1) << 16) << ch);
    }else{
        reg::LRADC_CTRL1::clr( ((1) << 16) << ch);
    }

}

uint32_t portLRADCConvCh(uint32_t ch, uint32_t samples)
{
    if(!samples)
    {
        return 1;
    }
    uint32_t n = samples;
    uint32_t acc_val = 0;

    do{
        reg::LRADC_CHn::clr(ch, reg::LRADC_CHn_::VALUE::mask);
        reg::LRADC_CHn::clr(ch, reg::LRADC_CHn_::TOGGLE::mask);
        reg::LRADC_CHn::clr(ch, reg::LRADC_CHn_::ACCUMULATE::mask);
        reg::LRADC_CHn::set(ch, reg::LRADC_CHn_::ACCUMULATE::val(1));
        reg::LRADC_CHn::clr(ch, reg::LRADC_CHn_::NUM_SAMPLES::mask);
        reg::LRADC_CHn::set(ch, reg::LRADC_CHn_::NUM_SAMPLES::val(1));
        reg::LRADC_CTRL0::B().SCHEDULE = (1) << ch;
        while(reg::LRADC_CHn::B(ch).TOGGLE == 0);
        acc_val += reg::LRADC_CHn::B(ch).VALUE;
        n--;
    }while(n > 0);

    return acc_val / samples;

}

// Dispatched by name from interrupt_up.c (stays C); keep C linkage.
extern "C" void port_LRADC_IRQ(uint32_t ch)
{

    INFO("\n\nLRADC IRQ:%u, val:%d\n", ch, reg::LRADC_CHn::B(ch).VALUE);
    reg::LRADC_CTRL1::clr(1 << ch);
}
