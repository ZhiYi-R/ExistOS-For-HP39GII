/**
 * @file Bootloader/drivers/stmp_audioout.cpp
 * @brief Audio output driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the file-scope state of the audio driver
 * (the two PCM ping-pong buffers, their completion flags, the play flag and the
 * DMA descriptor) is encapsulated as @c AudioOut private @c static @c inline
 * members. This peripheral is the migration's dual-context case: the same
 * private state is touched from two unrelated execution contexts ---
 *   - @c portDAC_IRQ — the DAC DMA completion interrupt, dispatched by name from
 *     up_isr() in interrupt_up.cpp (hence C linkage);
 *   - @c is_pcm_buffer_idle / @c pcm_buffer_load — the arm_do_swi audio fast path,
 *     forward-declared and called by name from the C translation unit vectors.c
 *     (hence C linkage).
 * All three are granted @c friend access so they can operate on that private
 * state directly. Phase 3.C folds the former @c stmp_audio_init shim away: the
 * @c AudioOut class is declared in stmp_audioout.hpp and boardInit calls
 * @c AudioOut::init() by name, so this file carries only the method and seam
 * definitions.
 *
 * The whole driver is gated on @c ENABLE_AUIDIOOUT, which is not defined in any
 * current build; the encapsulation is purely structural and changes no codegen
 * in the shipped image.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stmp_audioout.hpp"
#include "interrupt_up.h"

#include "reg_model.hpp"
#include "reg_values.hpp"
#include "hw_irq.h"

#include "SystemConfig.h"

#ifdef ENABLE_AUIDIOOUT

inline void AudioOut::pcm_dma_load()
{
    dac_dma.dma.buffer = (void *)st.cur_pcm_buffer;

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

    if(AudioOut::st.pcm_buffer_1_fin)
    {
        if(!AudioOut::st.pcm_buffer_2_fin)
        {
            AudioOut::st.pcm_buffer_2_fin = true;
            AudioOut::st.cur_pcm_buffer = AudioOut::pcm_buffer2;
            AudioOut::pcm_dma_load();
        }else{
            //all buffers played.
            AudioOut::st.pcm_playing = false;

        }
    }else{
            AudioOut::st.pcm_buffer_1_fin = true;
            AudioOut::st.cur_pcm_buffer = AudioOut::pcm_buffer1;
            AudioOut::pcm_dma_load();
    }

}

bool is_pcm_buffer_idle()
{
    return AudioOut::st.pcm_buffer_1_fin || AudioOut::st.pcm_buffer_2_fin;
}

void pcm_buffer_load(void *pcmdat)
{
    if(AudioOut::st.pcm_buffer_1_fin)
    {
        memcpy(AudioOut::pcm_buffer1, pcmdat, sizeof(AudioOut::pcm_buffer1));
        AudioOut::st.pcm_buffer_1_fin = false;
    }else if(AudioOut::st.pcm_buffer_2_fin)
    {
        memcpy(AudioOut::pcm_buffer2, pcmdat, sizeof(AudioOut::pcm_buffer2));
        AudioOut::st.pcm_buffer_2_fin = false;
    }else{
        return;
    }

    if(!AudioOut::st.pcm_playing)
    {
        AudioOut::st.pcm_playing = true;
        portDAC_IRQ(HW_IRQ_ADC_DMA);
    }
}


void AudioOut::init()
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
