#ifdef TEMPLATE_PP_C
#   define RENAME(a) a ## _C
#else
#   define TEMPLATE_PP_C 0
#endif

#ifdef TEMPLATE_PP_ALTIVEC
#   define RENAME(a) a ## _altivec
#else
#   define TEMPLATE_PP_ALTIVEC 0
#endif

#ifdef TEMPLATE_PP_MMX
#   define RENAME(a) a ## _MMX
#else
#   define TEMPLATE_PP_MMX 0
#endif

#ifdef TEMPLATE_PP_MMXEXT
#   undef  TEMPLATE_PP_MMX
#   define TEMPLATE_PP_MMX 1
#   define RENAME(a) a ## _MMX2
#else
#   define TEMPLATE_PP_MMXEXT 0
#endif

#ifdef TEMPLATE_PP_3DNOW
#   undef  TEMPLATE_PP_MMX
#   define TEMPLATE_PP_MMX 1
#   define RENAME(a) a ## _3DNow
#else
#   define TEMPLATE_PP_3DNOW 0
#endif

#ifdef TEMPLATE_PP_SSE2
#   undef  TEMPLATE_PP_MMX
#   define TEMPLATE_PP_MMX 1
#   undef  TEMPLATE_PP_MMXEXT
#   define TEMPLATE_PP_MMXEXT 1
#   define RENAME(a) a ## _sse2
#else
#   define TEMPLATE_PP_SSE2 0
#endif
#ifdef TEMPLATE_PP_AVX2
#   undef  TEMPLATE_PP_MMX
#   define TEMPLATE_PP_MMX 1
#   undef  TEMPLATE_PP_MMXEXT
#   define TEMPLATE_PP_MMXEXT 1
#   undef  TEMPLATE_PP_SSE2
#   define TEMPLATE_PP_SSE2 1
#   define RENAME(a) a ## _avx2
#else
#   define TEMPLATE_PP_AVX2 0
#endif
#undef REAL_PAVGB
#undef PAVGB
#undef PMINUB
#undef PMAXUB

#if   TEMPLATE_PP_MMXEXT
#define REAL_PAVGB(a,b) "pavgb " #a ", " #b " \n\t"
#elif TEMPLATE_PP_3DNOW
#define REAL_PAVGB(a,b) "pavgusb " #a ", " #b " \n\t"
#endif
#define PAVGB(a,b)  REAL_PAVGB(a,b)

#if   TEMPLATE_PP_MMXEXT
#define PMINUB(a,b,t) "pminub " #a ", " #b " \n\t"
#elif TEMPLATE_PP_MMX
#define PMINUB(b,a,t) \
    "movq " #a ", " #t " \n\t"\
    "psubusb " #b ", " #t " \n\t"\
    "psubb " #t ", " #a " \n\t"
#endif

#if   TEMPLATE_PP_MMXEXT
#define PMAXUB(a,b) "pmaxub " #a ", " #b " \n\t"
#elif TEMPLATE_PP_MMX
#define PMAXUB(a,b) \
    "psubusb " #a ", " #b " \n\t"\
    "paddb " #a ", " #b " \n\t"
#endif
#if TEMPLATE_PP_SSE2
extern void RENAME(ff_deInterlaceInterpolateLinear)(uint8_t *, int);
extern void RENAME(ff_deInterlaceInterpolateCubic)(uint8_t *, int);
extern void RENAME(ff_deInterlaceFF)(uint8_t *, int, uint8_t *);
extern void RENAME(ff_deInterlaceL5)(uint8_t *, int, uint8_t *, uint8_t*);
extern void RENAME(ff_deInterlaceBlendLinear)(uint8_t *, int, uint8_t *);
extern void RENAME(ff_deInterlaceMedian)(uint8_t *, int);
extern void RENAME(ff_blockCopy)(uint8_t*,int,const uint8_t*,
                                 int,int,int64_t*);
