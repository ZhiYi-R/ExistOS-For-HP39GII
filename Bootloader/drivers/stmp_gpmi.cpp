/**
 * @file Bootloader/drivers/stmp_gpmi.c
 * @brief GPMI NAND flash controller driver
 */

#include "mtd_up.hpp"
#include "interrupt_up.h"

#include <string.h>

#include "reg_model.hpp"
#include "reg_values.hpp"


#include "hw_irq.h"

#include "debug.h"

#include "stmp_board.hpp"

//#define PR_NAND_WR_TIMING_STATUS   // enable NAND read/write timing instrumentation
#include "stmp_gpmi.hpp"

#define CHIP_SEL  0
#define NAND_DMA_Channel  4

#define MASK_AND_REFERENCE_VALUE 0x0100

#define NAND_CMD_READ0      0
#define NAND_CMD_READSTART  0x30

#define NAND_CMD_ERASE1 0x60
#define NAND_CMD_ERASE2 0xd0

#define NAND_CMD_SEQIN 0x80
#define NAND_CMD_PAGEPROG 0x10

#define NAND_CMD_STATUS 0x70


// Write-only reserved-block scratch: filled with 0xFF by interfaceInit() and
// never read. As a file-scope `static` it keeps internal linkage, so -Os proves
// the fill dead and eliminates it exactly as the pre-class code did. Promoting
// it to a `static inline` class member would give it vague (COMDAT) linkage, on
// which the compiler can no longer prove deadness -- materialising a spurious
// 128-byte memset. It holds no live state the class needs, so it stays here.
static uint32_t ReserveBlock[32];


void Gpmi::GPMI_EnableDMAChannel(bool enable)
{
    if(enable){
        reg::APBH_CTRL0::clr(reg::APBH_CTRL0_::CLKGATE_CHANNEL::val(0x10));
    }else{
        reg::APBH_CTRL0::set(reg::APBH_CTRL0_::CLKGATE_CHANNEL::val(0x10));
    }
}

void Gpmi::GPMI_ResetDMAChannel()
{
    reg::APBH_CTRL0::set(reg::APBH_CTRL0_::RESET_CHANNEL::val(0x10));
}

void Gpmi::GPMI_SetAccessTiming(GPMI_Timing_t timing)
{

    uint32_t dh;
    uint32_t ds;
    uint32_t as;
    DeviceTimeOutCycles = nsToCycles(80000000, 1000000000ULL / (GPMIFreq / 4096ULL), 0);  //80ms
    //DeviceTimeOutCycles = 0xFFFF;
    ds = nsToCycles(timing.DataSetup_ns, 1000000000ULL / GPMIFreq, 1);
    dh = nsToCycles(timing.DataHold_ns, 1000000000ULL / GPMIFreq, 1);
    as = nsToCycles(timing.DataSetup_ns, 1000000000ULL / GPMIFreq, 0);
    INFO("DATA_SETUP:%u\n", ds);
    INFO("DATA_HOLD:%u\n", dh);
    INFO("ADDRESS_SETUP:%u\n", as);
    
    reg::GPMI_TIMING0::wr(reg::GPMI_TIMING0::rd() & ~(reg::GPMI_TIMING0_::DATA_SETUP::mask |
                                                      reg::GPMI_TIMING0_::DATA_HOLD::mask |
                                                      reg::GPMI_TIMING0_::ADDRESS_SETUP::mask));
    reg::GPMI_TIMING0::wr(reg::GPMI_TIMING0::rd() | (reg::GPMI_TIMING0_::DATA_SETUP::val(ds) |
                                                     reg::GPMI_TIMING0_::DATA_HOLD::val(dh) |
                                                     reg::GPMI_TIMING0_::ADDRESS_SETUP::val(as)));

    reg::GPMI_TIMING1::wr(reg::GPMI_TIMING1::rd() & ~reg::GPMI_TIMING1_::DEVICE_BUSY_TIMEOUT::mask);
    reg::GPMI_TIMING1::wr(reg::GPMI_TIMING1::rd() | reg::GPMI_TIMING1_::DEVICE_BUSY_TIMEOUT::val(DeviceTimeOutCycles));

    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::DSAMPLE_TIME::mask | reg::GPMI_CTRL1_::BURST_EN::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::DSAMPLE_TIME::val((uint32_t)(timing.SampleDelay_cyc * 2)) |
                         reg::GPMI_CTRL1_::BURST_EN::val(1));

}

void Gpmi::GPMI_ClockConfigure()
{
    reg::CLKCTRL_GPMI::wr(reg::CLKCTRL_GPMI::rd() & ~reg::CLKCTRL_GPMI_::CLKGATE::mask);
    reg::CLKCTRL_FRAC::clr(reg::CLKCTRL_FRAC_::CLKGATEIO::mask);

    reg::CLKCTRL_GPMI::wr(reg::CLKCTRL_GPMI_::DIV::val(2));    // 480 / 2 MHz
    INFO("GPMI CLK DIV:%d\n", reg::CLKCTRL_GPMI::B().DIV);
    //BF_CS1(CLKCTRL_GPMI, DIV, 2);

    reg::CLKCTRL_CLKSEQ::clr(reg::CLKCTRL_CLKSEQ_::BYPASS_GPMI::mask);    //Set TO HF

    GPMIFreq = 480000000ULL / 2ULL;


}

