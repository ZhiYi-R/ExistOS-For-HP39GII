/**
 * @file Bootloader/drivers/stmp_lradc.cpp
 * @brief LRADC driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the LRADC driver becomes the @c Lradc
 * pure-static singleton. The peripheral is stateless, so the class is the typed
 * home for its three operations; @c port_LRADC_IRQ touches no class state and so
 * stays a plain @c extern @c "C" free function (it is dispatched by name from the
 * C interrupt unit and holds its own logic).
 *
 * The legacy @c portLRADC* entry points survive as thin @c extern @c "C"
 * forwarding shims (stmp_lradc.hpp declares them @c extern @c "C"); the methods are
 * always_inline so each shim folds back to the pre-class body bit-for-bit.
 * Caller migration onto @c Lradc:: is deferred to the layer-merge phase.
 */


#include "stdint.h"
#include "stdbool.h"

#include "hw_irq.h"
#include "reg_model.hpp"
#include "reg_values.hpp"
#include "debug.h"

#include "interrupt_up.h"

#include "stmp_lradc.hpp"


class Lradc {
public:
    // Each method has a single caller (its extern "C" shim); always_inline folds
    // the body straight into that named entry so it is bit-for-bit the pre-class
    // function. Phase 3 migrates callers onto Lradc:: directly.
    [[gnu::always_inline]] static void init();
    [[gnu::always_inline]] static void enable(bool enable, uint32_t ch);
    [[gnu::always_inline]] static uint32_t convCh(uint32_t ch, uint32_t samples);
};

inline void Lradc::init()
{

    reg::LRADC_CONVERSION::B().AUTOMATIC = 1;

    INFO("LRADC_STATUS:%08x\n", reg::LRADC_STATUS::rd());


    reg::LRADC_CTRL4::B().LRADC5SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL15; //VDD5V

    reg::LRADC_CTRL2::B().TEMPSENSE_PWD = 0;

    reg::LRADC_CTRL4::B().LRADC4SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL9;
    reg::LRADC_CTRL4::B().LRADC3SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL8;
    reg::LRADC_CTRL4::B().LRADC2SELECT = reg::LRADC_CTRL4_sym::LRADC7SELECT__CHANNEL14;

}

inline void Lradc::enable(bool enable, uint32_t ch)
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

inline uint32_t Lradc::convCh(uint32_t ch, uint32_t samples)
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
// extern "C" seams. The portLRADC* entries (stmp_lradc.hpp) forward to the class.
// port_LRADC_IRQ is dispatched by name from interrupt_up.c (stays C); it touches
// no Lradc state, so it stays a free function holding its logic directly.
// ---------------------------------------------------------------------------
extern "C" void portLRADC_init()
{
    Lradc::init();
}

extern "C" void portLRADCEnable(bool enable, uint32_t ch)
{
    Lradc::enable(enable, ch);
}

extern "C" uint32_t portLRADCConvCh(uint32_t ch, uint32_t samples)
{
    return Lradc::convCh(ch, samples);
}

extern "C" void port_LRADC_IRQ(uint32_t ch)
{

    INFO("\n\nLRADC IRQ:%u, val:%d\n", ch, reg::LRADC_CHn::B(ch).VALUE);
    reg::LRADC_CTRL1::clr(1 << ch);
}
