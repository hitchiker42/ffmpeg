/*
 * Copyright (C) 2015 Tucker DiNapoli <T.Dinapoli42 at Gmail.com>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define DEBUG_PRINTF(fmt, args...) fprintf(stderr, fmt, ##args)
#define DEBUG_PRINTLN(line) fputs(line"\n",stderr)
static __attribute__((const)) __attribute__((always_inline))
                     uint8_t av_clip_uint8(int a){
    if (a&(~0xFF)) return (-a)>>31;
    else           return a;
}
#ifndef _ISOC11_SOURCE
void *alligned_alloc(size_t alignement, size_t sz){
    void *ret = NULL;
    int err = posix_memalign(&ret, alignement, sz);
    if(err){
        free(ret);
        return NULL;
    }
    return ret;
}
#endif
extern void ff_deInterlaceInterpolateLinear_mmx2(uint8_t *, int);
extern void ff_deInterlaceInterpolateCubic_mmx2(uint8_t *, int);
extern void ff_deInterlaceFF_mmx2(uint8_t *, int, uint8_t *);
extern void ff_deInterlaceL5_mmx2(uint8_t *, int,
                                  uint8_t *,uint8_t*);
extern void ff_deInterlaceBlendLinear_mmx2(uint8_t *, int, uint8_t *);
extern void ff_deInterlaceMedian_mmx2(uint8_t *, int);
extern void ff_blockCopy_mmx2(uint8_t*,int,const uint8_t*,int,int,int64_t*);
/*typedef int64_t x86_reg;
#define REG_a "rax"
#define REG_d "rdi"
#define REG_c "rcx"
#define REGa rax
#define REGd rdi
#define REGc rcx*/
#define TEMPLATE_PP_C 1
#include "deinterlace.c"
#define TEMPLATE_PP_MMX 1
#include "deinterlace.c"
#define TEMPLATE_PP_MMXEXT 1
#include "deinterlace.c"
#define TEMPLATE_PP_3DNOW 1
#include "deinterlace.c"
#define TEMPLATE_PP_SSE2 1
#include "deinterlace.c"
#define TEMPLATE_PP_AVX2 1
#include "deinterlace.c"
static const int num_test_blocks = 96;
static const int rand_seed = 0x12345678;
static uint8_t *generate_test_blocks(int num_rand_blocks){
    //generate various blocks to use in comparing simd and non-simd code
    //initialize the rng with a constant to ensure consistant test results
    srand(rand_seed);
//    uint8_t *blocks = aligned_alloc(32,
    uint8_t *blocks = malloc((num_test_blocks + num_rand_blocks) * 64);
    uint8_t *block_ptr = blocks;
    int i,j,k;
    /*
      This next bunch of code just tries to generate various different types of
      blocks to try and test specific/edge cases.
      Blocks are generated in sets of 4, since thats the size the avx2
      code works with
     */
    //generate 8 sets of 4 blocks with one bit set in each byte
    for(i=1;i<=0xff;i<<=1){
        memset(block_ptr, i, 256);
        block_ptr += 256;
    } //8*256 bytes
    //generate the transpose of above
    for(i=0;i<8;i++){
        for(j=0;j<4;j++){
            for(k=1;k<=0xff;k<<=1){
                *block_ptr++ = k;
            }
        }
    } //2*8*256 bytes
    //generate blocks with alternating bits/low and high 4 bits/lines set
    uint64_t *lineptr = (uint64_t*)block_ptr;
    uint64_t *lineptr2 = lineptr + 8*8*8;
    uint64_t *lineptr3 = lineptr2 + 8*8*8;
    for(i=0;i<8;i++){
        uint64_t line = i > 4 ? 0xaaaaaaaaaaaaaaaa : 0x5555555555555555;
        uint64_t line2 = i > 4 ? 0xf0f0f0f0f0f0f0f0 : 0x0f0f0f0f0f0f0f0f;
        for(j=0;j<8;j++){
            uint64_t line3 = (j + (i>4))%2 ? 0xffffffffffffffff : 0;
            *lineptr++ = line;
            *lineptr2++ = line2;
            *lineptr3++ = line3;
        }
    }//3*8*8*8 + 2*8*256 bytes
    lineptr = lineptr3;
    //generate blocks with all bits set (check overflow/saturation), ostensably the
    //deinterlace filters shouldn't change these at all
    for(i=0;i<64;i++){
        *lineptr ++=  0xffffffffffffffff;
    }//4*8*8*8 + 2*8*256 bytes = 8*256 + 16*256 = 24*256 = 6*1024 = 6K
    uint32_t *rand_ptr = (uint32_t*)lineptr;
//fill the given number of blocks with random data
    for(i=0;i<num_rand_blocks + (num_rand_blocks%4);i++){
        for(j=0;j<8;j++){
            *rand_ptr++ = rand();
            *rand_ptr++ = rand();
        }
    }

    return blocks;
}
//the rest of this file is almost certinaly not portable
typedef void(*deinterlace_filter)();//exploit () meaning any number of args
char* filter_names[6] = {"Interpolate_Linear","Interpolate_Cubic", "FF", "L5",
                        "Blend_Linear","Median"};
