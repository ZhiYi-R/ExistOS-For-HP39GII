/**
 * @file Bootloader/Hal/FTL_up.cpp
 * @brief Ftl class method bodies + the extern "C" dhara_nand_* seam (Phase 3.5c).
 *
 * The class declaration and the Dhara/Mtd seam rationale live in FTL_up.hpp.
 */

#include "FTL_up.hpp"
#include "debug.h"

#include <stdio.h>
#include <string.h>

//#define PR_FTL_TIMING_STATUS
#ifdef PR_FTL_TIMING_STATUS
#include "reg_model.hpp"
#endif

static int _pow(int x, int y) // x^y
{
    int res = x;
    for (int i = 1; i < y; i++) {
        res = res * x;
    }
    return res;
}

static int _log2(int x) // log2(x)
{
    int res = 0;
    int r = x;
    while (r > 1) {
        r = r / 2;
        ++res;
    }
    return res;
}

// ---------------------------------------------------------------------------
// Dhara NAND page primitives. Dhara (vendored C) dispatches these by name, so
// they are free extern "C" functions (C linkage from the wrapped map.h include
// in FTL_up.hpp) and reach Ftl's private geometry / scratch state via friendship.
// ---------------------------------------------------------------------------
int dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b) {
    uint32_t ret = 0;
    ret = Mtd::readPhyPageMeta((DATA_START_BLOCK + b) * Ftl::st.mtdinfo->PagesPerBlock, 4, (uint8_t *)&Ftl::st.meta);
    // printf("TEST BAD\n");
    if ((ret == -1) || (Ftl::st.meta == BAD_BLOCK)) {

        printf("Found BAD Block:%u\n", DATA_START_BLOCK + b);
        return 1;
    }
    return 0;
}

void dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b) {
    Ftl::st.meta = BAD_BLOCK;
    printf("MARK BAD BLOCK\n");
    uint8_t *tempbuf = (uint8_t *)pvPortMalloc(2048);
    uint8_t *tempmeta = (uint8_t *)pvPortMalloc(19);
    if (!tempbuf || !tempmeta) {
        printf("MALLOC FAIL\n");
        if (tempbuf) vPortFree(tempbuf);
        if (tempmeta) vPortFree(tempmeta);
        return;
    }
    Mtd::readPhyPage((DATA_START_BLOCK + b) * Ftl::st.mtdinfo->PagesPerBlock, 0, 2048, tempbuf);
    memset(tempmeta, 0, 19);
    *((uint32_t *)&tempmeta[0]) = BAD_BLOCK;
    Mtd::writePhyPageWithMeta((DATA_START_BLOCK + b) * Ftl::st.mtdinfo->PagesPerBlock, 4, tempbuf, tempmeta);
    // MTD_WritePhyPageMeta((DATA_START_BLOCK + b) * _pow(2, n->log2_ppb), 4, (uint8_t *)&meta);
    vPortFree(tempbuf);
    vPortFree(tempmeta);
}

int dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b,
                     dhara_error_t *err) {
    int ret = 0;
    ret = Mtd::erasePhyBlock(DATA_START_BLOCK + b);
    // printf("ERASE ret %d, block:%d\n",ret,b);
    *err = DHARA_E_NONE;
    if (ret) {
        *err = DHARA_E_BAD_BLOCK;
        printf("ERASE ERR\n");
    }
    return ret;
}

int dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p,
                    const uint8_t *data,
                    dhara_error_t *err) {
    int ret = 0;
    uint32_t metadata = DATA_BLOCK;
    // printf("PROG page:%d, data:%p\n",p, data);
    // ret = Mtd::writePhyPage(p + (DATA_START_BLOCK *  Ftl::st.mtdinfo->PagesPerBlock) , (uint8_t *)data);
    ret = Mtd::writePhyPageWithMeta(
        p + (DATA_START_BLOCK * Ftl::st.mtdinfo->PagesPerBlock),
        4,
        (uint8_t *)data,
        (uint8_t *)&metadata);

    // printf("ret %d.PROG page:%d, data:%p\n",ret, p, data);
    *err = DHARA_E_NONE;
    if (ret) {
        *err = DHARA_E_BAD_BLOCK;
        printf("PROG ERR\n");
    }

    return ret;
}

int dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p) {
    int ret = 0;
    ret = Mtd::readPhyPage(p + (DATA_START_BLOCK * Ftl::st.mtdinfo->PagesPerBlock), 0, _pow(2, n->log2_page_size), NULL);
    // printf("is_free %d\n",ret);
    return (ret == 1);
}

