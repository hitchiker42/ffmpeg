#include "postprocess_internal.h"
#if HAVE_AVX2
#define RENAME(name) x264_##name##_avx2
#elif HAVE_SSE2
#define RENAME(name) x264_##name##_SSE2
#elif HAVE_MMX2
#define RENAME(name) x264_##name##_mmx2
#else
#define RENAME(name) name##_C
#endif
int  RENAME(vertClassify)(const uint8_t src[], int stride, PPContext *c);
void RENAME(doVertLowPass)(uint8_t *src, int stride, PPContext *c);
int  RENAME(vertClassify)(const uint8_t src[], int stride, PPContext *c);
void RENAME(doVertLowPass)(uint8_t *src, int stride, PPContext *c);
void RENAME(vertX1Filter)(uint8_t *src, int stride, PPContext *co);
void RENAME(doVertDefFilter)(uint8_t src[], int stride, PPContext *c);
void RENAME(dering)(uint8_t src[], int stride, PPContext *c);
void RENAME(deInterlaceInterpolateLinear)(uint8_t src[], int stride);
void RENAME(deInterlaceInterpolateCubic)(uint8_t src[], int stride);
void RENAME(deInterlaceFF)(uint8_t src[], int stride, uint8_t *tmp);
void RENAME(deInterlaceL5)(uint8_t src[], int stride, uint8_t *tmp, uint8_t *tmp2);
void RENAME(deInterlaceBlendLinear)(uint8_t src[], int stride, uint8_t *tmp);
void RENAME(deInterlaceMedian)(uint8_t src[], int stride);
void RENAME(transpose1)(uint8_t *dst1, uint8_t *dst2,
                                    const uint8_t *src, int srcStride);
