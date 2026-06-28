/**
 * @file Bootloader/drivers/stmp_board.cpp
 * @brief Board bring-up driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the board bring-up sequence and the only
 * piece of board state (@c boardTick) become the @c Board pure-static singleton.
 * @c boardTick was a file-scope global with no cross-TU reference, so it moves
 * into the class as a @c private @c static @c inline member; the eight one-shot
 * sub-block init helpers become @c private @c always_inline methods so -Os folds
 * them into @c Board::init exactly as it folded the file-scope @c static helpers.
 *
 * Three stateless cross-cutting leaf primitives stay free @c extern @c "C"
 * functions: @c nsToCycles (a pure units helper used by name from lcdif/gpmi),
 * and @c portDelayus / @c portDelayms (hot busy-wait primitives called from five
 * other TUs). They hold no board state, so encapsulating them buys nothing and
 * only adds shim hops; folding them into a HAL facade is left to Phase 3.
 *
 * The legacy @c portBoard* / @c portGet* entries survive as thin @c extern @c "C"
 * forwarding shims (stmp_board.hpp declares the interface @c extern @c "C"; the time
 * reads are also SWI seams reached by name from vectors.c, and @c portBoardReset
 * from stub.c). The tiny accessors are implicitly inline so -Os folds each into
 * its shim bit-for-bit; caller migration onto @c Board:: is deferred to Phase 3.
 */

#include "stmp_board.hpp"
#include "stmp_power.hpp"
#include "stmp_lradc.hpp"
#include "reg_model.hpp"
#include "reg_values.hpp"

// Stateless cross-cutting leaf utility (see file banner): used by name from
// lcdif/gpmi, holds no board state, so it stays a free extern "C" function.
uint64_t nsToCycles(uint64_t nstime, uint64_t period, uint64_t min)
{
    uint64_t k = 0;
    k = (nstime + period - 1) / period;
    return (k > min) ? k : min;
}

class Board {
public:
    // Larger single-caller entry; always_inline folds the body into its extern
    // "C" shim so the named entry is bit-for-bit the pre-class function.
    [[gnu::always_inline]] static void init();

    // getPWRSpeed owns a function-local `static last_val`. As a non-inline
    // method that static keeps internal linkage, so -Os eliminates its dead
    // store exactly as it did for the file-scope function; marking the method
    // inline would give the static vague (COMDAT) linkage and defeat that DSE.
    // It stays out-of-line behind its shim (one body, +1 trampoline hop).
    static uint32_t getPWRSpeed();

    // Tiny accessors: implicitly inline, -Os folds them into their shims (and,
    // for getTime_ms, into getTick/resetTick) exactly as the originals folded.
    static uint32_t getTime_us() { return reg::DIGCTL_MICROSECONDS::rd(); }
    static uint32_t getTime_ms() { return reg::RTC_MILLISECONDS::rd(); }
    static uint32_t getTime_s()  { return reg::RTC_SECONDS::rd(); }
    static uint32_t getTick()    { return getTime_ms() - boardTick; }
    static void resetTick()      { boardTick = getTime_ms(); }
    static void reset()
    {
        reg::CLKCTRL_RESET::wr(reg::CLKCTRL_RESET::rd() & ~reg::CLKCTRL_RESET_::CHIP::mask);
        reg::CLKCTRL_RESET::wr(reg::CLKCTRL_RESET::rd() | reg::CLKCTRL_RESET_::CHIP::val(1));
    }
    static uint32_t getBatteryVoltage_mv()
    {
        //portLRADCConvCh(7, 1);
        uint32_t ad_val = reg::POWER_BATTMONITOR::B().BATT_VAL;
        return ad_val * 8;
    }
    static uint32_t getBatteryMode() { return reg::POWER_STS::B().MODE; }

private:
    static inline uint32_t boardTick = 0;

