/**
 * @file Bootloader/Hal/mtd_up.cpp
 * @brief MTD service layer — @c Mtd method bodies over the @c Gpmi driver.
 *
 * Phase 3.5b fold: the former free @c MTD_*() service functions are now the
 * out-of-line definitions of the pure-static @c Mtd class (mtd_up.hpp), and the
 * physical layer is driven through @c Gpmi:: directly — the old @c portMTD*
 * extern "C" forwarding shims are gone. Bodies are otherwise unchanged: each
 * synchronous op still marshals an @c MTD_Operates onto @c st.opQueue and blocks
 * on a task notification, and @c Mtd::task() drains the queue. The three GPMI
 * ISRs call @c Mtd::upOpaFin() (defined here) to latch the ECC result and wake
 * the waiter.
 *
 * The @c g_mtd_*_cnt counters stay free globals: start.cpp's flash-stats dump
 * reads them through @c extern declarations, so they must keep plain symbols.
 */

#include "SystemConfig.h"
#include "FreeRTOS.h"

#include <stdio.h>
#include <string.h>

#include "stmp_clkctrl.hpp"

#include "mtd_up.hpp"
#include "stmp_gpmi.hpp"
#include "nand.h"
#include "debug.h"

// Flash-activity counters — read elsewhere (start.cpp) via extern, so free
// globals rather than Mtd members (folding them in would mangle the symbols).
uint32_t g_mtd_write_cnt = 0;
uint32_t g_mtd_read_cnt = 0;
uint32_t g_mtd_erase_cnt = 0;
uint32_t g_mtd_ecc_cnt = 0;
uint32_t g_mtd_ecc_fatal_cnt = 0;

void Mtd::interfaceInit()
{
    Gpmi::interfaceInit();
}

bool Mtd::isDeviceInited()
{
    return st.deviceInited;
}

mtdInfo_t *Mtd::getDeviceInfo()
{
    return &st.info;
}

bool Mtd::upOpaFin(uint32_t eccResult)
{
    BaseType_t flag = false;
    st.eccResult = eccResult;
    st.opaDone = true;
    //xEventGroupSetBitsFromISR(MTDDriverOpaDone, 1, &flag);
    return flag != 0;

}

