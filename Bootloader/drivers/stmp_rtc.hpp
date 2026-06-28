/**
 * @file Bootloader/drivers/stmp_rtc.hpp
 * @brief RTC driver — pure-static @c Rtc singleton + C SWI seams.
 *
 * Phase 3 of the HAL C++23 migration. The @c Rtc pure-static singleton is the
 * RTC driver: bring-up (@c init) and the persistent seconds counter read/write.
 * The peripheral is stateless (no file-scope data), so there is nothing to
 * @c friend. Method bodies live out-of-line in stmp_rtc.cpp, so this header
 * pulls no reg_model.hpp and stays clear of the reg_types VERSION bitfield's
 * collision with SystemConfig.h's @c VERSION macro.
 *
 * @c rtc_get_seconds / @c rtc_set_seconds stay @c extern @c "C" seams, declared
 * below the class so the C translation unit that includes this header
 * (vectors.c's arm_do_swi fast path) resolves them unmangled; start.cpp reaches
 * the counter read through the same seam. The class is guarded by
 * @c __cplusplus, so that C unit simply skips it. Their .cpp defs are the
 * in-class @c getSeconds / @c setSeconds, defined @c inline so the shims fold to
 * the bare register access bit-for-bit. @c init is an ordinary out-of-line
 * static method, called directly by @c boardInit (the rtc_init / portRTC_init
 * thin wrappers are gone).
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus

// RTC pure-static singleton (method definitions in stmp_rtc.cpp).
class Rtc {
public:
    // Bring-up entry; out-of-line static method, called directly by boardInit.
    static void init();

    // Persistent seconds counter. Reached only through the kept extern "C" shims
    // (same TU), so their .cpp defs are `inline` and fold into those shims
    // bit-for-bit; defs stay in the .cpp because they touch reg_model (kept out
    // of this header to dodge the VERSION collision).
    static uint32_t getSeconds();
    static void     setSeconds(uint32_t s);
};

extern "C" {
#endif

uint32_t rtc_get_seconds();
void     rtc_set_seconds(uint32_t s);

#ifdef __cplusplus
}
#endif
