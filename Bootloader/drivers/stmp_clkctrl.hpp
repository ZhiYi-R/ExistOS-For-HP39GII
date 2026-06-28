/**
 * @file Bootloader/drivers/stmp_clkctrl.hpp
 * @brief Clock-control HAL seams (forward to the Clk singleton).
 *
 * Split out of board_up.h in Phase 3. extern "C" so the existing callers
 * (start.cpp, llapi.cpp, mtd_up.cpp) resolve the unmangled symbols until
 * they migrate to Clk::. The clock-init seams (portCLKCtrlInit/CLKCtrlInit)
 * still live in clkctrl_up.h pending the thin-wrapper merge.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void setCPUDivider(uint32_t div);
void setCPUFracDivider(uint32_t div);
void setHCLKDivider(uint32_t div);
void portGetCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV);

void enterSlowDown();
void exitSlowDown();
void slowDownEnable(int mode);
void setSlowDownMinCpuFrac(uint8_t frac);

#ifdef __cplusplus
}
#endif