int dhara_nand_read(const struct dhara_nand *n, dhara_page_t p,
                    size_t offset, size_t length,
                    uint8_t *data,
                    dhara_error_t *err) {
    int ret = 0;
    // printf("start READ page:%d, offset:%d, len:%d, buff:%p\n", p,offset,length,data);
    ret = Mtd::readPhyPage(p + (DATA_START_BLOCK * Ftl::st.mtdinfo->PagesPerBlock), offset, length, data);

    // printf("READ END\n");
    /*
    for(int i=0;i<64; i++){
        printf("%02X ", data[i]);
    }
    printf("\n");*/
    *err = DHARA_E_NONE;
    if (ret < 0) {
        *err = DHARA_E_ECC;
        printf("READ ERR\n");
        return ret;
    }
    /* ECC-scrub hook (not implemented):
     * Mtd::readPhyPage() currently collapses every correctable read to ret==0,
     * so the per-read bit-correction count is not visible here. Scrubbing must
     * also run out-of-band (re-entering dhara_map_* from this callback is
     * forbidden). The actual refresh hook therefore lives in Ftl::task()'s
     * FTL_SECTOR_READ branch; see the note there and mtd_up.cpp (ECCResult /
     * g_mtd_ecc_cnt) for the correction count that would need to be exposed. */
    return 0;
}

int dhara_nand_copy(const struct dhara_nand *n,
                    dhara_page_t src, dhara_page_t dst,
                    dhara_error_t *err) {
    int ret = 0;
    // printf("COPY SRC %d dst %d\n",src,dst);
    /*
    ret = Mtd::copyPhyPage(src + (DATA_START_BLOCK *  Ftl::st.mtdinfo->PagesPerBlock),
                          dst + (DATA_START_BLOCK *  Ftl::st.mtdinfo->PagesPerBlock));*/

    uint8_t *CopyBuffer = (uint8_t *)pvPortMalloc(2048);

    *err = DHARA_E_NONE;

    ret = Mtd::readPhyPage(src + (DATA_START_BLOCK * Ftl::st.mtdinfo->PagesPerBlock), 0, Ftl::st.mtdinfo->PageSize_B, (uint8_t *)CopyBuffer);
    if (ret < 0) {
        *err = DHARA_E_ECC;
        printf("COPY RD ERR\n");
        vPortFree(CopyBuffer);
        return -1;
    }
    ret = Mtd::writePhyPage(dst + (DATA_START_BLOCK * Ftl::st.mtdinfo->PagesPerBlock), (uint8_t *)CopyBuffer);

    if (ret) {
        *err = DHARA_E_BAD_BLOCK;
        printf("COPY WR ERR\n");
    }

    vPortFree(CopyBuffer);

    return ret;
}

//===================================================================================

// static EventGroupHandle_t FTLLockEventGroup;

// static int32_t FTLStatusBuf[32];
// static int32_t pFTLStatus = 0;

// static int32_t FTLLockBit = 0;

bool Ftl::isInited() {
    return st.inited;
}

void Ftl::clearAllSector() {
    dhara_map_clear(&st.ftlMap);
}

int Ftl::mapInit() {
    dhara_error_t err;
    int ret = 0;
    dhara_map_init(&st.ftlMap, &st.nandDevice, PageBuffer, GC_RATIO);
    err = (dhara_error_t)0;
    ret = dhara_map_resume(&st.ftlMap, &err);
    INFO("Resume FTL: %d\n", ret);

    st.maxFtlPages = dhara_map_capacity(&st.ftlMap);
    INFO("FTL capacity %u/%u (%u K/ %u K)\n", dhara_map_size(&st.ftlMap), st.maxFtlPages, dhara_map_size(&st.ftlMap) * st.mtdinfo->PageSize_B / 1024, dhara_map_capacity(&st.ftlMap) * st.mtdinfo->PageSize_B / 1024);

    return ret;
}

