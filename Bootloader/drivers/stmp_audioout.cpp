/**
 * @file Bootloader/drivers/stmp_audioout.c
 * @brief Audio output driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_up.h"
#include "interrupt_up.h"

#include "reg_model.hpp"
#include "reg_values.hpp"
#include "hw_irq.h"

#include "SystemConfig.h"

#ifdef ENABLE_AUIDIOOUT

struct apb_dma_command_t
{
    struct apb_dma_command_t *next;
    uint32_t cmd;
    void *buffer;
    /* PIO words follow */
} __attribute__((packed));

struct pcm_dma_command_t
{
    struct apb_dma_command_t dma;
    /* padded to next multiple of cache line size (32 bytes) */
    uint32_t pad[5];
} __attribute__((packed));

uint32_t pcm_buffer1[4096] __attribute__((aligned(4)));
uint32_t pcm_buffer2[4096] __attribute__((aligned(4)));

volatile bool pcm_buffer_1_fin = true;
volatile bool pcm_buffer_2_fin = true; 

volatile bool pcm_playing = false;

volatile void *cur_pcm_buffer;

static struct pcm_dma_command_t dac_dma;


inline static void pcm_dma_load()
{
    dac_dma.dma.buffer = (void *)cur_pcm_buffer;

    reg::APBX_CHn_NXTCMDAR::B(1).CMD_ADDR = (uint32_t)&dac_dma;
    reg::APBX_CHn_SEMA::B(1).INCREMENT_SEMA = 1;
    

}

void portDAC_IRQ(uint32_t IRQn)
{
    //printf("DAC_IRQ\n");
    reg::APBX_CTRL1::clr(reg::APBX_CTRL1_::CH1_CMDCMPLT_IRQ::mask);
    if(IRQn == HW_IRQ_ADC_ERROR)
    {
        printf("DAC ERROR!\n");

    }

    if(pcm_buffer_1_fin)
    {
        if(!pcm_buffer_2_fin)
        {
            pcm_buffer_2_fin = true;
            cur_pcm_buffer = pcm_buffer2;
            pcm_dma_load();
        }else{
            //all buffers played.
            pcm_playing = false;

        }
    }else{
            pcm_buffer_1_fin = true;
            cur_pcm_buffer = pcm_buffer1;
            pcm_dma_load();
    }
    
}

// is_pcm_buffer_idle / pcm_buffer_load have no shared header; they are
// forward-declared and called by name from the C translation unit vectors.c
// (arm_do_swi audio fast path), so their definitions keep C linkage.
extern "C" bool is_pcm_buffer_idle()
{
    return pcm_buffer_1_fin || pcm_buffer_2_fin;
}

extern "C" void pcm_buffer_load(void *pcmdat)
{
    if(pcm_buffer_1_fin)
    {
        memcpy(pcm_buffer1, pcmdat, sizeof(pcm_buffer1));
        pcm_buffer_1_fin = false;
    }else if(pcm_buffer_2_fin)
    {
        memcpy(pcm_buffer2, pcmdat, sizeof(pcm_buffer2));
        pcm_buffer_2_fin = false;
    }else{
        return;
    }
    
    if(!pcm_playing)
    {
        pcm_playing = true;
        portDAC_IRQ(HW_IRQ_ADC_DMA);
    }
}


