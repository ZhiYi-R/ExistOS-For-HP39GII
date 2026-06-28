/**
 * @file Bootloader/drivers/stmp_rtc.cpp
 * @brief RTC driver — pure-static @c Rtc singleton (definitions).
 *
 * The @c Rtc class is declared in stmp_rtc.hpp; this file carries the method
 * definitions and the two @c extern @c "C" SWI seams. The peripheral is
 * stateless (no file-scope data), so there is nothing to @c friend.
 *
 * @c init() is an ordinary out-of-line static method, called directly by
 * @c boardInit (the rtc_init / portRTC_init thin wrappers are gone). The
 * @c getSeconds / @c setSeconds accessors are defined @c inline so the kept
 * @c rtc_get_seconds / @c rtc_set_seconds shims (reached by name from vectors.c's
 * arm_do_swi fast path) fold to the bare register access bit-for-bit.
 */

#include "stmp_rtc.hpp"

#include "reg_model.hpp"


inline uint32_t Rtc::getSeconds()
{
    return reg::RTC_SECONDS::B().COUNT;
}

inline void Rtc::setSeconds(uint32_t s)
{
    while (reg::RTC_STAT::B().NEW_REGS);
    reg::RTC_SECONDS::wr(s);
    while (reg::RTC_STAT::B().NEW_REGS);
}

void Rtc::init()
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
// extern "C" SWI seams (declared in stmp_rtc.hpp; reached by name from the C
// translation unit vectors.c's arm_do_swi fast path). start.cpp reaches the
// counter read through rtc_get_seconds too. The inline getSeconds/setSeconds
// above fold into these so the emitted ldr/str is the pre-class path.
// ---------------------------------------------------------------------------
extern "C" uint32_t rtc_get_seconds()
{
    return Rtc::getSeconds();
}

extern "C" void rtc_set_seconds(uint32_t s)
{
    Rtc::setSeconds(s);
}