int Ftl::init() {
    int ret = 0;

    st.opQueue = xQueueCreate(32, sizeof(FTL_Operates));
    // FTLLockEventGroup = xEventGroupCreate();

    while (!Mtd::isDeviceInited()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    st.mtdinfo = Mtd::getDeviceInfo();

    st.nandDevice.num_blocks = st.mtdinfo->Blocks - DATA_START_BLOCK;
    st.nandDevice.log2_page_size = _log2(st.mtdinfo->PageSize_B);
    st.nandDevice.log2_ppb = _log2(st.mtdinfo->PagesPerBlock);

    INFO("FTL num_blocks:%d\n", st.nandDevice.num_blocks);
    INFO("FTL log2_page_size:%d\n", st.nandDevice.log2_page_size);
    INFO("FTL log2_ppb:%d\n", st.nandDevice.log2_ppb);

    ret = mapInit();

    // err = 0;
    // ret = dhara_map_sync(&FTLmap, &err);
    // INFO("Sync FTL: %d,%s\n",ret,dhara_strerror(err));

    /*
        FTLLockBit = 0;
        for(int i=0; i<(sizeof(FTLStatusBuf) / sizeof(int32_t)); i++){
            FTLStatusBuf[i] = -5;
        }
        pFTLStatus = 0;
    */

    st.inited = true;

    return ret;
}

void Ftl::task() {
    dhara_error_t err;
    int ret = 0;
    while (1) {
        if (xQueueReceive(st.opQueue, &st.curOpa, portMAX_DELAY) == pdTRUE) {
            switch (st.curOpa.opa) {
            case FTL_SECTOR_READ:
                for (int i = 0; i < st.curOpa.num; i++) {
                    #ifdef PR_FTL_TIMING_STATUS
                    st.rdt = reg::DIGCTL_MICROSECONDS::rd();
                    #endif
                    ret = dhara_map_read(&st.ftlMap, st.curOpa.sector++, st.curOpa.buf, &err);
                    #ifdef PR_FTL_TIMING_STATUS
                    INFO("frd=%ld\n",reg::DIGCTL_MICROSECONDS::rd() - st.rdt);
                    #endif
                    /* ECC-scrub hook (not implemented this round):
                     * The sector just read is (st.curOpa.sector - 1). To refresh a
                     * page whose ECC correction count crossed a threshold, rewrite
                     * it here via dhara_map_write(&st.ftlMap, st.curOpa.sector - 1,
                     * st.curOpa.buf, &err) -- this runs in Ftl::task context, safely
                     * out of the Dhara NAND callbacks. Gate it on a real metric:
                     * MTD must first expose the per-read correction count
                     * (mtd_up.cpp ECCResult / g_mtd_ecc_cnt), which it currently
                     * does not surface through Mtd::readPhyPage(). */
                    st.curOpa.buf += st.mtdinfo->PageSize_B;
                    if (ret) {
                        FTL_WARN("FTL READ FAIL:%d,%s\n", ret, dhara_strerror(err));
                        break;
                    }
                }
                //*st.curOpa.StatusBuf = ret;
                xTaskNotify(st.curOpa.task, ret, eSetValueWithOverwrite);
                break;

            case FTL_SECTOR_WRITE:
                for (int i = 0; i < st.curOpa.num; i++) {
                    #ifdef PR_FTL_TIMING_STATUS
                    st.wrt = reg::DIGCTL_MICROSECONDS::rd();
                    #endif
                    ret = dhara_map_write(&st.ftlMap, st.curOpa.sector++, st.curOpa.buf, &err);
                    #ifdef PR_FTL_TIMING_STATUS
                    INFO("fwr=%ld\n",reg::DIGCTL_MICROSECONDS::rd() - st.wrt);
                    #endif
                    st.curOpa.buf += st.mtdinfo->PageSize_B;
                    if (ret) {
                        FTL_WARN("FTL WRITE FAIL:%d,%s\n", ret, dhara_strerror(err));
                        break;
                    }
                }

                //*st.curOpa.StatusBuf = ret;
                xTaskNotify(st.curOpa.task, ret, eSetValueWithOverwrite);
                break;

            case FTL_SECTOR_TRIM:
                ret = dhara_map_trim(&st.ftlMap, st.curOpa.sector, &err);
                //*st.curOpa.StatusBuf = ret;
                xTaskNotify(st.curOpa.task, ret, eSetValueWithOverwrite);
                break;

            case FTL_SYNC:
                ret = dhara_map_sync(&st.ftlMap, &err);
                if (ret) {
                    FTL_WARN("FTL SYNC FAIL:%d,%s\n", ret, dhara_strerror(err));
                }
                //*st.curOpa.StatusBuf = ret;
                xTaskNotify(st.curOpa.task, ret, eSetValueWithOverwrite);
                break;

            default:
                break;
            }

            // xEventGroupSetBits(FTLLockEventGroup , (1 << st.curOpa.BLock));
        }
    }
}

int Ftl::getSectorCount() {
    return dhara_map_capacity(&st.ftlMap);
}

int Ftl::getSectorSize() {
    return st.mtdinfo->PageSize_B;
}

int Ftl::readSector(uint32_t sector, uint32_t num, uint8_t *buf) {

    FTL_Operates newOpa;
    int retVal = 0;
    if (!isInited()) {
        INFO("FTL Not Inited.\n");
        return -1;
    }

    if (sector + num > st.maxFtlPages) {
        INFO("sector + num > max_ftl_pages, %u, %u, %u\n", sector, num, st.maxFtlPages);
        return -1;
    }

    newOpa.opa = FTL_SECTOR_READ;
    newOpa.sector = sector;
    newOpa.num = num;
    newOpa.buf = buf;
    newOpa.task = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    return retVal;
}

int Ftl::writeSector(uint32_t sector, uint32_t num, uint8_t *buf) {
    FTL_Operates newOpa;
    int retVal = 0;
    if (!isInited()) {
        return -1;
    }

    if (sector + num > st.maxFtlPages) {
        return -1;
    }

    newOpa.opa = FTL_SECTOR_WRITE;
    newOpa.sector = sector;
    newOpa.num = num;
    newOpa.buf = buf;
    // newOpa.BLock = FTL_getLock();
    // newOpa.StatusBuf = FTL_GetStatusBuf();
    newOpa.task = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);
    /*
        xEventGroupWaitBits(FTLLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
        retVal = *newOpa.StatusBuf;
        *newOpa.StatusBuf = -5;
        */
    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    return retVal;
}

