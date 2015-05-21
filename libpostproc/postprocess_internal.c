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

#define postprocess postprocess_ ## SUFFIX
#define deinterlace_interpolate_linear deinterlace_interpolate_linear_ ## SUFFIX
#define deinterlace_interpolate_cubic deinterlace_interpolate_cubic_ ## SUFFIX
#define deinterlace_FF deinterlace_FF_ ## SUFFIX
#define deinterlace_l5 deinterlace_l5_ ## SUFFIX
#define deinterlace_blend_linear deinterlace_blend_linear_ ## SUFFIX
#define deinterlace_median deinterlace_median_ ## SUFFIX
#define horiz_classify horiz_classify_ ## SUFFIX
#define vert_classify vert_classify_ ## SUFFIX
#define do_horiz_def_filter do_horiz_def_filter_ ## SUFFIX
#define do_horiz_low_pass do_horiz_low_pass_ ## SUFFIX
#define horiz_X1_filter horiz_X1_filter_ ## SUFFIX
#define do_a_deblock do_a_deblock_ ## SUFFIX
#define do_vert_low_pass do_vert_low_pass_ ## SUFFIX
#define vert_X1_filter vert_X1_filter_ ## SUFFIX
#define do_vert_def_filter do_vert_def_filter_ ## SUFFIX
#define temp_noise_reducer temp_noise_reducer_ ## SUFFIX
#define dering dering_ ## SUFFIX
#define prefetchnta prefetchnta_ ## SUFFIX
#define prefetcht0 prefetcht0_ ## SUFFIX
#define block_copy block_copy_ ## SUFFIX
#define duplicate duplicate_ ## SUFFIX
#define deinterlace_internal deinterlace_internal_ ## SUFFIX
#define vdeblock_internal vdeblock_internal_ ## SUFFIX
#define hdeblock_internal hdeblock_internal_ ## SUFFIX
/*
  Put the code dealing with the number of blocks in these helper functions, that way
  both the filters and the postprocess function don't need to worry about how many
  blocks are being processed each loop.
*/
static inline vdeblock_internal(uint8_t *dstBlock, int stride, PPContext *c, int mode, int blocks)
{

    if(mode & V_X1_FILTER){
        vertX1Filter(dstBlock, stride, &c);
    } else if(mode & V_DEBLOCK){
        const int t = vertClassify(dstBlock, stride, &c);

        if(t==1){
            doVertLowPass(dstBlock, stride, &c);
        } else if(t==2){
            doVertDefFilter(dstBlock, stride, &c);
        }
    } else if(mode & V_A_DEBLOCK){
        do_a_deblock(dstBlock, stride, 1, &c, mode);
    }
}

//This may need to be a more complicated function
static inline hdeblock_internal(uint8_t *dstBlock, int stride, PPContext *c, int mode, int blocks)
{
    av_unused uint8_t *tempBlock1 = c.tempBlocks;
    av_unused uint8_t *tempBlock2 = c.tempBlocks + 8;
#if POSTPROCESS_SIMD
    transpose1(tempBlock1, tempBlock2, dstBlock, dstStride);
    if(mode & H_X1_FILTER){
        vertX1Filter(tempBlock1, 16, &c);
    } else if(mode & H_DEBLOCK){
        const int t = vertClassify(tempBlock1, 16, &c);
        if(t==1){
            doVertLowPass(tempBlock1, 16, &c);
        } else if(t==2){
            doVertDefFilter(tempBlock1, 16, &c);
        }
    } else if(mode & H_A_DEBLOCK){
        do_a_deblock(tempBlock1, 16, 1, &c, mode);
    }

    transpose2(dstBlock-4, dstStride, tempBlock1 + 4*16);

#else
    if(mode & H_X1_FILTER){
        horizX1Filter(dstBlock-4, stride, c.QP);
    } else if(mode & H_DEBLOCK){
        const int t = horizClassify(dstBlock-4, stride, &c);

        if(t==1){
            doHorizLowPass(dstBlock-4, stride, &c);
        } else if(t==2){
            doHorizDefFilter(dstBlock-4, stride, &c);
        }
    } else if(mode & H_A_DEBLOCK){
        do_a_deblock(dstBlock-8, 1, stride, &c, mode);
    }
#endif //TEMPLATE_PP_MMX
    if(mode & DERING){
        //FIXME filter first line
        if(y>0) dering(dstBlock - stride - 8, stride, &c);
    }    
    if(mode & TEMP_NOISE_FILTER){
        tempNoiseReducer(dstBlock-8, stride,
                         c.tempBlurred[isColor] + y*dstStride + x,
                         c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                         c.ppMode.maxTmpNoise);
    }
    dstBlock+=8;
    srcBlock+=8;

#if POSTPROCESS_SIMD
    FFSWAP(uint8_t *, tempBlock1, tempBlock2);
#endif
}