void Gpmi::GPMI_DMAChainsInit()
{
    // ============================================Command Chains================================================
    //  Phase 1: Wait for ready;
    chains_cmd[0].dma_cmd       =   reg::APBH_CHn_CMD_::CMDWORDS::val(1)         |          // 发送1个PIO命令到GPMI控制器
                                    reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |          // 完成当前命令之后再继续执行
                                    reg::APBH_CHn_CMD_::NANDWAIT4READY::val(1)   |          // 等待NAND就绪后开始执行
                                    reg::APBH_CHn_CMD_::NANDLOCK::val(0)         |          // 锁住NAND防止被其它DMA通道占用
                                    reg::APBH_CHn_CMD_::CHAIN::val(1)            |          // 还有剩下的描述符链
                                    reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_cmd[0].dma_bar       =   0;

    chains_cmd[0].gpmi_ctrl0    =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WAIT_FOR_READY)            | // 当前模式：等待NAND就绪
                                    reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(0)             |
                                    reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT) | // 8bit总线模式
                                    reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)                      | // 数据模式
                                    reg::GPMI_CTRL0_::CS::val(CHIP_SEL);                                   // 片选

    chains_cmd[0].dma_nxtcmdar  =   (uint32_t)&chains_cmd[1];

    //  Phase 2: Send command and address; Lock the nand flash
    chains_cmd[1].dma_cmd       =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1 + 00000000)    | // 1字节命令 和剩下的地址数据
                                    reg::APBH_CHn_CMD_::CMDWORDS::val(3)                 | // 发送3个PIO命令到GPMI控制器
                                    reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              | // 等待NAND就绪后开始执行
                                    reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                    reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                    reg::APBH_CHn_CMD_::NANDLOCK::val(1)                 |
                                    reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                    reg::APBH_CHn_CMD_::CHAIN::val(1)                    |
                                    reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);      // 从内存读取，发送到NAND

    chains_cmd[1].dma_bar       =   00000000;

    chains_cmd[1].gpmi_ctrl0    =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     | // 写入NAND
                                    reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                    reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)       |
                                    reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(0)             |
                                    reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |
                                    reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                    reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(1)          | // 1 byte CMD before multi-byte ADDRESS
                                    reg::GPMI_CTRL0_::XFER_COUNT::val(1 + 00000000);
    
    chains_cmd[1].gpmi_compare = 0;
    chains_cmd[1].gpmi_eccctrl = 0;

    chains_cmd[1].dma_nxtcmdar  =   (uint32_t)&chains_cmd[2];   //
    //  Phase 3: Readback command result;    
    chains_cmd[2].dma_cmd       =   reg::APBH_CHn_CMD_::XFER_COUNT::val(00000000)        |  //Readback bytes
                                    reg::APBH_CHn_CMD_::CMDWORDS::val(3)                 |
                                    reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |
                                    reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                    reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                    reg::APBH_CHn_CMD_::NANDLOCK::val(0)                 |
                                    reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                    reg::APBH_CHn_CMD_::CHAIN::val(1)                    |
                                    reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_WRITE);

    chains_cmd[2].dma_bar       =   00000000;    //readback address

    chains_cmd[2].gpmi_ctrl0    =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__READ)      |
                                    reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                    reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)       |
                                    reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(0)             |
                                    reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |
                                    reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)      |
                                    reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                    reg::GPMI_CTRL0_::XFER_COUNT::val(00000000);             //Readback bytes

    chains_cmd[2].gpmi_compare = 0;
    chains_cmd[2].gpmi_eccctrl = 0;

    chains_cmd[2].dma_nxtcmdar = (uint32_t)&chains_cmd[3];

    //  Phase 4: Terminate;  

    chains_cmd[3].dma_cmd      =    reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)                   |
                                    reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)                  |
                                    reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                    |  
                                    reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_cmd[3].dma_bar = 0;
    chains_cmd[3].dma_nxtcmdar = 0;

    // ============================================Read Chains================================================
    //  Phase 1: issue NAND read setup command (CLE/ALE);
    chains_read[0].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1 + 4)   |       // point to the next descriptor
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)         |       // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |       
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |       
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |       
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |       
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);    // read data from DMA, write to NAND

    chains_read[0].dma_bar          =   (uint32_t)FlashSendCommandBuffer;

    chains_read[0].gpmi_ctrl0       =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE) |
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)  |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)    |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)              |
                                        reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(1)             |
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)   |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(1)      |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1 + 4);            

    chains_read[0].gpmi_compare     =   0;

    chains_read[0].gpmi_eccctrl     =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE); // disable the ECC

    chains_read[0].dma_nxtcmdar     =   (uint32_t)&chains_read[1];

    //  Phase 2: issue NAND read execute command (CLE);
    
    chains_read[1].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1)       |       // 1 byte read command
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(1)         |       // send 1 word to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |       
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |       
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |       // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |       
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);    // read data from DMA, write to NAND

    chains_read[1].dma_bar          =   ((uint32_t)FlashSendCommandBuffer) + 5;     // point to byte 6, read execute command

    chains_read[1].gpmi_ctrl0       =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)       |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                        reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(1)             |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);                      // 1 byte command

    chains_read[1].dma_nxtcmdar     =   (uint32_t)&chains_read[2];                // point to the next descriptor


    //  Phase 3: wait for ready (DATA);
    chains_read[2].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)       |       // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(1)         |       // send 1 word to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |       
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(1)   |       // wait for nand to be ready
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |       // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |       
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER); // no dma transfer

    chains_read[2].dma_bar          =   0; // field not used
            // 1 word sent to the GPMI
    chains_read[2].gpmi_ctrl0       =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WAIT_FOR_READY)    | // wait for NAND ready
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          | // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::val(1)             |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(0);

    chains_read[2].dma_nxtcmdar     =   (uint32_t)&chains_read[3];    // point to the next descriptor


    //  Phase 4: psense compare (time out check);
    chains_read[3].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)               |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                 |   // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |   // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                 |
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_SENSE);       // perform a sense check

    chains_read[3].dma_bar          =   (uint32_t)&chains_read[8];                      // if sense check fails, branch to error handler

    chains_read[3].dma_nxtcmdar     =   (uint32_t)&chains_read[4];                      // point to the next descriptor



    //  Phase 5: read 2K page plus 19 byte meta-data Nand data and send it to ECC block (DATA);
    chains_read[4].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)       |           // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(6)         |           // send 6 words to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |           // wait for command to finish beforecontinuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |           // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |           // ECC block generates ecc8 interrupt on completion
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |           // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no DMA transfer, ECC block handlestransfer

    chains_read[4].dma_bar          =   0;                                              // field not used
    // 6 words sent to the GPMI
    chains_read[4].gpmi_ctrl0       =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__READ)      |   // read from the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)       |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)      |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(4 * (512 + 9) + (19 + 9)); 
                                        // 2K PAGE SIZE four 512 byte data blocks (plusparity, t = 4) 
                                        // and one 19 byte aux block (plusparity, t = 4)

    chains_read[4].gpmi_compare     =   0; // field not used but necessary to seteccctrl

    // GPMI ECCCTRL PIO This launches the 2K byte transfer through ECC8’s
    // bus master. Setting the ECC_ENABLE bit redirects the data flow
    // within the GPMI so that read data flows to the ECC8 engine instead
    // of flowing to the GPMI’s DMA channel.

    chains_read[4].gpmi_eccctrl     =   reg::GPMI_ECCCTRL_::ECC_CMD::val(reg::GPMI_ECCCTRL_sym::ECC_CMD__DECODE_4_BIT) |   // specify t = 4 mode
                                        reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__ENABLE)    |   // enable ECC module
                                        reg::GPMI_ECCCTRL_::BUFFER_MASK::val(0X10F);             // read all 4 data blocks and 1 aux block
    
    chains_read[4].gpmi_ecccount    =   reg::GPMI_ECCCOUNT_::COUNT::val(4 * (512 + 9) + (19 + 9)); 
                                        // 2K PAGE SIZE specify number of bytes read from NAND

    chains_read[4].gpmi_aux_ptr     =   (uint32_t)GPMI_AuxiliaryBuffer; 
                                        // pointer for the 19 byte aux area + parity and syndrome bytes 
	                                    // for both data and aux blocks.

    chains_read[4].gpmi_data_ptr    =   00000000;  

    chains_read[4].dma_nxtcmdar     =   (uint32_t)&chains_read[5];                  // point to the next descriptor

    //  Phase 6: disable ECC block;
    chains_read[5].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)               |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)                 |   // send 3 words to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(1)           |   // wait for nand to be ready
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                 |   // need nand lock to be thread safe while turnoff ECC8
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no dma transfer
    chains_read[5].dma_bar          =   0;                                              // field not used
    
    chains_read[5].gpmi_ctrl0       =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WAIT_FOR_READY)    |
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          | // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(0);

    chains_read[5].gpmi_compare     =   0;                                          // field not used but necessary to set eccctrl
    chains_read[5].gpmi_eccctrl     =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);  // disable the ECC block

    chains_read[5].dma_nxtcmdar     =   (uint32_t)&chains_read[6];                      // point to the next descriptor

    
    //  Phase 7: deassert nand lock;
    chains_read[6].dma_cmd          =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)               |       // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                 |       // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                 |       // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |       // ECC8 engine generates interrupt
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |       // terminate DMA chain processing
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);         // no dma transfer
    chains_read[6].dma_bar          =   0;                                  // field not used
    chains_read[6].dma_nxtcmdar     =   (uint32_t)&chains_read[7];                      


    //  Phase 8: Terminate;
    chains_read[7].dma_cmd          =   reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)               |
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                |
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_read[7].dma_bar          =   0;
    chains_read[7].dma_nxtcmdar     =   0;

    //  ERROR Brunch;
    chains_read[8].dma_cmd          =   reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)               |
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                |
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_read[8].dma_bar          =   0;
    chains_read[8].dma_nxtcmdar     =   0;




    // ============================================Write Chains================================================
    //  Phase 1: issue NAND write setup command (CLE/ALE);

    chains_write[0].dma_nxtcmdar    =   (uint32_t)&chains_write[1];
    chains_write[0].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1 + 4)       |       // 1 byte command, 4 byte address
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)             |       // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)          |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)            |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)       |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)             |       // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)           |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);        // read data from DMA, write to NAND

    chains_write[0].dma_bar         =   (uint32_t)FlashSendCommandBuffer;               // byte 0 write setup, bytes 1 - 4 NAND address
    chains_write[0].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE) |       // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)  |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)    |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)              |       // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)   |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(1)      |       // send command and address
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1 + 4);                // 1 byte command, 4 byte address

    chains_write[0].gpmi_compare    =   0;                             // field not used but necessary to set eccctrl
    chains_write[0].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);      // disable the ECC block


    //  Phase 2: write the data payload (DATA)
    
    chains_write[1].dma_nxtcmdar    =   (uint32_t)&chains_write[2];                 // point to the next descriptor
    chains_write[1].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(4 * 512)     |       // NOTE: DMA transfer only the data payload
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(4)             |       // send 4 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)          |       // DON’T wait to end, wait in the next descriptor
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)            |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)       |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)             |       // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)           |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);        // read data from DMA, write to NAND

    chains_write[1].dma_bar         =   00000000;                               //DATA               

    chains_write[1].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE) |       // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)  |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)    |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)              |       // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)  |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)      |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(4 * 512 + 19);         
                                        // NOTE: this field contains the total amount                      
                                        // DMA transferred (4 data and 1 aux blocks)
                                        // to GPMI
    chains_write[1].gpmi_compare    =   0;                                              // field not used but necessary to set eccctrl
    chains_write[1].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ECC_CMD::val(reg::GPMI_ECCCTRL_sym::ECC_CMD__ENCODE_4_BIT) |   // specify t = 4 mode
                                        reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__ENABLE)    |   // enable ECC module
                                        reg::GPMI_ECCCTRL_::BUFFER_MASK::val(0x10F);             // write all 8 data blocks and 1 aux block
                                                                                        
                                                                                        
    chains_write[1].gpmi_ecccount   =   reg::GPMI_ECCCOUNT_::COUNT::val(4 * (512 + 9) + (19 + 9)); // specify number of bytes written to NAND
    // NOTE: the extra 4*(9)+9 bytes are parity
    // bytes generated by the ECC block.



    // Phase 3: write the aux payload (DATA)
    
    chains_write[2].dma_nxtcmdar    =   (uint32_t)&chains_write[3];             // point to the next descriptor
    chains_write[2].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(19)      |       // NOTE: DMA transfer only the aux block
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)         |       // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |       // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);    // read data from DMA, write to NAND

    chains_write[2].dma_bar         =   00000000;                               //PAYLOAD DATA   

    // Phase 4: issue NAND write execute command (CLE)
    
    chains_write[3].dma_nxtcmdar    =   (uint32_t)&chains_write[4];                 // point to the next descriptor
    chains_write[3].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1)       |           // 1 byte command
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)         |           // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |           // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |   
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |   
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |           // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |   
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |           // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);        // read data from DMA, write to NAND
    chains_write[3].dma_bar         =   ((uint32_t)FlashSendCommandBuffer) + 5;         // point to byte 6, write execute command
                                                                                        // 3 words sent to the GPMI

    chains_write[3].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)        |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);                    // 1 byte command

    chains_write[3].gpmi_compare    =   0;                                              // field not used but necessary to set eccctrl
    chains_write[3].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);      // disable the ECC block


    // Phase 5: wait for ready (CLE);
    chains_write[4].dma_nxtcmdar    =   (uint32_t)&chains_write[5];                 // point to the next descriptor
    chains_write[4].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)           |       // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(1)             |       // send 1 word to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)          |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)            |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(1)       |       // wait for nand to be ready
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)             |       // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)           |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no dma transfer
    chains_write[4].dma_bar         =   0;                         // field not used

    // 1 word sent to the GPMI
    chains_write[4].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WAIT_FOR_READY)    | // wait for NAND ready
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          | // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(0);


    // Phase 6: psense compare (time out check)
    chains_write[5].dma_nxtcmdar    =   (uint32_t)&chains_write[6];                     // point to the next descriptor
    chains_write[5].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)       |           // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)         |           // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)      |           // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)         |
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |           // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_SENSE);       // perform a sense check
    chains_write[5].dma_bar = (uint32_t)&chains_write[10];          // if sense check fails, branch to error handler
    

    // Phase 7: issue NAND status command (CLE)
    chains_write[6].dma_nxtcmdar    =   (uint32_t)&chains_write[7];                 // point to the next descriptor
    chains_write[6].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1)       |       // 1 byte command
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)         |       // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |       // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |   
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |   
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |       // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |   
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);    // read data from DMA, write to NAND

    chains_write[6].dma_bar = ((uint32_t)FlashSendCommandBuffer) + 6; // point to byte 6, status command
    chains_write[6].gpmi_compare    =   0;                                              // field not used but necessary to set eccctrl
    chains_write[6].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);      // disable the ECC block
                                                                                        // 3 words sent to the GPMI
    chains_write[6].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)        |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);                    // 1 byte command


    // Phase 8: read status and compare (DATA);
    chains_write[7].dma_nxtcmdar    =   (uint32_t)&chains_write[8];                     // point to the next descriptor
    chains_write[7].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)       |           // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(2)         |           // send 2 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)      |           // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)         |           // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |           // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no dma transfer
    chains_write[7].dma_bar         =   0;                                  // field not used
    // 2 word sent to the GPMI
    chains_write[7].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__READ_AND_COMPARE)  |   // read from the NAND and compare to expect
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          | // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);

    chains_write[7].gpmi_compare    =   MASK_AND_REFERENCE_VALUE;   // NOTE: mask and reference values are NAND
                                                                    //       SPECIFIC to evaluate the NAND status

    // Phase 9: psense compare (time out check);
    chains_write[8].dma_nxtcmdar    =   (uint32_t)&chains_write[9];                 // point to the next descriptor
    chains_write[8].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)       |       // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)         |       // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)      |       // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)         |       // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)            |       // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_SENSE);   // perform a sense check
    chains_write[8].dma_bar         =   (uint32_t)&chains_write[10];                // if sense check fails, branch to error handler


    // Phase 10: emit GPMI interrupt
    chains_write[9].dma_nxtcmdar    =   0;          // not used since this is last descriptor
    chains_write[9].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)               |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                 |   // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |   // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                 |
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)               |   // emit GPMI interrupt
                                        reg::APBH_CHn_CMD_::CHAIN::val(0)                    |   // terminate DMA chain processing
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no dma transfer


    //  ERROR Brunch;
    chains_write[10].dma_cmd          =   reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)               |
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                |
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_write[10].dma_bar          =   0;
    chains_write[10].dma_nxtcmdar     =   0;

    // ============================================Erase Chains================================================
    //  Phase 1: issue NAND erase setup command (CLE/ALE);
    chains_erase[0].dma_nxtcmdar    =   (uint32_t)&chains_erase[1];
    chains_erase[0].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1 + 3)           |   // 1 byte command, 3 byte block address
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)                 |   // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                 |   // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);        // read data from DMA, write to NAND

    chains_erase[0].dma_bar         =   ((uint32_t)FlashSendCommandBuffer) + 0;       // byte 0 write setup, bytes 1 - 3 NAND address
                                                                                        // 3 words sent to the GPMI
    chains_erase[0].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)        |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(1)          |   // send command and address
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1 + 3);                // 1 byte command, 2 byte address
    chains_erase[0].gpmi_compare    =   (uint32_t)NULL;                             // field not used but necessary to set eccctrl
    chains_erase[0].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);      // disable the ECC block


    //  Phase 2: issue NAND erase conform command (CLE/ALE);


    chains_erase[1].dma_nxtcmdar    =   (uint32_t)&chains_erase[2];                 // point to the next descriptor
    chains_erase[1].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1)               |   // 1 byte command
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)                 |   // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)           |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                 |   // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);        // read data from DMA, write to NAND
    chains_erase[1].dma_bar         =   ((uint32_t)FlashSendCommandBuffer) + 4;       // point to byte 4, write execute command
                                                                                        // 3 words sent to the GPMI
    chains_erase[1].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)     |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)      |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)        |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                  |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)       |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)          |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);                    // 1 byte command
    chains_erase[1].gpmi_compare    =   (uint32_t)NULL;                             // field not used but necessary to set eccctrl
    chains_erase[1].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);      // disable the ECC block



    //  Phase 3: wait for ready (CLE)
    chains_erase[2].dma_nxtcmdar    =   (uint32_t)&chains_erase[3];                 // point to the next descriptor
    chains_erase[2].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)               |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(1)                 |   // send 1 word to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)              |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(1)           |   // wait for nand to be ready
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                 |   // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)               |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                    |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);     // no dma transfer
    chains_erase[2].dma_bar         =   (uint32_t)NULL;                             // field not used

    // 1 word sent to the GPMI
    chains_erase[2].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WAIT_FOR_READY)    |   // wait for NAND ready
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(0);


    //  Phase 4: psense compare (time out check)
    chains_erase[3].dma_nxtcmdar    =   (uint32_t)&chains_erase[4];                         // point to the next descriptor
    chains_erase[3].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)                       |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                         |   // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)                      |   // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)                   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                         |
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)                       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                            |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_SENSE);               // perform a sense check
    
    chains_erase[3].dma_bar         =   (uint32_t)&chains_erase[8];            // if sense check fails, branch to error handler


    //  Phase 5: setup read flash status command

    chains_erase[4].dma_nxtcmdar    =   (uint32_t)&chains_erase[5];                         // point to the next descriptor
    chains_erase[4].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(1)                       |   // 1 byte command
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(3)                         |   // send 3 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)                      |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)                   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                         |   // prevent other DMA channels from taking over
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)                       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                            |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_READ);                // read data from DMA, write to NAND
    chains_erase[4].dma_bar         =   ((uint32_t)FlashSendCommandBuffer) + 5;               // status command
    chains_erase[4].gpmi_compare    =   (uint32_t)NULL;                                     // field not used but necessary to set eccctrl
    chains_erase[4].gpmi_eccctrl    =   reg::GPMI_ECCCTRL_::ENABLE_ECC::val(reg::GPMI_ECCCTRL_sym::ENABLE_ECC__DISABLE);              // disable the ECC block
                                                                                                // 3 words sent to the GPMI
    chains_erase[4].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__WRITE)             |   // write to the NAND
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__ENABLED)                |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_CLE)               |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);                            // 1 byte command

    //  Phase 6: read status and compare (DATA)

    chains_erase[5].dma_nxtcmdar    =   (uint32_t)&chains_erase[6];                         // point to the next descriptor
    chains_erase[5].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)                       |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(2)                         |   // send 2 words to the GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(1)                      |   // wait for command to finish before continuing
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)                   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(1)                         |   // maintain resource lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)                       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                            |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);             // no dma transfer
    chains_erase[5].dma_bar         =   (uint32_t)NULL;                                     // field not used
    // 2 word sent to the GPMI
    chains_erase[5].gpmi_ctrl0      =   reg::GPMI_CTRL0_::COMMAND_MODE::val(reg::GPMI_CTRL0_sym::COMMAND_MODE__READ_AND_COMPARE)  |   // read from the NAND and compare to expect
                                        reg::GPMI_CTRL0_::WORD_LENGTH::val(reg::GPMI_CTRL0_sym::WORD_LENGTH__8_BIT)              |
                                        reg::GPMI_CTRL0_::LOCK_CS::val(reg::GPMI_CTRL0_sym::LOCK_CS__DISABLED)               |
                                        reg::GPMI_CTRL0_::CS::val(CHIP_SEL)                          |   // must correspond to NAND CS used
                                        reg::GPMI_CTRL0_::ADDRESS::val(reg::GPMI_CTRL0_sym::ADDRESS__NAND_DATA)              |
                                        reg::GPMI_CTRL0_::ADDRESS_INCREMENT::val(0)                  |
                                        reg::GPMI_CTRL0_::XFER_COUNT::val(1);
    chains_erase[5].gpmi_compare    =   MASK_AND_REFERENCE_VALUE;                           // NOTE: mask and reference values are NAND
                                                                                            // SPECIFIC to evaluate the NAND status
    //  Phase 7: psense compare (time out check)

    chains_erase[6].dma_nxtcmdar    =   (uint32_t)&chains_erase[7];                         // point to the next descriptor
    chains_erase[6].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)                       |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                         |   // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)                      |   // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(0)                        |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)                   |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                         |   // relinquish nand lock
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(0)                       |
                                        reg::APBH_CHn_CMD_::CHAIN::val(1)                            |   // follow chain to next command
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__DMA_SENSE);               // perform a sense check
    chains_erase[6].dma_bar         =   (uint32_t)&chains_erase[8];                      // if sense check fails, branch to error handler

    //  Phase 8: emit GPMI interrupt
    chains_erase[7].dma_nxtcmdar    =   0;           // not used since this is last descriptor
    chains_erase[7].dma_cmd         =   reg::APBH_CHn_CMD_::XFER_COUNT::val(0)                   |   // no dma transfer
                                        reg::APBH_CHn_CMD_::CMDWORDS::val(0)                     |   // no words sent to GPMI
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)                  |   // do not wait to continue
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                    |
                                        reg::APBH_CHn_CMD_::NANDWAIT4READY::val(0)               |
                                        reg::APBH_CHn_CMD_::NANDLOCK::val(0)                     |
                                        reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)                   |   // emit GPMI interrupt
                                        reg::APBH_CHn_CMD_::CHAIN::val(0)                        |   // terminate DMA chain processing
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);         // no dma transfer

    //  ERROR Brunch;
    chains_erase[8].dma_cmd          =  reg::APBH_CHn_CMD_::IRQONCMPLT::val(1)               |
                                        reg::APBH_CHn_CMD_::WAIT4ENDCMD::val(0)              |
                                        reg::APBH_CHn_CMD_::SEMAPHORE::val(1)                |
                                        reg::APBH_CHn_CMD_::COMMAND::val(reg::APBH_CHn_CMD_sym::COMMAND__NO_DMA_XFER);

    chains_erase[8].dma_bar          =   0;
    chains_erase[8].dma_nxtcmdar     =   0;

    

}