    // One-shot sub-block init helpers (file-scope `static`, single call site in
    // init() -> -Os folded them); always_inline preserves that folding.
    [[gnu::always_inline]] static void AHBH_DMAInit();
    [[gnu::always_inline]] static void AHBX_DMAInit();
    [[gnu::always_inline]] static void GPMI_Init();
    [[gnu::always_inline]] static void HardECC8_Init();
    [[gnu::always_inline]] static void USBPHYInit();
    [[gnu::always_inline]] static void LCDIF_Init();
    [[gnu::always_inline]] static void RTC_Init();
    [[gnu::always_inline]] static void LRADC_init();
};


inline void Board::AHBH_DMAInit()
{


    reg::APBH_CTRL0::clr(reg::APBH_CTRL0_::SFTRST::mask);
    reg::APBH_CTRL0::clr(reg::APBH_CTRL0_::CLKGATE::mask);

    reg::APBH_CTRL0::set(reg::APBH_CTRL0_::SFTRST::mask);
    while(reg::APBH_CTRL0::B().CLKGATE == 0){
        ;
    }

    reg::APBH_CTRL0::clr(reg::APBH_CTRL0_::SFTRST::mask);
    reg::APBH_CTRL0::clr(reg::APBH_CTRL0_::CLKGATE::mask);
}

inline void Board::AHBX_DMAInit()
{
    reg::APBX_CTRL0::clr(reg::APBX_CTRL0_::SFTRST::mask);
    reg::APBX_CTRL0::clr(reg::APBX_CTRL0_::CLKGATE::mask);

    reg::APBX_CTRL0::set(reg::APBX_CTRL0_::SFTRST::mask);
    while(reg::APBX_CTRL0::B().CLKGATE == 0){
        ;
    }

    reg::APBX_CTRL0::clr(reg::APBX_CTRL0_::SFTRST::mask);
    reg::APBX_CTRL0::clr(reg::APBX_CTRL0_::CLKGATE::mask);
}

inline void Board::GPMI_Init()
{

    reg::CLKCTRL_GPMI::wr(reg::CLKCTRL_GPMI::rd() & ~reg::CLKCTRL_GPMI_::CLKGATE::mask);

    reg::GPMI_CTRL0::clr(reg::GPMI_CTRL0_::SFTRST::mask);
    reg::GPMI_CTRL0::clr(reg::GPMI_CTRL0_::CLKGATE::mask);

    reg::GPMI_CTRL0::set(reg::GPMI_CTRL0_::SFTRST::mask);

    while(reg::GPMI_CTRL0::B().CLKGATE == 0){
        ;
    }

    reg::GPMI_CTRL0::clr(reg::GPMI_CTRL0_::SFTRST::mask);
    reg::GPMI_CTRL0::clr(reg::GPMI_CTRL0_::CLKGATE::mask);

    reg::PINCTRL_MUXSEL0::clr(
        reg::PINCTRL_MUXSEL0_::BANK0_PIN07::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN06::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN05::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN04::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN03::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN02::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN01::mask |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN00::mask);
    reg::PINCTRL_MUXSEL0::set(
        reg::PINCTRL_MUXSEL0_::BANK0_PIN07::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN06::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN05::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN04::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN03::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN02::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN01::val(0) |
        reg::PINCTRL_MUXSEL0_::BANK0_PIN00::val(0));

    reg::PINCTRL_MUXSEL4::clr(reg::PINCTRL_MUXSEL4_::BANK2_PIN15::mask);
    reg::PINCTRL_MUXSEL4::set(reg::PINCTRL_MUXSEL4_::BANK2_PIN15::val(1));

    reg::PINCTRL_MUXSEL1::clr(
        reg::PINCTRL_MUXSEL1_::BANK0_PIN25::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN24::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN23::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN22::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN19::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN17::mask |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN16::mask);
    reg::PINCTRL_MUXSEL1::set(
        reg::PINCTRL_MUXSEL1_::BANK0_PIN25::val(0) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN24::val(0) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN23::val(3) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN22::val(0) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN19::val(0) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN17::val(0) |
        reg::PINCTRL_MUXSEL1_::BANK0_PIN16::val(0));

    reg::PINCTRL_DRIVE0::clr(
        reg::PINCTRL_DRIVE0_::BANK0_PIN07_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN06_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN05_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN04_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN03_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN02_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN01_MA::mask |
        reg::PINCTRL_DRIVE0_::BANK0_PIN00_MA::mask);
    reg::PINCTRL_DRIVE0::set(
        reg::PINCTRL_DRIVE0_::BANK0_PIN07_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN06_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN05_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN04_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN03_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN02_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN01_MA::val(2) |
        reg::PINCTRL_DRIVE0_::BANK0_PIN00_MA::val(2));

    reg::PINCTRL_DRIVE2::clr(
        reg::PINCTRL_DRIVE2_::BANK0_PIN23_MA::mask |
        reg::PINCTRL_DRIVE2_::BANK0_PIN22_MA::mask |
        reg::PINCTRL_DRIVE2_::BANK0_PIN19_MA::mask |
        reg::PINCTRL_DRIVE2_::BANK0_PIN17_MA::mask |
        reg::PINCTRL_DRIVE2_::BANK0_PIN16_MA::mask);
    reg::PINCTRL_DRIVE2::set(
        reg::PINCTRL_DRIVE2_::BANK0_PIN23_MA::val(2) |
        reg::PINCTRL_DRIVE2_::BANK0_PIN22_MA::val(2) |
        reg::PINCTRL_DRIVE2_::BANK0_PIN19_MA::val(2) |
        reg::PINCTRL_DRIVE2_::BANK0_PIN17_MA::val(2) |
        reg::PINCTRL_DRIVE2_::BANK0_PIN16_MA::val(2));

    reg::PINCTRL_DRIVE3::clr(
        reg::PINCTRL_DRIVE3_::BANK0_PIN25_MA::mask |
        reg::PINCTRL_DRIVE3_::BANK0_PIN24_MA::mask);
    reg::PINCTRL_DRIVE3::set(
        reg::PINCTRL_DRIVE3_::BANK0_PIN25_MA::val(2) |
        reg::PINCTRL_DRIVE3_::BANK0_PIN24_MA::val(2));

}

