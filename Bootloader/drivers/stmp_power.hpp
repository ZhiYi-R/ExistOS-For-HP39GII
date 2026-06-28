/**
 * @file Bootloader/drivers/stmp_power.hpp
 * @brief Power / charger HAL seams (forward to the Power singleton).
 *
 * Split out of board_up.h in Phase 3. extern "C" so the existing port*
 * callers resolve the unmangled symbols until they migrate to Power::.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void portPowerInit();
void portChargeEnable(bool enable);
void portBoardPowerOff(void);

#ifdef __cplusplus
}
#endif
