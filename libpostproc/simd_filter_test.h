#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <alloca.h>
#define DEBUG_PRINTF(fmt, args...) fprintf(stderr, fmt, ##args)
#define DEBUG_PRINTLN(line) fputs(line"\n",stderr)
#define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
#if !(defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))
static void *aligned_alloc(size_t alignement, size_t sz){
    void *ret = NULL;
    int err = posix_memalign(&ret, alignement, sz);
    if(err){
        free(ret);
        return NULL;
    }
    return ret;
}
#endif
typedef struct deinterlace_filter {
    void *filter;
    uint8_t tmp;
    uint8_t tmp2;
} deinterlace_filter;
typedef struct PPMode{
    int lumMode;                    ///< activates filters for luminance
    int chromMode;                  ///< activates filters for chrominance
    int error;                      ///< non zero on error

    int minAllowedY;                ///< for brightness correction
    int maxAllowedY;                ///< for brightness correction
    float maxClippedThreshold;      ///< amount of "black" you are willing to lose to get a brightness-corrected picture

    int maxTmpNoise[3];             ///< for Temporal Noise Reducing filter (Maximal sum of abs differences)

    int baseDcDiff;
    int flatnessThreshold;

    int forcedQuant;                ///< quantizer if FORCE_QUANT is used
} PPMode;

/**
 * postprocess context.
 */
typedef struct PPContext{
    /**
     * info on struct for av_log
     */
    void *av_class;

    uint8_t *tempBlocks; ///<used for the horizontal code

    /**
     * luma histogram.
     * we need 64bit here otherwise we'll going to have a problem
     * after watching a black picture for 5 hours
     */
    uint64_t *yHistogram;

    DECLARE_ALIGNED(8, uint64_t, packedYOffset);
    DECLARE_ALIGNED(8, uint64_t, packedYScale);

    /** Temporal noise reducing buffers */
    uint8_t *tempBlurred[3];
    int32_t *tempBlurredPast[3];

    /** Temporary buffers for handling the last row(s) */
    uint8_t *tempDst;
    uint8_t *tempSrc;

    uint8_t *deintTemp;

    DECLARE_ALIGNED(8, uint64_t, pQPb);
    DECLARE_ALIGNED(8, uint64_t, pQPb2);

    DECLARE_ALIGNED(32, uint64_t, pQPb_block)[4];
    DECLARE_ALIGNED(32, uint64_t, pQPb2_block)[4];

    DECLARE_ALIGNED(32, uint64_t, mmxDcOffset)[64];
    DECLARE_ALIGNED(32, uint64_t, mmxDcThreshold)[64];

    uint8_t *stdQPTable;       ///< used to fix MPEG2 style qscale
    uint8_t *nonBQPTable;
    uint8_t *forcedQPTable;

    int QP;
    int nonBQP;

    uint8_t QP_block[4];
    uint8_t nonBQP_block[4];

    int frameNum;

    int cpuCaps;

    int qpStride; ///<size of qp buffers (needed to realloc them if needed)
    int stride;   ///<size of some buffers (needed to realloc them if needed)

    int hChromaSubSample;
    int vChromaSubSample;

    PPMode ppMode;
} PPContext;
static const int num_test_blocks = 96;
static const int num_rand_blocks = 256;
static const int num_total_blocks = 256 + 96;
static const int rand_seed = 0x12345678;
static const char *simd_type[4] = {"C","MMX2","sse2","avx2"};
static const int simd_widths[4] = {1,2,2,4};
static const char *deint_filter_names[6] = {"Interpolate_Linear",
                                            "Interpolate_Cubic", "FF", "L5",
                                            "Blend_Linear","Median"};