inline void Board::HardECC8_Init()
{
    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::SFTRST::mask);
    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::CLKGATE::mask);

    reg::ECC8_CTRL::set(reg::ECC8_CTRL_::SFTRST::mask);
    while(reg::ECC8_CTRL::B().CLKGATE == 0)
    {
        ;
    }

    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::SFTRST::mask);
    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::CLKGATE::mask);

    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::AHBM_SFTRST::mask);

}

inline void Board::USBPHYInit()
{
    reg::USBPHY_CTRL::clr(reg::USBPHY_CTRL_::SFTRST::mask);
    reg::USBPHY_CTRL::clr(reg::USBPHY_CTRL_::CLKGATE::mask);

    reg::USBPHY_CTRL::set(reg::USBPHY_CTRL_::SFTRST::mask);
    while(reg::USBPHY_CTRL::B().CLKGATE == 0)
    {
        ;
    }

    reg::USBPHY_CTRL::clr(reg::USBPHY_CTRL_::SFTRST::mask);
    reg::USBPHY_CTRL::clr(reg::USBPHY_CTRL_::CLKGATE::mask);
}

inline void Board::LCDIF_Init()
{

    reg::CLKCTRL_CLKSEQ::set(reg::CLKCTRL_CLKSEQ_::BYPASS_PIX::mask);
    reg::CLKCTRL_PIX::wr(reg::CLKCTRL_PIX::rd() & ~reg::CLKCTRL_PIX_::CLKGATE::mask);

    reg::LCDIF_CTRL::clr(reg::LCDIF_CTRL_::SFTRST::mask);
    reg::LCDIF_CTRL::clr(reg::LCDIF_CTRL_::CLKGATE::mask);

    reg::LCDIF_CTRL::set(reg::LCDIF_CTRL_::SFTRST::mask);
    while(reg::LCDIF_CTRL::B().CLKGATE == 0)
    {
        ;
    }

    reg::LCDIF_CTRL::clr(reg::LCDIF_CTRL_::SFTRST::mask);
    reg::LCDIF_CTRL::clr(reg::LCDIF_CTRL_::CLKGATE::mask);
}

