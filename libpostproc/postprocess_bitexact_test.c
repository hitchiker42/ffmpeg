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
//most if not all of the functions that need to be tested are static so it's
//eaisest to just include the code itself rather than linking the object file
#include "postprocess.c"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
const int num_test_blocks = 96;
static uint8_t *generate_test_blocks(int num_rand_blocks)
{
    //generate various blocks to use in comparing simd and non-simd code
    //initialize the rng with a constant to ensure consistant test results
    srand(0x12345678);
    uint8_t *blocks = av_malloc((num_test_blocks + num_rand_blocks) * 64);
    uint8_t *block_ptr = blocks;
    uint32_t *rand_ptr;
    uint64_t *lineptr, *lineptr2, *lineptr3;
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
    lineptr = (uint64_t*)block_ptr;
    lineptr2 = lineptr + 8*8*8;
    lineptr3 = lineptr2 + 8*8*8;
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
    rand_ptr = (uint32_t*)lineptr;
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
    uint8_t *copied_blocks = av_malloc(num_blocks * 64);
    uint8_t *blockptr;
    uint8_t *tmp = av_malloc(128);
    int i,j,k;
//we process the blocks as if there are 4 blocks to a row
    for(i=0;i<6;i++){
        memcpy(copied_blocks, blocks, num_blocks * 64);
        blockptr = copied_blocks + 4 * 256;//skip first 4 rows
        for(j=0;j<num_blocks-1;j+=4){
            for(k=0;k<4;k+=simd_width){
                filters[i](blockptr, 32, tmp + k*8, tmp + 32 + k*8);
                blockptr += 8*8*simd_width;
            }
        }
        int fd = open(outfile_names[i], O_WRONLY | O_CREAT);
        if(fd == -1){
            perror("open");
            fprintf(stderr,"Error opening %s\n",outfile_names[i]);
        } else {
            write(fd, copied_blocks, num_blocks*64);
            close(fd);
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
    const char *outfiles[4][6];
    char *prefixes[4] = {"C","MMX2","sse2","avx2"};
    deinterlace_filter *filters[4]={C_filters, MMX2_filters,
                                    sse2_filters, avx2_filters};
    int simd_widths[4] = {1,2,2,4};
    int filename_len = strlen(outdir) + 32;
    int i,j;
    int num_random_blocks = 256;
    uint8_t *blocks = generate_test_blocks(num_random_blocks);
    for(i=0;i<4;i++){
        for(j=0;j<6;j++){
            outfiles[i][j] = malloc(filename_len);
            snprintf((char *)outfiles[i][j], filename_len, "%s%s_%s",
                     outdir, prefixes[i], filter_names[i]);
        }
        run_tests(filters[i], outfiles[i], blocks, 
                  num_random_blocks + num_test_blocks, simd_widths[i]);
    }
}
