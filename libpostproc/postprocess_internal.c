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
#define deinterlace_interpolate_linear deinterlace_interpolate_linear_## ##SUFFIX
#define deinterlace_interpolate_cubic deinterlace_interpolate_cubic_## ##SUFFIX
#define deinterlace_FF deinterlace_FF_## ##SUFFIX
#define deinterlace_l5 deinterlace_l5_## ##SUFFIX
#define deinterlace_blend_linear deinterlace_blend_linear_## ##SUFFIX
#define deinterlace_median deinterlace_median_## ##SUFFIX
#define horiz_classify horiz_classify_## ##SUFFIX
#define vert_classify vert_classify_## ##SUFFIX
#define do_horiz_def_filter do_horiz_def_filter_## ##SUFFIX
#define do_horiz_low_pass do_horiz_low_pass_## ##SUFFIX
#define horiz_X1_filter horiz_X1_filter_## ##SUFFIX
#define do_a_deblock do_a_deblock_## ##SUFFIX
#define do_vert_low_pass do_vert_low_pass_## ##SUFFIX
#define vert_X1_filter vert_X1_filter_## ##SUFFIX
#define do_vert_def_filter do_vert_def_filter_## ##SUFFIX
#define temp_noise_reducer temp_noise_reducer_## ##SUFFIX
#define dering dering_## ##SUFFIX
/**
 * Filter array of bytes (Y or U or V values)
 */