inline void Board::RTC_Init()
{
    reg::RTC_CTRL::clr(reg::RTC_CTRL_::SFTRST::mask);
    reg::RTC_CTRL::clr(reg::RTC_CTRL_::CLKGATE::mask);

    reg::RTC_CTRL::set(reg::RTC_CTRL_::SFTRST::mask);
    while(reg::RTC_CTRL::B().CLKGATE == 0)
    {
        ;
    }

    reg::RTC_CTRL::clr(reg::RTC_CTRL_::SFTRST::mask);
    reg::RTC_CTRL::clr(reg::RTC_CTRL_::CLKGATE::mask);

}

inline void Board::LRADC_init()
{
    reg::LRADC_CTRL0::clr(reg::LRADC_CTRL0_::SFTRST::mask);
    reg::LRADC_CTRL0::clr(reg::LRADC_CTRL0_::CLKGATE::mask);

    reg::LRADC_CTRL0::set(reg::LRADC_CTRL0_::SFTRST::mask);
    while(reg::LRADC_CTRL0::B().CLKGATE == 0)
    {
        ;
    }

    reg::LRADC_CTRL0::clr(reg::LRADC_CTRL0_::SFTRST::mask);
    reg::LRADC_CTRL0::clr(reg::LRADC_CTRL0_::CLKGATE::mask);

}

uint32_t Board::getPWRSpeed()
{
    uint8_t val = 0;
    static uint8_t last_val;

    vTaskEnterCritical();
    reg::POWER_SPEED::B().CTRL = 0;
    portDelayus(1);
    reg::POWER_SPEED::B().CTRL = 1;
    portDelayus(1);
    reg::POWER_SPEED::B().CTRL = 3;
    val = reg::POWER_SPEED::B().STATUS;
    vTaskExitCritical();
    last_val = val;
    return (val + last_val) / 2;
}

inline void Board::init()
{



    Power::init();

    USBPHYInit();
    AHBH_DMAInit();
    AHBX_DMAInit();
    GPMI_Init();
    HardECC8_Init();
    LCDIF_Init();
    RTC_Init();
    LRADC_init();
    portLRADC_init();



}

// Stateless cross-cutting leaf busy-wait primitives (see file banner): hot, used
// by name from five other TUs, no board state -> stay free extern "C" functions.
void portDelayus(uint32_t us)
{
    uint32_t start;
    uint32_t cur;
    start = cur = reg::DIGCTL_MICROSECONDS::rd();
    while (cur < start + us) {
        cur = reg::DIGCTL_MICROSECONDS::rd();
    }
}

void portDelayms(uint32_t ms)
{
    uint32_t start;
    uint32_t cur;
    start = cur = reg::RTC_MILLISECONDS::rd();
    while (cur < start + ms) {
        cur = reg::RTC_MILLISECONDS::rd();
    }
}

// ---------------------------------------------------------------------------
// extern "C" seams (stmp_board.hpp declares the interface extern "C"; the time reads
// are SWI seams from vectors.c and portBoardReset is reached from stub.c).
// Caller migration onto Board:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" uint32_t portBoardGetTime_us()       { return Board::getTime_us(); }
extern "C" uint32_t portBoardGetTime_ms()       { return Board::getTime_ms(); }
extern "C" uint32_t portBoardGetTime_s()        { return Board::getTime_s(); }
extern "C" uint32_t portBoardGetTick()          { return Board::getTick(); }
extern "C" void portBoardResetTick()            { Board::resetTick(); }
extern "C" void portBoardReset()                { Board::reset(); }
extern "C" uint32_t portGetBatterVoltage_mv()   { return Board::getBatteryVoltage_mv(); }
extern "C" uint32_t portGetBatteryMode()        { return Board::getBatteryMode(); }
extern "C" uint32_t portGetPWRSpeed()           { return Board::getPWRSpeed(); }
extern "C" void portBoardInit()                 { Board::init(); }