void Mtd::task()
{
    //vTaskDelay(pdMS_TO_TICKS(1000));
    while(1){

        if(xQueueReceive(st.opQueue, &st.curOpa, portMAX_DELAY) == pdTRUE)
        {
            Clk::enterSlow();


            st.retryCnt = 5;
            retry:

            MTD_INFO("MTD REC OPA\n");
            st.opaDone = false;

            switch (st.curOpa.opa)
            {

            case MTD_PHY_READ:

                MTD_INFO_READ("QUEUE READ, page:%d, buf:%p, len: %d, needMove:%d\n",st.curOpa.page, st.curOpa.buf, st.curOpa.len, st.curOpa.needToMoveData);

                if(st.curOpa.needToMoveData)
                {
                    MTD_INFO_READ("Move Dat Read,page:%d,%p\n",st.curOpa.page,pageBuffer);
                    Gpmi::readPage(st.curOpa.page, pageBuffer);
                }else{
                    Gpmi::readPage(st.curOpa.page, st.curOpa.buf);
                }

                break;
            case MTD_PHY_READ_META:
                MTD_INFO_READ("MTD READ META:page:%d\n", st.curOpa.page);
                Gpmi::readPage(st.curOpa.page, NULL);
                break;


            case MTD_PHY_ERASE:
                MTD_INFO_ERASE("MTD ERASE:b %d\n", st.curOpa.page);
                Gpmi::eraseBlock(st.curOpa.page);
                break;
            case MTD_PHY_WRITE:
                MTD_INFO_WRITE("MTD WRITE, page:%d,data:%08x, move:%d\n", st.curOpa.page, st.curOpa.buf, st.curOpa.needToMoveData);
                if(st.curOpa.needToMoveData)
                {
                    memcpy(pageBuffer, st.curOpa.buf, st.info.PageSize_B);
                    Gpmi::writePage(st.curOpa.page, pageBuffer);

                }else{
                    Gpmi::writePage(st.curOpa.page, st.curOpa.buf);
                }

                break;
            case MTD_PHY_WRITE_META:
                MTD_INFO_WRITE("MTD WRITE META, page:%d,data:%08x, len:%d\n", st.curOpa.page, st.curOpa.metaDat, st.curOpa.len);
                memset(Gpmi::getMetaData(), 0xFF, st.info.MetaSize_B);
                memcpy(Gpmi::getMetaData(), st.curOpa.metaDat, st.curOpa.len);

                if(st.curOpa.needToMoveData)
                {
                    memcpy(pageBuffer, st.curOpa.buf, st.info.PageSize_B);
                    Gpmi::writePageMeta(st.curOpa.page, pageBuffer, Gpmi::getMetaData());

                }else{
                    Gpmi::writePageMeta(st.curOpa.page, st.curOpa.buf, Gpmi::getMetaData());
                }


                break;

            case MTD_PHY_COPY:
                Gpmi::copyPage(st.curOpa.page, st.curOpa.copyDstPage);
                break;

            default:
                MTD_WARN("UNEXPECTED MTD REC OPA!\n");
                break;
            }
            uint32_t start_tick = xTaskGetTickCount();
            while((int)st.opaDone == false)
            {
                if(xTaskGetTickCount() - start_tick > pdMS_TO_TICKS(2000)){
                    INFO("MTD Waiting Timeout! %u\n", st.retryCnt);
                    INFO("Cur opa:%d\n", st.curOpa.opa);
                    INFO("Cur opa.page:%u\n", st.curOpa.page);
                    INFO("Cur opa.offset:%u\n", st.curOpa.offset);
                    INFO("Cur opa.needToMoveData:%d\n", st.curOpa.needToMoveData);
                    INFO("Cur opa.buf:%p\n", st.curOpa.buf);
                    if(st.retryCnt)
                    {
                        st.retryCnt--;
                        goto retry;
                    }else{
                        st.opaDone = true;
                        st.eccResult = 0x0E0E0E0E;
                    }

                    //while(1);


                }


            }
            //xEventGroupWaitBits(MTDDriverOpaDone, 1, pdTRUE, pdFALSE, portMAX_DELAY);


            switch (st.curOpa.opa)
            {
            case MTD_PHY_READ_META:
                memcpy(st.curOpa.buf, Gpmi::getMetaData(), st.curOpa.len);
            case MTD_PHY_READ:
                if(st.curOpa.needToMoveData)
                {
                    if(st.curOpa.len > st.info.PageSize_B - st.curOpa.offset){
                        MTD_WARN("MTD READ DATA Corrupted.page:%u len:%u offset:%u\n",st.curOpa.page, st.curOpa.len, st.curOpa.offset);
                        st.curOpa.len = st.info.PageSize_B - st.curOpa.offset;
                    }
                    MTD_INFO("Read Move Data,dst:%p,src:%p,len:%d\n",st.curOpa.buf, pageBuffer + st.curOpa.offset, st.curOpa.len);
                    memcpy(st.curOpa.buf, pageBuffer + st.curOpa.offset, st.curOpa.len);
                }

                if(
                    (((st.eccResult ) & 0xF)      == 0xE) ||
                    (((st.eccResult >> 8) & 0xF)  == 0xE) ||
                    (((st.eccResult >> 16) & 0xF) == 0xE) ||
                    (((st.eccResult >> 24) & 0xF) == 0xE)
                ){
                    MTD_WARN("BAD BLOCK:%u\n", st.curOpa.page);
                    //*st.curOpa.StatusBuf =  -1;
                    xTaskNotify(st.curOpa.task, -1, eSetValueWithOverwrite);
                }else if(st.eccResult == 0x0F0F0F0F){
                    xTaskNotify(st.curOpa.task, 1, eSetValueWithOverwrite);
                    MTD_INFO_READ("EMPTY PAGE\n");
                    //*st.curOpa.StatusBuf = 1;          //EMPTY
                }else {
                    xTaskNotify(st.curOpa.task, 0, eSetValueWithOverwrite);
                    //*st.curOpa.StatusBuf = 0;
                }

                if((st.eccResult > 1) && (st.eccResult < 0x0F0F0F0F)){
                //if((st.eccResult != 0))
                    //printf("ECC Err found:%08lX, PhySector:%ld\n",st.eccResult, st.curOpa.page);
                    g_mtd_ecc_cnt++;

                }
                lastReadPage = st.curOpa.page;

                break;



                case MTD_PHY_ERASE:
                    //*st.curOpa.StatusBuf = st.eccResult;
                    xTaskNotify(st.curOpa.task, st.eccResult, eSetValueWithOverwrite);
                    break;

                case MTD_PHY_WRITE_META:
                case MTD_PHY_WRITE:
                    xTaskNotify(st.curOpa.task, st.eccResult, eSetValueWithOverwrite);
                    //*st.curOpa.StatusBuf = st.eccResult;
                    break;

                case MTD_PHY_COPY:
                    if(
                        (((st.eccResult ) & 0xF)      == 0xE) ||
                        (((st.eccResult >> 8) & 0xF)  == 0xE) ||
                        (((st.eccResult >> 16) & 0xF) == 0xE) ||
                        (((st.eccResult >> 24) & 0xF) == 0xE)
                    ){
                        g_mtd_ecc_fatal_cnt++;
                        MTD_WARN("BAD BLOCK:%u\n", st.curOpa.page);
                        //*st.curOpa.StatusBuf =  -1;
                        xTaskNotify(st.curOpa.task, -1, eSetValueWithOverwrite);
                    }else if(st.eccResult == 0x0F0F0F0F){
                        xTaskNotify(st.curOpa.task, 0, eSetValueWithOverwrite);
                        //*st.curOpa.StatusBuf = 0;      //EMPTY
                    }else {
                        xTaskNotify(st.curOpa.task, 0, eSetValueWithOverwrite);
                        //*st.curOpa.StatusBuf = 0;
                    }

                    break;

            default:
                break;
            }

            //xEventGroupSetBits(MTDLockEventGroup , (1 << st.curOpa.BLock));
            Clk::exitSlow();

        }

    }
}
/**
static EventBits_t MTD_getLock()
{
    uint32_t bit;
    EventBits_t GroupBits;
    EventBits_t curBits;

    taskENTER_CRITICAL();

    GroupBits = xEventGroupGetBits(MTDLockEventGroup);

    curBits = (GroupBits >> MTDLockBit) & 1;
    while(curBits == 1)
    {
        GroupBits = xEventGroupGetBits(MTDLockEventGroup);
        MTDLockBit++;
        if(MTDLockBit > 23)
        {
            MTDLockBit = 0;
        }
        curBits = (GroupBits >> MTDLockBit) & 1;
    }

    bit = MTDLockBit;
    xEventGroupClearBits(MTDLockEventGroup, 1 << bit);

    MTDLockBit++;
    if(MTDLockBit > 23)
    {
        MTDLockBit = 0;
    }

    taskEXIT_CRITICAL();
    return bit;

}

static uint32_t *MTD_GetStatusBuf()
{
    int32_t *StatusBuf;
    taskENTER_CRITICAL();

    StatusBuf = &MTDStatusBuf[pMTDStatus];
    while(*StatusBuf != -5){
        pMTDStatus++;
        if(pMTDStatus >= (sizeof(MTDStatusBuf)/sizeof(int32_t))){
            pMTDStatus = 0;
        }
        StatusBuf = &MTDStatusBuf[pMTDStatus];
    }

    *StatusBuf = 0;

    pMTDStatus++;
    if(pMTDStatus >= (sizeof(MTDStatusBuf)/sizeof(int32_t))){
        pMTDStatus = 0;
    }
    //MTD_INFO("pMTDStatus:%d\n",pMTDStatus);
    taskEXIT_CRITICAL();
    return StatusBuf;
}

*/

