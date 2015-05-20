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
/* The horizontal functions exist only in C because the MMX
 * code is faster with vertical filters and transposing. */

/**
 * Check if the given 8x8 Block is mostly "flat"
 */
static inline int is_horiz_DC_C(const uint8_t src[], int stride, const PPContext *c)
{
    int numEq= 0;
    int y;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;

    for(y=0; y<8; y++){
        numEq += ((unsigned)(src[0] - src[1] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[1] - src[2] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[2] - src[3] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[3] - src[4] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[4] - src[5] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[5] - src[6] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[6] - src[7] + dcOffset)) < dcThreshold;
        src+= stride;
    }
    return numEq > c->ppMode.flatnessThreshold;
}

/**
 * Check if the middle 8x8 Block in the given 8x16 block is flat
 */
static inline int is_vert_DC_C(const uint8_t src[], int stride, const PPContext *c)
{
    int numEq= 0;
    int y;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;

    src+= stride*4; // src points to begin of the 8x8 Block
    for(y=0; y<8-1; y++){
        numEq += ((unsigned)(src[0] - src[0+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[1] - src[1+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[2] - src[2+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[3] - src[3+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[4] - src[4+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[5] - src[5+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[6] - src[6+stride] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[7] - src[7+stride] + dcOffset)) < dcThreshold;
        src+= stride;
    }
    return numEq > c->ppMode.flatnessThreshold;
}

static inline int is_horiz_min_max_ok_C(const uint8_t src[], int stride, int QP)
{
    int i;
    for(i=0; i<2; i++){
        if((unsigned)(src[0] - src[5] + 2*QP) > 4*QP) return 0;
        src += stride;
        if((unsigned)(src[2] - src[7] + 2*QP) > 4*QP) return 0;
        src += stride;
        if((unsigned)(src[4] - src[1] + 2*QP) > 4*QP) return 0;
        src += stride;
        if((unsigned)(src[6] - src[3] + 2*QP) > 4*QP) return 0;
        src += stride;
    }
    return 1;
}

static inline int is_vert_min_max_ok_C(const uint8_t src[], int stride, int QP)
{
    int x;
    src+= stride*4;
    for(x=0; x<8; x+=4){
        if((unsigned)(src[  x + 0*stride] - src[  x + 5*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[1+x + 2*stride] - src[1+x + 7*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[2+x + 4*stride] - src[2+x + 1*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[3+x + 6*stride] - src[3+x + 3*stride] + 2*QP) > 4*QP) return 0;
    }
    return 1;
}

int horiz_classify_C(const uint8_t src[], int stride, const PPContext *c)
{
    if(is_horiz_DC_C(src, stride, c)){
        return is_horiz_min_max_ok_C(src, stride, c->QP);
    } else {
        return 2;
    }
}

int vert_classify_C(const uint8_t src[], int stride, const PPContext *c)
{
    if(is_vert_DC_C(src, stride, c)){
        return is_vert_min_max_ok_C(src, stride, c->QP);
    } else {
        return 2;
    }
}

void do_horiz_def_filter_C(uint8_t dst[], int stride, const PPContext *c)
{
    int y;
    for(y=0; y<8; y++){
        const int middleEnergy= 5*(dst[4] - dst[3]) + 2*(dst[2] - dst[5]);

        if(FFABS(middleEnergy) < 8*c->QP){
            const int q=(dst[3] - dst[4])/2;
            const int leftEnergy=  5*(dst[2] - dst[1]) + 2*(dst[0] - dst[3]);
            const int rightEnergy= 5*(dst[6] - dst[5]) + 2*(dst[4] - dst[7]);

            int d= FFABS(middleEnergy) - FFMIN( FFABS(leftEnergy), FFABS(rightEnergy) );
            d= FFMAX(d, 0);

            d= (5*d + 32) >> 6;
            d*= FFSIGN(-middleEnergy);

            if(q>0)
            {
                d = FFMAX(d, 0);
                d = FFMIN(d, q);
            }
            else
            {
                d = FFMIN(d, 0);
                d = FFMAX(d, q);
            }

            dst[3]-= d;
            dst[4]+= d;
        }
        dst+= stride;
    }
}

/**
 * Do a horizontal low pass filter on the 10x8 block (dst points to middle 8x8 Block)
 * using the 9-Tap Filter (1,1,2,2,4,2,2,1,1)/16 (C version)
 */
void do_horiz_low_pass_C(uint8_t dst[], int stride, const PPContext *c)
{
    int y;
    for(y=0; y<8; y++){
        const int first= FFABS(dst[-1] - dst[0]) < c->QP ? dst[-1] : dst[0];
        const int last= FFABS(dst[8] - dst[7]) < c->QP ? dst[8] : dst[7];

        int sums[10];
        sums[0] = 4*first + dst[0] + dst[1] + dst[2] + 4;
        sums[1] = sums[0] - first  + dst[3];
        sums[2] = sums[1] - first  + dst[4];
        sums[3] = sums[2] - first  + dst[5];
        sums[4] = sums[3] - first  + dst[6];
        sums[5] = sums[4] - dst[0] + dst[7];
        sums[6] = sums[5] - dst[1] + last;
        sums[7] = sums[6] - dst[2] + last;
        sums[8] = sums[7] - dst[3] + last;
        sums[9] = sums[8] - dst[4] + last;

        dst[0]= (sums[0] + sums[2] + 2*dst[0])>>4;
        dst[1]= (sums[1] + sums[3] + 2*dst[1])>>4;
        dst[2]= (sums[2] + sums[4] + 2*dst[2])>>4;
        dst[3]= (sums[3] + sums[5] + 2*dst[3])>>4;
        dst[4]= (sums[4] + sums[6] + 2*dst[4])>>4;
        dst[5]= (sums[5] + sums[7] + 2*dst[5])>>4;
        dst[6]= (sums[6] + sums[8] + 2*dst[6])>>4;
        dst[7]= (sums[7] + sums[9] + 2*dst[7])>>4;

        dst+= stride;
    }
}

/**
 * Experimental Filter 1 (Horizontal)
 * will not damage linear gradients
 * Flat blocks should look like they were passed through the (1,1,2,2,4,2,2,1,1) 9-Tap filter
 * can only smooth blocks at the expected locations (it cannot smooth them if they did move)
 * MMX2 version does correct clipping C version does not
 * not identical with the vertical one
 */
void horiz_X1_filter_C(uint8_t *src, int stride, int QP)
{
    int y;
    static uint64_t lut[256];
    if(!lut[255])
    {
        int i;
        for(i=0; i<256; i++)
        {
            int v= i < 128 ? 2*i : 2*(i-256);
/*
//Simulate 112242211 9-Tap filter
            uint64_t a= (v/16)  & 0xFF;
            uint64_t b= (v/8)   & 0xFF;
            uint64_t c= (v/4)   & 0xFF;
            uint64_t d= (3*v/8) & 0xFF;
*/
//Simulate piecewise linear interpolation
            uint64_t a= (v/16)   & 0xFF;
            uint64_t b= (v*3/16) & 0xFF;
            uint64_t c= (v*5/16) & 0xFF;
            uint64_t d= (7*v/16) & 0xFF;
            uint64_t A= (0x100 - a)&0xFF;
            uint64_t B= (0x100 - b)&0xFF;
            uint64_t C= (0x100 - c)&0xFF;
            uint64_t D= (0x100 - c)&0xFF;

            lut[i]   = (a<<56) | (b<<48) | (c<<40) | (d<<32) |
                       (D<<24) | (C<<16) | (B<<8)  | (A);
            //lut[i] = (v<<32) | (v<<24);
        }
    }

    for(y=0; y<8; y++){
        int a= src[1] - src[2];
        int b= src[3] - src[4];
        int c= src[5] - src[6];

        int d= FFMAX(FFABS(b) - (FFABS(a) + FFABS(c))/2, 0);

        if(d < QP){
            int v = d * FFSIGN(-b);

            src[1] +=v/8;
            src[2] +=v/4;
            src[3] +=3*v/8;
            src[4] -=3*v/8;
            src[5] -=v/4;
            src[6] -=v/8;
        }
        src+=stride;
    }
}

/**
 * accurate deblock filter
 */
void do_a_deblock_C(uint8_t *src, int step,
                    int stride, const PPContext *c, int mode)
{
    int y;
    const int QP= c->QP;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;

    src+= step*4; // src points to begin of the 8x8 Block
    for(y=0; y<8; y++){
        int numEq= 0;

        numEq += ((unsigned)(src[-1*step] - src[0*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 0*step] - src[1*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 1*step] - src[2*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 2*step] - src[3*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 3*step] - src[4*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 4*step] - src[5*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 5*step] - src[6*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 6*step] - src[7*step] + dcOffset)) < dcThreshold;
        numEq += ((unsigned)(src[ 7*step] - src[8*step] + dcOffset)) < dcThreshold;
        if(numEq > c->ppMode.flatnessThreshold){
            int min, max, x;

            if(src[0] > src[step]){
                max= src[0];
                min= src[step];
            } else {
                max= src[step];
                min= src[0];
            }
            for(x=2; x<8; x+=2){
                if(src[x*step] > src[(x+1)*step]){
                        if(src[x*step] > max) max= src[x*step];
                        if(src[(x+1)*step] < min) min= src[(x+1)*step];
                } else {
                        if(src[(x+1)*step] > max) max= src[(x+1)*step];
                        if(src[x*step] < min) min= src[x*step];
                }
            }
            if(max-min < 2*QP){
                const int first= FFABS(src[-1*step] - src[0]) < QP ? src[-1*step] : src[0];
                const int last= FFABS(src[8*step] - src[7*step]) < QP ? src[8*step] : src[7*step];

                int sums[10];
                sums[0] = 4*first + src[0*step] + src[1*step] + src[2*step] + 4;
                sums[1] = sums[0] - first       + src[3*step];
                sums[2] = sums[1] - first       + src[4*step];
                sums[3] = sums[2] - first       + src[5*step];
                sums[4] = sums[3] - first       + src[6*step];
                sums[5] = sums[4] - src[0*step] + src[7*step];
                sums[6] = sums[5] - src[1*step] + last;
                sums[7] = sums[6] - src[2*step] + last;
                sums[8] = sums[7] - src[3*step] + last;
                sums[9] = sums[8] - src[4*step] + last;

                if (mode & VISUALIZE) {
                    src[0*step] =
                    src[1*step] =
                    src[2*step] =
                    src[3*step] =
                    src[4*step] =
                    src[5*step] =
                    src[6*step] =
                    src[7*step] = 128;
                }
                src[0*step]= (sums[0] + sums[2] + 2*src[0*step])>>4;
                src[1*step]= (sums[1] + sums[3] + 2*src[1*step])>>4;
                src[2*step]= (sums[2] + sums[4] + 2*src[2*step])>>4;
                src[3*step]= (sums[3] + sums[5] + 2*src[3*step])>>4;
                src[4*step]= (sums[4] + sums[6] + 2*src[4*step])>>4;
                src[5*step]= (sums[5] + sums[7] + 2*src[5*step])>>4;
                src[6*step]= (sums[6] + sums[8] + 2*src[6*step])>>4;
                src[7*step]= (sums[7] + sums[9] + 2*src[7*step])>>4;
            }
        } else {
            const int middleEnergy= 5*(src[4*step] - src[3*step]) + 2*(src[2*step] - src[5*step]);

            if(FFABS(middleEnergy) < 8*QP){
                const int q=(src[3*step] - src[4*step])/2;
                const int leftEnergy=  5*(src[2*step] - src[1*step]) + 2*(src[0*step] - src[3*step]);
                const int rightEnergy= 5*(src[6*step] - src[5*step]) + 2*(src[4*step] - src[7*step]);

                int d = FFABS(middleEnergy) - FFMIN( FFABS(leftEnergy), FFABS(rightEnergy) );
                d = FFMAX(d, 0);

                d = (5*d + 32) >> 6;
                d *= FFSIGN(-middleEnergy);

                if(q>0){
                    d = FFMAX(d, 0);
                    d = FFMIN(d, q);
                } else {
                    d = FFMIN(d, 0);
                    d = FFMAX(d, q);
                }

                if ((mode & VISUALIZE) && d) {
                    d = (d < 0) ? 32 : -32;
                    src[3*step]= av_clip_uint8(src[3*step] - d);
                    src[4*step]= av_clip_uint8(src[4*step] + d);
                    d = 0;
                }

                src[3*step] -= d;
                src[4*step] += d;
            }
        }
        src += stride;
    }
}

/**
 * Do a vertical low pass filter on the 8x16 block (only write to the 8x8 block in the middle)
 * using the 9-Tap Filter (1,1,2,2,4,2,2,1,1)/16
 */
void do_vert_low_pass_C(uint8_t *src, int stride, PPContext *c)
{
    const int l1= stride;
    const int l2= stride + l1;
    const int l3= stride + l2;
    const int l4= stride + l3;
    const int l5= stride + l4;
    const int l6= stride + l5;
    const int l7= stride + l6;
    const int l8= stride + l7;
    const int l9= stride + l8;
    int x;
    src+= stride*3;
    for(x=0; x<8; x++){
        const int first= FFABS(src[0] - src[l1]) < c->QP ? src[0] : src[l1];
        const int last= FFABS(src[l8] - src[l9]) < c->QP ? src[l9] : src[l8];

        int sums[10];
        sums[0] = 4*first + src[l1] + src[l2] + src[l3] + 4;
        sums[1] = sums[0] - first  + src[l4];
        sums[2] = sums[1] - first  + src[l5];
        sums[3] = sums[2] - first  + src[l6];
        sums[4] = sums[3] - first  + src[l7];
        sums[5] = sums[4] - src[l1] + src[l8];
        sums[6] = sums[5] - src[l2] + last;
        sums[7] = sums[6] - src[l3] + last;
        sums[8] = sums[7] - src[l4] + last;
        sums[9] = sums[8] - src[l5] + last;

        src[l1]= (sums[0] + sums[2] + 2*src[l1])>>4;
        src[l2]= (sums[1] + sums[3] + 2*src[l2])>>4;
        src[l3]= (sums[2] + sums[4] + 2*src[l3])>>4;
        src[l4]= (sums[3] + sums[5] + 2*src[l4])>>4;
        src[l5]= (sums[4] + sums[6] + 2*src[l5])>>4;
        src[l6]= (sums[5] + sums[7] + 2*src[l6])>>4;
        src[l7]= (sums[6] + sums[8] + 2*src[l7])>>4;
        src[l8]= (sums[7] + sums[9] + 2*src[l8])>>4;

        src++;
    }
}

/**
 * Experimental Filter 1
 * will not damage linear gradients
 * Flat blocks should look like they were passed through the (1,1,2,2,4,2,2,1,1) 9-Tap filter
 * can only smooth blocks at the expected locations (it cannot smooth them if they did move)
 * MMX2 version does correct clipping C version does not
 */
void vert_X1_filter_C(uint8_t *src, int stride, PPContext *co)
{
    const int l1= stride;
    const int l2= stride + l1;
    const int l3= stride + l2;
    const int l4= stride + l3;
    const int l5= stride + l4;
    const int l6= stride + l5;
    const int l7= stride + l6;
//    const int l8= stride + l7;
//    const int l9= stride + l8;
    int x;

    src+= stride*3;
    for(x=0; x<8; x++){
        int a= src[l3] - src[l4];
        int b= src[l4] - src[l5];
        int c= src[l5] - src[l6];

        int d= FFABS(b) - ((FFABS(a) + FFABS(c))>>1);
        d= FFMAX(d, 0);

        if(d < co->QP*2){
            int v = d * FFSIGN(-b);

            src[l2] +=v>>3;
            src[l3] +=v>>2;
            src[l4] +=(3*v)>>3;
            src[l5] -=(3*v)>>3;
            src[l6] -=v>>2;
            src[l7] -=v>>3;
        }
        src++;
    }
}
void do_vert_def_filter_C(uint8_t src[], int stride, PPContext *c)
{
    const int l1= stride;
    const int l2= stride + l1;
    const int l3= stride + l2;
    const int l4= stride + l3;
    const int l5= stride + l4;
    const int l6= stride + l5;
    const int l7= stride + l6;
    const int l8= stride + l7;
//    const int l9= stride + l8;
    int x;
    src+= stride*3;
    for(x=0; x<8; x++){
        const int middleEnergy= 5*(src[l5] - src[l4]) + 2*(src[l3] - src[l6]);
        if(FFABS(middleEnergy) < 8*c->QP){
            const int q=(src[l4] - src[l5])/2;
            const int leftEnergy=  5*(src[l3] - src[l2]) + 2*(src[l1] - src[l4]);
            const int rightEnergy= 5*(src[l7] - src[l6]) + 2*(src[l5] - src[l8]);

            int d= FFABS(middleEnergy) - FFMIN(FFABS(leftEnergy), FFABS(rightEnergy) );
            d = FFMAX(d, 0);

            d = (5*d + 32) >> 6;
            d *= FFSIGN(-middleEnergy);

            if(q>0){
                d = FFMAX(d, 0);
                d = FFMIN(d, q);
            } else {
                d = FFMIN(d, 0);
                d = FFMAX(d, q);
            }

            src[l4]-= d;
            src[l5]+= d;
        }
        src++;
    }
}
