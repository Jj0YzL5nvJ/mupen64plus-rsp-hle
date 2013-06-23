/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-rsp-hle - mp3.c                                           *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2012 Bobby Smiles                                       *
 *   Copyright (C) 2009 Richard Goedeken                                   *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hle.h"
#include "m64p_types.h"

#include "mp3.h"

static void butterfly(s32 *x, s32 *y, s32 w);
static void apply_gain(u16 mem, unsigned count, s16 gain);
static void idot8(s32 *dot1, s32 *dot2, const s16 *x, const s16 *y);
static void mdct32(u32 inPtr, u32 t5, u32 t6);
static void window(u32 t4, u32 t6, u32 outPtr);

static u8 mp3data[0x1000];


/* {2*cos((2k+1)PI/64)} with k = {0,1,3,2,7,6,4,5,15,14,12,13,8,9,11,10} */
static const s32 C64_ODD[16] =
{
    0x1ff64, 0x1fa74, 0x1e214, 0x1f0a8, 0x17b5c, 0x19b40, 0x1ced8, 0x1b728,
    0x01920, 0x04b20, 0x0ac7c, 0x07c68, 0x157d8, 0x13100, 0x0dae8, 0x10738
};

/* {cos(2*(2k+1)PI/64)} with k = {0,1,3,2,7,6,4,5} */
static const u16 C64_EVEN1[8] =
{
    0xfec4, 0xf4fa, 0xc5e4, 0xe1c4, 0x1916, 0x4a50, 0xa268, 0x78ae
};

/* {cos(2*(2k)PI/64)} with k = {1,3,7,5} */
static const u16 C64_EVEN2[4] = { 0xfb14, 0xd4dc, 0x31f2, 0x8e3a };

/* {cos(2*(2k)PI/64)} with k = {2,6} */
static const u16 C64_EVEN3[2] = { 0xec84, 0x61f8 };

/* {1/sqrt(2), sqrt(2), 2*sqrt(2), 4*sqrt(2) } */
static const s32 K[] = { 0xb504, 0x16a09, 0x2d413, 0x5a827 };

