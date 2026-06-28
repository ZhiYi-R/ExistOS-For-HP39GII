/**
 * @file Bootloader/drivers/stmp_rtc.cpp
 * @brief RTC driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the RTC driver becomes the @c Rtc
 * pure-static singleton. The peripheral is stateless (no file-scope data), so
 * the class is purely the typed home for its three operations; there is nothing
 * to @c friend.
 *
 * The legacy C entry points survive as thin @c extern @c "C" forwarding shims:
 *   - @c portRTC_init — the board bring-up entry;
 *   - @c rtc_get_seconds / @c rtc_set_seconds — the arm_do_swi fast path,
 *     reached by name from the C translation unit vectors.c.
 * They keep C linkage (rtc_up.h declares the whole interface @c extern @c "C").
 * The two SWI seams are defined in-class so they inline into their shims and the
 * emitted ldr/str is bit-for-bit identical to the pre-class path. Migrating the
 * board caller onto @c Rtc:: is deferred to the layer-merge phase.
 */


#include "reg_model.hpp"

#include "rtc_up.h"


class Rtc {
public:
    // init() has a single caller (the portRTC_init shim) and runs once at boot.
    // always_inline folds its body straight into that shim so the named entry is
    // bit-for-bit the pre-class function -- the class is a zero-cost reorg, not a
    // forwarding layer. (Phase 3 migrates the caller onto Rtc::init() directly.)
    [[gnu::always_inline]] static void init();

    // Hot SWI-seam reads/writes: defined in-class (inline) so the extern "C"
    // shims below collapse to the original register access with no extra call.
    static uint32_t getSeconds()
    {
        return reg::RTC_SECONDS::B().COUNT;
    }

    static void setSeconds(uint32_t s)
    {
        while (reg::RTC_STAT::B().NEW_REGS);
        reg::RTC_SECONDS::wr(s);
        while (reg::RTC_STAT::B().NEW_REGS);
    }
};

inline void Rtc::init()
{
    while (reg::RTC_STAT::B().NEW_REGS);

    reg::RTC_PERSISTENT0::B().XTAL32KHZ_PWRUP = 1;

    while (reg::RTC_STAT::B().NEW_REGS);

    reg::RTC_PERSISTENT0::B().CLOCKSOURCE = 1;

    while (reg::RTC_STAT::B().NEW_REGS);

    reg::RTC_PERSISTENT0::B().AUTO_RESTART = 0;

    while (reg::RTC_STAT::B().NEW_REGS);

    reg::RTC_PERSISTENT0::B().DISABLE_PSWITCH = 1;

    while (reg::RTC_STAT::B().NEW_REGS);
}

// ---------------------------------------------------------------------------
// extern "C" seams (rtc_up.h declares the whole RTC interface extern "C").
// Caller migration onto Rtc:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" void portRTC_init()
{
    Rtc::init();
}

extern "C" uint32_t rtc_get_seconds()
{
    return Rtc::getSeconds();
}

extern "C" void rtc_set_seconds(uint32_t s)
{
    Rtc::setSeconds(s);
}
