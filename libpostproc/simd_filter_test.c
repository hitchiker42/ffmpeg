/*
 * Copyright (C) 2015 Tucker DiNapoli <T.Dinapoli42 at Gmail.com>
 *
 * This file is part of FFmpeg (kinda).
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
#include "simd_filter_test.h"
static inline __attribute__((const))
uint8_t av_clip_uint8(int a){
    if (a&(~0xFF)) return (-a)>>31;
    else           return a;
}
extern void ff_deInterlaceInterpolateLinear_mmx2(uint8_t *, int);
extern void ff_deInterlaceInterpolateCubic_mmx2(uint8_t *, int);
extern void ff_deInterlaceFF_mmx2(uint8_t *, int, uint8_t *);
extern void ff_deInterlaceL5_mmx2(uint8_t *, int,
                                  uint8_t *,uint8_t*);
extern void ff_deInterlaceBlendLinear_mmx2(uint8_t *, int, uint8_t *);
extern void ff_deInterlaceMedian_mmx2(uint8_t *, int);
extern void ff_blockCopy_mmx2(uint8_t*,int,const uint8_t*,int,int,int64_t*);
#define  deInterlaceInterpolateLinear_mmx2 ff_deInterlaceInterpolateLinear_mmx2
#define  deInterlaceInterpolateCubic_mmx2 ff_deInterlaceInterpolateCubic_mmx2
#define  deInterlaceFF_mmx2 ff_deInterlaceFF_mmx2
#define  deInterlaceL5_mmx2 ff_deInterlaceL5_mmx2
#define  deInterlaceBlendLinear_mmx2 ff_deInterlaceBlendLinear_mmx2
#define  deInterlaceMedian_mmx2 ff_deInterlaceMedian_mmx2
#define  blockCopy_mmx2 ff_blockCopy_mmx2
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
static const deinterlace_filter C_filters[6] = {
    {deInterlaceInterpolateLinear_C,0,0},
    {deInterlaceInterpolateCubic_C,0,0},
    {deInterlaceFF_C,1,0},
    {deInterlaceL5_C,1,1},
    {deInterlaceBlendLinear_C,1,0},
    {deInterlaceMedian_C,0,0}};
static const deinterlace_filter mmx2_filters[6] = {
    {deInterlaceInterpolateLinear_mmx2,0,0},
    {deInterlaceInterpolateCubic_mmx2,0,0},
    {deInterlaceFF_mmx2,1,0},
    {deInterlaceL5_mmx2,1,1},
    {deInterlaceBlendLinear_mmx2,1,0},
    {deInterlaceMedian_mmx2,0,0}};
static const deinterlace_filter MMX2_filters[6] = {
    {deInterlaceInterpolateLinear_MMX2,0,0},
    {deInterlaceInterpolateCubic_MMX2,0,0},
    {deInterlaceFF_MMX2,1,0},
    {deInterlaceL5_MMX2,1,1},
    {deInterlaceBlendLinear_MMX2,1,0},
    {deInterlaceMedian_MMX2,0,0}};
static const deinterlace_filter sse2_filters[6] = {
    {deInterlaceInterpolateLinear_sse2,0,0},
    {deInterlaceInterpolateCubic_sse2,0,0},
    {deInterlaceFF_sse2,1,0},
    {deInterlaceL5_sse2,1,1},
    {deInterlaceBlendLinear_sse2,1,0},
    {deInterlaceMedian_sse2,0,0}};
static const deinterlace_filter avx2_filters[6] = {
    {deInterlaceInterpolateLinear_avx2,0,0},
    {deInterlaceInterpolateCubic_avx2,0,0},
    {deInterlaceFF_avx2,1,0},
    {deInterlaceL5_avx2,1,1},
    {deInterlaceBlendLinear_avx2,1,0},
    {deInterlaceMedian_avx2,0,0}};
static deinterlace_filter *filters[5]={C_filters, mmx2_filters,MMX2_filters,
                                       sse2_filters, avx2_filters};
void run_deint_filter(deinterlace_filter filter, uint8_t *src, int stride,
                      uint8_t *tmp, uint8_t *tmp2){
    if(filter.tmp2){
        ((void(*)(uint8_t*,int,uint8_t*,uint8_t*))
         (filter.filter))(src, stride, tmp, tmp2);
    } else if (filter.tmp){
        ((void(*)(uint8_t*,int,uint8_t*))
         (filter.filter))(src, stride, tmp);
    } else {
        ((void(*)(uint8_t*,int))
         (filter.filter))(src, stride);
    }
}
static uint8_t *generate_test_blocks(){
    //generate various blocks to use in comparing simd and non-simd code
    //initialize the rng with a constant to ensure consistant test results
    srand(rand_seed);
//    uint8_t *blocks = aligned_alloc(32,
    uint8_t *blocks = aligned_alloc(32,
                                     (num_test_blocks + num_rand_blocks) * 64);
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
        *lineptr++ =  0xffffffffffffffff;
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
void write_avg_results(uint64_t *block_sums, uint8_t *block_averages,
                       int num_blocks, const char *outfile_name){
    FILE *outfile = fopen(outfile_name, "w");
    if(!outfile){
        perror("fopen");
        fprintf(stderr, "Error opening %s\n", outfile_name);
        exit(1);
    }
    fprintf(outfile,"          sum avg |   sum avg |   sum avg |   sum avg\n");
    int i,j;
    int stride = num_total_blocks;
    for(i=0;i<num_blocks;i+=4){
        for(j=0;j<5;j++){
            fprintf(outfile,
                    "%-8s%5lu %3hhu | %5lu %3hhu | %5lu %3hhu | %5lu %3hhu\n",
                    simd_type[j],
                    block_sums[j*stride+i], block_averages[j*stride+i],
                    block_sums[j*stride+i+1], block_averages[j*stride+i+1],
                    block_sums[j*stride+i+2], block_averages[j*stride+i+2],
                    block_sums[j*stride+i+3], block_averages[j*stride+i+3]);
        }
    }
    fclose(outfile);
}
void write_data_to_file(char *outfile_name, uint8_t *blocks,
                        int num_blocks){
    int fd = open(outfile_name, O_CREAT | O_WRONLY | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if(fd == -1){
        perror("open");
        fprintf(stderr, "Error opening %s\n", outfile_name);
    } else {
        write(fd, blocks, num_blocks * 64);
        close(fd);
    }
}
/*
  This writes data out in rows of 32 bytes, i.e 4 blocks per row.
  Each block (in a row) is seperated by a '|' and each set of 4 blocks
  is seperated by a blank line
  
*/
void write_data_as_text(char *outfile_name, uint8_t *blocks, int num_blocks){
    int i,j,k;
    char *tmp_lines[4];
    for(i=0;i<4;i++){
        tmp_lines[i] = calloc(1,40);
    }
    FILE *outfile = fopen(outfile_name, "w");
    if(!outfile){
        perror("fopen");
        fprintf(stderr, "Error opening %s\n", outfile_name);
        exit(1);
    }
    for(i=0;i<num_blocks;i++){
        for(k=0;k<8;k++){
            for(j=0;j<8;j++){
                char comma = (j==7 ? ' ': ',');
                //really slow
                snprintf(tmp_lines[0]+(j*4),5,"%3hhu%c",
                         blocks[i*64 + k*32 +j],comma);
                snprintf(tmp_lines[1]+(j*4),5,"%3hhu%c",
                         blocks[(i+1)*64 + k*32 + j],comma);
                snprintf(tmp_lines[2]+(j*4),5,"%3hhu%c",
                         blocks[(i+2)*64 + k*32 +j],comma);
                snprintf(tmp_lines[3]+(j*4),5,"%3hhu%c",
                         blocks[(i+3)*64 + k*32 +j],comma);
            }
            fprintf(outfile,"%s | %s | %s | %s\n",
                    tmp_lines[0],tmp_lines[1],tmp_lines[2],tmp_lines[3]);
        }
        fprintf(outfile, "\n");
    }
    fflush(outfile);
    fclose(outfile);
    for(i=0;i<4;i++){
        free(tmp_lines[i]);
    }
}
/*//allocate a PPContext struct and initialize any fields that may be needed
PPContext *allocate_PPContext(){
    PPContext *c = malloc(sizeof(PPContext));
//defaults taken from postprocess.c    
    c->ppMode.lumMode= 0;
    c->ppMode.chromMode= 0;
    c->ppMode.maxTmpNoise[0]= 700;
    c->ppMode.maxTmpNoise[1]= 1500;
    c->ppMode.maxTmpNoise[2]= 3000;
    c->ppMode.maxAllowedY= 234;
    c->ppMode.minAllowedY= 16;
    c->ppMode.baseDcDiff= 256/8;
    c->ppMode.flatnessThreshold= 56-16-1;
    c->ppMode.maxClippedThreshold= 0.01;
    c->ppMode.error=0;
    int i;
    //from postprocess_template.c
    for(i=0; i<57; i++){
        int offset= ((i*c.ppMode.baseDcDiff)>>8) + 1;
        int threshold= offset*2 + 1;
        c->mmxDcOffset[i]= 0x7F - offset;
        c->mmxDcThreshold[i]= 0x7F - threshold;
        c->mmxDcOffset[i]*= 0x0101010101010101LL;
        c->mmxDcThreshold[i]*= 0x0101010101010101LL;
    }
    return c;
}
//default for chroma, luma is too complicated for right now
    c.packedYScale= 0x0100010001000100LL;
    c.packedYOffset= 0;*/