void Gpmi::GPMIConfigure()
{
    GPMI_ClockConfigure();
    GPMI_EnableDMAChannel(true);
    GPMI_ResetDMAChannel();

    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::DEV_RESET::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::DEV_RESET::val(reg::GPMI_CTRL1_sym::DEV_RESET__DISABLED));
    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::ATA_IRQRDY_POLARITY::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::ATA_IRQRDY_POLARITY::val(reg::GPMI_CTRL1_sym::ATA_IRQRDY_POLARITY__ACTIVEHIGH));
    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::GPMI_MODE::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::GPMI_MODE::val(reg::GPMI_CTRL1_sym::GPMI_MODE__NAND));

    GPMI_SetAccessTiming(defaultTiming);

    GPMI_DMAChainsInit();

    st.GPMI_curOpa = GPMI_OPA_NONE;

    portEnableIRQ(HW_IRQ_GPMI, true);
    portEnableIRQ(HW_IRQ_GPMI_DMA, true);
    portEnableIRQ(HW_IRQ_ECC8_IRQ, true);

    reg::APBH_CTRL1::set(reg::APBH_CTRL1_::CH4_CMDCMPLT_IRQ_EN::mask);
    reg::ECC8_CTRL::set(reg::ECC8_CTRL_::COMPLETE_IRQ_EN::mask);
    reg::GPMI_CTRL0::set(reg::GPMI_CTRL0_::DEV_IRQ_EN::mask);
    reg::GPMI_CTRL0::set(reg::GPMI_CTRL0_::TIMEOUT_IRQ_EN::mask);

    

}