static const u16 DEWINDOW_LUT[0x420] =
{
    0x0000, 0xfff3, 0x005d, 0xff38, 0x037a, 0xf736, 0x0b37, 0xc00e,
    0x7fff, 0x3ff2, 0x0b37, 0x08ca, 0x037a, 0x00c8, 0x005d, 0x000d,
    0x0000, 0xfff3, 0x005d, 0xff38, 0x037a, 0xf736, 0x0b37, 0xc00e,
    0x7fff, 0x3ff2, 0x0b37, 0x08ca, 0x037a, 0x00c8, 0x005d, 0x000d,
    0x0000, 0xfff2, 0x005f, 0xff1d, 0x0369, 0xf697, 0x0a2a, 0xbce7,
    0x7feb, 0x3ccb, 0x0c2b, 0x082b, 0x0385, 0x00af, 0x005b, 0x000b,
    0x0000, 0xfff2, 0x005f, 0xff1d, 0x0369, 0xf697, 0x0a2a, 0xbce7,
    0x7feb, 0x3ccb, 0x0c2b, 0x082b, 0x0385, 0x00af, 0x005b, 0x000b,
    0x0000, 0xfff1, 0x0061, 0xff02, 0x0354, 0xf5f9, 0x0905, 0xb9c4,
    0x7fb0, 0x39a4, 0x0d08, 0x078c, 0x038c, 0x0098, 0x0058, 0x000a,
    0x0000, 0xfff1, 0x0061, 0xff02, 0x0354, 0xf5f9, 0x0905, 0xb9c4,
    0x7fb0, 0x39a4, 0x0d08, 0x078c, 0x038c, 0x0098, 0x0058, 0x000a,
    0x0000, 0xffef, 0x0062, 0xfee6, 0x033b, 0xf55c, 0x07c8, 0xb6a4,
    0x7f4d, 0x367e, 0x0dce, 0x06ee, 0x038f, 0x0080, 0x0056, 0x0009,
    0x0000, 0xffef, 0x0062, 0xfee6, 0x033b, 0xf55c, 0x07c8, 0xb6a4,
    0x7f4d, 0x367e, 0x0dce, 0x06ee, 0x038f, 0x0080, 0x0056, 0x0009,
    0x0000, 0xffee, 0x0063, 0xfeca, 0x031c, 0xf4c3, 0x0671, 0xb38c,
    0x7ec2, 0x335d, 0x0e7c, 0x0652, 0x038e, 0x006b, 0x0053, 0x0008,
    0x0000, 0xffee, 0x0063, 0xfeca, 0x031c, 0xf4c3, 0x0671, 0xb38c,
    0x7ec2, 0x335d, 0x0e7c, 0x0652, 0x038e, 0x006b, 0x0053, 0x0008,
    0x0000, 0xffec, 0x0064, 0xfeac, 0x02f7, 0xf42c, 0x0502, 0xb07c,
    0x7e12, 0x3041, 0x0f14, 0x05b7, 0x038a, 0x0056, 0x0050, 0x0007,
    0x0000, 0xffec, 0x0064, 0xfeac, 0x02f7, 0xf42c, 0x0502, 0xb07c,
    0x7e12, 0x3041, 0x0f14, 0x05b7, 0x038a, 0x0056, 0x0050, 0x0007,
    0x0000, 0xffeb, 0x0064, 0xfe8e, 0x02ce, 0xf399, 0x037a, 0xad75,
    0x7d3a, 0x2d2c, 0x0f97, 0x0520, 0x0382, 0x0043, 0x004d, 0x0007,
    0x0000, 0xffeb, 0x0064, 0xfe8e, 0x02ce, 0xf399, 0x037a, 0xad75,
    0x7d3a, 0x2d2c, 0x0f97, 0x0520, 0x0382, 0x0043, 0x004d, 0x0007,
    0xffff, 0xffe9, 0x0063, 0xfe6f, 0x029e, 0xf30b, 0x01d8, 0xaa7b,
    0x7c3d, 0x2a1f, 0x1004, 0x048b, 0x0377, 0x0030, 0x004a, 0x0006,
    0xffff, 0xffe9, 0x0063, 0xfe6f, 0x029e, 0xf30b, 0x01d8, 0xaa7b,
    0x7c3d, 0x2a1f, 0x1004, 0x048b, 0x0377, 0x0030, 0x004a, 0x0006,
    0xffff, 0xffe7, 0x0062, 0xfe4f, 0x0269, 0xf282, 0x001f, 0xa78d,
    0x7b1a, 0x271c, 0x105d, 0x03f9, 0x036a, 0x001f, 0x0046, 0x0006,
    0xffff, 0xffe7, 0x0062, 0xfe4f, 0x0269, 0xf282, 0x001f, 0xa78d,
    0x7b1a, 0x271c, 0x105d, 0x03f9, 0x036a, 0x001f, 0x0046, 0x0006,
    0xffff, 0xffe4, 0x0061, 0xfe2f, 0x022f, 0xf1ff, 0xfe4c, 0xa4af,
    0x79d3, 0x2425, 0x10a2, 0x036c, 0x0359, 0x0010, 0x0043, 0x0005,
    0xffff, 0xffe4, 0x0061, 0xfe2f, 0x022f, 0xf1ff, 0xfe4c, 0xa4af,
    0x79d3, 0x2425, 0x10a2, 0x036c, 0x0359, 0x0010, 0x0043, 0x0005,
    0xffff, 0xffe2, 0x005e, 0xfe10, 0x01ee, 0xf184, 0xfc61, 0xa1e1,
    0x7869, 0x2139, 0x10d3, 0x02e3, 0x0346, 0x0001, 0x0040, 0x0004,
    0xffff, 0xffe2, 0x005e, 0xfe10, 0x01ee, 0xf184, 0xfc61, 0xa1e1,
    0x7869, 0x2139, 0x10d3, 0x02e3, 0x0346, 0x0001, 0x0040, 0x0004,
    0xffff, 0xffe0, 0x005b, 0xfdf0, 0x01a8, 0xf111, 0xfa5f, 0x9f27,
    0x76db, 0x1e5c, 0x10f2, 0x025e, 0x0331, 0xfff3, 0x003d, 0x0004,
    0xffff, 0xffe0, 0x005b, 0xfdf0, 0x01a8, 0xf111, 0xfa5f, 0x9f27,
    0x76db, 0x1e5c, 0x10f2, 0x025e, 0x0331, 0xfff3, 0x003d, 0x0004,
    0xffff, 0xffde, 0x0057, 0xfdd0, 0x015b, 0xf0a7, 0xf845, 0x9c80,
    0x752c, 0x1b8e, 0x1100, 0x01de, 0x0319, 0xffe7, 0x003a, 0x0003,
    0xffff, 0xffde, 0x0057, 0xfdd0, 0x015b, 0xf0a7, 0xf845, 0x9c80,
    0x752c, 0x1b8e, 0x1100, 0x01de, 0x0319, 0xffe7, 0x003a, 0x0003,
    0xfffe, 0xffdb, 0x0053, 0xfdb0, 0x0108, 0xf046, 0xf613, 0x99ee,
    0x735c, 0x18d1, 0x10fd, 0x0163, 0x0300, 0xffdc, 0x0037, 0x0003,
    0xfffe, 0xffdb, 0x0053, 0xfdb0, 0x0108, 0xf046, 0xf613, 0x99ee,
    0x735c, 0x18d1, 0x10fd, 0x0163, 0x0300, 0xffdc, 0x0037, 0x0003,
    0xfffe, 0xffd8, 0x004d, 0xfd90, 0x00b0, 0xeff0, 0xf3cc, 0x9775,
    0x716c, 0x1624, 0x10ea, 0x00ee, 0x02e5, 0xffd2, 0x0033, 0x0003,
    0xfffe, 0xffd8, 0x004d, 0xfd90, 0x00b0, 0xeff0, 0xf3cc, 0x9775,
    0x716c, 0x1624, 0x10ea, 0x00ee, 0x02e5, 0xffd2, 0x0033, 0x0003,
    0xfffe, 0xffd6, 0x0047, 0xfd72, 0x0051, 0xefa6, 0xf16f, 0x9514,
    0x6f5e, 0x138a, 0x10c8, 0x007e, 0x02ca, 0xffc9, 0x0030, 0x0003,
    0xfffe, 0xffd6, 0x0047, 0xfd72, 0x0051, 0xefa6, 0xf16f, 0x9514,
    0x6f5e, 0x138a, 0x10c8, 0x007e, 0x02ca, 0xffc9, 0x0030, 0x0003,
    0xfffe, 0xffd3, 0x0040, 0xfd54, 0xffec, 0xef68, 0xeefc, 0x92cd,
    0x6d33, 0x1104, 0x1098, 0x0014, 0x02ac, 0xffc0, 0x002d, 0x0002,
    0xfffe, 0xffd3, 0x0040, 0xfd54, 0xffec, 0xef68, 0xeefc, 0x92cd,
    0x6d33, 0x1104, 0x1098, 0x0014, 0x02ac, 0xffc0, 0x002d, 0x0002,
    0x0030, 0xffc9, 0x02ca, 0x007e, 0x10c8, 0x138a, 0x6f5e, 0x9514,
    0xf16f, 0xefa6, 0x0051, 0xfd72, 0x0047, 0xffd6, 0xfffe, 0x0003,
    0x0030, 0xffc9, 0x02ca, 0x007e, 0x10c8, 0x138a, 0x6f5e, 0x9514,
    0xf16f, 0xefa6, 0x0051, 0xfd72, 0x0047, 0xffd6, 0xfffe, 0x0003,
    0x0033, 0xffd2, 0x02e5, 0x00ee, 0x10ea, 0x1624, 0x716c, 0x9775,
    0xf3cc, 0xeff0, 0x00b0, 0xfd90, 0x004d, 0xffd8, 0xfffe, 0x0003,
    0x0033, 0xffd2, 0x02e5, 0x00ee, 0x10ea, 0x1624, 0x716c, 0x9775,
    0xf3cc, 0xeff0, 0x00b0, 0xfd90, 0x004d, 0xffd8, 0xfffe, 0x0003,
    0x0037, 0xffdc, 0x0300, 0x0163, 0x10fd, 0x18d1, 0x735c, 0x99ee,
    0xf613, 0xf046, 0x0108, 0xfdb0, 0x0053, 0xffdb, 0xfffe, 0x0003,
    0x0037, 0xffdc, 0x0300, 0x0163, 0x10fd, 0x18d1, 0x735c, 0x99ee,
    0xf613, 0xf046, 0x0108, 0xfdb0, 0x0053, 0xffdb, 0xfffe, 0x0003,
    0x003a, 0xffe7, 0x0319, 0x01de, 0x1100, 0x1b8e, 0x752c, 0x9c80,
    0xf845, 0xf0a7, 0x015b, 0xfdd0, 0x0057, 0xffde, 0xffff, 0x0003,
    0x003a, 0xffe7, 0x0319, 0x01de, 0x1100, 0x1b8e, 0x752c, 0x9c80,
    0xf845, 0xf0a7, 0x015b, 0xfdd0, 0x0057, 0xffde, 0xffff, 0x0004,
    0x003d, 0xfff3, 0x0331, 0x025e, 0x10f2, 0x1e5c, 0x76db, 0x9f27,
    0xfa5f, 0xf111, 0x01a8, 0xfdf0, 0x005b, 0xffe0, 0xffff, 0x0004,
    0x003d, 0xfff3, 0x0331, 0x025e, 0x10f2, 0x1e5c, 0x76db, 0x9f27,
    0xfa5f, 0xf111, 0x01a8, 0xfdf0, 0x005b, 0xffe0, 0xffff, 0x0004,
    0x0040, 0x0001, 0x0346, 0x02e3, 0x10d3, 0x2139, 0x7869, 0xa1e1,
    0xfc61, 0xf184, 0x01ee, 0xfe10, 0x005e, 0xffe2, 0xffff, 0x0004,
    0x0040, 0x0001, 0x0346, 0x02e3, 0x10d3, 0x2139, 0x7869, 0xa1e1,
    0xfc61, 0xf184, 0x01ee, 0xfe10, 0x005e, 0xffe2, 0xffff, 0x0005,
    0x0043, 0x0010, 0x0359, 0x036c, 0x10a2, 0x2425, 0x79d3, 0xa4af,
    0xfe4c, 0xf1ff, 0x022f, 0xfe2f, 0x0061, 0xffe4, 0xffff, 0x0005,
    0x0043, 0x0010, 0x0359, 0x036c, 0x10a2, 0x2425, 0x79d3, 0xa4af,
    0xfe4c, 0xf1ff, 0x022f, 0xfe2f, 0x0061, 0xffe4, 0xffff, 0x0006,
    0x0046, 0x001f, 0x036a, 0x03f9, 0x105d, 0x271c, 0x7b1a, 0xa78d,
    0x001f, 0xf282, 0x0269, 0xfe4f, 0x0062, 0xffe7, 0xffff, 0x0006,
    0x0046, 0x001f, 0x036a, 0x03f9, 0x105d, 0x271c, 0x7b1a, 0xa78d,
    0x001f, 0xf282, 0x0269, 0xfe4f, 0x0062, 0xffe7, 0xffff, 0x0006,
    0x004a, 0x0030, 0x0377, 0x048b, 0x1004, 0x2a1f, 0x7c3d, 0xaa7b,
    0x01d8, 0xf30b, 0x029e, 0xfe6f, 0x0063, 0xffe9, 0xffff, 0x0006,
    0x004a, 0x0030, 0x0377, 0x048b, 0x1004, 0x2a1f, 0x7c3d, 0xaa7b,
    0x01d8, 0xf30b, 0x029e, 0xfe6f, 0x0063, 0xffe9, 0xffff, 0x0007,
    0x004d, 0x0043, 0x0382, 0x0520, 0x0f97, 0x2d2c, 0x7d3a, 0xad75,
    0x037a, 0xf399, 0x02ce, 0xfe8e, 0x0064, 0xffeb, 0x0000, 0x0007,
    0x004d, 0x0043, 0x0382, 0x0520, 0x0f97, 0x2d2c, 0x7d3a, 0xad75,
    0x037a, 0xf399, 0x02ce, 0xfe8e, 0x0064, 0xffeb, 0x0000, 0x0007,
    0x0050, 0x0056, 0x038a, 0x05b7, 0x0f14, 0x3041, 0x7e12, 0xb07c,
    0x0502, 0xf42c, 0x02f7, 0xfeac, 0x0064, 0xffec, 0x0000, 0x0007,
    0x0050, 0x0056, 0x038a, 0x05b7, 0x0f14, 0x3041, 0x7e12, 0xb07c,
    0x0502, 0xf42c, 0x02f7, 0xfeac, 0x0064, 0xffec, 0x0000, 0x0008,
    0x0053, 0x006b, 0x038e, 0x0652, 0x0e7c, 0x335d, 0x7ec2, 0xb38c,
    0x0671, 0xf4c3, 0x031c, 0xfeca, 0x0063, 0xffee, 0x0000, 0x0008,
    0x0053, 0x006b, 0x038e, 0x0652, 0x0e7c, 0x335d, 0x7ec2, 0xb38c,
    0x0671, 0xf4c3, 0x031c, 0xfeca, 0x0063, 0xffee, 0x0000, 0x0009,
    0x0056, 0x0080, 0x038f, 0x06ee, 0x0dce, 0x367e, 0x7f4d, 0xb6a4,
    0x07c8, 0xf55c, 0x033b, 0xfee6, 0x0062, 0xffef, 0x0000, 0x0009,
    0x0056, 0x0080, 0x038f, 0x06ee, 0x0dce, 0x367e, 0x7f4d, 0xb6a4,
    0x07c8, 0xf55c, 0x033b, 0xfee6, 0x0062, 0xffef, 0x0000, 0x000a,
    0x0058, 0x0098, 0x038c, 0x078c, 0x0d08, 0x39a4, 0x7fb0, 0xb9c4,
    0x0905, 0xf5f9, 0x0354, 0xff02, 0x0061, 0xfff1, 0x0000, 0x000a,
    0x0058, 0x0098, 0x038c, 0x078c, 0x0d08, 0x39a4, 0x7fb0, 0xb9c4,
    0x0905, 0xf5f9, 0x0354, 0xff02, 0x0061, 0xfff1, 0x0000, 0x000b,
    0x005b, 0x00af, 0x0385, 0x082b, 0x0c2b, 0x3ccb, 0x7feb, 0xbce7,
    0x0a2a, 0xf697, 0x0369, 0xff1d, 0x005f, 0xfff2, 0x0000, 0x000b,
    0x005b, 0x00af, 0x0385, 0x082b, 0x0c2b, 0x3ccb, 0x7feb, 0xbce7,
    0x0a2a, 0xf697, 0x0369, 0xff1d, 0x005f, 0xfff2, 0x0000, 0x000d,
    0x005d, 0x00c8, 0x037a, 0x08ca, 0x0b37, 0x3ff2, 0x7fff, 0xc00e,
    0x0b37, 0xf736, 0x037a, 0xff38, 0x005d, 0xfff3, 0x0000, 0x000d,
    0x005d, 0x00c8, 0x037a, 0x08ca, 0x0b37, 0x3ff2, 0x7fff, 0xc00e,
    0x0b37, 0xf736, 0x037a, 0xff38, 0x005d, 0xfff3, 0x0000, 0x0000
};

