/**
 * @file Bootloader/drivers/stmp_rtc.cpp
 * @brief RTC driver
 *
 * Pilot for the typed C++23 register model (reg_model.hpp): the legacy
 * HW_RTC_*.B / HW_RTC_SECONDS_WR macros are replaced by reg::RTC_*::B() /
 * reg::RTC_SECONDS::wr(). Field access goes through the same hw_rtc_*_t unions,
 * so the emitted ldr/str sequence is bit-for-bit identical to the macro path.
 */


#include "reg_model.hpp"

#include "rtc_up.h"


void portRTC_init()
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

uint32_t rtc_get_seconds()
{
    return reg::RTC_SECONDS::B().COUNT;
}

void rtc_set_seconds(uint32_t s)
{
    while (reg::RTC_STAT::B().NEW_REGS);
    reg::RTC_SECONDS::wr(s);
    while (reg::RTC_STAT::B().NEW_REGS);
}