//IDEA: replace this with a function pointer
static inline void deinterlace_internal(uint8_t *dstBlock, int dstStride,
                                        uint8_t *tmp, uint8_t *tmp2, 
                                        int mode, int blocks)
{
    if(mode & LINEAR_IPOL_DEINT_FILTER){
        deinterlace_interpolate_linear(dstBlock + block_index*8, dstStride);
    } else if(mode & LINEAR_BLEND_DEINT_FILTER){
        deinterlace_blend_linear(dstBlock + block_index*8, dstStride, tmp);
    } else if(mode & MEDIAN_DEINT_FILTER){
        deinterlace_median(dstBlock + block_index*8, dstStride);
    } else if(mode & CUBIC_IPOL_DEINT_FILTER){
        deinterlace_interpolate_cubic(dstBlock + block_index*8, dstStride);
    } else if(mode & FFMPEG_DEINT_FILTER){
        deinterlace_FF(dstBlock + block_index*8, dstStride, tmp);
    } else if(mode & LOWPASS5_DEINT_FILTER){
        deinterlace_l5(dstBlock + block_index*8, dstStride, tmp, tmp2);
    }
}

/**
 * Filter array of bytes (Y or U or V values)
 */
void post_process(const uint8_t src[], int srcStride,
                 uint8_t dst[], int dstStride, int width, int height,
                 const QP_STORE_T QPs[], int QPStride,
                 int isColor, PPContext *c2)
{
    DECLARE_ALIGNED(8, PPContext, c) = *c2; //copy to stack for faster access
    int x,y;
#ifdef TEMPLATE_PP_TIME_MODE
    const int mode = TEMPLATE_PP_TIME_MODE;
#else
    const int mode = isColor ? c.ppMode.chromMode : c.ppMode.lumMode;
#endif
    int black = 0, white = 0xff; // blackest black and whitest white in the picture
    int QPCorrecture = 0xff*0xff;

    int copyAhead;
    av_unused int i;//only needed for simd code

    const int qpHShift = isColor ? 4-c.hChromaSubSample : 4;
    const int qpVShift = isColor ? 4-c.vChromaSubSample : 4;

    //FIXME remove
    uint64_t *const yHistogram = c.yHistogram;
    uint8_t *const tempSrc = srcStride > 0 ? c.tempSrc : c.tempSrc - 23*srcStride;
    uint8_t *const tempDst = (dstStride > 0 ? c.tempDst : c.tempDst - 23*dstStride) + 32;

    if (mode & VISUALIZE){
        if(!(mode & (V_A_DEBLOCK | H_A_DEBLOCK)) || TEMPLATE_PP_MMX) {
            av_log(c2, AV_LOG_WARNING,
                   "Visualization is currently only supported with the accurate deblock filter without SIMD\n");
        }
    }

#if POSTPROCESS_SIMD
    for(i=0; i<57; i++){
        int offset = ((i*c.ppMode.baseDcDiff)>>8) + 1;
        int threshold = offset*2 + 1;
        c.mmxDcOffset[i] = 0x7F - offset;
        c.mmxDcThreshold[i] = 0x7F - threshold;
        c.mmxDcOffset[i] *= 0x0101010101010101LL;
        c.mmxDcThreshold[i] *= 0x0101010101010101LL;
    }
#endif

    if(mode & CUBIC_IPOL_DEINT_FILTER){
        copyAhead=8;
    } else if((mode & LINEAR_BLEND_DEINT_FILTER)
           || (mode & FFMPEG_DEINT_FILTER)
           || (mode & LOWPASS5_DEINT_FILTER)){
        copyAhead=6;
    } else if((mode & V_DEBLOCK)
           || (mode & LINEAR_IPOL_DEINT_FILTER)
           || (mode & MEDIAN_DEINT_FILTER)
           || (mode & V_A_DEBLOCK)){
        copyAhead=5;
    } else if(mode & V_X1_FILTER){
        copyAhead=3;
    } else if(mode & DERING){
        copyAhead=1;
    }

    if(!isColor){
        uint64_t sum = 0;
        int i;
        uint64_t maxClipped;
        uint64_t clipped;
        double scale;

        c.frameNum++;
        // first frame is fscked so we ignore it
        if(c.frameNum == 1) yHistogram[0] = width*(uint64_t)height/64*15/256;

        for(i=0; i<256; i++){
            sum += yHistogram[i];
        }

        /* We always get a completely black picture first. */
        maxClipped = (uint64_t)(sum * c.ppMode.maxClippedThreshold);

        clipped = sum;
        for(black=255; black>0; black--){
            if(clipped < maxClipped) break;
            clipped-= yHistogram[black];
        }

        clipped = sum;
        for(white=0; white<256; white++){
            if(clipped < maxClipped) break;
            clipped-= yHistogram[white];
        }

        scale = (double)(c.ppMode.maxAllowedY - c.ppMode.minAllowedY) / (double)(white-black);

#if POSTPROCESS_SIMD
        c.packedYScale = (uint16_t)(scale*256.0 + 0.5);
        c.packedYOffset = (((black*c.packedYScale)>>8) - c.ppMode.minAllowedY) & 0xFFFF;
#else
        c.packedYScale = (uint16_t)(scale*1024.0 + 0.5);
        c.packedYOffset = (black - c.ppMode.minAllowedY) & 0xFFFF;
#endif

        c.packedYOffset |= c.packedYOffset<<32;
        c.packedYOffset |= c.packedYOffset<<16;

        c.packedYScale |= c.packedYScale<<32;
        c.packedYScale |= c.packedYScale<<16;

        if(mode & LEVEL_FIX){
            QPCorrecture = (int)(scale*256*256 + 0.5);
        } else {
            QPCorrecture = 256*256;
        }
    } else {
        c.packedYScale = 0x0100010001000100LL;
        c.packedYOffset = 0;
        QPCorrecture = 256*256;
    }

    /* copy & deinterlace first row of blocks */
    y=-8;
    {
        const uint8_t *srcBlock = &(src[y*srcStride]);
        uint8_t *dstBlock = tempDst + dstStride;

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // finish 1 block before the next otherwise we might have a problem
        // with the L1 Cache of the P4 ... or only a few blocks at a time or something
        for(x=0; x<width; x+=8){
            prefetchnta(srcBlock + (((x>>2)&6) + copyAhead)*srcStride + 32);
            prefetchnta(srcBlock + (((x>>2)&6) + copyAhead+1)*srcStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + copyAhead)*dstStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + copyAhead+1)*dstStride + 32);

            blockCopy(dstBlock + dstStride*8, dstStride,
                      srcBlock + srcStride*8, srcStride,
                      mode & LEVEL_FIX, &c.packedYOffset);

            duplicate(dstBlock + dstStride*8, dstStride);

            deinterlace_internal(dstBlock, dstStride, c.deintTemp + x,
                                 c.deintTemp + width + x, mode);

            dstBlock+=8;
            srcBlock+=8;
        }
        if(width==FFABS(dstStride)){
            linecpy(dst, tempDst + 9*dstStride, copyAhead, dstStride);
        } else {
            int i;
            for(i=0; i<copyAhead; i++){
                memcpy(dst + i*dstStride, tempDst + (9+i)*dstStride, width);
            }
        }
    }

    for(y=0; y<height; y+=8){
        //1% speedup if these are here instead of the inner loop
        const uint8_t *srcBlock = &(src[y*srcStride]);
        uint8_t *dstBlock = &(dst[y*dstStride]);

        const int8_t *QPptr = &QPs[(y>>qpVShift)*QPStride];
        int8_t *nonBQPptr = &c.nonBQPTable[(y>>qpVShift)*FFABS(QPStride)];
        int QP=0, nonBQP=0;
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

            dstBlock = tempDst + dstStride;
            srcBlock = tempSrc;
        }

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // Process BLOCKS_PER_ITERATION number of blocks each loop iteration
        for(x=0; x<width; ){
            int startx = x;
            int endx = FFMIN(width, x+(8*BLOCKS_PER_ITERATION));
            uint8_t *dstBlockStart = dstBlock;
            const uint8_t *srcBlockStart = srcBlock;
            int qp_index = 0;
            for(qp_index=0; qp_index < (endx-startx)/8; qp_index++){
                QP = QPptr[(x+qp_index*8)>>qpHShift];
                nonBQP = nonBQPptr[(x+qp_index*8)>>qpHShift];
                if(!isColor){
                    QP = (QP* QPCorrecture + 256*128)>>16;
                    nonBQP = (nonBQP* QPCorrecture + 256*128)>>16;
                    yHistogram[(srcBlock+qp_index*8)[srcStride*12 + 4]]++;
                }
                c.QP_block[qp_index] = QP;
                c.nonBQP_block[qp_index] = nonBQP;
#if POSTPROCESS_SIMD
                pack_QP(&c);
#endif
            }
            for(; x < endx; x+=8){
                prefetchnta(srcBlock + (((x>>2)&6) + copyAhead)*srcStride + 32);
                prefetchnta(srcBlock + (((x>>2)&6) + copyAhead+1)*srcStride + 32);
                prefetcht0(dstBlock + (((x>>2)&6) + copyAhead)*dstStride + 32);
                prefetcht0(dstBlock + (((x>>2)&6) + copyAhead+1)*dstStride + 32);

                blockCopy(dstBlock + dstStride*copyAhead, dstStride,
                          srcBlock + srcStride*copyAhead, srcStride,
                          mode & LEVEL_FIX, &c.packedYOffset);

                deinterlace_internal(dstBlock, dstStride, c.deintTemp +x,
                                     c.deintTemp + width + x, mode);

                dstBlock+=8;
                srcBlock+=8;
            }

            dstBlock = dstBlockStart;
            srcBlock = srcBlockStart;

            for(x = startx, qp_index = 0; x < endx; x+=8, qp_index++){
                const int stride = dstStride;
                //temporary while changing QP stuff to make things continue to work
                //eventually QP,nonBQP,etc will be arrays and this will be unnecessary
                c.QP = c.QP_block[qp_index];
                c.nonBQP = c.nonBQP_block[qp_index];
                c.pQPb = c.pQPb_block[qp_index];
                c.pQPb2 = c.pQPb2_block[qp_index];

                /* only deblock if we have 2 blocks */
                if(y + 8 < height){
                    deblock_internal(dstBlock, dstStride, &c, mode);
                }


                dstBlock+=8;
                srcBlock+=8;
            }

            dstBlock = dstBlockStart;
            srcBlock = srcBlockStart;

            for(x = startx, qp_index=0; x < endx; x+=8, qp_index++){
                c.QP = c.QP_block[qp_index];
                c.nonBQP = c.nonBQP_block[qp_index];
                c.pQPb = c.pQPb_block[qp_index];
                c.pQPb2 = c.pQPb2_block[qp_index];
                /* check if we have a previous block to deblock it with dstBlock */
                if(x - 8 >= 0){
                    hdeblock_internal(dstBlock, dstStride, &c, mode);
                }
            }
        }

        if(mode & DERING){
            if(y > 0) dering(dstBlock - dstStride - 8, dstStride, &c);
        }

        if((mode & TEMP_NOISE_FILTER)){
            tempNoiseReducer(dstBlock-8, dstStride,
                             c.tempBlurred[isColor] + y*dstStride + x,
                             c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                             c.ppMode.maxTmpNoise);
        }

        /* did we use a tmp buffer for the last lines*/
        if(y+15 >= height){
            uint8_t *dstBlock = &(dst[y*dstStride]);
            if(width==FFABS(dstStride)){
                linecpy(dstBlock, tempDst + dstStride, height-y, dstStride);
            } else {
                int i;
                for(i=0; i<height-y; i++){
                    memcpy(dstBlock + i*dstStride, tempDst + (i+1)*dstStride, width);
                }
            }
        }
    }