static s16 clamp_s16(s32 x)
{
    if (x > 32767) { x = 32767; } else if (x < -32768) { x = -32768; }
    return x;
}

static s32 mul(s32 x, s32 y)
{
    return (x*y) >> 16;
}

static s32 dmul_round(s16 x, s16 y)
{
    return ((s32)x * (s32)y + 0x4000) >> 15;
}

static void smul(s16 *x, s16 gain)
{
    *x = clamp_s16((s32)(*x) * (s32)gain);
}

static s16* sample_at(u16 mem)
{
    assert((mem & 0x1) == 0);
    assert((mem & ~0xfff) == 0);

    return (s16*)(mp3data+(mem^S16));
}

static void swap(u32 *a, u32 *b)
{
    u32 tmp = *b;
    *b = *a;
    *a = tmp;
}

/* global function */
void mp3_decode(u32 address, unsigned char index)
{
    // Initialization Code
    u32 readPtr; // s5
    u32 writePtr; // s6
    //u32 Count = 0x0480; // s4
    u32 inPtr, outPtr;
    int cnt, cnt2;

    u32 t6 = 0x08A0; // I think these are temporary storage buffers
    u32 t5 = 0x0AC0;
    u32 t4 = index;

    writePtr = address;
    readPtr  = writePtr;

    memcpy (mp3data+0xce8, rsp.RDRAM+readPtr, 8);
    s16 mult6 = *(s32*)(mp3data+0xce8) >> 16;
    s16 mult4 = *(s32*)(mp3data+0xcec) >> 16;
    readPtr += 8;

    for (cnt = 0; cnt < 0x480; cnt += 0x180)
    {
        /* buffer 6*32 frequency lines */
        memcpy (mp3data+0xCF0, rsp.RDRAM+readPtr, 0x180); // DMA: 0xCF0 <- RDRAM[s5] : 0x180
        inPtr  = 0xCF0; // s7
        outPtr = 0xE70; // s3

        /* process them */
        for (cnt2 = 0; cnt2 < 0x180; cnt2 += 0x40)
        {
            t6 = (t6 & 0xffe0) | (t4 << 1);
            t5 = (t5 & 0xffe0) | (t4 << 1);

            /* synthesis polyphase filter bank */
            mdct32(inPtr, t5, t6);
            window(t4, t6, outPtr);
            apply_gain(outPtr + 0x00, 17, mult6);
            apply_gain(outPtr + 0x22, 16, (t4 & 0x1) ? mult4 : mult6);
                
            t4 = (t4 - 1) & 0x0f;
            swap(&t5, &t6);
            outPtr += 0x40;
            inPtr += 0x40;
        }

        /* flush results */
        memcpy (rsp.RDRAM+writePtr, mp3data+0xe70, 0x180);
        writePtr += 0x180;
        readPtr  += 0x180;
    }
}