inline void Gpmi::waitLastOpa()
{
    switch (st.LastOpa)
    {
    case GPMI_OPA_READ:
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
/*
        while(reg::DIGCTL_MICROSECONDS::rd() - st.LastReadTime < defaultTiming.tREAD_us)
        {
            ;
        }*/

        break;
    case GPMI_OPA_WRITE:
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
/*
        while(reg::DIGCTL_MICROSECONDS::rd() - st.LastProgTime < defaultTiming.tPROG_us)
        {
            ;
        }*/

        break;
    case GPMI_OPA_ERASE:


        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        
        while(reg::DIGCTL_MICROSECONDS::rd() - st.LastEraseTime < defaultTiming.tBERS_us)
        {
            ;
        }

        break;

    default:
        break;
    }
}

inline void Gpmi::GPMI_sendCommand(uint32_t *cmd, uint32_t *para, uint16_t paraLen, uint32_t *buf, uint32_t RBlen, bool block)
{
    
    while (reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA)
            ;

    chains_cmd[1].dma_cmd &= 0x0000FFFF;
    chains_cmd[1].dma_cmd |= (1 + paraLen) << 16;
    chains_cmd[1].dma_bar = (uint32_t)cmd;

    chains_cmd[1].gpmi_ctrl0 &= 0xFFFF0000;
    chains_cmd[1].gpmi_ctrl0 |= ((1 + paraLen) & 0xFFFF);

    if(paraLen){
        chains_cmd[1].dma_nxtcmdar  =   (uint32_t)&chains_cmd[2];

        chains_cmd[2].dma_cmd &= 0x0000FFFF;
        chains_cmd[2].dma_cmd |= (1 + RBlen) << 16;
        chains_cmd[2].gpmi_ctrl0 &= 0xFFFF0000;
        chains_cmd[2].gpmi_ctrl0 |= ((1 + RBlen) & 0xFFFF);
        chains_cmd[2].dma_bar = (uint32_t)buf;

    }else{
        chains_cmd[1].dma_nxtcmdar  =   (uint32_t)&chains_cmd[3];
    }

    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_cmd; // 填写DMA寄存器下个描述符地址
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1;   

    if(block){
        while (reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA)
            ;
    }

}

