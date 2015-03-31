/*
 * Copyright (c) 2015 Tucker DiNapoli
 *
 * Deinterlace functions
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
#include "PPUtil.S"
function deInterlaceInterpolateLinear /* src, stride */
    mov r2, r0 @save initial src
@would a shift be faster or does it not matter
    add r3, r1, r1 @scale by 2 lines at a time
    @load q0-q4 from [r0], indexing by r3
    vldm128 0,4, r0, r3

    @     dst, src1, src2, tmp
    vavgb q5,  q0,   q1,   q9 @L1
    vavgb q6,  q1,   q2,   q9 @L3
    vavgb q7,  q2,   q3,   q9 @L5
    vavgb q8,  q3,   q4,   q9 @L7

    @store q5-q8 into [r2], indexing by r3
    vstm128 5, 8, r2, r3

    bx lr
endfunc

function deInterlaceInterpolateCubic /*src, stride*/
    mov r4, r1
    add r4, r1, lsl 1 @r4 = r1*3
    add r0, r4
    add r2, r0, r4
    add r3, r1, r1

    vldm128 0, 7, r0, r3 @{q0,q1...q6} = {L0,L2,...L12}

.ifndef deint_cubic
.macro deint_cubic a,b,d,e
    pavgb q8, \a, \e, q15 @(a+e)/2
    pavgb q9, \b, \d, q15 @(b+d)/2

@expand bytes to words
    vmovl.u8 q10, q8_h
    vmovl.u8 q8, q8_l
    vmovl.u8 q11, q9_h
    vmovl.u8 q9, q9_l

    vsub.i16 q8, q9
    vsub.i16 q10, q11 @((a+e) - (b+d))/2
    vshr.i16 q8, #3
    vshr.i16 q10, #3 @((a+e) - (b+d))/16
    vadd.i16 q9, q8
    vadd.i16 q11, q10 @(9(b+d) - (a+e))/16
@pack back into bytes, saturating if necessary
    vqmovn.u16 q8_l, q9
    vqmovn.u16 q8_h, q11
    vst1.64 q8, [r2 :128], r3
.endm
.endif
    deint_cubic q0,q1,q2,q3 @L3
    deint_cubic q1,q2,q3,q4 @L5
    deint_cubic q2,q3,q4,q5 @L7
    deint_cubic q3,q4,q5,q6 @L9
    
    bx lr
endfunc