int Mtd::readPhyPage(uint32_t page, uint32_t offset, uint32_t len, uint8_t *buffer)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_READ;
    newOpa.page = page;
    newOpa.offset = offset;
    newOpa.buf = buffer;
    newOpa.len = len;
    newOpa.needToMoveData = false;
    newOpa.task = xTaskGetCurrentTaskHandle();


    if((offset != 0) || (len != st.info.PageSize_B) || (((uint32_t)buffer & 3) != 0)){
        newOpa.needToMoveData = true;
    }
    if(buffer == NULL){
        newOpa.needToMoveData = false;
    }
    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }
    MTD_INFO("POST READ CMD, queue num:%lx\n",uxQueueGetQueueNumber(st.opQueue));

    g_mtd_read_cnt++;
    xTaskNotifyStateClear(NULL);

    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);

    MTD_INFO("POST READ CMD END:%lx\n", uxQueueGetQueueNumber(st.opQueue));

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    MTD_INFO("POST READ RET:%d\n", retVal);
    return retVal;
}

int Mtd::writePhyPage(uint32_t page,uint8_t *buffer)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_WRITE;
    newOpa.page = page;
    newOpa.buf = buffer;
    //newOpa.BLock = MTD_getLock();
    //newOpa.StatusBuf = MTD_GetStatusBuf();
    newOpa.needToMoveData = false;
    newOpa.task = xTaskGetCurrentTaskHandle();
/*
    if(((uint32_t)buffer) >= MEMORY_SIZE ){
        newOpa.needToMoveData = true;
        INFO("Data is not loaded in RAM.\n");
    }
    */
    if(((uint32_t)buffer) & 0x3)
    {
        newOpa.needToMoveData = true;
    }

    if(buffer == NULL){
        newOpa.needToMoveData = false;
    }

    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }

    g_mtd_write_cnt++;
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);
    /*
    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
    if(*newOpa.StatusBuf){
        retVal = -1;
    }
    *newOpa.StatusBuf = -5;*/

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    //MTD_WARN("BAD BLOCK 2:%d\n", retVal);

    return retVal;
}