int Ftl::trimSector(uint32_t sector) {
    FTL_Operates newOpa;
    int retVal = 0;
    if (!isInited()) {
        INFO("FTL Not Inited.\n");
        return -1;
    }

    newOpa.opa = FTL_SECTOR_TRIM;
    newOpa.sector = sector;

    // newOpa.BLock = FTL_getLock();
    // newOpa.StatusBuf = FTL_GetStatusBuf();
    newOpa.task = xTaskGetCurrentTaskHandle();

    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);

    /*
        xEventGroupWaitBits(FTLLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
        retVal = *newOpa.StatusBuf;
        *newOpa.StatusBuf = -5;
        */
    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);
    return retVal;
}

int Ftl::sync() {
    //FTL_Operates newOpa;
    dhara_error_t err;
    int ret = 0;

    int retVal = 0;
    if (!isInited()) {
        return -1;
    }

    ret = dhara_map_sync(&st.ftlMap, &err);
    if (ret) {
        FTL_WARN("FTL SYNC FAIL:%d,%s\n", ret, dhara_strerror(err));
    }
    INFO("Sync.\n");
    return ret;

    /*
        newOpa.opa = FTL_SYNC;
        newOpa.task = xTaskGetCurrentTaskHandle();
        //newOpa.BLock = FTL_getLock();
        //newOpa.StatusBuf = FTL_GetStatusBuf();
        xTaskNotifyStateClear(NULL);
        xQueueSend(FTL_Operates_Queue, &newOpa, portMAX_DELAY);


        xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);
    */

    return retVal;
}

PartitionInfo_t *Ftl::getPartitionInfo() {
    return st.partitionInfo;
}

/*
bool Ftl::scanPartition()
{
  uint8_t *buf;
  uint32_t SectorStart[4];
  uint32_t Sectors[4];

    if(st.partitionInfo == NULL){
        st.partitionInfo = pvPortMalloc(sizeof(PartitionInfo_t));
      }
    memset(st.partitionInfo, 0, sizeof(PartitionInfo_t));
  buf = pvPortMalloc(getSectorSize());
  readSector(0, 1, buf);

  if((buf[0x1FE] != 0x55) || (buf[0x1FF] != 0xAA)){
    return false;
  }

  for(int i = 0; i < 4 ; i++){
    memcpy(&SectorStart[i],(void *) (((uint32_t)&buf[0x1c6]) + 0x10 * i)  , 4);
    memcpy(&Sectors[i],(void *) (((uint32_t)&buf[0x1ca]) + 0x10 * i)  , 4);
  }
  for(int i = 0; i < 4 ; i++){
    if((SectorStart[i] < getSectorCount()) && (Sectors[i] > 0)){
      INFO("DISK PART[%d], Start Sector:%u, Size:%ld\n",i,SectorStart[i], Sectors[i] * getSectorSize());
      st.partitionInfo->Partitions++;
      st.partitionInfo->Sectors[i] = Sectors[i];
      st.partitionInfo->SectorStart[i] = SectorStart[i];
    }
  }

  if(st.partitionInfo->Partitions > 1){
    return true;
  }
  return false;
}
*/