#if POSTPROCESS_SIMD
    SIMD_CLEANUP();//basically for mmx code to call emms
#endif

#ifdef DEBUG_BRIGHTNESS
    if(!isColor){
        int max=1;
        int i;
        for(i=0; i<256; i++)
            if(yHistogram[i] > max) max=yHistogram[i];

        for(i=1; i<256; i++){
            int x;
            int start = yHistogram[i-1]/(max/256+1);
            int end = yHistogram[i]/(max/256+1);
            int inc = end > start ? 1 : -1;
            for(x=start; x!=end+inc; x+=inc)
                dst[i*dstStride + x]+=128;
        }

        for(i=0; i<100; i+=2){
            dst[(white)*dstStride + i]+=128;
            dst[(black)*dstStride + i]+=128;
        }
    }
#endif

    *c2 = c; //copy local context back

}

#undef deinterlace_interpolate_linear
#undef deinterlace_interpolate_cubic
#undef deinterlace_FF
#undef deinterlace_l5
#undef deinterlace_blend_linear
#undef deinterlace_median
#undef horiz_classify
#undef vert_classify
#undef do_horiz_def_filter
#undef do_horiz_low_pass
#undef horiz_X1_filter
#undef do_a_deblock
#undef do_vert_low_pass
#undef vert_X1_filter
#undef do_vert_def_filter
#undef temp_noise_reducer
#undef dering
#undef prefetchnta
#undef prefetcht0
#undef block_copy
#undef duplicate
#undef deinterlace_internal
#undef vdeblock_internal
#undef hdeblock_internal
/*
  ex use:

  #define SUFFIX C
  #define POSTPROCESS_SIMD 0
  #include "postprocess_internal.c"
  #undef SUFFIX
  #undef POSTPROCESS_SIMD

  #define POSTPROCESS_SIMD 1
  #define SUFFIX MMX2
  #define SIMD_CLEANUP() __asm__ volatile("emms")
  #include "postprocess_internal.c"
  #undef SUFFIX
  #undef SIMD_CLEANUP

  #define SIMD_CLEANUP()
  #define SUFFIX SSE2
  #include "postprocess_internal.c"
  #undef SUFFIX
  
  #define SUFFIX AVX2
  #include "postprocess_internal.c"
  #undef SUFFIX
  
  /* do altivec stuff */
*/
