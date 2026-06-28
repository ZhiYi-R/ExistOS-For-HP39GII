/**
 * @file Bootloader/drivers/stmp_board.hpp
 * @brief Board timing, delay, reset and identity HAL seams.
 *
 * Phase 3 of the HAL C++23 migration splits the monolithic board_up.h
 * "everything" header back into per-peripheral headers. These extern "C"
 * seams forward (in stmp_board.cpp) to the Board singleton; nsToCycles and
 * portDelay* are stateless free functions; driverWait* / boardInit still
 * live in the board_up.cpp orchestration layer pending its own merge. The
 * seam set stays C-callable: vectors.c (portBoardGetTime_us/ms) and stub.c
 * (portBoardReset) resolve these unmangled symbols across the C boundary.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"   // TickType_t for driverWait*

#ifdef __cplusplus
extern "C" {
#endif

uint64_t nsToCycles(uint64_t nstime, uint64_t period, uint64_t min);

bool driverWaitTrueF(bool (*f)(), TickType_t timeout);
bool driverWaitFalseF(bool (*f)(), TickType_t timeout);

void portBoardInit(void);
void portDelayus(uint32_t us);
void portDelayms(uint32_t ms);
void boardInit(void);

uint32_t portGetBatterVoltage_mv();
uint32_t portGetBatteryMode();
uint32_t portGetPWRSpeed();

uint32_t portBoardGetTime_us(void);
uint32_t portBoardGetTime_ms(void);
uint32_t portBoardGetTime_s(void);

void portBoardReset(void);

#ifdef __cplusplus
}
#endif