extern void RENAME(ff_duplicate)(uint8_t*, int);
static inline void RENAME(deInterlaceInterpolateLinear)(uint8_t src[],
                                                        int stride)
{
    RENAME(ff_deInterlaceInterpolateLinear)(src, stride);
}
static inline void RENAME(deInterlaceInterpolateCubic)(uint8_t src[],
                                                        int stride)
{
    RENAME(ff_deInterlaceInterpolateCubic)(src, stride);
}
static inline void RENAME(deInterlaceFF)(uint8_t src[], int stride,
                                         uint8_t *tmp)
{
    RENAME(ff_deInterlaceFF)(src, stride, tmp);
}
static inline void RENAME(deInterlaceL5)(uint8_t src[], int stride,
                                         uint8_t *tmp, uint8_t *tmp2)
{
    RENAME(ff_deInterlaceL5)(src, stride, tmp, tmp2);
}
static inline void RENAME(deInterlaceBlendLinear)(uint8_t src[], int stride,
                                                  uint8_t *tmp)
{
    RENAME(ff_deInterlaceBlendLinear)(src, stride, tmp);
}
static inline void RENAME(deInterlaceMedian)(uint8_t src[], int stride)
{
    RENAME(ff_deInterlaceMedian)(src, stride);
}
static inline void RENAME(blockCopy)(uint8_t dst[], int dstStride,
                                     const uint8_t src[], int srcStride,
                                     int levelFix, int64_t *packedOffsetAndScale)
{
    RENAME(ff_blockCopy)(dst,dstStride,src,srcStride,
                         levelFix,packedOffsetAndScale);
}
static inline void RENAME(duplicate)(uint8_t* src, int stride){
    RENAME(ff_duplicate)(src, stride);
}
/*
Use yasm mmx code
#if !TEMPLATE_PP_AVX2
static inline void deInterlaceInterpolateCubic_mmx2(uint8_t src[],
                                                    int stride)
{
    ff_deInterlaceInterpolateCubic_mmx2(src, stride);
}
static inline void deInterlaceInterpolateLinear_mmx2(uint8_t src[],
                                                     int stride)
{
    ff_deInterlaceInterpolateLinear_mmx2(src, stride);
}
static inline void deInterlaceFF_mmx2(uint8_t src[], int stride,
                                      uint8_t *tmp)
{
    ff_deInterlaceFF_mmx2(src, stride, tmp);
}
static inline void deInterlaceL5_mmx2(uint8_t src[], int stride,
                                      uint8_t *tmp, uint8_t *tmp2)
{
    ff_deInterlaceL5_mmx2(src, stride, tmp, tmp2);
}
static inline void deInterlaceBlendLinear_mmx2(uint8_t src[], int stride,
                                               uint8_t *tmp)
{
    ff_deInterlaceBlendLinear_mmx2(src, stride, tmp);
}
static inline void deInterlaceMedian_mmx2(uint8_t src[], int stride){
    ff_deInterlaceMedian_mmx2(src, stride);
}
static inline void blockCopy_mmx2(uint8_t dst[], int dstStride,
                                  const uint8_t src[], int srcStride,
                                  int levelFix, int64_t *packedOffsetAndScale){
    ff_blockCopy_mmx2(dst,dstStride,src,srcStride,
                      levelFix,packedOffsetAndScale);
}
#endif //!TEMPLATE_PP_AVX2*/
#else
/**
 * Deinterlace the given block by linearly interpolating every second line.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 */
static inline void RENAME(deInterlaceInterpolateLinear)(uint8_t src[], int stride)
{
#if TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
    src+= 4*stride;
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rcx      \n\t"
//      0       1       2       3       4       5       6       7       8       9
//      %0      eax     eax+%1  eax+2%1 %0+4%1  ecx     ecx+%1  ecx+2%1 %0+8%1  ecx+4%1

        "movq (%0), %%mm0                       \n\t"
        "movq (%%rax, %1), %%mm1            \n\t"
        PAVGB(%%mm1, %%mm0)
        "movq %%mm0, (%%rax)                \n\t"
        "movq (%0, %1, 4), %%mm0                \n\t"
        PAVGB(%%mm0, %%mm1)
        "movq %%mm1, (%%rax, %1, 2)         \n\t"
        "movq (%%rcx, %1), %%mm1            \n\t"
        PAVGB(%%mm1, %%mm0)
        "movq %%mm0, (%%rcx)                \n\t"
        "movq (%0, %1, 8), %%mm0                \n\t"
        PAVGB(%%mm0, %%mm1)
        "movq %%mm1, (%%rcx, %1, 2)         \n\t"

        : : "r" (src), "r" ((int64_t)stride)
        : "%rax", "%rcx"
    );