inline void Gpmi::GPMI_ReadPage(uint32_t ColumnAddress, uint32_t RowAddress, uint32_t *data, uint32_t *auxData, bool block)
{
    volatile uint8_t *probe = NULL;
    waitLastOpa();
    //INFO("Start READ\n");
    volatile uint8_t *cmdBuf = (uint8_t *)FlashSendCommandBuffer;

    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && !st.ECC_FIN)
        ;

    cmdBuf[0] = NAND_CMD_READ0;
    cmdBuf[1] = ColumnAddress & 0xFF;
    cmdBuf[2] = (ColumnAddress >> 8) & 0xFF;
    cmdBuf[3] = RowAddress & 0xFF;
    cmdBuf[4] = (RowAddress >> 8) & 0xFF;
    cmdBuf[5] = NAND_CMD_READSTART;

    if(data){
        chains_read[4].gpmi_data_ptr    =   (uint32_t)data;
    }else{
        chains_read[4].gpmi_data_ptr    =   (uint32_t)GPMI_DataBuffer;
    }

    if(auxData){
        chains_read[4].gpmi_aux_ptr     =   (uint32_t)auxData; 
    }else{
        chains_read[4].gpmi_aux_ptr     =   (uint32_t)GPMI_AuxiliaryBuffer; 
    }

    MTD_INFO("WAIT MTD READ FIN\n");

    probe = (uint8_t *)chains_read[4].gpmi_aux_ptr;
    probe[16] = 0x23;

    st.ECC_FIN = false;
    st.ECCResult = 0x0E0E0E0E;

    st.GPMI_curOpa = GPMI_OPA_READ;
