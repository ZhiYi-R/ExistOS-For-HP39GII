/**
 * @file Bootloader/drivers/stmp_lradc.hpp
 * @brief LRADC HAL seams (forward to the Lradc singleton).
 *
 * Split out of board_up.h in Phase 3. extern "C" so the existing port*
 * callers resolve the unmangled symbols until they migrate to Lradc::.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t portLRADCConvCh(uint32_t ch, uint32_t samples);
void portLRADCEnable(bool enable, uint32_t ch);
void portLRADC_init();

#ifdef __cplusplus
}
#endif