int Mtd::erasePhyBlock(uint32_t block)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_ERASE;
    newOpa.page = block;
    //newOpa.BLock = MTD_getLock();
    //newOpa.StatusBuf = MTD_GetStatusBuf();
    newOpa.task = xTaskGetCurrentTaskHandle();
    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }

    g_mtd_erase_cnt++;
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);
/*
    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
    if(*newOpa.StatusBuf){
        retVal = -1;
    }
    *newOpa.StatusBuf = -5;
*/

    return retVal;
}

int Mtd::eraseAllBlock(void)
{
    int ret = 0;
    for(int i=0; i<st.info.Blocks; i++){
        ret = erasePhyBlock(i);
        MTD_WARN("Erase Block:%d ret:%d\n",i , ret);
        if(ret)
        {
            return ret;
        }
    }
    return ret;
}

int Mtd::readPhyPageMeta(uint32_t page, uint32_t len, uint8_t *buffer)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_READ_META;
    newOpa.page = page;
    newOpa.offset = 0;
    newOpa.buf = buffer;
    newOpa.len = len > st.info.MetaSize_B ? st.info.MetaSize_B : len;
    //newOpa.BLock = MTD_getLock();
    //newOpa.StatusBuf = MTD_GetStatusBuf();
    newOpa.needToMoveData = false;
    newOpa.task = xTaskGetCurrentTaskHandle();

    g_mtd_read_cnt++;
    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);
    /*
    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);

    retVal = *newOpa.StatusBuf;
    *newOpa.StatusBuf = -5;*/

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    return retVal;
}

/**
int MTD_WritePhyPageMeta(uint32_t page, uint32_t len, uint8_t *buffer)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_WRITE_META;
    newOpa.page = page;
    newOpa.len = len > mtdinfo.MetaSize_B ? mtdinfo.MetaSize_B : len;
    newOpa.buf = buffer;
    newOpa.BLock = MTD_getLock();
    newOpa.StatusBuf = MTD_GetStatusBuf();

    while (!deviceInited)
    {
        vTaskDelay(2);
    }
    xQueueSend(MTD_Operates_Queue, &newOpa, portMAX_DELAY);

    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
    if(*newOpa.StatusBuf){
        retVal = -1;
    }
    *newOpa.StatusBuf = -5;
    return retVal;
}
*/

int Mtd::writePhyPageWithMeta(uint32_t page, uint32_t meta_len, uint8_t *buffer, uint8_t *meta)
{
    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_WRITE_META;
    newOpa.page = page;
    newOpa.len = meta_len > st.info.MetaSize_B ? st.info.MetaSize_B : meta_len  ;
    newOpa.buf = buffer;
    newOpa.metaDat = meta;
    //newOpa.BLock = MTD_getLock();
    //newOpa.StatusBuf = MTD_GetStatusBuf();
    newOpa.task = xTaskGetCurrentTaskHandle();

    if(((uint32_t)buffer) & 3)
    {
        newOpa.needToMoveData = true;
    }

    g_mtd_write_cnt++;

    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }
    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);
/*
    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
    if(*newOpa.StatusBuf){
        retVal = -1;
    }
    *newOpa.StatusBuf = -5;*/

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    return retVal;
}


int Mtd::copyPhyPage(uint32_t srcPage, uint32_t dstPage)
{


    MTD_Operates newOpa;
    int retVal = 0;

    newOpa.opa = MTD_PHY_COPY;
    newOpa.page = srcPage;
    newOpa.copyDstPage = dstPage;
    //newOpa.BLock = MTD_getLock();
    //newOpa.StatusBuf = MTD_GetStatusBuf();
    newOpa.task = xTaskGetCurrentTaskHandle();

    while (!st.deviceInited)
    {
        vTaskDelay(2);
    }

    xTaskNotifyStateClear(NULL);
    xQueueSend(st.opQueue, &newOpa, portMAX_DELAY);

    g_mtd_read_cnt++;
    g_mtd_write_cnt++;
/*
    xEventGroupWaitBits(MTDLockEventGroup, (1 << newOpa.BLock), pdTRUE, pdFALSE, portMAX_DELAY);
    retVal = *newOpa.StatusBuf;
    *newOpa.StatusBuf = -5;*/

    xTaskNotifyWait(0, 0xFFFFFFFF, (uint32_t *)&retVal, portMAX_DELAY);

    return retVal;
}


void Mtd::deviceInit()
{
    st.opQueue = xQueueCreate(4, sizeof(MTD_Operates));
    printf("MTD_Operates_Queue:%p\n", st.opQueue);
    Gpmi::deviceInit(&st.info);


    //MTDLockEventGroup = xEventGroupCreate();
//    MTDDriverOpaDone = xEventGroupCreate();
/*
    MTDLockBit = 0;
    for(int i=0; i<(sizeof(MTDStatusBuf) / sizeof(int32_t)); i++){
        MTDStatusBuf[i] = -5;
    }
    pMTDStatus = 0;
    */
    st.deviceInited = true;
}
