/**
 * @file Bootloader/drivers/stmp_power.cpp
 * @brief Power management driver
 *
 * Migrated to the typed register model. POWER_CTRL and POWER_RESET carry MMIO
 * atomic SET/CLR aliases (-> reg::*::set/clr); POWER_DCLIMITS/VDDxCTRL only have
 * software-RMW SET/CLR macros, so their BF_WR(=BF_CS1) expansions become
 * read-modify-write via wr(rd() & ~mask) / wr(rd() | val). Plain HW_*.B.F writes
 * map straight to reg::*::B().F.
 */

#include "board_up.h"
#include "reg_model.hpp"
#include "reg_values.hpp"
#include "interrupt_up.h"
#include "hw_irq.h"

#include "debug.h"

bool g_chargeEnable = false;

void portChargeEnable(bool enable)
{
    g_chargeEnable = enable;
    if(g_chargeEnable)
    {
        reg::POWER_CHARGE::B().PWD_BATTCHRG = 0;
        reg::POWER_VDDDCTRL::B().DISABLE_FET = 0;
        reg::POWER_5VCTRL::B().ENABLE_DCDC = 1;
    }else{
        reg::POWER_CHARGE::B().PWD_BATTCHRG = 1;
        reg::POWER_VDDDCTRL::B().DISABLE_FET = 1;
        reg::POWER_5VCTRL::B().ENABLE_DCDC = 0;
    }
}

// Dispatched by name from interrupt_up.c (stays C); keep C linkage.
extern "C" void portPowerIRQ(uint32_t nirq)
{
    switch (nirq)
    {
    case HW_IRQ_VDD5V:
        INFO("USB 5V Connect\\Disconnect\n");
        break;
    case HW_IRQ_VDD18_BRNOUT:
        INFO("VDD 1.8v Brownout\n");
        break;
    case HW_IRQ_VDDD_BRNOUT:
        INFO("VDDD Brownout\n");
        break;
    case HW_IRQ_VDDIO_BRNOUT:
        INFO("VDDIO Brownout\n");
        break;
    case HW_IRQ_BATT_BRNOUT:
        INFO("BAT Brownout\n");
        break;
    case 63:

        reg::POWER_CTRL::B().DC_OK_IRQ = 0;
        reg::POWER_CTRL::B().VBUSVALID_IRQ = 0;
        reg::POWER_CTRL::B().LINREG_OK_IRQ = 0;
        reg::POWER_CTRL::B().POLARITY_LINREG_OK = 0;
        reg::POWER_CTRL::B().POLARITY_PSWITCH = 0;
        reg::POWER_CTRL::B().POLARITY_VBUSVALID = 0;
        reg::POWER_CTRL::B().POLARITY_VDD5V_GT_VDDIO = 0;
        reg::POWER_CTRL::B().VDDA_BO_IRQ = 0;
        reg::POWER_CTRL::B().VDDD_BO_IRQ = 0;
        reg::POWER_CTRL::B().VDDIO_BO_IRQ = 0;
        reg::POWER_CTRL::B().BATT_BO_IRQ = 0;
        INFO("portPowerIRQ\n");
        break;
    }

}


void portBoardPowerOff()
{

    //HW_POWER_VDDIOCTRL.B.DISABLE_FET = 0;
    //HW_POWER_5VCTRL.B.ENABLE_DCDC = 0;

    reg::POWER_RESET::B().UNLOCK = 0x3E77;
    reg::POWER_RESET::clr(reg::POWER_RESET_::PWD_OFF::mask);
    reg::POWER_RESET::set(reg::POWER_RESET_::PWD_OFF::val(1));
    reg::POWER_RESET::clr(reg::POWER_RESET_::PWD::mask);
    reg::POWER_RESET::set(reg::POWER_RESET_::PWD::val(1));


}


