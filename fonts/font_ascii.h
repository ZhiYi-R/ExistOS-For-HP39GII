/**
 * @file fonts/font_ascii.h
 * @brief Shared ASCII font declarations.
 *
 * The ASCII glyph tables are defined once in fonts/vgafont.c
 * (VGA_Ascii_5x8 / 6x12 / 7x14 / 8x16). This header only declares them so
 * both the Bootloader (rom.elf) and the System (sys.elf) link against the
 * same single copy. Indexing convention: glyphs start at space (0x20),
 * glyph N (= ch - ' ') begins at offset N * font_height; 1 byte per row,
 * MSB is the leftmost pixel, 8 px wide.
 */

#ifndef __FONT_ASCII_H__
#define __FONT_ASCII_H__

extern const unsigned char VGA_Ascii_5x8[];
extern const unsigned char VGA_Ascii_6x12[];
extern const unsigned char VGA_Ascii_7x14[];
extern const unsigned char VGA_Ascii_8x16[];

#endif