/* local functions */
static void MP3AB0(s32 *v)
{
    unsigned i;

    /* 1 8-wide butterfly */
    for (i = 0; i < 8; ++i)
        butterfly(&v[0+i], &v[8+i], C64_EVEN1[i]);

    /* 2 4-wide butterfly */
    for (i = 0; i < 4; ++i)
    {
        butterfly(&v[0+i], &v[ 4+i], C64_EVEN2[i]);
        butterfly(&v[8+i], &v[12+i], C64_EVEN2[i]);
    }

    /* 4 2-wide butterfly */
    for (i = 0; i < 16; i+=4)
    {
        butterfly(&v[0+i], &v[2+i], C64_EVEN3[0]);
        butterfly(&v[1+i], &v[3+i], C64_EVEN3[1]);
    }

    /* 8 1-wide butterfly */
    butterfly(&v[ 0], &v[ 1], K[0]);
    butterfly(&v[ 2], &v[ 3], K[1]);
    butterfly(&v[ 4], &v[ 5], K[1]);
    butterfly(&v[ 6], &v[ 7], K[2]);
    butterfly(&v[ 8], &v[ 9], K[1]);
    butterfly(&v[10], &v[11], K[2]);
    butterfly(&v[12], &v[13], K[2]);
    butterfly(&v[14], &v[15], K[3]);

    v[6]  <<= 1;
    v[10] <<= 1;
    v[12] <<= 1;
    v[14] <<= 2;
}

