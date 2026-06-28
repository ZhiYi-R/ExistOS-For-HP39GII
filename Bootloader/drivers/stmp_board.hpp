/**
 * @file Bootloader/drivers/stmp_board.hpp
 * @brief Board bring-up / timing / identity — pure-static @c Board singleton + C seams.
 *
 * Phase 3 of the HAL C++23 migration. The @c Board pure-static singleton is the
 * board driver: bring-up (@c init), timing reads, chip reset, and battery /
 * identity accessors. Its method bodies live out-of-line in stmp_board.cpp, so
 * this header pulls no reg_model.hpp and stays clear of the reg_types VERSION
 * bitfield's collision with SystemConfig.h's @c VERSION macro. C++ consumers call
 * @c Board::getTime_s / @c getBatteryVoltage_mv / @c getBatteryMode / @c getPWRSpeed
 * directly.
 *
 * A few entries stay @c extern @c "C" seams, declared below the class so the C
 * translation units that include this header (vectors.c: portBoardGetTime_us/ms;
 * stub.c: portBoardReset) resolve them unmangled. The class is guarded by
 * @c __cplusplus, so those C units simply skip it. @c nsToCycles / @c portDelay*
 * are stateless leaf primitives used by name from lcdif/gpmi; @c driverWait* /
 * @c boardInit / @c portBoardInit are the bring-up orchestration seams pending
 * their own later-phase merge into the class.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"   // TickType_t for driverWait*

#ifdef __cplusplus

// Board pure-static singleton (method definitions in stmp_board.cpp).
class Board {
public:
    // Bring-up entry; always_inline, folded into the portBoardInit seam (its
    // thin-wrapper merge with boardInit is deferred to a later phase).
    [[gnu::always_inline]] static void init();

    // getPWRSpeed owns a function-local `static last_val`. Staying non-inline
    // keeps that static internal, so -Os eliminates its dead store exactly as it
    // did for the pre-class function; an inline def would give the static vague
    // (COMDAT) linkage and defeat that DSE. Out-of-line behind nothing now —
    // called directly via Board:: from llapi.cpp / start.cpp.
    static uint32_t getPWRSpeed();

    // Timing / identity accessors. getTime_us/ms, getTick, resetTick and reset
    // are reached only through their kept extern "C" shims (same TU), so their
    // out-of-line defs are `inline` and fold into those shims bit-for-bit
    // (getTime_ms also folds into getTick/resetTick). getTime_s / getBattery*
    // are called directly via Board:: from other TUs, so they stay out-of-line.
    static uint32_t getTime_us();
    static uint32_t getTime_ms();
    static uint32_t getTime_s();
    static uint32_t getTick();
    static void     resetTick();
    static void     reset();
    static uint32_t getBatteryVoltage_mv();
    static uint32_t getBatteryMode();

private:
    static inline uint32_t boardTick = 0;

    // One-shot sub-block init helpers: single call site in init(), so
    // always_inline preserves -Os folding into Board::init.
    [[gnu::always_inline]] static void AHBH_DMAInit();
    [[gnu::always_inline]] static void AHBX_DMAInit();
    [[gnu::always_inline]] static void GPMI_Init();
    [[gnu::always_inline]] static void HardECC8_Init();
    [[gnu::always_inline]] static void USBPHYInit();
    [[gnu::always_inline]] static void LCDIF_Init();
    [[gnu::always_inline]] static void RTC_Init();
    [[gnu::always_inline]] static void LRADC_init();
};

extern "C" {
#endif

uint64_t nsToCycles(uint64_t nstime, uint64_t period, uint64_t min);

bool driverWaitTrueF(bool (*f)(), TickType_t timeout);
bool driverWaitFalseF(bool (*f)(), TickType_t timeout);

void portBoardInit(void);
void portDelayus(uint32_t us);
void portDelayms(uint32_t ms);
void boardInit(void);

uint32_t portBoardGetTime_us(void);
uint32_t portBoardGetTime_ms(void);

void portBoardReset(void);

#ifdef __cplusplus
}
#endif