#else
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
#endif
}
/**
 * Deinterlace the given block by cubic interpolating every second line.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 3-15 and write 7-13
 */
static inline void RENAME(deInterlaceInterpolateCubic)(uint8_t src[], int stride)
{
#if TEMPLATE_PP_SSE2 || TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
    src+= stride*3;
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
        "lea (%%rdx, %1, 4), %%rcx      \n\t"
        "add %1, %%rcx                      \n\t"
#if TEMPLATE_PP_SSE2
        "pxor %%xmm7, %%xmm7                    \n\t"
#define REAL_DEINT_CUBIC(a,b,c,d,e)\
        "movq " #a ", %%xmm0                    \n\t"\
        "movq " #b ", %%xmm1                    \n\t"\
        "movq " #d ", %%xmm2                    \n\t"\
        "movq " #e ", %%xmm3                    \n\t"\
        "pavgb %%xmm2, %%xmm1                   \n\t"\
        "pavgb %%xmm3, %%xmm0                   \n\t"\
        "punpcklbw %%xmm7, %%xmm0               \n\t"\
        "punpcklbw %%xmm7, %%xmm1               \n\t"\
        "psubw %%xmm1, %%xmm0                   \n\t"\
        "psraw $3, %%xmm0                       \n\t"\
        "psubw %%xmm0, %%xmm1                   \n\t"\
        "packuswb %%xmm1, %%xmm1                \n\t"\
        "movlps %%xmm1, " #c "                  \n\t"
#else //TEMPLATE_PP_SSE2
        "pxor %%mm7, %%mm7                      \n\t"
//      0       1       2       3       4       5       6       7       8       9       10
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1 ecx

#define REAL_DEINT_CUBIC(a,b,c,d,e)\
        "movq " #a ", %%mm0                     \n\t"\
        "movq " #b ", %%mm1                     \n\t"\
        "movq " #d ", %%mm2                     \n\t"\
        "movq " #e ", %%mm3                     \n\t"\
        PAVGB(%%mm2, %%mm1)                             /* (b+d) /2 */\
        PAVGB(%%mm3, %%mm0)                             /* (a+e) /2 */\
        "movq %%mm0, %%mm2                      \n\t"\
        "punpcklbw %%mm7, %%mm0                 \n\t"\
        "punpckhbw %%mm7, %%mm2                 \n\t"\
        "movq %%mm1, %%mm3                      \n\t"\
        "punpcklbw %%mm7, %%mm1                 \n\t"\
        "punpckhbw %%mm7, %%mm3                 \n\t"\
        "psubw %%mm1, %%mm0                     \n\t"   /* L(a+e - (b+d))/2 */\
        "psubw %%mm3, %%mm2                     \n\t"   /* H(a+e - (b+d))/2 */\
        "psraw $3, %%mm0                        \n\t"   /* L(a+e - (b+d))/16 */\
        "psraw $3, %%mm2                        \n\t"   /* H(a+e - (b+d))/16 */\
        "psubw %%mm0, %%mm1                     \n\t"   /* L(9b + 9d - a - e)/16 */\
        "psubw %%mm2, %%mm3                     \n\t"   /* H(9b + 9d - a - e)/16 */\
        "packuswb %%mm3, %%mm1                  \n\t"\
        "movq %%mm1, " #c "                     \n\t"
#endif //TEMPLATE_PP_SSE2
#define DEINT_CUBIC(a,b,c,d,e)  REAL_DEINT_CUBIC(a,b,c,d,e)

DEINT_CUBIC((%0)        , (%%rax, %1), (%%rax, %1, 2), (%0, %1, 4) , (%%rdx, %1))
DEINT_CUBIC((%%rax, %1), (%0, %1, 4) , (%%rdx)       , (%%rdx, %1), (%0, %1, 8))
DEINT_CUBIC((%0, %1, 4) , (%%rdx, %1), (%%rdx, %1, 2), (%0, %1, 8) , (%%rcx))
DEINT_CUBIC((%%rdx, %1), (%0, %1, 8) , (%%rdx, %1, 4), (%%rcx)    , (%%rcx, %1, 2))

        : : "r" (src), "r" ((int64_t)stride)
        :
#if TEMPLATE_PP_SSE2
        XMM_CLOBBERS("%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm7",)
#endif
        "%rax", "%rdx", "%rcx"
    );