void stmp_audio_init()
{
    reg::AUDIOOUT_CTRL::clr(reg::AUDIOOUT_CTRL_::SFTRST::mask);
    reg::AUDIOOUT_CTRL::clr(reg::AUDIOOUT_CTRL_::CLKGATE::mask);

    reg::AUDIOOUT_CTRL::set(reg::AUDIOOUT_CTRL_::SFTRST::mask);
    while(reg::AUDIOOUT_CTRL::B().CLKGATE == 0)
    {
        ;
    }
    reg::AUDIOOUT_CTRL::clr(reg::AUDIOOUT_CTRL_::SFTRST::mask);
    reg::AUDIOOUT_CTRL::clr(reg::AUDIOOUT_CTRL_::CLKGATE::mask);

    reg::CLKCTRL_XTAL::clr(reg::CLKCTRL_XTAL_::FILT_CLK24M_GATE::mask);



    /* Enable DAC */
    reg::AUDIOOUT_ANACLKCTRL::clr(reg::AUDIOOUT_ANACLKCTRL_::CLKGATE::mask);

    reg::AUDIOOUT_PWRDN::clr(reg::AUDIOOUT_PWRDN_::CAPLESS::mask);

    /* Set word-length to 16-bit */
    reg::AUDIOOUT_CTRL::set(reg::AUDIOOUT_CTRL_::WORD_LENGTH::mask);

    /* Power up DAC */
    reg::AUDIOOUT_PWRDN::clr(reg::AUDIOOUT_PWRDN_::DAC::mask);
    /* Hold HP to ground to avoid pop, then release and power up HP */
    reg::AUDIOOUT_ANACTRL::set(reg::AUDIOOUT_ANACTRL_::HP_HOLD_GND::mask);
    reg::AUDIOOUT_PWRDN::clr(reg::AUDIOOUT_PWRDN_::HEADPHONE::mask);


    /* Set HP mode to AB */
    reg::AUDIOOUT_ANACTRL::set(reg::AUDIOOUT_ANACTRL_::HP_CLASSAB::mask);

    /* change bias to -50% */

    reg::AUDIOOUT_TEST::clr(reg::AUDIOOUT_TEST_::HP_I1_ADJ::mask);
    reg::AUDIOOUT_TEST::set(reg::AUDIOOUT_TEST_::HP_I1_ADJ::val(1));
    reg::AUDIOOUT_REFCTRL::clr(reg::AUDIOOUT_REFCTRL_::BIAS_CTRL::mask);
    reg::AUDIOOUT_REFCTRL::set(reg::AUDIOOUT_REFCTRL_::BIAS_CTRL::val(1));
    reg::AUDIOOUT_REFCTRL::set(reg::AUDIOOUT_REFCTRL_::RAISE_REF::mask);
    reg::AUDIOOUT_REFCTRL::set(reg::AUDIOOUT_REFCTRL_::XTAL_BGR_BIAS::mask);




    /* Stop holding to ground */
    reg::AUDIOOUT_ANACTRL::clr(reg::AUDIOOUT_ANACTRL_::HP_HOLD_GND::mask);


    /* Set dmawait count to 31 (see errata, workaround random stop) */
    reg::AUDIOOUT_CTRL::clr(reg::AUDIOOUT_CTRL_::DMAWAIT_COUNT::mask);
    reg::AUDIOOUT_CTRL::set(reg::AUDIOOUT_CTRL_::DMAWAIT_COUNT::val(31));
    /* start converting audio */
    reg::AUDIOOUT_CTRL::set(reg::AUDIOOUT_CTRL_::RUN::mask);
    /* unmute DAC */
    reg::AUDIOOUT_DACVOLUME::clr(reg::AUDIOOUT_DACVOLUME_::MUTE_LEFT::mask);
    reg::AUDIOOUT_DACVOLUME::clr(reg::AUDIOOUT_DACVOLUME_::MUTE_RIGHT::mask);

    reg::AUDIOOUT_HPVOL::B().MUTE = 0;

    reg::AUDIOOUT_HPVOL::B().VOL_LEFT = 0x31;
    reg::AUDIOOUT_HPVOL::B().VOL_RIGHT = 0x31;
    /*send a few samples to avoid pop*/

    reg::AUDIOOUT_DATA::wr(0);
    reg::AUDIOOUT_DATA::wr(0);
    reg::AUDIOOUT_DATA::wr(0);
    reg::AUDIOOUT_DATA::wr(0);

    printf("stmp_audio_init \n");

/*
    //44100 Hz
    HW_AUDIOOUT_DACSRR.B.BASEMULT = 0x1; // quad-rate
    HW_AUDIOOUT_DACSRR.B.SRC_HOLD = 0x0; // 0 for full- double- quad-rates
    HW_AUDIOOUT_DACSRR.B.SRC_INT = 0x11; // 15 for the integer portion
    HW_AUDIOOUT_DACSRR.B.SRC_FRAC = 0x0037; // the fractional portion
*/
/*
    //32000 Hz
    HW_AUDIOOUT_DACSRR.B.BASEMULT = 0x1; // quad-rate
    HW_AUDIOOUT_DACSRR.B.SRC_HOLD = 0x0; // 0 for full- double- quad-rates
    HW_AUDIOOUT_DACSRR.B.SRC_INT = 0x17;
    HW_AUDIOOUT_DACSRR.B.SRC_FRAC = 0x0E00; // the fractional portion
*/
    //22050 Hz
    reg::AUDIOOUT_DACSRR::B().BASEMULT = 0x1; // quad-rate
    reg::AUDIOOUT_DACSRR::B().SRC_HOLD = 0x1; // 0 for full- double- quad-rates
    reg::AUDIOOUT_DACSRR::B().SRC_INT = 0x11;
    reg::AUDIOOUT_DACSRR::B().SRC_FRAC = 0x0037; // the fractional portion


    reg::APBX_CTRL1::B().CH1_CMDCMPLT_IRQ_EN = 1;
    reg::APBX_CTRL1::B().CH1_ERROR_IRQ = 1;



    dac_dma.dma.next = NULL;
    dac_dma.dma.cmd = reg::APBX_CHn_CMD_::XFER_COUNT::val(sizeof(pcm_buffer1)) |
                      reg::APBX_CHn_CMD_::IRQONCMPLT::val(1) |
                      reg::APBX_CHn_CMD_::SEMAPHORE::val(1) |
                      reg::APBX_CHn_CMD_::COMMAND::val(reg::APBX_CHn_CMD_sym::COMMAND__DMA_READ);

    portEnableIRQ(HW_IRQ_DAC_DMA, 1);
    portEnableIRQ(HW_IRQ_DAC_ERROR, 1);


    reg::AUDIOOUT_DACVOLUME::wr(0x00ff00ff);

    


}

#endif


