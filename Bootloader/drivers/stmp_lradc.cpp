/**
 * @file Bootloader/drivers/stmp_lradc.cpp
 * @brief LRADC driver — pure-static @c Lradc singleton.
 *
 * The LRADC driver is the @c Lradc pure-static singleton (declared in
 * stmp_lradc.hpp). Its three operations are ordinary out-of-line static methods,
 * called directly by their C++ consumers as @c Lradc::init / @c Lradc::enable /
 * @c Lradc::convCh.
 *
 * @c port_LRADC_IRQ is dispatched by name from the C interrupt unit and touches
 * no class state, so it stays a plain @c extern @c "C" free function holding its
 * own logic.
 */


#include "stdint.h"
#include "stdbool.h"

#include "hw_irq.h"
#include "reg_model.hpp"
#include "reg_values.hpp"
#include "debug.h"

#include "interrupt_up.h"

#include "stmp_lradc.hpp"


void Lradc::init()
{

    reg::LRADC_CONVERSION::B().AUTOMATIC = 1;

    INFO("LRADC_STATUS:%08x\n", reg::LRADC_STATUS::rd());


    reg::LRADC_CTRL4::B().LRADC5SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL15; //VDD5V

    reg::LRADC_CTRL2::B().TEMPSENSE_PWD = 0;

    reg::LRADC_CTRL4::B().LRADC4SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL9;
    reg::LRADC_CTRL4::B().LRADC3SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL8;
    reg::LRADC_CTRL4::B().LRADC2SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL14;

}

void Lradc::enable(bool enable, uint32_t ch)
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

uint32_t Lradc::convCh(uint32_t ch, uint32_t samples)
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

// ---------------------------------------------------------------------------
// port_LRADC_IRQ is dispatched by name from interrupt_up.c (stays C); it touches
// no Lradc state, so it stays a free extern "C" function holding its logic.
// ---------------------------------------------------------------------------
extern "C" void port_LRADC_IRQ(uint32_t ch)
{

    INFO("\n\nLRADC IRQ:%u, val:%d\n", ch, reg::LRADC_CHn::B(ch).VALUE);
    reg::LRADC_CTRL1::clr(1 << ch);
}