static void load_v(s32 *v, u16 inPtr)
{
    v[0] = *sample_at(inPtr+0x00) + *sample_at(inPtr+0x3E);
    v[1] = *sample_at(inPtr+0x02) + *sample_at(inPtr+0x3C);
    v[2] = *sample_at(inPtr+0x06) + *sample_at(inPtr+0x38);
    v[3] = *sample_at(inPtr+0x04) + *sample_at(inPtr+0x3A);
    v[4] = *sample_at(inPtr+0x0E) + *sample_at(inPtr+0x30);
    v[5] = *sample_at(inPtr+0x0C) + *sample_at(inPtr+0x32);
    v[6] = *sample_at(inPtr+0x08) + *sample_at(inPtr+0x36);
    v[7] = *sample_at(inPtr+0x0A) + *sample_at(inPtr+0x34);
    v[8] = *sample_at(inPtr+0x1E) + *sample_at(inPtr+0x20);
    v[9] = *sample_at(inPtr+0x1C) + *sample_at(inPtr+0x22);
    v[10]= *sample_at(inPtr+0x18) + *sample_at(inPtr+0x26);
    v[11]= *sample_at(inPtr+0x1A) + *sample_at(inPtr+0x24);
    v[12]= *sample_at(inPtr+0x10) + *sample_at(inPtr+0x2E);
    v[13]= *sample_at(inPtr+0x12) + *sample_at(inPtr+0x2C);
    v[14]= *sample_at(inPtr+0x16) + *sample_at(inPtr+0x28);
    v[15]= *sample_at(inPtr+0x14) + *sample_at(inPtr+0x2A);
}

