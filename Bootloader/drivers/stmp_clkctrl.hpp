/**
 * @file Bootloader/drivers/stmp_clkctrl.hpp
 * @brief Clock-control driver — pure-static @c Clk singleton.
 *
 * The clock driver is the @c Clk pure-static singleton. Its public operations
 * are ordinary out-of-line static methods, called directly by their C++
 * consumers (start.cpp, llapi.cpp, mtd_up.cpp) as @c Clk::setCPUDivider /
 * @c Clk::enterSlow / etc. The three register-divider primitives are also used
 * by the bring-up sequence; the slowdown entries carry the policy.
 *
 * @c init() is the bring-up entry, an ordinary out-of-line static method called
 * directly by @c boardInit (the portCLKCtrlInit / CLKCtrlInit / setCoreFreq thin
 * wrappers are gone). The @c private PLL / HFreq-domain / USB-clock sequencing
 * helpers it calls stay @c always_inline, folded into @c init(). The slowdown
 * policy state @c min_cpu_frac_sd is @c private and defined in the .cpp (so its
 * @c CPU_DIVIDE_IDLE_INITIAL seed keeps SystemConfig.h out of this header, where
 * it would otherwise collide reg_model's VERSION bitfield).
 *
 * The cross-cutting @c g_slowdown_enable is intentionally NOT encapsulated:
 * start.cpp reads it by name via `extern int g_slowdown_enable`, so it stays a
 * global-scope (unmangled) data seam, the moral twin of the extern "C" seams.
 */
#pragma once

#include <stdint.h>

class Clk {
public:
    // Register-divider primitives: shared by init() and external callers.
    static void setCPUDivider(uint32_t div);
    static void setHCLKDivider(uint32_t div);
    static void setCPUFracDivider(uint32_t div);
    static void getCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV);

    // Slowdown policy.
    static void setSlowMinFrac(uint8_t frac);
    static void enterSlow();
    static void exitSlow();
    static void slowEnable(int mode);

    // Bring-up entry; out-of-line static method, called directly by boardInit.
    static void init();

private:
    static uint8_t min_cpu_frac_sd;   // defined in .cpp (CPU_DIVIDE_IDLE_INITIAL seed)

    [[gnu::always_inline]] static void PLLEnable(bool enable);
    [[gnu::always_inline]] static void setCPU_HFreqDomain(bool enable);
    [[gnu::always_inline]] static void enableUSBClock(bool enable);
};