deinterlace_filter C_filters[6] = {deInterlaceInterpolateLinear_C,
                                   deInterlaceInterpolateCubic_C,
                                   deInterlaceFF_C,
                                   deInterlaceL5_C,
                                   deInterlaceBlendLinear_C,
                                   deInterlaceMedian_C};
deinterlace_filter MMX2_filters[6] = {deInterlaceInterpolateLinear_MMX2,
                                      deInterlaceInterpolateCubic_MMX2,
                                      deInterlaceFF_MMX2,
                                      deInterlaceL5_MMX2,
                                      deInterlaceBlendLinear_MMX2,
                                      deInterlaceMedian_MMX2};
deinterlace_filter sse2_filters[6] = {deInterlaceInterpolateLinear_sse2,
                                      deInterlaceInterpolateCubic_sse2,
                                      deInterlaceFF_sse2,
                                      deInterlaceL5_sse2,
                                      deInterlaceBlendLinear_sse2,
                                      deInterlaceMedian_sse2};
deinterlace_filter avx2_filters[6] = {deInterlaceInterpolateLinear_avx2,
                                      deInterlaceInterpolateCubic_avx2,
                                      deInterlaceFF_avx2,
                                      deInterlaceL5_avx2,
                                      deInterlaceBlendLinear_avx2,
                                      deInterlaceMedian_avx2};
void run_tests(deinterlace_filter *filters, const char *outfile_names[6],
               uint8_t *blocks, int num_blocks, int simd_width){
//    uint8_t *copied_blocks = alligned_alloc(32, num_blocks * 64);
    uint8_t *copied_blocks = malloc(num_blocks*64);
    uint8_t *blockptr;
    uint8_t *tmp = malloc(128);//aligned_alloc(32, 128)
    uint8_t *tmp_ptr = tmp;
    int i,j,k;
//we process the blocks as if there are 4 blocks to a row
    for(i=0;i<6;i++){
        DEBUG_PRINTF("Running %s filter\n", filter_names[i]);
        tmp_ptr = tmp;
        blockptr = copied_blocks + 4 * 256;//skip first 4 rows
        memcpy(copied_blocks, blocks, num_blocks * 64);
        for(j=0;j<num_blocks-1;j+=4){
            for(k=0;k<4;k+=simd_width){
                filters[i](blockptr, 32, tmp_ptr + k*8, tmp_ptr + 32 + k*8);
                blockptr += 8*8*simd_width;
            }
        }
        FILE* fd = fopen(outfile_names[i], "w");
        if(fd == NULL){
            perror("open");
            fprintf(stderr,"Error opening %s\n",outfile_names[i]);
        } else {
            fwrite(copied_blocks, num_blocks*64, 1 , fd);
            fclose(fd);
        }
    }
    free(tmp);
    free(copied_blocks);
    return;
}
int main(int argc, char **argv){
    char *outdir;
    if(argc < 1){
        outdir = "/tmp/";
    } else {
        outdir = argv[1];
    }
    const char *outfiles[6];
    char *prefixes[4] = {"C","MMX2","sse2","avx2"};
    deinterlace_filter *filters[4]={C_filters, MMX2_filters,
                                    sse2_filters, avx2_filters};
    int simd_widths[4] = {1,2,2,4};
    int filename_len = strlen(outdir) + 32;
    int i,j;
    int num_random_blocks = 256;
    uint8_t *blocks = generate_test_blocks(num_random_blocks);
    for(i=0;i<4;i++){
        DEBUG_PRINTF("Running %s filters\n", prefixes[i]);
        for(j=0;j<6;j++){
            outfiles[i][j] = malloc(filename_len);
            snprintf((char *)outfiles[i][j], filename_len, "%s%s_%s",
                     outdir, prefixes[i], filter_names[i]);
            DEBUG_PRINTF("%s\n",outfiles[i][j]);
        }
        run_tests(filters[i], outfiles[i], blocks,
                  num_random_blocks + num_test_blocks, simd_widths[i]);
    }
}
