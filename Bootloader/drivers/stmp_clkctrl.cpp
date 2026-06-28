/**
 * @file Bootloader/drivers/stmp_clkctrl.cpp
 * @brief Clock control driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the clock driver becomes the @c Clk
 * pure-static singleton. Its slowdown policy state @c min_cpu_frac_sd moves into
 * the class as a @c private @c static @c inline member; the internal sequencing
 * helpers (PLL / HFreq-domain / USB-clock) become private methods. The cross-
 * cutting @c g_slowdown_enable is intentionally NOT encapsulated: start.cpp reads
 * it by name via `extern int g_slowdown_enable`, so it must remain a global-scope
 * (unmangled) symbol -- a data seam, the moral twin of the extern "C" seams.
 *
 * The legacy entries survive as thin @c extern @c "C" forwarding shims
 * (stmp_clkctrl.hpp / clkctrl_up.h declare the interface @c extern @c "C"). The single-
 * caller policy/bring-up entries are always_inline so each shim folds back to the
 * pre-class body bit-for-bit; the three register-divider primitives are shared by
 * the bring-up sequence AND external callers, so they stay out-of-line methods
 * behind their shims (caller migration onto @c Clk:: is deferred to Phase 3).
 */

#include "stmp_clkctrl.hpp"
#include "stmp_board.hpp"
#include "clkctrl_up.h"
#include "reg_model.hpp"

#include "debug.h"

#include "SystemConfig.h"

#define PLL_FREQ_HZ (480000000UL)

// Cross-TU data seam (see file banner): must stay a global-scope symbol.
int g_slowdown_enable = 0;

class Clk {
public:
    // Single-caller entries (only their extern "C" shim); always_inline folds
    // each body into the named entry so it is bit-for-bit the pre-class function.
    [[gnu::always_inline]] static void init();
    [[gnu::always_inline]] static void setSlowMinFrac(uint8_t frac);
    [[gnu::always_inline]] static void enterSlow();
    [[gnu::always_inline]] static void exitSlow();
    [[gnu::always_inline]] static void slowEnable(int mode);
    [[gnu::always_inline]] static void getCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV);

    // Shared register-divider primitives: called by init() AND by external TUs,
    // so they stay out-of-line (one body, internal callers bl it; their shims
    // forward). Stateless -- touch no class member.
    static void setCPUDivider(uint32_t div);
    static void setHCLKDivider(uint32_t div);
    static void setCPUFracDivider(uint32_t div);

private:
    static inline uint8_t min_cpu_frac_sd = CPU_DIVIDE_IDLE_INITIAL;

    // Were file-scope `static` single-caller helpers (-Os folded them into the
    // bring-up path); always_inline keeps that folding now that they are methods.
    [[gnu::always_inline]] static void PLLEnable(bool enable);
    [[gnu::always_inline]] static void setCPU_HFreqDomain(bool enable);
    [[gnu::always_inline]] static void enableUSBClock(bool enable);
};

inline void Clk::PLLEnable(bool enable) {
    reg::CLKCTRL_PLLCTRL0::set(reg::CLKCTRL_PLLCTRL0_::POWER::val(enable));
    portDelayus(20);
}

void Clk::setHCLKDivider(uint32_t div)
{
    if (!div) {
        return;
    }
    while(reg::CLKCTRL_HBUS::B().BUSY);
    reg::CLKCTRL_HBUS::set(reg::CLKCTRL_HBUS_::DIV::val(div));
    while(reg::CLKCTRL_HBUS::B().BUSY);
    reg::CLKCTRL_HBUS::clr(reg::CLKCTRL_HBUS_::DIV::val(reg::CLKCTRL_HBUS::B().DIV ^ div));

}

inline void Clk::setSlowMinFrac(uint8_t frac)
{
    if(frac > 14 || frac < 2)
    {
        return;
    }
    min_cpu_frac_sd = frac;
}

inline void Clk::enterSlow()
{
    if(g_slowdown_enable)
    {
        setCPUDivider(min_cpu_frac_sd);
    }
}

inline void Clk::exitSlow()
{
    if(g_slowdown_enable == 1)
    {
        setCPUDivider(CPU_DIVIDE_NORMAL);
    }else if(g_slowdown_enable == 2){
        setCPUDivider(CPU_DIVIDE_PWRSAVE);
    }else{
        setCPUDivider(CPU_DIVIDE_NORMAL);
    }

}


