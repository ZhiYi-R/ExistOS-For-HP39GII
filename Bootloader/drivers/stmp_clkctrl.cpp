/**
 * @file Bootloader/drivers/stmp_clkctrl.c
 * @brief Clock control driver
 */

#include "board_up.h"
#include "clkctrl_up.h"
#include "reg_model.hpp"

#include "debug.h"

#include "SystemConfig.h"

#define PLL_FREQ_HZ (480000000UL)

int g_slowdown_enable = 0;
static uint8_t min_cpu_frac_sd = CPU_DIVIDE_IDLE_INITIAL;

static void PLLEnable(bool enable) {
    reg::CLKCTRL_PLLCTRL0::set(reg::CLKCTRL_PLLCTRL0_::POWER::val(enable));
    portDelayus(20);
}

void setHCLKDivider(uint32_t div)
{
    if (!div) {
        return;
    }
    while(reg::CLKCTRL_HBUS::B().BUSY);
    reg::CLKCTRL_HBUS::set(reg::CLKCTRL_HBUS_::DIV::val(div));
    while(reg::CLKCTRL_HBUS::B().BUSY);
    reg::CLKCTRL_HBUS::clr(reg::CLKCTRL_HBUS_::DIV::val(reg::CLKCTRL_HBUS::B().DIV ^ div));

}

void setSlowDownMinCpuFrac(uint8_t frac)
{
    if(frac > 14 || frac < 2)
    {
        return;
    }
    min_cpu_frac_sd = frac;
}

void enterSlowDown()
{
    if(g_slowdown_enable)
    {
        setCPUDivider(min_cpu_frac_sd);
    }
}

void exitSlowDown()
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


void slowDownEnable(int mode)
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



void setCPUDivider(uint32_t div) 
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

void setCPUFracDivider(uint32_t div) {

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

static void setCPU_HFreqDomain(bool enable) {
    if (enable) {

        reg::CLKCTRL_CLKSEQ::clr(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    } else {
        reg::CLKCTRL_CLKSEQ::set(reg::CLKCTRL_CLKSEQ_::BYPASS_CPU::mask);
    }
}

static void enableUSBClock(bool enable) {
    if (enable) {
        reg::CLKCTRL_PLLCTRL0::set(reg::CLKCTRL_PLLCTRL0_::EN_USB_CLKS::mask);
    } else {
        reg::CLKCTRL_PLLCTRL0::clr(reg::CLKCTRL_PLLCTRL0_::EN_USB_CLKS::mask);
    }
}

void portCLKCtrlInit(void) {
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

void portGetCoreFreqDIV(uint32_t *CPU_DIV, uint32_t *CPU_Frac, uint32_t *HCLK_DIV)
{
    *CPU_DIV = reg::CLKCTRL_CPU::B().DIV_CPU;
    *CPU_Frac = reg::CLKCTRL_FRAC::B().CPUFRAC;
    *HCLK_DIV = reg::CLKCTRL_HBUS::B().DIV;
}