#undef REAL_DEINT_CUBIC
#else //TEMPLATE_PP_SSE2 || TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
    int x;
    src+= stride*3;
    for(x=0; x<8; x++){
        src[stride*3] = av_clip_uint8((-src[0]        + 9*src[stride*2] + 9*src[stride*4] - src[stride*6])>>4);
        src[stride*5] = av_clip_uint8((-src[stride*2] + 9*src[stride*4] + 9*src[stride*6] - src[stride*8])>>4);
        src[stride*7] = av_clip_uint8((-src[stride*4] + 9*src[stride*6] + 9*src[stride*8] - src[stride*10])>>4);
        src[stride*9] = av_clip_uint8((-src[stride*6] + 9*src[stride*8] + 9*src[stride*10] - src[stride*12])>>4);
        src++;
    }
#endif //TEMPLATE_PP_SSE2 || TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
}
/**
 * Deinterlace the given block by filtering every second line with a (-1 4 2 4 -1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 5-11
 */
static inline void RENAME(deInterlaceFF)(uint8_t src[], int stride, uint8_t *tmp)
{
#if TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
    src+= stride*4;
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
        "pxor %%mm7, %%mm7                      \n\t"
        "movq (%2), %%mm0                       \n\t"
//      0       1       2       3       4       5       6       7       8       9       10
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1 ecx

#define REAL_DEINT_FF(a,b,c,d)\
        "movq " #a ", %%mm1                     \n\t"\
        "movq " #b ", %%mm2                     \n\t"\
        "movq " #c ", %%mm3                     \n\t"\
        "movq " #d ", %%mm4                     \n\t"\
        PAVGB(%%mm3, %%mm1)                          \
        PAVGB(%%mm4, %%mm0)                          \
        "movq %%mm0, %%mm3                      \n\t"\
        "punpcklbw %%mm7, %%mm0                 \n\t"\
        "punpckhbw %%mm7, %%mm3                 \n\t"\
        "movq %%mm1, %%mm4                      \n\t"\
        "punpcklbw %%mm7, %%mm1                 \n\t"\
        "punpckhbw %%mm7, %%mm4                 \n\t"\
        "psllw $2, %%mm1                        \n\t"\
        "psllw $2, %%mm4                        \n\t"\
        "psubw %%mm0, %%mm1                     \n\t"\
        "psubw %%mm3, %%mm4                     \n\t"\
        "movq %%mm2, %%mm5                      \n\t"\
        "movq %%mm2, %%mm0                      \n\t"\
        "punpcklbw %%mm7, %%mm2                 \n\t"\
        "punpckhbw %%mm7, %%mm5                 \n\t"\
        "paddw %%mm2, %%mm1                     \n\t"\
        "paddw %%mm5, %%mm4                     \n\t"\
        "psraw $2, %%mm1                        \n\t"\
        "psraw $2, %%mm4                        \n\t"\
        "packuswb %%mm4, %%mm1                  \n\t"\
        "movq %%mm1, " #b "                     \n\t"\

#define DEINT_FF(a,b,c,d)  REAL_DEINT_FF(a,b,c,d)

DEINT_FF((%0)        , (%%rax)       , (%%rax, %1), (%%rax, %1, 2))
DEINT_FF((%%rax, %1), (%%rax, %1, 2), (%0, %1, 4) , (%%rdx)       )
DEINT_FF((%0, %1, 4) , (%%rdx)       , (%%rdx, %1), (%%rdx, %1, 2))
DEINT_FF((%%rdx, %1), (%%rdx, %1, 2), (%0, %1, 8) , (%%rdx, %1, 4))

        "movq %%mm0, (%2)                       \n\t"
        : : "r" (src), "r" ((int64_t)stride), "r"(tmp)
        : "%rax", "%rdx"
    );
#else //TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
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
#endif //TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
}

/**
 * Deinterlace the given block by filtering every line with a (-1 2 6 2 -1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 4-11
 */