#ifdef PR_NAND_WR_TIMING_STATUS
    st.pgrdt = reg::DIGCTL_MICROSECONDS::rd();
#endif
    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_read[0]; 
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1;  

    MTD_INFO("MTD READ OPA SENT\n");

    if(block){
        while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && !st.ECC_FIN)
            ;
    }

    MTD_INFO("MTD READ OPA SENT FIN\n");

    st.LastReadTime = reg::DIGCTL_MICROSECONDS::rd();
    st.LastOpa = GPMI_OPA_READ;

        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        //portDelayus(100);

        while(probe[16] == 0x23)
            ;
}

inline void Gpmi::GPMI_EraseBlock(uint32_t blockAddress, bool block)
{
    waitLastOpa();

    volatile uint8_t *cmdBuf = (uint8_t *)FlashSendCommandBuffer;

    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && !st.ECC_FIN)
        ;

    cmdBuf[0] = NAND_CMD_ERASE1;
    cmdBuf[1] = (blockAddress << 6) & 0xFF;
    cmdBuf[2] = (blockAddress >> 2) & 0xFF;
    cmdBuf[3] = (blockAddress >> 10) & 0xFF;
    cmdBuf[4] = NAND_CMD_ERASE2;
    cmdBuf[5] = NAND_CMD_STATUS;




    
    st.LastEraseTime = reg::DIGCTL_MICROSECONDS::rd();

    st.GPMI_curOpa = GPMI_OPA_ERASE;

    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_erase[0]; 
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1; 

    if(block){
        while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && !st.ECC_FIN)
            ;
    }

    st.LastEraseTime = reg::DIGCTL_MICROSECONDS::rd();
    st.LastOpa = GPMI_OPA_ERASE;

        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        
        //portDelayms(4);
}

inline void Gpmi::GPMI_WritePage(uint32_t ColumnAddress, uint32_t RowAddress, uint32_t *data, uint32_t *auxData, bool block)
{
    waitLastOpa();

    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA))
        ;
   

    volatile uint8_t *cmdBuf = (uint8_t *)FlashSendCommandBuffer;
    cmdBuf[0] = NAND_CMD_SEQIN;
    cmdBuf[1] = ColumnAddress & 0xFF;
    cmdBuf[2] = (ColumnAddress >> 8) & 0xFF;
    cmdBuf[3] = RowAddress & 0xFF;
    cmdBuf[4] = (RowAddress >> 8) & 0xFF;
    cmdBuf[5] = NAND_CMD_PAGEPROG;
    cmdBuf[6] = NAND_CMD_STATUS;

    

    if(data == NULL){
        memset(GPMI_DataBuffer, 0xFF, sizeof(GPMI_DataBuffer));
        chains_write[1].dma_bar     =   (uint32_t)GPMI_DataBuffer;
    }else{
        chains_write[1].dma_bar     =   (uint32_t)data;
    }

    if(auxData == NULL)
    {
        memset(GPMI_AuxiliaryBuffer, 0xFF, sizeof(GPMI_AuxiliaryBuffer));
        chains_write[2].dma_bar     =   (uint32_t)GPMI_AuxiliaryBuffer;
    }else{
        chains_write[2].dma_bar     =   (uint32_t)auxData;
    }



    st.GPMI_curOpa = GPMI_OPA_WRITE;