void RENAME(postProcess)(const uint8_t src[], int srcStride,
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
    uint64_t * const yHistogram = c.yHistogram;
    uint8_t * const tempSrc = srcStride > 0 ? c.tempSrc : c.tempSrc - 23*srcStride;
    uint8_t * const tempDst = (dstStride > 0 ? c.tempDst : c.tempDst - 23*dstStride) + 32;

    if (mode & VISUALIZE){
        if(!(mode & (V_A_DEBLOCK | H_A_DEBLOCK)) || TEMPLATE_PP_MMX) {
            av_log(c2, AV_LOG_WARNING, "Visualization is currently only supported with the accurate deblock filter without SIMD\n");
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

        scale = (double)(c.ppMode.maxAllowedY - c.ppMode.minAllowedY) / (double)(white-black);

#if TEMPLATE_PP_MMXEXT
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

        if(mode & LEVEL_FIX)        QPCorrecture = (int)(scale*256*256 + 0.5);
        else                        QPCorrecture = 256*256;
    } else {
        c.packedYScale= 0x0100010001000100LL;
        c.packedYOffset= 0;
        QPCorrecture= 256*256;
    }

#if TEMPLATE_PP_SSE2
#undef RENAME
#define RENAME(x) x ## _MMX2
#endif
    /* copy & deinterlace first row of blocks */
    y=-BLOCK_SIZE;
    {
        const uint8_t *srcBlock= &(src[y*srcStride]);
        uint8_t *dstBlock= tempDst + dstStride;

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // finish 1 block before the next otherwise we might have a problem
        // with the L1 Cache of the P4 ... or only a few blocks at a time or something
        for(x=0; x<width;){
            int endx = FFMIN(width, x+32);
            int num_blocks = (endx-x)/8;
            int block_index;
            RENAME(prefetchnta)(srcBlock + (((x>>2)&6) + copyAhead)*srcStride + 32);
            RENAME(prefetchnta)(srcBlock + (((x>>2)&6) + copyAhead+1)*srcStride + 32);
            RENAME(prefetcht0)(dstBlock + (((x>>2)&6) + copyAhead)*dstStride + 32);
            RENAME(prefetcht0)(dstBlock + (((x>>2)&6) + copyAhead+1)*dstStride + 32);
            for(block_index=0; block_index<num_blocks; block_index++){
                RENAME_SCALAR(duplicate)(dstBlock + (dstStride+block_index)*8, dstStride);
                RENAME_SCALAR(blockCopy)(dstBlock + (dstStride+block_index)*8, dstStride,
                                         srcBlock + (srcStride+block_index)*8, srcStride,
                                         mode & LEVEL_FIX, &c.packedYOffset);
            }
            RENAME(deInterlace)(dstBlock, dstStride, c.deintTemp +x,
                                c.deintTemp + width + x, mode, num_blocks);
            dstBlock += num_blocks*8;
            srcBlock += num_blocks*8;
            x = endx;
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
        const uint8_t *srcBlock = &(src[y*srcStride]);
        uint8_t *dstBlock = &(dst[y*dstStride]);
#if TEMPLATE_PP_MMX
        uint8_t *tempBlock1 = c.tempBlocks;
        uint8_t *tempBlock2 = c.tempBlocks + 8;
#endif
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

            dstBlock= tempDst + dstStride;
            srcBlock= tempSrc;
        }

        // From this point on it is guaranteed that we can read and write 16 lines downward
        // finish 1 block before the next otherwise we might have a problem
        // with the L1 Cache of the P4 ... or only a few blocks at a time or something
        for(x=0; x<width; ){
            int startx = x;
            int endx = FFMIN(width, x+32);
            int num_blocks = (endx-startx)/8;
            int block_index;
            int qp_index = 0;
            for(qp_index=0; qp_index < num_blocks; qp_index++){
                QP = QPptr[(x+qp_index*BLOCK_SIZE)>>qpHShift];
                nonBQP = nonBQPptr[(x+qp_index*BLOCK_SIZE)>>qpHShift];

                if(!isColor){
                    QP = (QP* QPCorrecture + 256*128)>>16;
                    nonBQP = (nonBQP* QPCorrecture + 256*128)>>16;
                    yHistogram[(srcBlock+qp_index*8)[srcStride*12 + 4]]++;
                }
                c.QP_block[qp_index] = QP;
                c.nonBQP_block[qp_index] = nonBQP;
#if TEMPLATE_PP_MMX
                RENAME(packQP)(&c);
#endif
            }
            RENAME(prefetchnta)(srcBlock + (((x>>2)&6) + copyAhead)*srcStride + 32);
            RENAME(prefetchnta)(srcBlock + (((x>>2)&6) + copyAhead+1)*srcStride + 32);
            RENAME(prefetcht0)(dstBlock + (((x>>2)&6) + copyAhead)*dstStride + 32);
            RENAME(prefetcht0)(dstBlock + (((x>>2)&6) + copyAhead+1)*dstStride + 32);
            for(block_index=0; block_index<num_blocks; block_index++){
                RENAME_SCALAR(blockCopy)(dstBlock + dstStride*copyAhead + block_index*8, dstStride,
                                         srcBlock + srcStride*copyAhead + block_index*8, srcStride,
                                         mode & LEVEL_FIX, &c.packedYOffset);
            }
            RENAME(deInterlace)(dstBlock, dstStride, c.deintTemp +x,
                                c.deintTemp + width + x, mode, num_blocks);
            if(y+8<height){
                RENAME(deblock)(dstBlock, dstStride, 1, c, mode, num_blocks);
            }
            for(x = startx, qp_index=0; x < endx; x+=BLOCK_SIZE, qp_index++){
                const int stride= dstStride;
                av_unused uint8_t *tmpXchg;
                c.QP = c.QP_block[qp_index];
                c.nonBQP = c.nonBQP_block[qp_index];
                c.pQPb = c.pQPb_block[qp_index];
                c.pQPb2 = c.pQPb2_block[qp_index];
#if TEMPLATE_PP_MMX
                RENAME_SCALAR(transpose1)(tempBlock1, tempBlock2, dstBlock, dstStride);
#endif
                /* check if we have a previous block to deblock it with dstBlock */
                if(x - 8 >= 0){
#if TEMPLATE_PP_MMX
                if(mode & H_X1_FILTER)
                        RENAME_SCALAR(vertX1Filter)(tempBlock1, 16, &c);
                else if(mode & H_DEBLOCK){
                    const int t= RENAME_SCALAR(vertClassify)(tempBlock1, 16, &c);
                    if(t==1)
                        RENAME_SCALAR(doVertLowPass)(tempBlock1, 16, &c);
                    else if(t==2)
                        RENAME_SCALAR(doVertDefFilter)(tempBlock1, 16, &c);
                }else if(mode & H_A_DEBLOCK){
                        RENAME_SCALAR(do_a_deblock)(tempBlock1, 16, 1, &c, mode);
                }

                RENAME_SCALAR(transpose2)(dstBlock-4, dstStride, tempBlock1 + 4*16);
                    if(mode & H_X1_FILTER){
                        RENAME_SCALAR(vertX1Filter)(tempBlock1, 16, &c);
                    } else if(mode & H_DEBLOCK){
                        const int t= RENAME_SCALAR(vertClassify)(tempBlock1, 16, &c);
                        if(t==1){
                            RENAME_SCALAR(doVertLowPass)(tempBlock1, 16, &c);
                        } else if(t==2){
                            RENAME_SCALAR(doVertDefFilter)(tempBlock1, 16, &c);
                        }
                    } else if(mode & H_A_DEBLOCK){
                        RENAME_SCALAR(do_a_deblock)(tempBlock1, 16, 1, &c, mode);
                    }

                    RENAME_SCALAR(transpose2)(dstBlock-4, dstStride, tempBlock1 + 4*16);

#else
                    if(mode & H_X1_FILTER){
                        horizX1Filter(dstBlock-4, stride, c.QP);
                    } else if(mode & H_DEBLOCK){
#if TEMPLATE_PP_ALTIVEC
                        DECLARE_ALIGNED(16, unsigned char, tempBlock)[272];
                        int t;
                        transpose_16x8_char_toPackedAlign_altivec(tempBlock, dstBlock - (4 + 1), stride);

                        t = vertClassify_altivec(tempBlock-48, 16, &c);
                        if(t==1) {
                            doVertLowPass_altivec(tempBlock-48, 16, &c);
                            transpose_8x16_char_fromPackedAlign_altivec(dstBlock - (4 + 1), tempBlock, stride);
                        }
                        else if(t==2) {
                            doVertDefFilter_altivec(tempBlock-48, 16, &c);
                            transpose_8x16_char_fromPackedAlign_altivec(dstBlock - (4 + 1), tempBlock, stride);
                        }
#else
                        const int t= RENAME_SCALAR(horizClassify)(dstBlock-4, stride, &c);

                        if(t==1){
                            RENAME_SCALAR(doHorizLowPass)(dstBlock-4, stride, &c);
                        } else if(t==2){
                            RENAME_SCALAR(doHorizDefFilter)(dstBlock-4, stride, &c);
                        }
#endif
                    } else if(mode & H_A_DEBLOCK){
                        RENAME_SCALAR(do_a_deblock)(dstBlock-8, 1, stride, &c, mode);
                    }
#endif //TEMPLATE_PP_MMX
                    if(mode & DERING){
                        //FIXME filter first line
                        if(y>0) RENAME_SCALAR(dering)(dstBlock - stride - 8, stride, &c);
                    }

                    if(mode & TEMP_NOISE_FILTER){
                        RENAME_SCALAR(tempNoiseReducer)(dstBlock-8, stride,
                                                 c.tempBlurred[isColor] + y*dstStride + x,
                                                 c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                                                 c.ppMode.maxTmpNoise);
                    }
                }

                dstBlock+=8;
                srcBlock+=8;

#if TEMPLATE_PP_MMX
                tmpXchg = tempBlock1;
                tempBlock1= tempBlock2;
                tempBlock2 = tmpXchg;
#endif
            }
        }

        if(mode & DERING){
            if(y > 0) RENAME_SCALAR(dering)(dstBlock - dstStride - 8, dstStride, &c);
        }

        if((mode & TEMP_NOISE_FILTER)){
            RENAME_SCALAR(tempNoiseReducer)(dstBlock-8, dstStride,
                                            c.tempBlurred[isColor] + y*dstStride + x,
                                            c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                                            c.ppMode.maxTmpNoise);
        }

        /* did we use a tmp buffer for the last lines*/
        if(y+15 >= height){
            uint8_t *dstBlock= &(dst[y*dstStride]);
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
#if   TEMPLATE_PP_3DNOW
    __asm__ volatile("femms");
#elif TEMPLATE_PP_MMX
    __asm__ volatile("emms");
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
                dst[ i*dstStride + x]+=128;
        }

        for(i=0; i<100; i+=2){
            dst[(white)*dstStride + i]+=128;
            dst[(black)*dstStride + i]+=128;
        }
    }
#endif

    *c2= c; //copy local context back

}
