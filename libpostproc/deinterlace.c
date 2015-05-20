/*
 * Copyright (C) 2001-2003 Michael Niedermayer (michaelni@gmx.at)
 * Copyright (C) 2015 Tucker DiNapoli (T.DiNapoli at gmail.com)
 *
 * AltiVec optimizations (C) 2004 Romain Dolbeau <romain@dolbeau.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "postprocess_internal.h"
/**
 * Deinterlace the given block by linearly interpolating every second line.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 */
void deinterlace_interpolate_linear_C(uint8_t src[], int stride)
{
    int a, b, x;
    src+= 4*stride;

    for(x=0; x<2; x++){
        a= *(uint32_t*)&src[stride*0];
        b= *(uint32_t*)&src[stride*2];
        *(uint32_t*)&src[stride*1]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);
        a= *(uint32_t*)&src[stride*4];
        *(uint32_t*)&src[stride*3]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);
        b= *(uint32_t*)&src[stride*6];
        *(uint32_t*)&src[stride*5]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);
        a= *(uint32_t*)&src[stride*8];
        *(uint32_t*)&src[stride*7]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);
        src += 4;
    }
}

/**
 * Deinterlace the given block by cubic interpolating every second line.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 3-15 and write 7-13
 */
void d_interlace_interpolate_cubic_C(uint8_t src[], int stride)
{
    int x;
    src+= stride*3;
    for(x=0; x<8; x++){
        src[stride*3] = av_clip_uint8((-src[0]        + 9*src[stride*2] + 9*src[stride*4] - src[stride*6])>>4);
        src[stride*5] = av_clip_uint8((-src[stride*2] + 9*src[stride*4] + 9*src[stride*6] - src[stride*8])>>4);
        src[stride*7] = av_clip_uint8((-src[stride*4] + 9*src[stride*6] + 9*src[stride*8] - src[stride*10])>>4);
        src[stride*9] = av_clip_uint8((-src[stride*6] + 9*src[stride*8] + 9*src[stride*10] - src[stride*12])>>4);
        src++;
    }
}

/**
 * Deinterlace the given block by filtering every second line with a (-1 4 2 4 -1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 5-11
 */
void deinterlace_FF_C(uint8_t src[], int stride, uint8_t *tmp)
{
    int x;
    src+= stride*4;
    for(x=0; x<8; x++){
        int t1= tmp[x];
        int t2= src[stride*1];

        src[stride*1]= av_clip_uint8((-t1 + 4*src[stride*0] + 2*t2 + 4*src[stride*2] - src[stride*3] + 4)>>3);
        t1= src[stride*4];
        src[stride*3]= av_clip_uint8((-t2 + 4*src[stride*2] + 2*t1 + 4*src[stride*4] - src[stride*5] + 4)>>3);
        t2= src[stride*6];
        src[stride*5]= av_clip_uint8((-t1 + 4*src[stride*4] + 2*t2 + 4*src[stride*6] - src[stride*7] + 4)>>3);
        t1= src[stride*8];
        src[stride*7]= av_clip_uint8((-t2 + 4*src[stride*6] + 2*t1 + 4*src[stride*8] - src[stride*9] + 4)>>3);
        tmp[x]= t1;

        src++;
    }
}

/**
 * Deinterlace the given block by filtering every line with a (-1 2 6 2 -1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 4-11
 */
void deinterlace_l5_C(uint8_t src[], int stride, uint8_t *tmp, uint8_t *tmp2)
{
    int x;
    src+= stride*4;
    for(x=0; x<8; x++){
        int t1= tmp[x];
        int t2= tmp2[x];
        int t3= src[0];

        src[stride*0]= av_clip_uint8((-(t1 + src[stride*2]) + 2*(t2 + src[stride*1]) + 6*t3 + 4)>>3);
        t1= src[stride*1];
        src[stride*1]= av_clip_uint8((-(t2 + src[stride*3]) + 2*(t3 + src[stride*2]) + 6*t1 + 4)>>3);
        t2= src[stride*2];
        src[stride*2]= av_clip_uint8((-(t3 + src[stride*4]) + 2*(t1 + src[stride*3]) + 6*t2 + 4)>>3);
        t3= src[stride*3];
        src[stride*3]= av_clip_uint8((-(t1 + src[stride*5]) + 2*(t2 + src[stride*4]) + 6*t3 + 4)>>3);
        t1= src[stride*4];
        src[stride*4]= av_clip_uint8((-(t2 + src[stride*6]) + 2*(t3 + src[stride*5]) + 6*t1 + 4)>>3);
        t2= src[stride*5];
        src[stride*5]= av_clip_uint8((-(t3 + src[stride*7]) + 2*(t1 + src[stride*6]) + 6*t2 + 4)>>3);
        t3= src[stride*6];
        src[stride*6]= av_clip_uint8((-(t1 + src[stride*8]) + 2*(t2 + src[stride*7]) + 6*t3 + 4)>>3);
        t1= src[stride*7];
        src[stride*7]= av_clip_uint8((-(t2 + src[stride*9]) + 2*(t3 + src[stride*8]) + 6*t1 + 4)>>3);

        tmp[x]= t3;
        tmp2[x]= t1;

        src++;
    }
}

/**
 * Deinterlace the given block by filtering all lines with a (1 2 1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 4-11
 */
void deinterlace_blend_linear_C(uint8_t src[], int stride, uint8_t *tmp)
{
    int a, b, c, x;
    src+= 4*stride;

    for(x=0; x<2; x++){
        a= *(uint32_t*)&tmp[stride*0];
        b= *(uint32_t*)&src[stride*0];
        c= *(uint32_t*)&src[stride*1];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*0]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*2];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*1]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        b= *(uint32_t*)&src[stride*3];
        c= (b&c) + (((b^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*2]= (c|a) - (((c^a)&0xFEFEFEFEUL)>>1);

        c= *(uint32_t*)&src[stride*4];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*3]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*5];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*4]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        b= *(uint32_t*)&src[stride*6];
        c= (b&c) + (((b^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*5]= (c|a) - (((c^a)&0xFEFEFEFEUL)>>1);

        c= *(uint32_t*)&src[stride*7];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*6]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*8];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*7]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        *(uint32_t*)&tmp[stride*0]= c;
        src += 4;
        tmp += 4;
    }
}

/**
 * Deinterlace the given block by applying a median filter to every second line.
 * will be called for every 8x8 block and can read & write from line 4-15,
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 */
void deinterlace_median_C(uint8_t src[], int stride)
{
    int x, y;
    src+= 4*stride;
    // FIXME - there should be a way to do a few columns in parallel like w/mmx
    for(x=0; x<8; x++){
        uint8_t *colsrc = src;
        for (y=0; y<4; y++){
            int a, b, c, d, e, f;
            a = colsrc[0      ];
            b = colsrc[stride ];
            c = colsrc[stride*2];
            d = (a-b)>>31;
            e = (b-c)>>31;
            f = (c-a)>>31;
            colsrc[stride ] = (a|(d^f)) & (b|(d^e)) & (c|(e^f));
            colsrc += stride*2;
        }
        src++;
    }
}