#ifdef PR_NAND_WR_TIMING_STATUS
    st.pgwdt = reg::DIGCTL_MICROSECONDS::rd();
#endif
    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_write[0]; 
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1; 

    st.LastProgTime = reg::DIGCTL_MICROSECONDS::rd();

    if(block){
        while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA))
            ;
    }

    st.LastEraseTime = reg::DIGCTL_MICROSECONDS::rd();
    st.LastOpa = GPMI_OPA_WRITE;

        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        
        //portDelayus(200);
}


inline void Gpmi::GPMI_CopyPage(uint32_t srcPage, uint32_t dstPage)
{
    waitLastOpa();
    
    volatile uint8_t *cmdBuf = (uint8_t *)FlashSendCommandBuffer;
    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && !st.ECC_FIN)
        ;

    cmdBuf[0] = NAND_CMD_READ0;
    cmdBuf[1] = 0;
    cmdBuf[2] = 0;
    cmdBuf[3] =  srcPage & 0xFF;
    cmdBuf[4] = (srcPage >> 8) & 0xFF;
    cmdBuf[5] = NAND_CMD_READSTART;

    chains_read[4].gpmi_data_ptr    =   (uint32_t)GPMI_DataBuffer;
    chains_read[4].gpmi_aux_ptr     =   (uint32_t)GPMI_AuxiliaryBuffer; 
    
    st.ECC_FIN = false;
    st.GPMI_curOpa = GPMI_OPA_COPY;
    st.GPMI_CopyState = 0;

    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_read[0]; 
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1;

    
    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && (!st.ECC_FIN))
        ;

        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        /*
    portDelayus(defaultTiming.tREAD_us * 10);
    portDelayus(defaultTiming.tREAD_us * 10);
    portDelayus(defaultTiming.tREAD_us * 10);*/
    
        //portDelayus(100);

    while ((reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA) && (!st.ECC_FIN))
        ;

    st.GPMI_curOpa = GPMI_OPA_COPY;
    st.GPMI_CopyState = 1;

    cmdBuf[0] = NAND_CMD_SEQIN;
    cmdBuf[1] = 0;
    cmdBuf[2] = 0;
    cmdBuf[3] = dstPage & 0xFF;
    cmdBuf[4] = (dstPage >> 8) & 0xFF;
    cmdBuf[5] = NAND_CMD_PAGEPROG;
    cmdBuf[6] = NAND_CMD_STATUS;    

    chains_write[1].dma_bar     =   (uint32_t)GPMI_DataBuffer;
    chains_write[2].dma_bar     =   (uint32_t)GPMI_AuxiliaryBuffer;

    reg::APBH_CHn_NXTCMDAR::B(NAND_DMA_Channel).CMD_ADDR = (uint32_t)&chains_write[0]; 
    reg::APBH_CHn_SEMA::B(NAND_DMA_Channel).INCREMENT_SEMA = 1; 

    st.LastProgTime = reg::DIGCTL_MICROSECONDS::rd();
    st.LastOpa = GPMI_OPA_WRITE;

        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).AHB_BYTES);
        while(reg::APBH_CHn_DEBUG2::B(NAND_DMA_Channel).APB_BYTES);
        
        //portDelayus(200);
}

void Gpmi::NAND_Reset()
{
    FlashSendCommandBuffer[0] = 0xFF;
    GPMI_sendCommand(FlashSendCommandBuffer, 0, 0, 0, 0, true);
    portDelayus(10);
}

void Gpmi::GPMI_GetNANDInfo(mtdInfo_t *mtdinfo)
{

    FlashSendCommandBuffer[0] = 0x90;
    FlashSendParaBuffer[0] = 0;
    GPMI_sendCommand(FlashSendCommandBuffer, FlashSendParaBuffer, 1, FlashRecCommandBuffer, 6, true);

    INFO("Flash ID:\n");
    for(int i = 0; i < 6;i++){
        INFO("%02x ", ((uint8_t *)FlashRecCommandBuffer)[i] );
    }
    INFO("\n");

    char DIDesc4Rd = ((uint8_t *)FlashRecCommandBuffer)[3];

    mtdinfo->PageSize_B = (1 << ( DIDesc4Rd & 0x3 )) * 1024 ;
    mtdinfo->SpareSizePerPage_B = mtdinfo->PageSize_B / 512 * ((1 << ((DIDesc4Rd >> 2) & 1)) * 8);
    mtdinfo->BlockSize_KB = (1 << ((DIDesc4Rd >> 4) & 0x3)) * 64;
    mtdinfo->PagesPerBlock = mtdinfo->BlockSize_KB * 1024 / mtdinfo->PageSize_B;

    mtdinfo->MetaSize_B = 19;
    mtdinfo->Blocks = 1024;

    INFO("PageSize:%u B\n", mtdinfo->PageSize_B);
    INFO("SpareSizePerPage:%u B\n", mtdinfo->SpareSizePerPage_B);
    INFO("BlockSize:%u KB\n", mtdinfo->BlockSize_KB);
    INFO("PagesPerBlock:%u\n", mtdinfo->PagesPerBlock);
    INFO("Blocks:%u\n", mtdinfo->Blocks);

}

// portMTD_ISR / portMTD_DMA_ISR / portMTD_ECC_ISR are dispatched by name from
// interrupt_up.c (stays C until its phase); keep C linkage on the definitions.
extern "C" void portMTD_ISR()
{

    if(reg::GPMI_CTRL1::B().TIMEOUT_IRQ)
        INFO("GPMI_TIMEOUT_IRQ\n");
    if(reg::GPMI_CTRL1::B().DEV_IRQ)
        INFO("GPMI_DEV_IRQ\n");

    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::DEV_IRQ::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::DEV_IRQ::val(0));
    reg::GPMI_CTRL1::clr(reg::GPMI_CTRL1_::TIMEOUT_IRQ::mask);
    reg::GPMI_CTRL1::set(reg::GPMI_CTRL1_::TIMEOUT_IRQ::val(0));
}

