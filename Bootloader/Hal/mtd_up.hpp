/**
 * @file Bootloader/Hal/mtd_up.hpp
 * @brief MTD service layer — pure-static @c Mtd singleton over the @c Gpmi driver.
 *
 * Phase 3.5b of the HAL C++23 migration folds the former free @c MTD_*() service
 * API (declared @c extern "C" in mtd_up.h) onto this class. @c Mtd is the
 * operation-queue / task layer: each public physical-op method marshals an
 * @c MTD_Operates request onto @c st.opQueue and blocks on a task notification;
 * @c Mtd::task() drains the queue and drives the @c Gpmi hardware class
 * (drivers/stmp_gpmi.hpp) directly. The three GPMI interrupts call
 * @c Mtd::upOpaFin() by name to latch the ECC result and release the waiter, so
 * that one method is the service<->driver completion bridge (mirrors how the
 * audio SWI/IRQ contexts share AudioOut state).
 *
 * The driver's file-scope state is encapsulated as private @c static members.
 * The co-accessed block (queue handle, geometry, current op, flags, retry count)
 * is grouped into one @c State object so it lands as a single COMDAT symbol and
 * keeps base-register + offset codegen -- separate @c static @c inline scalars
 * would be distinct symbols the compiler cannot prove adjacent, each costing its
 * own address load (the same grouping the sibling @c Gpmi::st uses).
 *
 * The @c g_mtd_*_cnt counters stay free globals (defined in mtd_up.cpp): they are
 * read by start.cpp's flash-stats dump via @c extern declarations, so folding
 * them into the class would mangle the symbols start.cpp links against.
 */
#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// NAND geometry descriptor -- populated by Gpmi::deviceInit()/GetNANDInfo() and
// handed back to callers through Mtd::getDeviceInfo(). Namespace-scope because
// the Gpmi driver names mtdInfo_t in its own interface.
// ---------------------------------------------------------------------------
typedef struct
{
    uint32_t PageSize_B;
    uint32_t SpareSizePerPage_B;
    uint32_t BlockSize_KB;
    uint32_t PagesPerBlock;
    uint32_t MetaSize_B;
    uint32_t Blocks;
} mtdInfo_t;

// ---------------------------------------------------------------------------
// Operation-queue message types -- the service API enqueues these and the task
// dispatches on them. File-visible (not class-private) so they read cleanly in
// the queue-drain switch; they are otherwise pure implementation detail.
// ---------------------------------------------------------------------------
typedef enum {
    MTD_PHY_READ,
    MTD_PHY_WRITE,
    MTD_PHY_ERASE,
    MTD_PHY_READ_META,
    MTD_PHY_WRITE_META,
    MTD_PHY_COPY
} MTD_OPAS;

typedef struct
{
    MTD_OPAS opa;
    uint32_t page;
    uint32_t copyDstPage;
    uint32_t offset;
    uint8_t *buf;
    uint8_t *metaDat;
    uint32_t len;
    bool needToMoveData;
    TaskHandle_t task;
} MTD_Operates;

class Mtd {
public:
    // ---- lifecycle ----------------------------------------------------------
    static void interfaceInit();   // bring up the GPMI interface (boardInit)
    static void deviceInit();      // create the queue + probe the NAND geometry
    static void task();            // the MTD task: drains the op queue forever
    static bool isDeviceInited();
    static mtdInfo_t *getDeviceInfo();

    // ---- GPMI completion bridge --------------------------------------------
    // The three GPMI ISRs call this by name to latch the ECC result and release
    // the task blocked in the op loop. Public because it is the driver->service
    // seam; it only touches this class's own state so no friendship is needed.
    static bool upOpaFin(uint32_t eccResult);

    // ---- synchronous physical operations (enqueue, then block on notify) ----
    static int readPhyPage(uint32_t page, uint32_t offset, uint32_t len, uint8_t *buffer);
    static int writePhyPage(uint32_t page, uint8_t *buffer);
    static int erasePhyBlock(uint32_t block);
    static int writePhyPageWithMeta(uint32_t page, uint32_t meta_len, uint8_t *buffer, uint8_t *meta);
    static int readPhyPageMeta(uint32_t page, uint32_t len, uint8_t *buffer);
    static int copyPhyPage(uint32_t srcPage, uint32_t dstPage);
    static int eraseAllBlock();

private:
    // ---- encapsulated file-scope state (zero-init .bss, no global ctor) ------
    // One object so this co-accessed block keeps single base-register + offset
    // codegen (see file header; mirrors Gpmi::st).
    struct State {
        QueueHandle_t opQueue;
        mtdInfo_t     info;
        MTD_Operates  curOpa;
        bool          deviceInited;
        uint32_t      eccResult;
        volatile bool opaDone;
        uint32_t      retryCnt;
    };
    static inline State st{};

    // The page-staging buffer is large and always addressed by its own base, so
    // it stays a separate member rather than bloating every State offset.
    static inline uint8_t pageBuffer[2048];

    // Write-only diagnostic latch (last physically-read page). Kept separate and
    // .data-initialised to match the previous free global exactly.
    static inline uint32_t lastReadPage = 0xFFFFFFFF;
};
