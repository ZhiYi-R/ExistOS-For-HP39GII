/**
 * @file Bootloader/drivers/stmp_audioout.hpp
 * @brief Audio output driver — pure-static @c AudioOut singleton.
 *
 * Split out of board_up.h in Phase 3, then folded onto the class in Phase 3.C:
 * board_up.cpp's boardInit now calls @c AudioOut::init() directly, so the class
 * is declared here rather than hidden behind the former @c stmp_audio_init shim.
 *
 * The whole driver is gated on @c ENABLE_AUIDIOOUT (undefined in every current
 * build): when it is off this header is empty and contributes no codegen. The
 * peripheral is the migration's dual-context case — its private buffer state is
 * touched both by the DAC completion IRQ (@c portDAC_IRQ, dispatched by name
 * from up_isr) and by the arm_do_swi audio fast path (@c is_pcm_buffer_idle /
 * @c pcm_buffer_load, called by name from the C unit vectors.c). All three keep
 * C linkage and are granted @c friend access; their definitions, like the class
 * methods', live in stmp_audioout.cpp.
 */
#pragma once

#ifdef ENABLE_AUIDIOOUT

#include <stdint.h>

// ---------------------------------------------------------------------------
// DMA command descriptor types (only used by AudioOut below).
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Dual-context seams: the DAC completion IRQ (dispatched by name from up_isr())
// and the two arm_do_swi fast-path entries (called by name from the C unit
// vectors.c) all share AudioOut's private buffer state. They keep C linkage and
// are declared before the class so it can friend them.
// ---------------------------------------------------------------------------
extern "C" {
void portDAC_IRQ(uint32_t IRQn);
bool is_pcm_buffer_idle();
void pcm_buffer_load(void *pcmdat);
}

class AudioOut {
public:
    static void init();

private:
    // ---- ping-pong sample buffers and the DMA descriptor ----------------
    static inline uint32_t pcm_buffer1[4096] __attribute__((aligned(4)));
    static inline uint32_t pcm_buffer2[4096] __attribute__((aligned(4)));
    static inline pcm_dma_command_t dac_dma;

    // ---- shared scalar runtime state -----------------------------------
    // Touched by the DAC IRQ *and* the two vectors.c SWI entries. Grouped into
    // one object so this originally-contiguous block keeps its section-anchor
    // base+offset codegen: separate `static inline` members are distinct COMDAT
    // symbols the compiler cannot prove adjacent, so each would cost its own
    // address load. The seams reach these through friendship as AudioOut::st.* .
    struct SharedState {
        volatile bool  pcm_buffer_1_fin;
        volatile bool  pcm_buffer_2_fin;
        volatile bool  pcm_playing;
        volatile void *cur_pcm_buffer;
    };
    static inline SharedState st{ true, true, false, nullptr };

    // ---- internal helper (was a file-scope `inline static` function) ----
    static void pcm_dma_load();

    // ---- dual-context friends: the DAC IRQ and the two vectors.c SWI
    //      entries operate on the shared state above ----------------------
    friend void ::portDAC_IRQ(uint32_t);
    friend bool ::is_pcm_buffer_idle();
    friend void ::pcm_buffer_load(void *);
};

#endif // ENABLE_AUIDIOOUT
