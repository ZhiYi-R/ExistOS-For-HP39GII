/**
 * @file Bootloader/Hal/rtc_up.h
 * @brief rtc_up module
 */

#pragma once
#include <stdint.h>

// rtc_get_seconds/rtc_set_seconds are reached by name from the C translation
// units that stay C (vectors.c's arm_do_swi fast path, and start.c until its
// phase lands), so the whole RTC HAL interface keeps C linkage.
#ifdef __cplusplus
extern "C" {
#endif

void rtc_init();
void portRTC_init();


uint32_t rtc_get_seconds();

void rtc_set_seconds(uint32_t s);

#ifdef __cplusplus
}
#endif