static inline void RENAME(deInterlaceL5)(uint8_t src[], int stride, uint8_t *tmp, uint8_t *tmp2)
{
#if (TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW) && HAVE_6REGS
    src+= stride*4;
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
        "pxor %%mm7, %%mm7                      \n\t"
        "movq (%2), %%mm0                       \n\t"
        "movq (%3), %%mm1                       \n\t"
//      0       1       2       3       4       5       6       7       8       9       10
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1 ecx

#define REAL_DEINT_L5(t1,t2,a,b,c)\
        "movq " #a ", %%mm2                     \n\t"\
        "movq " #b ", %%mm3                     \n\t"\
        "movq " #c ", %%mm4                     \n\t"\
        PAVGB(t2, %%mm3)                             \
        PAVGB(t1, %%mm4)                             \
        "movq %%mm2, %%mm5                      \n\t"\
        "movq %%mm2, " #t1 "                    \n\t"\
        "punpcklbw %%mm7, %%mm2                 \n\t"\
        "punpckhbw %%mm7, %%mm5                 \n\t"\
        "movq %%mm2, %%mm6                      \n\t"\
        "paddw %%mm2, %%mm2                     \n\t"\
        "paddw %%mm6, %%mm2                     \n\t"\
        "movq %%mm5, %%mm6                      \n\t"\
        "paddw %%mm5, %%mm5                     \n\t"\
        "paddw %%mm6, %%mm5                     \n\t"\
        "movq %%mm3, %%mm6                      \n\t"\
        "punpcklbw %%mm7, %%mm3                 \n\t"\
        "punpckhbw %%mm7, %%mm6                 \n\t"\
        "paddw %%mm3, %%mm3                     \n\t"\
        "paddw %%mm6, %%mm6                     \n\t"\
        "paddw %%mm3, %%mm2                     \n\t"\
        "paddw %%mm6, %%mm5                     \n\t"\
        "movq %%mm4, %%mm6                      \n\t"\
        "punpcklbw %%mm7, %%mm4                 \n\t"\
        "punpckhbw %%mm7, %%mm6                 \n\t"\
        "psubw %%mm4, %%mm2                     \n\t"\
        "psubw %%mm6, %%mm5                     \n\t"\
        "psraw $2, %%mm2                        \n\t"\
        "psraw $2, %%mm5                        \n\t"\
        "packuswb %%mm5, %%mm2                  \n\t"\
        "movq %%mm2, " #a "                     \n\t"\

#define DEINT_L5(t1,t2,a,b,c)  REAL_DEINT_L5(t1,t2,a,b,c)

DEINT_L5(%%mm0, %%mm1, (%0)           , (%%rax)       , (%%rax, %1)   )
DEINT_L5(%%mm1, %%mm0, (%%rax)       , (%%rax, %1)   , (%%rax, %1, 2))
DEINT_L5(%%mm0, %%mm1, (%%rax, %1)   , (%%rax, %1, 2), (%0, %1, 4)   )
DEINT_L5(%%mm1, %%mm0, (%%rax, %1, 2), (%0, %1, 4)    , (%%rdx)       )
DEINT_L5(%%mm0, %%mm1, (%0, %1, 4)    , (%%rdx)       , (%%rdx, %1)   )
DEINT_L5(%%mm1, %%mm0, (%%rdx)       , (%%rdx, %1)   , (%%rdx, %1, 2))
DEINT_L5(%%mm0, %%mm1, (%%rdx, %1)   , (%%rdx, %1, 2), (%0, %1, 8)   )
DEINT_L5(%%mm1, %%mm0, (%%rdx, %1, 2), (%0, %1, 8)    , (%%rdx, %1, 4))

        "movq %%mm0, (%2)                       \n\t"
        "movq %%mm1, (%3)                       \n\t"
        : : "r" (src), "r" ((int64_t)stride), "r"(tmp), "r"(tmp2)
        : "%rax", "%rdx"
    );
#else //(TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW) && HAVE_6REGS
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
#endif //(TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW) && HAVE_6REGS
}
/**
 * Deinterlace the given block by filtering all lines with a (1 2 1) filter.
 * will be called for every 8x8 block and can read & write from line 4-15
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 * this filter will read lines 4-13 and write 4-11
 */