inline void Clk::slowEnable(int mode)
{
    g_slowdown_enable = mode;
    if(g_slowdown_enable == 0)
    {
        setCPUDivider(CPU_DIVIDE_NORMAL);
    }else if(g_slowdown_enable == 2)
    {
        setCPUDivider(CPU_DIVIDE_PWRSAVE);
    }
}



void Clk::setCPUDivider(uint32_t div)
{
    //uint32_t val = BF_RD(CLKCTRL_CPU, DIV_CPU);
    //INFO("CPU old Div:%lu\n", val);
    if (!div) {
        return;
    }
    //while (BF_RD(CLKCTRL_CPU, BUSY_REF_CPU));
    reg::CLKCTRL_CPU::set(reg::CLKCTRL_CPU_::DIV_CPU::val(div));
    //while (BF_RD(CLKCTRL_CPU, BUSY_REF_CPU));
    reg::CLKCTRL_CPU::clr(reg::CLKCTRL_CPU_::DIV_CPU::val(reg::CLKCTRL_CPU::B().DIV_CPU ^ div));

    //INFO("CPU new Div:%d\n", BF_RD(CLKCTRL_CPU, DIV_CPU));
}

void Clk::setCPUFracDivider(uint32_t div) {

    if (!div) {
        return;
    }
    bool bypass;
    bypass = reg::CLKCTRL_CLKSEQ::B().BYPASS_CPU;
    reg::CLKCTRL_CLKSEQ::set(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    // BW_CLKCTRL_FRAC_CPUFRAC == BF_CS1 (atomic clear-field-then-set-value).
    reg::CLKCTRL_FRAC::clr(reg::CLKCTRL_FRAC_::CPUFRAC::mask);
    reg::CLKCTRL_FRAC::set(reg::CLKCTRL_FRAC_::CPUFRAC::val(div));
    if(!bypass){
        reg::CLKCTRL_CLKSEQ::clr(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    }
}

inline void Clk::setCPU_HFreqDomain(bool enable) {
    if (enable) {

        reg::CLKCTRL_CLKSEQ::clr(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    } else {
        reg::CLKCTRL_CLKSEQ::set(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    }
}

inline void Clk::enableUSBClock(bool enable) {
    if (enable) {
        reg::CLKCTRL_PLLCTRL0::set(reg::CLKCTRL_PLLCTRL0_::EN_USB_CLKS::mask);
    } else {
        reg::CLKCTRL_PLLCTRL0::clr(reg::CLKCTRL_PLLCTRL0_::EN_USB_CLKS::mask);
    }
}

inline void Clk::init() {
    //BF_SETV(POWER_VDDDCTRL, TRG, 26); // Set voltage = 1.45 V
    //BF_SETV(POWER_VDDACTRL, TRG, 18); // Set voltage = 1.95 V  val = (TAG_v - 1.5v)/0.025v


    PLLEnable(true);

    reg::CLKCTRL_FRAC::clr(reg::CLKCTRL_FRAC_::CLKGATECPU::mask);


    setCPUDivider(5);
    setHCLKDivider(4);

    setCPU_HFreqDomain(true);

    setHCLKDivider(2);
    setCPUFracDivider(22);

    enableUSBClock(true);
}

inline void Clk::getCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV)
{
    *CPU_DIV = reg::CLKCTRL_CPU::B().DIV_CPU;
    *CPU_Frac = reg::CLKCTRL_FRAC::B().CPUFRAC;
    *HCLK_DIV = reg::CLKCTRL_HBUS::B().DIV;
}

// ---------------------------------------------------------------------------
// extern "C" seams (stmp_clkctrl.hpp / clkctrl_up.h declare the interface extern "C").
// Caller migration onto Clk:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" void portCLKCtrlInit(void)            { Clk::init(); }
extern "C" void setHCLKDivider(uint32_t div)     { Clk::setHCLKDivider(div); }
extern "C" void setCPUDivider(uint32_t div)      { Clk::setCPUDivider(div); }
extern "C" void setCPUFracDivider(uint32_t div)  { Clk::setCPUFracDivider(div); }
extern "C" void setSlowDownMinCpuFrac(uint8_t frac) { Clk::setSlowMinFrac(frac); }
extern "C" void enterSlowDown()                  { Clk::enterSlow(); }
extern "C" void exitSlowDown()                   { Clk::exitSlow(); }
extern "C" void slowDownEnable(int mode)         { Clk::slowEnable(mode); }
extern "C" void portGetCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV)
{
    Clk::getCoreFreqDIV(CPU_DIV, CPU_Frac, HCLK_DIV);
}
