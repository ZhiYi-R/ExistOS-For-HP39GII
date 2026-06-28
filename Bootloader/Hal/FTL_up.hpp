/**
 * @file Bootloader/Hal/FTL_up.hpp
 * @brief Flash Translation Layer — pure-static singleton class over Dhara + Mtd.
 *
 * Phase 3.5c of the HAL C++23 migration folds the former free `FTL_*()` API and
 * its file-scope state into the pure-static @c Ftl class. The layer sits on top
 * of the @c Mtd NAND service (mtd_up.hpp) and the vendored Dhara map/journal.
 *
 * Dhara seam: Dhara is a vendored C library with no @c extern "C" guards of its
 * own, and its map/journal layers call the seven @c dhara_nand_* page primitives
 * @e by @e name. Those callbacks therefore stay free @c extern "C" functions
 * (declared via map.h under C linkage below) and are granted @c friend access so
 * they can reach @c Ftl's private NAND geometry / scratch state directly — the
 * same hard-seam treatment the GPMI ISRs get in stmp_gpmi.hpp. The FTL stays
 * Dhara-native; no custom remap layer (see the rejected spare-block experiment).
 */
#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "SystemConfig.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mtd_up.hpp"   // mtdInfo_t + Mtd::*

// Dhara has no extern "C" guards of its own. Pull its headers under C linkage so
// this (C++) TU both calls dhara_map_*/dhara_strerror with the right names and
// exports its dhara_nand_* callbacks unmangled for Dhara to dispatch by name.
#ifdef __cplusplus
extern "C" {
#endif
#include "map.h"
#ifdef __cplusplus
}
#endif

#define BAD_BLOCK           (0)
#define DATA_BLOCK          (0xDADADADA)
#define GOOD_BLOCK          (0xFFFFFFFF)

#define DATA_START_BLOCK    FLASH_DATA_BLOCK
#define GC_RATIO        6

typedef enum {
    FTL_SECTOR_READ,
    FTL_SECTOR_WRITE,
    FTL_SECTOR_TRIM,
    FTL_SYNC

}FTL_OPAS;

typedef struct
{
    FTL_OPAS opa;
    uint32_t sector;
    uint32_t num;
    uint8_t *buf;
    //EventBits_t BLock;
    //int32_t *StatusBuf;
    TaskHandle_t task;
}FTL_Operates;

typedef struct PartitionInfo_t
{
  uint32_t Partitions;
  uint32_t SectorStart[4];
  uint32_t Sectors[4];
}PartitionInfo_t;

// ---------------------------------------------------------------------------
// Dhara NAND callbacks: dispatched by name from the vendored Dhara map/journal
// (C, no extern "C" guard of its own), so they keep C linkage and unmangled
// names. They are declared by the wrapped map.h include above; Ftl friends them
// below and they are defined out-of-line in FTL_up.cpp where they reach Ftl::st.
// ---------------------------------------------------------------------------

class Ftl {
public:
    static int  init();
    static int  mapInit();
    static bool isInited();
    static void task();

    static int  getSectorCount();
    static int  getSectorSize();

    static int  readSector(uint32_t sector, uint32_t num, uint8_t *buf);
    static int  writeSector(uint32_t sector, uint32_t num, uint8_t *buf);
    static int  trimSector(uint32_t sector);
    static int  sync();
    static void clearAllSector();

    static bool scanPartition();
    static PartitionInfo_t *getPartitionInfo();

private:
    // ---- shared state (was file-scope statics) --------------------------
    // Grouped into one object so this originally-contiguous .bss block keeps its
    // section-anchor base+offset codegen: separate `static inline` members are
    // distinct COMDAT symbols the compiler cannot prove adjacent, so each co-
    // accessed scalar would otherwise cost its own address load. Every field is
    // zero-initialised, so the aggregate lands in .bss with no global ctor. The
    // dhara_nand_* friends reach these as Ftl::st.* .
    struct State {
        QueueHandle_t      opQueue;       // was FTL_Operates_Queue
        mtdInfo_t         *mtdinfo;       // was pMtdinfo
        struct dhara_nand  nandDevice;
        struct dhara_map   ftlMap;        // was FTLmap
        PartitionInfo_t   *partitionInfo; // was PartitionInfo
        FTL_Operates       curOpa;
        uint32_t           maxFtlPages;   // was max_ftl_pages
        uint32_t           meta;          // dhara_nand_is_bad/mark_bad scratch
        bool               inited;
#ifdef PR_FTL_TIMING_STATUS
        uint32_t           rdt;           // was ftl_rdt
        uint32_t           wrt;           // was ftl_wrt
#endif
    };
    static inline State st{};

    // Dhara map's page-scratch buffer (kept as its own aligned symbol, matching
    // the baseline file-scope static it replaces).
    static inline uint8_t PageBuffer[2048] __attribute__((aligned(4)));

    // ---- Dhara seam friends: the seven page primitives reach Ftl::st ----
    friend int  ::dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b);
    friend void ::dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b);
    friend int  ::dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b, dhara_error_t *err);
    friend int  ::dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p, const uint8_t *data, dhara_error_t *err);
    friend int  ::dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p);
    friend int  ::dhara_nand_read(const struct dhara_nand *n, dhara_page_t p, size_t offset, size_t length, uint8_t *data, dhara_error_t *err);
    friend int  ::dhara_nand_copy(const struct dhara_nand *n, dhara_page_t src, dhara_page_t dst, dhara_error_t *err);
};
