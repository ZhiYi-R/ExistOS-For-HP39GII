/**
 * @file Bootloader/drivers/stmp_audioout.hpp
 * @brief Audio-out HAL seam (forwards to the AudioOut singleton).
 *
 * Split out of board_up.h in Phase 3. The driver body is gated on
 * ENABLE_AUIDIOOUT (undefined in every current build); the seam is declared
 * here so its single caller (board_up.cpp's boardInit) keeps resolving it.
 * pcm_play, declared in the old board_up.h, had no definition anywhere and
 * is dropped.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void stmp_audio_init();

#ifdef __cplusplus
}
#endif
