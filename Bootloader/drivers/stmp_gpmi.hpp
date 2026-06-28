/**
 * @file Bootloader/drivers/stmp_gpmi.hpp
 * @brief GPMI NAND flash controller — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the file-scope shared state of the GPMI
 * driver (operation/copy/ECC flags, DMA descriptor chains, scratch buffers) is
 * encapsulated as @c Gpmi private @c static @c inline members. The three GPMI
 * interrupt seams are dispatched by name from up_isr() and therefore keep C
 * linkage; they are declared just below and granted @c friend access so they can
 * operate on that private state directly.
 *
 * Phase 3.5b merged the MTD service layer (Hal/mtd_up) onto this driver: the
 * @c Mtd class (mtd_up.hpp) calls these public methods directly, so the former
 * @c portMTD* extern "C" forwarding shims are gone. The three ISRs below call
 * @c Mtd::upOpaFin() to latch the ECC result and wake the blocked MTD task.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mtd_up.hpp"   // mtdInfo_t + Mtd::upOpaFin

// ---------------------------------------------------------------------------
// Hardware-description types (namespace scope so the extern "C" ISR seams can
// name the GPMI_Operation enumerators without qualification).
// ---------------------------------------------------------------------------
typedef struct GPMI_DMA_Desc
{
    // DMA related fields
    uint32_t dma_nxtcmdar;
    uint32_t dma_cmd;
    uint32_t dma_bar;
    // GPMI related fields
    uint32_t gpmi_ctrl0;    //PIO 0
    uint32_t gpmi_compare;  //PIO 1
    uint32_t gpmi_eccctrl;  //PIO 2
    uint32_t gpmi_ecccount; //PIO 3
    uint32_t gpmi_data_ptr; //PIO 4
    uint32_t gpmi_aux_ptr;  //PIO 5
} GPMI_DMA_Desc;

typedef struct GPMI_Timing_t {
    uint32_t tPROG_us;
    uint32_t tBERS_us;
    uint32_t tREAD_us;
    unsigned char DataSetup_ns;
    unsigned char DataHold_ns;
    unsigned char AddressSetup_ns;
    float SampleDelay_cyc;
} GPMI_Timing_t;

typedef enum {
    GPMI_OPA_NONE,
    GPMI_OPA_WRITE,
    GPMI_OPA_READ,
    GPMI_OPA_ERASE,
    GPMI_OPA_COPY
} GPMI_Operation;

// ---------------------------------------------------------------------------
// Interrupt seams: up_isr() in interrupt_up.cpp dispatches these by name, so
// they keep C linkage. Declared before Gpmi so the class can friend them.
// ---------------------------------------------------------------------------
extern "C" {
void portMTD_ISR();
void portMTD_DMA_ISR();
bool portMTD_ECC_ISR();
}

class Gpmi {
public:
    static void interfaceInit();
    static void deviceInit(mtdInfo_t *mtdinfo);
    static void readPage(uint32_t page, uint8_t *buf);
    static void writePage(uint32_t page, uint8_t *buf);
    static void writePageMeta(uint32_t page, uint8_t *buf, uint8_t *metaBuf);
    static uint8_t *getMetaData();
    static void eraseBlock(uint32_t block);
    static void copyPage(uint32_t src, uint32_t dst);

private:
    // ---- shared state (was file-scope statics) --------------------------
    static inline GPMI_Timing_t defaultTiming =
    {
            .tPROG_us = 15,
            .tBERS_us = 2000,
            .tREAD_us = 7,
            .DataSetup_ns = 6 + 2,
            .DataHold_ns = 5 + 2,
            .AddressSetup_ns = 6 + 2,
            .SampleDelay_cyc = 3.0,
    };

    static inline uint64_t GPMIFreq;
    static inline uint64_t DeviceTimeOutCycles;

    static inline volatile GPMI_DMA_Desc chains_cmd[4];
    static inline volatile GPMI_DMA_Desc chains_read[8 + 1];
    static inline volatile GPMI_DMA_Desc chains_write[10 + 1];
    static inline volatile GPMI_DMA_Desc chains_erase[8 + 1];

    static inline uint32_t FlashSendCommandBuffer[8];
    static inline uint32_t FlashSendParaBuffer[4];
    static inline uint32_t FlashRecCommandBuffer[4];

    static inline uint32_t GPMI_DataBuffer[ 2048 / 4 ] __attribute__((aligned(4)));
    static inline uint32_t GPMI_AuxiliaryBuffer[ 512 / 4 ] __attribute__((aligned(4)));

    // ---- shared scalar runtime state -----------------------------------
    // Touched by the three GPMI ISRs *and* the operation methods. Grouped into
    // one object so this originally-contiguous .bss block keeps its section-
    // anchor base+offset codegen: separate `static inline` members are distinct
    // COMDAT symbols the compiler cannot prove adjacent, so each would cost its
    // own address load. The ISRs reach these through friendship as Gpmi::st.* .
    struct SharedState {
        volatile bool           ECC_FIN;
        volatile GPMI_Operation GPMI_curOpa;
        volatile uint32_t       GPMI_CopyState;
        uint32_t                ECCResult;
        uint32_t                CopyECCResult;
        uint32_t                LastProgTime;
        uint32_t                LastEraseTime;
        uint32_t                LastReadTime;
        uint32_t                LastOpa;
#ifdef PR_NAND_WR_TIMING_STATUS
        uint32_t                pgwdt;
        uint32_t                pgrdt;
#endif
    };
    static inline SharedState st{};

    // ---- internal helpers (were file-scope static functions) -----------
    static void GPMI_EnableDMAChannel(bool enable);
    static void GPMI_ResetDMAChannel();
    static void GPMI_SetAccessTiming(GPMI_Timing_t timing);
    static void GPMI_ClockConfigure();
    static void GPMI_DMAChainsInit();
    static void GPMIConfigure();
    static void waitLastOpa();
    static void GPMI_sendCommand(uint32_t *cmd, uint32_t *para, uint16_t paraLen, uint32_t *buf, uint32_t RBlen, bool block);
    static void GPMI_ReadPage(uint32_t ColumnAddress, uint32_t RowAddress, uint32_t *data, uint32_t *auxData, bool block);
    static void GPMI_EraseBlock(uint32_t blockAddress, bool block);
    static void GPMI_WritePage(uint32_t ColumnAddress, uint32_t RowAddress, uint32_t *data, uint32_t *auxData, bool block);
    static void GPMI_CopyPage(uint32_t srcPage, uint32_t dstPage);
    static void NAND_Reset();
    static void GPMI_GetNANDInfo(mtdInfo_t *mtdinfo);

    // ---- interrupt friends: the three GPMI ISRs operate on the private
    //      shared state above ------------------------------------------------
    friend void ::portMTD_ISR();
    friend void ::portMTD_DMA_ISR();
    friend bool ::portMTD_ECC_ISR();
};