/* Looks like a 32 point MDCT (not sure)
 * It outputs 33 values (spaced by 30 bytes) */
static void mdct32(u32 inPtr, u32 t5, u32 t6)
{
    s32 v[16];
    s32 t[16]; // temporary values
    int i;

    load_v(v, inPtr);

    MP3AB0(v);

    t[5]  = v[5] + v[4];
    t[2]  = v[8] + v[9];
    t[3]  = v[8] + v[10];
    t[13] = v[13] - t[2] + v[12];
    t[14] = t[3] - v[14];
    t[15] = v[15] - t[2] - v[11];
    
    *(s16*)(mp3data+((t6 + 0x000))) = (s16)v[1];
    *(s16*)(mp3data+((t6 + 0x040))) = (s16)(v[9] + t[14]);
    *(s16*)(mp3data+((t6 + 0x080))) = (s16)(t[5] - v[6]);
    *(s16*)(mp3data+((t6 + 0x0c0))) = (s16)(t[13] - v[10]);
    *(s16*)(mp3data+((t6 + 0x100))) = (s16)(v[3] - v[2]);
    *(s16*)(mp3data+((t6 + 0x140))) = (s16)(v[11] - t[13]);
    *(s16*)(mp3data+((t6 + 0x180))) = (s16)(v[7] - t[5]);
    *(s16*)(mp3data+((t6 + 0x1c0))) = (s16)t[15];

    *(s16*)(mp3data+((t5 + 0x000))) = (s16)(-v[1]);
    *(s16*)(mp3data+((t5 + 0x040))) = (s16)t[14];
    *(s16*)(mp3data+((t5 + 0x080))) = (s16)(v[4] - v[6]);
    *(s16*)(mp3data+((t5 + 0x0c0))) = (s16)(v[12] - v[10] - v[8]);
    *(s16*)(mp3data+((t5 + 0x100))) = (s16)(-v[2]);
    *(s16*)(mp3data+((t5 + 0x140))) = (s16)(v[8] - v[12]);
    *(s16*)(mp3data+((t5 + 0x180))) = (s16)(-v[4]);
    *(s16*)(mp3data+((t5 + 0x1c0))) = (s16)(-v[8]);
    *(s16*)(mp3data+((t5 + 0x200))) = (s16)(-v[0]);

    load_v(v, inPtr);

    for (i = 0; i < 16; i++)
        v[i] = mul(v[i], C64_ODD[i]);
    
    MP3AB0(v);

    t[0] = v[0] >> 1;
    t[4] = v[4] + t[0];
    t[5] = v[5] + v[1];
    t[6] = v[6] + t[0] + v[2];
    t[7] = v[7] + t[0] + v[1] + v[3];
    t[10] = v[10] + v[8];
    t[11] = v[11] + v[8] + v[9];
    t[12] = t[4] - v[12];
    t[13] = v[13] - t[12] - t[5];
    t[14] = t[6] - v[14];
    t[15] = v[15] - t[7];
    t[5] = t[4] + t[5];
    t[9] = v[9] + t[10];

    *(s16 *)(mp3data+((t5 + 0x020))) = (s16)t[14];
    *(s16 *)(mp3data+((t5 + 0x060))) = (s16)(t[10] - t[6]);
    *(s16 *)(mp3data+((t5 + 0x0a0))) = (s16)(t[4] - t[10] + v[2]);
    *(s16 *)(mp3data+((t5 + 0x0e0))) = (s16)(-t[12] - v[2]);
    *(s16 *)(mp3data+((t5 + 0x120))) = (s16)t[12];
    *(s16 *)(mp3data+((t5 + 0x160))) = (s16)(v[8] - t[4]);
    *(s16 *)(mp3data+((t5 + 0x1a0))) = (s16)(t[0] - v[8]);
    *(s16 *)(mp3data+((t5 + 0x1e0))) = (s16)(-t[0]);
    
    *(s16 *)(mp3data+((t6 + 0x020))) = (s16)(t[14] + v[1]);
    *(s16 *)(mp3data+((t6 + 0x060))) = (s16)(t[9] - v[1] - t[6]);
    *(s16 *)(mp3data+((t6 + 0x0a0))) = (s16)(t[5] + v[2] - t[9]);
    *(s16 *)(mp3data+((t6 + 0x0e0))) = (s16)(t[13] - v[2]);
    *(s16 *)(mp3data+((t6 + 0x120))) = (s16)(v[3] - t[13]);
    *(s16 *)(mp3data+((t6 + 0x160))) = (s16)(t[11] - v[3] - t[5]);
    *(s16 *)(mp3data+((t6 + 0x1a0))) = (s16)(t[7] - t[11]);
    *(s16 *)(mp3data+((t6 + 0x1e0))) = (s16)t[15];
}