static inline uint8_t unsigned_saturate(uint64_t x){
    return x >= 0xff ? 0xff : x;
}
void run_deint_filter_test(deinterlace_filter filter, uint8_t *blocks,
                           int num_blocks, int simd_width,
                           char *outfile_name, uint64_t *block_sums,
                           uint8_t *block_averages){
    int i,j,k;
    uint8_t *block_ptr = blocks;
    uint8_t *tmp = alloca(128 + 32);
    tmp = tmp + (32 - ((uint64_t)tmp & 32));
    for(i=0;i<num_blocks;i+=4){
        uint8_t *row_start = block_ptr;
        for(j=0;j<4;j+=simd_width){
            run_deint_filter(filter, block_ptr, 32,
                             tmp + j*8, tmp + 32 + j*8);
            block_ptr += 64 * simd_width;
        }
        //really slow
        for(j=0;j<4;j++){
            for(k=0;k<64;k++){
                block_sums[i+j] += row_start[j*64 + k];
            }
            block_averages[i+j] = unsigned_saturate(block_sums[i+j]/64);
        }
    }
    write_data_to_file(outfile_name, blocks, num_blocks);
}

/*
  useful arguments:
  num random blocks
  random seed
  select which filters to use
*/
int main(int argc, char **argv){
    char *outdir;
    if(argc < 2){
        outdir = "/tmp/";
    } else {
        outdir = argv[1];
        int len = strlen(outdir);
        //append '/' to outdir if not present
        if(outdir[len-1] != '/'){
            char *tmp = calloc(len+2,1);
            memcpy(tmp, argv[1], len);
            tmp[len] = '/';
            outdir = tmp;
        }
    }
    int outfile_size = strlen(outdir) + 64;//way more than enough space
    int i,j;
    char *data_outfiles[30];
    char *avg_outfiles[6];
    uint8_t *blocks = aligned_alloc(32, num_total_blocks*64);
    uint8_t *blocks_src = generate_test_blocks();
    for(i=0;i<6;i++){
        avg_outfiles[i] = calloc(outfile_size,1);
        snprintf(avg_outfiles[i], outfile_size,
                 "%s%s_avgs", outdir, deint_filter_names[i]);
        for(j=0;j<5;j++){
            data_outfiles[i + j*6] = malloc(outfile_size);
            snprintf(data_outfiles[i + j*6], outfile_size,
                     "%s%s_%s", outdir, simd_type[j], deint_filter_names[i]);
        }
    }

/*
  I have no idea why but I was getting an error earlier when calling
  aligned alloc, I moved things around and that fixed it, and it seems to
  be fine now...Weird
*/
    uint64_t *block_sums = malloc(sizeof(uint64_t)*num_total_blocks*5);
    uint8_t *block_avgs = malloc(sizeof(uint8_t)*num_total_blocks*5);
    char *input_outfile = calloc(outfile_size,1);
    snprintf(input_outfile, outfile_size, "%s%s", outdir, "input_data");
    write_data_as_text(input_outfile, blocks_src, num_total_blocks);
    __builtin_cpu_init();
    int features[5] = {1,
                       __builtin_cpu_supports("sse"),
                       __builtin_cpu_supports("sse"),
                       __builtin_cpu_supports("sse2"),
                       __builtin_cpu_supports("avx2")};
    for(i=0;i<6;i++){
        DEBUG_PRINTF("Running %s filters\n", deint_filter_names[i]);
        memset(block_sums, '\0', sizeof(uint64_t)*num_total_blocks*5);
        memset(block_avgs, '\0', sizeof(uint8_t)*num_total_blocks*5);
        for(j=0;j<5;j++){
            if(features[j]){
                DEBUG_PRINTF("Using %s implementation\n",simd_type[j]);
                memcpy(blocks, blocks_src, num_total_blocks*64);
                run_deint_filter_test(filters[j][i], blocks,
                                      num_total_blocks, simd_widths[j],
                                      data_outfiles[j*6+i],
                                      block_sums+j*num_total_blocks,
                                      block_avgs+j*num_total_blocks);
            }
        }
        write_avg_results(block_sums, block_avgs, num_total_blocks,
                          avg_outfiles[i]);
    }
}
