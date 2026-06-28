/**
 * @file Bootloader/drivers/display/stmp_lcdif.hpp
 * @brief LCDIF display-controller driver — pure-static @c Lcdif singleton.
 *
 * Phase 3.B of the HAL C++23 migration. The @c Lcdif pure-static singleton is the
 * LCDIF display driver; its method bodies live out-of-line in stmp_lcdif.cpp, so
 * this header pulls no reg_model.hpp and stays clear of the reg_types VERSION
 * bitfield's collision with SystemConfig.h's @c VERSION macro. C++ consumers
 * (display_up.cpp, start.cpp) call @c Lcdif::interfaceInit / @c setIndicate /
 * @c readBackVRAM / @c flushAreaBuf / @c deviceInit / @c clean / @c setContrast
 * directly; the legacy portDisp* / Display* forwarding shims are gone.
 *
 * The observable driver state (clock cache, line buffer, indicator latches) is
 * @c private @c static @c inline (lands in @c .bss, zero construction). @c opaFinish
 * is the one piece shared with the LCDIF DMA-completion ISR, so @c portDISP_ISR
 * (dispatched by name from interrupt_up.cpp, hence @c extern @c "C") is befriended
 * to reach it directly. The two @c private one-shot helpers stay @c always_inline
 * so -Os folds them into @c deviceInit.
 */
#pragma once

#include <stdint.h>

// Dispatched by name from interrupt_up.cpp (stays C linkage); forward-declared
// here so Lcdif can befriend it for access to the private opaFinish flag.
extern "C" void portDISP_ISR();

// LCDIF display-controller driver — pure-static singleton (method definitions in
// stmp_lcdif.cpp). interfaceInit / setIndicate / readBackVRAM / flushAreaBuf /
// deviceInit are now called directly via Lcdif:: from other TUs, so they are
// ordinary out-of-line external methods; clean / setContrast (also reached
// internally by deviceInit) are likewise out-of-line.
class Lcdif {
public:
    static void interfaceInit();
    static void setIndicate(int indicateBit, int batteryBit);
    static void readBackVRAM(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *buf);
    static void flushAreaBuf(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *buf);
    static void deviceInit();
    static void clean();
    static void setContrast(uint8_t contrast);

private:
    static inline uint64_t LCDIFFreq = 0;
    static inline volatile bool opaFinish = false;
    static inline uint8_t lineBuffer[256 + 4] __attribute__((aligned(4))) = {};  // SCREEN_WIDTH (256) + 4
    static inline int save_bat = 0;
    static inline int save_ind_bit = 0;

    // Single (deviceInit) call site that -Os folded; always_inline preserves
    // that fold as members. SetTiming caches the pixel clock into LCDIFFreq;
    // DMAChainsInit primes the file-scope DMA-descriptor scratch chains.
    [[gnu::always_inline]] static void DMAChainsInit();
    [[gnu::always_inline]] static void SetTiming();

    friend void ::portDISP_ISR();
};