static void window(u32 t4, u32 t6, u32 outPtr)
{
    unsigned i;
    u32 offset = 0x10-t4;
    u32 addptr = t6 & 0xFFE0;
    s32 v2=0, v4=0, v6=0;

    for (i = 0; i < 16; ++i)
    {
        idot8(&v2, &v6, (s16*)(mp3data + addptr), (s16*)(DEWINDOW_LUT + offset));
        // clamp ?
        *sample_at(outPtr) = v2 + v6;

        outPtr += 2;
        addptr += 0x20;
        offset += 0x20;
    }

    idot8(&v2, &v4, (s16*)(mp3data + addptr), (s16*)(DEWINDOW_LUT + offset));
    *sample_at(outPtr) = (t4 & 0x1) ? v2 : v4;
    outPtr += 2;

    addptr -= 0x40;
    offset  = 0x22f - t4;
    for (i = 0; i < 8; ++i)
    {
        idot8(&v2, &v4, (s16*)(mp3data+addptr+0x20), (s16*)(DEWINDOW_LUT + offset + 0x00));
        v2 -= v4;
        idot8(&v6, &v4, (s16*)(mp3data+addptr+0x00), (s16*)(DEWINDOW_LUT + offset + 0x20));
        v6 -= v4;
        // clamp ?
        *sample_at(outPtr) = v2;
        *sample_at(outPtr + 2) = v6;
        
        outPtr += 4;
        addptr -= 0x40;
        offset += 0x40;
    }
}

static void apply_gain(u16 mem, unsigned count, s16 gain)
{
    while (count != 0)
    {
        smul(sample_at(mem), gain);
        mem += 2;
        --count;
    }
}

static void idot8(s32 *dot1, s32 *dot2, const s16 *x, const s16 *y)
{
    unsigned i;

    *dot1 = 0;
    *dot2 = 0;
    
    for(i = 0; i < 8; ++i)
    {
        *dot1 += dmul_round(*(x++), *(y++));
        *dot2 += dmul_round(*(x++), *(y++));
    }
}

static void butterfly(s32 *x, s32 *y, s32 w)
{
    s32 sum  = *x + *y;
    s32 diff = *x - *y;

    *x = sum;
    *y = mul(diff, w);
}