void portPowerInit()
{

    INFO("PWR Init.\n");

    reg::POWER_CTRL::clr(reg::POWER_CTRL_::CLKGATE::mask);
/*
    BF_SET(POWER_DEBUG, VBUSVALIDPIOLOCK);
    BF_SET(POWER_DEBUG, AVALIDPIOLOCK);
    BF_SET(POWER_DEBUG, BVALIDPIOLOCK);

    BF_SET(POWER_STS, BVALID);
    BF_SET(POWER_STS, AVALID);
    BF_SET(POWER_STS, VBUSVALID);
*/
    //

    portEnableIRQ(HW_IRQ_VDD5V, true);
    portEnableIRQ(HW_IRQ_VDD18_BRNOUT, true);
    portEnableIRQ(HW_IRQ_VDDD_BRNOUT, true);
    portEnableIRQ(HW_IRQ_VDDIO_BRNOUT, true);
    portEnableIRQ(HW_IRQ_BATT_BRNOUT, true);
    portEnableIRQ(63, true);




    double DCDC_VDDD =  1.5L;    //360MHz  1.6
    double DCDC_VDDA = 1.8L;    //360MHz  2.0
    double DCDC_VDDIO = 3.3L; //360MHz  3.6

/*
    double DCDC_VDDD =  1.4L;    //360MHz  1.6
    double DCDC_VDDA = 1.85L;    //360MHz  2.0
    double DCDC_VDDIO = 3.3L; //360MHz  3.6
*/
    reg::POWER_CTRL::B().ENIRQ_DC_OK = 1;
    reg::POWER_CTRL::B().ENIRQ_LINREG_OK = 1;
    reg::POWER_CTRL::B().ENIRQ_VBUS_VALID = 1;
    reg::POWER_CTRL::B().ENIRQ_VDD5V_GT_VDDIO = 1;
    reg::POWER_CTRL::B().ENIRQ_VDDA_BO = 1;
    reg::POWER_CTRL::B().ENIRQ_VDDD_BO = 1;
    reg::POWER_CTRL::B().ENIRQ_VDDIO_BO = 1;
    reg::POWER_CTRL::B().PSWITCH_IRQ_SRC = 1;



    portGetBatterVoltage_mv();

    //goto fin;
/*
    BF_SET(POWER_DEBUG, VBUSVALIDPIOLOCK);
    BF_SET(POWER_DEBUG, AVALIDPIOLOCK);
    BF_SET(POWER_DEBUG, BVALIDPIOLOCK);

    BF_SET(POWER_STS, BVALID);
    BF_SET(POWER_STS, AVALID);
    BF_SET(POWER_STS, VBUSVALID);*/

    //HW_POWER_MINPWR.B.DOUBLE_FETS = 1;
    //HW_POWER_MINPWR.B.HALF_FETS = 1;

/*
    HW_POWER_VDDACTRL.B.ENABLE_LINREG = 1;
    HW_POWER_VDDDCTRL.B.ENABLE_LINREG = 1;

    HW_POWER_VDDACTRL.B.LINREG_OFFSET = 3;
    HW_POWER_VDDDCTRL.B.LINREG_OFFSET = 3;
    HW_POWER_VDDIOCTRL.B.LINREG_OFFSET = 3;

    HW_POWER_VDDDCTRL.B.DISABLE_STEPPING = 1;
    HW_POWER_VDDACTRL.B.DISABLE_STEPPING = 1;
    HW_POWER_VDDIOCTRL.B.DISABLE_STEPPING = 1;*/

    portDelayus(100);
/*
    BF_SET(POWER_LOOPCTRL, TOGGLE_DIF);
    BF_SET(POWER_LOOPCTRL, EN_CM_HYST);
    BF_SET(POWER_LOOPCTRL, EN_DF_HYST);
*/



    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() & ~reg::POWER_DCLIMITS_::POSLIMIT_BOOST::mask);
    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() | reg::POWER_DCLIMITS_::POSLIMIT_BOOST::val(0xF));
    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() & ~reg::POWER_DCLIMITS_::POSLIMIT_BUCK::mask);
    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() | reg::POWER_DCLIMITS_::POSLIMIT_BUCK::val(0xF));
    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() & ~reg::POWER_DCLIMITS_::NEGLIMIT::mask);
    reg::POWER_DCLIMITS::wr(reg::POWER_DCLIMITS::rd() | reg::POWER_DCLIMITS_::NEGLIMIT::val(0x6F));


    //BF_WR(POWER_DCLIMITS, POSLIMIT_BUCK, 0xF);
    //BF_WR(POWER_DCLIMITS, POSLIMIT_BOOST, 0xF);
    //BF_WR(POWER_DCLIMITS, NEGLIMIT, 0x6F);



	//BF_SET(POWER_LOOPCTRL, RCSCALE_THRESH);
    //HW_POWER_LOOPCTRL.B.EN_RCSCALE = 3;

   // BF_SET(POWER_MINPWR, HALF_FETS);
    //BF_SET(POWER_MINPWR, DOUBLE_FETS);



    //HW_POWER_SPEED.B.CTRL = 3;


    reg::POWER_DCFUNCV::B().VDDD =  (uint16_t)((DCDC_VDDD * DCDC_VDDA)/((DCDC_VDDA - DCDC_VDDD)*6.25e-3));
    reg::POWER_DCFUNCV::B().VDDIO = (uint16_t)((DCDC_VDDIO * DCDC_VDDA)/((DCDC_VDDIO - DCDC_VDDD)*6.25e-3));
    INFO("VDDD VAL:0x%03x, VDDIO VAL:0x%03x\n",reg::POWER_DCFUNCV::B().VDDD, reg::POWER_DCFUNCV::B().VDDIO);

    reg::POWER_5VCTRL::B().EN_BATT_PULLDN = 1;

    reg::POWER_5VCTRL::B().DCDC_XFER = 1;
    reg::POWER_5VCTRL::B().ENABLE_ILIMIT = 0;
    reg::POWER_LOOPCTRL::B().EN_RCSCALE = 2;
    reg::POWER_LOOPCTRL::B().DC_C = 0;


    reg::POWER_5VCTRL::B().OTG_PWRUP_CMPS = 1; //VBUSVALID comparators are enabled !
    reg::POWER_5VCTRL::B().VBUSVALID_5VDETECT = 1;
    reg::POWER_5VCTRL::B().ILIMIT_EQ_ZERO = 0;   //short

    reg::POWER_5VCTRL::B().PWDN_5VBRNOUT = 0;

    reg::POWER_5VCTRL::B().ENABLE_DCDC = 0;  //Enable DC_DC later

    portDelayus(100);

    reg::POWER_BATTMONITOR::B().BRWNOUT_LVL = 0; // 0.79 V
    reg::POWER_BATTMONITOR::B().BRWNOUT_PWD = 0;
    reg::POWER_BATTMONITOR::B().EN_BATADJ = 1;
    reg::POWER_BATTMONITOR::B().PWDN_BATTBRNOUT = 1;


    reg::POWER_CHARGE::B().ENABLE_FAULT_DETECT = 0;
    //HW_POWER_CHARGE.B.BATTCHRG_I  = 1 << 5;
    //HW_POWER_CHARGE.B.STOP_ILIMIT = 1 << 3;
    reg::POWER_CHARGE::B().USE_EXTERN_R = 1;

    reg::POWER_CHARGE::B().CHRG_STS_OFF = 0;
    reg::POWER_CHARGE::B().PWD_BATTCHRG = 1;


    portDelayus(100);

