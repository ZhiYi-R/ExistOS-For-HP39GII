/**
 * @file Bootloader/drivers/display/display_up.hpp
 * @brief Display graphics / operation-queue layer — pure-static @c Display singleton.
 *
 * Phase 3.5a of the HAL C++23 migration folds the former free @c DisplayXxx()
 * API (declared @c extern "C" in Hal/display_up.h) onto this class. @c Display
 * is the graphics + operation-queue layer; it sits over the @c Lcdif hardware
 * class (drivers/display/stmp_lcdif.hpp) and is reached only from C++ translation
 * units, so no @c extern "C" seam is needed — every former @c DisplayXxx caller
 * now calls @c Display::xxx by name.
 *
 * The driver's file-scope state (the FreeRTOS operation queue and the indicator
 * latch bits) is encapsulated as private @c static @c inline members: it lands
 * in @c .bss with no global constructor, matching the previous globals bit for
 * bit. The two inner span helpers and the @c DispOpa / @c DisplayOpaQueue_t
 * queue-message types stay file-scope in display_up.cpp — they are pure
 * implementation detail the public API does not expose, and keeping the helpers
 * internal preserves the compiler's inline-and-discard of them.
 *
 * @c g_lcd_contrast is deliberately NOT a member: it is a cross-cutting global
 * shared by start.cpp (the contrast hot-key writer) and stmp_lcdif.cpp (the
 * @c Lcdif reader), so it stays a free global to avoid a Display<->Lcdif cycle.
 */
#pragma once

#include "FreeRTOS.h"
#include "queue.h"

#include <stdint.h>

class Display {
public:
    // ---- operation-queue producers (enqueue onto the display task) ----------
    static void clean();
    static void readArea(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *buf, bool *fin);
    static void flushArea(uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end, uint8_t *buf, bool block);
    static void putChar(uint32_t x, uint32_t y, char c, uint8_t fg, uint8_t bg, uint8_t fontSize);
    static bool putStr(uint32_t x, uint32_t y, char *s, uint8_t fg, uint8_t bg, uint8_t fontSize);
    static void box(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t c);
    static void boxBlock(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t c);
    static void hLine(uint32_t x0, uint32_t x1, uint32_t y, uint8_t c);
    static void vLine(uint32_t y0, uint32_t y1, uint32_t x, uint8_t c);
    static void fillBox(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t c);
    static void setIndicate(int Indicate, int batInd);

    // ---- lifecycle ----------------------------------------------------------
    static void interfaceInit();   // bring up the Lcdif interface (boardInit)
    static void init();            // create the queue + device-init the panel
    static void task();            // the display task: drains the queue forever

private:
    // Retained (declared-nowhere, caller-less) legacy entry; kept so the fold is
    // a pure structural rename rather than a dead-code prune.
    static void fillBoxBlock(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t c);

    // ---- encapsulated file-scope state (zero-init .bss, no global ctor) ------
    static inline QueueHandle_t opaQueue;

    // The four indicator latches are read/written together in setIndicate() and
    // the task loop. Grouped into one object so this co-accessed block keeps its
    // single base-register + offset codegen: as separate `static inline` members
    // they are distinct COMDAT symbols the compiler cannot prove adjacent, so
    // each costs its own address load (the same trap documented on AudioOut::st).
    struct IndicatorState {
        uint32_t indicator;
        uint32_t battery;
        uint32_t lastIndicator;
        uint32_t lastBattery;
    };
    static inline IndicatorState ind{};
};