void RENAME(transpose2)(uint8_t *dst, int dstStride, const uint8_t *src);
void RENAME(tempNoiseReducer)(uint8_t *src, int stride,;
void RENAME(do_a_deblock)(uint8_t *src, int step, int stride, const PPContext *c, int mode){;
void RENAME(postProcess)(const uint8_t src[], int srcStride, uint8_t dst[], int dstStride,
                                     int width, int height, const QP_STORE_T QPs[],
                                     int QPStride, int isColor, PPContext *c);
void RENAME(blockCopy)(uint8_t dst[], int dstStride, const uint8_t src[], int srcStride,
                                   int levelFix, int64_t *packedOffsetAndScale);
void RENAME(duplicate)(uint8_t src[], int stride);


/**
 * Filter array of bytes (Y or U or V values)
 */
static void postProcess(const uint8_t src[], int srcStride, uint8_t dst[], int dstStride, int width, int height,
                        const QP_STORE_T QPs[], int QPStride, int isColor, PPContext *c2)
{
    DECLARE_ALIGNED(8, PPContext, c)= *c2; //copy to stack for faster access
    int x,y;

    const int mode= isColor ? c.ppMode.chromMode : c.ppMode.lumMode;

    int black=0, white=255; // blackest black and whitest white in the picture
    int QPCorrecture= 256*256;

    int copyAhead;
#if ARCH_X86
    int i, j;
#endif

    const int qpHShift= isColor ? 4-c.hChromaSubSample : 4;
    const int qpVShift= isColor ? 4-c.vChromaSubSample : 4;

    //FIXME remove
    uint64_t * const yHistogram= c.yHistogram;
    uint8_t * const tempSrc= srcStride > 0 ? c.tempSrc : c.tempSrc - 23*srcStride;
    uint8_t * const tempDst= (dstStride > 0 ? c.tempDst : c.tempDst - 23*dstStride) + 32;
    //const int mbWidth= isColor ? (width+7)>>3 : (width+15)>>4;

    if (mode & VISUALIZE){
        if(!(mode & (V_A_DEBLOCK | H_A_DEBLOCK)) || TEMPLATE_PP_MMX) {
            av_log(c2, AV_LOG_WARNING, "Visualization is currently only supported with the accurate deblock filter without SIMD\n");
        }
    }
//redefine mmxDcOffset/Threshold to simd_dc_offset/threshold
#if ARCH_X86
    for(i=0; i<57; i++){
        int offset= ((i*c.ppMode.baseDcDiff)>>8) + 1;
        int threshold= offset*2 + 1;
        for(j = 0;j < block_width/8; j++){
          c.mmxDcOffset[i][j]= 0x7F - offset;
          c.mmxDcThreshold[i][j]= 0x7F - threshold;
          c.mmxDcOffset[i][j]*= 0x0101010101010101LL;
          c.mmxDcThreshold[i][j]*= 0x0101010101010101LL;
        }
    }
#endif

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

#if ARCH_X86
        c.packedYScale[0]= (uint16_t)(scale*256.0 + 0.5);
        c.packedYOffset[0]= (((black*c.packedYScale)>>8) - c.ppMode.minAllowedY) & 0xFFFF;

        c.packedYOffset[0]|= c.packedYOffset<<32;
        c.packedYOffset[0]|= c.packedYOffset<<16;

        c.packedYScale[0]|= c.packedYScale<<32;
        c.packedYScale[0]|= c.packedYScale<<16;

        c.packedYScale[1] = c.packedYScale[2] = c.packedYScale[3] = c.packedYScale[0];
        c.packedYOffset[1] = c.packedYOffset[2] = c.packedYOffset[3] = c.packedYOffset[0];
#else
        c.packedYScale= (uint16_t)(scale*1024.0 + 0.5);
        c.packedYOffset= (black - c.ppMode.minAllowedY) & 0xFFFF;

        c.packedYOffset|= c.packedYOffset<<32;
        c.packedYOffset|= c.packedYOffset<<16;

        c.packedYScale|= c.packedYScale<<32;
        c.packedYScale|= c.packedYScale<<16;
#endif

        if(mode & LEVEL_FIX)        QPCorrecture= (int)(scale*256*256 + 0.5);
        else                        QPCorrecture= 256*256;
    } else {
#if ARCH_X86
        c.packedYScale= {0x0100010001000100LL,0x0100010001000100LL,
                         0x0100010001000100LL,0x0100010001000100LL};
        c.packedYOffset= {0,0,0,0};
#else
        c.packedYScale= 0x0100010001000100LL;
        c.packedYOffset= 0;
#endif
        QPCorrecture= 256*256;
    }

    /* copy & deinterlace first row of blocks */
    y=-block_height;
    {
        const uint8_t *srcBlock= &(src[y*srcStride]);
        uint8_t *dstBlock= tempDst + dstStride;

        for(x=0; x<width; x+=block_width){
            prefetchnta(srcBlock + (((x>>2)&6) + 5)*srcStride + 32);
            prefetchnta(srcBlock + (((x>>2)&6) + 6)*srcStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + 5)*dstStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + 6)*dstStride + 32);

            RENAME(blockCopy)(dstBlock + dstStride*8, dstStride,
                              srcBlock + srcStride*8, srcStride, mode & LEVEL_FIX, &c.packedYOffset);

            RENAME(duplicate)(dstBlock + dstStride*8, dstStride);
//move these checks outh of the inner loop
            if(mode & LINEAR_IPOL_DEINT_FILTER)
                RENAME(deInterlaceInterpolateLinear)(dstBlock, dstStride);
            else if(mode & LINEAR_BLEND_DEINT_FILTER)
                RENAME(deInterlaceBlendLinear)(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & MEDIAN_DEINT_FILTER)
                RENAME(deInterlaceMedian)(dstBlock, dstStride);
            else if(mode & CUBIC_IPOL_DEINT_FILTER)
                RENAME(deInterlaceInterpolateCubic)(dstBlock, dstStride);
            else if(mode & FFMPEG_DEINT_FILTER)
                RENAME(deInterlaceFF)(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & LOWPASS5_DEINT_FILTER)
                RENAME(deInterlaceL5)(dstBlock, dstStride, c.deintTemp + x, c.deintTemp + width + x);
/*          else if(mode & CUBIC_BLEND_DEINT_FILTER)
                RENAME(deInterlaceBlendCubic)(dstBlock, dstStride);
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

    for(y=0; y<height; y+=block_height){
        //1% speedup if these are here instead of the inner loop
        const uint8_t *srcBlock= &(src[y*srcStride]);
        uint8_t *dstBlock= &(dst[y*dstStride]);
#if ARCH_X86
        uint8_t *tempBlock1= c.tempBlocks;
        uint8_t *tempBlock2= c.tempBlocks + 8;
#endif
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
        for(x=0; x<width; x+=block_width){
            const int stride= dstStride;
#if ARCH_X86
            uint8_t *tmpXchg;
#endif
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
#if ARCH_X86
            __asm__ volatile(
                "movd %1, %%mm7         \n\t"
                "packuswb %%mm7, %%mm7  \n\t" // 0, 0, 0, QP, 0, 0, 0, QP
                "packuswb %%mm7, %%mm7  \n\t" // 0,QP, 0, QP, 0,QP, 0, QP
                "packuswb %%mm7, %%mm7  \n\t" // QP,..., QP
                "movq %%mm7, %0         \n\t"
                : "=m" (c.pQPb)
                : "r" (QP)
            );
#endif



            prefetchnta(srcBlock + (((x>>2)&6) + 5)*srcStride + 32);
            prefetchnta(srcBlock + (((x>>2)&6) + 6)*srcStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + 5)*dstStride + 32);
            prefetcht0(dstBlock + (((x>>2)&6) + 6)*dstStride + 32);
            RENAME(blockCopy)(dstBlock + dstStride*copyAhead, dstStride,
                              srcBlock + srcStride*copyAhead, srcStride, mode & LEVEL_FIX, &c.packedYOffset);

            if(mode & LINEAR_IPOL_DEINT_FILTER)
                RENAME(deInterlaceInterpolateLinear)(dstBlock, dstStride);
            else if(mode & LINEAR_BLEND_DEINT_FILTER)
                RENAME(deInterlaceBlendLinear)(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & MEDIAN_DEINT_FILTER)
                RENAME(deInterlaceMedian)(dstBlock, dstStride);
            else if(mode & CUBIC_IPOL_DEINT_FILTER)
                RENAME(deInterlaceInterpolateCubic)(dstBlock, dstStride);
            else if(mode & FFMPEG_DEINT_FILTER)
                RENAME(deInterlaceFF)(dstBlock, dstStride, c.deintTemp + x);
            else if(mode & LOWPASS5_DEINT_FILTER)
                RENAME(deInterlaceL5)(dstBlock, dstStride, c.deintTemp + x, c.deintTemp + width + x);
/*          else if(mode & CUBIC_BLEND_DEINT_FILTER)
                RENAME(deInterlaceBlendCubic)(dstBlock, dstStride);
*/

            /* only deblock if we have 2 blocks */
            if(y + 8 < height){
                if(mode & V_X1_FILTER)
                    RENAME(vertX1Filter)(dstBlock, stride, &c);
                else if(mode & V_DEBLOCK){
                    const int t= RENAME(vertClassify)(dstBlock, stride, &c);

                    if(t==1)
                        RENAME(doVertLowPass)(dstBlock, stride, &c);
                    else if(t==2)
                        RENAME(doVertDefFilter)(dstBlock, stride, &c);
                }else if(mode & V_A_DEBLOCK){
                    RENAME(do_a_deblock)(dstBlock, stride, 1, &c, mode);
                }
            }
            /* check if we have a previous block to deblock it with dstBlock */
            if(x - 8 >= 0){
#if ARCH_X86
            RENAME(transpose1)(tempBlock1, tempBlock2, dstBlock, dstStride);
            /* check if we have a previous block to deblock it with dstBlock */
            if(x - 8 >= 0){
                if(mode & H_X1_FILTER)
                        RENAME(vertX1Filter)(tempBlock1, 16, &c);
                else if(mode & H_DEBLOCK){
//START_TIMER
                    const int t= RENAME(vertClassify)(tempBlock1, 16, &c);
//STOP_TIMER("dc & minmax")
                    if(t==1)
                        RENAME(doVertLowPass)(tempBlock1, 16, &c);
                    else if(t==2)
                        RENAME(doVertDefFilter)(tempBlock1, 16, &c);
                }else if(mode & H_A_DEBLOCK){
                        RENAME(do_a_deblock)(tempBlock1, 16, 1, &c, mode);
                }

                RENAME(transpose2)(dstBlock-4, dstStride, tempBlock1 + 4*16);
#elif HAVE_ALTIVEC
                if(mode & H_X1_FILTER)
                    horizX1Filter(dstBlock-4, stride, QP);
                else if(mode & H_DEBLOCK){
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
                }else if(mode & H_A_DEBLOCK){
                    RENAME(do_a_deblock)(dstBlock-8, 1, stride, &c, mode);
                }
#else
                if(mode & H_X1_FILTER)
                    horizX1Filter(dstBlock-4, stride, QP);
                else if(mode & H_DEBLOCK){
                    const int t= RENAME(horizClassify)(dstBlock-4, stride, &c);

                    if(t==1)
                        RENAME(doHorizLowPass)(dstBlock-4, stride, &c);
                    else if(t==2)
                        RENAME(doHorizDefFilter)(dstBlock-4, stride, &c);
                }else if(mode & H_A_DEBLOCK){
                    RENAME(do_a_deblock)(dstBlock-8, 1, stride, &c, mode);
                }
#endif //ARCH_X86
                if(mode & DERING){
                //FIXME filter first line
                    if(y>0) RENAME(dering)(dstBlock - stride - 8, stride, &c);
                }

                if(mode & TEMP_NOISE_FILTER)
                {
                    RENAME(tempNoiseReducer)(dstBlock-8, stride,
                            c.tempBlurred[isColor] + y*dstStride + x,
                            c.tempBlurredPast[isColor] + (y>>3)*256 + (x>>3) + 256,
                            c.ppMode.maxTmpNoise);
                }
            }

            dstBlock+=8;
            srcBlock+=8;

#if ARCH_X86
            tmpXchg= tempBlock1;
            tempBlock1= tempBlock2;
            tempBlock2 = tmpXchg;
#endif
        }

        if(mode & DERING){
            if(y > 0) RENAME(dering)(dstBlock - dstStride - 8, dstStride, &c);
        }

        if((mode & TEMP_NOISE_FILTER)){
            RENAME(tempNoiseReducer)(dstBlock-8, dstStride,
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
            int start=yHistogram[i-1]/(max/256+1);
            int end=yHistogram[i]/(max/256+1);
            int inc= end > start ? 1 : -1;
            for(x=start; x!=end+inc; x+=inc)
                dst[ i*dstStride + x]+=128;
        }

        for(i=0; i<100; i+=2){
            dst[ (white)*dstStride + i]+=128;
            dst[ (black)*dstStride + i]+=128;
        }
    }
#endif

    *c2= c; //copy local context back

}
#undef RENAME