extern "C" void portMTD_DMA_ISR()
{
    uint32_t error = reg::APBH_CTRL1::B().CH4_AHB_ERROR_IRQ;
    //INFO("\nportMTD_DMA_ISR\n");
    
    reg::APBH_CTRL1::clr(reg::APBH_CTRL1_::CH4_CMDCMPLT_IRQ::mask);
    reg::APBH_CTRL1::clr(reg::APBH_CTRL1_::CH4_AHB_ERROR_IRQ::mask);

    if(error)
    {
        INFO("GPMI_DMA_ERR\n");
    }   



    switch (Gpmi::st.GPMI_curOpa)
    {
    case GPMI_OPA_READ:
#ifdef PR_NAND_WR_TIMING_STATUS
        INFO("prd=%ld\n", reg::DIGCTL_MICROSECONDS::rd() - Gpmi::st.pgrdt);
#endif
        if(reg::APBH_CHn_CURCMDAR::B(NAND_DMA_Channel).CMD_ADDR == (uint32_t)&Gpmi::chains_read[8]){
            INFO("GPMI_OPA_READ psense compare ERROR\n");
        }

        break;
    case GPMI_OPA_WRITE:
#ifdef PR_NAND_WR_TIMING_STATUS
        INFO("pwr=%ld\n", reg::DIGCTL_MICROSECONDS::rd() - Gpmi::st.pgwdt);
#endif
        if(reg::APBH_CHn_CURCMDAR::B(NAND_DMA_Channel).CMD_ADDR == (uint32_t)&Gpmi::chains_read[8]){
            INFO("GPMI_OPA_WRITE psense compare ERROR\n");
            Mtd::upOpaFin(1);
        }else{
            Mtd::upOpaFin(0);
        }
        break;
    case GPMI_OPA_ERASE:
        if(reg::APBH_CHn_CURCMDAR::B(NAND_DMA_Channel).CMD_ADDR == (uint32_t)&Gpmi::chains_read[8]){
            INFO("GPMI_OPA_ERASE psense compare ERROR\n");
            Mtd::upOpaFin(1);
        }else{
            Mtd::upOpaFin(0);
        }
        break;  

    case GPMI_OPA_COPY:
        if(Gpmi::st.GPMI_CopyState == 0)
        {
            if(reg::APBH_CHn_CURCMDAR::B(NAND_DMA_Channel).CMD_ADDR == (uint32_t)&Gpmi::chains_read[8]){
                INFO("GPMI_OPA_COPY_READ psense compare ERROR\n");
            }
            return;
        }
        if(Gpmi::st.GPMI_CopyState == 1){
            if(reg::APBH_CHn_CURCMDAR::B(NAND_DMA_Channel).CMD_ADDR == (uint32_t)&Gpmi::chains_read[8]){
                INFO("GPMI_OPA_COPY_WRITE psense compare ERROR\n");
                Mtd::upOpaFin(0x0E0E0E0E);
            }else{
                Mtd::upOpaFin(Gpmi::st.CopyECCResult);
            }
        }

        break;

    default:
        break;
    }
    
    Gpmi::st.GPMI_curOpa = GPMI_OPA_NONE;
    
}

extern "C" bool portMTD_ECC_ISR()
{


    Gpmi::st.ECCResult = reg::ECC8_STATUS1::B().STATUS_PAYLOAD0            |
                (reg::ECC8_STATUS1::B().STATUS_PAYLOAD1 << 8)     |
                (reg::ECC8_STATUS1::B().STATUS_PAYLOAD2 << 16)    |
                (reg::ECC8_STATUS1::B().STATUS_PAYLOAD3 << 24)    ;
    

    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::COMPLETE_IRQ::mask);
    reg::ECC8_CTRL::clr(reg::ECC8_CTRL_::BM_ERROR_IRQ::mask);
    
    //INFO("portMTD_ECC_ISR\n");

    if((Gpmi::st.GPMI_curOpa == GPMI_OPA_COPY) && (Gpmi::st.GPMI_CopyState == 0)){
        Gpmi::st.CopyECCResult = Gpmi::st.ECCResult;
        Gpmi::st.ECC_FIN = true;
        return false;
    }

    Gpmi::st.ECC_FIN = true;

    return Mtd::upOpaFin(Gpmi::st.ECCResult);

}


void Gpmi::interfaceInit()
{
    for(int i =0; i < 32; i++)
    {
        ReserveBlock[i] = 0xFFFFFFFF;
    }
    GPMIConfigure();
    st.ECC_FIN = true;
    
}

void Gpmi::deviceInit(mtdInfo_t *mtdinfo)
{
    NAND_Reset();
    GPMI_GetNANDInfo(mtdinfo);


    //GPMI_GetNANDInfo(mtdinfo);

}
//static uint32_t lastRDPage = 0xFFFFFFFF;

void Gpmi::readPage(uint32_t page, uint8_t *buf)
{
    /*
    if((st.LastOpa == GPMI_OPA_READ) && (lastRDPage == page)){
        //INFO("RD:%d\n", page);
        Mtd::upOpaFin(st.ECCResult);
        return;
    }*/
    
    GPMI_ReadPage(0, page, (uint32_t *)buf, NULL, false);

    //lastRDPage = page;
}

void Gpmi::writePage(uint32_t page, uint8_t *buf)
{
    //NFO("WR:%d\n", page);
    GPMI_WritePage(0, page, (uint32_t *)buf, NULL, false);
}

void Gpmi::writePageMeta(uint32_t page, uint8_t *buf, uint8_t *metaBuf)
{
    //INFO("WRM:%d\n", page);
    GPMI_WritePage(0, page, (uint32_t *)buf, (uint32_t *)metaBuf, false);
}

uint8_t *Gpmi::getMetaData()
{
    return (uint8_t *)GPMI_AuxiliaryBuffer;
}


void Gpmi::eraseBlock(uint32_t block)
{
    GPMI_EraseBlock(block, false);
}

void Gpmi::copyPage(uint32_t src, uint32_t dst)
{
    GPMI_CopyPage(src, dst);
}

// The eight public methods above are plain out-of-line definitions (not
// `inline`): the MTD service layer (Mtd, Hal/mtd_up.cpp) links against their
// real symbols and drives the NAND through Gpmi:: directly, so the former
// portMTD* extern "C" forwarding shims were removed in the Phase 3.5b layer
// merge.
