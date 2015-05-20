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
void dering_C(uint8_t src[], int stride, PPContext *c)
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
    avg = (min + max + 1)>>1;

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

        p = src + stride*y;
        for(x=1; x<9; x++){
            p++;
            if(t & (1<<x)){
                int f = (*(p-stride-1)) + 2*(*(p-stride)) + (*(p-stride+1)) +
                        2*(*(p-1))      + 4*(*p)          + 2*(*(p+1))      +
                        (*(p+stride-1)) + 2*(*(p+stride)) + (*(p+stride+1));
                f += 8;
                f >>= 4;

#ifdef DEBUG_DERING_THRESHOLD
                __asm__ volatile("emms\n\t":);
                {
                    static long long numPixels=0;
                    if(x!=1 && x!=8 && y!=1 && y!=8) numPixels++;
                    if(max-min < 20){
                        static int numSkipped=0;
                        static int errorSum=0;
                        static int worstQP=0;
                        static int worstRange=0;
                        static int worstDiff=0;
                        int diff= (f - *p);
                        int absDiff= FFABS(diff);
                        int error= diff*diff;

                        if(x==1 || x==8 || y==1 || y==8) continue;

                        numSkipped++;
                        if(absDiff > worstDiff){
                            worstDiff= absDiff;
                            worstQP= QP;
                            worstRange= max-min;
                        }
                        errorSum += error;

                        if(1024LL*1024LL*1024LL % numSkipped == 0){
                            av_log(c, AV_LOG_INFO, "sum:%1.3f, skip:%d, wQP:%d, "
                                   "wRange:%d, wDiff:%d, relSkip:%1.3f\n",
                                   (float)errorSum/numSkipped, numSkipped, worstQP, worstRange,
                                   worstDiff, (float)numSkipped/numPixels);
                        }
                    }
                }
#endif
                if(*p + QP2 < f){
                    *p= *p + QP2;
                } else if(*p - QP2 > f){
                    *p= *p - QP2;
                } else {
                    *p=f;
                }
            }
        }
    }
#ifdef DEBUG_DERING_THRESHOLD
    if(max-min < 20){
        for(y=1; y<9; y++){
            int x;
            int t = 0;
            p= src + stride*y;
            for(x=1; x<9; x++){
                p++;
                *p = FFMIN(*p + 20, 255);
            }
        }
    }
#endif
}

void temp_noise_reducer_C(uint8_t *src, int stride,
                          uint8_t *tempBlurred, uint32_t *tempBlurredPast,
                          const int *maxNoise)
{
    int y, i;
    int d=0;


    tempBlurredPast[127]= maxNoise[0];
    tempBlurredPast[128]= maxNoise[1];
    tempBlurredPast[129]= maxNoise[2];

    for(y=0; y<8; y++){
        int x;
        for(x=0; x<8; x++){
            int ref = tempBlurred[x + y*stride];
            int cur = src[x + y*stride];
            int d1 = ref - cur;
            d += d1*d1;
        }
    }
    i=d;
    d = (4*d+
         (*(tempBlurredPast-256))+
         (*(tempBlurredPast-1))+ (*(tempBlurredPast+1))+
         (*(tempBlurredPast+256))+ 4)>>3;
    *tempBlurredPast=i;

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
                    int ref = tempBlurred[x + y*stride];
                    int cur = src[x + y*stride];
                    tempBlurred[x + y*stride]=
                    src[x + y*stride] = (ref + cur + 1)>>1;
                }
            }
        } else {
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    tempBlurred[x + y*stride] = src[x + y*stride];
                }
            }
        }
    } else {
        if(d < maxNoise[0]){
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    int ref = tempBlurred[x + y*stride];
                    int cur = src[x + y*stride];
                    tempBlurred[x + y*stride] =
                        src[x + y*stride] = (ref*7 + cur + 4)>>3;
                }
            }
        }else{
            for(y=0; y<8; y++){
                int x;
                for(x=0; x<8; x++){
                    int ref = tempBlurred[x + y*stride];
                    int cur = src[x + y*stride];
                    tempBlurred[x + y*stride] =
                        src[x + y*stride] = (ref*3 + cur + 2)>>2;
                }
            }
        }
    }
}