/*
    HW_POWER_VDDDCTRL.B.ALKALINE_CHARGE = 1;
    HW_POWER_VDDDCTRL.B.DISABLE_FET = 1;
    HW_POWER_VDDDCTRL.B.LINREG_OFFSET = 3;
    HW_POWER_VDDIOCTRL.B.LINREG_OFFSET = 3;
*/


    portDelayus(500);

    reg::POWER_VDDDCTRL::B().ENABLE_LINREG = 1;
    reg::POWER_VDDACTRL::B().ENABLE_LINREG = 1;


    reg::POWER_VDDDCTRL::B().DISABLE_FET = 1;
    reg::POWER_VDDDCTRL::B().LINREG_OFFSET = 0;
    reg::POWER_VDDDCTRL::B().ALKALINE_CHARGE = 0;




//fin:


    //HW_POWER_5VCTRL.B.EN_BATT_PULLDN = 1;

    reg::POWER_VDDDCTRL::wr(reg::POWER_VDDDCTRL::rd() & ~reg::POWER_VDDDCTRL_::TRG::mask);
    reg::POWER_VDDDCTRL::wr(reg::POWER_VDDDCTRL::rd() | reg::POWER_VDDDCTRL_::TRG::val((uint8_t)((1.44 - 0.8)/0.025)));  // val = (TAG_v - 0.8v)/0.025v, 0.8~1.45, 1.2
    portDelayus(200);
    reg::POWER_VDDACTRL::wr(reg::POWER_VDDACTRL::rd() & ~reg::POWER_VDDACTRL_::TRG::mask);
    reg::POWER_VDDACTRL::wr(reg::POWER_VDDACTRL::rd() | reg::POWER_VDDACTRL_::TRG::val((uint8_t)((1.8 - 1.5)/0.025)));  // val = (TAG_v - 1.5v)/0.025v, 1.5~1.95, 1.75
    portDelayus(200);
    reg::POWER_VDDIOCTRL::wr(reg::POWER_VDDIOCTRL::rd() & ~reg::POWER_VDDIOCTRL_::TRG::mask);
    reg::POWER_VDDIOCTRL::wr(reg::POWER_VDDIOCTRL::rd() | reg::POWER_VDDIOCTRL_::TRG::val((uint8_t)((3.3 - 2.8)/0.025)));  // val = (TAG_v - 2.8v)/0.025v, 2.8~3.575, 3.1
    portDelayus(100);

    INFO("VDDIO LDO VAL:0x%03x\n", reg::POWER_VDDIOCTRL::B().TRG );
    INFO("VDDD LDO VAL:0x%03x\n", reg::POWER_VDDDCTRL::B().TRG );
    INFO("VDDA LDO VAL:0x%03x\n", reg::POWER_VDDACTRL::B().TRG );

}