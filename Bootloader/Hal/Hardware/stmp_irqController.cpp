/**
 * @file Bootloader/Hal/Hardware/stmp_irqController.c
 * @brief Interrupt controller driver
 */


#include "interrupt_up.h"
#include "reg_model.hpp"
#include "reg_values.hpp"
#include "hw_irq.h"
#include <stdint.h>
#include "debug.h"

void portIRQCtrlInit()
{
    reg::ICOLL_CTRL::clr(reg::ICOLL_CTRL_::SFTRST::mask);
    reg::ICOLL_CTRL::clr(reg::ICOLL_CTRL_::CLKGATE::mask);
    reg::ICOLL_CTRL::set(reg::ICOLL_CTRL_::SFTRST::mask);
    while (!reg::ICOLL_CTRL::B().CLKGATE)
    {
        ;
    }
    reg::ICOLL_CTRL::clr(reg::ICOLL_CTRL_::SFTRST::mask);
    reg::ICOLL_CTRL::clr(reg::ICOLL_CTRL_::CLKGATE::mask);

    reg::ICOLL_CTRL::clr(reg::ICOLL_CTRL_::BYPASS_FSM::mask | reg::ICOLL_CTRL_::NO_NESTING::mask | reg::ICOLL_CTRL_::ARM_RSE_MODE::mask);

    reg::ICOLL_CTRL::set(reg::ICOLL_CTRL_::FIQ_FINAL_ENABLE::mask |
                      reg::ICOLL_CTRL_::IRQ_FINAL_ENABLE::mask |
                      reg::ICOLL_CTRL_::ARM_RSE_MODE::mask |
                      reg::ICOLL_CTRL_::NO_NESTING::mask);

    reg::ICOLL_VBASE::clr(reg::ICOLL_VBASE_::TABLE_ADDRESS::mask);
    reg::ICOLL_VBASE::set(reg::ICOLL_VBASE_::TABLE_ADDRESS::val(0));
    reg::ICOLL_CTRL::B().VECTOR_PITCH = reg::ICOLL_CTRL_sym::VECTOR_PITCH__DEFAULT_BY4;

}

/**
bool portIRQDecode(IRQNumber* IRQNum, IRQTypes *IRQType, IRQInfo *IRQInfo)
{
    *IRQNum = BF_RD(ICOLL_VECTOR, IRQVECTOR) / 4;
    switch (*IRQNum)
    {
    case HW_IRQ_TIMER0:
        *IRQType = IRQType_Timer;
        *IRQInfo = HW_IRQ_TIMER0;
        break;
    case HW_IRQ_TIMER1:
        *IRQType = IRQType_Timer;
        *IRQInfo = HW_IRQ_TIMER1;
        break;    
    case HW_IRQ_USB_CTRL:
        *IRQType = IRQType_USBCtrl;
        *IRQInfo = HW_IRQ_USB_CTRL;
        break; 
    case HW_IRQ_GPMI:
        *IRQType = IRQType_MTD;
        *IRQInfo = HW_IRQ_GPMI;
        break;
    case HW_IRQ_GPMI_DMA:
        *IRQType = IRQType_MTD_DMA;
        *IRQInfo = HW_IRQ_GPMI_DMA;
        break;
    case HW_IRQ_ECC8_IRQ:
        *IRQType = IRQType_MTD_ECC;
        *IRQInfo = HW_IRQ_ECC8_IRQ;
        break;
    case HW_IRQ_LCDIF_DMA:
    case HW_IRQ_LCDIF_ERROR:
        *IRQType = IRQType_DISP;
        *IRQInfo = HW_IRQ_LCDIF_DMA;
        break;
        
    case HW_IRQ_LRADC_CH0:
        *IRQInfo = 0;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH1:
        *IRQInfo = 1;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH2:
        *IRQInfo = 2;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH3:
        *IRQInfo = 3;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH4:
        *IRQInfo = 4;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH5:
        *IRQInfo = 5;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH6:
        *IRQInfo = 6;
        *IRQType = IRQType_LRADC;
        break;
    case HW_IRQ_LRADC_CH7:
        *IRQInfo = 7;
        *IRQType = IRQType_LRADC;
        break;    
    case HW_IRQ_VDD5V:
    case HW_IRQ_VDD5V_DROOP:
    case HW_IRQ_VDD18_BRNOUT:
    case HW_IRQ_VDDD_BRNOUT:
    case HW_IRQ_VDDIO_BRNOUT:
    case HW_IRQ_BATT_BRNOUT:
    case 63:
        *IRQInfo = *IRQNum;
        *IRQType = IRQType_PWR;
    break;
    case HW_IRQ_DAC_DMA:
    case HW_IRQ_DAC_ERROR:
        *IRQInfo = *IRQNum;
        *IRQType = IRQType_DAC;
        break;
        
    default:
        PANIC("ERR IRQNum:%d\n", *IRQNum);
        return false;
    }
    return true;
}*/
void portAckIRQ(IRQNumber IRQNum)
{
//    BF_SETV(ICOLL_VECTOR, IRQVECTOR, IRQNum);
    //BF_SETV(ICOLL_LEVELACK, IRQLEVELACK, 
    //   1 << ((*((volatile unsigned int *)HW_ICOLL_PRIORITYn_ADDR(HW_ICOLL_STAT.B.VECTOR_NUMBER / 4)) >> (8 * (HW_ICOLL_STAT.B.VECTOR_NUMBER % 4))) & 0x03)); 
    reg::ICOLL_LEVELACK::B().IRQLEVELACK = 1;
}

void portEnableIRQ(unsigned int IRQNum, unsigned int enable) 
{
    //if (IRQNum > 63)
     //   return;
    volatile unsigned int *baseAddress = (unsigned int *)reg::ICOLL_PRIORITYn::address(IRQNum / 4);
    if (enable){
        baseAddress[1] = (0x4 << ((IRQNum % 4) * 8));
    } else {
        baseAddress[2] = (0x4 << ((IRQNum % 4) * 8));
    }
}