static inline void RENAME(deInterlaceBlendLinear)(uint8_t src[], int stride, uint8_t *tmp)
{
#if TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
    src+= 4*stride;
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
//      0       1       2       3       4       5       6       7       8       9
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1

        "movq (%2), %%mm0                       \n\t" // L0
        "movq (%%rax), %%mm1                \n\t" // L2
        PAVGB(%%mm1, %%mm0)                           // L0+L2
        "movq (%0), %%mm2                       \n\t" // L1
        PAVGB(%%mm2, %%mm0)
        "movq %%mm0, (%0)                       \n\t"
        "movq (%%rax, %1), %%mm0            \n\t" // L3
        PAVGB(%%mm0, %%mm2)                           // L1+L3
        PAVGB(%%mm1, %%mm2)                           // 2L2 + L1 + L3
        "movq %%mm2, (%%rax)                \n\t"
        "movq (%%rax, %1, 2), %%mm2         \n\t" // L4
        PAVGB(%%mm2, %%mm1)                           // L2+L4
        PAVGB(%%mm0, %%mm1)                           // 2L3 + L2 + L4
        "movq %%mm1, (%%rax, %1)            \n\t"
        "movq (%0, %1, 4), %%mm1                \n\t" // L5
        PAVGB(%%mm1, %%mm0)                           // L3+L5
        PAVGB(%%mm2, %%mm0)                           // 2L4 + L3 + L5
        "movq %%mm0, (%%rax, %1, 2)         \n\t"
        "movq (%%rdx), %%mm0                \n\t" // L6
        PAVGB(%%mm0, %%mm2)                           // L4+L6
        PAVGB(%%mm1, %%mm2)                           // 2L5 + L4 + L6
        "movq %%mm2, (%0, %1, 4)                \n\t"
        "movq (%%rdx, %1), %%mm2            \n\t" // L7
        PAVGB(%%mm2, %%mm1)                           // L5+L7
        PAVGB(%%mm0, %%mm1)                           // 2L6 + L5 + L7
        "movq %%mm1, (%%rdx)                \n\t"
        "movq (%%rdx, %1, 2), %%mm1         \n\t" // L8
        PAVGB(%%mm1, %%mm0)                           // L6+L8
        PAVGB(%%mm2, %%mm0)                           // 2L7 + L6 + L8
        "movq %%mm0, (%%rdx, %1)            \n\t"
        "movq (%0, %1, 8), %%mm0                \n\t" // L9
        PAVGB(%%mm0, %%mm2)                           // L7+L9
        PAVGB(%%mm1, %%mm2)                           // 2L8 + L7 + L9
        "movq %%mm2, (%%rdx, %1, 2)         \n\t"
        "movq %%mm1, (%2)                       \n\t"

        : : "r" (src), "r" ((int64_t)stride), "r" (tmp)
        : "%rax", "%rdx"
    );
#else //TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
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
#endif //TEMPLATE_PP_MMXEXT || TEMPLATE_PP_3DNOW
}
/**
 * Deinterlace the given block by applying a median filter to every second line.
 * will be called for every 8x8 block and can read & write from line 4-15,
 * lines 0-3 have been passed through the deblock / dering filters already, but can be read, too.
 * lines 4-12 will be read into the deblocking filter and should be deinterlaced
 */
