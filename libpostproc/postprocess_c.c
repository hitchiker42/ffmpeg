/******************************************************************************
* C implementation of postprocessing routines
* Copyright (C) 2001-2002 Michael Niedermayer (michaelni@gmx.at)
* Copyright (c) 2015 Tucker DiNapoli
*
* This file is part of FFmpeg.
*
* FFmpeg is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* FFmpeg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
******************************************************************************/

/* The horizontal functions exist only in C because the MMX
 * code is faster with vertical filters and transposing. */

/**
 * Check if the given 8x8 Block is mostly "flat"
 */
static inline int isHorizDC_C(const uint8_t src[], int stride, const PPContext *c)
{
    int numEq= 0;
    int y;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;

    for(y=0; y<BLOCK_SIZE; y++){
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
static inline int isVertDC_C(const uint8_t src[], int stride, const PPContext *c)
{
    int numEq= 0;
    int y;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;

    src+= stride*4; // src points to begin of the 8x8 Block
    for(y=0; y<BLOCK_SIZE-1; y++){
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

static inline int isHorizMinMaxOk_C(const uint8_t src[], int stride, int QP)
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

static inline int isVertMinMaxOk_C(const uint8_t src[], int stride, int QP)
{
    int x;
    src+= stride*4;
    for(x=0; x<BLOCK_SIZE; x+=4){
        if((unsigned)(src[  x + 0*stride] - src[  x + 5*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[1+x + 2*stride] - src[1+x + 7*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[2+x + 4*stride] - src[2+x + 1*stride] + 2*QP) > 4*QP) return 0;
        if((unsigned)(src[3+x + 6*stride] - src[3+x + 3*stride] + 2*QP) > 4*QP) return 0;
    }
    return 1;
}

static inline int horizClassify_C(const uint8_t src[], int stride, const PPContext *c)
{
    if( isHorizDC_C(src, stride, c) ){
        return isHorizMinMaxOk_C(src, stride, c->QP);
    }else{
        return 2;
    }
}

static inline int vertClassify_C(const uint8_t src[], int stride, const PPContext *c)
{
    if( isVertDC_C(src, stride, c) ){
        return isVertMinMaxOk_C(src, stride, c->QP);
    }else{
        return 2;
    }
}

static inline void doHorizDefFilter_C(uint8_t dst[], int stride, const PPContext *c)
{
    int y;
    for(y=0; y<BLOCK_SIZE; y++){
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
static inline void doHorizLowPass_C(uint8_t dst[], int stride, const PPContext *c)
{
    int y;
    for(y=0; y<BLOCK_SIZE; y++){
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
static inline void horizX1Filter(uint8_t *src, int stride, int QP)
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

    for(y=0; y<BLOCK_SIZE; y++){
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
static av_always_inline void do_a_deblock_C(uint8_t *src, int step,
                                            int stride, const PPContext *c, int mode)
{
    int y;
    const int QP= c->QP;
    const int dcOffset= ((c->nonBQP*c->ppMode.baseDcDiff)>>8) + 1;
    const int dcThreshold= dcOffset*2 + 1;
//START_TIMER
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
            }else{
                max= src[step];
                min= src[0];
            }
            for(x=2; x<8; x+=2){
                if(src[x*step] > src[(x+1)*step]){
                        if(src[x    *step] > max) max= src[ x   *step];
                        if(src[(x+1)*step] < min) min= src[(x+1)*step];
                }else{
                        if(src[(x+1)*step] > max) max= src[(x+1)*step];
                        if(src[ x   *step] < min) min= src[ x   *step];
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
        }else{
            const int middleEnergy= 5*(src[4*step] - src[3*step]) + 2*(src[2*step] - src[5*step]);

            if(FFABS(middleEnergy) < 8*QP){
                const int q=(src[3*step] - src[4*step])/2;
                const int leftEnergy=  5*(src[2*step] - src[1*step]) + 2*(src[0*step] - src[3*step]);
                const int rightEnergy= 5*(src[6*step] - src[5*step]) + 2*(src[4*step] - src[7*step]);

                int d= FFABS(middleEnergy) - FFMIN( FFABS(leftEnergy), FFABS(rightEnergy) );
                d= FFMAX(d, 0);

                d= (5*d + 32) >> 6;
                d*= FFSIGN(-middleEnergy);

                if(q>0){
                    d = FFMAX(d, 0);
                    d = FFMIN(d, q);
                }else{
                    d = FFMIN(d, 0);
                    d = FFMAX(d, q);
                }

                if ((mode & VISUALIZE) && d) {
                    d= (d < 0) ? 32 : -32;
                    src[3*step]= av_clip_uint8(src[3*step] - d);
                    src[4*step]= av_clip_uint8(src[4*step] + d);
                    d = 0;
                }

                src[3*step]-= d;
                src[4*step]+= d;
            }
        }

        src += stride;
    }
/*if(step==16){
    STOP_TIMER("step16")
}else{
    STOP_TIMER("stepX")
}*/
}

#define PAVGB(a,b) REAL_PAVGB(a,b)

//FIXME? |255-0| = 1 (should not be a problem ...)

/**
 * Do a vertical low pass filter on the 8x16 block (only write to the 8x8 block in the middle)
 * using the 9-Tap Filter (1,1,2,2,4,2,2,1,1)/16
 */
static inline void doVertLowPass_C(uint8_t *src, int stride, PPContext *c)
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
    for(x=0; x<BLOCK_SIZE; x++){
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
static inline void vertX1Filter_C(uint8_t *src, int stride, PPContext *co)
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
    for(x=0; x<BLOCK_SIZE; x++){
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

static inline void doVertDefFilter_C(uint8_t src[], int stride, PPContext *c)
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
    for(x=0; x<BLOCK_SIZE; x++){
        const int middleEnergy= 5*(src[l5] - src[l4]) + 2*(src[l3] - src[l6]);
        if(FFABS(middleEnergy) < 8*c->QP){
            const int q=(src[l4] - src[l5])/2;
            const int leftEnergy=  5*(src[l3] - src[l2]) + 2*(src[l1] - src[l4]);
            const int rightEnergy= 5*(src[l7] - src[l6]) + 2*(src[l5] - src[l8]);

            int d= FFABS(middleEnergy) - FFMIN( FFABS(leftEnergy), FFABS(rightEnergy) );
            d= FFMAX(d, 0);

            d= (5*d + 32) >> 6;
            d*= FFSIGN(-middleEnergy);

            if(q>0){
                d = FFMAX(d, 0);
                d = FFMIN(d, q);
            }else{
                d = FFMIN(d, 0);
                d = FFMAX(d, q);
            }

            src[l4]-= d;
            src[l5]+= d;
        }
        src++;
    }
}

static inline void dering_C(uint8_t src[], int stride, PPContext *c)
{
    int y;
    int min=255;
    int max=0;
    int avg;
    uint8_t *p;
    int s[10];
    const int QP2= c->QP/2 + 1;

    src --;
    for(y=1; y<9; y++){
        int x;
        p= src + stride*y;
        for(x=1; x<9; x++){
            p++;
            if(*p > max) max= *p;
            if(*p < min) min= *p;
        }
    }
    avg= (min + max + 1)>>1;

    if(max - min <deringThreshold) return;

    for(y=0; y<10; y++){
        int t = 0;

        if(src[stride*y + 0] > avg) t+= 1;
        if(src[stride*y + 1] > avg) t+= 2;
        if(src[stride*y + 2] > avg) t+= 4;
        if(src[stride*y + 3] > avg) t+= 8;
        if(src[stride*y + 4] > avg) t+= 16;
        if(src[stride*y + 5] > avg) t+= 32;
        if(src[stride*y + 6] > avg) t+= 64;
        if(src[stride*y + 7] > avg) t+= 128;
        if(src[stride*y + 8] > avg) t+= 256;
        if(src[stride*y + 9] > avg) t+= 512;

        t |= (~t)<<16;
        t &= (t<<1) & (t>>1);
        s[y] = t;
    }

    for(y=1; y<9; y++){
        int t = s[y-1] & s[y] & s[y+1];
        t|= t>>16;
        s[y-1]= t;
    }

    for(y=1; y<9; y++){
        int x;
        int t = s[y-1];

        p= src + stride*y;
        for(x=1; x<9; x++){
            p++;
            if(t & (1<<x)){
                int f= (*(p-stride-1)) + 2*(*(p-stride)) + (*(p-stride+1))
                      +2*(*(p     -1)) + 4*(*p         ) + 2*(*(p     +1))
                      +(*(p+stride-1)) + 2*(*(p+stride)) + (*(p+stride+1));
                f= (f + 8)>>4;

                    if     (*p + QP2 < f) *p= *p + QP2;
                    else if(*p - QP2 > f) *p= *p - QP2;
                    else *p=f;
            }
        }
    }
}

/**
 * Deinterlace the given block by linearly interpolating every second line.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 */
static inline void deInterlaceInterpolateLinear_C(uint8_t src[], int stride)
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
static inline void deInterlaceInterpolateCubic_C(uint8_t src[], int stride)
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
static inline void deInterlaceFF_C(uint8_t src[], int stride, uint8_t *tmp)
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
static inline void deInterlaceL5_C(uint8_t src[], int stride, uint8_t *tmp, uint8_t *tmp2)
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
static inline void deInterlaceBlendLinear_C(uint8_t src[], int stride, uint8_t *tmp)
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
static inline void deInterlaceMedian_C(uint8_t src[], int stride)
{
    int x, y;
    src+= 4*stride;
    // FIXME - there should be a way to do a few columns in parallel like w/mmx
    for(x=0; x<8; x++){
        uint8_t *colsrc = src;
        for (y=0; y<4; y++){
            int a, b, c, d, e, f;
            a = colsrc[0       ];
            b = colsrc[stride  ];
            c = colsrc[stride*2];
            d = (a-b)>>31;
            e = (b-c)>>31;
            f = (c-a)>>31;
            colsrc[stride  ] = (a|(d^f)) & (b|(d^e)) & (c|(e^f));
            colsrc += stride*2;
        }
        src++;
    }
}

//static long test=0;

static inline void tempNoiseReducer_C(uint8_t *src, int stride,
                                    uint8_t *tempBlurred, uint32_t *tempBlurredPast, const int *maxNoise)
{
    // to save a register (FIXME do this outside of the loops)
    tempBlurredPast[127]= maxNoise[0];
    tempBlurredPast[128]= maxNoise[1];
    tempBlurredPast[129]= maxNoise[2];

#define FAST_L2_DIFF 
//#define L1_DIFF //u should change the thresholds too if u try that one
{
    int y;
    int d=0;
//    int sysd=0;
    int i;

    for(y=0; y<8; y++){
        int x;
        for(x=0; x<8; x++){
            int ref= tempBlurred[ x + y*stride ];
            int cur= src[ x + y*stride ];
            int d1=ref - cur;
//            if(x==0 || x==7) d1+= d1>>1;
//            if(y==0 || y==7) d1+= d1>>1;
//            d+= FFABS(d1);
            d+= d1*d1;
//            sysd+= d1;
        }
    }
    i=d;
    d=  (
        4*d
        +(*(tempBlurredPast-256))
        +(*(tempBlurredPast-1))+ (*(tempBlurredPast+1))
        +(*(tempBlurredPast+256))
        +4)>>3;
    *tempBlurredPast=i;
//    ((*tempBlurredPast)*3 + d + 2)>>2;

/*
Switch between
 1  0  0  0  0  0  0  (0)
64 32 16  8  4  2  1  (1)
64 48 36 27 20 15 11 (33) (approx)
64 56 49 43 37 33 29 (200) (approx)
*/
    if(d > maxNoise[1]){
        if(d < maxNoise[2]){
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    int ref= tempBlurred[ x + y*stride ];
                    int cur= src[ x + y*stride ];
                    tempBlurred[ x + y*stride ]=
                    src[ x + y*stride ]=
                        (ref + cur + 1)>>1;
                }
            }
        }else{
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    tempBlurred[ x + y*stride ]= src[ x + y*stride ];
                }
            }
        }
    }else{
        if(d < maxNoise[0]){
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    int ref= tempBlurred[ x + y*stride ];
                    int cur= src[ x + y*stride ];
                    tempBlurred[ x + y*stride ]=
                    src[ x + y*stride ]=
                        (ref*7 + cur + 4)>>3;
                }
            }
        }else{
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    int ref= tempBlurred[ x + y*stride ];
                    int cur= src[ x + y*stride ];
                    tempBlurred[ x + y*stride ]=
                    src[ x + y*stride ]=
                        (ref*3 + cur + 2)>>2;
                }
            }
        }
    }
}
}


static void postProcess_C(const uint8_t src[], int srcStride, uint8_t dst[], int dstStride, int width, int height,
                                const QP_STORE_T QPs[], int QPStride, int isColor, PPContext *c);

/**
 * Copy a block from src to dst and fixes the blacklevel.
 * levelFix == 0 -> do not touch the brightness & contrast
 */
#undef REAL_SCALED_CPY
#undef SCALED_CPY

static inline void blockCopy_C(uint8_t dst[], int dstStride, const uint8_t src[], int srcStride,
                                     int levelFix, int64_t *packedOffsetAndScale)
{
    int i;
    if(levelFix){
    for(i=0; i<8; i++)
        memcpy( &(dst[dstStride*i]),
                &(src[srcStride*i]), BLOCK_SIZE);
    }else{
    for(i=0; i<8; i++)
        memcpy( &(dst[dstStride*i]),
                &(src[srcStride*i]), BLOCK_SIZE);
    }
}

/**
 * Duplicate the given 8 src pixels ? times upward
 */
static inline void duplicate_C(uint8_t src[], int stride)
{
    int i;
    uint8_t *p=src;
    for(i=0; i<5; i++){
        p-= stride;
        memcpy(p, src, 8);
    }
}

/**
 * Filter array of bytes (Y or U or V values)
 */
static void postProcess_C(const uint8_t src[], int srcStride, uint8_t dst[], int dstStride, int width, int height,
                                const QP_STORE_T QPs[], int QPStride, int isColor, PPContext *c2)
{
    DECLARE_ALIGNED(8, PPContext, c)= *c2; //copy to stack for faster access
    int x,y;
    const int mode= isColor ? c.ppMode.chromMode : c.ppMode.lumMode;
    int black=0, white=255; // blackest black and whitest white in the picture
    int QPCorrecture= 256*256;

    int copyAhead;

    const int qpHShift= isColor ? 4-c.hChromaSubSample : 4;
    const int qpVShift= isColor ? 4-c.vChromaSubSample : 4;

    //FIXME remove
    uint64_t * const yHistogram= c.yHistogram;
    uint8_t * const tempSrc= srcStride > 0 ? c.tempSrc : c.tempSrc - 23*srcStride;
    uint8_t * const tempDst= (dstStride > 0 ? c.tempDst : c.tempDst - 23*dstStride) + 32;
    //const int mbWidth= isColor ? (width+7)>>3 : (width+15)>>4;

    if (mode & VISUALIZE){
        if(!(mode & (V_A_DEBLOCK | H_A_DEBLOCK))) {
            av_log(c2, AV_LOG_WARNING, "Visualization is currently only supported with the accurate deblock filter without SIMD\n");
        }
    }


    if(mode & CUBIC_IPOL_DEINT_FILTER) copyAhead=16;
    else if(   (mode & LINEAR_BLEND_DEINT_FILTER)
            || (mode & FFMPEG_DEINT_FILTER)
            || (mode & LOWPASS5_DEINT_FILTER)) copyAhead=14;
    else if(   (mode & V_DEBLOCK)
            || (mode & LINEAR_IPOL_DEINT_FILTER)
            || (mode & MEDIAN_DEINT_FILTER)
            || (mode & V_A_DEBLOCK)) copyAhead=13;
    else if(mode & V_X1_FILTER) copyAhead=11;
//    else if(mode & V_RK1_FILTER) copyAhead=10;
    else if(mode & DERING) copyAhead=9;
    else copyAhead=8;

    copyAhead-= 8;

    if(!isColor){
        uint64_t sum= 0;
        int i;
        uint64_t maxClipped;
        uint64_t clipped;
        double scale;

        c.frameNum++;
        // first frame is fscked so we ignore it
        if(c.frameNum == 1) yHistogram[0]= width*(uint64_t)height/64*15/256;

        for(i=0; i<256; i++){
            sum+= yHistogram[i];
        }

        /* We always get a completely black picture first. */
        maxClipped= (uint64_t)(sum * c.ppMode.maxClippedThreshold);

        clipped= sum;
        for(black=255; black>0; black--){
            if(clipped < maxClipped) break;
            clipped-= yHistogram[black];
        }

        clipped= sum;
        for(white=0; white<256; white++){
            if(clipped < maxClipped) break;
            clipped-= yHistogram[white];
        }

        scale= (double)(c.ppMode.maxAllowedY - c.ppMode.minAllowedY) / (double)(white-black);

        c.packedYScale= (uint16_t)(scale*1024.0 + 0.5);
        c.packedYOffset= (black - c.ppMode.minAllowedY) & 0xFFFF;

        c.packedYOffset|= c.packedYOffset<<32;
        c.packedYOffset|= c.packedYOffset<<16;

        c.packedYScale|= c.packedYScale<<32;
        c.packedYScale|= c.packedYScale<<16;

        if(mode & LEVEL_FIX)        QPCorrecture= (int)(scale*256*256 + 0.5);
        else                        QPCorrecture= 256*256;
    }else{
        c.packedYScale= 0x0100010001000100LL;
        c.packedYOffset= 0;
        QPCorrecture= 256*256;
    }

    /* copy & deinterlace first row of blocks */
    y=-BLOCK_SIZE;
    {
        const uint8_t *srcBlock= &(src[y*srcStride]);
        uint8_t *dstBlock= tempDst + dstStride;

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // finish 1 block before the next otherwise we might have a problem
        // with the L1 Cache of the P4 ... or only a few blocks at a time or something
        for(x=0; x<width; x+=BLOCK_SIZE){


            blockCopy_C(dstBlock + dstStride*8, dstStride,
                              srcBlock + srcStride*8, srcStride, mode & LEVEL_FIX, &c.packedYOffset);

            duplicate_C(dstBlock + dstStride*8, dstStride);

            if(mode & LINEAR_IPOL_DEINT_FILTER)
                deInterlaceInterpolateLinear_C(dstBlock, dstStride);
            else if(mode & LINEAR_BLEND_DEINT_FILTER)
                deInterlaceBlendLinear_C(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & MEDIAN_DEINT_FILTER)
                deInterlaceMedian_C(dstBlock, dstStride);
            else if(mode & CUBIC_IPOL_DEINT_FILTER)
                deInterlaceInterpolateCubic_C(dstBlock, dstStride);
            else if(mode & FFMPEG_DEINT_FILTER)
                deInterlaceFF_C(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & LOWPASS5_DEINT_FILTER)
                deInterlaceL5_C(dstBlock, dstStride, c.deintTemp + x, c.deintTemp + width + x);
/*          else if(mode & CUBIC_BLEND_DEINT_FILTER)
                deInterlaceBlendCubic_C(dstBlock, dstStride);
*/
            dstBlock+=8;
            srcBlock+=8;
        }
        if(width==FFABS(dstStride))
            linecpy(dst, tempDst + 9*dstStride, copyAhead, dstStride);
        else{
            int i;
            for(i=0; i<copyAhead; i++){
                memcpy(dst + i*dstStride, tempDst + (9+i)*dstStride, width);
            }
        }
    }

    for(y=0; y<height; y+=BLOCK_SIZE){
        //1% speedup if these are here instead of the inner loop
        const uint8_t *srcBlock= &(src[y*srcStride]);
        uint8_t *dstBlock= &(dst[y*dstStride]);
        const int8_t *QPptr= &QPs[(y>>qpVShift)*QPStride];
        int8_t *nonBQPptr= &c.nonBQPTable[(y>>qpVShift)*FFABS(QPStride)];
        int QP=0;
        /* can we mess with a 8x16 block from srcBlock/dstBlock downwards and 1 line upwards
           if not than use a temporary buffer */
        if(y+15 >= height){
            int i;
            /* copy from line (copyAhead) to (copyAhead+7) of src, these will be copied with
               blockcopy to dst later */
            linecpy(tempSrc + srcStride*copyAhead, srcBlock + srcStride*copyAhead,
                    FFMAX(height-y-copyAhead, 0), srcStride);

            /* duplicate last line of src to fill the void up to line (copyAhead+7) */
            for(i=FFMAX(height-y, 8); i<copyAhead+8; i++)
                    memcpy(tempSrc + srcStride*i, src + srcStride*(height-1), FFABS(srcStride));

            /* copy up to (copyAhead+1) lines of dst (line -1 to (copyAhead-1))*/
            linecpy(tempDst, dstBlock - dstStride, FFMIN(height-y+1, copyAhead+1), dstStride);

            /* duplicate last line of dst to fill the void up to line (copyAhead) */
            for(i=height-y+1; i<=copyAhead; i++)
                    memcpy(tempDst + dstStride*i, dst + dstStride*(height-1), FFABS(dstStride));

            dstBlock= tempDst + dstStride;
            srcBlock= tempSrc;
        }

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // finish 1 block before the next otherwise we might have a problem
        // with the L1 Cache of the P4 ... or only a few blocks at a time or something
        for(x=0; x<width; x+=BLOCK_SIZE){
            const int stride= dstStride;
            if(isColor){
                QP= QPptr[x>>qpHShift];
                c.nonBQP= nonBQPptr[x>>qpHShift];
            }else{
                QP= QPptr[x>>4];
                QP= (QP* QPCorrecture + 256*128)>>16;
                c.nonBQP= nonBQPptr[x>>4];
                c.nonBQP= (c.nonBQP* QPCorrecture + 256*128)>>16;
                yHistogram[ srcBlock[srcStride*12 + 4] ]++;
            }
            c.QP= QP;



            blockCopy_C(dstBlock + dstStride*copyAhead, dstStride,
                              srcBlock + srcStride*copyAhead, srcStride, mode & LEVEL_FIX, &c.packedYOffset);

            if(mode & LINEAR_IPOL_DEINT_FILTER)
                deInterlaceInterpolateLinear_C(dstBlock, dstStride);
            else if(mode & LINEAR_BLEND_DEINT_FILTER)
                deInterlaceBlendLinear_C(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & MEDIAN_DEINT_FILTER)
                deInterlaceMedian_C(dstBlock, dstStride);
            else if(mode & CUBIC_IPOL_DEINT_FILTER)
                deInterlaceInterpolateCubic_C(dstBlock, dstStride);
            else if(mode & FFMPEG_DEINT_FILTER)
                deInterlaceFF_C(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & LOWPASS5_DEINT_FILTER)
                deInterlaceL5_C(dstBlock, dstStride, c.deintTemp + x, c.deintTemp + width + x);
/*          else if(mode & CUBIC_BLEND_DEINT_FILTER)
                deInterlaceBlendCubic_C(dstBlock, dstStride);
*/

            /* only deblock if we have 2 blocks */
            if(y + 8 < height){
                if(mode & V_X1_FILTER)
                    vertX1Filter_C(dstBlock, stride, &c);
                else if(mode & V_DEBLOCK){
                    const int t= vertClassify_C(dstBlock, stride, &c);

                    if(t==1)
                        doVertLowPass_C(dstBlock, stride, &c);
                    else if(t==2)
                        doVertDefFilter_C(dstBlock, stride, &c);
                }else if(mode & V_A_DEBLOCK){
                    do_a_deblock_C(dstBlock, stride, 1, &c, mode);
                }
            }

            /* check if we have a previous block to deblock it with dstBlock */
            if(x - 8 >= 0){
                if(mode & H_X1_FILTER)
                    horizX1Filter(dstBlock-4, stride, QP);
                else if(mode & H_DEBLOCK){
                    const int t= horizClassify_C(dstBlock-4, stride, &c);

                    if(t==1)
                        doHorizLowPass_C(dstBlock-4, stride, &c);
                    else if(t==2)
                        doHorizDefFilter_C(dstBlock-4, stride, &c);
                }else if(mode & H_A_DEBLOCK){
                    do_a_deblock_C(dstBlock-8, 1, stride, &c, mode);
                }
                if(mode & DERING){
                //FIXME filter first line
                    if(y>0) dering_C(dstBlock - stride - 8, stride, &c);
                }

                if(mode & TEMP_NOISE_FILTER)
                {
                    tempNoiseReducer_C(dstBlock-8, stride,
                            c.tempBlurred[isColor] + y*dstStride + x,
                            c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                            c.ppMode.maxTmpNoise);
                }
            }

            dstBlock+=8;
            srcBlock+=8;

        }

        if(mode & DERING){
            if(y > 0) dering_C(dstBlock - dstStride - 8, dstStride, &c);
        }

        if((mode & TEMP_NOISE_FILTER)){
            tempNoiseReducer_C(dstBlock-8, dstStride,
                    c.tempBlurred[isColor] + y*dstStride + x,
                    c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                    c.ppMode.maxTmpNoise);
        }

        /* did we use a tmp buffer for the last lines*/
        if(y+15 >= height){
            uint8_t *dstBlock= &(dst[y*dstStride]);
            if(width==FFABS(dstStride))
                linecpy(dstBlock, tempDst + dstStride, height-y, dstStride);
            else{
                int i;
                for(i=0; i<height-y; i++){
                    memcpy(dstBlock + i*dstStride, tempDst + (i+1)*dstStride, width);
                }
            }
        }
/*
        for(x=0; x<width; x+=32){
            volatile int i;
            i+=   dstBlock[x + 7*dstStride] + dstBlock[x + 8*dstStride]
                + dstBlock[x + 9*dstStride] + dstBlock[x +10*dstStride]
                + dstBlock[x +11*dstStride] + dstBlock[x +12*dstStride];
                + dstBlock[x +13*dstStride]
                + dstBlock[x +14*dstStride] + dstBlock[x +15*dstStride];
        }*/
    }


    *c2= c; //copy local context back

}
