/**
 * @file Bootloader/Hal/FTL_up.h
 * @brief FTL_up module
 */

#ifndef __FTL_UP_H__
#define __FTL_UP_H__


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "SystemConfig.h"

#include <stdint.h>
#include <stdbool.h>
// Dhara is a vendored C library with no extern "C" guards of its own. Pull its
// headers under C linkage so this (now C++) TU both calls dhara_map_*/dhara_strerror
// with the right names and exports its dhara_nand_* callbacks unmangled for Dhara.
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

// The FTL API is called by name from still-C TUs (vmMgr.c, msc_disk.c,
// start.c, llapi.c, ftl_service.c). Keep C linkage so those callers resolve
// the unmangled symbols even though FTL_up is now compiled as C++.
#ifdef __cplusplus
extern "C" {
#endif

int FTL_init(void);
int FTL_MapInit(void);
bool FTL_inited(void);
void FTL_task(void);

int FTL_GetSectorCount(void);
int FTL_GetSectorSize(void);

int FTL_ReadSector(uint32_t sector, uint32_t num, uint8_t *buf);
int FTL_WriteSector(uint32_t sector, uint32_t num, uint8_t *buf);
int FTL_TrimSector(uint32_t sector);
int FTL_Sync(void);
void FTL_ClearAllSector(void);

bool FTL_ScanPartition(void);
PartitionInfo_t *FTL_GetPartitionInfo(void);

#ifdef __cplusplus
}
#endif

#endif