static inline void RENAME(deInterlaceMedian)(uint8_t src[], int stride)
{
#if TEMPLATE_PP_MMX
    src+= 4*stride;
#if TEMPLATE_PP_MMXEXT
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
//      0       1       2       3       4       5       6       7       8       9
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1

        "movq (%0), %%mm0                       \n\t"
        "movq (%%rax, %1), %%mm2            \n\t"
        "movq (%%rax), %%mm1                \n\t"
        "movq %%mm0, %%mm3                      \n\t"
        "pmaxub %%mm1, %%mm0                    \n\t"
        "pminub %%mm3, %%mm1                    \n\t"
        "pmaxub %%mm2, %%mm1                    \n\t"
        "pminub %%mm1, %%mm0                    \n\t"
        "movq %%mm0, (%%rax)                \n\t"

        "movq (%0, %1, 4), %%mm0                \n\t"
        "movq (%%rax, %1, 2), %%mm1         \n\t"
        "movq %%mm2, %%mm3                      \n\t"
        "pmaxub %%mm1, %%mm2                    \n\t"
        "pminub %%mm3, %%mm1                    \n\t"
        "pmaxub %%mm0, %%mm1                    \n\t"
        "pminub %%mm1, %%mm2                    \n\t"
        "movq %%mm2, (%%rax, %1, 2)         \n\t"

        "movq (%%rdx), %%mm2                \n\t"
        "movq (%%rdx, %1), %%mm1            \n\t"
        "movq %%mm2, %%mm3                      \n\t"
        "pmaxub %%mm0, %%mm2                    \n\t"
        "pminub %%mm3, %%mm0                    \n\t"
        "pmaxub %%mm1, %%mm0                    \n\t"
        "pminub %%mm0, %%mm2                    \n\t"
        "movq %%mm2, (%%rdx)                \n\t"

        "movq (%%rdx, %1, 2), %%mm2         \n\t"
        "movq (%0, %1, 8), %%mm0                \n\t"
        "movq %%mm2, %%mm3                      \n\t"
        "pmaxub %%mm0, %%mm2                    \n\t"
        "pminub %%mm3, %%mm0                    \n\t"
        "pmaxub %%mm1, %%mm0                    \n\t"
        "pminub %%mm0, %%mm2                    \n\t"
        "movq %%mm2, (%%rdx, %1, 2)         \n\t"


        : : "r" (src), "r" ((int64_t)stride)
        : "%rax", "%rdx"
    );

#else // MMX without MMX2
    __asm__ volatile(
        "lea (%0, %1), %%rax                \n\t"
        "lea (%%rax, %1, 4), %%rdx      \n\t"
//      0       1       2       3       4       5       6       7       8       9
//      %0      eax     eax+%1  eax+2%1 %0+4%1  edx     edx+%1  edx+2%1 %0+8%1  edx+4%1
        "pxor %%mm7, %%mm7                      \n\t"

#define REAL_MEDIAN(a,b,c)\
        "movq " #a ", %%mm0                     \n\t"\
        "movq " #b ", %%mm2                     \n\t"\
        "movq " #c ", %%mm1                     \n\t"\
        "movq %%mm0, %%mm3                      \n\t"\
        "movq %%mm1, %%mm4                      \n\t"\
        "movq %%mm2, %%mm5                      \n\t"\
        "psubusb %%mm1, %%mm3                   \n\t"\
        "psubusb %%mm2, %%mm4                   \n\t"\
        "psubusb %%mm0, %%mm5                   \n\t"\
        "pcmpeqb %%mm7, %%mm3                   \n\t"\
        "pcmpeqb %%mm7, %%mm4                   \n\t"\
        "pcmpeqb %%mm7, %%mm5                   \n\t"\
        "movq %%mm3, %%mm6                      \n\t"\
        "pxor %%mm4, %%mm3                      \n\t"\
        "pxor %%mm5, %%mm4                      \n\t"\
        "pxor %%mm6, %%mm5                      \n\t"\
        "por %%mm3, %%mm1                       \n\t"\
        "por %%mm4, %%mm2                       \n\t"\
        "por %%mm5, %%mm0                       \n\t"\
        "pand %%mm2, %%mm0                      \n\t"\
        "pand %%mm1, %%mm0                      \n\t"\
        "movq %%mm0, " #b "                     \n\t"
#define MEDIAN(a,b,c)  REAL_MEDIAN(a,b,c)

MEDIAN((%0)        , (%%rax)       , (%%rax, %1))
MEDIAN((%%rax, %1), (%%rax, %1, 2), (%0, %1, 4))
MEDIAN((%0, %1, 4) , (%%rdx)       , (%%rdx, %1))
MEDIAN((%%rdx, %1), (%%rdx, %1, 2), (%0, %1, 8))

        : : "r" (src), "r" ((int64_t)stride)
        : "%rax", "%rdx"
    );
#endif //TEMPLATE_PP_MMXEXT
#else //TEMPLATE_PP_MMX
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
#endif //TEMPLATE_PP_MMX
}
#endif

#undef RENAME
#undef TEMPLATE_PP_C
#undef TEMPLATE_PP_ALTIVEC
#undef TEMPLATE_PP_MMX

#undef TEMPLATE_PP_MMXEXT
#undef TEMPLATE_PP_3DNOW
#undef TEMPLATE_PP_SSE2
#undef TEMPLATE_PP_AVX2
